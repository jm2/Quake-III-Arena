/*
 * Issue #457: retail 1.32c loads qagame, cgame and ui as QVMs, so every
 * VM_Create and VM_Restart starts a module from a fresh image: initialized
 * data as in the file, everything else zero, the G_Alloc and UI_Alloc pools
 * included. The Mac build links the modules into the application, and their
 * globals used to carry over from one load to the next. Module code relies on
 * the fresh image, so this was not just a memory difference:
 *  - bots dropped on a map change ("BotAISetupClient: client N already
 *    setup"): BotAISetupClient takes a bot_state_t from G_Alloc and tests its
 *    inuse flag, which a fresh pool keeps zero (the #454 class);
 *  - after map_restart a new bot got another bot's bot_state_t and was
 *    dropped (botstates[] pointed into the pool G_InitMemory had reset);
 *  - every map change added the g_banIPs bans to ipFilters again, doubling
 *    g_banIPs, and removeip then left the address banned;
 *  - NumBots() grew by the bot count on every map change;
 *  - the cgame loading screen kept the earlier loads' item and player icons;
 *  - the q3_ui server browser kept its list after the ui was reloaded and
 *    never refreshed it, and the cursor kept its old position.
 *
 * Sys_LoadDll now puts the image back with VM_LoadStaticModule
 * (code/qcommon/vm_static.c) before every load, using brackets the linker
 * puts around each module's data and bss (cmake/static_modules.py on the
 * Mac). This test builds game, cgame and q3_ui from their real sources the
 * way the static build does, links them into one program with the same kind
 * of brackets (a GNU ld script from the runner), and drives them through the
 * real vmMain entry points against a stub engine. Shared code (bg_*.c,
 * q_shared.c, q_math.c) stays outside the brackets, as on the Mac.
 *
 * It checks, with the reset done as Sys_LoadDll does it:
 *  - the init after a reload leaves each module's memory byte for byte as the
 *    first init in a fresh process did, for a game map change and
 *    map_restart (with bots), a cgame reload and a ui reload;
 *  - every scenario above behaves as in a fresh process;
 *  - after the reset every module's memory equals its image at process start.
 * Each scenario is also run without the reset between loads (only at the
 * start of the scenario) and must then show the stale behaviour, so a
 * scenario that stops detecting it fails too.
 *
 * The runner compiles this file once per part: Q3_TEST_GAME, Q3_TEST_CGAME
 * and Q3_TEST_UI hold the stub traps of each module (they need that module's
 * headers), and the default part holds the stub engine and the checks.
 */

/* --- stub engine shared by every part --- */
void		Q3T_Fail( const char *fmt, ... );
void		Q3T_DefaultTrap( const char *name );
int			Q3T_CvarIndex( const char *name, const char *defaultValue );
const char	*Q3T_CvarString( int index );
int			Q3T_CvarModified( int index );
void		Q3T_CvarSet( const char *name, const char *value );
const char	*Q3T_CvarGet( const char *name );
void		Q3T_ConfigstringSet( int index, const char *value );
const char	*Q3T_ConfigstringGet( int index );
void		Q3T_ConfigstringsClear( void );
void		Q3T_UserinfoSet( int client, const char *value );
const char	*Q3T_UserinfoGet( int client );
int			Q3T_Handle( const char *name );
const char	*Q3T_HandleName( int handle );
int			Q3T_Milliseconds( void );
void		Q3T_SetArgs( const char *line );
int			Q3T_Argc( void );
const char	*Q3T_Argv( int n );
void		Q3T_Args( char *buffer, int size );

/* --- what the module parts offer the checks --- */
void		Q3Game_ResetEngine( void );
void		Q3Game_SetMap( int extraEntities );
void		Q3Game_Init( int restart );
void		Q3Game_Shutdown( int restart );
void		Q3Game_Frames( int count );
void		Q3Game_Console( const char *line );
int			Q3Game_Reconnect( int clientNum );
int			Q3Game_Drops( void );
int			Q3Game_NumBots( void );
int			Q3Game_FilterPacket( const char *address );
int			Q3Game_SharedBotStates( void );
int			Q3Game_InModule( int which );
void		Q3CGame_ResetEngine( void );
void		Q3CGame_Init( void );
void		Q3CGame_Shutdown( void );
void		Q3CGame_Frames( int count );
int			Q3CGame_LoadingIcons( void );
int			Q3CGame_InModule( void );
void		Q3UI_ResetEngine( void );
void		Q3UI_Init( void );
void		Q3UI_Shutdown( void );
void		Q3UI_MainMenu( void );
void		Q3UI_Refresh( int count );
void		Q3UI_Mouse( int dx, int dy );
void		Q3UI_OpenBrowser( void );
int			Q3UI_ServerQueries( void );
int			Q3UI_CursorX( void );
int			Q3UI_InModule( void );

#if defined( Q3_TEST_GAME )
/* ======================================================================
 * game: stub traps for qagame, compiled with the game module's macros
 * ====================================================================== */
#include "../code/game/g_local.h"
#include "../code/game/botlib.h"
#include "../code/game/be_aas.h"
#include "../code/game/be_ea.h"
#include "../code/game/be_ai_char.h"
#include "../code/game/be_ai_chat.h"
#include "../code/game/be_ai_gen.h"
#include "../code/game/be_ai_goal.h"
#include "../code/game/be_ai_move.h"
#include "../code/game/be_ai_weap.h"
#include "../code/game/ai_main.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int vmMain( int command, int arg0, int arg1, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7, int arg8, int arg9, int arg10, int arg11 );
void Sys_SnapVector( float *v );
extern bot_state_t	*botstates[MAX_CLIENTS];

static const char *entityString =
	"{\n\"classname\" \"worldspawn\"\n\"message\" \"Reset Arena\"\n}\n"
	"{\n\"classname\" \"info_player_deathmatch\"\n\"origin\" \"0 0 64\"\n\"angle\" \"90\"\n}\n"
	"{\n\"classname\" \"info_player_deathmatch\"\n\"origin\" \"256 0 64\"\n}\n"
	"{\n\"classname\" \"weapon_rocketlauncher\"\n\"origin\" \"128 0 32\"\n}\n"
	"{\n\"classname\" \"item_armor_body\"\n\"origin\" \"128 128 32\"\n}\n"
	"{\n\"classname\" \"item_health_large\"\n\"origin\" \"-128 0 32\"\n}\n"
	"{\n\"classname\" \"func_door\"\n\"model\" \"*1\"\n\"angle\" \"-1\"\n\"targetname\" \"door1\"\n}\n"
	"{\n\"classname\" \"trigger_multiple\"\n\"model\" \"*2\"\n\"target\" \"door1\"\n}\n";
static const char *entityParse;
static char		bigMap[65536];
static const char	*currentMap;

static const char *files[][2] = {
	{ "scripts/bots.txt",
	  "{\nname Sarge\nmodel sarge\nheadmodel sarge\naifile bots/sarge_c.c\n}\n"
	  "{\nname Grunt\nmodel grunt\nheadmodel grunt\naifile bots/grunt_c.c\n}\n"
	  "{\nname Major\nmodel major\nheadmodel major\naifile bots/major_c.c\n}\n"
	  "{\nname Doom\nmodel doom\nheadmodel doom\naifile bots/doom_c.c\n}\n" },
	{ "scripts/arenas.txt",
	  "{\nmap \"testmap\"\nbots \"sarge grunt\"\nlongname \"Reset Arena\"\nfraglimit 10\ntype \"ffa\"\n}\n" },
};
static int		fileOffset[2];
static int		botSlots[MAX_CLIENTS];
static int		levelTime;
static int		drops;

