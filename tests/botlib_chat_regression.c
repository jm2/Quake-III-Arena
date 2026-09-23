/* Issue #35: execute the actual chat dispatcher with complete-output callbacks. */
#include "../code/server/sv_game.c"
#include "../code/qcommon/vm_local.h"
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#define IMAGE_SIZE 4096
static vm_t vm;
vm_t *gvm = &vm;
static botlib_export_t api;
static byte before[IMAGE_SIZE];
static int expectError, callbacks, emptyConsole, synonymSize;
static jmp_buf errorJump;

/** Stop on unexpected output or native dispatch. */
static void Check( int ok, const char *message ) {
	if (!ok) { fprintf(stderr,"Botlib chat regression failed: %s\n",message); exit(1); }
}
/** Require rejection before native calls or data mutation. */
void QDECL Com_Error( int level, const char *format, ... ) {
	(void)format;
	Check(expectError && level==ERR_DROP && vm.interpretFaulted && !vm.currentlyInterpreting,"error state");
	Check(!memcmp(before,vm.dataBase,IMAGE_SIZE),"rejection changed data");
	longjmp(errorJump,1);
}
/** Ignore unused VM diagnostics. */
void QDECL Com_Printf( const char *format, ... ) { (void)format; }
/** Reject any native callback on malformed input. */
static void Callback( void ) { Check(!expectError,"invalid request dispatched"); callbacks++; }
/** Touch the complete console structure, including pointer padding. */
static int Console( int state, bot_consolemessage_t *out ) {
	Callback(); Check(state==1,"console state"); if(emptyConsole) return 0;
	memset(out,0,sizeof(*out)); out->handle=2; out->time=3.5f; out->type=4;
	strcpy(out->message,"test"); out->prev=(bot_consolemessage_t *)out; out->next=(bot_consolemessage_t *)out; return 2;
}
/** Reproduce native fixed-buffer concatenation for all eight nullable variables. */
static void Initial( int state, char *type, int context, char *v0, char *v1, char *v2, char *v3, char *v4, char *v5, char *v6, char *v7 ) {
	char output[MAX_MESSAGE_SIZE]="", *vars[]={v0,v1,v2,v3,v4,v5,v6,v7}; int i;
	Callback(); Check(state==1 && !strcmp(type,"test") && context==3,"initial args");
	for(i=0;i<8;i++) if(vars[i]) strcat(output,vars[i]);
	Check(strlen(output)<sizeof(output),"initial capacity");
}
/** Include the original message in native fixed-buffer concatenation. */
static int Reply( int state, char *message, int mc, int vc, char *v0, char *v1, char *v2, char *v3, char *v4, char *v5, char *v6, char *v7 ) {
	char output[MAX_MESSAGE_SIZE], *vars[]={v0,v1,v2,v3,v4,v5,v6,v7}; int i;
	Callback(); Check(state==1 && mc==3 && vc==4,"reply args"); strcpy(output,message);
	for(i=0;i<8;i++) if(vars[i]) strcat(output,vars[i]); return 5;
}
/** Touch the entire caller-supplied chat output. */
static void GetMessage( int state, char *out, int size ) { Callback(); Check(state==1,"message state"); memset(out,'m',size); }
/** Record the granted synonym size while writing only the original string span. */
static void Synonyms( char *text, unsigned long context, int size ) {
	size_t length=strlen(text); Callback(); Check(context==1,"synonym context"); memset(text,'s',length); text[length]=0;
	synonymSize=size;
}
/** Mark the game VM as a native module. */
static int QDECL NativeEntry( int command, ... ) { (void)command; return 0; }
/** Read the full match span and fill the requested output capacity. */
static void MatchVariable( bot_match_t *match, int variable, char *out, int size ) {
	int offset=match->variables[variable].offset, length=match->variables[variable].length;
	Callback(); if(offset>=0) Check(!memcmp(match->string+offset,"test",length),"match span"); memset(out,'v',size);
}
/** Preserve native nullable substring queries. */
static int Contains( char *a, char *b, int sensitive ) { Callback(); Check(!a && !b && sensitive==1,"nullable contains"); return -1; }
/** Map a page below 4 GiB so the dispatcher's 32-bit native pointer ABI can address it. */
static char *LowPage( void ) {
	char *page;
#ifdef MAP_32BIT
	page=mmap(NULL,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_32BIT,-1,0);
#else
	page=mmap((void *)0x10000000,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
#endif
	Check(page!=MAP_FAILED && (unsigned long)page==(unsigned int)(unsigned long)page,"low native page");
	return page;
}
/** Require malformed requests to leave the image and callback count unchanged. */
static void Reject( int *args ) {
	int calls=callbacks; vm.interpretFaulted=qfalse; vm.currentlyInterpreting=qtrue;
	memcpy(before,vm.dataBase,IMAGE_SIZE); expectError=1;
	if(setjmp(errorJump)==0) { SV_BotLibChatCalls(args); Check(0,"invalid request accepted"); }
	expectError=0; Check(callbacks==calls,"rejection dispatched");
}
/** Cover complete outputs, optional variables, combined sizes, embedded metadata, and API availability. */
int main( void ) {
	int args[16]={0}, i;
	bot_match_t *match; qvmBotConsoleMessage_t *console; char *native;
	vm.dataBase=malloc(IMAGE_SIZE); Check(vm.dataBase!=NULL,"allocation");
	vm.dataMask=IMAGE_SIZE-1; currentVM=&vm; botlib_export=&api;
	api.ai.BotNextConsoleMessage=Console; api.ai.BotInitialChat=Initial; api.ai.BotReplyChat=Reply;
	api.ai.BotGetChatMessage=GetMessage; api.ai.BotReplaceSynonyms=Synonyms;
	api.ai.BotMatchVariable=MatchVariable; api.ai.StringContains=Contains;
	memset(vm.dataBase,'x',IMAGE_SIZE); memcpy(vm.dataBase+32,"test",5);
	memset(vm.dataBase+512,'a',255); vm.dataBase[767]=0; memcpy(vm.dataBase+1024,"a",2);
	args[0]=BOTLIB_AI_NEXT_CONSOLE_MESSAGE; args[1]=1; args[2]=IMAGE_SIZE-sizeof(qvmBotConsoleMessage_t);
	Check(SV_BotLibChatCalls(args)==2,"whole QVM console output");
	console=(qvmBotConsoleMessage_t *)(vm.dataBase+args[2]);
	Check(console->handle==2 && console->time==3.5f && console->type==4 && !strcmp(console->message,"test") && !console->prev && !console->next,"QVM console layout and cleared links");
	emptyConsole=1; memcpy(before,vm.dataBase,IMAGE_SIZE);
	Check(SV_BotLibChatCalls(args)==0 && !memcmp(before,vm.dataBase,IMAGE_SIZE),"empty console preserves output"); emptyConsole=0;
	args[2]=2048; memset(vm.dataBase+args[2]+sizeof(*console),0x5a,12);
	Check(SV_BotLibChatCalls(args)==2,"contained QVM console object");
	for(i=0;i<12;i++) Check(vm.dataBase[args[2]+sizeof(*console)+i]==0x5a,"QVM object tail overwritten");
	args[2]=IMAGE_SIZE-sizeof(*console)+4; Reject(args); args[2]=0; Reject(args);
	args[0]=BOTLIB_AI_INITIAL_CHAT; args[2]=32; args[3]=3;
	Check(SV_BotLibChatCalls(args)==0,"all absent variables");
	for(i=0;i<8;i++) args[4+i]=1024;
	Check(SV_BotLibChatCalls(args)==0,"eight variables");
	for(i=0;i<8;i++) args[4+i]=0; args[4]=512;
	Check(SV_BotLibChatCalls(args)==0,"exact combined capacity"); args[11]=1024; Reject(args);
	args[11]=IMAGE_SIZE-1; vm.dataBase[IMAGE_SIZE-1]='x'; Reject(args);
	memset(args+4,0,8*sizeof(int)); args[0]=BOTLIB_AI_REPLY_CHAT; args[2]=512; args[3]=3; args[4]=4;
	Check(SV_BotLibChatCalls(args)==5,"exact reply message"); args[12]=1024; Reject(args);
	args[12]=0; vm.dataBase[767]='a'; vm.dataBase[768]=0; Reject(args); vm.dataBase[767]=0;
	args[0]=BOTLIB_AI_GET_CHAT_MESSAGE; args[2]=IMAGE_SIZE-8; args[3]=8;
	Check(SV_BotLibChatCalls(args)==0,"whole message output"); args[3]=0; Reject(args); args[3]=9; Reject(args);
	args[0]=BOTLIB_AI_REPLACE_SYNONYMS; args[1]=IMAGE_SIZE-MAX_MESSAGE_SIZE; args[2]=1; args[3]=MAX_MESSAGE_SIZE;
	memset(vm.dataBase+args[1],'a',MAX_MESSAGE_SIZE); vm.dataBase[IMAGE_SIZE-1]=0;
	Check(SV_BotLibChatCalls(args)==0 && synonymSize==MAX_MESSAGE_SIZE,"maximum original span"); args[1]++;
	Check(SV_BotLibChatCalls(args)==0 && synonymSize==MAX_MESSAGE_SIZE-1,"interior string near image end");
	args[1]=IMAGE_SIZE-2; memcpy(vm.dataBase+args[1],"a",2);
	Check(SV_BotLibChatCalls(args)==0 && synonymSize==2 && vm.dataBase[IMAGE_SIZE-2]=='s' && !vm.dataBase[IMAGE_SIZE-1],"two-byte object at image end");
	args[1]=IMAGE_SIZE-1; Check(SV_BotLibChatCalls(args)==0 && synonymSize==1 && !vm.dataBase[IMAGE_SIZE-1],"empty string at image end");
	args[1]=2064; memset(vm.dataBase+2048,0x5a,32); memcpy(vm.dataBase+args[1],"hi hi",6); memcpy(before,vm.dataBase,IMAGE_SIZE);
	Check(SV_BotLibChatCalls(args)==0 && !memcmp(before,vm.dataBase,args[1]) &&
	      !memcmp(before+args[1]+6,vm.dataBase+args[1]+6,IMAGE_SIZE-args[1]-6) && !vm.dataBase[args[1]+5],"interior object canaries");
	args[1]=IMAGE_SIZE-2; vm.dataBase[IMAGE_SIZE-2]=vm.dataBase[IMAGE_SIZE-1]='a'; Reject(args);
	args[1]=3072; memset(vm.dataBase+args[1],'a',256); vm.dataBase[args[1]+256]=0; Reject(args);
	args[1]=0; Reject(args);
	native=LowPage(); strcpy(native+100,"hi hi"); args[1]=(int)(unsigned long)(native+100); args[3]=MAX_MESSAGE_SIZE-100;
	vm.entryPoint=NativeEntry; Check(SV_BotLibChatCalls(args)==0 && synonymSize==MAX_MESSAGE_SIZE-100 && !strcmp(native+100,"sssss"),"native message size");
	vm.entryPoint=NULL; munmap(native,4096); args[3]=0;
	args[0]=BOTLIB_AI_MATCH_VARIABLE; args[1]=64; args[2]=0; args[3]=IMAGE_SIZE-8; args[4]=8;
	match=(bot_match_t *)(vm.dataBase+64); memset(match,0,sizeof(*match)); strcpy(match->string,"test");
	match->variables[0].offset=0; match->variables[0].length=4;
	Check(SV_BotLibChatCalls(args)==0,"full valid span");
	match->variables[0].length=-1; Reject(args); match->variables[0].length=INT_MAX; Reject(args);
	match->variables[0].length=4; match->variables[0].offset=5; Reject(args);
	match->variables[0].offset=0; args[2]=-1; Reject(args); args[2]=MAX_MATCHVARIABLES; Reject(args); args[2]=0;
	memset(match->string,'x',sizeof(match->string)); Reject(args); strcpy(match->string,"test");
	args[0]=BOTLIB_AI_STRING_CONTAINS; args[1]=0; args[2]=0; args[3]=1;
	Check(SV_BotLibChatCalls(args)==-1,"nullable contains"); botlib_export=NULL; Reject(args);
	free(vm.dataBase); puts("Botlib chat dispatcher regressions passed (issue #35)"); return 0;
}
