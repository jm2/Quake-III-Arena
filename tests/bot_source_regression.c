/* Actual script/source creation, native punctuation, global copies and I/O rollback. */
static int LargeNullable(unsigned long size);
static void ReturnedFatal(int level);
#define Q3_CHARACTER_HEAP_HOOK LargeNullable
#define Q3_CHARACTER_FATAL_HOOK ReturnedFatal
#define main CharacterFixtureMain
#include "bot_character_regression.c"
#undef main
extern define_t *globaldefines;
static int fatalErrors, reportLength, shortRead, closed, allowLarge;
static int LargeNullable(unsigned long size) {return allowLarge&&size>65536;}
static void ReturnedFatal(int level) {Check(level==ERR_FATAL,"native token-copy fatal error preserved");fatalErrors++;}
static int SourceOpen(const char *path,fileHandle_t *file,fsMode_t mode) {Check(mode==FS_READ&&strlen(path)<sizeof(openedPath),"source lookup has complete native path");strcpy(openedPath,path);opens++;*file=1;return reportLength;}
static int SourceRead(void *out,int length,fileHandle_t file) {int amount;Check(file==1&&!closed&&length==reportLength&&length>=0&&length==(int)strlen(fileText),"read only validated representable fixture bytes");amount=shortRead?length-1:length;if(amount>0)memcpy(out,fileText,amount);fileReads++;return amount;}
static void SourceClose(fileHandle_t file) {Check(file==1&&!closed,"source file closes exactly once");closes++;closed=1;}
static void SourceReset(const char *text) {Reset(text);fatalErrors=shortRead=closed=allowLarge=0;reportLength=(int)strlen(text);botimport.FS_FOpenFile=SourceOpen;botimport.FS_Read=SourceRead;botimport.FS_FCloseFile=SourceClose;PC_SetBaseFolder("");}
static void SourceDone(source_t *source) {FreeSource(source);Check(!liveOwners&&!numtokens,"all actual script/table/source/dictionary/token owners physically release");}
static void ReadGolden(source_t *source,const char *text,int type) {token_t token;Check(PC_ReadToken(source,&token)&&!strcmp(token.string,text)&&token.type==type,"literal real source lexer/macro token");}
static void Plain(int memory,int empty) {
    source_t *source;char *text=empty?"":"native { 42 \"text\" }";SourceReset(text);
    source=memory?LoadSourceMemory(text,(int)strlen(text),"memory-label"):LoadSourceFile("scripts/native.c");
    Check(source&&liveOwners==4&&!numtokens&&!errors&&!formatWarnings,"all four required source owners complete before publication");
    Check(memory?(!opens&&!closes&&!fileReads):(opens==1&&closes==1&&fileReads==1),"native file versus memory import lifecycle");
    if(!empty){ReadGolden(source,"native",TT_NAME);ReadGolden(source,"{",TT_PUNCTUATION);ReadGolden(source,"42",TT_NUMBER);ReadGolden(source,"\"text\"",TT_STRING);ReadGolden(source,"}",TT_PUNCTUATION);}
    {token_t token;Check(!PC_ReadToken(source,&token),"real source EOF");}SourceDone(source);
}
static void BaseFailure(int memory,int position) {
    source_t *source;SourceReset("native { 42 }");failAt=position;
    source=memory?LoadSourceMemory((char *)fileText,(int)strlen(fileText),"memory-label"):LoadSourceFile("scripts/native.c");
    if(source||requests!=position||liveOwners||numtokens||fatalErrors)fprintf(stderr,"Source allocation state: memory=%d position=%d result=%p requests=%d owners=%d tokens=%d fatals=%d\n",memory,position,(void *)source,requests,liveOwners,numtokens,fatalErrors);
    Check(!source&&requests==position&&!liveOwners&&!numtokens&&!fatalErrors,"every required source allocation fails without partial owners");
    Check(memory?(!opens&&!closes&&!fileReads):(opens==1&&closes==1&&fileReads==(position>2)),"nullable source stage retains exact native file closure/read order");
    failAt=0;closed=0;source=memory?LoadSourceMemory((char *)fileText,(int)strlen(fileText),"memory-label"):LoadSourceFile("scripts/native.c");Check(source&&liveOwners==4,"failed base source creation retries");SourceDone(source);
}
static void InvalidCost(int memory,int length) {
    source_t *source;SourceReset("native");allowLarge=1;reportLength=length;
    source=memory?LoadSourceMemory((char *)fileText,length,"memory-label"):LoadSourceFile("scripts/native.c");
    Check(!source&&!requests&&!liveOwners&&!numtokens&&!fileReads,"negative/overflowing complete script costs reject before imports");
    Check(memory?(!opens&&!closes):(opens==1&&closes==1),"invalid file length closes the opened owner");
}
static void Short(void) {source_t *source;SourceReset("native { 42 }");shortRead=1;source=LoadSourceFile("scripts/native.c");Check(!source&&opens==1&&closes==1&&fileReads==1&&requests==2&&!liveOwners&&!numtokens,"short file read rejects and releases script/punctuation owners before dictionary allocation");}
static void Names(void) {
    char name[1025],path[64];source_t *source;SourceReset("");memset(name,'x',1023);name[1023]=0;source=LoadSourceMemory("",0,name);
    Check(source&&strlen(source->filename)==1023&&!strcmp(source->filename,name)&&!strcmp(source->scriptstack->filename,name),"maximum native memory label remains terminated in both owners");SourceDone(source);
    SourceReset("");name[1023]='x';name[1024]=0;Check(!LoadSourceMemory(NULL,0,name)&&!requests&&!liveOwners,"overlong memory label rejects before imports");
    Check(!LoadSourceMemory(NULL,1,"label")&&!LoadSourceMemory(NULL,0,NULL)&&!requests,"invalid memory pointers reject before imports");
    source=LoadSourceMemory(NULL,0,"");Check(source&&!*source->filename&&!opens,"zero-byte anonymous memory source permits a null data pointer");SourceDone(source);
    SourceReset("");memset(path,'x',63);path[63]=0;source=LoadSourceFile(path);Check(source&&strlen(openedPath)==63,"last fitting native file path accepted");SourceDone(source);
    SourceReset("");memset(name,'x',64);name[64]=0;Check(!LoadSourceFile(name)&&!opens&&!requests,"next native path byte rejects before lookup");
    SourceReset("");PC_SetBaseFolder("prefix");memset(path,'x',56);path[56]=0;source=LoadSourceFile(path);Check(source&&strlen(openedPath)==63,"last fitting complete base/name path accepted");SourceDone(source);
    SourceReset("");PC_SetBaseFolder("prefix");memset(path,'x',57);path[57]=0;Check(!LoadSourceFile(path)&&!opens&&!requests&&!formatWarnings,"oversized complete path rejects instead of native truncation");
}
static void MemoryMacros(void) {
    char *text="#define join(a,b) a ## b\n#define alias 42\njoin(native,Suffix) alias";source_t *source;SourceReset(text);source=LoadSourceMemory(text,(int)strlen(text),"real-memory-macros");Check(source!=NULL,"real memory macro source creates");ReadGolden(source,"nativeSuffix",TT_NAME);ReadGolden(source,"42",TT_NUMBER);{token_t token;Check(!PC_ReadToken(source,&token)&&!errors&&!warnings,"real memory macro source parses to EOF without errors");}SourceDone(source);
}
static void HighByte(void) {
    char text[]={(char)255,0};script_t *script;token_t token;SourceReset("");script=LoadScriptMemory(text,1,"high-byte");Check(script!=NULL,"high-byte memory script creates");
    Check(!PS_ReadToken(script,&token)&&errors==1,"native high-byte punctuation rejects without signed array index");FreeScript(script);Check(!liveOwners&&!numtokens,"rejected high-byte script physically releases");
}
static void AttemptReset(void) {requests=failAt=fatalErrors=opens=closes=fileReads=errors=warnings=formatWarnings=closed=0;memset(requestCosts,0,sizeof(requestCosts));}
static void Globals(int memory,int onlyPosition) {
    int position,baselineOwners,baselineTokens;define_t *globals;source_t *source;
    SourceReset("join(native,Suffix) alias");Check(PC_AddGlobalDefine("alias 42")&&PC_AddGlobalDefine("join(a,b) a ## b"),"real native global macro definitions prepare");
    globals=globaldefines;baselineOwners=liveOwners;baselineTokens=numtokens;Check(baselineOwners==8&&baselineTokens==6,"known native global name/body/parameter owners");
    for(position=onlyPosition?onlyPosition:1;position<=(onlyPosition?onlyPosition:12);position++) {
        AttemptReset();failAt=position;
        source=memory?LoadSourceMemory((char *)fileText,(int)strlen(fileText),"global-memory"):LoadSourceFile("scripts/native.c");
        Check(!source&&requests==position&&liveOwners==baselineOwners&&numtokens==baselineTokens&&globaldefines==globals,"all base/global-copy allocation failures roll back source and preserve original globals");
        Check(fatalErrors==((position>=6&&position<=10)||position==12),"only returned null token copies retain native fatal diagnostic");
        Check(memory?(!opens&&!closes&&!fileReads):(opens==1&&closes==1&&fileReads==(position>2)),"global copy failure retains native file closure/read lifecycle");
        AttemptReset();source=memory?LoadSourceMemory((char *)fileText,(int)strlen(fileText),"global-memory"):LoadSourceFile("scripts/native.c");
        Check(source&&liveOwners==baselineOwners+12&&numtokens==baselineTokens+6,"all source/global copies complete on retry");ReadGolden(source,"nativeSuffix",TT_NAME);ReadGolden(source,"42",TT_NUMBER);
        {token_t token;Check(!PC_ReadToken(source,&token)&&!errors,"copied real global macros parse to EOF");}FreeSource(source);Check(liveOwners==baselineOwners&&numtokens==baselineTokens&&globaldefines==globals,"source cleanup preserves original global owners");
    }
    PC_RemoveAllGlobalDefines();Check(!liveOwners&&!numtokens&&!globaldefines,"actual global cleanup physically releases all original owners");
}
static void LargestNullable(int memory) {
    source_t *source;int length=INT_MAX-(int)sizeof(script_t)-1;SourceReset("native");allowLarge=1;reportLength=length;
    source=memory?LoadSourceMemory((char *)fileText,length,"maximum"):LoadSourceFile("scripts/native.c");
    Check(!source&&requests==1&&!liveOwners&&!numtokens&&!fileReads,"last signed buffer cost remains representable and propagates a nullable import");Check(memory?(!opens&&!closes):(opens==1&&closes==1),"large nullable file import closes the opened handle");
}
static void PathOverflow(void) {SourceReset("");PC_SetBaseFolder("prefix");Check(!LoadSourceFile("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx")&&!opens&&!requests&&!formatWarnings,"full native source path rejects before lookup/truncation");}
int main(int argc,char **argv) {
    if(argc>1){int proof=atoi(argv[1]);if(proof==0)BaseFailure(0,1);else if(proof==1)BaseFailure(0,2);else if(proof==2)BaseFailure(0,3);else if(proof==3)BaseFailure(0,4);else if(proof==4)Short();else if(proof==5)InvalidCost(0,INT_MAX);else if(proof==6)BaseFailure(1,2);else if(proof==7)Names();else if(proof==8)HighByte();else if(proof==9)Globals(1,5);else if(proof==10)Globals(1,6);else if(proof==11)Globals(1,9);else if(proof==12)PathOverflow();else InvalidCost(1,-1);}
    else {int memory,position;for(memory=0;memory<2;memory++){Plain(memory,0);Plain(memory,1);for(position=1;position<=4;position++)BaseFailure(memory,position);InvalidCost(memory,-1);InvalidCost(memory,INT_MAX);LargestNullable(memory);}Short();Names();MemoryMacros();HighByte();Globals(0,0);Globals(1,0);puts("Real native source owner transactions, file costs/reads, memory punctuation and copied-global rollback passed (issue #48)");}
    return 0;
}