/* The test map, plus extraEntities info_null entities. */
void Q3Game_SetMap( int extraEntities ) {
	int i;
	Q_strncpyz( bigMap, entityString, sizeof( bigMap ) );
	for ( i = 0; i < extraEntities; i++ ) {
		Q_strcat( bigMap, sizeof( bigMap ), va( "{\n\"classname\" \"info_null\"\n\"targetname\" \"null%d\"\n}\n", i ) );
	}
	currentMap = bigMap;
}

void Q3Game_ResetEngine( void ) {
	Q3Game_SetMap( 0 );
	memset( fileOffset, 0, sizeof( fileOffset ) );
	memset( botSlots, 0, sizeof( botSlots ) );
	levelTime = 1000;
	drops = 0;
}

void trap_Printf( const char *fmt ) {}
void trap_Error( const char *fmt ) { Q3T_Fail( "game trap_Error: %s", fmt ); }
int trap_Milliseconds( void ) { return Q3T_Milliseconds(); }
int trap_Argc( void ) { return Q3T_Argc(); }
void trap_Argv( int n, char *buffer, int len ) { Q_strncpyz( buffer, Q3T_Argv( n ), len ); }
void trap_Args( char *buffer, int len ) { Q3T_Args( buffer, len ); }
int trap_FS_FOpenFile( const char *qpath, fileHandle_t *f, fsMode_t mode ) {
	int i;
	if ( f ) {
		*f = 0;
	}
	if ( mode != FS_READ ) {
		return -1;
	}
	for ( i = 0; i < (int)( sizeof( files ) / sizeof( files[0] ) ); i++ ) {
		if ( !Q_stricmp( qpath, files[i][0] ) ) {
			if ( f ) {
				*f = i + 1;
				fileOffset[i] = 0;
			}
			return strlen( files[i][1] );
		}
	}
	return -1;
}
void trap_FS_Read( void *buffer, int len, fileHandle_t f ) {
	memcpy( buffer, files[f - 1][1] + fileOffset[f - 1], len );
	fileOffset[f - 1] += len;
}
void trap_FS_FCloseFile( fileHandle_t f ) {}
void trap_Cvar_Register( vmCvar_t *vmCvar, const char *varName, const char *defaultValue, int flags ) {
	int i = Q3T_CvarIndex( varName, defaultValue );
	if ( vmCvar ) {
		vmCvar->handle = i;
		trap_Cvar_Update( vmCvar );
	}
}
void trap_Cvar_Update( vmCvar_t *vmCvar ) {
	vmCvar->modificationCount = Q3T_CvarModified( vmCvar->handle );
	Q_strncpyz( vmCvar->string, Q3T_CvarString( vmCvar->handle ), sizeof( vmCvar->string ) );
	vmCvar->value = atof( vmCvar->string );
	vmCvar->integer = atoi( vmCvar->string );
}
void trap_Cvar_Set( const char *name, const char *value ) { Q3T_CvarSet( name, value ); }
int trap_Cvar_VariableIntegerValue( const char *name ) { return atoi( Q3T_CvarGet( name ) ); }
void trap_Cvar_VariableStringBuffer( const char *name, char *buffer, int size ) { Q_strncpyz( buffer, Q3T_CvarGet( name ), size ); }
void trap_LocateGameData( gentity_t *gEnts, int numGEntities, int sizeofGEntity_t, playerState_t *clients, int sizeofGClient ) {}
void trap_DropClient( int clientNum, const char *reason ) { drops++; }
void trap_SendServerCommand( int clientNum, const char *text ) {}
void trap_SetConfigstring( int num, const char *string ) { Q3T_ConfigstringSet( num, string ); }
void trap_GetConfigstring( int num, char *buffer, int size ) { Q_strncpyz( buffer, Q3T_ConfigstringGet( num ), size ); }
void trap_GetUserinfo( int num, char *buffer, int size ) { Q_strncpyz( buffer, Q3T_UserinfoGet( num ), size ); }
void trap_SetUserinfo( int num, const char *buffer ) { Q3T_UserinfoSet( num, buffer ); }
void trap_GetServerinfo( char *buffer, int size ) {
	Com_sprintf( buffer, size, "\\mapname\\%s\\g_gametype\\%s\\sv_maxclients\\%s",
		Q3T_CvarGet( "mapname" ), Q3T_CvarGet( "g_gametype" ), Q3T_CvarGet( "sv_maxclients" ) );
}
void trap_LinkEntity( gentity_t *ent ) {
	ent->r.linked = qtrue;
	VectorAdd( ent->r.currentOrigin, ent->r.mins, ent->r.absmin );
	VectorAdd( ent->r.currentOrigin, ent->r.maxs, ent->r.absmax );
}
void trap_UnlinkEntity( gentity_t *ent ) { ent->r.linked = qfalse; }
void trap_SetBrushModel( gentity_t *ent, const char *name ) {
	ent->s.modelindex = atoi( name + 1 );
	VectorSet( ent->r.mins, -16, -16, 0 );
	VectorSet( ent->r.maxs, 16, 16, 64 );
	ent->r.bmodel = qtrue;
	ent->r.contents = CONTENTS_SOLID;
	trap_LinkEntity( ent );
}
void trap_Trace( trace_t *results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentmask ) {
	memset( results, 0, sizeof( *results ) );
	results->fraction = 1.0f;
	VectorCopy( end, results->endpos );
	results->entityNum = ENTITYNUM_NONE;
}
qboolean trap_InPVS( const vec3_t p1, const vec3_t p2 ) { return qtrue; }
qboolean trap_InPVSIgnorePortals( const vec3_t p1, const vec3_t p2 ) { return qtrue; }
qboolean trap_AreasConnected( int area1, int area2 ) { return qtrue; }
void trap_GetUsercmd( int clientNum, usercmd_t *cmd ) { memset( cmd, 0, sizeof( *cmd ) ); cmd->serverTime = levelTime; }
qboolean trap_GetEntityToken( char *buffer, int size ) {
	const char *token = COM_Parse( (char **)&entityParse );
	Q_strncpyz( buffer, token, size );
	return entityParse || token[0] ? qtrue : qfalse;
}
int trap_RealTime( qtime_t *qtime ) { memset( qtime, 0, sizeof( *qtime ) ); qtime->tm_year = 126; return 0; }
void trap_SnapVector( float *v ) { Sys_SnapVector( v ); }
/* botlib: just enough for bots to connect */
int trap_BotAllocateClient( void ) {
	int i;
	for ( i = 1; i < MAX_CLIENTS; i++ ) {
		if ( !botSlots[i] ) {
			botSlots[i] = 1;
			return i;
		}
	}
	return -1;
}
void trap_BotFreeClient( int clientNum ) { botSlots[clientNum] = 0; }
int trap_AAS_Initialized( void ) { return 1; }
int trap_BotLoadCharacter( char *charfile, float skill ) { return 1; }
void trap_Characteristic_String( int character, int index, char *buf, int size ) { if ( size ) buf[0] = 0; }
int trap_BotLibVarGet( char *name, char *value, int size ) { Q_strncpyz( value, "1", size ); return 0; }

