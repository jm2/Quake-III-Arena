/* Issue #239: the native cgame must bound every server-supplied table index. */
#include "../code/cgame/cg_local.h"
#ifdef MISSIONPACK
#include "../ui/menudef.h"
#include "../code/ui/ui_shared.h"
#endif
#include <limits.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_VOICECHATBUFFER 32	/* private to cg_servercmds.c */

void dllEntry( int (QDECL *syscallptr)( int arg, ... ) );
#ifdef MISSIONPACK
extern displayContextDef_t cgDC;
void CG_LoadHudMenu( void );
#endif
extern localEntity_t cg_activeLocalEntities;

static jmp_buf errorJump;
static int expectError, errorCount;
static char errorText[1024];
static char commandText[MAX_STRING_CHARS];
static char *commandArgs[MAX_STRING_TOKENS];
static int commandArgc;
static snapshot_t serverSnap;
static const char *voiceFile;

/** Fail with the index contract that broke. */
static void Check( int ok, const char *message ) {
	if ( !ok ) { fprintf( stderr, "Cgame server index regression failed: %s\n", message ); exit( 1 ); }
}

/** Serve the client syscalls reached by the tested cgame paths. */
static int QDECL FakeSyscall( int command, ... ) {
	va_list ap;
	int result = 0;
	va_start( ap, command );
	switch ( command ) {
	case CG_ERROR:
		Q_strncpyz( errorText, va_arg( ap, const char * ), sizeof( errorText ) );
		va_end( ap );
		Check( expectError, errorText );
		errorCount++;
		longjmp( errorJump, 1 );
	case CG_ARGC:
		result = commandArgc;
		break;
	case CG_ARGV: {
		int n = va_arg( ap, int );
		char *buffer = va_arg( ap, char * );
		int length = va_arg( ap, int );
		Q_strncpyz( buffer, n >= 0 && n < commandArgc ? commandArgs[n] : "", length );
		break;
	}
	case CG_ARGS: {
		char *buffer = va_arg( ap, char * );
		buffer[0] = 0;
		break;
	}
	case CG_CVAR_VARIABLESTRINGBUFFER: {
		char *buffer;
		va_arg( ap, const char * );
		buffer = va_arg( ap, char * );
		buffer[0] = 0;
		break;
	}
	case CG_GETSERVERCOMMAND:
		result = 1;
		break;
	case CG_GETCURRENTSNAPSHOTNUMBER:
		*va_arg( ap, int * ) = 1;
		*va_arg( ap, int * ) = serverSnap.serverTime;
		break;
	case CG_GETSNAPSHOT:
		va_arg( ap, int );
		*va_arg( ap, snapshot_t * ) = serverSnap;
		result = 1;
		break;
	case CG_GETUSERCMD:
		va_arg( ap, int );
		memset( va_arg( ap, usercmd_t * ), 0, sizeof( usercmd_t ) );
		result = 1;
		break;
	case CG_FS_FOPENFILE: {
		const char *path = va_arg( ap, const char * );
		fileHandle_t *f = va_arg( ap, fileHandle_t * );
		*f = voiceFile && !strcmp( path, "scripts/female1.voice" );
		result = *f ? strlen( voiceFile ) : -1;
		if ( !strcmp( path, "ui/hud.txt" ) ) {
			*f = 2;		// an empty Team Arena HUD: no menus
			result = 0;
		}
		break;
	}
	case CG_FS_READ: {
		void *buffer = va_arg( ap, void * );
		int length = va_arg( ap, int );
		if ( length > 0 ) {
			memcpy( buffer, voiceFile, length );
		}
		break;
	}
	case CG_CM_BOXTRACE: case CG_CM_TRANSFORMEDBOXTRACE:
	case CG_CM_CAPSULETRACE: case CG_CM_TRANSFORMEDCAPSULETRACE: {
		trace_t *trace = va_arg( ap, trace_t * );
		memset( trace, 0, sizeof( *trace ) );
		trace->fraction = 1;
		trace->entityNum = ENTITYNUM_NONE;
		break;
	}
	case CG_R_LERPTAG: {
		orientation_t *tag = va_arg( ap, orientation_t * );
		memset( tag, 0, sizeof( *tag ) );
		AxisClear( tag->axis );
		result = 1;
		break;
	}
	case CG_R_MODELBOUNDS: {
		float *mins, *maxs;
		va_arg( ap, int );
		mins = va_arg( ap, float * );
		maxs = va_arg( ap, float * );
		VectorClear( mins );
		VectorClear( maxs );
		break;
	}
	case CG_R_REGISTERMODEL: case CG_R_REGISTERSKIN: case CG_R_REGISTERSHADER:
	case CG_R_REGISTERSHADERNOMIP: case CG_S_REGISTERSOUND:
		result = 1;
		break;
	}
	va_end( ap );
	return result;
}

