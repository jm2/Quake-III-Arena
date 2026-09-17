/* Issue #35: execute actual remaining AI dispatch with full-array and struct callbacks. */
#include "../code/server/sv_game.c"
#include "../code/qcommon/vm_local.h"
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

#define IMAGE_SIZE 4096
static vm_t vm;
vm_t *gvm=&vm;
static botlib_export_t api;
static byte before[IMAGE_SIZE];
static int expectError,callbacks;
static jmp_buf errorJump;
/** Stop on unexpected arguments, ranges, or dispatch. */
static void Check( int ok, const char *message ) {
	if(!ok) { fprintf(stderr,"Bot AI regression failed: %s\n",message); exit(1); }
}
/** Require invalid AI requests to fault before callbacks or image mutation. */
void QDECL Com_Error( int level, const char *format, ... ) {
	(void)format; Check(expectError && level==ERR_DROP && vm.interpretFaulted && !vm.currentlyInterpreting,"error state");
	Check(!memcmp(before,vm.dataBase,IMAGE_SIZE),"image changed on error"); longjmp(errorJump,1);
}
/** Ignore diagnostics from unused VM paths. */
void QDECL Com_Printf( const char *format, ... ) { (void)format; }
/** Reject all callbacks during malformed requests. */
static void Callback( void ) { Check(!expectError,"invalid request dispatched"); callbacks++; }
/** Touch the entire native goal output. */
static int TopGoal( int state, bot_goal_t *goal ) { Callback(); Check(state==1,"goal state"); memset(goal,0,sizeof(*goal)); return 2; }
/** Check all retail inventory entries, exposing partial range checks. */
static int BestWeapon( int state, int *inventory ) {
	int i; Callback(); Check(state==1,"weapon state");
	for(i=0;i<BOTLIB_INVENTORY_SIZE;i++) Check(inventory[i]==i,"full inventory"); return 3;
}
/** Touch the complete embedded weapon/projectile structure. */
static void WeaponInfo( int state, int weapon, weaponinfo_t *out ) {
	Callback(); Check(state==1 && weapon==2,"weapon info args"); memset(out,0,sizeof(*out));
}
/** Preserve NULL long-term goals while checking the full inventory. */
static int NearbyGoal( int state, vec3_t origin, int *inventory, int flags, bot_goal_t *goal, float time ) {
	int i; Callback(); Check(state==1 && (byte *)origin==vm.dataBase+4 && flags==3 && time==1,"nearby args");
	Check(!goal || (byte *)goal==vm.dataBase+64,"optional long-term goal");
	for(i=0;i<BOTLIB_INVENTORY_SIZE;i++) Check(inventory[i]==i,"nearby full inventory"); return 4;
}
/** Touch the full move result even when no goal is supplied. */
static void MoveGoal( bot_moveresult_t *result, int state, bot_goal_t *goal, int flags ) {
	Callback(); Check(state==1 && !goal && flags==3,"no move goal"); memset(result,0,sizeof(*result));
}
/** Preserve no-goal queries with optional unused target outputs. */
static int ViewTarget( int state, bot_goal_t *goal, int flags, float ahead, vec3_t out ) {
	Callback(); Check(state==1 && flags==3 && ahead==1,"view args");
	if(!goal) { Check(!out,"optional unused target"); return 0; }
	memset(out,0,sizeof(vec3_t)); return 5;
}
/** Validate every rank and all three scalar outputs. */
static int Genetic( int count, float *ranks, int *p1, int *p2, int *child ) {
	int i; Callback(); for(i=0;i<count;i++) Check(ranks[i]==i,"whole rankings"); *p1=1; *p2=2; *child=3; return 6;
}
/** Touch all bytes of bounded characteristic output. */
static void Characteristic( int state, int index, char *out, int size ) {
	Callback(); Check(state==1 && index==2,"characteristic args"); memset(out,'c',size);
}
/** Check the filename before native character loading. */
static int LoadCharacter( char *file, float skill ) { Callback(); Check(!strcmp(file,"test") && skill==1,"load args"); return 7; }
/** Require invalid requests to preserve image and callback count. */
static void Reject( int *args ) {
	int calls=callbacks; vm.interpretFaulted=qfalse; vm.currentlyInterpreting=qtrue;
	memcpy(before,vm.dataBase,IMAGE_SIZE); expectError=1;
	if(setjmp(errorJump)==0) { SV_BotLibAICalls(args); Check(0,"invalid request accepted"); }
	expectError=0; Check(callbacks==calls,"rejection dispatched");
}
/** Cover remaining AI structs, arrays, nullable goals, strings, alignment, count overflow, and missing API. */
int main( void ) {
	int args[16]={0},i; float one=1;
	vm.dataBase=malloc(IMAGE_SIZE); Check(vm.dataBase!=NULL,"allocation"); vm.dataMask=IMAGE_SIZE-1;
	currentVM=&vm; botlib_export=&api; memset(vm.dataBase,'x',IMAGE_SIZE); memcpy(vm.dataBase+32,"test",5);
	api.ai.BotGetTopGoal=TopGoal; api.ai.BotChooseBestFightWeapon=BestWeapon; api.ai.BotGetWeaponInfo=WeaponInfo;
	api.ai.BotChooseNBGItem=NearbyGoal; api.ai.BotMoveToGoal=MoveGoal; api.ai.BotMovementViewTarget=ViewTarget;
	api.ai.GeneticParentsAndChildSelection=Genetic; api.ai.Characteristic_String=Characteristic; api.ai.BotLoadCharacter=LoadCharacter;
	args[0]=BOTLIB_AI_GET_TOP_GOAL; args[1]=1; args[2]=IMAGE_SIZE-sizeof(bot_goal_t);
	Check(SV_BotLibAICalls(args)==2,"whole goal output"); args[2]+=4; Reject(args); args[2]=0; Reject(args);
	args[0]=BOTLIB_AI_CHOOSE_BEST_FIGHT_WEAPON; args[2]=IMAGE_SIZE-BOTLIB_INVENTORY_SIZE*sizeof(int);
	for(i=0;i<BOTLIB_INVENTORY_SIZE;i++) ((int *)(vm.dataBase+args[2]))[i]=i;
	Check(SV_BotLibAICalls(args)==3,"whole inventory"); args[2]+=4; Reject(args); args[2]--;
	Reject(args); args[2]=IMAGE_SIZE-BOTLIB_INVENTORY_SIZE*sizeof(int);
	args[0]=BOTLIB_AI_CHOOSE_NBG_ITEM; args[2]=4; args[3]=IMAGE_SIZE-BOTLIB_INVENTORY_SIZE*sizeof(int); args[4]=3; args[5]=0; memcpy(args+6,&one,4);
	Check(SV_BotLibAICalls(args)==4,"NULL long-term goal"); args[5]=64; Check(SV_BotLibAICalls(args)==4,"checked long-term goal");
	args[5]=IMAGE_SIZE-sizeof(bot_goal_t)+4; Reject(args);
	args[0]=BOTLIB_AI_MOVE_TO_GOAL; args[1]=IMAGE_SIZE-sizeof(bot_moveresult_t); args[2]=1; args[3]=0; args[4]=3;
	Check(SV_BotLibAICalls(args)==0,"full no-goal move result"); args[1]+=4; Reject(args);
	args[0]=BOTLIB_AI_MOVEMENT_VIEW_TARGET; args[1]=1; args[2]=0; args[3]=3; memcpy(args+4,&one,4); args[5]=0;
	Check(SV_BotLibAICalls(args)==0,"NULL no-goal target query"); args[2]=64; Reject(args);
	args[5]=IMAGE_SIZE-sizeof(vec3_t); Check(SV_BotLibAICalls(args)==5,"whole target output"); args[5]+=4; Reject(args);
	args[0]=BOTLIB_AI_GET_WEAPON_INFO; args[1]=1; args[2]=2; args[3]=IMAGE_SIZE-sizeof(weaponinfo_t);
	Check(SV_BotLibAICalls(args)==0,"whole weapon info"); args[3]+=4; Reject(args);
	args[0]=BOTLIB_AI_GENETIC_PARENTS_AND_CHILD_SELECTION; args[1]=3; args[2]=IMAGE_SIZE-3*sizeof(float); args[3]=64; args[4]=68; args[5]=72;
	for(i=0;i<3;i++) ((float *)(vm.dataBase+args[2]))[i]=i;
	Check(SV_BotLibAICalls(args)==6 && *(int *)(vm.dataBase+64)==1 && *(int *)(vm.dataBase+72)==3,"whole genetic input/output");
	args[2]+=4; Reject(args); args[2]-=4; args[1]=-1; Reject(args); args[1]=INT_MAX; Reject(args);
	args[1]=3; args[3]=0; Reject(args); args[3]=65; Reject(args);
	args[0]=BOTLIB_AI_CHARACTERISTIC_STRING; args[1]=1; args[2]=2; args[3]=IMAGE_SIZE-8; args[4]=8;
	Check(SV_BotLibAICalls(args)==0,"characteristic output"); args[4]=0; Reject(args); args[4]=9; Reject(args);
	args[0]=BOTLIB_AI_LOAD_CHARACTER; args[1]=32; memcpy(args+2,&one,4);
	Check(SV_BotLibAICalls(args)==7,"character filename"); args[1]=IMAGE_SIZE-1; Reject(args);
	botlib_export=NULL; args[1]=32; Reject(args);
	free(vm.dataBase); puts("Bot remaining AI dispatcher regressions passed (issue #35)"); return 0;
}