void Q3Game_Init( int restart ) {
	entityParse = currentMap;	/* SV_InitGameVM restarts the entity parse */
	vmMain( GAME_INIT, levelTime, 4242, restart, 0, 0, 0, 0, 0, 0, 0, 0, 0 );
}
void Q3Game_Shutdown( int restart ) { vmMain( GAME_SHUTDOWN, restart, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 ); }
void Q3Game_Frames( int count ) {
	while ( count-- > 0 ) {
		levelTime += 50;
		vmMain( GAME_RUN_FRAME, levelTime, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 );
	}
}
void Q3Game_Console( const char *line ) {
	Q3T_SetArgs( line );
	vmMain( GAME_CONSOLE_COMMAND, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 );
}
/* SV_SpawnServer and SV_MapRestart_f connect every connected client again;
   ClientConnect is what GAME_CLIENT_CONNECT runs (it returns a pointer). */
int Q3Game_Reconnect( int clientNum ) {
	int before = drops;
	if ( ClientConnect( clientNum, qfalse, qtrue ) || drops != before ) {
		return 0;
	}
	vmMain( GAME_CLIENT_BEGIN, clientNum, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 );
	return 1;
}
int Q3Game_Drops( void ) { return drops; }
int Q3Game_NumBots( void ) { return NumBots(); }
int Q3Game_FilterPacket( const char *address ) { return G_FilterPacket( (char *)address ); }
int Q3Game_SharedBotStates( void ) {
	int a, b, shared = 0;
	for ( a = 0; a < MAX_CLIENTS; a++ ) {
		for ( b = 0; b < a; b++ ) {
			if ( botstates[a] && botstates[b] && botstates[a]->inuse && botstates[b]->inuse &&
				(char *)botstates[a] < (char *)botstates[b] + sizeof( bot_state_t ) &&
				(char *)botstates[b] < (char *)botstates[a] + sizeof( bot_state_t ) ) {
				shared++;
			}
		}
	}
	return shared;
}
/* 0: level (module bss), 1: gameCvarTable (module data), 2: bg_itemlist (shared) */
extern unsigned char q3static_game_data_start[], q3static_game_data_end[];
extern unsigned char q3static_game_bss_start[], q3static_game_bss_end[];
int Q3Game_InModule( int which ) {
	unsigned char *p = which == 0 ? (unsigned char *)&level :
		which == 1 ? (unsigned char *)&g_gametype : (unsigned char *)bg_itemlist;
	return ( p >= q3static_game_data_start && p < q3static_game_data_end ) ||
		( p >= q3static_game_bss_start && p < q3static_game_bss_end );
}

#elif defined( Q3_TEST_CGAME )
/* ======================================================================
 * cgame: stub traps for cgame, compiled with the cgame module's macros
 * ====================================================================== */
#include "../code/cgame/cg_local.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int vmMain( int command, int arg0, int arg1, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7, int arg8, int arg9, int arg10, int arg11 );
void Sys_SnapVector( float *v );

static int		snapNum, cmdNum, iconDraws;
static int		animOffset;
static char		animCfg[2048];

void Q3CGame_ResetEngine( void ) {
	int i;
	snapNum = 0;
	cmdNum = 0;
	animCfg[0] = 0;
	Q_strcat( animCfg, sizeof( animCfg ), "sex m\nfootsteps normal\n" );
	for ( i = 0; i < MAX_ANIMATIONS; i++ ) {
		Q_strcat( animCfg, sizeof( animCfg ), "0 10 0 15\n" );
	}
}

/* The gamestate of a server running testmap with two players. */
static void Gamestate( void ) {
	Q3T_ConfigstringSet( CS_SERVERINFO, "\\mapname\\testmap\\g_gametype\\0\\sv_hostname\\reset\\sv_maxclients\\8" );
	Q3T_ConfigstringSet( CS_SYSTEMINFO, "\\sv_serverid\\1234" );
	Q3T_ConfigstringSet( CS_GAME_VERSION, GAME_VERSION );
	Q3T_ConfigstringSet( CS_LEVEL_START_TIME, "1000" );
	Q3T_ConfigstringSet( CS_MESSAGE, "Reset Arena" );
	Q3T_ConfigstringSet( CS_MODELS + 1, "*1" );
	/* the shotgun, rocket launcher and red armor */
	Q3T_ConfigstringSet( CS_ITEMS, "000100000101" );
	Q3T_ConfigstringSet( CS_PLAYERS + 0, "n\\Tester\\t\\0\\model\\sarge\\hmodel\\sarge\\c1\\4\\c2\\5\\hc\\100" );
	Q3T_ConfigstringSet( CS_PLAYERS + 1, "n\\Other\\t\\0\\model\\grunt\\hmodel\\grunt\\c1\\4\\c2\\5\\hc\\100" );
}

