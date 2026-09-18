/* Actual game initializers and the prefixes before first frame/client imports. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../code/game/g_local.h"
#include "../code/game/botlib.h"
#include "../code/game/be_aas.h"
#include "../code/game/be_ai_char.h"
#include "../code/game/be_ai_chat.h"
#include "../code/game/be_ai_goal.h"
#include "../code/game/be_ai_move.h"
#include "../code/game/be_ai_weap.h"
#include "../code/game/ai_main.h"
#define MAX_PATH 144
static qboolean botmapready;
bot_state_t *botstates[MAX_CLIENTS];
level_locals_t level;
gentity_t g_entities[MAX_GENTITIES];
gclient_t g_clients[MAX_CLIENTS];
vmCvar_t g_gametype,g_log,g_logSync,g_maxclients;
vmCvar_t bot_thinktime,bot_memorydump,bot_saveroutingcache,bot_pause,bot_report;
vmCvar_t bot_testsolid,bot_testclusters,bot_developer,bot_interbreedchar;
vmCvar_t bot_interbreedbots,bot_interbreedcycle,bot_interbreedwrite;
static int loadError,setupError,loads,resets,dmSetups,initBots,remaps,warnings,enabled;
static void Check(int ok,const char *message){if(!ok){fprintf(stderr,"FAIL: %s\n",message);exit(1);}}
int BotInitLibrary(void){return setupError;}
void BotResetState(bot_state_t *state){resets++;state->setupcount=0;}
void BotSetupDeathmatchAI(void){dmSetups++;}
int BotAIShutdownClient(int client,qboolean restart){(void)client;(void)restart;return qtrue;}
int trap_BotLibShutdown(void){return BLERR_NOERROR;}
void trap_Cvar_Register(vmCvar_t *cv,const char *name,const char *value,int flags){(void)value;(void)flags;if(!strcmp(name,"mapname"))strcpy(cv->string,"native");}
int trap_BotLibLoadMap(const char *name){Check(!strcmp(name,"native"),"native map name");loads++;return loadError;}
void QDECL G_Printf(const char *fmt,...){if(strstr(fmt,"bots disabled"))warnings++;}
void G_RegisterCvars(void){}
void G_ProcessIPBans(void){}
void G_InitMemory(void){}
void G_InitWorldSession(void){}
void InitBodyQue(void){}
void ClearRegisteredItems(void){}
void G_SpawnEntitiesFromString(void){}
void G_FindTeams(void){}
void G_CheckTeamItems(void){}
void SaveRegisteredItems(void){}
void G_RemapTeamShaders(void){remaps++;}
void G_InitBots(qboolean restart){(void)restart;initBots++;}
int G_SoundIndex(char *name){(void)name;return 1;}
int G_ModelIndex(char *name){(void)name;return 1;}
int trap_Cvar_VariableIntegerValue(const char *name){return !strcmp(name,"bot_enable")?enabled:0;}
int trap_FS_FOpenFile(const char *name,fileHandle_t *file,fsMode_t mode){(void)name;(void)mode;*file=0;return 0;}
void trap_GetServerinfo(char *buffer,int size){(void)size;buffer[0]=0;}
void QDECL G_LogPrintf(const char *fmt,...){(void)fmt;}
void trap_LocateGameData(gentity_t *ents,int count,int esize,playerState_t *clients,int csize){Check(ents==g_entities&&count==MAX_CLIENTS&&esize==sizeof(gentity_t)&&clients==&g_clients[0].ps&&csize==sizeof(gclient_t),"native game entity registration continues");}
#include Q3_GAME_MAP_BODIES
static bot_state_t priorBot;
static void Begin(int occupied){memset(botstates,0,sizeof(botstates));memset(&priorBot,0,sizeof(priorBot));priorBot.inuse=qtrue;priorBot.setupcount=9;priorBot.client=7;if(occupied)botstates[7]=&priorBot;botmapready=occupied;loadError=setupError=loads=resets=dmSetups=initBots=remaps=warnings=0;enabled=1;g_gametype.integer=GT_FFA;g_log.string[0]=0;g_maxclients.integer=2;}
static void MapFailure(int occupied,int error){bot_state_t saved;Begin(occupied);saved=priorBot;loadError=error;Check(!BotAILoadMap(0)&&loads==1&&!resets&&!dmSetups&&!botmapready&&!memcmp(&saved,&priorBot,sizeof(saved)),"game map error retains bot bytes and stops reset/deathmatch/readiness");Check(!BotAIStartFrame(100)&&!BotAISetupClient(7,NULL,0),"failed map blocks frame/client before their first import");Check(!BotAILoadMap(1)&&loads==1&&!resets&&!dmSetups&&!botmapready,"restart cannot revive a failed bot map");loadError=0;Check(BotAILoadMap(0)&&loads==2&&resets==occupied&&dmSetups==1&&botmapready&&(!occupied||priorBot.setupcount==4),"successful retry publishes bot readiness and native resets");Check(BotAIStartFrame(100)==73&&BotAISetupClient(7,NULL,0)==74,"successful retry reaches native frame/client first imports");}
static void MapGolden(void){Begin(1);Check(BotAILoadMap(0)&&loads==1&&resets==1&&dmSetups==1&&botmapready&&priorBot.setupcount==4,"native complete map load");Check(BotAILoadMap(1)&&loads==1&&resets==2&&dmSetups==2&&botmapready,"native restart skips library reload");Check(BotAIShutdown(1)&&botmapready,"restart shutdown retains valid map readiness");Check(BotAIShutdown(0)&&!botmapready,"full shutdown clears readiness");}
static void InitFailure(int setup){Begin(1);if(setup)setupError=BLERR_LIBRARYNOTSETUP;else loadError=BLERR_LIBRARYNOTSETUP;G_InitGame(200,7,0);Check(!botmapready&&loads==!setup&&!dmSetups&&!initBots&&remaps==1&&warnings==1&&level.time==200&&level.num_entities==MAX_CLIENTS,"game setup/map failure disables bots and completes human map setup");Check(!BotAIStartFrame(100)&&!BotAISetupClient(7,NULL,0),"game init failure blocks retained bot use");loadError=setupError=0;G_InitGame(300,7,0);Check(botmapready&&initBots==1&&remaps==2&&warnings==1&&dmSetups==1,"fresh successful initialization retries bot setup");}
static void InitGolden(void){Begin(0);G_InitGame(200,7,0);Check(botmapready&&loads==1&&dmSetups==1&&initBots==1&&remaps==1&&!warnings,"native successful game initializer");G_InitGame(300,7,1);Check(botmapready&&loads==1&&dmSetups==2&&initBots==2&&remaps==2&&!warnings,"native restart initializer");Begin(0);enabled=0;G_InitGame(200,7,0);Check(!loads&&!dmSetups&&!initBots&&!warnings&&remaps==1&&!botmapready,"native bots-disabled initializer");}
int main(int argc,char **argv){int occupied;if(argc>1){if(atoi(argv[1])==0)MapFailure(1,BLERR_LIBRARYNOTSETUP);else if(atoi(argv[1])==1)InitFailure(0);else{MapGolden();InitGolden();}return 0;}for(occupied=0;occupied<2;occupied++){MapFailure(occupied,BLERR_LIBRARYNOTSETUP);MapFailure(occupied,BLERR_CANNOTOPENAASFILE);}InitFailure(0);InitFailure(1);MapGolden();InitGolden();puts("Actual game map/setup errors disable bots before reset/frame/client imports; native retry/restart/human setup pass (issues #47/#48)");return 0;}
