/* Issue #35: execute the actual botlib navigation dispatcher with native callbacks. */
#include "../code/server/sv_game.c"
#include "../code/qcommon/vm_local.h"
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

#define IMAGE_SIZE 4096
server_t sv;
serverStatic_t svs;
static cvar_t maxclients;
cvar_t *sv_maxclients = &maxclients;
static vm_t vm;
vm_t *gvm = &vm;
static botlib_export_t api;
static byte before[IMAGE_SIZE];
static int expectError, callbacks;
static jmp_buf errorJump;

/** Stop when a buffer, return value, or native call differs. */
static void Check( int ok, const char *message ) {
	if ( !ok ) { fprintf( stderr, "Botlib navigation regression failed: %s\n", message ); exit( 1 ); }
}
/** Catch drops and require rejection before native callbacks or data mutation. */
void QDECL Com_Error( int level, const char *format, ... ) {
	(void)format;
	Check( expectError && level == ERR_DROP && vm.interpretFaulted && !vm.currentlyInterpreting,
	       "unexpected engine error or fault state" );
	Check( !memcmp( before, vm.dataBase, IMAGE_SIZE ), "rejection changed VM data" );
	longjmp( errorJump, 1 );
}
/** Ignore diagnostics from unused engine paths. */
void QDECL Com_Printf( const char *format, ... ) { (void)format; }
/** Assert every callback was reached only after validation. */
static void Callback( void ) { Check( !expectError, "invalid request reached native callback" ); callbacks++; }
/** Preserve setup's native return behavior. */
int SV_BotLibSetup( void ) { Callback(); return 7; }
/** Preserve shutdown's native return behavior. */
int SV_BotLibShutdown( void ) { Callback(); return 8; }
/** Check the client filter before snapshot retrieval. */
int SV_BotGetSnapshotEntity( int client, int sequence ) { Callback(); Check(client==1 && sequence==2,"snapshot args"); return 9; }
/** Touch the complete checked console output. */
int SV_BotGetConsoleMessage( int client, char *buffer, int size ) {
	Callback(); Check(client==1,"console client"); memset(buffer, 'c', size); return 10;
}
/** Check that the client pointer and whole command reach native thinking. */
void SV_ClientThink( client_t *client, usercmd_t *command ) {
	Callback(); Check(client==svs.clients+1 && (byte *)command==vm.dataBase+IMAGE_SIZE-sizeof(*command),"client command");
	Check(command->serverTime==42,"whole user command");
}
/** Check the bounded input string and touch the full output capacity. */
static int VarGet( char *name, char *value, int size ) { Callback(); Check(!strcmp(name,"test"),"var name"); memset(value,'v',size); return 11; }
/** Preserve NULL map-name queries. */
static int LoadMap( const char *name ) { Callback(); Check(!name || !strcmp(name,"test"),"map name"); return 12; }
/** Preserve NULL state removal and validate the whole entity-state boundary. */
static int UpdateEntity( int entity, bot_entitystate_t *state ) {
	Callback(); Check(entity==1,"entity number"); if(state) memset(state,0,sizeof(*state)); return 13;
}
/** Touch the complete native area-info output. */
static int AreaInfo( int area, aas_areainfo_t *info ) { Callback(); Check(area==1,"area number"); memset(info,0,sizeof(*info)); return 14; }
/** Touch all area outputs, exposing incomplete array range checks. */
static int BoxAreas( vec3_t mins, vec3_t maxs, int *areas, int count ) {
	Callback(); Check((byte *)mins==vm.dataBase+4 && (byte *)maxs==vm.dataBase+16,"box vectors"); memset(areas,0,count*sizeof(*areas)); return count;
}
/** Touch both parallel arrays while preserving an optional NULL point output. */
static int TraceAreas( vec3_t start, vec3_t end, int *areas, vec3_t *points, int count ) {
	Callback(); Check((byte *)start==vm.dataBase+4 && (byte *)end==vm.dataBase+16,"trace vectors");
	memset(areas,0,count*sizeof(*areas)); if(points) memset(points,0,count*sizeof(*points)); return count;
}
/** Preserve NULL reachability-count queries and checked non-NULL origins. */
static int ReachabilityIndex( vec3_t origin ) {
	Callback(); Check(!origin || (byte *)origin==vm.dataBase+4,"reachability origin"); return 15;
}
/** Preserve NULL-origin cached travel-time queries. */
static int TravelTime( int area, vec3_t origin, int goal, int flags ) {
	Callback(); Check(area==1 && goal==2 && flags==3,"travel args");
	Check(!origin || (byte *)origin==vm.dataBase+4,"travel origin"); return 16;
}
/** Require malformed arguments to fail before calling the native API. */
static void Reject( int *args ) {
	int calls = callbacks;
	vm.interpretFaulted = qfalse; vm.currentlyInterpreting = qtrue;
	memcpy(before,vm.dataBase,IMAGE_SIZE); expectError=1;
	if(setjmp(errorJump)==0) { SV_BotLibNavigationCalls(args); Check(0,"invalid request accepted"); }
	expectError=0; Check(callbacks==calls,"rejected request dispatched");
}
/** Cover strings, structs, arrays, optional outputs, empty capacities, and actual client allocation. */
int main( void ) {
	int args[16]={0}, i, calls;
	vm.dataBase=malloc(IMAGE_SIZE); Check(vm.dataBase!=NULL,"allocation");
	vm.dataMask=IMAGE_SIZE-1; currentVM=&vm; maxclients.integer=2;
	svs.clients=calloc(2,sizeof(*svs.clients)); Check(svs.clients!=NULL,"clients"); svs.clientCapacity=2;
	botlib_export=&api; api.BotLibVarGet=VarGet; api.BotLibLoadMap=LoadMap;
	api.BotLibUpdateEntity=UpdateEntity; api.aas.AAS_AreaInfo=AreaInfo;
	api.aas.AAS_BBoxAreas=BoxAreas; api.aas.AAS_TraceAreas=TraceAreas;
	api.aas.AAS_PointReachabilityAreaIndex=ReachabilityIndex; api.aas.AAS_AreaTravelTimeToGoalArea=TravelTime;
	memset(vm.dataBase,'x',IMAGE_SIZE); memcpy(vm.dataBase+32,"test",5);
	args[0]=BOTLIB_LIBVAR_GET; args[1]=32; args[2]=IMAGE_SIZE-8; args[3]=8;
	Check(SV_BotLibNavigationCalls(args)==11,"var result");
	args[3]=0; Reject(args); args[3]=9; Reject(args); args[3]=8;
	args[1]=IMAGE_SIZE-4; Reject(args); args[1]=32;
	args[0]=BOTLIB_LOAD_MAP; args[1]=0; Check(SV_BotLibNavigationCalls(args)==12,"NULL map query");
	args[1]=32; Check(SV_BotLibNavigationCalls(args)==12,"named map");
	args[0]=BOTLIB_AAS_POINT_REACHABILITY_AREA_INDEX; args[1]=0;
	Check(SV_BotLibNavigationCalls(args)==15,"NULL reachability count");
	args[1]=4; Check(SV_BotLibNavigationCalls(args)==15,"checked reachability origin");
	args[1]=IMAGE_SIZE-8; Reject(args);
	args[0]=BOTLIB_AAS_AREA_TRAVEL_TIME_TO_GOAL_AREA; args[1]=1; args[2]=0; args[3]=2; args[4]=3;
	Check(SV_BotLibNavigationCalls(args)==16,"NULL cached travel query");
	args[2]=4; Check(SV_BotLibNavigationCalls(args)==16,"checked travel origin");
	args[2]=IMAGE_SIZE-8; Reject(args);
	args[0]=BOTLIB_UPDATENTITY; args[1]=1; args[2]=0;
	Check(SV_BotLibNavigationCalls(args)==13,"NULL entity removal");
	args[2]=IMAGE_SIZE-sizeof(bot_entitystate_t); Check(SV_BotLibNavigationCalls(args)==13,"whole state");
	args[2]+=4; Reject(args);
	args[0]=BOTLIB_AAS_AREA_INFO; args[1]=1; args[2]=IMAGE_SIZE-sizeof(aas_areainfo_t);
	Check(SV_BotLibNavigationCalls(args)==14,"whole area info"); args[2]+=4; Reject(args);
	args[0]=BOTLIB_AAS_BBOX_AREAS; args[1]=4; args[2]=16; args[3]=IMAGE_SIZE-3*sizeof(int); args[4]=3;
	Check(SV_BotLibNavigationCalls(args)==3,"whole area array");
	args[3]+=4; Reject(args); args[3]-=4; args[4]=-1; Reject(args); args[4]=INT_MAX; Reject(args);
	args[4]=0; calls=callbacks; Check(SV_BotLibNavigationCalls(args)==0 && callbacks==calls,"empty box output");
	args[0]=BOTLIB_AAS_TRACE_AREAS; args[3]=64; args[4]=IMAGE_SIZE-3*sizeof(vec3_t); args[5]=3;
	Check(SV_BotLibNavigationCalls(args)==3,"parallel output arrays");
	args[4]+=4; Reject(args); args[4]=0; Check(SV_BotLibNavigationCalls(args)==3,"optional trace points");
	args[5]=0; calls=callbacks; Check(SV_BotLibNavigationCalls(args)==0 && callbacks==calls,"empty trace output");
	args[0]=BOTLIB_AAS_ALTERNATIVE_ROUTE_GOAL; args[7]=0;
	calls=callbacks; Check(SV_BotLibNavigationCalls(args)==0 && callbacks==calls,"empty alternate routes");
	args[0]=BOTLIB_GET_CONSOLE_MESSAGE; args[1]=1; args[2]=IMAGE_SIZE-8; args[3]=8;
	Check(SV_BotLibNavigationCalls(args)==10,"console output");
	args[0]=BOTLIB_GET_SNAPSHOT_ENTITY; args[2]=2; Check(SV_BotLibNavigationCalls(args)==9,"snapshot result");
	args[0]=BOTLIB_USER_COMMAND; args[2]=IMAGE_SIZE-sizeof(usercmd_t);
	((usercmd_t *)(vm.dataBase+args[2]))->serverTime=42;
	Check(SV_BotLibNavigationCalls(args)==0,"client command result");
	for(i=0;i<3;i++) {
		args[0]=i==0?BOTLIB_GET_CONSOLE_MESSAGE:i==1?BOTLIB_GET_SNAPSHOT_ENTITY:BOTLIB_USER_COMMAND;
		args[1]=-1; Reject(args); args[1]=INT_MAX; Reject(args);
		maxclients.integer=MAX_CLIENTS; args[1]=2; Reject(args);
	}
	args[0]=BOTLIB_AAS_AREA_INFO; botlib_export=NULL; Reject(args);
	args[0]=BOTLIB_SETUP; Check(SV_BotLibNavigationCalls(args)==7,"setup without export");
	args[0]=BOTLIB_SHUTDOWN; Check(SV_BotLibNavigationCalls(args)==8,"shutdown without export");
	free(svs.clients); free(vm.dataBase);
	puts("Botlib navigation dispatcher regressions passed (issue #35)");
	return 0;
}
