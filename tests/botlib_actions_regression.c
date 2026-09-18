/* Issue #35: actual VM action dispatch and native input access with exact allocations. */
#include "../code/server/sv_game.c"
#include "../code/qcommon/vm_local.h"
#include "../code/botlib/be_interface.h"
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

#define IMAGE_SIZE 512
serverStatic_t svs;
static vm_t vm;
vm_t *gvm=&vm;
botlib_globals_t botlibglobals;
botlib_import_t botimport;
extern bot_input_t *botinputs;
static botlib_export_t api;
static byte before[IMAGE_SIZE];
static bot_input_t nativeBefore[2];
static int expectError, commands, allocations;
static jmp_buf errorJump;

/** Stop on any unexpected data, allocation, or native command. */
static void Check( int ok, const char *message ) {
	if(!ok) { fprintf(stderr,"Bot action regression failed: %s\n",message); exit(1); }
}
/** Require controlled rejection before touching either VM or native input data. */
void QDECL Com_Error( int level, const char *format, ... ) {
	(void)format; Check(expectError && level==ERR_DROP && vm.interpretFaulted && !vm.currentlyInterpreting,"error state");
	Check(!memcmp(before,vm.dataBase,IMAGE_SIZE),"VM mutation on error");
	Check(!botinputs || !memcmp(nativeBefore,botinputs,sizeof(nativeBefore)),"native mutation on error");
	longjmp(errorJump,1);
}
/** Ignore unused VM diagnostics. */
void QDECL Com_Printf( const char *format, ... ) { (void)format; }
/** Allocate exactly the native input capacity requested by setup. */
void *GetClearedHunkMemory( unsigned long size ) { allocations++; return calloc(1,size); }
/** Release the fixture's native input storage. */
void FreeMemory( void *pointer ) { free(pointer); }
/** Support the real native input snapshot. */
void Com_Memcpy( void *dest, const void *src, size_t size ) { memcpy(dest,src,size); }
/** Accept only validated command client indices and complete strings. */
static void ClientCommand( int client, char *command ) {
	Check(!expectError && client==1,"command client");
	Check(!strcmp(command,"say test") || !strcmp(command,"say_team test") || !strcmp(command,"test"),"command text"); commands++;
}
/** Require malformed VM action requests to preserve both data images. */
static void Reject( int *args ) {
	int oldCommands=commands; vm.interpretFaulted=qfalse; vm.currentlyInterpreting=qtrue;
	memcpy(before,vm.dataBase,IMAGE_SIZE); if(botinputs) memcpy(nativeBefore,botinputs,sizeof(nativeBefore)); expectError=1;
	if(setjmp(errorJump)==0) { SV_BotLibActionCalls(args); Check(0,"invalid request accepted"); }
	expectError=0; Check(commands==oldCommands,"invalid command dispatched");
}
/** Bind the same exported native implementations used by the engine. */
static void BindActions( void ) {
#define BIND(name) api.ea.name=name
	BIND(EA_Say); BIND(EA_SayTeam); BIND(EA_Command); BIND(EA_Action); BIND(EA_Gesture);
	BIND(EA_Talk); BIND(EA_Attack); BIND(EA_Use); BIND(EA_Respawn); BIND(EA_Crouch);
	BIND(EA_MoveUp); BIND(EA_MoveDown); BIND(EA_MoveForward); BIND(EA_MoveBack);
	BIND(EA_MoveLeft); BIND(EA_MoveRight); BIND(EA_SelectWeapon); BIND(EA_Jump);
	BIND(EA_DelayedJump); BIND(EA_Move); BIND(EA_View); BIND(EA_EndRegular);
	BIND(EA_GetInput); BIND(EA_ResetInput);
#undef BIND
}
/** Cover VM ranges, actual capacities, native invalid-index guards, and unchanged valid action behavior. */
int main( void ) {
	int args[16]={0}, bad[]={-1,2,INT_MAX,INT_MIN}, i,j, oldAllocations;
	void (*simple[])(int)={EA_Gesture,EA_Talk,EA_Attack,EA_Use,EA_Respawn,EA_Crouch,EA_Walk,
		EA_MoveUp,EA_MoveDown,EA_MoveForward,EA_MoveBack,EA_MoveLeft,EA_MoveRight,EA_Jump,EA_DelayedJump,EA_ResetInput};
	float value=450;
	vm.dataBase=malloc(IMAGE_SIZE); Check(vm.dataBase!=NULL,"VM allocation");
	vm.dataMask=IMAGE_SIZE-1; currentVM=&vm;
	svs.clients=calloc(4,sizeof(*svs.clients)); Check(svs.clients!=NULL,"server allocation"); svs.clientCapacity=4;
	memset(vm.dataBase,'x',IMAGE_SIZE); memcpy(vm.dataBase+32,"test",5);
	((float *)(vm.dataBase+4))[0]=1; ((float *)(vm.dataBase+4))[1]=2; ((float *)(vm.dataBase+4))[2]=3;
	botlib_export=&api; botimport.BotClientCommand=ClientCommand; BindActions();
	args[0]=BOTLIB_EA_VIEW; args[1]=1; args[2]=4; Reject(args);
	botlibglobals.maxclients=2; Check(EA_Setup()==BLERR_NOERROR,"setup");
	Check(EA_ClientValid(0) && EA_ClientValid(1) && !EA_ClientValid(2),"native capacity");
	args[0]=BOTLIB_EA_MOVE; memcpy(args+3,&value,sizeof(value));
	Check(SV_BotLibActionCalls(args)==0 && botinputs[1].speed==400 && botinputs[1].dir[2]==3,"valid move and speed clamp");
	args[0]=BOTLIB_EA_VIEW; Check(SV_BotLibActionCalls(args)==0 && botinputs[1].viewangles[1]==2,"valid view");
	args[2]=IMAGE_SIZE-8; Reject(args); args[2]=5; Reject(args); args[2]=0; Reject(args); args[2]=4;
	args[0]=BOTLIB_EA_ACTION; args[2]=ACTION_ATTACK;
	Check(SV_BotLibActionCalls(args)==-1 && (botinputs[1].actionflags&ACTION_ATTACK),"legacy generic action return");
	args[0]=BOTLIB_EA_GET_INPUT; value=1.5f; memcpy(args+2,&value,sizeof(value)); args[3]=IMAGE_SIZE-sizeof(bot_input_t);
	Check(SV_BotLibActionCalls(args)==0 && !memcmp(vm.dataBase+args[3],botinputs+1,sizeof(bot_input_t)),"full input output");
	args[3]+=4; Reject(args); args[3]=0; Reject(args);
	args[0]=BOTLIB_EA_SAY; args[2]=32; Check(SV_BotLibActionCalls(args)==0,"say");
	args[0]=BOTLIB_EA_SAY_TEAM; Check(SV_BotLibActionCalls(args)==0,"team say");
	args[0]=BOTLIB_EA_COMMAND; Check(SV_BotLibActionCalls(args)==0,"command");
	args[2]=IMAGE_SIZE-1; vm.dataBase[IMAGE_SIZE-1]='x'; Reject(args); args[2]=32;
	botlibglobals.maxclients=MAX_CLIENTS;
	for(i=0;i<4;i++) { args[1]=bad[i]; Reject(args); }
	args[1]=1; svs.clientCapacity=1; Reject(args); svs.clientCapacity=4;
	memcpy(nativeBefore,botinputs,sizeof(nativeBefore)); oldAllocations=commands;
	for(i=0;i<4;i++) {
		for(j=0;j<(int)(sizeof(simple)/sizeof(simple[0]));j++) simple[j](bad[i]);
		EA_Action(bad[i],-1); EA_SelectWeapon(bad[i],99); EA_Move(bad[i],NULL,1);
		EA_View(bad[i],NULL); EA_EndRegular(bad[i],1); EA_GetInput(bad[i],1,NULL);
		EA_Say(bad[i],NULL); EA_SayTeam(bad[i],NULL); EA_Command(bad[i],NULL);
	}
	Check(commands==oldAllocations && !memcmp(nativeBefore,botinputs,sizeof(nativeBefore)),"native invalid clients preserved");
	for(i=0;i<3;i++) {
		botlibglobals.maxclients=i==0?0:i==1?-1:INT_MAX; oldAllocations=allocations;
		Check(EA_Setup()!=BLERR_NOERROR && allocations==oldAllocations && EA_ClientValid(1),"invalid setup preserves allocation");
	}
	args[1]=1; botlib_export=NULL; Reject(args); botlib_export=&api;
	EA_Shutdown(); Check(!EA_ClientValid(1),"shutdown capacity reset"); Reject(args);
	free(svs.clients); free(vm.dataBase); puts("Bot elementary-action regressions passed (issue #35)"); return 0;
}
