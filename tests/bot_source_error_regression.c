/* Actual source/include error status, lookahead, queue retention and cleanup. */
#define Q3_SOURCE_ENTRY SourceOwnerFixtureMain
#include "bot_source_regression.c"
#undef Q3_SOURCE_ENTRY
#ifdef Q3_SOURCE_QUERY
#include Q3_SOURCE_QUERY
#endif
static const char *innerText,*leafText;
static int ErrorCatalogOpen(const char *path,fileHandle_t *file,fsMode_t mode) {
    fileText=!strcmp(path,"one.c")?innerText:leafText;reportLength=(int)strlen(fileText);closed=0;return SourceOpen(path,file,mode);
}
static source_t *ErrorSource(char *text) {
    source_t *source;SourceReset(text);source=LoadSourceMemory(text,(int)strlen(text),"root");Check(source&&liveOwners==4&&!PC_SourceHasError(source),"real root source prepares without recorded error");return source;
}
static void ErrorInclude(int deep,int comment) {
    source_t *source=ErrorSource("head\n#include \"one.c\"\nparent");script_t *root=source->scriptstack,*failed;token_t token,cached;token_t *queue;int imports;
    leafText=comment?"184467440737095516160":"\"\\q\"";innerText=deep?"#include \"two.c\"\nchild":leafText;botimport.FS_FOpenFile=ErrorCatalogOpen;
    ReadGolden(source,"head",TT_NAME);cached=source->token;
    Check(!PC_ReadToken(source,&token),"malformed include rejects instead of continuing into its parent");failed=source->scriptstack;
    Check(failed!=root&&PC_SourceHasError(source)&&errors==1&&opens==deep+1&&closes==opens&&liveOwners==(deep?8:6)&&!numtokens,"failed include stack remains physically owned with recorded lexical status");
    memset(&token,0,sizeof(token));token.type=TT_NAME;strcpy(token.string,"queued");PC_UnreadToken(source,&token);queue=source->tokens;imports=requests;
    Check(queue&&numtokens==1&&!PC_ReadToken(source,&token)&&!PC_ReadToken(source,&token)&&source->tokens==queue&&source->scriptstack==failed&&requests==imports&&!memcmp(&cached,&source->token,sizeof(cached)),"failed source preserves queued owner and prior published token across later reads");SourceDone(source);
}
static void ErrorLookahead(void) {
    source_t *source=ErrorSource("\"prefix\" 184467440737095516160 tail");token_t token;
    Check(!PC_ReadToken(source,&token),"string lookahead failure rejects the outer token");Check(PC_SourceHasError(source)&&errors==1&&liveOwners==4&&!numtokens,"lookahead records failure without partial owner publication");SourceDone(source);
}
static void ErrorQueue(void) {
    source_t *source=ErrorSource("\"\\q\"");token_t token;token_t *queue;int imports;
    Check(!PC_ReadToken(source,&token)&&PC_SourceHasError(source),"root lexical error records failure");memset(&token,0,sizeof(token));token.type=TT_NAME;strcpy(token.string,"queued");PC_UnreadToken(source,&token);queue=source->tokens;imports=requests;
    Check(!PC_ReadToken(source,&token)&&source->tokens==queue&&numtokens==1&&requests==imports,"lexical failure cannot drain source queue");SourceDone(source);
}
static void ErrorHistory(void) {
    source_t *source=ErrorSource("head\n#include \"one.c\"\nparent");script_t *root=source->scriptstack;token_t token;
    innerText="#unknown\nchild";leafText="";botimport.FS_FOpenFile=ErrorCatalogOpen;ReadGolden(source,"head",TT_NAME);
    Check(!PC_ReadToken(source,&token)&&PC_SourceHasError(source)&&source->scriptstack!=root&&errors==1,"preprocessor diagnostic records include error");
    ReadGolden(source,"child",TT_NAME);ReadGolden(source,"parent",TT_NAME);
    Check(source->scriptstack==root&&PC_SourceHasError(source)&&liveOwners==4&&!numtokens,"popped include transfers error history to remaining parent");SourceDone(source);
}
static void ErrorSilent(int silent) {
    source_t *source=ErrorSource("\"\\q\"");token_t token;SetScriptFlags(source->scriptstack,silent?SCFL_NOERRORS:0);
    Check(!PC_ReadToken(source,&token)&&PC_SourceHasError(source)&&errors==(silent?0:1),"diagnostic suppression preserves actual lexical failure status");SetScriptFlags(source->scriptstack,0);
    Check(PC_SourceHasError(source)&&!PC_ReadToken(source,&token),"formatting flags cannot clear recorded source error");SourceDone(source);
}
static void ErrorScriptCache(void) {
    source_t *source=ErrorSource("head");token_t token;ReadGolden(source,"head",TT_NAME);ScriptError(source->scriptstack,"record failure");PS_UnreadLastToken(source->scriptstack);
    Check(!PS_ReadToken(source->scriptstack,&token)&&source->scriptstack->tokenavailable==1&&PC_SourceHasError(source),"lexical failure cannot publish or consume cached script token");SourceDone(source);
}
static void ErrorFlags(void) {
    source_t *source=ErrorSource("tail");ScriptError(source->scriptstack,"record failure");SetScriptFlags(source->scriptstack,SCFL_NOERRORS);
    Check(PC_SourceHasError(source)&&(source->scriptstack->flags&SCFL_NOERRORS),"formatting setter preserves internal parse status");SourceDone(source);
}
static void ErrorComment(void) {
    source_t *source=ErrorSource("\"prefix\" /* incomplete");token_t token;Check(!PC_ReadToken(source,&token)&&PC_SourceHasError(source)&&errors==1,"raw-memory unterminated comment rejects the preceding concatenation token");SourceDone(source);
}
static void ErrorOrdinary(int deep) {
    source_t *source=ErrorSource("head\n#include \"one.c\"\nparent");token_t token;innerText=deep?"#include \"two.c\"\nchild":"native 42";leafText="native 42";botimport.FS_FOpenFile=ErrorCatalogOpen;
    ReadGolden(source,"head",TT_NAME);ReadGolden(source,"native",TT_NAME);ReadGolden(source,"42",TT_NUMBER);if(deep)ReadGolden(source,"child",TT_NAME);ReadGolden(source,"parent",TT_NAME);
    Check(!PC_ReadToken(source,&token)&&!PC_SourceHasError(source)&&liveOwners==4&&!numtokens&&!errors&&!warnings&&opens==deep+1&&closes==opens,"native complete nested include EOF releases child owners without false error status");SourceDone(source);
}
int main(int argc,char **argv) {
    if(argc>1){int proof=atoi(argv[1]);if(proof<2)ErrorInclude(proof,proof);else if(proof==2)ErrorLookahead();else if(proof==3)ErrorQueue();else if(proof==4)ErrorHistory();else if(proof==5)ErrorSilent(1);else if(proof==6)ErrorScriptCache();else if(proof==7)ErrorFlags();else if(proof==8)ErrorComment();else {ErrorOrdinary(0);ErrorOrdinary(1);}}
    else {ErrorInclude(0,0);ErrorInclude(1,1);ErrorLookahead();ErrorQueue();ErrorHistory();ErrorSilent(0);ErrorSilent(1);ErrorScriptCache();ErrorFlags();ErrorComment();ErrorOrdinary(0);ErrorOrdinary(1);Check(!PC_SourceHasError(NULL),"missing source has no parse status");puts("Real source error status, include/lookahead stop, recovery history and owned cleanup passed (issue #48)");}
    return 0;
}
