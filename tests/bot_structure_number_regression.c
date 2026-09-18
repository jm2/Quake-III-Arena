/* Actual structure numbers, lexer/source and physical native metadata. */
#define main ItemConfigFixtureMain
#include "bot_item_config_regression.c"
#undef main
extern int PC_UnreadSourceToken(source_t *source,token_t *token);
extern qboolean ReadNumber(source_t *source,fielddef_t *field,void *out);
static unsigned int FloatBits(float value){unsigned int bits;memcpy(&bits,&value,sizeof(bits));return bits;}
static void NumberCase(const char *text,int type,float low,float high,int success,unsigned int expected,int historical)
{
    source_t *source;fielddef_t field={"native",0,0,0,0,0,NULL};
    unsigned int result=0xa5a5a5a5U,before=result;
    Begin();source=LoadSourceMemory((char *)text,(int)strlen(text),"native");Check(source!=NULL,"actual memory lexer/source imports");
    field.type=type;field.floatmin=low;field.floatmax=high;
    if(historical)SourceError(source,"historical native error");
    Check(ReadNumber(source,&field,&result)==success,"actual numeric field outcome");
    Check(success?result==expected:result==before,"native valid output bits or failed field bytes remain");
    Check(success?errors==historical:errors>0,"native numeric diagnostics and historical recovery");
    FreeSource(source);End();
}
static void NumericGolden(void)
{
    int value,type,j,n=0;char text[64];float values[]={-64,-1.5f,0,0.125f,1.25f,64,32767};
    for(type=FT_CHAR;type<=FT_INT;type++)for(value=-128;value<=127;value++) {
        snprintf(text,sizeof(text),"%d",value);
        NumberCase(text,type,0,0,1,type==FT_CHAR?(0xa5a5a500U|(unsigned char)value):(unsigned int)value,0);n++;
    }
    for(j=0;j<7;j++) {snprintf(text,sizeof(text),"%.9f",values[j]);NumberCase(text,FT_FLOAT,0,0,1,FloatBits(values[j]),0);n++;}
    NumberCase("32767",FT_INT,0,0,1,32767,0);NumberCase("-32768",FT_INT,0,0,1,(unsigned int)-32768,0);
    NumberCase("65535",FT_INT|FT_UNSIGNED,0,0,1,65535,0);NumberCase("255",FT_CHAR|FT_UNSIGNED,0,0,1,0xa5a5a5ffU,0);
    NumberCase("42",FT_INT|FT_BOUNDED,-1.5f,42.75f,1,42,0);NumberCase("-1",FT_INT|FT_BOUNDED,-1.5f,42.75f,1,(unsigned int)-1,0);
    NumberCase("1.25",FT_FLOAT|FT_BOUNDED,0,2,1,FloatBits(1.25f),0);NumberCase("42",FT_INT,0,0,1,42,1);
    printf("%d ordinary native numeric bits and boundary/historical goldens passed\n",n+8);
}
static void NativeWords(void)
{
    NumberCase("4294967295",FT_INT,0,0,1,(unsigned int)-1,0);
    NumberCase("-4294967295",FT_INT,0,0,1,1,0);
    NumberCase("2147483648",FT_FLOAT,0,0,1,FloatBits(-2147483648.0f),0);
    NumberCase("-2147483648",FT_FLOAT,0,0,1,FloatBits(-2147483648.0f),0);
    NumberCase("4294967296",FT_INT,0,0,0,0,0);
}

static void QueuedFloat(unsigned int bits)
{
    source_t *source;token_t token;fielddef_t field={"native",0,FT_FLOAT,0,0,0,NULL};unsigned int result=0xa5a5a5a5U;
    Begin();source=LoadSourceMemory("",0,"native");Check(source!=NULL,"queued native source prepares");memset(&token,0,sizeof(token));token.type=TT_NUMBER;token.subtype=TT_FLOAT;strcpy(token.string,"native");token.floatvalue=Opaque(bits);Check(PC_UnreadSourceToken(source,&token),"actual checked queued token copy");
    Check(!ReadNumber(source,&field,&result)&&result==0xa5a5a5a5U&&errors==1,"queued nonfinite float rejects before destination publication");FreeSource(source);End();
}
static void Defects(int proof)
{
    if(proof==0)NumberCase("-9223372036854775808",FT_INT,0,0,0,0,0);
    else if(proof==1)NumberCase("10000000000000000000000000000000000000000.0",FT_FLOAT,0,0,0,0,0);
    else if(proof==2)NumberCase("0",FT_INT|FT_BOUNDED,1e30f,1e31f,0,0,0);
    else if(proof==3)NumberCase("0",FT_INT|FT_BOUNDED,Opaque(0x7fc00000U),100,0,0,0);
    else if(proof==4)NumberCase("#unknown\n42",FT_INT,0,0,0,0,0);
    else QueuedFloat(proof==5?0x7f800000U:proof==6?0xff800000U:0x7fc00000U);
}
int main(int argc,char **argv)
{
    int i;
    if(argc>1){i=atoi(argv[1]);if(i<8)Defects(i);else NumericGolden();return 0;}
    NumericGolden();NativeWords();for(i=0;i<8;i++)Defects(i);
    NumberCase("32768",FT_INT,0,0,0,0,0);NumberCase("-32769",FT_INT,0,0,0,0,0);
    NumberCase("65536",FT_INT|FT_UNSIGNED,0,0,0,0,0);NumberCase("-1",FT_INT|FT_UNSIGNED,0,0,0,0,0);
    NumberCase("256",FT_CHAR|FT_UNSIGNED,0,0,0,0,0);NumberCase("-129",FT_CHAR,0,0,0,0,0);
    NumberCase("1.25",FT_INT,0,0,0,0,0);NumberCase("43",FT_INT|FT_BOUNDED,-1.5f,42.75f,0,0,0);
    NumberCase("2.5",FT_FLOAT|FT_BOUNDED,0,2,0,0,0);
    puts("Actual structure numeric bounds/words/floats, prior destination bytes and physical native source ownership passed (issue #48)");return 0;
}
