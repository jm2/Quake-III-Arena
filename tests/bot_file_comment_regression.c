/* Actual file/comment imports and native compression/lexer publication. */
#define main CharacterFixtureMain
#include "bot_character_regression.c"
#undef main
static char diagnostic[2048];
static void QDECL FilePrint(int level,char *format,...){va_list args;size_t length=strlen(diagnostic);va_start(args,format);vsnprintf(diagnostic+length,sizeof(diagnostic)-length,format,args);va_end(args);if(level==PRT_ERROR||level==PRT_FATAL)errors++;else if(level==PRT_WARNING)warnings++;else Check(level==PRT_MESSAGE,"native file print severity");}
static void FileReset(const char *text){Reset(text);botimport.Print=FilePrint;diagnostic[0]=0;}
static void FileBad(int entry){
    const char *text=entry==0?"/*":entry==1?"native /*\nunclosed\n":entry==2?"native /**":entry==6?"skill 4 { 0 42 } \"x\\\" /*":entry==7?"skill 4 { 0 42 } '/*":"skill 4 { 0 42 } /*\nunclosed\n";
    FileReset(text);
    if(entry<2)Check(!LoadScriptFile("bad.c"),"malformed block comment rejects direct file import");
    else if(entry==2)Check(!LoadSourceFile("bad.c"),"malformed block comment rejects source import");
    else Check(!BotLoadCharacterFromFile("bad.c",4),"malformed comment cannot publish a complete-prefix character");
    Check(errors==1+(entry>=3)&&opens==1&&fileReads==1&&closes==1&&!liveOwners&&!numtokens&&strstr(diagnostic,"unterminated block comment")&&strstr(diagnostic,entry==1||entry==3?"line 3:":"line 1:"),"malformed file retains raw line diagnostics and physically frees closed script/punctuation owners");
}
static void FileInclude(int quoted){
    const char *body=quoted?"#include \"bad.c\"\nnext":"#include <bad.c>\nnext";source_t *source;token_t token;
    FileReset("/*\nunclosed");source=LoadSourceMemory((char *)body,(int)strlen(body),"parent");Check(source&&liveOwners==4,"actual parent source imports");
    Check(!PC_ReadToken(source,&token)&&errors==(quoted?3:2)&&opens==(quoted?2:1)&&closes==opens&&liveOwners==4&&!numtokens&&PC_SourceHasError(source),"malformed included file rejects without child owners/publication, retaining native quoted fallback");
    Check(PC_ReadToken(source,&token)&&!strcmp(token.string,"next")&&!PC_ReadToken(source,&token)&&errors==(quoted?3:2),"native included failure retains synchronized parent recovery");
    FreeSource(source);Check(!liveOwners&&!numtokens,"parent recovery physically frees owners");
}
static void FileMemory(void){
    const char *text="native /*\nunclosed";source_t *source;token_t token;
    FileReset(text);source=LoadSourceMemory((char *)text,(int)strlen(text),"memory");
    Check(source&&PC_ReadToken(source,&token)&&!strcmp(token.string,"native")&&!PC_ReadToken(source,&token)&&errors==1&&PC_SourceHasError(source)&&!opens&&liveOwners==4,"actual memory lexer retains sticky raw comment diagnostics");
    FreeSource(source);Check(!liveOwners&&!numtokens,"raw memory lexer owners release");
}
static void FileGolden(const char *text){
    char compressed[4096];script_t *file,*memory;token_t a,b;int length,ra,rb,count=0;
    Check(strlen(text)<sizeof(compressed),"complete native compression fixture");strcpy(compressed,text);length=COM_Compress(compressed);
    FileReset(text);file=LoadScriptFile("native.c");Check(file&&file->length==length&&!memcmp(file->buffer,compressed,(size_t)length+1)&&file->line==1&&file->lastline==1&&file->script_p==file->buffer&&file->lastscript_p==file->buffer&&file->flags==0&&opens==1&&closes==1&&!errors&&liveOwners==2,"valid file retains complete native compressed bytes and untouched lexer metadata");
    memory=LoadScriptMemory(compressed,length,"native.c");Check(memory&&liveOwners==4,"actual native compressed memory lexer prepares");
    do{ra=PS_ReadToken(file,&a);rb=PS_ReadToken(memory,&b);Check(ra==rb,"native compressed/file token availability remains");if(ra){Check(a.type==b.type&&a.subtype==b.subtype&&!strcmp(a.string,b.string)&&a.intvalue==b.intvalue&&a.floatvalue==b.floatvalue&&a.line==b.line&&a.linescrossed==b.linescrossed,"valid file preserves actual native token metadata");count++;}}while(ra);
    Check(!errors&&!warnings&&file->script_p==file->buffer+length&&!*file->script_p&&EndOfScript(memory),"valid file reaches actual EOF without new diagnostics");
    FreeScript(memory);FreeScript(file);Check(!liveOwners&&!numtokens,"all valid file/memory lexer owners physically release");(void)count;
}
static void FileGoldens(void){
    const char *texts[]={"","// /* unclosed marker","native // \" /* marker\nnext","native /**/ next","native /* nested /* marker */ next","native /* a\nb */ next","native \"/*\" \"//\"\nnext","native '/* */' next","native \"escaped \\\" /* closed */\" next","native '\\\\' /* closed */ next"," \t native /* closed */ next // tail\n42\n"};
    char text[4096];size_t i;int length,newlines;
    for(i=0;i<sizeof(texts)/sizeof(texts[0]);i++)FileGolden(texts[i]);
    for(length=0;length<256;length++)for(newlines=0;newlines<4;newlines++){strcpy(text,"native /*");memset(text+9,'x',(size_t)length);memset(text+9+length,'\n',(size_t)newlines);strcpy(text+9+length+newlines,"*/ next // tail\n42");FileGolden(text);}
}

#ifndef Q3_FILE_COMMENT_ENTRY
#define Q3_FILE_COMMENT_ENTRY main
#endif
int Q3_FILE_COMMENT_ENTRY(int argc,char **argv){int i;if(argc>1){i=atoi(argv[1]);if(i<4)FileBad(i);else if(i==4)FileInclude(0);else if(i==5)FileInclude(1);else if(i==6||i==7)FileBad(i);else FileGoldens();return 0;}for(i=0;i<4;i++)FileBad(i);FileBad(6);FileBad(7);FileInclude(0);FileInclude(1);FileMemory();FileGoldens();Golden("bots/native.c");puts("Real file-comment validation, native compressed bytes/token metadata, included recovery and publication owners passed (issue #48)");return 0;}
