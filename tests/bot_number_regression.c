/* Real native number tokens, converter extremes and index-wrap rejection. */
#define main CharacterFixtureMain
#include "bot_character_regression.c"
#undef main
#include <float.h>
#include <limits.h>
#ifdef Q3_NUMBER_LEGACY
void NumberValue(char *,int,unsigned long *,long double *);
#else
int NumberValue(char *,int,unsigned long *,long double *);
#endif
static script_t *NumberScript(const char *text) {
    script_t *script;Reset(text);script=LoadScriptMemory((char *)text,(int)strlen(text),"numbers");Check(script&&liveOwners==2&&!numtokens&&!errors,"real native lexer script/table owners");return script;
}
static void NumberEnd(script_t *script) {FreeScript(script);Check(!liveOwners&&!numtokens,"native numeric script/table owners physically release");}
static void NumberGolden(const char *text,const char *spelling,int subtype,unsigned long integer,long double value) {
    script_t *script=NumberScript(text);token_t token;
    Check(PS_ReadToken(script,&token)&&token.type==TT_NUMBER&&token.subtype==subtype&&!strcmp(token.string,spelling)&&token.intvalue==integer&&fabsl(token.floatvalue-value)<=LDBL_EPSILON*8*fabsl(value),"literal native number text/type/subtype/integer/float values");
    Check(!PS_ReadToken(script,&token)&&!errors&&!opens,"valid number reaches native EOF without file/error imports");NumberEnd(script);
}
static void Ordinary(void) {
    NumberGolden("42","42",TT_DECIMAL|TT_INTEGER,42,42);NumberGolden("0","0",TT_OCTAL|TT_INTEGER,0,0);NumberGolden("0123","0123",TT_OCTAL|TT_INTEGER,83,83);NumberGolden("08","08",TT_DECIMAL|TT_INTEGER,8,8);
    NumberGolden("0x2a","0x2a",TT_HEX|TT_INTEGER,42,42);NumberGolden("0XA","0XA",TT_HEX|TT_INTEGER,10,10);NumberGolden("0b101010","0b101010",TT_BINARY|TT_INTEGER,42,42);NumberGolden("0B0101","0B0101",TT_BINARY|TT_INTEGER,5,5);
    NumberGolden("42u","42",TT_DECIMAL|TT_INTEGER|TT_UNSIGNED,42,42);NumberGolden("42L","42",TT_DECIMAL|TT_INTEGER|TT_LONG,42,42);NumberGolden("42ul","42",TT_DECIMAL|TT_INTEGER|TT_LONG|TT_UNSIGNED,42,42);NumberGolden("42LU","42",TT_DECIMAL|TT_INTEGER|TT_LONG|TT_UNSIGNED,42,42);
    NumberGolden("0x2aul","0x2a",TT_HEX|TT_INTEGER|TT_LONG|TT_UNSIGNED,42,42);NumberGolden("1.25","1.25",TT_DECIMAL|TT_FLOAT,1,1.25);NumberGolden("0.5","0.5",TT_OCTAL|TT_FLOAT,0,0.5);NumberGolden(".5",".5",TT_DECIMAL|TT_FLOAT,0,0.5);
    NumberGolden("1.25l","1.25",TT_DECIMAL|TT_FLOAT|TT_LONG,1,1.25);NumberGolden("0.001","0.001",TT_OCTAL|TT_FLOAT,0,0.001L);
}
static void NumberRejected(const char *text) {
    script_t *script=NumberScript(text);token_t token;Check(!PS_ReadToken(script,&token)&&errors==1&&!token.intvalue&&!token.floatvalue,"invalid/unrepresentable number rejects without partial numeric values");NumberEnd(script);
}
static void Fraction(int length) {
    char text[1024];long double reference;memcpy(text,"0.",2);memset(text+2,'1',length-2);text[length]=0;reference=strtold(text,NULL);NumberGolden(text,text,TT_OCTAL|TT_FLOAT,0,reference);
}
static void UnsignedBoundary(int base,int overflow) {
    char text[256],spelling[256];int i,length;int subtype;
    if(base==10){snprintf(text,sizeof(text),"%lu",ULONG_MAX);subtype=TT_DECIMAL;}
    else if(base==16){snprintf(text,sizeof(text),"0x%lx",ULONG_MAX);subtype=TT_HEX;}
    else if(base==8){snprintf(text,sizeof(text),"0%lo",ULONG_MAX);subtype=TT_OCTAL;}
    else {memcpy(text,"0b",2);for(i=0;i<(int)(sizeof(unsigned long)*CHAR_BIT);i++)text[i+2]='1';text[i+2]=0;subtype=TT_BINARY;}
    if(!overflow){NumberGolden(text,text,subtype|TT_INTEGER,ULONG_MAX,(long double)ULONG_MAX);return;}
    length=(int)strlen(text);text[length++]='0';text[length]=0;strcpy(spelling,text);strcat(text," tail");
    {script_t *script=NumberScript(text);token_t token;Check(!PS_ReadToken(script,&token)&&errors==1&&!token.intvalue&&!token.floatvalue&&!strcmp(token.string,spelling),"each base overflow rejects before wrapping");Check(PS_ReadToken(script,&token)&&token.type==TT_NAME&&!strcmp(token.string,"tail"),"rejected numeric token preserves following token");NumberEnd(script);}
}
static void LeadingBoundary(int base) {
    char text[1025];int length=base==8?1022:1023;int subtype=base==8?TT_OCTAL:base==16?TT_HEX:TT_BINARY;
    memset(text,'0',length);if(base!=8){text[0]='0';text[1]=base==16?'x':'b';}text[length-1]='1';text[length]=0;
    NumberGolden(text,text,subtype|TT_INTEGER,1,1);text[length]='0';text[length+1]=0;NumberRejected(text);
}
static void FloatAuxiliary(void) {
    char text[256];snprintf(text,sizeof(text),"%lu0.0",ULONG_MAX);NumberGolden(text,text,TT_DECIMAL|TT_FLOAT,ULONG_MAX,strtold(text,NULL));
}
static void WrappedIndex(int base) {
    const char *values[]={"18446744073709551616","0x10000000000000000","010000000000000000000000","0b10000000000000000000000000000000000000000000000000000000000000000"};
    char text[256];bot_character_t *character;snprintf(text,sizeof(text),"skill 4 { 1 \"owned\" %s \"invalid\" }",values[base]);Reset(text);character=BotLoadCharacterFromFile("bots/native.c",4);
    Check(!character&&errors>0&&opens==1&&closes==1&&!liveOwners&&!numtokens,"unrepresentable index rejects before wrap/assignment and releases prior source/character/string owners");
}
static void SignedTokens(void) {
    script_t *script=NumberScript("-1.25 + 08");token_t token;
    Check(PS_ReadToken(script,&token)&&token.type==TT_PUNCTUATION&&token.subtype==P_SUB,"native minus remains punctuation");
    Check(PS_ReadToken(script,&token)&&token.type==TT_NUMBER&&token.floatvalue==1.25&&token.intvalue==1,"native unsigned float follows minus");
    Check(PS_ReadToken(script,&token)&&token.type==TT_PUNCTUATION&&token.subtype==P_ADD&&PS_ReadToken(script,&token)&&token.type==TT_NUMBER&&token.intvalue==8&&!PS_ReadToken(script,&token)&&!errors,"native plus/leading-eight decimal and EOF");NumberEnd(script);
}
static void ConverterFraction(int tiny) {
    char text[6004];unsigned long integer=7;long double value=7,reference;
    Reset("");memcpy(text,"0.",2);memset(text+2,tiny?'0':'1',6000);if(tiny)text[6001]='1';text[6002]=0;reference=strtold(text,NULL);
    NumberValue(text,TT_DECIMAL|TT_FLOAT,&integer,&value);
    Check(integer==0&&fabsl(value-reference)<=LDBL_EPSILON*8*fabsl(reference)&&!liveOwners&&!numtokens,"actual converter fraction exceeds host divisor range without overflow and retains representable/subnormal result");
}
static void ConverterOverflow(void) {
    char text[6004];unsigned long integer=7;long double value=7;Reset("");memset(text,'9',6000);memcpy(text+6000,".0",3);
    NumberValue(text,TT_DECIMAL|TT_FLOAT,&integer,&value);Check(!integer&&!value&&!liveOwners&&!numtokens,"actual converter rejects unrepresentable whole floats without partial numeric output");
}
int main(int argc,char **argv) {
    if(argc>1){int proof=atoi(argv[1]);if(proof==0)NumberGolden("1.","1.",TT_DECIMAL|TT_FLOAT,1,1);else if(proof==1)Fraction(100);else if(proof==2)UnsignedBoundary(10,1);else if(proof==3)NumberRejected("1..2");else if(proof==4)NumberGolden("0XABCD","0XABCD",TT_HEX|TT_INTEGER,43981,43981);else if(proof==5)FloatAuxiliary();else if(proof>=6&&proof<=9)WrappedIndex(proof-6);else if(proof==10)ConverterOverflow();else Ordinary();}
    else {int base;Ordinary();NumberGolden("0XABCD","0XABCD",TT_HEX|TT_INTEGER,43981,43981);NumberGolden("1.","1.",TT_DECIMAL|TT_FLOAT,1,1);NumberGolden("0.","0.",TT_OCTAL|TT_FLOAT,0,0);Fraction(100);Fraction(1022);
        for(base=2;base<=16;base*=2){if(base==4)continue;UnsignedBoundary(base,0);UnsignedBoundary(base,1);LeadingBoundary(base);}UnsignedBoundary(10,0);UnsignedBoundary(10,1);FloatAuxiliary();SignedTokens();
        NumberRejected("0x");NumberRejected("0b");NumberRejected("1..2");for(base=0;base<4;base++)WrappedIndex(base);ConverterFraction(0);ConverterFraction(1);ConverterOverflow();puts("Real native numeric token boundaries, float conversion and index-wrap rejection passed (issue #48)");}
    return 0;
}
