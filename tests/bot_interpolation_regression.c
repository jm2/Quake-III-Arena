/* Actual interpolation with real parsed inputs and physical staged owners. */
static int ObserveImport(unsigned long size);
#define Q3_CHARACTER_HEAP_HOOK ObserveImport
#define main CharacterFixtureMain
#include "bot_character_regression.c"
#undef main
#define CHARACTER_BYTES (sizeof(bot_character_t)+MAX_CHARACTERISTICS*sizeof(bot_characteristic_t))
static unsigned char firstSnapshot[CHARACTER_BYTES],secondSnapshot[CHARACTER_BYTES];
static int watchHandle;
static int ObserveImport(unsigned long size) {
    (void)size;if(watchHandle)Check(!botcharacters[watchHandle],"prospective interpolation stays unpublished through every import");return 0;
}
static void Inputs(void) {
    Reset("skill 1 { 0 1.25 1 11 2 \"first\" 3 9.5 5 13 79 \"\" } skill 4 { 0 7.25 1 44 2 \"second\" 3 99 4 \"upper only\" 5 8.5 79 \"upper last\" }");
    botcharacters[1]=BotLoadCharacterFromFile("bots/first.c",1);botcharacters[2]=BotLoadCharacterFromFile("bots/second.c",4);
    Check(botcharacters[1]&&botcharacters[2]&&liveOwners==7&&!numtokens&&!errors,"real lower/upper skill characters prepare seven physical owners");
    memcpy(firstSnapshot,botcharacters[1],CHARACTER_BYTES);memcpy(secondSnapshot,botcharacters[2],CHARACTER_BYTES);
}
static void Unchanged(void) {
    Check(!memcmp(firstSnapshot,botcharacters[1],CHARACTER_BYTES)&&!memcmp(secondSnapshot,botcharacters[2],CHARACTER_BYTES),"interpolation preserves every input characteristic/name/skill");
    Check(!strcmp(botcharacters[1]->c[2].value.string,"first")&&!strcmp(botcharacters[2]->c[2].value.string,"second")&&!strcmp(botcharacters[2]->c[4].value.string,"upper only")&&!strcmp(botcharacters[2]->c[79].value.string,"upper last"),"input string owners remain unchanged");
}
static void End(void) {Unchanged();BotFreeCharacter2(1);BotFreeCharacter2(2);Check(!liveOwners&&!numtokens,"input owners finally physically release");}
static void Output(int handle,float skill) {
    bot_character_t *out=botcharacters[handle];char text[64];
    Check(handle==3&&out&&out->skill==skill&&!strcmp(out->filename,"bots/first.c"),"native first free handle and lower filename retain desired skill");
    Check(out->c[0].type==CT_FLOAT&&fabs(out->c[0].value._float-(1.25+(skill-1)*2))<0.00001,"native float interpolation value");
    Check(out->c[1].type==CT_INTEGER&&out->c[1].value.integer==11&&out->c[5].type==CT_INTEGER&&out->c[5].value.integer==13,"native lower integer choice across equal/mixed types");
    Check(!out->c[3].type&&!out->c[4].type,"native unsupported lower/mixed fields remain unset");
    Check(out->c[2].type==CT_STRING&&!strcmp(out->c[2].value.string,"first")&&out->c[2].value.string!=botcharacters[1]->c[2].value.string&&out->c[79].type==CT_STRING&&!out->c[79].value.string[0]&&out->c[79].value.string!=botcharacters[1]->c[79].value.string,"lower ordinary/empty strings copy into independent owners");
    Check(Characteristic_Integer(handle,1)==11&&fabs(Characteristic_Float(handle,0)-out->c[0].value._float)<0.00001,"public native numeric getters retain interpolation");
    Characteristic_String(handle,2,text,sizeof(text));Check(!strcmp(text,"first")&&liveOwners==10&&!numtokens,"public string getter and complete output owners");Unchanged();
}
static void InterpolationGolden(float skill,int observe) {
    int handle,before;Inputs();before=requests;watchHandle=observe?3:0;handle=BotInterpolateCharacters(1,2,skill);watchHandle=0;
    Check(requests==before+3&&!errors&&!formatWarnings,"native complete interpolation requires only output and two string imports");Output(handle,skill);BotFreeCharacter2(handle);Check(liveOwners==7&&!botcharacters[3],"completed output cleanup retains original owners");End();
}
static void Failure(int position,int observe) {
    int before;Inputs();before=requests;failAt=before+position;watchHandle=observe?3:0;
    Check(!BotInterpolateCharacters(1,2,2.5),"nullable interpolation import rejects");watchHandle=0;
    Check(requests==failAt&&errors==1&&!botcharacters[3]&&liveOwners==7&&!numtokens,"failed output/either string releases staged owners without publishing a handle");Unchanged();
    failAt=0;watchHandle=3;Check(BotInterpolateCharacters(1,2,2.5)==3,"failed interpolation retries into the same free handle");watchHandle=0;Output(3,2.5);BotFreeCharacter2(3);End();
}
static void Invalid(void) {
    int before,i;Inputs();before=requests;
    Check(!BotInterpolateCharacters(0,2,2.5)&&!BotInterpolateCharacters(1,MAX_CLIENTS+1,2.5)&&!BotInterpolateCharacters(1,3,2.5)&&requests==before&&errors==3&&liveOwners==7&&!botcharacters[3],"native invalid/missing handles reject without allocation");
    for(i=3;i<=MAX_CLIENTS;i++)botcharacters[i]=botcharacters[1];
    Check(!BotInterpolateCharacters(1,2,2.5)&&requests==before&&liveOwners==7,"full native handle table rejects before allocation");
    for(i=3;i<=MAX_CLIENTS;i++)botcharacters[i]=NULL;End();
}
#ifndef Q3_INTERPOLATION_ENTRY
#define Q3_INTERPOLATION_ENTRY main
#endif
int Q3_INTERPOLATION_ENTRY(int argc,char **argv) {
    if(argc>1){int proof=atoi(argv[1]);if(proof<3)Failure(proof+1,0);else if(proof==3)InterpolationGolden(2.5,1);else {InterpolationGolden(1,0);InterpolationGolden(2.5,0);InterpolationGolden(4,0);InterpolationGolden(5,0);Invalid();}}
    else {int position;InterpolationGolden(1,1);InterpolationGolden(2.5,1);InterpolationGolden(4,1);InterpolationGolden(5,1);for(position=1;position<=3;position++)Failure(position,1);Invalid();puts("Real native interpolation values, private publication and nullable owner cleanup passed (issue #48)");}
    return 0;
}
