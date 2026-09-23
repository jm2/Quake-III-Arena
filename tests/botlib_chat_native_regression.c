/* Issues #35/#48: execute native fixed-buffer synonym and match routines. */
#include "../code/botlib/be_ai_chat.c"
#include <stdlib.h>
#include <string.h>

botlib_import_t botimport;
/** Fail on any unexpected native output. */
static void Check( int ok, const char *message ) {
	if(!ok) { fprintf(stderr,"Native bot chat regression failed: %s\n",message); exit(1); }
}
/** Keep real match routines linked without pulling in the full engine. */
void Com_Memcpy( void *dest, const void *src, size_t size ) { memcpy(dest,src,size); }
/** Support the real chat helpers' native initialization. */
void Com_Memset( void *dest, int value, size_t size ) { memset(dest,value,size); }
/** Treat unexpected shared utility errors as regression failures. */
void QDECL Com_Error( int level, const char *format, ... ) { (void)level; (void)format; Check(0,"engine error"); }
/** Ignore unrelated shared utility output. */
void QDECL Com_Printf( const char *format, ... ) { (void)format; }
/** Issue #246: variables a template never sets ("camp there" has no KEYAREA) stay unset even with unsigned char. */
static void UnsetVariables( bot_match_t *match, char *buffer, char *out ) {
	bot_matchstring_t there={" camp there",NULL};
	bot_matchpiece_t text={MT_STRING,&there,0,NULL}, name={MT_VARIABLE,NULL,0,&text};
	bot_matchtemplate_t camp={1,0,0,&name,NULL};
	char keyarea[]={ESCAPE_CHAR,'v','3',ESCAPE_CHAR,0}; int i;
	matchtemplates=&camp; memset(match,0x5a,sizeof(*match));
	Check(BotFindMatch("Sarge camp there",match,1),"camp there template");
	for(i=1;i<MAX_MATCHVARIABLES;i++) Check(match->variables[i].offset<0,"unset variable reads negative");
	BotMatchVariable(match,0,out,8); Check(!strcmp(out,"Sarge"),"set variable beside unset ones");
	strcpy(buffer,"z"); (void)BotExpandChatMessage(buffer,keyarea,0,match,1,qfalse);
	Check(!*buffer,"unset variable with stale length expands to nothing");
	for(i=1;i<MAX_MATCHVARIABLES;i++) { out[0]='z'; BotMatchVariable(match,i,out,8); Check(!*out,"unset variable reads empty"); }
	matchtemplates=NULL;
}
/** A variable starting past byte 127 does not fit the signed offset: it reads unset, never asserts or wraps. */
static void FarVariables( bot_match_t *match, char *out ) {
	char input[MAX_MESSAGE_SIZE]; int i, n;
	bot_matchstring_t kill={" kill ",NULL}, now={" now",NULL};
	bot_matchpiece_t tail={MT_STRING,&now,0,NULL}, enemy={MT_VARIABLE,NULL,1,NULL};
	bot_matchpiece_t word={MT_STRING,&kill,0,&enemy}, name={MT_VARIABLE,NULL,0,&word};
	bot_matchtemplate_t order={1,0,0,&name,NULL};
	matchtemplates=&order;
	for(i=0;i<4;i++) {
		n=121+(i&1); enemy.next=(i&2)?&tail:NULL;
		memset(input,'a',n); strcpy(input+n,(i&2)?" kill bob now":" kill bob");
		memset(match,0x5a,sizeof(*match));
		Check(BotFindMatch(input,match,1),"template with a far variable still matches");
		BotMatchVariable(match,0,out,8); Check(!strcmp(out,"aaaaaaa"),"leading variable before a far one");
		BotMatchVariable(match,1,out,8);
		if(n+6<=127) Check(match->variables[1].offset==127 && !strcmp(out,"bob"),"variable starting at byte 127");
		else Check(match->variables[1].offset==-1 && !*out,"variable starting at byte 128 reads unset");
	}
	matchtemplates=NULL;
}
/** Cover growing, shrinking, unchanged, full-capacity, and overlapping native operations. */
int main( void ) {
	char *buffer=malloc(MAX_MESSAGE_SIZE), *inside=malloc(8), *source=malloc(1024), *out=malloc(8), *small=malloc(6);
	char variableMessage[]={ESCAPE_CHAR,'v','0',ESCAPE_CHAR,0}; int i;
	bot_match_t *match=malloc(sizeof(*match));
	bot_synonym_t replacement={0}, old={0}; bot_synonymlist_t list={0};
	Check(buffer && inside && source && out && small && match,"allocation");
	strcpy(buffer,"hi hi"); StringReplaceWordsSized(buffer,"hi","hello",MAX_MESSAGE_SIZE); Check(!strcmp(buffer,"hello hello"),"growth");
	StringReplaceWordsSized(buffer,"hello","hi",MAX_MESSAGE_SIZE); Check(!strcmp(buffer,"hi hi"),"shrink");
	StringReplaceWordsSized(buffer,"hi","",MAX_MESSAGE_SIZE); Check(!strcmp(buffer," "),"empty replacement");
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
	strcpy(small,"hi hi"); BotReplaceSynonyms(small,1); Check(!strcmp(small,"hi hi"),"exact object skips growth");
	replacement.string="yo"; BotReplaceSynonyms(small,1); Check(!strcmp(small,"yo yo"),"same-size replacement");
	replacement.string="h"; strcpy(small,"hi hi"); BotReplaceSynonyms(small,1); Check(!strcmp(small,"h h"),"shrinking replacement");
	replacement.string="hello"; memset(buffer,0x5a,MAX_MESSAGE_SIZE); strcpy(buffer+128,"hi hi");
	BotReplaceSynonyms(buffer+128,1); Check(!strcmp(buffer+128,"hi hi"),"interior object skips growth");
	for(i=0;i<128;i++) Check((unsigned char)buffer[i]==0x5a,"interior prefix canary");
	for(i=134;i<MAX_MESSAGE_SIZE;i++) Check((unsigned char)buffer[i]==0x5a,"interior tail canary");
	memset(buffer,0x5a,MAX_MESSAGE_SIZE); strcpy(buffer+250,"hi hi"); BotReplaceSynonyms(buffer+250,1);
	Check(!strcmp(buffer+250,"hi hi"),"interior object at allocation end");
	for(i=0;i<250;i++) Check((unsigned char)buffer[i]==0x5a,"near-end prefix canary");
	buffer[255]=0; BotReplaceSynonyms(buffer+255,1); Check(!buffer[255],"empty final-byte object");
	BotReplaceSynonyms(NULL,1);
	memset(match,0,sizeof(*match)); strcpy(match->string,"hi");
	for(i=0;i<MAX_MATCHVARIABLES;i++) match->variables[i].offset=-1;
	match->variables[0].offset=0; match->variables[0].length=2;
	(void)BotExpandChatMessage(buffer,variableMessage,0,match,1,qfalse);
	Check(!strcmp(buffer,"hello"),"known-capacity initial variable still grows");
	(void)BotExpandChatMessage(buffer,variableMessage,0,match,1,qtrue);
	Check(!strcmp(buffer,"hello"),"known-capacity reply variable still grows");
	synonyms=NULL;
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
	UnsetVariables(match,buffer,out); FarVariables(match,out);
	free(small); free(match); free(out); free(source); free(inside); free(buffer);
	puts("Native bot chat buffer regressions passed (issues #35/#48)"); return 0;
}
