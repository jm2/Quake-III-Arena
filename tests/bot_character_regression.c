/* Actual native character parser with real source/lexer and in-memory file imports. */
#include Q3_CHARACTER_SOURCE
botlib_import_t botimport;
extern int numtokens;
#ifndef Q3_CHARACTER_MAX_REQUESTS
#define Q3_CHARACTER_MAX_REQUESTS 256
#endif
#ifndef Q3_CHARACTER_MAX_OWNERS
#define Q3_CHARACTER_MAX_OWNERS 256
#endif
static void *owners[Q3_CHARACTER_MAX_OWNERS];
static unsigned long costs[Q3_CHARACTER_MAX_OWNERS];
static unsigned long requestCosts[Q3_CHARACTER_MAX_REQUESTS];
static int liveOwners, requests, failAt, characterRequest, opens, closes, fileReads, errors, warnings, formatWarnings;
static const char *fileText;
static char openedPath[MAX_QPATH];
static void Check(int condition,const char *message) {if(!condition){fprintf(stderr,"Bot character regression failed: %s\n",message);exit(1);}}
void *GetMemory(unsigned long size) {
    int i;
#ifdef Q3_CHARACTER_HEAP_HOOK
    if(Q3_CHARACTER_HEAP_HOOK(size)){requests++;return NULL;}
#endif
    Check(size>0&&size<=65536&&requests<Q3_CHARACTER_MAX_REQUESTS,"bounded real native parser allocation");requestCosts[requests++]=size;
    if(size==sizeof(bot_character_t)+MAX_CHARACTERISTICS*sizeof(bot_characteristic_t))characterRequest=requests;
    if(requests==failAt)return NULL;
    for(i=0;i<Q3_CHARACTER_MAX_OWNERS;i++)if(!owners[i]){owners[i]=malloc(size);costs[i]=size;Check(owners[i]!=NULL,"fixture heap allocation");liveOwners++;return owners[i];}
    Check(0,"bounded native owners");return NULL;
}
void *GetClearedMemory(unsigned long size) {void *pointer=GetMemory(size);if(pointer)memset(pointer,0,size);return pointer;}
void FreeMemory(void *pointer) {int i;Check(pointer!=NULL,"native cleanup never releases a missing owner");for(i=0;i<Q3_CHARACTER_MAX_OWNERS;i++)if(owners[i]==pointer){free(pointer);owners[i]=NULL;costs[i]=0;liveOwners--;return;}Check(0,"native physical owner releases once");}
#ifndef Com_Memcpy
void Com_Memcpy(void *out,const void *in,size_t size) {memcpy(out,in,size);}
#endif
#ifndef Com_Memset
void Com_Memset(void *out,int value,size_t size) {memset(out,value,size);}
#endif
void QDECL Com_Error(int level,const char *format,...) {
#ifdef Q3_CHARACTER_FATAL_HOOK
    Q3_CHARACTER_FATAL_HOOK(level);
#else
    (void)level;Check(0,"unexpected native fatal error");
#endif
    (void)format;
}
void QDECL Com_Printf(const char *format,...) {(void)format;formatWarnings++;}
void QDECL Log_Write(char *format,...) {
#ifdef Q3_CHARACTER_LOG_HOOK
    va_list args;va_start(args,format);Q3_CHARACTER_LOG_HOOK(format,args);va_end(args);
#else
    (void)format;
#endif
}
static void QDECL Print(int level,char *format,...) {(void)format;if(level==PRT_ERROR||level==PRT_FATAL)errors++;else if(level==PRT_WARNING)warnings++;else Check(level==PRT_MESSAGE,"native print severity");}
static int Open(const char *path,fileHandle_t *file,fsMode_t mode) {Check(mode==FS_READ&&strlen(path)<sizeof(openedPath),"complete fitting native VFS path");strcpy(openedPath,path);opens++;*file=1;return (int)strlen(fileText);}
static int Read(void *out,int length,fileHandle_t file) {Check(file==1&&length==(int)strlen(fileText),"complete native fixture read");memcpy(out,fileText,length);fileReads++;return length;}
static void Close(fileHandle_t file) {Check(file==1,"known native fixture file close");closes++;}
static void Reset(const char *text) {
    int i;Check(!liveOwners&&!numtokens,"previous native parser/character owners released");for(i=0;i<=MAX_CLIENTS;i++)Check(!botcharacters[i],"previous character handle released");
    fileText=text;requests=failAt=characterRequest=opens=closes=fileReads=errors=warnings=formatWarnings=0;memset(openedPath,0,sizeof(openedPath));memset(requestCosts,0,sizeof(requestCosts));
    botimport.Print=Print;botimport.FS_FOpenFile=Open;botimport.FS_Read=Read;botimport.FS_FCloseFile=Close;
}
static void Release(bot_character_t *character) {BotFreeCharacterStrings(character);FreeMemory(character);Check(!liveOwners&&!numtokens,"all native source/character/string owners physically released");}
static void Golden(char *filename) {
    bot_character_t *character;char text[64];Reset("skill 1 { 0 1 } skill 4 { 0 42 1 1.25 79 \"native\" }");character=BotLoadCharacterFromFile(filename,4);
    Check(character&&character->skill==4&&!strcmp(character->filename,filename)&&character->c[0].type==CT_INTEGER&&character->c[0].value.integer==42&&character->c[1].type==CT_FLOAT&&character->c[1].value._float==1.25&&character->c[79].type==CT_STRING&&!strcmp(character->c[79].value.string,"native"),"real native lexer/character skill and 0/1/79 values");
    Check(opens==1&&fileReads==1&&closes==1&&!errors&&!warnings&&!formatWarnings&&liveOwners==2&&!numtokens,"successful source owners close/release before character publication");
    botcharacters[1]=character;Check(Characteristic_Integer(1,0)==42&&Characteristic_Float(1,1)==1.25,"public native numeric characteristic values");Characteristic_String(1,79,text,sizeof(text));Check(!strcmp(text,"native"),"public native characteristic 79 string");
    Check(!CheckCharacteristicIndex(1,80)&&errors==1,"public characteristic 80 remains inaccessible");BotFreeCharacter2(1);Check(!botcharacters[1]&&!liveOwners&&!numtokens,"native handle cleanup physically releases character/string owners");
}
static void BadPath(int length,int method) {
    char filename[1025];Reset("skill 4 { 0 42 }");memset(filename,'x',length);filename[length]=0;
    if(method==0)Check(BotLoadCharacterFromFile(filename,4)==NULL,"overlong character filename rejects");
    else if(method==1)Check(BotLoadCachedCharacter(filename,4,0)==0,"overlong cached filename rejects before default fallback");
    else Check(BotLoadCharacter(filename,4)==0,"overlong public filename rejects before cache/default loading");
    Check(!opens&&!requests&&!liveOwners&&!numtokens&&!formatWarnings&&errors==1,"invalid full native filename costs reject before VFS/imports");
}
static void BadIndex(unsigned long index) {
    char text[128];bot_character_t *character;snprintf(text,sizeof(text),"skill 4 { 0 \"existing\" %lu \"invalid\" }",index);Reset(text);character=BotLoadCharacterFromFile("bots/native.c",4);
    Check(character==NULL&&opens==1&&closes==1&&errors==1&&!liveOwners&&!numtokens,"inaccessible/large index rejects before assignment and frees existing strings/source");
}
static void AllocationFailure(int position) {
    bot_character_t *character;int target,i;Reset("skill 4 { 0 \"first\" 79 \"last\" }");character=BotLoadCharacterFromFile("bots/native.c",4);Check(character!=NULL,"baseline real parser allocation sequence");target=characterRequest;
    if(position)for(i=0;i<requests;i++)if(requestCosts[i]==(position==1?6UL:5UL)){target=i+1;break;}
    Check(target>0&&(position==0||target>characterRequest),"actual character/string import position identified");Release(character);
    Reset("skill 4 { 0 \"first\" 79 \"last\" }");failAt=target;character=BotLoadCharacterFromFile("bots/native.c",4);
    Check(character==NULL&&requests==failAt&&opens==1&&closes==1&&errors==1&&!liveOwners&&!numtokens,"nullable character/either string allocation releases all prior physical owners");
    failAt=0;character=BotLoadCharacterFromFile("bots/native.c",4);Check(character!=NULL&&!strcmp(character->c[79].value.string,"last"),"failed character parse can retry");Release(character);
}
static void Malformed(const char *text) {Reset(text);Check(BotLoadCharacterFromFile("bots/native.c",4)==NULL&&opens==1&&closes==1&&errors>0&&!liveOwners&&!numtokens,"malformed selected skill rejects without a partial character/source/string owner");}
static void Quotes(void) {
    char a[]="\"native\"",b[]="'native'",empty[]="",one[]="\"",single[]="'";StripDoubleQuotes(a);StripSingleQuotes(b);Check(!strcmp(a,"native")&&!strcmp(b,"native"),"real quote stripping removes surrounding delimiters without overlap");
    StripDoubleQuotes(empty);StripSingleQuotes(empty);StripDoubleQuotes(one);StripSingleQuotes(single);Check(!*empty&&!*one&&!*single,"empty/unmatched quote removal never indexes before text");
}
int main(int argc,char **argv) {
    if(argc>1){int proof=atoi(argv[1]);if(proof==0)BadPath(71,0);else if(proof==1)BadIndex(80);else if(proof==2)AllocationFailure(0);else if(proof==3)AllocationFailure(2);else if(proof==4)Malformed("skill 4 { 0 \"existing\"");else if(proof==5)BadPath(55,2);else if(proof==6)Quotes();else if(proof==7){char text[]="";StripDoubleQuotes(text);}else if(proof==8){char text[]="";StripSingleQuotes(text);}else {char text[]="'native'";StripSingleQuotes(text);Check(!strcmp(text,"native"),"single quote stripping native text");}}
    else {char boundary[55];int method;Golden("bots/native.c");memset(boundary,'x',54);boundary[54]=0;Golden(boundary);
        for(method=0;method<3;method++){BadPath(55,method);BadPath(64,method);BadPath(1023,method);}BadIndex(80);BadIndex(81);BadIndex(2147483647UL);BadIndex(4294967295UL);
        AllocationFailure(0);AllocationFailure(1);AllocationFailure(2);Malformed("skill 4 { 0 \"existing\"");Malformed("skill 4 { 0 42 79");Malformed("skill 4 { 0 42 0 1 }");Malformed("skill 4 { invalid 1 }");Malformed("skill 4 { 0 invalid }");
        Reset("skill 4 { 0 42 }");Check(BotLoadCharacterFromFile(NULL,4)==NULL&&BotLoadCharacterFromFile("",4)==NULL&&!opens&&!requests&&errors==2,"missing filename pointers/text reject before imports");
        Quotes();puts("Real native character lexer, path/index boundaries and partial-owner cleanup passed (issue #48)");}
    return 0;
}