/** Require one controlled CG_Error from a hostile index before any table access. */
#define EXPECT_REJECTED( call, what ) do { \
	int before = errorCount; \
	expectError = 1; \
	if ( setjmp( errorJump ) == 0 ) { call; } \
	expectError = 0; \
	Check( errorCount == before + 1, "not rejected: " what ); \
} while ( 0 )

/** Start each case from the zeroed state CG_Init leaves behind. */
static void Reset( void ) {
	memset( &cg, 0, sizeof( cg ) );
	memset( &cgs, 0, sizeof( cgs ) );
	memset( cg_entities, 0, sizeof( cg_entities ) );
	memset( cg_weapons, 0, sizeof( cg_weapons ) );
	memset( cg_items, 0, sizeof( cg_items ) );
	memset( &serverSnap, 0, sizeof( serverSnap ) );
	CG_InitLocalEntities();
	CG_InitMarkPolys();
	cg.time = 1000;
	serverSnap.ps.clientNum = 5;
	serverSnap.ps.stats[STAT_HEALTH] = 100;
}

/** Tokenize and execute one reliable server command through the real dispatcher. */
static void ServerCommand( const char *text ) {
	char *token;
	Q_strncpyz( commandText, text, sizeof( commandText ) );
	commandArgc = 0;
	for ( token = strtok( commandText, " " ); token; token = strtok( NULL, " " ) ) {
		commandArgs[commandArgc++] = token;
	}
	cgs.serverCommandSequence = 0;
	CG_ExecuteNewServerCommands( 1 );
}

/** Deliver serverSnap as the first snapshot through the real reader. */
static void DeliverSnapshot( void ) {
	cg.snap = cg.nextSnap = NULL;
	cg.latestSnapshotNum = cgs.processedSnapshotNum = 0;
	CG_ProcessSnapshots();
}

/** Mark a clientinfo slot as a drawable teammate. */
static void ValidClient( int clientNum ) {
	clientInfo_t *ci = &cgs.clientinfo[clientNum];
	ci->infoValid = qtrue;
	ci->team = TEAM_RED;
	ci->legsModel = ci->torsoModel = ci->headModel = 1;
}

/** Team overlay counts, client numbers, and weapons from "tinfo". */
static void TestTeamInfo( void ) {
	char text[MAX_STRING_CHARS];
	int i, saved[TEAM_MAXOVERLAY];

	Reset();
	ServerCommand( "tinfo 2 3 4 100 50 5 1 7 0 80 20 2 0" );
	Check( numSortedTeamPlayers == 2 && sortedTeamPlayers[0] == 3 && sortedTeamPlayers[1] == 7,
		"valid team overlay order" );
	Check( cgs.clientinfo[3].location == 4 && cgs.clientinfo[3].health == 100 &&
		cgs.clientinfo[3].armor == 50 && cgs.clientinfo[3].curWeapon == 5 &&
		cgs.clientinfo[3].powerups == 1 && cgs.clientinfo[7].curWeapon == 2, "valid team overlay fields" );

	Q_strncpyz( text, "tinfo 32", sizeof( text ) );
	for ( i = 0; i < TEAM_MAXOVERLAY; i++ ) {
		Q_strcat( text, sizeof( text ), va( " %i 0 100 0 %i 0", i, i % WP_NUM_WEAPONS ) );
	}
	ServerCommand( text );
	Check( numSortedTeamPlayers == TEAM_MAXOVERLAY && sortedTeamPlayers[31] == 31, "full team overlay" );
	memcpy( saved, sortedTeamPlayers, sizeof( saved ) );

	EXPECT_REJECTED( ServerCommand( "tinfo 33" ), "tinfo count past TEAM_MAXOVERLAY" );
	EXPECT_REJECTED( ServerCommand( "tinfo 2147483647" ), "tinfo count INT_MAX" );
	EXPECT_REJECTED( ServerCommand( "tinfo -1" ), "negative tinfo count" );
	EXPECT_REJECTED( ServerCommand( "tinfo 1 64 0 100 0 0 0" ), "tinfo client MAX_CLIENTS" );
	EXPECT_REJECTED( ServerCommand( "tinfo 1 -1 0 100 0 0 0" ), "negative tinfo client" );
	EXPECT_REJECTED( ServerCommand( "tinfo 2 3 0 1 1 1 0 1000000 0 1 1 1 0" ), "late tinfo client" );
	// entries before a bad client hold checked numbers; the count stays the old one
	Check( numSortedTeamPlayers == TEAM_MAXOVERLAY, "rejected tinfo published its count" );
	Check( !memcmp( saved + 1, sortedTeamPlayers + 1, sizeof( saved ) - sizeof( saved[0] ) ),
		"rejected tinfo changed later overlay slots" );

	ServerCommand( "tinfo 2 3 0 100 0 200 0 4 0 100 0 -1 0" );
	Check( cgs.clientinfo[3].curWeapon == WP_NONE && cgs.clientinfo[4].curWeapon == WP_NONE,
		"hostile tinfo weapon reaches the overlay" );
}

