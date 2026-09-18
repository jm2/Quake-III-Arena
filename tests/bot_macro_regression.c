/* Actual token builders, macro expansion and private-chain rollback. */
static int RejectAllocation(void);
static void FatalDiagnostic(int level);
#define Q3_INCLUDE_HEAP_HOOK RejectAllocation
#define Q3_INCLUDE_FATAL_HOOK FatalDiagnostic
#define main IncludeFixtureMain
#include "bot_include_regression.c"
#undef main
static int requests, failAt, fatalErrors;
static define_t define;
static token_t definition[8], parameters[128], invocation;
static int definitionCount;
static int RejectAllocation(void) {requests++;return requests==failAt;}
static void FatalDiagnostic(int level) {Check(level==ERR_FATAL,"native copied-token fatal diagnostic");fatalErrors++;}
static void InitToken(token_t *token,const char *text,int type) {memset(token,0,sizeof(*token));strcpy(token->string,text);token->type=type;token->subtype=(int)strlen(text);token->line=37;token->linescrossed=2;token->intvalue=123;token->floatvalue=1.25;}
static void MacroReset(int count) {
    int i;requests=failAt=fatalErrors=0;Reset("",NULL);memset(&define,0,sizeof(define));memset(definition,0,sizeof(definition));memset(parameters,0,sizeof(parameters));definitionCount=0;
    define.name="macro";define.numparms=count;InitToken(&invocation,"macro",TT_NAME);
    for(i=0;i<count&&i<128;i++){char name[16];sprintf(name,"p%d",i);InitToken(&parameters[i],name,TT_NAME);if(i)parameters[i-1].next=&parameters[i];}
    if(count>0&&count<=128)define.parms=parameters;
}
static void Body(const char *text,int type) {token_t *token;Check(definitionCount<8,"bounded native definition fixture");token=&definition[definitionCount++];InitToken(token,text,type);if(definitionCount>1)definition[definitionCount-2].next=token;define.tokens=definition;}
static void ReleaseChain(token_t *token) {while(token){token_t *next=token->next;PC_FreeToken(token);token=next;}}
static void LongText(char *text,int length,char value,int quoted) {
    int begin=quoted?1:0;Check(length>=begin*2&&length<MAX_TOKEN,"bounded literal input token");if(quoted)text[0]='"';memset(text+begin,value,length-begin*2);if(quoted)text[length-1]='"';text[length]=0;
}
static void NativeGoldens(void) {
    const struct {char *a,*b,*result;int first,second;} cases[]={
        {"native","Suffix","nativeSuffix",TT_NAME,TT_NAME},{"native","123","native123",TT_NAME,TT_NUMBER},
        {"","name","name",TT_NAME,TT_NAME},{"name","","name",TT_NAME,TT_NAME},
        {"\"a\"","\"b\"","\"ab\"",TT_STRING,TT_STRING},{"\"\"","\"b\"","\"b\"",TT_STRING,TT_STRING},
        {"\"a\"","\"\"","\"a\"",TT_STRING,TT_STRING},{"\"\"","\"\"","\"\"",TT_STRING,TT_STRING}
    };size_t i;token_t a,b,out;MacroReset(0);
    for(i=0;i<sizeof(cases)/sizeof(cases[0]);i++){InitToken(&a,cases[i].a,cases[i].first);InitToken(&b,cases[i].b,cases[i].second);Check(PC_MergeTokens(&a,&b)&&!strcmp(a.string,cases[i].result)&&a.type==cases[i].first,"literal native paste text/type");}
    InitToken(&a,"native",TT_NAME);InitToken(&b,"123",TT_NUMBER);a.next=&b;InitToken(&out,"prior",TT_NAME);
    Check(PC_StringizeTokens(&a,&out)&&!strcmp(out.string,"\"native123\"")&&out.type==TT_STRING,"literal native stringize concatenation");
    Check(PC_StringizeTokens(NULL,&out)&&!strcmp(out.string,"\"\""),"literal native empty stringize");Finish();
}
static void StringizeBoundary(int length,int count,int accepted) {
    token_t a,b,out,saved;char text[MAX_TOKEN];MacroReset(0);LongText(text,length,'x',0);InitToken(&a,text,TT_NAME);InitToken(&b,"y",TT_NAME);if(count==2)a.next=&b;
    InitToken(&out,"prior",TT_NAME);out.whitespace_p=primary.filename;out.endwhitespace_p=primary.filename+1;saved=out;
    if(accepted){Check(PC_StringizeTokens(&a,&out)&&strlen(out.string)==(size_t)(length+count+1)&&out.string[0]=='"'&&out.string[length+count]=='"'&&out.type==TT_STRING&&out.subtype==length+count+1,"full fitting stringize text and defined subtype");Check(!out.whitespace_p&&!out.endwhitespace_p&&out.line==37&&out.linescrossed==2&&out.intvalue==123&&out.floatvalue==1.25,"valid stringize retains initialized non-format metadata");}
    else Check(!PC_StringizeTokens(&a,&out)&&!memcmp(&out,&saved,sizeof(saved)),"oversized stringize rejects without output mutation");Finish();
}
static void PasteBoundary(int first,int second,int quoted,int accepted) {
    token_t a,b,saved;char text[MAX_TOKEN];MacroReset(0);LongText(text,first,'a',quoted);InitToken(&a,text,quoted?TT_STRING:TT_NAME);LongText(text,second,'b',quoted);InitToken(&b,text,quoted?TT_STRING:TT_NAME);saved=a;
    if(accepted)Check(PC_MergeTokens(&a,&b)&&strlen(a.string)==(size_t)(first+second-2*quoted)&&a.type==saved.type&&a.subtype==saved.subtype,"last fitting paste retains full native text/type/copied metadata");
    else Check(!PC_MergeTokens(&a,&b)&&!memcmp(&a,&saved,sizeof(saved)),"oversized paste rejects before output mutation");Finish();
}
static void EmptyTypedString(void) {token_t a,b,saved;MacroReset(0);InitToken(&a,"",TT_STRING);InitToken(&b,"\"b\"",TT_STRING);saved=a;Check(!PC_MergeTokens(&a,&b)&&!memcmp(&a,&saved,sizeof(saved)),"empty malformed string cannot index before buffer");Finish();}
static void Aliases(void) {
    token_t a,b;MacroReset(0);InitToken(&a,"native",TT_NAME);InitToken(&b,"123",TT_NUMBER);a.next=&b;
    Check(PC_StringizeTokens(&a,&a)&&!strcmp(a.string,"\"native123\"")&&a.next==&b,"aliased stringize builds privately before output mutation");
    InitToken(&a,"native",TT_NAME);Check(PC_MergeTokens(&a,&a)&&!strcmp(a.string,"nativenative"),"complete append safely handles an aliased name input");Finish();
}
static void PasteSetup(int tooLong) {
    char text[MAX_TOKEN];MacroReset(2);Body("p0",TT_NAME);Body("##",TT_PUNCTUATION);Body("p1",TT_NAME);Body("tail",TT_NAME);
    Input("(",TT_PUNCTUATION,0);if(tooLong){LongText(text,800,'a',0);Input(text,TT_NAME,0);}else Input("native",TT_NAME,0);
    Input(",",TT_PUNCTUATION,0);if(tooLong){LongText(text,800,'b',0);Input(text,TT_NAME,0);}else Input("Suffix",TT_NAME,0);Input(")",TT_PUNCTUATION,0);
}
static void PasteOutput(token_t *first,token_t *last) {Check(first&&first->next==last&&last&&!last->next&&!strcmp(first->string,"nativeSuffix")&&!strcmp(last->string,"tail")&&liveOwners==2&&numtokens==2,"actual parameter substitution/paste publishes two native tokens");ReleaseChain(first);Check(!liveOwners&&!numtokens,"successful pasted output physically releases");}
static void PasteFailure(int failure) {
    token_t *first,*last;PasteSetup(0);failAt=failure;first=last=&invocation;
    Check(!PC_ExpandDefine(&source,&invocation,&define,&first,&last)&&!first&&!last&&!liveOwners&&!numtokens&&fatalErrors==1&&requests==failure,"every returned nullable parameter/output copy rolls back all private owners");
    failAt=0;inputPosition=0;Check(PC_ExpandDefine(&source,&invocation,&define,&first,&last),"failed expansion can retry");PasteOutput(first,last);Finish();
}
static void PasteOverflow(void) {token_t *first,*last;PasteSetup(1);Check(!PC_ExpandDefine(&source,&invocation,&define,&first,&last)&&!first&&!last&&!liveOwners&&!numtokens&&errors==1&&!fatalErrors,"oversized parameter paste releases parameter and partial output chains");Finish();}
static void StringizeMacro(int tooLong) {
    token_t *first,*last;char text[MAX_TOKEN];MacroReset(1);Body("head",TT_NAME);Body("#",TT_PUNCTUATION);Body("p0",TT_NAME);Body("tail",TT_NAME);
    Input("(",TT_PUNCTUATION,0);if(tooLong){LongText(text,1021,'x',0);Input(text,TT_NAME,0);}else Input("native",TT_NAME,0);Input("123",TT_NUMBER,0);Input(")",TT_PUNCTUATION,0);
    if(tooLong)Check(!PC_ExpandDefine(&source,&invocation,&define,&first,&last)&&!first&&!last&&!liveOwners&&!numtokens&&errors==1,"oversized stringize releases arguments and already-copied output");
    else {Check(PC_ExpandDefine(&source,&invocation,&define,&first,&last)&&first&&first->next&&first->next->next==last&&!last->next&&!strcmp(first->string,"head")&&!strcmp(first->next->string,"\"native123\"")&&!strcmp(last->string,"tail")&&first->next->type==TT_STRING&&first->next->subtype==11&&first->next->line==37&&first->next->linescrossed==2&&!first->next->whitespace_p&&!first->next->endwhitespace_p&&liveOwners==3&&numtokens==3,"actual stringized substitution has full native text and initialized invocation metadata");ReleaseChain(first);}Finish();
}
static void Incomplete(void) {token_t *first,*last;MacroReset(1);Body("p0",TT_NAME);Input("(",TT_PUNCTUATION,0);Input("native",TT_NAME,0);Check(!PC_ExpandDefine(&source,&invocation,&define,&first,&last)&&!first&&!last&&!liveOwners&&!numtokens&&errors==1,"incomplete argument read releases already-copied parameters");Finish();}
static void Unsupported(int success) {
    token_t saved,*previous;MacroReset(0);Body(success?"native":"1",success?TT_NAME:TT_NUMBER);Body("##",TT_PUNCTUATION);Body(success?"Suffix":"2",success?TT_NAME:TT_NUMBER);
    InitToken(&saved,"previous",TT_NAME);previous=PC_CopyToken(&saved);source.tokens=previous;
    if(success){Check(PC_ExpandDefineIntoSource(&source,&invocation,&define)&&source.tokens!=previous&&source.tokens->next==previous&&liveOwners==2&&numtokens==2,"successful source expansion links before prior queued tokens");Check(PC_ReadSourceToken(&source,&saved)&&!strcmp(saved.string,"nativeSuffix"),"actual source consumes pasted token");}
    else Check(!PC_ExpandDefineIntoSource(&source,&invocation,&define)&&source.tokens==previous&&liveOwners==1&&numtokens==1&&errors==1,"unsupported paste releases output while preserving prior source queue");
    Check(PC_ReadSourceToken(&source,&saved)&&!strcmp(saved.string,"previous"),"prior queued source token remains intact");Finish();
}
static void ParameterCounts(void) {
    int count;for(count=-1;count<=129;count+=130){token_t *first,*last;MacroReset(count);Check(!PC_ExpandDefine(&source,&invocation,&define,&first,&last)&&!first&&!last&&!requests&&!liveOwners&&errors==1,"invalid parameter counts reject before read/import");Finish();}
    {token_t *first,*last;MacroReset(128);Body("literal",TT_NAME);Input("(",TT_PUNCTUATION,0);Input("native",TT_NAME,0);Input(")",TT_PUNCTUATION,0);Check(PC_ExpandDefine(&source,&invocation,&define,&first,&last)&&first==last&&!strcmp(first->string,"literal")&&warnings==1&&liveOwners==1,"native maximum parameter count and too-few warning remain accepted");ReleaseChain(first);Finish();}
}
int main(int argc,char **argv) {
    if(argc>1){int proof=atoi(argv[1]);if(proof==0)StringizeBoundary(1022,1,0);else if(proof==1)PasteBoundary(800,800,0,0);else if(proof==2)PasteBoundary(800,800,1,0);else if(proof==3)EmptyTypedString();else if(proof==4)Unsupported(0);else if(proof==5)Incomplete();else if(proof==6)PasteFailure(1);else if(proof==7)Aliases();else NativeGoldens();}
    else {int failure;NativeGoldens();StringizeBoundary(0,1,1);StringizeBoundary(1021,1,1);StringizeBoundary(1020,2,1);StringizeBoundary(1022,1,0);StringizeBoundary(1021,2,0);PasteBoundary(511,512,0,1);PasteBoundary(512,512,0,0);PasteBoundary(512,513,1,1);PasteBoundary(513,513,1,0);EmptyTypedString();Aliases();for(failure=1;failure<=6;failure++)PasteFailure(failure);PasteOverflow();StringizeMacro(0);StringizeMacro(1);Incomplete();Unsupported(0);Unsupported(1);ParameterCounts();puts("Native macro token bounds, substitution, metadata and private-chain rollback passed (issue #48)");}
    return 0;
}
