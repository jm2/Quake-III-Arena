/* PS_ExpectTokenType names the expected punctuation by its id, not by table index (issue #48). */
#define main CharacterFixtureMain
#include "bot_character_regression.c"
#undef main
static char lastError[1024];
static void QDECL CapturePrint(int level,char *format,...) {
    va_list args;Check(level==PRT_ERROR,"punctuation mismatch reports an error");errors++;
    va_start(args,format);vsnprintf(lastError,sizeof(lastError),format,args);va_end(args);
}
static void Expect(script_t *script,int subtype,const char *message) {
    token_t token;errors=0;lastError[0]=0;botimport.Print=CapturePrint;
    Check(!PS_ExpectTokenType(script,TT_PUNCTUATION,subtype,&token)&&errors==1,"mismatched punctuation is rejected once");
    if(!strstr(lastError,message)){fprintf(stderr,"got: %s",lastError);Check(0,message);}
}
static void Default(int subtype,const char *message) {
    script_t *script;Reset("]");script=LoadScriptMemory("]",1,"punctuation");Check(script!=NULL,"default punctuation script");
    Expect(script,subtype,message);FreeScript(script);Check(!liveOwners&&!numtokens,"default punctuation script released");
}
static void Custom(const char *text,int subtype,const char *message) {
    /* An exact-size heap table whose ids do not match its indexes: indexing by id reads past it. */
    static const punctuation_t source[]={{"@",7,NULL},{"#",3,NULL},{NULL,0,NULL}};
    punctuation_t *table=malloc(sizeof(source));script_t *script;
    Check(table!=NULL,"custom punctuation table");memcpy(table,source,sizeof(source));
    Reset(text);script=LoadScriptMemory((char *)text,(int)strlen(text),"custom");Check(script!=NULL,"custom punctuation script");SetScriptPunctuations(script,table);
    Expect(script,subtype,message);FreeScript(script);free(table);Check(!liveOwners&&!numtokens,"custom punctuation script released");
}
int main(void) {
    Default(P_RSHIFT_ASSIGN,"expected >>=, found ]");Default(P_ADD,"expected +, found ]");Default(P_DOLLAR,"expected $, found ]");
    Custom("#",7,"expected @, found #");Custom("@",3,"expected #, found @");puts("PS_ExpectTokenType punctuation mismatch messages passed (issue #48)");
    return 0;
}