/** CG_RegisterWeapon must reject entity weapons before touching cg_weapons. */
static void TestRegisterWeapon( void ) {
	static weaponInfo_t saved[MAX_WEAPONS];

	Reset();
	CG_RegisterWeapon( WP_NONE );
	CG_RegisterWeapon( WP_MACHINEGUN );
	Check( cg_weapons[WP_MACHINEGUN].registered && cg_weapons[WP_MACHINEGUN].item, "valid weapon registration" );
	memcpy( saved, cg_weapons, sizeof( saved ) );
	EXPECT_REJECTED( CG_RegisterWeapon( 255 ), "CG_RegisterWeapon(255)" );
	EXPECT_REJECTED( CG_RegisterWeapon( MAX_WEAPONS ), "CG_RegisterWeapon(MAX_WEAPONS)" );
	EXPECT_REJECTED( CG_RegisterWeapon( -1 ), "CG_RegisterWeapon(-1)" );
	Check( !memcmp( saved, cg_weapons, sizeof( saved ) ), "rejected weapon changed cg_weapons" );
}

/** Playerstate indexes are checked when the snapshot is read. */
static void TestSnapshotPlayerState( void ) {
	Reset();
	serverSnap.ps.weapon = WP_MACHINEGUN;
	serverSnap.ps.loopSound = MAX_SOUNDS - 1;
	serverSnap.ps.stats[STAT_HOLDABLE_ITEM] = bg_numItems - 1;
	DeliverSnapshot();
	Check( cg.snap && cg.snap->ps.clientNum == 5 && cg.weaponSelect == WP_MACHINEGUN, "valid snapshot" );

	Reset(); serverSnap.ps.clientNum = MAX_CLIENTS;
	EXPECT_REJECTED( DeliverSnapshot(), "playerstate clientNum MAX_CLIENTS" );
	Reset(); serverSnap.ps.clientNum = 255;
	EXPECT_REJECTED( DeliverSnapshot(), "playerstate clientNum 255" );
	Reset(); serverSnap.ps.weapon = MAX_WEAPONS;
	EXPECT_REJECTED( DeliverSnapshot(), "playerstate weapon MAX_WEAPONS" );
	Reset(); serverSnap.ps.weapon = 31;
	EXPECT_REJECTED( DeliverSnapshot(), "playerstate weapon 31" );
	Reset(); serverSnap.ps.loopSound = MAX_SOUNDS;
	EXPECT_REJECTED( DeliverSnapshot(), "playerstate loopSound MAX_SOUNDS" );
	Reset(); serverSnap.ps.loopSound = 65535;
	EXPECT_REJECTED( DeliverSnapshot(), "playerstate loopSound 65535" );
	Reset(); serverSnap.ps.stats[STAT_HOLDABLE_ITEM] = bg_numItems;
	EXPECT_REJECTED( DeliverSnapshot(), "holdable item bg_numItems" );
	Reset(); serverSnap.ps.stats[STAT_HOLDABLE_ITEM] = -1;
	EXPECT_REJECTED( DeliverSnapshot(), "negative holdable item" );
#ifdef MISSIONPACK
	Reset(); serverSnap.ps.stats[STAT_PERSISTANT_POWERUP] = bg_numItems;
	EXPECT_REJECTED( DeliverSnapshot(), "persistant powerup bg_numItems" );
#endif
}

