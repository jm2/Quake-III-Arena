/* Actual native include directive, path normalization and script publication. */
#include Q3_PRECOMP_SOURCE

botlib_import_t botimport;
static source_t source;
static script_t primary;
static token_t inputs[12];
static void *owners[16];
static int liveOwners, inputCount, inputPosition, reads, lookups, errors, warnings, scriptReleases;
static char paths[4][MAX_TOKEN];
static const char *foundPath;
static void Check(int condition,const char *message) {if(!condition){fprintf(stderr,"Bot include regression failed: %s\n",message);exit(1);}}
void *GetMemory(unsigned long size) {
    int i;Check(size>0&&size<=sizeof(script_t)+sizeof(token_t),"bounded fixture native allocation");
    for(i=0;i<16;i++)if(!owners[i]){owners[i]=malloc(size);Check(owners[i]!=NULL,"fixture heap allocation");liveOwners++;return owners[i];}
    Check(0,"bounded native owners");return NULL;
}
void FreeMemory(void *pointer) {int i;for(i=0;i<16;i++)if(owners[i]==pointer){free(pointer);owners[i]=NULL;liveOwners--;return;}Check(0,"known native owner releases once");}
#ifndef Com_Memcpy
void Com_Memcpy(void *out,const void *in,size_t size) {memcpy(out,in,size);}
#endif
#ifndef Com_Memset
void Com_Memset(void *out,int value,size_t size) {memset(out,value,size);}
#endif
void QDECL Com_Error(int level,const char *format,...) {(void)level;(void)format;Check(0,"unexpected native fatal allocation");}
void QDECL Com_Printf(const char *format,...) {(void)format;Check(0,"unexpected native formatting");}
static void QDECL Print(int level,char *format,...) {
    char text[2048];va_list args;va_start(args,format);vsnprintf(text,sizeof(text),format,args);va_end(args);
    Check(strstr(text,"file scripts/main.bot, line 37:")!=NULL,"native source diagnostic");
    if(level==PRT_ERROR)errors++;else if(level==PRT_WARNING)warnings++;else Check(0,"expected native diagnostic severity");
}
int PS_ReadToken(script_t *script,token_t *token) {
    Check(script==&primary,"native include reads primary script");reads++;
    if(inputPosition==inputCount)return 0;*token=inputs[inputPosition++];return 1;
}
int EndOfScript(script_t *script) {Check(script==&primary,"native primary script EOF");return inputPosition==inputCount;}
void FreeScript(script_t *script) {Check(script!=&primary,"primary fixture script remains owned by caller");scriptReleases++;FreeMemory(script);}
void StripDoubleQuotes(char *text) {size_t length=strlen(text);Check(length>=2&&text[0]=='"'&&text[length-1]=='"',"lexer supplies quoted filename");memmove(text,text+1,length-2);text[length-2]=0;}
script_t *LoadScriptFile(const char *filename) {
    script_t *script;Check(lookups<4&&strlen(filename)<MAX_TOKEN,"bounded native lookup record");strcpy(paths[lookups++],filename);
    if(!foundPath||strcmp(foundPath,filename))return NULL;
    script=GetMemory(sizeof(script_t));memset(script,0,sizeof(*script));strcpy(script->filename,filename);return script;
}
static void Reset(const char *prefix,const char *found) {
    Check(!liveOwners&&!numtokens,"previous native ownership released");memset(&source,0,sizeof(source));memset(&primary,0,sizeof(primary));memset(inputs,0,sizeof(inputs));memset(paths,0,sizeof(paths));
    strcpy(primary.filename,"scripts/main.bot");primary.line=37;source.scriptstack=&primary;strcpy(source.includepath,prefix);
    foundPath=found;inputCount=inputPosition=reads=lookups=errors=warnings=scriptReleases=0;botimport.Print=Print;
}
static void Input(const char *text,int type,int lines) {token_t *token;Check(inputCount<12&&strlen(text)<MAX_TOKEN,"bounded native lexer fixture");token=&inputs[inputCount++];strcpy(token->string,text);token->type=type;token->line=37+lines;token->linescrossed=lines;}
static void Quote(const char *filename,int lines) {char text[MAX_TOKEN];size_t length=strlen(filename);Check(length+3<=sizeof(text),"valid quoted native token size");text[0]='"';memcpy(text+1,filename,length);text[length+1]='"';text[length+2]=0;Input(text,TT_STRING,lines);}
static void Angle(const char *filename,int closed) {Input("<",TT_PUNCTUATION,0);Input(filename,TT_NAME,0);if(closed)Input(">",TT_PUNCTUATION,0);}
static void Next(void) {Input("after",TT_NAME,1);}
static void After(void) {token_t token;Check(PC_ReadSourceToken(&source,&token)&&!strcmp(token.string,"after")&&token.linescrossed==1,"next-line token survives rejected include");}
static void Finish(void) {
    while(source.tokens){token_t *token=source.tokens;source.tokens=token->next;PC_FreeToken(token);}
    while(source.scriptstack!=&primary){script_t *script=source.scriptstack;source.scriptstack=script->next;FreeScript(script);}
    Check(!liveOwners&&!numtokens,"all native token/script owners physically released");
}
static void Valid(void) {
    char boundary[64],direct[128];Reset("scripts/","native.bot");Quote("native.bot",0);Check(PC_Directive_include(&source)&&lookups==1&&!strcmp(paths[0],"native.bot")&&source.scriptstack!=&primary&&liveOwners==1&&!errors&&!warnings,"quoted direct lookup first");Finish();
    Reset("scripts/","scripts/native.bot");Quote("native.bot",0);Check(PC_Directive_include(&source)&&lookups==2&&!strcmp(paths[0],"native.bot")&&!strcmp(paths[1],"scripts/native.bot")&&source.scriptstack!=&primary,"quoted fallback preserves native lookup order");Finish();
    Reset("scripts/","scripts/native.bot");Angle("native.bot",1);Check(PC_Directive_include(&source)&&lookups==1&&!strcmp(paths[0],"scripts/native.bot"),"angle prefix retains native path");Finish();
    Reset("scripts/","scripts/native.bot");Input("<",TT_PUNCTUATION,0);Input("native",TT_NAME,0);Input(".",TT_PUNCTUATION,0);Input("bot",TT_NAME,0);Input(">",TT_PUNCTUATION,0);Check(PC_Directive_include(&source)&&!strcmp(paths[0],"scripts/native.bot"),"angle pieces concatenate completely");Finish();
    memset(boundary,'x',63);boundary[63]=0;Reset("",boundary);Angle(boundary,1);Check(PC_Directive_include(&source)&&lookups==1&&strlen(paths[0])==63,"last fitting native include path accepted");Finish();
    Reset("",boundary);Quote(boundary,0);Check(PC_Directive_include(&source)&&lookups==1&&strlen(paths[0])==63,"last fitting direct quoted path accepted");Finish();
    memset(direct,'x',127);direct[127]=0;Reset("oversized fallback prefix/",direct);Quote(direct,0);Check(PC_Directive_include(&source)&&lookups==1&&strlen(paths[0])==127,"successful quoted direct lookup retains its original larger token capacity");Finish();
}
static void QuotedOverflow(int prefix) {
    char large[72];memset(large,'x',71);large[71]=0;Reset(prefix?large:"",NULL);Quote(prefix?"native.bot":large,0);Next();
    Check(!PC_Directive_include(&source)&&lookups==1&&errors==1&&!warnings&&source.scriptstack==&primary&&!liveOwners,"oversized quoted fallback rejects before any partial lookup");After();Finish();
}
static void AngleOverflow(int prefix,int split) {
    char large[72];memset(large,'x',71);large[71]=0;Reset(prefix?large:"","native.bot");
    Input("<",TT_PUNCTUATION,0);Input(split?"native.bot":large,TT_NAME,0);if(split)Input(large,TT_NAME,0);Input(">",TT_PUNCTUATION,0);Next();
    Check(!PC_Directive_include(&source)&&!lookups&&errors==1&&!warnings&&inputPosition==inputCount-1&&!liveOwners,"oversized angle consumes complete directive without a truncated lookup");After();Finish();
}
static void Missing(int newline) {
    Reset("","native.bot");Angle("native.bot",0);if(newline)Next();
    Check(!PC_Directive_include(&source)&&!lookups&&warnings==1&&!errors&&source.scriptstack==&primary,"incomplete angle rejects instead of loading another file");if(newline)After();Finish();
}
static void Crossed(void) {Reset("",NULL);Quote("after",1);Check(!PC_Directive_include(&source)&&!lookups&&errors==1&&source.tokens&&numtokens==1,"missing include operand unreads crossed-line token");{token_t token;Check(PC_ReadSourceToken(&source,&token)&&!strcmp(token.string,"\"after\"")&&token.linescrossed==1,"crossed quoted token preserved unchanged");}Finish();}
static void Normalize(void) {char text[]="scripts\\\\//native///bot";PC_ConvertPath(text);Check(!strcmp(text,"scripts/native/bot"),"overlapping separator removal preserves normalized native path");}
static void Prefix(void) {
    char large[1025],saved[1024];Reset("previous/",NULL);PC_SetIncludePath(&source,"");Check(!*source.includepath&&!errors,"empty include prefix remains empty");
    PC_SetIncludePath(&source,"scripts");Check(!strcmp(source.includepath,"scripts/"),"native prefix separator added");PC_SetIncludePath(&source,source.includepath);Check(!strcmp(source.includepath,"scripts/"),"aliased native prefix retains value");
    memset(large,'x',1022);large[1022]=0;PC_SetIncludePath(&source,large);Check(strlen(source.includepath)==1023&&source.includepath[1022]=='/',"last fitting complete prefix accepted");strcpy(saved,source.includepath);
    large[1022]='x';large[1023]=0;PC_SetIncludePath(&source,large);Check(!strcmp(source.includepath,saved)&&errors==1,"separator cost rejects oversized prefix without mutation");large[1023]='x';large[1024]=0;PC_SetIncludePath(&source,large);Check(!strcmp(source.includepath,saved)&&errors==2,"oversized prefix copy rejects without truncation");Finish();
}
static void Recursive(void) {Reset("","scripts/main.bot");Quote("scripts/main.bot",0);Check(!PC_Directive_include(&source)&&lookups==1&&errors==1&&scriptReleases==1&&!liveOwners&&source.scriptstack==&primary,"recursive include rejects and frees newly loaded script");Finish();}
static void Other(void) {
    Reset("prefix/",NULL);Input("<",TT_PUNCTUATION,0);Input(">",TT_PUNCTUATION,0);Next();Check(!PC_Directive_include(&source)&&!lookups&&errors==1,"empty angle filename rejects even with a prefix");After();Finish();
    Reset("",NULL);source.skip=1;Quote("unused.bot",0);Check(PC_Directive_include(&source)&&!reads&&!lookups&&inputPosition==0&&!errors&&!warnings,"skipped native include consumes nothing");Finish();
    Reset("",NULL);Check(!PC_Directive_include(&source)&&!lookups&&errors==1,"missing include filename reports native error");Finish();
    Reset("",NULL);Input("123",TT_NUMBER,0);Next();Check(!PC_Directive_include(&source)&&!lookups&&errors==1,"invalid include operand rejects");After();Finish();
}
int main(int argc,char **argv) {
    if(argc>1){int proof=atoi(argv[1]);if(proof==0)QuotedOverflow(0);else if(proof==1)AngleOverflow(1,0);else if(proof==2)AngleOverflow(0,1);else if(proof==3)Missing(1);else if(proof==4)Crossed();else if(proof==5)Normalize();else if(proof==6)Prefix();else Recursive();}
    else {Valid();QuotedOverflow(0);QuotedOverflow(1);AngleOverflow(0,0);AngleOverflow(1,0);AngleOverflow(0,1);Missing(0);Missing(1);Crossed();Normalize();Prefix();Recursive();Other();puts("Native include path boundaries, lookup order, synchronization and script ownership passed (issue #48)");}
    return 0;
}