void trap_Print( const char *fmt ) {}
void trap_Error( const char *fmt ) { Q3T_Fail( "cgame trap_Error: %s", fmt ); }
int trap_Milliseconds( void ) { return Q3T_Milliseconds(); }
void trap_Cvar_Register( vmCvar_t *vmCvar, const char *varName, const char *defaultValue, int flags ) {
	int i = Q3T_CvarIndex( varName, defaultValue );
	if ( vmCvar ) {
		vmCvar->handle = i;
		trap_Cvar_Update( vmCvar );
	}
}
void trap_Cvar_Update( vmCvar_t *vmCvar ) {
	vmCvar->modificationCount = Q3T_CvarModified( vmCvar->handle );
	Q_strncpyz( vmCvar->string, Q3T_CvarString( vmCvar->handle ), sizeof( vmCvar->string ) );
	vmCvar->value = atof( vmCvar->string );
	vmCvar->integer = atoi( vmCvar->string );
}
void trap_Cvar_Set( const char *name, const char *value ) { Q3T_CvarSet( name, value ); }
void trap_Cvar_VariableStringBuffer( const char *name, char *buffer, int size ) { Q_strncpyz( buffer, Q3T_CvarGet( name ), size ); }
int trap_Argc( void ) { return Q3T_Argc(); }
void trap_Argv( int n, char *buffer, int len ) { Q_strncpyz( buffer, Q3T_Argv( n ), len ); }
void trap_Args( char *buffer, int len ) { Q3T_Args( buffer, len ); }
int trap_FS_FOpenFile( const char *qpath, fileHandle_t *f, fsMode_t mode ) {
	if ( f ) {
		*f = 0;
	}
	if ( mode != FS_READ ) {
		return -1;
	}
	if ( strstr( qpath, "animation.cfg" ) ) {
		if ( f ) {
			*f = 1;
			animOffset = 0;
		}
		return strlen( animCfg );
	}
	/* every player model has its default skins; nothing reads them */
	if ( !f && !Q_stricmpn( qpath, "models/players/", 15 ) && strstr( qpath, "_default.skin" ) &&
		!strstr( qpath, "/heads/" ) ) {
		return 1;
	}
	return -1;
}
void trap_FS_Read( void *buffer, int len, fileHandle_t f ) { memcpy( buffer, animCfg + animOffset, len ); animOffset += len; }
void trap_FS_FCloseFile( fileHandle_t f ) {}
int trap_CM_NumInlineModels( void ) { return 2; }
clipHandle_t trap_CM_InlineModel( int index ) { return index; }
void trap_CM_BoxTrace( trace_t *results, const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, clipHandle_t model, int brushmask ) {
	memset( results, 0, sizeof( *results ) );
	results->fraction = 1.0f;
	VectorCopy( end, results->endpos );
	results->entityNum = ENTITYNUM_NONE;
}
void trap_CM_TransformedBoxTrace( trace_t *results, const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs, clipHandle_t model, int brushmask, const vec3_t origin, const vec3_t angles ) {
	trap_CM_BoxTrace( results, start, end, mins, maxs, model, brushmask );
}
sfxHandle_t trap_S_RegisterSound( const char *sample, qboolean compressed ) { return Q3T_Handle( sample ); }
qhandle_t trap_R_RegisterModel( const char *name ) { return Q3T_Handle( name ); }
qhandle_t trap_R_RegisterSkin( const char *name ) { return Q3T_Handle( name ); }
qhandle_t trap_R_RegisterShader( const char *name ) { return Q3T_Handle( name ); }
qhandle_t trap_R_RegisterShaderNoMip( const char *name ) { return Q3T_Handle( name ); }
int trap_R_LerpTag( orientation_t *tag, clipHandle_t mod, int startFrame, int endFrame, float frac, const char *tagName ) {
	memset( tag, 0, sizeof( *tag ) );
	AxisClear( tag->axis );
	return 1;
}
void trap_R_ModelBounds( clipHandle_t model, vec3_t mins, vec3_t maxs ) { VectorSet( mins, -8, -8, -8 ); VectorSet( maxs, 8, 8, 8 ); }
void trap_R_DrawStretchPic( float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader ) {
	if ( strstr( Q3T_HandleName( hShader ), "icon" ) ) {
		iconDraws++;
	}
}
void trap_GetGlconfig( glconfig_t *glconfig ) {
	memset( glconfig, 0, sizeof( *glconfig ) );
	glconfig->vidWidth = 640;
	glconfig->vidHeight = 480;
	glconfig->colorBits = 32;
}
void trap_GetGameState( gameState_t *gs ) {
	int i, len;
	memset( gs, 0, sizeof( *gs ) );
	gs->dataCount = 1;
	for ( i = 0; i < MAX_CONFIGSTRINGS; i++ ) {
		const char *s = Q3T_ConfigstringGet( i );
		if ( !s[0] ) {
			continue;
		}
		len = strlen( s );
		gs->stringOffsets[i] = gs->dataCount;
		memcpy( gs->stringData + gs->dataCount, s, len + 1 );
		gs->dataCount += len + 1;
	}
}
void trap_GetCurrentSnapshotNumber( int *snapshotNumber, int *serverTime ) {
	*snapshotNumber = snapNum;
	*serverTime = 1000 + 50 * snapNum;
}
qboolean trap_GetSnapshot( int snapshotNumber, snapshot_t *snap ) {
	memset( snap, 0, sizeof( *snap ) );
	snap->serverTime = 1000 + 50 * snapshotNumber;
	snap->ps.commandTime = snap->serverTime;
	snap->ps.stats[STAT_HEALTH] = 100;
	snap->ps.stats[STAT_WEAPONS] = 1 << WP_MACHINEGUN;
	snap->ps.weapon = WP_MACHINEGUN;
	snap->ps.ammo[WP_MACHINEGUN] = 50;
	VectorSet( snap->ps.origin, 0, 0, 64 );
	snap->ps.viewheight = 26;
	snap->numEntities = 1;
	snap->entities[0].number = 1;
	snap->entities[0].eType = ET_PLAYER;
	snap->entities[0].clientNum = 1;
	VectorSet( snap->entities[0].pos.trBase, 128, 0, 64 );
	return qtrue;
}
int trap_GetCurrentCmdNumber( void ) { return cmdNum; }
qboolean trap_GetUserCmd( int cmdNumber, usercmd_t *ucmd ) {
	memset( ucmd, 0, sizeof( *ucmd ) );
	ucmd->serverTime = 1000 + 16 * cmdNumber;
	return qtrue;
}
int trap_MemoryRemaining( void ) { return 8 << 20; }
int trap_RealTime( qtime_t *qtime ) { memset( qtime, 0, sizeof( *qtime ) ); qtime->tm_year = 126; return 0; }
void trap_SnapVector( float *v ) { Sys_SnapVector( v ); }

void Q3CGame_Init( void ) {
	Gamestate();
	vmMain( CG_INIT, snapNum, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 );
}
void Q3CGame_Shutdown( void ) { vmMain( CG_SHUTDOWN, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 ); }
void Q3CGame_Frames( int count ) {
	while ( count-- > 0 ) {
		snapNum++;
		cmdNum += 3;
		vmMain( CG_DRAW_ACTIVE_FRAME, 1000 + 50 * snapNum + 10, STEREO_CENTER, qfalse, 0, 0, 0, 0, 0, 0, 0, 0, 0 );
	}
}
/* The loading screen (CG_DrawInformation) of a cgame that has no snapshot. */
int Q3CGame_LoadingIcons( void ) {
	int saved = snapNum;
	iconDraws = 0;
	snapNum = 0;
	vmMain( CG_DRAW_ACTIVE_FRAME, 1000, STEREO_CENTER, qfalse, 0, 0, 0, 0, 0, 0, 0, 0, 0 );
	snapNum = saved;
	return iconDraws;
}
extern unsigned char q3static_cgame_bss_start[], q3static_cgame_bss_end[];
int Q3CGame_InModule( void ) {
	return (unsigned char *)&cg >= q3static_cgame_bss_start && (unsigned char *)&cg < q3static_cgame_bss_end &&
		(unsigned char *)&cg_pmoveFixed >= q3static_cgame_bss_start &&
		(unsigned char *)&cg_pmoveFixed < q3static_cgame_bss_end;
}

#elif defined( Q3_TEST_UI )
/* ======================================================================
 * ui: stub traps for q3_ui, compiled with the ui module's macros
 * ====================================================================== */
#include "../code/q3_ui/ui_local.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int vmMain( int command, int arg0, int arg1, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7, int arg8, int arg9, int arg10, int arg11 );

#define SERVER_ADDRESS	"10.0.0.5:27960"
static int		realtime, catcher, serverQueries, pinged, cursorX;

void Q3UI_ResetEngine( void ) {
	realtime = 0;
	catcher = 0;
	serverQueries = 0;
	pinged = 0;
	cursorX = -1000;
}