/** New player entities are reset before CG_Player validates their clientNum. */
static void TestPlayerEntityReset( void ) {
	Reset();
	serverSnap.numEntities = 1;
	serverSnap.entities[0].number = 9;
	serverSnap.entities[0].eType = ET_PLAYER;
	serverSnap.entities[0].clientNum = 3;
	VectorSet( serverSnap.entities[0].pos.trBase, 1, 2, 3 );
	DeliverSnapshot();
	Check( VectorCompare( cg_entities[9].rawOrigin, serverSnap.entities[0].pos.trBase ), "valid player reset" );

	Reset();
	serverSnap.numEntities = 1;
	serverSnap.entities[0].number = 9;
	serverSnap.entities[0].eType = ET_PLAYER;
	serverSnap.entities[0].clientNum = 200;
	EXPECT_REJECTED( DeliverSnapshot(), "player entity clientNum 200" );
}

/** EV_RAILTRAIL colours its impact with the clamped client, not es->clientNum. */
static void TestRailImpact( void ) {
	vec3_t position = { 32, 0, 0 };
	centity_t *cent = &cg_entities[20];
	localEntity_t *le;

	Reset();
	cg.snap = &serverSnap;
	cgs.media.ringFlashModel = 1;
	VectorSet( cgs.clientinfo[0].color1, 0.25f, 0.5f, 0.75f );
	cent->currentState.number = 20;
	cent->currentState.event = EV_RAILTRAIL;
	cent->currentState.clientNum = 200;
	VectorCopy( position, cent->currentState.pos.trBase );
	CG_EntityEvent( cent, position );
	le = cg_activeLocalEntities.next;
	Check( le != &cg_activeLocalEntities && VectorCompare( le->color, cgs.clientinfo[0].color1 ),
		"rail impact colour from clamped client" );
}

/** The announcer ring buffer must wrap its read index when it overflows. */
static void TestBufferedSounds( void ) {
	int i;

	Reset();
	for ( i = 1; i <= 3 * MAX_SOUNDBUFFER; i++ ) {
		CG_AddBufferedSound( i );
		Check( cg.soundBufferOut >= 0 && cg.soundBufferOut < MAX_SOUNDBUFFER, "announcer read index" );
		if ( i >= MAX_SOUNDBUFFER ) {
			Check( cg.soundBuffer[cg.soundBufferOut] == i - MAX_SOUNDBUFFER + 2, "announcer drops oldest" );
		}
	}
}

/** Hostile serverinfo cannot extend loops over cgs.clientinfo. */
static void TestServerInfo( void ) {
	const char *info[2] = { "\\sv_maxclients\\8\\mapname\\q3dm1", "\\sv_maxclients\\1000\\mapname\\q3dm1" };
	int i;

	for ( i = 0; i < 2; i++ ) {
		Reset();
		cgs.gameState.stringOffsets[CS_SERVERINFO] = 1;
		Q_strncpyz( cgs.gameState.stringData + 1, info[i], sizeof( cgs.gameState.stringData ) - 1 );
		cgs.gameState.dataCount = strlen( info[i] ) + 2;
		CG_ParseServerinfo();
		CG_LoadDeferredPlayers();
		Check( cgs.maxclients == ( i ? MAX_CLIENTS : 8 ), "sv_maxclients bound" );
	}
}

/** Score plums draw at most ten digit slots, whatever the server sends. */
static void TestScorePlum( void ) {
	const int scores[4] = { 42, -1000000000, INT_MIN, INT_MAX };
	const int drawn[4] = { 42, -9999999, -9999999, 9999999 };
	vec3_t origin = { 1000, 0, 0 };
	int i;

	for ( i = 0; i < 4; i++ ) {
		Reset();
		cg_scorePlum.integer = 1;
		cg.predictedPlayerState.clientNum = 5;
		CG_ScorePlum( 5, origin, scores[i] );
		CG_AddLocalEntities();
		Check( cg_activeLocalEntities.next->leType == LE_SCOREPLUM &&
			cg_activeLocalEntities.next->radius == drawn[i], "score plum value" );
	}
}

