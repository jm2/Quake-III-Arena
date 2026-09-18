/* Actual native builtin expansion and copied-token ownership. */
#define time FixtureTime
#define ctime FixtureCtime
#include Q3_PRECOMP_SOURCE
#undef time
#undef ctime

botlib_import_t botimport;
static void *owner;
static int allocations, releases, failAllocation, failTime, errors, fatalErrors;
static char borrowedTime[] = "Fri Sep 18 13:17:26 2026\n";
static char whitespace[] = "  ";
static source_t source;
static script_t script;
static token_t original;

static void Check(int condition, const char *message) {
    if (!condition) {fprintf(stderr,"Bot builtin regression failed: %s\n",message);exit(1);}
}
time_t FixtureTime(time_t *out) {time_t result = 1790000000;if(out)*out=result;return result;}
char *FixtureCtime(const time_t *input) {Check(input&&*input==1790000000,"native time passed to conversion");return failTime?NULL:borrowedTime;}
void *GetMemory(unsigned long size) {
    Check(size==sizeof(token_t)&&!owner,"single exact native copied-token owner");allocations++;
    if(failAllocation)return NULL;
    owner=malloc(size);Check(owner!=NULL,"fixture token allocation");return owner;
}
void FreeMemory(void *pointer) {Check(pointer&&pointer==owner,"copied-token storage releases once");free(pointer);owner=NULL;releases++;}
#ifndef Com_Memcpy
void Com_Memcpy(void *out,const void *in,size_t size) {memcpy(out,in,size);}
#endif
#ifndef Com_Memset
void Com_Memset(void *out,int value,size_t size) {memset(out,value,size);}
#endif
int PS_ReadToken(script_t *input,token_t *token) {(void)input;(void)token;Check(0,"unexpected script token read");return 0;}
int EndOfScript(script_t *input) {(void)input;Check(0,"unexpected script end check");return 0;}
void FreeScript(script_t *input) {(void)input;Check(0,"unexpected script release");}
script_t *LoadScriptFile(const char *filename) {(void)filename;Check(0,"unexpected script file load");return NULL;}
void StripDoubleQuotes(char *text) {(void)text;Check(0,"unexpected script quote conversion");}
void QDECL Com_Error(int level,const char *format,...) {(void)format;Check(level==ERR_FATAL,"native token allocation fatal error");fatalErrors++;}
void QDECL Com_Printf(const char *format,...) {(void)format;Check(0,"unexpected native formatting");}
static void QDECL Print(int level,char *format,...) {
    char text[2048];va_list args;Check(level==PRT_ERROR,"native expansion error severity");
    va_start(args,format);vsnprintf(text,sizeof(text),format,args);va_end(args);
    Check(strstr(text,"could not expand __")!=NULL,"native time expansion diagnostic");errors++;
}
static void Reset(void) {
    Check(!owner&&!numtokens,"no prior copied-token owner");memset(&source,0,sizeof(source));memset(&script,0,sizeof(script));memset(&original,0,sizeof(original));
    strcpy(script.filename,"scripts/native.bot");script.line=37;source.scriptstack=&script;
    strcpy(original.string,"__synthetic__");original.type=TT_NAME;original.subtype=13;original.line=37;original.linescrossed=2;
    original.whitespace_p=whitespace;original.endwhitespace_p=whitespace+2;original.intvalue=123;original.floatvalue=1.25;
    original.next=&original;allocations=releases=failAllocation=failTime=errors=fatalErrors=0;botimport.Print=Print;
}
static int Expand(int builtin,token_t **first,token_t **last) {
    define_t define;memset(&define,0,sizeof(define));define.builtin=builtin;
    *first=*last=&original;return PC_ExpandBuiltinDefine(&source,&original,&define,first,last);
}
static void Golden(int builtin,const char *text,int type,int subtype) {
    token_t *first,*last,saved;Reset();saved=original;
    Check(Expand(builtin,&first,&last)==qtrue,"native builtin succeeds");
    Check(first&&last==first&&owner==first&&numtokens==1&&allocations==1&&!releases,"one published native token");
    Check(!strcmp(first->string,text)&&first->type==type&&first->subtype==subtype,"literal native token text/type/subtype");
    Check(first->line==37&&first->linescrossed==2&&first->whitespace_p==whitespace&&first->endwhitespace_p==whitespace+2&&!first->next,"source location and whitespace metadata preserved");
    if(builtin==BUILTIN_LINE)Check(first->intvalue==37&&first->floatvalue==37,"native line numeric values");
    else Check(first->intvalue==123&&first->floatvalue==1.25,"non-number native copied metadata");
    Check(!memcmp(&original,&saved,sizeof(saved))&&!errors&&!fatalErrors,"input token unchanged and no errors");
    Check(!strcmp(borrowedTime,"Fri Sep 18 13:17:26 2026\n"),"borrowed time text unchanged");PC_FreeToken(first);
    Check(!owner&&!numtokens&&releases==1,"published native token physically released");
}
static void MissingTime(int builtin) {
    token_t *first,*last;Reset();failTime=1;
    Check(Expand(builtin,&first,&last)==qfalse,"failed C runtime conversion rejects expansion");
    Check(!first&&!last&&!owner&&!numtokens&&allocations==1&&releases==1&&errors==1&&!fatalErrors,"failed expansion releases copied token and publishes no result");
    failTime=0;Check(Expand(builtin,&first,&last)==qtrue&&first==last&&numtokens==1,"failed conversion can retry");PC_FreeToken(first);Check(!owner&&!numtokens&&releases==2,"retried token physically released");
}
static void Empty(int builtin) {
    token_t *first,*last;Reset();Check(Expand(builtin,&first,&last)==qtrue,"native unsupported builtin remains empty success");
    Check(!first&&!last&&!owner&&!numtokens&&allocations==1&&releases==1&&!errors&&!fatalErrors,"empty result does not leak a copied token");
}
static void Nullable(void) {
    int builtin;for(builtin=BUILTIN_LINE;builtin<=BUILTIN_STDC;builtin++) {
        token_t *first,*last;Reset();failAllocation=1;Check(Expand(builtin,&first,&last)==qfalse,"returned nullable copy rejects expansion");
        Check(!first&&!last&&!owner&&!numtokens&&allocations==1&&!releases&&!errors&&fatalErrors==1,"nullable copy retains native fatal diagnostic and no result");
    }
}
static void FileBoundary(void) {
    token_t *first,*last;Reset();memset(script.filename,'x',sizeof(script.filename)-1);script.filename[sizeof(script.filename)-1]=0;
    Check(Expand(BUILTIN_FILE,&first,&last)==qtrue&&first==last&&first->subtype==1023&&!strcmp(first->string,script.filename),"maximum valid native filename fits token unchanged");PC_FreeToken(first);
}
int main(int argc,char **argv) {
    if(argc>1) {int proof=atoi(argv[1]);if(proof==0)Golden(BUILTIN_DATE,"\"Sep 18 2026\"",TT_NAME,13);else if(proof==1)Golden(BUILTIN_TIME,"\"13:17:26\"",TT_NAME,10);else if(proof==2)MissingTime(BUILTIN_DATE);else if(proof==3)MissingTime(BUILTIN_TIME);else if(proof==4)Empty(BUILTIN_STDC);else Nullable();}
    else {int repeat;for(repeat=0;repeat<3;repeat++) {Golden(BUILTIN_DATE,"\"Sep 18 2026\"",TT_NAME,13);Golden(BUILTIN_TIME,"\"13:17:26\"",TT_NAME,10);}Golden(BUILTIN_LINE,"37",TT_NUMBER,TT_DECIMAL|TT_INTEGER);Golden(BUILTIN_FILE,"scripts/native.bot",TT_NAME,18);FileBoundary();MissingTime(BUILTIN_DATE);MissingTime(BUILTIN_TIME);Empty(BUILTIN_STDC);Empty(99);Nullable();puts("Native builtin token text, borrowed time storage and copied-token ownership passed (issue #48)");}
    return 0;
}