void trap_Print( const char *s ) {}
void trap_Error( const char *s ) { Q3T_Fail( "ui trap_Error: %s", s ); }
int trap_Milliseconds( void ) { return Q3T_Milliseconds(); }
void trap_Cvar_Register( vmCvar_t *vmCvar, const char *varName, const char *defaultValue, int flags ) {
	int i = Q3T_CvarIndex( varName, defaultValue );
	if ( vmCvar ) {
		vmCvar->handle = i;
		trap_Cvar_Update( vmCvar );
	}
}
void trap_Cvar_Update( vmCvar_t *vmCvar ) {
	vmCvar->modificationCount = Q3T_CvarModified( vmCvar->handle );
	Q_strncpyz( vmCvar->string, Q3T_CvarString( vmCvar->handle ), sizeof( vmCvar->string ) );
	vmCvar->value = atof( vmCvar->string );
	vmCvar->integer = atoi( vmCvar->string );
}
void trap_Cvar_Set( const char *name, const char *value ) { Q3T_CvarSet( name, value ); }
float trap_Cvar_VariableValue( const char *name ) { return atof( Q3T_CvarGet( name ) ); }
void trap_Cvar_VariableStringBuffer( const char *name, char *buffer, int size ) { Q_strncpyz( buffer, Q3T_CvarGet( name ), size ); }
void trap_Cvar_SetValue( const char *name, float value ) { Q3T_CvarSet( name, va( "%g", value ) ); }
void trap_Cvar_Create( const char *name, const char *value, int flags ) { Q3T_CvarIndex( name, value ); }
int trap_Argc( void ) { return Q3T_Argc(); }
void trap_Argv( int n, char *buffer, int len ) { Q_strncpyz( buffer, Q3T_Argv( n ), len ); }
void trap_Cmd_ExecuteText( int exec_when, const char *text ) {
	if ( !strcmp( text, "localservers\n" ) ) {
		serverQueries++;
	} else if ( !strcmp( text, "ping " SERVER_ADDRESS "\n" ) ) {
		pinged = 1;
	}
}
int trap_FS_FOpenFile( const char *qpath, fileHandle_t *f, fsMode_t mode ) { if ( f ) *f = 0; return -1; }
int trap_FS_GetFileList( const char *path, const char *extension, char *listbuf, int bufsize ) { if ( bufsize ) listbuf[0] = 0; return 0; }
qhandle_t trap_R_RegisterModel( const char *name ) { return Q3T_Handle( name ); }
qhandle_t trap_R_RegisterSkin( const char *name ) { return Q3T_Handle( name ); }
qhandle_t trap_R_RegisterShaderNoMip( const char *name ) { return Q3T_Handle( name ); }
sfxHandle_t trap_S_RegisterSound( const char *sample, qboolean compressed ) { return Q3T_Handle( sample ); }
void trap_R_DrawStretchPic( float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader ) {
	if ( !strcmp( Q3T_HandleName( hShader ), "menu/art/3_cursor2" ) ) {
		cursorX = (int)x;
	}
}
int trap_CM_LerpTag( orientation_t *tag, clipHandle_t mod, int startFrame, int endFrame, float frac, const char *tagName ) {
	memset( tag, 0, sizeof( *tag ) );
	AxisClear( tag->axis );
	return 1;
}
void trap_Key_KeynumToStringBuf( int keynum, char *buf, int buflen ) { Q_strncpyz( buf, "KEY", buflen ); }
void trap_Key_GetBindingBuf( int keynum, char *buf, int buflen ) { if ( buflen ) buf[0] = 0; }
int trap_Key_GetCatcher( void ) { return catcher; }
void trap_Key_SetCatcher( int value ) { catcher = value; }
void trap_GetClipboardData( char *buf, int bufsize ) { if ( bufsize ) buf[0] = 0; }
void trap_GetClientState( uiClientState_t *state ) { memset( state, 0, sizeof( *state ) ); state->connState = CA_DISCONNECTED; }
void trap_GetGlconfig( glconfig_t *glconfig ) {
	memset( glconfig, 0, sizeof( *glconfig ) );
	glconfig->vidWidth = 640;
	glconfig->vidHeight = 480;
	glconfig->colorBits = 32;
}
int trap_GetConfigString( int index, char *buff, int buffsize ) { Q_strncpyz( buff, Q3T_ConfigstringGet( index ), buffsize ); return 1; }
int trap_MemoryRemaining( void ) { return 8 << 20; }
void trap_GetCDKey( char *buf, int buflen ) { if ( buflen ) buf[0] = 0; }
int trap_RealTime( qtime_t *qtime ) { memset( qtime, 0, sizeof( *qtime ) ); qtime->tm_year = 126; return 0; }
void trap_Cvar_InfoStringBuffer( int bit, char *buffer, int bufsize ) { if ( bufsize ) buffer[0] = 0; }
/* one LAN server, answering the browser's first ping */
int trap_LAN_GetServerCount( int source ) { return source == AS_LOCAL ? 1 : 0; }
void trap_LAN_GetServerAddressString( int source, int n, char *buf, int buflen ) { Q_strncpyz( buf, SERVER_ADDRESS, buflen ); }
void trap_LAN_GetServerInfo( int source, int n, char *buf, int buflen ) { if ( buflen ) buf[0] = 0; }
int trap_LAN_GetPingQueueCount( void ) { return pinged; }
void trap_LAN_ClearPing( int n ) { if ( !n ) pinged = 0; }
void trap_LAN_GetPing( int n, char *buf, int buflen, int *pingtime ) {
	Q_strncpyz( buf, !n && pinged ? SERVER_ADDRESS : "", buflen );
	*pingtime = !n && pinged ? 50 : 0;
}
void trap_LAN_GetPingInfo( int n, char *buf, int buflen ) {
	Q_strncpyz( buf, "\\hostname\\LAN Arena\\mapname\\q3dm1\\clients\\1\\sv_maxclients\\8\\gametype\\0\\nettype\\1\\minping\\0\\maxping\\0", buflen );
}

void Q3UI_Init( void ) { vmMain( UI_INIT, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 ); }
void Q3UI_Shutdown( void ) { vmMain( UI_SHUTDOWN, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 ); }
void Q3UI_MainMenu( void ) { vmMain( UI_SET_ACTIVE_MENU, UIMENU_MAIN, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 ); }
void Q3UI_Refresh( int count ) {
	while ( count-- > 0 ) {
		realtime += 16;
		vmMain( UI_REFRESH, realtime, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 );
	}
}
void Q3UI_Mouse( int dx, int dy ) { vmMain( UI_MOUSE_EVENT, dx, dy, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 ); }
/* What the main menu's Multiplayer item opens. */
void Q3UI_OpenBrowser( void ) { UI_ArenaServersMenu(); }
int Q3UI_ServerQueries( void ) { return serverQueries; }
int Q3UI_CursorX( void ) { return cursorX; }
extern unsigned char q3static_ui_bss_start[], q3static_ui_bss_end[];
int Q3UI_InModule( void ) {
	return (unsigned char *)&uis >= q3static_ui_bss_start && (unsigned char *)&uis < q3static_ui_bss_end;
}

#else
/* ======================================================================
 * the stub engine and the checks
 * ====================================================================== */
#include "../code/qcommon/vm_static.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* the vmMain of each module, renamed as in CMakeLists.txt */
int Game_vmMain( int command, int arg0, int arg1, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7, int arg8, int arg9, int arg10, int arg11 );

/* the runner's linker script brackets each module as cmake/static_modules.py does */
extern unsigned char q3static_game_data_start[], q3static_game_data_end[];
extern unsigned char q3static_game_bss_start[], q3static_game_bss_end[];
extern unsigned char q3static_cgame_data_start[], q3static_cgame_data_end[];
extern unsigned char q3static_cgame_bss_start[], q3static_cgame_bss_end[];
extern unsigned char q3static_ui_data_start[], q3static_ui_data_end[];
extern unsigned char q3static_ui_bss_start[], q3static_ui_bss_end[];

/* the same table as code/mac/mac_main.c */
static vmStaticModule_t modules[] = {
	{ "qagame", q3static_game_data_start, q3static_game_data_end,
		q3static_game_bss_start, q3static_game_bss_end },
	{ "cgame", q3static_cgame_data_start, q3static_cgame_data_end,
		q3static_cgame_bss_start, q3static_cgame_bss_end },
	{ "ui", q3static_ui_data_start, q3static_ui_data_end,
		q3static_ui_bss_start, q3static_ui_bss_end },
};
#define MODULES		( (int)( sizeof( modules ) / sizeof( modules[0] ) ) )
enum { GAME, CGAME, UI };

static char		currentCase[256];
static int		checks;

void Q3T_Fail( const char *fmt, ... ) {
	va_list ap;
	fprintf( stderr, "static module reset regression failed: %s: ", currentCase );
	va_start( ap, fmt );
	vfprintf( stderr, fmt, ap );
	va_end( ap );
	fputc( '\n', stderr );
	exit( 1 );
}

static void Check( int condition, const char *what ) {
	checks++;
	if ( !condition ) {
		Q3T_Fail( "%s", what );
	}
}