#ifdef MISSIONPACK
/** Voice chats: the buffered read index wraps, and hostile client numbers clamp. */
static void TestVoiceChatBuffer( void ) {
	int i;

	Reset();
	cg.snap = &serverSnap;
	voiceFile = "female\nhello { sound/hello.wav \"hi\" }\n";
	CG_LoadVoiceChats();
	for ( i = 0; i < 4 * MAX_VOICECHATBUFFER; i++ ) {
		ServerCommand( i & 1 ? "vchat 0 3 1 hello" : "vtchat 0 255 1 hello" );
		Check( cg.voiceChatBufferOut >= 0 && cg.voiceChatBufferOut < MAX_VOICECHATBUFFER,
			"voice chat read index" );
	}
	cg.voiceChatTime = 0;
	CG_PlayBufferedVoiceChats();
	Check( cg.voiceChatBufferOut >= 0 && cg.voiceChatBufferOut < MAX_VOICECHATBUFFER,
		"played voice chat index" );
}

/** Corpses and hostile ET_PLAYER numbers must not index per-client arrays. */
static void TestPlayerNumbers( void ) {
	centity_t *cent;

	Reset();
	cg.snap = &serverSnap;
	cgs.gametype = GT_HARVESTER;
	cg_enableBreath.integer = 1;
	ValidClient( 3 );
	cent = &cg_entities[3];
	cent->currentState.number = 3;
	cent->currentState.clientNum = 3;
	cent->currentState.eType = ET_PLAYER;
	cent->currentState.generic1 = 2;
	// one smoke puff per run: Q_rand's signed LCG overflows UBSan on the second
	cgs.clientinfo[3].breathPuffTime = cg.time + 1;
	CG_Player( cent );
	Check( cg.skulltrails[3].numpositions == 2, "valid player tokens" );

	cgs.clientinfo[3].breathPuffTime = 0;
	cent = &cg_entities[100];
	cent->currentState = cg_entities[3].currentState;
	cent->currentState.number = 100;
	cent->currentState.generic1 = 0;
	CG_Player( cent );
	Check( cg.skulltrails[3].numpositions == 2, "corpse tokens changed a client trail" );
	Check( cgs.clientinfo[3].breathPuffTime == cg.time + 2000, "breath puff uses clientNum" );
}

/** Team Arena HUD owner draws with a hostile weapon on our entity slot. */
static void TestOwnerDraws( void ) {
	vec4_t color = { 1, 1, 1, 1 };

	Reset();
	cg.snap = &serverSnap;
	cg_drawStatus.integer = 1;
	cg_draw3dIcons.integer = 1;
	serverSnap.ps.ammo[WP_MACHINEGUN] = 50;
	cg_entities[5].currentState.weapon = WP_MACHINEGUN;
	Check( CG_GetValue( CG_PLAYER_AMMO_VALUE ) == 50, "valid ammo value" );

	cg_entities[5].currentState.weapon = 200;
	Check( CG_GetValue( CG_PLAYER_AMMO_VALUE ) == -1, "hostile ammo value" );
	CG_OwnerDraw( 0, 0, 10, 10, 0, 0, CG_PLAYER_AMMO_ICON, 0, 0, 0, 1, color, 0, 0 );
	CG_OwnerDraw( 0, 0, 10, 10, 0, 0, CG_PLAYER_AMMO_VALUE, 0, 0, 0, 1, color, 1, 0 );

	ValidClient( 3 );
	ServerCommand( "tinfo 1 3 0 100 0 200 0" );
	CG_OwnerDraw( 0, 0, 10, 10, 0, 0, CG_SELECTEDPLAYER_WEAPON, 0, 0, 0, 1, color, 0, 0 );
}

