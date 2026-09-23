/* Issues #35/#48: execute native fixed-buffer synonym and match routines. */
#include "../code/botlib/be_ai_chat.c"
#include <stdlib.h>
#include <string.h>

botlib_import_t botimport;
/** Fail on any unexpected native output. */
static void Check( int ok, const char *message ) {
	if(!ok) { fprintf(stderr,"Native bot chat regression failed: %s\n",message); exit(1); }
}
static int printErrors;
/** Count botlib errors so each rejected escape reports exactly once. */
static void QDECL CountPrint( int type, char *format, ... ) { (void)format; if(type==PRT_ERROR) printErrors++; }
/** Issue #290: variable escapes accept only decimal indexes below MAX_MATCHVARIABLES and bounded spans. */
static void VariableEscapes( bot_match_t *match, char *buffer, char *source ) {
	static char *rejected[]={"8","99999999999","x","/","-1",""};
	bot_randomstring_t no={"No.",NULL}; bot_randomlist_t negative={"negative",1,&no,NULL};
	char message[64]; int i;
	botimport.Print=CountPrint; randomstrings=&negative;
	memset(match,0,sizeof(*match)); strcpy(match->string,"Sarge sure camper");
	for(i=0;i<MAX_MATCHVARIABLES;i++) match->variables[i].offset=-1;
	match->variables[0].offset=6; match->variables[0].length=4;
	match->variables[7].offset=11; match->variables[7].length=6;
	sprintf(message,"%crnegative%c I'm not %cv0%c.",ESCAPE_CHAR,ESCAPE_CHAR,ESCAPE_CHAR,ESCAPE_CHAR);
	Check(BotExpandChatMessage(buffer,message,0,match,0,qtrue) && !strcmp(buffer,"No. I'm not sure."),"retail reply template");
	sprintf(message,"Is drinking the cause of your problem, %cv7%c?",ESCAPE_CHAR,ESCAPE_CHAR);
	Check(!BotExpandChatMessage(buffer,message,0,match,0,qtrue) && !strcmp(buffer,"Is drinking the cause of your problem, camper?"),"last match variable 7 expands");
	sprintf(message,"%cv007%c",ESCAPE_CHAR,ESCAPE_CHAR);
	Check(!BotExpandChatMessage(buffer,message,0,match,0,qfalse) && !strcmp(buffer,"camper"),"leading zeros keep retail index");
	match->subtype=MAX_MESSAGE_SIZE*4; /* the old parser read "/" as variables[-1], overlapping type/subtype */
	for(i=0;i<(int)(sizeof(rejected)/sizeof(rejected[0]));i++) {
		sprintf(message,"%cv%s%c",ESCAPE_CHAR,rejected[i],ESCAPE_CHAR); strcpy(buffer,"z"); printErrors=0;
		Check(!BotExpandChatMessage(buffer,message,0,match,0,qfalse) && !strcmp(buffer,"z") && printErrors==1,"invalid variable index rejected before copying");
		sprintf(message,"%crnegative%c %cv%s%c",ESCAPE_CHAR,ESCAPE_CHAR,ESCAPE_CHAR,rejected[i],ESCAPE_CHAR); printErrors=0;
		Check(!BotExpandChatMessage(buffer,message,0,match,0,qfalse) && printErrors==1,"invalid variable index after a random rejected");
	}
	match->variables[7].length=MAX_MESSAGE_SIZE*4; sprintf(message,"%cv7%c!",ESCAPE_CHAR,ESCAPE_CHAR);
	Check(!BotExpandChatMessage(buffer,message,0,match,0,qfalse) && !strcmp(buffer,"!"),"span past the match string expands to nothing");
	memset(match->string,'x',MAX_MESSAGE_SIZE-1); match->string[MAX_MESSAGE_SIZE-1]=0;
	match->variables[7].offset=0; match->variables[7].length=MAX_MESSAGE_SIZE-1; sprintf(message,"%cv7%c",ESCAPE_CHAR,ESCAPE_CHAR);
	Check(!BotExpandChatMessage(buffer,message,0,match,0,qfalse) && strlen(buffer)==MAX_MESSAGE_SIZE-1,"full-length variable fills temp exactly");
	source[0]=ESCAPE_CHAR; source[1]='r'; memset(source+2,'a',MAX_MESSAGE_SIZE*2); source[MAX_MESSAGE_SIZE*2+2]=ESCAPE_CHAR; source[MAX_MESSAGE_SIZE*2+3]=0;
	strcpy(buffer,"z"); printErrors=0;
	Check(!BotExpandChatMessage(buffer,source,0,match,0,qfalse) && !strcmp(buffer,"z") && printErrors==1,"overlong random name rejected before copying");
	randomstrings=NULL; botimport.Print=NULL;
}
/** Literal text that fills the output keeps its terminator inside MAX_MESSAGE_SIZE. */
static void LiteralCapacity( bot_match_t *match, char *buffer, char *source ) {
	static const int lengths[]={MAX_MESSAGE_SIZE-1,MAX_MESSAGE_SIZE,MAX_MESSAGE_SIZE+44};
	char message[16]; int i;
	botimport.Print=CountPrint;
	for(i=0;i<3;i++) {
		memset(source,'a',lengths[i]); source[lengths[i]]=0; printErrors=0;
		Check(!BotExpandChatMessage(buffer,source,0,match,0,qfalse) && strlen(buffer)==MAX_MESSAGE_SIZE-1 &&
		      printErrors==(lengths[i]>=MAX_MESSAGE_SIZE),"literal text keeps its terminator inside the output");
	}
	memset(match,0,sizeof(*match)); memset(match->string,'x',250);
	for(i=0;i<MAX_MATCHVARIABLES;i++) match->variables[i].offset=-1;
	match->variables[0].offset=0; match->variables[0].length=250;
	sprintf(message,"%cv0%c123456789",ESCAPE_CHAR,ESCAPE_CHAR); printErrors=0;
	Check(!BotExpandChatMessage(buffer,message,0,match,0,qfalse) && strlen(buffer)==MAX_MESSAGE_SIZE-1 &&
	      !strcmp(buffer+250,"12345") && printErrors==1,"literal text after a variable stops before the terminator");
	botimport.Print=NULL;
}
/** Keep real match routines linked without pulling in the full engine. */
void Com_Memcpy( void *dest, const void *src, size_t size ) { memcpy(dest,src,size); }
/** Support the real chat helpers' native initialization. */
void Com_Memset( void *dest, int value, size_t size ) { memset(dest,value,size); }
/** Treat unexpected shared utility errors as regression failures. */
void QDECL Com_Error( int level, const char *format, ... ) { (void)level; (void)format; Check(0,"engine error"); }
/** Ignore unrelated shared utility output. */
void QDECL Com_Printf( const char *format, ... ) { (void)format; }
/** Cover growing, shrinking, unchanged, full-capacity, and overlapping native operations. */
int main( void ) {
	char *buffer=malloc(MAX_MESSAGE_SIZE), *inside=malloc(8), *source=malloc(1024), *out=malloc(8), *small=malloc(6);
	char variableMessage[]={ESCAPE_CHAR,'v','0',ESCAPE_CHAR,0}; int i;
	bot_match_t *match=malloc(sizeof(*match));
	bot_synonym_t replacement={0}, old={0}; bot_synonymlist_t list={0};
	Check(buffer && inside && source && out && small && match,"allocation");
	/* 1.32 resumes each search at the delimiter after a replacement, which skips the adjacent word. */
	strcpy(buffer,"hi hi"); StringReplaceWordsSized(buffer,"hi","hello",MAX_MESSAGE_SIZE); Check(!strcmp(buffer,"hello hi"),"growth");
	StringReplaceWordsSized(buffer,"hello","hi",MAX_MESSAGE_SIZE); Check(!strcmp(buffer,"hi hi"),"shrink");
	StringReplaceWordsSized(buffer,"hi","",MAX_MESSAGE_SIZE); Check(!strcmp(buffer," hi"),"empty replacement");
	strcpy(inside,"foo bar"); StringReplaceWordsSized(inside,"bar","foo bar",8); Check(!strcmp(inside,"foo bar"),"inside existing replacement");
	memset(buffer,'a',MAX_MESSAGE_SIZE-1); buffer[MAX_MESSAGE_SIZE-1]=0;
	buffer[0]='h'; buffer[1]='i'; buffer[2]=' ';
	StringReplaceWordsSized(buffer,"hi","hello",MAX_MESSAGE_SIZE); Check(strlen(buffer)==255 && !memcmp(buffer,"hi ",3),"full capacity preserved");
	buffer[253]=0; StringReplaceWordsSized(buffer,"hi","hello",MAX_MESSAGE_SIZE); Check(strlen(buffer)==253 && !memcmp(buffer,"hi ",3),"one byte over capacity preserved");
	buffer[252]=0; StringReplaceWordsSized(buffer,"hi","hello",MAX_MESSAGE_SIZE); Check(strlen(buffer)==255 && !memcmp(buffer,"hello ",6),"exact growth capacity");
	StringReplaceWordsSized(buffer,"","x",MAX_MESSAGE_SIZE); Check(strlen(buffer)==255,"empty synonym");
	replacement.string="hello"; replacement.next=&old; old.string="hi";
	list.context=1; list.firstsynonym=&replacement; synonyms=&list;
	strcpy(buffer,"hi hi"); BotReplaceReplySynonymsSized(buffer,1,MAX_MESSAGE_SIZE); Check(!strcmp(buffer,"hello hello"),"reply synonym growth");
	memset(buffer,'a',255); buffer[255]=0; memcpy(buffer,"hi ",3);
	BotReplaceReplySynonymsSized(buffer,1,MAX_MESSAGE_SIZE); Check(strlen(buffer)==255 && !memcmp(buffer,"hi ",3),"reply synonym overflow preserved");
	strcpy(small,"hi hi"); BotReplaceSynonyms(small,1,6); Check(!strcmp(small,"hi hi"),"exact object skips growth");
	replacement.string="yo"; BotReplaceSynonyms(small,1,6); Check(!strcmp(small,"yo hi"),"same-size replacement");
	replacement.string="h"; strcpy(small,"hi hi"); BotReplaceSynonyms(small,1,0); BotReplaceSynonyms(small,1,-1);
	Check(!strcmp(small,"hi hi"),"missing or negative size");
	BotReplaceSynonyms(small,1,6); Check(!strcmp(small,"h hi"),"shrinking replacement");
	replacement.string="hello"; memset(buffer,0x5a,MAX_MESSAGE_SIZE); strcpy(buffer+128,"hi hi");
	BotReplaceSynonyms(buffer+128,1,6); Check(!strcmp(buffer+128,"hi hi"),"interior legacy span skips growth");
	for(i=0;i<128;i++) Check((unsigned char)buffer[i]==0x5a,"interior prefix canary");
	for(i=134;i<MAX_MESSAGE_SIZE;i++) Check((unsigned char)buffer[i]==0x5a,"interior tail canary");
	BotReplaceSynonyms(buffer+128,1,MAX_MESSAGE_SIZE-128); Check(!strcmp(buffer+128,"hello hi"),"interior known size grows");
	for(i=0;i<128;i++) Check((unsigned char)buffer[i]==0x5a,"grown interior prefix canary");
	for(i=137;i<MAX_MESSAGE_SIZE;i++) Check((unsigned char)buffer[i]==0x5a,"grown interior tail canary");
	memset(buffer,0x5a,MAX_MESSAGE_SIZE); strcpy(buffer+250,"hi hi"); BotReplaceSynonyms(buffer+250,1,MAX_MESSAGE_SIZE-250);
	Check(!strcmp(buffer+250,"hi hi"),"interior object at allocation end");
	for(i=0;i<250;i++) Check((unsigned char)buffer[i]==0x5a,"near-end prefix canary");
	buffer[255]=0; BotReplaceSynonyms(buffer+255,1,1); Check(!buffer[255],"empty final-byte object");
	BotReplaceSynonyms(NULL,1,MAX_MESSAGE_SIZE);
	memset(match,0,sizeof(*match)); strcpy(match->string,"hi");
	for(i=0;i<MAX_MATCHVARIABLES;i++) match->variables[i].offset=-1;
	match->variables[0].offset=0; match->variables[0].length=2;
	(void)BotExpandChatMessage(buffer,variableMessage,0,match,1,qfalse);
	Check(!strcmp(buffer,"hello"),"known-capacity initial variable still grows");
	(void)BotExpandChatMessage(buffer,variableMessage,0,match,1,qtrue);
	Check(!strcmp(buffer,"hello"),"known-capacity reply variable still grows");
	synonyms=NULL;
	VariableEscapes(match,buffer,source); LiteralCapacity(match,buffer,source);
	strcpy(inside,"foo bar"); Check(StringContainsWord(inside,"bar",qfalse)==inside+4,"later whole word");
	strcpy(inside,"foo    "); Check(StringContainsWord(inside,"bar",qfalse)==NULL,"trailing delimiters");
	memset(source,'x',1023); source[1023]=0; memset(match,0,sizeof(*match));
	Check(BotFindMatch(source,match,0)==0 && strlen(match->string)==255,"terminated long native match");
	strcpy(match->string,"some test"); match->variables[0].offset=5; match->variables[0].length=4;
	BotMatchVariable(match,0,out,8); Check(!strcmp(out,"test"),"valid native span");
	BotMatchVariable(match,0,out,3); Check(!strcmp(out,"te"),"truncated output");
	BotMatchVariable(match,0,match->string,256); Check(!strcmp(match->string,"test"),"overlapping output");
	match->variables[0].offset=0; match->variables[0].length=-1;
	BotMatchVariable(match,0,out,8); Check(!*out,"negative span");
	match->variables[0].length=INT_MAX; BotMatchVariable(match,0,out,8); Check(!*out,"oversized span");
	match->variables[0].offset=5; match->variables[0].length=0;
	BotMatchVariable(match,0,out,8); Check(!*out,"span after end");
	memset(match->string,'x',256); BotMatchVariable(match,0,out,8); Check(!*out,"unterminated embedded string");
	out[0]='z'; BotMatchVariable(match,0,out,0); Check(out[0]=='z',"empty output unchanged");
	free(small); free(match); free(out); free(source); free(inside); free(buffer);
	puts("Native bot chat buffer regressions passed (issues #35/#48)"); return 0;
}