void Q3T_DefaultTrap( const char *name ) {}

/* --- engine state: cvars, configstrings, userinfo, handles, clock, arguments --- */
#define MAX_TEST_CVARS		1024
#define MAX_TEST_STRINGS	1024
#define MAX_TEST_HANDLES	4096

static char		cvarNames[MAX_TEST_CVARS][64], cvarValues[MAX_TEST_CVARS][256];
static int		cvarModified[MAX_TEST_CVARS], numCvars;
static char		*configstrings[MAX_TEST_STRINGS];
static char		userinfo[64][1024];
static char		handleNames[MAX_TEST_HANDLES][64];
static int		numHandles, milliseconds;
static char		args[16][256];
static int		argc;

static void ResetEngine( void ) {
	int i;
	numCvars = 1;	/* handle 0 is unused, as in the engine */
	for ( i = 0; i < MAX_TEST_STRINGS; i++ ) {
		free( configstrings[i] );
		configstrings[i] = NULL;
	}
	memset( userinfo, 0, sizeof( userinfo ) );
	numHandles = 1;
	milliseconds = 0;
	argc = 0;
	srand( 1 );
	Q3Game_ResetEngine();
	Q3CGame_ResetEngine();
	Q3UI_ResetEngine();
}

int Q3T_CvarIndex( const char *name, const char *defaultValue ) {
	int i;
	for ( i = 1; i < numCvars; i++ ) {
		if ( !strcasecmp( cvarNames[i], name ) ) {
			return i;
		}
	}
	if ( numCvars == MAX_TEST_CVARS ) {
		Q3T_Fail( "too many cvars" );
	}
	snprintf( cvarNames[numCvars], sizeof( cvarNames[0] ), "%s", name );
	snprintf( cvarValues[numCvars], sizeof( cvarValues[0] ), "%s", defaultValue ? defaultValue : "" );
	cvarModified[numCvars] = 1;
	return numCvars++;
}
const char *Q3T_CvarString( int index ) { return index > 0 && index < numCvars ? cvarValues[index] : ""; }
int Q3T_CvarModified( int index ) { return index > 0 && index < numCvars ? cvarModified[index] : 0; }
void Q3T_CvarSet( const char *name, const char *value ) {
	int i = Q3T_CvarIndex( name, value );
	if ( strcmp( cvarValues[i], value ) ) {
		snprintf( cvarValues[i], sizeof( cvarValues[0] ), "%s", value );
		cvarModified[i]++;
	}
}
const char *Q3T_CvarGet( const char *name ) {
	int i;
	for ( i = 1; i < numCvars; i++ ) {
		if ( !strcasecmp( cvarNames[i], name ) ) {
			return cvarValues[i];
		}
	}
	return "";
}
void Q3T_ConfigstringSet( int index, const char *value ) {
	if ( index < 0 || index >= MAX_TEST_STRINGS ) {
		Q3T_Fail( "configstring %d out of range", index );
	}
	free( configstrings[index] );
	configstrings[index] = malloc( strlen( value ) + 1 );
	strcpy( configstrings[index], value );
}
const char *Q3T_ConfigstringGet( int index ) {
	return index >= 0 && index < MAX_TEST_STRINGS && configstrings[index] ? configstrings[index] : "";
}
void Q3T_ConfigstringsClear( void ) {
	int i;
	for ( i = 0; i < MAX_TEST_STRINGS; i++ ) {
		free( configstrings[i] );
		configstrings[i] = NULL;
	}
}
void Q3T_UserinfoSet( int client, const char *value ) { snprintf( userinfo[client], sizeof( userinfo[0] ), "%s", value ); }
const char *Q3T_UserinfoGet( int client ) { return userinfo[client]; }
/* The renderer and sound system hand out one handle per name. */
int Q3T_Handle( const char *name ) {
	int i;
	for ( i = 1; i < numHandles; i++ ) {
		if ( !strcasecmp( handleNames[i], name ) ) {
			return i;
		}
	}
	if ( numHandles == MAX_TEST_HANDLES ) {
		Q3T_Fail( "too many handles" );
	}
	snprintf( handleNames[numHandles], sizeof( handleNames[0] ), "%s", name );
	return numHandles++;
}
const char *Q3T_HandleName( int handle ) { return handle > 0 && handle < numHandles ? handleNames[handle] : ""; }
int Q3T_Milliseconds( void ) { return ++milliseconds; }
void Q3T_SetArgs( const char *line ) {
	argc = 0;
	while ( *line && argc < 16 ) {
		int n = 0;
		while ( *line == ' ' ) {
			line++;
		}
		if ( !*line ) {
			break;
		}
		while ( *line && *line != ' ' && n < 255 ) {
			args[argc][n++] = *line++;
		}
		args[argc++][n] = 0;
	}
}
int Q3T_Argc( void ) { return argc; }
const char *Q3T_Argv( int n ) { return n >= 0 && n < argc ? args[n] : ""; }
void Q3T_Args( char *buffer, int size ) {
	int i;
	buffer[0] = 0;
	for ( i = 1; i < argc; i++ ) {
		snprintf( buffer + strlen( buffer ), size - strlen( buffer ), "%s%s", i > 1 ? " " : "", args[i] );
	}
}

/* engine functions the hard-linked modules call directly */
void Com_Error( int level, const char *fmt, ... ) {
	char text[1024];
	va_list ap;
	va_start( ap, fmt );
	vsnprintf( text, sizeof( text ), fmt, ap );
	va_end( ap );
	Q3T_Fail( "Com_Error( %d, %s )", level, text );
}
void Com_Printf( const char *fmt, ... ) {}
void Sys_SnapVector( float *v ) {
	int i;
	for ( i = 0; i < 3; i++ ) {
		v[i] = (float)(int)( v[i] < 0 ? v[i] - 0.5f : v[i] + 0.5f );
	}
}

/* --- loading modules the way Sys_LoadDll and Sys_UnloadDll do --- */

/* 1: VM_Create resets the module (the fix); 0: it does not (the old build). */
static int		resetOnLoad;

static void Load( int m ) {
	const char *error;
	if ( !resetOnLoad ) {
		return;
	}
	error = VM_LoadStaticModule( &modules[m] );
	if ( error ) {
		Q3T_Fail( "VM_LoadStaticModule( %s ): %s", modules[m].name, error );
	}
}
static void Unload( int m ) {
	VM_UnloadStaticModule( &modules[m] );
}
/* Each module's data and bss as the process started. */
static unsigned char	*startImages[MODULES];

/* Each scenario starts as a fresh process would, with or without the fix:
   from the test's own copy of the process start, not VM_LoadStaticModule. */
static void FreshProcess( const char *name ) {
	int m;
	snprintf( currentCase, sizeof( currentCase ), "%s (%s)", name,
		resetOnLoad ? "reset on load" : "no reset on load" );
	for ( m = 0; m < MODULES; m++ ) {
		size_t data = modules[m].dataEnd - modules[m].dataStart;
		VM_UnloadStaticModule( &modules[m] );
		memcpy( modules[m].dataStart, startImages[m], data );
		memcpy( modules[m].bssStart, startImages[m] + data, modules[m].bssEnd - modules[m].bssStart );
	}
	ResetEngine();
}