/** Issue #40: the scoreboard list rows stay inside cg.scores whatever count "scores" sends. */
static void TestScoreList( void ) {
	static const char *counts[] = { "0", "-1", "-2147483647", "-2147483648", NULL };
	static menuDef_t menu;
	static itemDef_t item;
	static listBoxDef_t list;
	vec4_t color = { 1, 1, 1, 1 };
	qhandle_t handle;
	int i, column;

	Reset();
	cg.snap = &serverSnap;
	cgs.gametype = GT_FFA;
	CG_LoadHudMenu();	// the scoreboard feeders
	memset( &menu, 0, sizeof( menu ) );
	memset( &item, 0, sizeof( item ) );
	memset( &list, 0, sizeof( list ) );
	menu.itemCount = 1;
	menu.items[0] = &item;
	item.parent = &menu;
	item.type = ITEM_TYPE_LISTBOX;
	item.special = FEEDER_SCOREBOARD;
	item.typeData = &list;
	item.window.rect.w = item.window.rect.h = 100;
	list.elementWidth = list.elementHeight = 10;

	// two real rows: scrolling down selects the second, and its text is read
	ValidClient( 7 );
	ServerCommand( "scores 2 0 0  5 10 50 100 0 0 90 0 0 0 0 0 0 0  7 20 60 100 0 0 80 0 0 0 0 0 0 0" );
	Check( cg.numScores == 2 && cg.scores[1].client == 7, "valid scores" );
	Menu_ScrollFeeder( &menu, FEEDER_SCOREBOARD, qtrue );
	Check( cg.selectedScore == 1, "scrolled to the second score" );
	Check( !strcmp( cgDC.feederItemText( FEEDER_SCOREBOARD, 1, 4, &handle ), "20" ), "second score text" );
	CG_OwnerDraw( 0, 0, 10, 10, 0, 0, CG_ACCURACY, 0, 0, 0, 1, color, 0, 0 );

	for ( i = 0; counts[i]; i++ ) {
		ServerCommand( va( "scores %s 0 0", counts[i] ) );
		Check( cg.numScores == 0, "hostile score count" );
		// a selectable list moves its cursor to count - 1
		list.notselectable = qfalse;
		list.cursorPos = list.startPos = 0;
		Menu_ScrollFeeder( &menu, FEEDER_SCOREBOARD, qtrue );
		Check( cg.selectedScore >= 0 && cg.selectedScore < MAX_CLIENTS, "selected score row" );
		CG_OwnerDraw( 0, 0, 10, 10, 0, 0, CG_ACCURACY, 0, 0, 0, 1, color, 0, 0 );
		// a list that is not selectable scrolls its first row to count - 1
		list.notselectable = qtrue;
		list.cursorPos = list.startPos = 0;
		Menu_ScrollFeeder( &menu, FEEDER_SCOREBOARD, qtrue );
		for ( column = 0; column < 7; column++ ) {
			Check( !strcmp( cgDC.feederItemText( FEEDER_SCOREBOARD, list.startPos, column, &handle ), "" ),
				"row outside the scores reads as empty" );
		}
	}
}
#else
/** The status bar and team overlay with a hostile weapon on our entity slot. */
static void TestStatusBar( void ) {
	Reset();
	cg_draw2D.integer = 1;
	cg_drawStatus.integer = 1;
	cg_drawTeamOverlay.integer = 1;
	cgs.gametype = GT_TEAM;
	serverSnap.ps.persistant[PERS_TEAM] = TEAM_RED;
	serverSnap.numEntities = 1;
	serverSnap.entities[0].number = 5;
	serverSnap.entities[0].weapon = 200;
	DeliverSnapshot();
	Check( cg_entities[5].currentState.weapon == 200, "packet entity replaced our weapon" );
	ValidClient( 3 );
	ServerCommand( "tinfo 1 3 0 100 0 200 0" );
	CG_DrawActive( STEREO_CENTER );
}
#endif

/** Run every hostile index through the real cgame code. */
int main( void ) {
	dllEntry( FakeSyscall );
	TestTeamInfo();
	TestRegisterWeapon();
	TestSnapshotPlayerState();
	TestPlayerEntityReset();
	TestRailImpact();
	TestBufferedSounds();
	TestServerInfo();
	TestScorePlum();
#ifdef MISSIONPACK
	TestVoiceChatBuffer();
	TestPlayerNumbers();
	TestOwnerDraws();
	TestScoreList();
	puts( "Team Arena cgame server index regressions passed (issue #239)" );
#else
	TestStatusBar();
	puts( "Cgame server index regressions passed (issue #239)" );
#endif
	return 0;
}
