/* Actual bot numeric converter and variable backend: grammar, values and ownership. */
#include Q3_LIBVAR_SOURCE
static void *owners[16];
static int liveOwners;
static void Check(int condition,const char *message) {if(!condition){fprintf(stderr,"Bot libvar regression failed: %s\n",message);exit(1);}}
#ifndef Com_Memset
void Com_Memset(void *out,int value,size_t size) {memset(out,value,size);}
#endif
void *GetMemory(unsigned long size) {
    int i;Check(size>0&&size<=4096,"bounded exact native variable allocation");
    for(i=0;i<16;i++)if(!owners[i]){owners[i]=malloc(size);Check(owners[i]!=NULL,"fixture allocation");liveOwners++;return owners[i];}
    Check(0,"bounded fixture native variable owners");return NULL;
}
void FreeMemory(void *pointer) {
    int i;if(!pointer)return;for(i=0;i<16;i++)if(owners[i]==pointer){free(pointer);owners[i]=NULL;liveOwners--;return;}
    Check(0,"known variable allocation releases once");
}
void QDECL Com_Printf(const char *format,...) {(void)format;Check(0,"unexpected native formatting");}
static unsigned int Bits(float value) {unsigned int bits;memcpy(&bits,&value,sizeof(bits));return bits;}
static void Golden(char *text,unsigned int bits) {
    libvar_t *variable;float result;
    Check(!libvarlist&&!liveOwners,"previous native variables released");result=LibVarValue("synthetic",text);
    Check(Bits(result)==bits,"literal native numeric value");variable=LibVarGet("SYNTHETIC");
    Check(variable&&Bits(variable->value)==bits&&!strcmp(variable->string,text)&&liveOwners==2,"native case-insensitive variable cache retains text/value");
    Check(Bits(LibVarValue("synthetic","999"))==bits&&liveOwners==2,"native default caching preserves existing variable");
    LibVarSet("synthetic","2.25");Check(Bits(LibVarGetValue("synthetic"))==0x40100000U&&liveOwners==2,"native setter uses same converter and replaces string owner");
    LibVarDeAllocAll();Check(!libvarlist&&!liveOwners,"native variable name/string owners physically release");
}
static void Valid(void) {
    const struct {char *text;unsigned int bits;} cases[]={
        {"",0},{"0",0},{"000000",0},{"1",0x3f800000U},{"42",0x42280000U},{"4096",0x45800000U},
        {"1000000",0x49742400U},{"2097151",0x49fffff8U},{".5",0x3f000000U},{"0.25",0x3e800000U},
        {"1.25",0x3fa00000U},{"1.2",0x3f99999aU},{".001",0x3a83126fU},{".0001",0x38d1b717U},
        {"1.125",0x3f900000U},{"1.00000000",0x3f800000U},{"00042.000",0x42280000U}
    };size_t i;for(i=0;i<sizeof(cases)/sizeof(cases[0]);i++)Golden(cases[i].text,cases[i].bits);
}
static void Trailing(void) {
    char *text=malloc(3);Check(text!=NULL,"exact trailing-decimal fixture");memcpy(text,"1.",3);
    Check(Bits(LibVarStringValue(text))==0x3f800000U,"trailing decimal retains value without reading after NUL");free(text);
    Golden(".",0);Golden("0.",0);Golden("4096.",0x45800000U);
}
static void LongFraction(void) {
    char text[1024];int i;
    Golden("1.00000000000000000000000000000000000000000000000000000000001",0x3f800000U);
    Golden("0.50000000000000000000000000000000000000000000000000000000000",0x3f000000U);
    strcpy(text,"0.");for(i=2;i<1000;i++)text[i]='0';text[1000]='1';text[1001]=0;Golden(text,0);
    text[999]='x';Golden(text,0);text[999]='0';text[1000]='.';Golden(text,0);
    Check(Bits(LibVarStringValue("0.1234567890"))==0x3dfcd6eaU,"ten fractional digits retain rounded numeric value without signed divisor overflow");
}
static void Invalid(void) {
    char large[200];size_t i;const char *cases[]={"-1","+1"," 1","1 ","1e3","nan","inf","1..2","1.x",".x","1.2x","1.2.3","x1","\377"};
    for(i=0;i<sizeof(cases)/sizeof(cases[0]);i++)Golden((char *)cases[i],0);
    memset(large,'9',sizeof(large)-1);large[sizeof(large)-1]=0;Golden(large,0);
}
int main(int argc,char **argv) {
    if(argc>1){int proof=atoi(argv[1]);if(proof==0)Trailing();else if(proof==1)LongFraction();else if(proof==2)Golden("1.x",0);else if(proof==4)Valid();else {char large[200];memset(large,'9',sizeof(large)-1);large[sizeof(large)-1]=0;Golden(large,0);}}
    else {Valid();Trailing();LongFraction();Invalid();puts("Native bot numeric grammar, long decimal bounds, values and variable ownership passed (issue #48)");}
    return 0;
}