static unsigned char *Image( int m ) {
	vmStaticModule_t *module = &modules[m];
	size_t data = module->dataEnd - module->dataStart, bss = module->bssEnd - module->bssStart;
	unsigned char *image = malloc( data + bss + 1 );
	memcpy( image, module->dataStart, data );
	memcpy( image + data, module->bssStart, bss );
	return image;
}
/* Returns the number of bytes of the module that differ from image. */
static long Differences( int m, const unsigned char *image, long *first ) {
	vmStaticModule_t *module = &modules[m];
	size_t data = module->dataEnd - module->dataStart, bss = module->bssEnd - module->bssStart, i;
	long count = 0;
	*first = -1;
	for ( i = 0; i < data + bss; i++ ) {
		unsigned char now = i < data ? module->dataStart[i] : module->bssStart[i - data];
		if ( now != image[i] ) {
			if ( *first < 0 ) {
				*first = (long)i;
			}
			count++;
		}
	}
	return count;
}

/* Fails when bytes of module m differed from what they should be. */
static void CheckSame( int m, long differ, long first, const char *what ) {
	checks++;
	if ( differ ) {
		Q3T_Fail( "%ld bytes of the %s module differ from %s, from offset %ld",
			differ, modules[m].name, what, first );
	}
}

/* --- the game --- */

static void GameServer( int bots ) {
	Q3T_CvarSet( "bot_enable", bots ? "1" : "0" );
	Q3T_CvarSet( "sv_maxclients", "8" );
	Q3T_CvarSet( "mapname", "testmap" );
	Q3T_CvarSet( "g_log", "" );
}

/* A map change runs GAME_SHUTDOWN, frees the VM, creates it and runs GAME_INIT;
   a map_restart does the same through VM_Restart (free, then create). */
static void GameReload( int restart ) {
	Q3Game_Shutdown( restart );
	Unload( GAME );
	if ( !restart ) {
		Q3T_ConfigstringsClear();	/* SV_SpawnServer clears every configstring */
	}
	Load( GAME );
	Q3Game_Init( restart );
}

/* The init after a reload leaves the module as the first init did. */
static long GameInitMatchesFresh( int restart ) {
	unsigned char *fresh;
	long differ, first;

	FreshProcess( restart ? "game init after map_restart" : "game init after a map change" );
	GameServer( 1 );
	Q3T_CvarSet( "g_banIPs", "10.0.0.1 " );
	Load( GAME );
	Q3Game_Init( restart );
	fresh = Image( GAME );
	Q3Game_Console( "addbot sarge 3" );
	Q3Game_Console( "addbot grunt 3" );
	Q3Game_Frames( 20 );
	Q3Game_Console( "addip 192.168.1.1" );
	Q3Game_Frames( 20 );
	Q3Game_Shutdown( restart );
	Unload( GAME );
	/* the engine as the first init found it */
	ResetEngine();
	GameServer( 1 );
	Q3T_CvarSet( "g_banIPs", "10.0.0.1 " );
	Load( GAME );
	Q3Game_Init( restart );
	differ = Differences( GAME, fresh, &first );
	free( fresh );
	if ( resetOnLoad ) {
		CheckSame( GAME, differ, first, "its first init" );
	}
	return differ;
}

/* Bots stay when the server reloads the map (map change with bots). */
static int BotsDroppedOnMapChange( void ) {
	int map, dropped = 0;

	FreshProcess( "bots across map changes" );
	GameServer( 1 );
	Load( GAME );
	Q3Game_Init( 0 );
	Q3Game_Console( "addbot sarge 3" );
	Q3Game_Console( "addbot grunt 3" );
	Check( Q3Game_Drops() == 0 && Q3Game_NumBots() == 2, "two bots join the first map" );
	for ( map = 2; map <= 4; map++ ) {
		Q3Game_Frames( 10 );
		GameReload( 0 );
		dropped += !Q3Game_Reconnect( 1 );
		dropped += !Q3Game_Reconnect( 2 );
	}
	if ( resetOnLoad ) {
		Check( dropped == 0, "both bots reconnect after every map change" );
		Check( Q3Game_NumBots() == 2, "NumBots() is still 2 after three map changes" );
	}
	return dropped;
}

/* NumBots() counts the bots in the game. The maps grow, so the bots reconnect
   even without the reset. */
static int NumBotsAcrossMapChanges( void ) {
	int map, drift = 0;

	FreshProcess( "NumBots() across map changes" );
	GameServer( 1 );
	Load( GAME );
	Q3Game_Init( 0 );
	Q3Game_Console( "addbot sarge 3" );
	Q3Game_Console( "addbot grunt 3" );
	for ( map = 2; map <= 4; map++ ) {
		Q3Game_Frames( 10 );
		Q3Game_SetMap( 20 * ( map - 1 ) );
		GameReload( 0 );
		Check( Q3Game_Reconnect( 1 ) && Q3Game_Reconnect( 2 ), "both bots reconnect to a bigger map" );
		drift += Q3Game_NumBots() != 2;
	}
	if ( resetOnLoad ) {
		Check( !drift, "NumBots() is 2 on every map" );
	}
	return drift;
}

/* A bot added after map_restart gets its own bot_state_t. */
static int BotAddedAfterMapRestart( void ) {
	int failures;

	FreshProcess( "addbot after map_restart" );
	GameServer( 1 );
	Load( GAME );
	Q3Game_Init( 0 );
	Q3Game_Console( "addbot sarge 3" );
	Q3Game_Console( "addbot grunt 3" );
	Q3Game_Frames( 10 );
	GameReload( 1 );
	failures = !Q3Game_Reconnect( 1 ) + !Q3Game_Reconnect( 2 );
	Q3Game_Console( "addbot major 3" );
	Q3Game_Console( "addbot doom 3" );
	failures += Q3Game_Drops() + Q3Game_SharedBotStates();
	if ( resetOnLoad ) {
		Check( failures == 0, "the bots reconnect and two more join after map_restart" );
		Check( Q3Game_NumBots() == 4, "NumBots() is 4" );
	}
	return failures;
}

/* g_banIPs and the ban list are what a fresh game makes of the cvar. */
static int IpBansAcrossMapChanges( void ) {
	size_t first;
	int map, stale;

	FreshProcess( "IP bans across map changes" );
	GameServer( 0 );
	Q3T_CvarSet( "g_banIPs", "10.0.0.1 10.0.0.2 " );
	Load( GAME );
	Q3Game_Init( 0 );
	first = strlen( Q3T_CvarGet( "g_banIPs" ) );
	for ( map = 2; map <= 6; map++ ) {
		GameReload( 0 );
	}
	stale = strlen( Q3T_CvarGet( "g_banIPs" ) ) != first;
	Q3Game_Console( "removeip 10.0.0.1" );
	stale += Q3Game_FilterPacket( "10.0.0.1" );
	if ( resetOnLoad ) {
		Check( strlen( Q3T_CvarGet( "g_banIPs" ) ) == strlen( "10.0.0.2 " ), "g_banIPs keeps each ban once" );
		Check( !Q3Game_FilterPacket( "10.0.0.1" ), "removeip unbans the address" );
		Check( Q3Game_FilterPacket( "10.0.0.2" ), "the other ban stays" );
	}
	return stale;
}

/* --- the cgame --- */

/* A vid_restart (or the next gamestate) frees the cgame and creates it again. */
static void CGameReload( void ) {
	Q3CGame_Shutdown();
	Unload( CGAME );
	Load( CGAME );
	Q3CGame_Init();
}

