/* Actual default inheritance and cached/new character failure propagation. */
static int ObserveInheritImport(unsigned long size);
#define Q3_CHARACTER_HEAP_HOOK ObserveInheritImport
#define main CharacterFixtureMain
#include "bot_character_regression.c"
#undef main
#define CHARACTER_BYTES (sizeof(bot_character_t)+MAX_CHARACTERISTICS*sizeof(bot_characteristic_t))
static const char *defaultText="skill 4 { 0 1.25 1 7 2 \"first\" 11 1.25 40 \"\" 79 \"last\" }";
static const char *targetText="skill 4 { 0 42 3 \"owned\" 10 2.5 }";
static unsigned char defaultSnapshot[CHARACTER_BYTES],targetSnapshot[CHARACTER_BYTES];
static bot_character_t *watchTarget;
static int ObserveInheritImport(unsigned long size) {
    (void)size;if(watchTarget)Check(!memcmp(watchTarget,targetSnapshot,CHARACTER_BYTES),"target fields stay unchanged through every default string import");return 0;
}
static int InheritanceOpen(const char *path,fileHandle_t *file,fsMode_t mode) {
    if(strstr(path,"bots/missing.c")){Check(mode==FS_READ,"native failed VFS mode");opens++;*file=0;return -1;}
    fileText=strstr(path,DEFAULT_CHARACTER)?defaultText:targetText;return Open(path,file,mode);
}
static void InheritInputs(int target) {
    watchTarget=NULL;Reset(defaultText);botcharacters[1]=BotLoadCharacterFromFile(DEFAULT_CHARACTER,4);
    Check(botcharacters[1]&&liveOwners==4&&!numtokens&&!errors,"real default character has three independently owned strings");memcpy(defaultSnapshot,botcharacters[1],CHARACTER_BYTES);
    if(target){fileText=targetText;botcharacters[2]=BotLoadCharacterFromFile("bots/target.c",4);Check(botcharacters[2]&&liveOwners==6&&!numtokens&&!errors,"real partial target owns its original string");memcpy(targetSnapshot,botcharacters[2],CHARACTER_BYTES);}
    botimport.FS_FOpenFile=InheritanceOpen;
}
static void DefaultUnchanged(void) {
    Check(!memcmp(defaultSnapshot,botcharacters[1],CHARACTER_BYTES)&&!strcmp(botcharacters[1]->c[2].value.string,"first")&&!botcharacters[1]->c[40].value.string[0]&&!strcmp(botcharacters[1]->c[79].value.string,"last"),"default header/numeric/string owners remain unchanged");
}
static void TargetUnchanged(void) {
    Check(!memcmp(targetSnapshot,botcharacters[2],CHARACTER_BYTES)&&!strcmp(botcharacters[2]->c[3].value.string,"owned"),"failed inheritance preserves entire prior target and string contents");DefaultUnchanged();
}
static void InheritValues(int handle) {
    bot_character_t *target=botcharacters[handle];char text[64];DefaultUnchanged();
    Check(target&&target->skill==4&&!strcmp(target->filename,"bots/target.c")&&target->c[0].type==CT_INTEGER&&target->c[0].value.integer==42&&target->c[10].type==CT_FLOAT&&target->c[10].value._float==2.5&&!strcmp(target->c[3].value.string,"owned"),"native target metadata/existing fields remain intact");
    Check(target->c[1].type==CT_INTEGER&&target->c[1].value.integer==7&&target->c[11].type==CT_FLOAT&&target->c[11].value._float==1.25,"native missing numeric fields inherit defaults");
    Check(target->c[2].type==CT_STRING&&!strcmp(target->c[2].value.string,"first")&&target->c[40].type==CT_STRING&&!target->c[40].value.string[0]&&target->c[79].type==CT_STRING&&!strcmp(target->c[79].value.string,"last"),"native first/empty/last strings inherit complete values");
    Check(target->c[2].value.string!=botcharacters[1]->c[2].value.string&&target->c[40].value.string!=botcharacters[1]->c[40].value.string&&target->c[79].value.string!=botcharacters[1]->c[79].value.string&&liveOwners==(handle==3?11:9)&&!numtokens,"every inherited string owns an independent physical copy");
    Check(Characteristic_Integer(handle,1)==7&&Characteristic_Float(handle,11)==1.25,"public native inherited numeric getters");Characteristic_String(handle,79,text,sizeof(text));Check(!strcmp(text,"last"),"public native inherited final string getter");
}
static void InheritEnd(int target) {
    if(target)BotFreeCharacter2(2);DefaultUnchanged();Check(liveOwners==4&&!numtokens,"target cleanup preserves original default owners");BotFreeCharacter2(1);Check(!liveOwners&&!numtokens,"default and target owners finally physically release");
}
static void InheritGolden(int observe) {
    int before;InheritInputs(1);before=requests;watchTarget=observe?botcharacters[2]:NULL;BotDefaultCharacteristics(botcharacters[2],botcharacters[1]);watchTarget=NULL;
    Check(requests==before+3&&!errors&&!formatWarnings,"native missing strings clone exactly once");InheritValues(2);before=requests;
    BotDefaultCharacteristics(botcharacters[2],botcharacters[1]);BotDefaultCharacteristics(botcharacters[1],botcharacters[1]);Check(requests==before&&liveOwners==9,"complete/self defaults require no new owners");InheritEnd(1);
}
static void InheritFailure(int position,int observe) {
    InheritInputs(1);failAt=requests+position;watchTarget=observe?botcharacters[2]:NULL;BotDefaultCharacteristics(botcharacters[2],botcharacters[1]);watchTarget=NULL;
    Check(requests==failAt&&errors==1&&liveOwners==6&&!numtokens,"nullable default copy releases all staged strings");TargetUnchanged();failAt=0;
    watchTarget=botcharacters[2];BotDefaultCharacteristics(botcharacters[2],botcharacters[1]);watchTarget=NULL;InheritValues(2);InheritEnd(1);
}
static void SkillGolden(int existing) {
    int handle=existing==2?3:2;InheritInputs(existing);if(existing==2)LibVarSet("bot_reloadcharacters","1");if(existing)watchTarget=botcharacters[2];
    Check(BotLoadCharacterSkill("bots/target.c",4)==handle,"native skill loader returns complete cached/new/reloaded target");watchTarget=NULL;if(existing==2)LibVarDeAllocAll();
    InheritValues(handle);if(existing==2){TargetUnchanged();BotFreeCharacter2(3);}InheritEnd(1);
}
static void SkillFailure(int existing,int position) {
    int target,handle=existing==2?3:2;bot_character_t *previous[MAX_CLIENTS+1];
    InheritInputs(existing);if(existing==2)LibVarSet("bot_reloadcharacters","1");Check(BotLoadCharacterSkill("bots/target.c",4)==handle,"baseline native skill/default allocation sequence");target=requests-3+position;
    if(existing==2){LibVarDeAllocAll();BotFreeCharacter2(3);}InheritEnd(1);
    InheritInputs(existing);if(existing==2)LibVarSet("bot_reloadcharacters","1");memcpy(previous,botcharacters,sizeof(previous));failAt=target;if(existing)watchTarget=botcharacters[2];
    Check(!BotLoadCharacterSkill("bots/target.c",4),"native skill caller rejects failed default inheritance");watchTarget=NULL;if(existing==2)LibVarDeAllocAll();
    Check(requests==failAt&&errors==1&&!memcmp(previous,botcharacters,sizeof(previous))&&liveOwners==(existing?6:4)&&!numtokens,"failed skill load restores prior registry and releases only new target owners");
    if(existing)TargetUnchanged();else DefaultUnchanged();failAt=0;if(existing==2)LibVarSet("bot_reloadcharacters","1");if(existing)watchTarget=botcharacters[2];
    Check(BotLoadCharacterSkill("bots/target.c",4)==handle,"failed skill inheritance retries without a stranded target handle");watchTarget=NULL;if(existing==2)LibVarDeAllocAll();
    InheritValues(handle);if(existing==2){TargetUnchanged();BotFreeCharacter2(3);}InheritEnd(1);
}
static void MissingFallback(void) {
    int before;InheritInputs(0);before=requests;Check(BotLoadCharacterSkill("bots/missing.c",4)==1&&requests==before&&liveOwners==4&&!botcharacters[2],"native missing-file fallback retains complete cached default and self inheritance");DefaultUnchanged();InheritEnd(0);
}
int main(int argc,char **argv) {
    if(argc>1){int proof=atoi(argv[1]);if(proof<3)InheritFailure(proof+1,0);else if(proof==3)InheritGolden(1);else if(proof==4)SkillFailure(0,3);else if(proof==5)SkillFailure(1,3);else {InheritGolden(0);SkillGolden(0);MissingFallback();}}
    else {int position,existing;InheritGolden(1);for(position=1;position<=3;position++)InheritFailure(position,1);for(existing=0;existing<3;existing++){SkillGolden(existing);for(position=1;position<=3;position++)SkillFailure(existing,position);}MissingFallback();puts("Real native default inheritance transactions and cached/new target rollback passed (issue #48)");}
    return 0;
}
