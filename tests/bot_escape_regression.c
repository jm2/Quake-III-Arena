/* Real quoted-token escapes and separate legacy literal-helper cursors. */
#define main CharacterFixtureMain
#include "bot_character_regression.c"
#undef main
int PS_ReadEscapeCharacter(script_t *,char *);
int PS_ReadLiteral(script_t *,token_t *);
static script_t *EscapeScript(const char *text) {
    script_t *script;Reset(text);script=LoadScriptMemory((char *)text,(int)strlen(text),"escapes");Check(script&&liveOwners==2&&!numtokens&&!errors,"real lexer script/table owners prepare");return script;
}
static void EscapeEnd(script_t *script) {
    Check(script->script_p>=script->buffer&&script->script_p<=script->end_p,"native cursor stays at/before terminating NUL");FreeScript(script);Check(!liveOwners&&!numtokens,"all actual lexical owners physically release");
}
static void EscapeTail(script_t *script) {
    token_t token;Check(PS_ReadToken(script,&token)&&token.type==TT_NAME&&!strcmp(token.string,"tail"),"following native token retains all its characters");Check(!PS_ReadToken(script,&token)&&!PS_ReadToken(script,&token),"repeated native EOF reads remain bounded");
}
static void EscapeDirect(const char *spell,unsigned int value,int warning) {
    char text[8192],byte=0;script_t *script;snprintf(text,sizeof(text),"%s tail",spell);script=EscapeScript(text);
    Check(PS_ReadEscapeCharacter(script,&byte)&&(unsigned char)byte==value&&script->script_p==script->buffer+strlen(spell)&&warnings==warning&&!errors,"actual escape value/warning and complete digit consumption");EscapeTail(script);EscapeEnd(script);
}
static void EscapeQuoted(const char *spell,unsigned int value,int warning,int quote) {
    char text[8192],expected[4];script_t *script;token_t token;snprintf(text,sizeof(text),"%c%s%c tail",quote,spell,quote);script=EscapeScript(text);
    expected[0]=quote;expected[1]=(char)value;expected[2]=quote;expected[3]=0;
    Check(PS_ReadToken(script,&token)&&token.type==(quote=='"'?TT_STRING:TT_LITERAL)&&token.subtype==3&&!memcmp(token.string,expected,4)&&warnings==warning&&!errors,"active native quoted-token type/subtype/escaped bytes");EscapeTail(script);EscapeEnd(script);
}
static void EscapeOrdinary(void) {
    struct escape {const char *spell;unsigned int value;} cases[]={
        {"\\\\",'\\'},{"\\n",'\n'},{"\\r",'\r'},{"\\t",'\t'},{"\\v",'\v'},{"\\b",'\b'},{"\\f",'\f'},{"\\a",'\a'},{"\\'",'\''},{"\\\"",'"'},{"\\?",'?'},
        {"\\x41",'A'},{"\\xG",16},{"\\xz",35},{"\\065",65},{"\\255",255},{"\\xFF",255},{"\\0",0}
    };unsigned int i;
    for(i=0;i<sizeof(cases)/sizeof(cases[0]);i++){EscapeDirect(cases[i].spell,cases[i].value,0);EscapeQuoted(cases[i].spell,cases[i].value,0,'"');EscapeQuoted(cases[i].spell,cases[i].value,0,'\'');}
    EscapeDirect("\\256",255,1);EscapeDirect("\\x100",255,1);EscapeQuoted("\\256",255,1,'"');EscapeQuoted("\\x100",255,1,'\'');
}
static void EscapeLong(int hex) {
    char spell[6004];int offset=hex?2:1;spell[0]='\\';if(hex)spell[1]='x';memset(spell+offset,hex?'F':'9',6000);spell[offset+6000]=0;
    EscapeDirect(spell,255,1);EscapeQuoted(spell,255,1,'"');EscapeQuoted(spell,255,1,'\'');
}
static void EscapeRejected(const char *text) {
    script_t *script=EscapeScript(text);token_t token;Check(!PS_ReadToken(script,&token)&&errors==1,"invalid active escape rejects at the lexer");EscapeEnd(script);
}
static void EscapeUnknownDirect(void) {
    char byte=(char)0xa5;script_t *script=EscapeScript("\\q");int result=PS_ReadEscapeCharacter(script,&byte);
    Check((unsigned char)byte==0xa5,"failed direct escape preserves caller byte");Check(!result&&errors==1,"invalid direct escape returns failure");EscapeEnd(script);
}
static void LiteralGolden(const char *text,unsigned int value,int warning) {
    script_t *script=EscapeScript(text);token_t token;char expected[4]={'\'',(char)value,'\'',0};
    int result=PS_ReadLiteral(script,&token);EscapeTail(script);
    Check(result&&token.type==TT_LITERAL&&token.subtype==(char)value&&!memcmp(token.string,expected,4)&&warnings==warning&&!errors,"legacy literal helper retains first byte and complete closing quote");EscapeEnd(script);
}
static void LiteralRejected(const char *text,int eof) {
    script_t *script=EscapeScript(text);token_t token;int result=PS_ReadLiteral(script,&token);
    if(eof)Check(!PS_ReadToken(script,&token),"rejected legacy literal reaches bounded EOF");Check(!result&&errors>0,"malformed legacy literal rejects");EscapeEnd(script);
}
static void QuotedTextGolden(const char *text,const char *payload,int quote,int flags) {
    char expected[1024];script_t *script=EscapeScript(text);token_t token;script->flags=flags;snprintf(expected,sizeof(expected),"%c%s%c",quote,payload,quote);
    Check(PS_ReadToken(script,&token)&&token.type==(quote=='"'?TT_STRING:TT_LITERAL)&&token.subtype==(int)strlen(expected)&&!strcmp(token.string,expected)&&!errors&&!warnings,"native complete string/single-quote concatenation/flags retain text and length metadata");EscapeTail(script);EscapeEnd(script);
}
static void QuotedBoundary(void) {
    char payload[1023],text[2048];memset(payload,'a',1020);payload[1020]=0;snprintf(text,sizeof(text),"\"%s\" tail",payload);QuotedTextGolden(text,payload,'"',0);
    payload[1020]='a';payload[1021]=0;snprintf(text,sizeof(text),"\"%s\"",payload);EscapeRejected(text);
}
static void BoundedWhitespace(void) {
    const char *text[]={""," ","/","//","// tail","/*","/* tail\n","/**/"};unsigned int i;
    for(i=0;i<sizeof(text)/sizeof(text[0]);i++){script_t *script=EscapeScript(text[i]);token_t token;int result=PS_ReadToken(script,&token);Check(result==(i==2)&&errors==((i==5||i==6)?1:0)&&!warnings,"whitespace/comment EOF stays bounded and unterminated blocks record failure");EscapeEnd(script);}
}
static void PunctuationEnds(void) {
    struct punctuation {const char *text;int subtype;} cases[]={{">",P_LOGIC_GREATER},{">>",P_RSHIFT},{">>=",P_RSHIFT_ASSIGN},{"<",P_LOGIC_LESS},{"<<",P_LSHIFT},{"<<=",P_LSHIFT_ASSIGN},{"!",P_LOGIC_NOT},{"!=",P_LOGIC_UNEQ}};
    unsigned int i;for(i=0;i<sizeof(cases)/sizeof(cases[0]);i++){script_t *script=EscapeScript(cases[i].text);token_t token;Check(PS_ReadToken(script,&token)&&token.type==TT_PUNCTUATION&&token.subtype==cases[i].subtype&&!strcmp(token.string,cases[i].text)&&!PS_ReadToken(script,&token)&&!errors,"short final punctuation preserves longest native match");EscapeEnd(script);}
}
int main(int argc,char **argv) {
    if(argc>1){int proof=atoi(argv[1]);if(proof<2)EscapeLong(proof);else if(proof==2)EscapeRejected("\"\\q\"");else if(proof==3)EscapeRejected("\"\\x\"");else if(proof==4)LiteralRejected("'a",1);else if(proof==5)LiteralRejected("''",1);else if(proof==6)LiteralGolden("'ab'tail",'a',1);else if(proof==7)EscapeUnknownDirect();else {EscapeOrdinary();QuotedTextGolden("'ab'tail","ab",'\'',0);QuotedTextGolden("\"one\" /* comment\n */ \"two\" tail","onetwo",'"',0);QuotedTextGolden("\"\\n\" tail","\\n",'"',SCFL_NOSTRINGESCAPECHARS);PunctuationEnds();}}
    else {EscapeOrdinary();EscapeLong(0);EscapeLong(1);EscapeUnknownDirect();EscapeRejected("\"\\q\"");EscapeRejected("'\\q'");EscapeRejected("\"\\x\"");EscapeRejected("'\\x'");EscapeRejected("\"a\\");EscapeRejected("'a\\");
        LiteralGolden("'a'tail",'a',0);LiteralGolden("'\\n'tail",'\n',0);LiteralGolden("'ab'tail",'a',1);LiteralRejected("'",1);LiteralRejected("'a",1);LiteralRejected("''",1);LiteralRejected("'a\n",0);LiteralRejected("'\n",0);
        QuotedTextGolden("'ab'tail","ab",'\'',0);QuotedTextGolden("\"one\" /* comment\n */ \"two\" tail","onetwo",'"',0);QuotedTextGolden("\"\\n\" tail","\\n",'"',SCFL_NOSTRINGESCAPECHARS);QuotedBoundary();BoundedWhitespace();PunctuationEnds();puts("Real native escape bounds, quoted-token compatibility and legacy literal cursors passed (issue #48)");}
    return 0;
}