static long CGameInitMatchesFresh( void ) {
	unsigned char *fresh;
	long differ, first;

	FreshProcess( "cgame init after a reload" );
	Load( CGAME );
	Q3CGame_Init();
	fresh = Image( CGAME );
	Q3CGame_Frames( 30 );
	Q3CGame_Shutdown();
	Unload( CGAME );
	ResetEngine();
	Load( CGAME );
	Q3CGame_Init();
	differ = Differences( CGAME, fresh, &first );
	free( fresh );
	if ( resetOnLoad ) {
		CheckSame( CGAME, differ, first, "its first init" );
	}
	return differ;
}

/* The loading screen shows this load's icons only. */
static int LoadingIconsAcrossReloads( void ) {
	int fresh, load, stale = 0;

	FreshProcess( "cgame loading screen icons" );
	Load( CGAME );
	Q3CGame_Init();
	fresh = Q3CGame_LoadingIcons();
	Check( fresh > 0, "the first loading screen shows item and player icons" );
	for ( load = 2; load <= 3; load++ ) {
		CGameReload();
		stale += Q3CGame_LoadingIcons() != fresh;
	}
	if ( resetOnLoad ) {
		Check( stale == 0, "every loading screen shows the same icons as the first" );
	}
	return stale;
}

/* --- the ui --- */

/* A vid_restart or map load frees the ui and creates it again. */
static void UIReload( void ) {
	Q3UI_Shutdown();
	Unload( UI );
	Load( UI );
	Q3UI_Init();
}

static long UIInitMatchesFresh( void ) {
	unsigned char *fresh;
	long differ, first;

	FreshProcess( "ui init after a reload" );
	Load( UI );
	Q3UI_Init();
	fresh = Image( UI );
	Q3UI_MainMenu();
	Q3UI_Refresh( 3 );
	Q3UI_Mouse( 120, 80 );
	Q3UI_OpenBrowser();
	Q3UI_Refresh( 10 );
	Q3UI_Shutdown();
	Unload( UI );
	ResetEngine();
	Load( UI );
	Q3UI_Init();
	differ = Differences( UI, fresh, &first );
	free( fresh );
	if ( resetOnLoad ) {
		CheckSame( UI, differ, first, "its first init" );
	}
	return differ;
}

/* The server browser queries again after a reload; the cursor starts over. */
static int BrowserAndCursorAcrossReloads( void ) {
	int freshCursor, stale;

	FreshProcess( "ui server browser and cursor" );
	Load( UI );
	Q3UI_Init();
	Q3UI_MainMenu();
	Q3UI_Refresh( 1 );
	freshCursor = Q3UI_CursorX();
	Q3UI_Mouse( 200, 100 );
	Q3UI_OpenBrowser();
	Check( Q3UI_ServerQueries() == 1, "opening the browser queries the LAN" );
	Q3UI_Refresh( 10 );
	UIReload();
	Q3UI_MainMenu();
	Q3UI_Refresh( 1 );
	stale = Q3UI_CursorX() != freshCursor;
	Q3UI_OpenBrowser();
	stale += Q3UI_ServerQueries() != 2;
	if ( resetOnLoad ) {
		Check( Q3UI_ServerQueries() == 2, "the browser queries the LAN again after the ui reloads" );
		Check( Q3UI_CursorX() == freshCursor, "the cursor starts where it started in a fresh process" );
	}
	return stale;
}

int main( void ) {
	const char *error;
	int m, pass;
	long first;

	/* Sys_LoadDll's table, saved before any module code runs, as main() does */
	snprintf( currentCase, sizeof( currentCase ), "startup" );
	error = VM_InitStaticModules( modules, MODULES );
	if ( error ) {
		Q3T_Fail( "VM_InitStaticModules: %s", error );
	}
	for ( m = 0; m < MODULES; m++ ) {
		size_t i, bss = modules[m].bssEnd - modules[m].bssStart;
		Check( modules[m].bssEnd > modules[m].bssStart, "every module has a bss bracket" );
		for ( i = 0; i < bss; i++ ) {
			if ( modules[m].bssStart[i] ) {
				Q3T_Fail( "%s bss is not zero at startup", modules[m].name );
			}
		}
		startImages[m] = Image( m );
	}
	Check( Q3Game_InModule( 0 ) && Q3Game_InModule( 1 ), "level and g_gametype are inside the game brackets" );
	Check( !Q3Game_InModule( 2 ), "bg_itemlist (shared with cgame and ui) is outside the game brackets" );
	Check( Q3CGame_InModule(), "cg and cg_pmoveFixed are inside the cgame bss bracket" );
	Check( Q3UI_InModule(), "uis is inside the ui bss bracket" );
	Check( VM_FindStaticModule( modules, MODULES, "QAGAME" ) == &modules[GAME], "module names match without case" );
	Check( !VM_FindStaticModule( modules, MODULES, "botlib" ), "only the three modules are static modules" );
	error = VM_LoadStaticModule( &modules[UI] );
	Check( !error, "the ui loads" );
	Check( VM_LoadStaticModule( &modules[UI] ) != NULL, "a module that is still loaded is not reset" );
	VM_UnloadStaticModule( &modules[UI] );

	/* Without the reset every scenario must show the stale behaviour;
	   with it, every scenario must match a fresh process. */
	for ( pass = 0; pass < 2; pass++ ) {
		long gameMapChange, gameMapRestart, cgame, ui;
		int bots, numBots, botsRestart, bans, icons, browser;

		resetOnLoad = pass;
		gameMapChange = GameInitMatchesFresh( 0 );
		gameMapRestart = GameInitMatchesFresh( 1 );
		bots = BotsDroppedOnMapChange();
		numBots = NumBotsAcrossMapChanges();
		botsRestart = BotAddedAfterMapRestart();
		bans = IpBansAcrossMapChanges();
		cgame = CGameInitMatchesFresh();
		icons = LoadingIconsAcrossReloads();
		ui = UIInitMatchesFresh();
		browser = BrowserAndCursorAcrossReloads();
		if ( !resetOnLoad ) {
			printf( "without the reset: game init after a map change %ld and after map_restart %ld bytes "
				"off, %d bot drops, %d maps with NumBots() off, %d failed bot adds after map_restart, "
				"%d stale bans, cgame init %ld bytes off, %d stale loading screens, ui init %ld bytes "
				"off, %d stale browser/cursor\n", gameMapChange, gameMapRestart, bots, numBots,
				botsRestart, bans, cgame, icons, ui, browser );
			snprintf( currentCase, sizeof( currentCase ), "the scenarios without the reset" );
			Check( gameMapChange > 0, "the game init after a map change differs without the reset" );
			Check( gameMapRestart > 0, "the game init after map_restart differs without the reset" );
			Check( bots > 0, "bots are dropped on a map change without the reset" );
			Check( numBots > 0, "NumBots() grows across map changes without the reset" );
			Check( botsRestart > 0, "a bot added after map_restart fails without the reset" );
			Check( bans > 0, "the IP bans double without the reset" );
			Check( cgame > 0, "the cgame init after a reload differs without the reset" );
			Check( icons > 0, "the loading screen keeps old icons without the reset" );
			Check( ui > 0, "the ui init after a reload differs without the reset" );
			Check( browser > 0, "the browser and cursor keep their state without the reset" );
		}
	}

	/* After the reset each module is byte for byte as the process started. */
	resetOnLoad = 1;
	for ( m = 0; m < MODULES; m++ ) {
		long differ;
		snprintf( currentCase, sizeof( currentCase ), "%s memory after the reset", modules[m].name );
		Unload( m );
		Load( m );
		differ = Differences( m, startImages[m], &first );
		CheckSame( m, differ, first, "the process start" );
		free( startImages[m] );
	}

	printf( "static module reset regression passed (%d checks)\n", checks );
	return 0;
}
#endif
