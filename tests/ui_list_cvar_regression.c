/* Issue #389: the Team Arena UI's list selections are archived cvars that a
 * config, the console or (for a cvar nothing registered yet) a server can set
 * to anything, and the menus index their lists with them:
 * - ui_gameType, ui_netGameType and ui_joinGameType select a game type, and
 *   ui_currentMap and ui_currentNetMap a map, from the lists gameinfo.txt
 *   loads; ui_netSource and ui_serverFilterType select from static tables;
 * - ui_currentTier, g_spSkill, the ui_blueteamN/ui_redteamN slots,
 *   cg_drawCrosshair and ui_mapIndex are read back as floats, whose
 *   conversion to int is undefined past the int range;
 * - the postgame command looks up the map's time to beat for the game type
 *   the server names, and multiplies the score by g_spSkill;
 * - gameinfo.txt, which any pk3 can supply, names each game type's number,
 *   which indexes the maps' times to beat and shifts their game type bits,
 *   and each map's team size, which counts the team's players.
 * Each value reaches the UI through the real CL_SystemInfoChanged and cvar.c,
 * and every consumer that reads it runs: _UI_Init, the owner draws and their
 * widths, the visibility flags, the items' keys, the feeders and the menu
 * scripts. A value that names an entry must act as it always did; any other
 * must act as the entry the menu shows for it, and read nothing past a list.
 * The fixture includes the real ui_main.c and links the rest of the UI behind
 * the real syscall layer; the fake engine below answers as cl_ui.c does, from
 * a small install: the gameinfo.txt and teaminfo.txt below, and a server list.
 * Font glyph n draws with shader GLYPH + n, so the fake renderer reads back
 * the text an item draws. */
#include "../code/ui/ui_main.c"
#include "systeminfo_cvar_harness.h"
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void dllEntry( int (QDECL *syscallptr)( int arg, ... ) );	/* ui_syscalls.c */

#define ARRAY_LEN( a ) ( (int)( sizeof( a ) / sizeof( ( a )[0] ) ) )
#define GLYPH 0x10000	/* the shader of font glyph n is GLYPH + n */
#define CINEMATIC 0x2000	/* the cinematic handles the fake engine hands out */
#define PLAYING 0x3000	/* the maps' cinematics, PLAYING + map, before a stop */
#define SCALE 0.3f	/* between ui_smallFont and ui_bigFont */
#define START_TIME 10	/* the postgame match time, seconds */
#define BASE_SCORE "100"	/* the postgame base score */

/* gameinfo.txt: Team Arena's game types, and four skirmish maps with their
 * times to beat per game type (0 for none); the last has no single player.
 * The gtEnum and teamMembers cases change a number in it. */
static struct {
	const char *name;
	int gt;
} gameTypes[] = {
	{ "Free For All", GT_FFA }, { "Tournament", GT_TOURNAMENT }, { "Single Player", GT_SINGLE_PLAYER },
	{ "Team Deathmatch", GT_TEAM }, { "Capture the Flag", GT_CTF }, { "One Flag CTF", GT_1FCTF },
	{ "Overload", GT_OBELISK }, { "Harvester", GT_HARVESTER },
}, joinGameTypes[] = {
	{ "All", -1 }, { "Free For All", GT_FFA }, { "Tournament", GT_TOURNAMENT }, { "Team Deathmatch", GT_TEAM },
	{ "Capture the Flag", GT_CTF }, { "One Flag CTF", GT_1FCTF }, { "Overload", GT_OBELISK }, { "Harvester", GT_HARVESTER },
};
static struct {
	const char *name, *loadName, *opponent;
	int teamMembers;
	int times[GT_MAX_GAME_TYPE];
} maps[] = {
	{ "Zero Map", "mp_zero", "Grunt", 2, { 300, 310, 320, 330, 340, 0, 0, 0 } },
	{ "One Map", "mp_one", "Major", 3, { 400, 0, 420, 0, 440, 450, 0, 0 } },
	{ "Two Map", "mp_two", "Visor", 1, { 0, 510, 520, 530, 0, 0, 560, 570 } },
	{ "Three Map", "mp_three", "Grunt", 2, { 600, 610, 0, 630, 0, 0, 0, 670 } },
};
/* teaminfo.txt: the default Pagans and Stroggs teams, and three characters
 * with an alias each */
static const char teaminfoText[] =
	"teams {\n"
	"{ \"Pagans\" \"ui/assets/pagans\" \"Grunt\" \"Major\" \"Visor\" \"Grunt\" \"Major\" }\n"
	"{ \"Stroggs\" \"ui/assets/stroggs\" \"Visor\" \"Major\" \"Grunt\" \"Visor\" \"Major\" }\n"
	"}\n"
	"characters {\n{ \"Grunt\" male }\n{ \"Major\" female }\n{ \"Visor\" male }\n}\n"
	"aliases {\n{ \"Grunt\" \"Grunt\" \"d\" }\n{ \"Major\" \"Major\" \"o\" }\n{ \"Visor\" \"Visor\" \"d\" }\n}\n";
static const char *characters[] = { "Grunt", "Major", "Visor" };
/* the servers every source lists: their game types and mods */
static const struct {
	int gt;
	const char *game;
} servers[] = {
	{ GT_FFA, "" }, { GT_TOURNAMENT, "missionpack" }, { GT_TEAM, "arena" }, { GT_CTF, "osp" },
	{ GT_1FCTF, "missionpack" }, { GT_OBELISK, "" }, { GT_HARVESTER, "wfa" }, { GT_TEAM, "alliance20" },
};

static const char *cvar, *value;
static char install[64];	/* the case's change to gameinfo.txt */
static char gameinfoText[4096];
static qboolean noGameInfo;	/* the install has no gameinfo.txt yet */
static char serverInfo[MAX_INFO_STRING];	/* CS_SERVERINFO */
static const char *args[16];	/* the console command's arguments */
static int argCount;
static char drawnText[MAX_STRING_CHARS];	/* the glyphs drawn */
static qhandle_t drawnShader;	/* the last shader drawn */
static qhandle_t lastShader;
static char executed[MAX_STRING_CHARS];	/* command text */
static char opened[MAX_STRING_CHARS];	/* files opened for reading, one per line */
static char playedCinematic[MAX_QPATH];
static int stoppedCinematic;

/** Fail with the cvar and value under test and the contract that broke. */
static void Check( int ok, const char *what ) {
	if ( !ok ) {
		fprintf( stderr, "UI list cvar regression failed (%s %s%s): %s\n", cvar, value, install, what );
		exit( 1 );
	}
}

/** The fixture's own va, so an expectation never shares the UI's buffers. */
static const char *Format( const char *fmt, ... ) __attribute__(( format( printf, 1, 2 ) ));
static const char *Format( const char *fmt, ... ) {
	static char buffers[8][MAX_STRING_CHARS];
	static int index;
	char *buffer = buffers[index++ & 7];
	va_list ap;

	va_start( ap, fmt );
	vsnprintf( buffer, sizeof( buffers[0] ), fmt, ap );
	va_end( ap );
	return buffer;
}

/** The install's files. */
static const char *InstallFile( const char *name ) {
	if ( !Q_stricmp( name, "gameinfo.txt" ) ) {
		return noGameInfo ? NULL : gameinfoText;
	}
	if ( !Q_stricmp( name, "teaminfo.txt" ) ) {
		return teaminfoText;
	}
	return NULL;
}

/** cl_ui.c's syscalls on the paths under test; cvars go to cvar.c. */
static int QDECL FakeSyscall( int command, ... ) {
	va_list ap;
	int result = 0;

	va_start( ap, command );
	switch ( command ) {
	case UI_ERROR:
		Check( 0, va_arg( ap, const char * ) );
		break;
	case UI_PRINT:
	case UI_MILLISECONDS:
	case UI_R_SETCOLOR:
	case UI_R_REGISTERMODEL:	/* no player models */
	case UI_R_REGISTERSKIN:
	case UI_KEY_SETCATCHER:
	case UI_KEY_CLEARSTATES:
	case UI_FS_GETFILELIST:	/* no .team, .arena or games files */
	case UI_FS_FCLOSEFILE:
	case UI_PC_READ_TOKEN:	/* empty menu scripts */
	case UI_PC_FREE_SOURCE:
	case UI_CIN_RUNCINEMATIC:
	case UI_CIN_SETEXTENTS:
	case UI_CIN_DRAWCINEMATIC:
	case UI_LAN_LOADCACHEDSERVERS:
	case UI_LAN_MARKSERVERVISIBLE:
	case UI_LAN_RESETPINGS:
		break;
	case UI_CVAR_REGISTER: {
		vmCvar_t *vmCvar = va_arg( ap, vmCvar_t * );
		const char *name = va_arg( ap, const char * );
		const char *defaultValue = va_arg( ap, const char * );
		Cvar_Register( vmCvar, name, defaultValue, va_arg( ap, int ) );
		break;
	}
	case UI_CVAR_UPDATE:
		Cvar_Update( va_arg( ap, vmCvar_t * ) );
		break;
	case UI_CVAR_SET: {
		const char *name = va_arg( ap, const char * );
		Cvar_Set( name, va_arg( ap, const char * ) );
		break;
	}
	case UI_CVAR_SETVALUE: {
		const char *name = va_arg( ap, const char * );
		int bits = va_arg( ap, int );	// PASSFLOAT
		float f;
		memcpy( &f, &bits, sizeof( f ) );
		Cvar_SetValue( name, f );
		break;
	}
	case UI_CVAR_VARIABLEVALUE: {
		float f = Cvar_VariableValue( va_arg( ap, const char * ) );
		memcpy( &result, &f, sizeof( result ) );	// FloatAsInt
		break;
	}
	case UI_CVAR_VARIABLESTRINGBUFFER: {
		const char *name = va_arg( ap, const char * );
		char *buffer = va_arg( ap, char * );
		Cvar_VariableStringBuffer( name, buffer, va_arg( ap, int ) );
		break;
	}
	case UI_ARGC:
		result = argCount;
		break;
	case UI_ARGV: {
		int n = va_arg( ap, int );
		char *buffer = va_arg( ap, char * );
		Q_strncpyz( buffer, n >= 0 && n < argCount ? args[n] : "", va_arg( ap, int ) );
		break;
	}
	case UI_CMD_EXECUTETEXT:
		va_arg( ap, int );	// EXEC_APPEND, or EXEC_NOW for a server refresh
		Q_strcat( executed, sizeof( executed ), va_arg( ap, const char * ) );
		break;
	case UI_FS_FOPENFILE: {
		const char *name = va_arg( ap, const char * );
		fileHandle_t *f = va_arg( ap, fileHandle_t * );
		const char *text = NULL;
		if ( va_arg( ap, int ) == FS_READ ) {
			Q_strcat( opened, sizeof( opened ), name );
			Q_strcat( opened, sizeof( opened ), "\n" );
			text = InstallFile( name );
		}
		if ( f ) {
			*f = text ? ( text == gameinfoText ? 1 : 2 ) : 0;
		}
		result = text ? (int)strlen( text ) : -1;
		break;
	}
	case UI_FS_READ: {
		char *buffer = va_arg( ap, char * );
		int len = va_arg( ap, int );
		const char *text = va_arg( ap, fileHandle_t ) == 1 ? gameinfoText : teaminfoText;
		Check( len == (int)strlen( text ), "install files are read whole" );
		memcpy( buffer, text, len );
		break;
	}
	case UI_PC_LOAD_SOURCE:
		result = 1;
		break;
	case UI_R_REGISTERSHADERNOMIP:
		result = ++lastShader;
		break;
	case UI_R_DRAWSTRETCHPIC: {
		int i;
		size_t len = strlen( drawnText );
		for ( i = 0; i < 8; i++ ) {
			va_arg( ap, int );	// PASSFLOAT x, y, w, h, s1, t1, s2, t2
		}
		drawnShader = va_arg( ap, qhandle_t );
		if ( drawnShader >= GLYPH && drawnShader < GLYPH + 256 && len + 1 < sizeof( drawnText ) ) {
			drawnText[len] = (char)( drawnShader - GLYPH );
			drawnText[len + 1] = '\0';
		}
		break;
	}
	case UI_S_REGISTERSOUND:
		result = 1;
		break;
	case UI_KEY_GETBINDINGBUF: {
		char *buffer;
		va_arg( ap, int );
		buffer = va_arg( ap, char * );
		if ( va_arg( ap, int ) > 0 ) {
			buffer[0] = 0;
		}
		break;
	}
	case UI_KEY_GETCATCHER:
		result = KEYCATCH_UI;
		break;
	case UI_GETCLIENTSTATE: {
		uiClientState_t *state = va_arg( ap, uiClientState_t * );
		memset( state, 0, sizeof( *state ) );
		state->connState = CA_ACTIVE;
		break;
	}
	case UI_GETCONFIGSTRING: {
		int index = va_arg( ap, int );
		char *buffer = va_arg( ap, char * );
		Q_strncpyz( buffer, index == CS_SERVERINFO ? serverInfo : "", va_arg( ap, int ) );
		result = 1;
		break;
	}
	case UI_GETGLCONFIG: {
		glconfig_t *config = va_arg( ap, glconfig_t * );
		memset( config, 0, sizeof( *config ) );
		config->vidWidth = 640;
		config->vidHeight = 480;
		break;
	}
	case UI_REAL_TIME:
		memset( va_arg( ap, qtime_t * ), 0, sizeof( qtime_t ) );
		break;
	case UI_CIN_PLAYCINEMATIC:
		Q_strncpyz( playedCinematic, va_arg( ap, const char * ), sizeof( playedCinematic ) );
		result = CINEMATIC;
		break;
	case UI_CIN_STOPCINEMATIC:
		stoppedCinematic = va_arg( ap, int );
		break;
	case UI_LAN_GETSERVERCOUNT:
		result = ARRAY_LEN( servers );
		break;
	case UI_LAN_SERVERISVISIBLE:
		result = 1;
		break;
	case UI_LAN_GETSERVERPING:
		result = 50;
		break;
	case UI_LAN_GETSERVERINFO: {
		int n;
		char *buf;
		va_arg( ap, int );
		n = va_arg( ap, int );
		buf = va_arg( ap, char * );
		Check( n >= 0 && n < ARRAY_LEN( servers ), "the engine is asked about a listed server" );
		Com_sprintf( buf, va_arg( ap, int ), "\\hostname\\Server %d\\mapname\\mp_zero\\clients\\1\\sv_maxclients\\8\\gametype\\%d\\game\\%s",
			n, servers[n].gt, servers[n].game );
		break;
	}
	case UI_LAN_COMPARESERVERS: {
		int s1, s2;
		va_arg( ap, int );	// source, sort key and direction
		va_arg( ap, int );
		va_arg( ap, int );
		s1 = va_arg( ap, int );
		s2 = va_arg( ap, int );
		result = s1 < s2 ? -1 : s1 > s2;
		break;
	}
	default:
		Check( 0, Format( "unexpected UI syscall %d", command ) );
	}
	va_end( ap );
	return result;
}

/** Start the UI as the client does, without _UI_Init's start-up log on stdout,
 * then give its fonts glyphs the fake renderer can read back. */
static void Init( void ) {
	fontInfo_t *fonts[] = { &uiInfo.uiDC.Assets.textFont, &uiInfo.uiDC.Assets.smallFont, &uiInfo.uiDC.Assets.bigFont };
	int saved, null, i, c;

	fflush( stdout );
	saved = dup( STDOUT_FILENO );
	null = open( "/dev/null", O_WRONLY );
	Check( saved >= 0 && null >= 0 && dup2( null, STDOUT_FILENO ) >= 0, "stdout redirected" );
	close( null );
	opened[0] = '\0';
	_UI_Init( qfalse );
	fflush( stdout );
	Check( dup2( saved, STDOUT_FILENO ) >= 0, "stdout restored" );
	close( saved );

	Check( uiInfo.numGameTypes == ( noGameInfo ? 0 : ARRAY_LEN( gameTypes ) ), "gameinfo.txt game types" );
	Check( uiInfo.numJoinGameTypes == ( noGameInfo ? 0 : ARRAY_LEN( joinGameTypes ) ), "gameinfo.txt browser game types" );
	Check( uiInfo.mapCount == ( noGameInfo ? 0 : ARRAY_LEN( maps ) ), "gameinfo.txt maps" );
	Check( uiInfo.characterCount == ARRAY_LEN( characters ), "teaminfo.txt characters" );
	for ( i = 0; i < ARRAY_LEN( fonts ); i++ ) {
		fonts[i]->glyphScale = 1;
		for ( c = 0; c < GLYPHS_PER_FONT; c++ ) {
			fonts[i]->glyphs[c].glyph = GLYPH + c;
			fonts[i]->glyphs[c].xSkip = c % 7 + 1;	// so a width tells texts apart
		}
	}
}

/** Set the cvar under test as the player's console or a config would (a
 * server's systeminfo only reaches it before the UI registers it), then let
 * the UI pick it up as a frame does. Nothing registers ui_serverFilterType,
 * as in retail, so only the filter item sets it, in the vmCvar alone. */
static void Set( const char *name, const char *v ) {
	if ( !Q_stricmp( name, "ui_serverFilterType" ) ) {
		ui_serverFilterType.integer = (int)strtol( v, NULL, 10 );
		return;
	}
	SystemInfo_Set( name, v );
	UI_UpdateCvars();
}

/** The value of a cvar, as a string. */
static const char *CvarString( const char *name ) {
	static char buffer[MAX_CVAR_VALUE_STRING];

	Cvar_VariableStringBuffer( name, buffer, sizeof( buffer ) );
	return buffer;
}

/** The text and last shader an owner draw item draws. */
static const char *Draw( int ownerDraw ) {
	drawnText[0] = '\0';
	drawnShader = 0;
	UI_OwnerDraw( 0, 20, 100, 20, 0, 0, ownerDraw, 0, 0, 0, SCALE, colorWhite, 0, 0 );
	return drawnText;
}

/** The value under test, set afresh, then the item drawn. */
static void CheckDraw( int ownerDraw, const char *expected, const char *what ) {
	int ok;

	Set( cvar, value );
	ok = !strcmp( Draw( ownerDraw ), expected );
	Check( ok, Format( "%s draws \"%s\", not \"%s\"", what, expected, drawnText ) );
}

/** An item draws the same text and shader for the value under test as for 0:
 * the tier items, since there are no tiers. */
static void CheckSameDraw( int ownerDraw, const char *what ) {
	char text[MAX_STRING_CHARS];
	qhandle_t shader;

	Set( cvar, "0" );
	Q_strncpyz( text, Draw( ownerDraw ), sizeof( text ) );
	shader = drawnShader;
	CheckDraw( ownerDraw, text, what );
	Check( drawnShader == shader, Format( "%s draws the first tier's shader", what ) );
}

/** The value under test, set afresh, then the item's width. */
static void CheckWidth( int ownerDraw, const char *expected, const char *what ) {
	int ok;

	Set( cvar, value );
	ok = UI_OwnerDrawWidth( ownerDraw, SCALE ) == Text_Width( expected, SCALE, 0 );
	Check( ok, Format( "%s is as wide as \"%s\"", what, expected ) );
}

/** The value under test, set afresh, then a key on the item. */
static void Key( int ownerDraw, int key ) {
	Set( cvar, value );
	executed[0] = opened[0] = '\0';
	UI_OwnerDrawHandleKey( ownerDraw, 0, NULL, key );
}

/** The value under test, set afresh, then one menu script. */
static void RunScript( const char *script ) {
	char buffer[MAX_STRING_CHARS];
	char *p = buffer;

	Set( cvar, value );
	executed[0] = opened[0] = '\0';
	Q_strncpyz( buffer, script, sizeof( buffer ) );
	UI_RunMenuScript( &p );
}

/** Every map's cinematic playing, so a stop names the map it stops. */
static void PlayAll( void ) {
	int i;

	for ( i = 0; i < uiInfo.mapCount; i++ ) {
		uiInfo.mapList[i].cinematic = PLAYING + i;
	}
	stoppedCinematic = -1;
}

/** Game type e after a key on the skirmish game type item, which skips
 * Single Player (2) and never selects Free For All (0). */
static int StepGameType( int e, int key ) {
	if ( key == K_MOUSE2 ) {
		e--;
		return e == 2 ? 1 : e < 2 ? ARRAY_LEN( gameTypes ) - 1 : e;
	}
	e++;
	return e >= ARRAY_LEN( gameTypes ) ? 1 : e == 2 ? 3 : e;
}

/** An item that steps through count entries from e. */
static int Step( int e, int count, int key ) {
	return ( e + ( key == K_MOUSE2 ? count - 1 : 1 ) ) % count;
}

/** The capture limit a skirmish of game type gt plays to. */
static int CaptureLimit( int gt ) {
	return gt == GT_OBELISK ? 4 : gt == GT_HARVESTER ? 15 : 5;
}

/** Whether map m is listed for game type gt, in the skirmish or create server
 * list (UI_MapCountByGameType). */
static qboolean MapListed( int m, int gt, qboolean singlePlayer ) {
	if ( gt == GT_SINGLE_PLAYER ) {
		gt++;
	}
	if ( gt == GT_TEAM ) {
		gt = GT_FFA;
	}
	if ( gt < 0 || gt >= GT_MAX_GAME_TYPE ) {
		return qfalse;	// no map has a time for it
	}
	return maps[m].times[gt] && ( !singlePlayer || maps[m].times[GT_SINGLE_PLAYER] );
}

static int MapsListed( int gt, qboolean singlePlayer ) {
	int m, count = 0;

	for ( m = 0; m < ARRAY_LEN( maps ); m++ ) {
		count += MapListed( m, gt, singlePlayer );
	}
	return count;
}

/** The first map listed for game type gt, or map 0 for none. */
static int FirstMap( int gt, qboolean singlePlayer ) {
	int m;

	for ( m = 0; m < ARRAY_LEN( maps ); m++ ) {
		if ( MapListed( m, gt, singlePlayer ) ) {
			return m;
		}
	}
	return 0;
}

/** The servers the browser lists for join game type e and filter f. */
static int ServersListed( int e, int f ) {
	int n, count = 0;

	for ( n = 0; n < ARRAY_LEN( servers ); n++ ) {
		if ( ( joinGameTypes[e].gt == -1 || servers[n].gt == joinGameTypes[e].gt )
			&& ( f == 0 || !Q_stricmp( servers[n].game, serverFilters[f].basedir ) ) ) {
			count++;
		}
	}
	return count;
}

/** The best scores UI_LoadBestScores looked up. */
static void CheckScores( int m, int gt, const char *what ) {
	Check( strstr( opened, Format( "games/%s_%d.game\n", maps[m].loadName, gt ) ) != NULL,
		Format( "%s loads %s's best scores for game type %d", what, maps[m].loadName, gt ) );
}

/** The commands start with the map. */
static void CheckMapStarted( int m, const char *what ) {
	const char *start = Format( "wait ; wait ; map %s\n", maps[m].loadName );

	Check( !strncmp( executed, start, strlen( start ) ), Format( "%s starts %s", what, maps[m].loadName ) );
}

/** The postgame command for a match of START_TIME on the server's map and
 * game type, which beats a time to beat t by t - START_TIME seconds. */
static void CheckPostgame( int t ) {
	static char matchTime[16];
	int i;

	Set( "ui_matchStartTime", "0" );
	argCount = 15;
	args[0] = "postgame";
	for ( i = 1; i < argCount; i++ ) {
		args[i] = "0";
	}
	args[9] = BASE_SCORE;
	Com_sprintf( matchTime, sizeof( matchTime ), "%d", START_TIME * 1000 );
	args[13] = matchTime;
	Check( UI_ConsoleCommand( 0 ), "postgame" );
	Check( atoi( CvarString( "ui_scoreTimeBonus" ) ) == ( t > START_TIME ? ( t - START_TIME ) * 10 : 0 ), "postgame time bonus" );
}

/** The time to beat an item draws for map m and game type gt. */
static const char *TimeToBeat( int m, int gt ) {
	int t = gt >= 0 && gt < GT_MAX_GAME_TYPE ? maps[m].times[gt] : 0;
	return Format( "%02i:%02i", t / 60, t % 60 );
}

/** The selection e the cvar under test shows for value v of a list of count. */
static int Shown( long v, int count ) {
	return v >= 0 && v < count ? (int)v : 0;
}

/** ui_gameType: the skirmish game type. */
static void TestGameType( long v ) {
	int e = Shown( v, ARRAY_LEN( gameTypes ) );
	int gt = gameTypes[e].gt;
	int key, next;

	CheckScores( 0, gt, "_UI_Init" );
	CheckDraw( UI_GAMETYPE, gameTypes[e].name, "UI_GAMETYPE" );
	CheckWidth( UI_GAMETYPE, gameTypes[e].name, "UI_GAMETYPE" );
	Set( cvar, value );
	Check( UI_OwnerDrawVisible( UI_SHOW_ANYTEAMGAME ) == ( gt > GT_TEAM ), "UI_SHOW_ANYTEAMGAME" );
	Check( UI_OwnerDrawVisible( UI_SHOW_ANYNONTEAMGAME ) == ( gt <= GT_TEAM ), "UI_SHOW_ANYNONTEAMGAME" );
	CheckDraw( UI_MAP_TIMETOBEAT, TimeToBeat( 0, gt ), "UI_MAP_TIMETOBEAT" );
	Set( cvar, value );
	Check( UI_FeederCount( FEEDER_MAPS ) == MapsListed( gt, qtrue ), "FEEDER_MAPS count" );
	opened[0] = '\0';
	UI_FeederSelection( FEEDER_MAPS, 0 );
	Check( ui_currentMap.integer == FirstMap( gt, qtrue ), "FEEDER_MAPS selects the game type's first map" );
	CheckScores( FirstMap( gt, qtrue ), gt, "FEEDER_MAPS" );
	Set( "ui_currentMap", "0" );

	for ( key = K_MOUSE1; key <= K_MOUSE2; key++ ) {
		Key( UI_GAMETYPE, key );
		next = StepGameType( e, key );
		Check( atoi( CvarString( "ui_gameType" ) ) == next, "UI_GAMETYPE key steps from the game type shown" );
		Check( atoi( CvarString( "ui_Q3Model" ) ) == ( gameTypes[next].gt == GT_TOURNAMENT ), "UI_GAMETYPE key sets ui_Q3Model" );
		Check( atoi( CvarString( "ui_captureLimit" ) ) == CaptureLimit( gameTypes[next].gt ), "UI_GAMETYPE key sets ui_captureLimit" );
		CheckScores( 0, gameTypes[next].gt, "UI_GAMETYPE key" );
		Set( "ui_currentMap", "0" );
	}

	RunScript( "updateSPMenu" );
	next = StepGameType( StepGameType( e, K_MOUSE1 ), K_MOUSE2 );
	Check( atoi( CvarString( "ui_gameType" ) ) == next, "updateSPMenu steps the game type forth and back" );
	Check( atoi( CvarString( "ui_captureLimit" ) ) == CaptureLimit( gameTypes[next].gt ), "updateSPMenu sets ui_captureLimit" );
	Set( "ui_currentMap", "0" );

	RunScript( "loadGameInfo" );
	CheckScores( 0, gt, "loadGameInfo" );
	uiInfo.demoAvailable = qtrue;
	RunScript( "RunSPDemo" );
	Check( !strcmp( executed, Format( "demo %s_%i\n", maps[0].loadName, gt ) ), "RunSPDemo" );
	RunScript( "SkirmishStart" );
	CheckMapStarted( 0, "SkirmishStart" );
	Check( atoi( CvarString( "g_gametype" ) ) == gt, "SkirmishStart sets g_gametype" );
	Check( atoi( CvarString( "capturelimit" ) ) == CaptureLimit( gt ), "SkirmishStart sets capturelimit" );
}

/** ui_netGameType: the create server game type. */
static void TestNetGameType( long v ) {
	int e = Shown( v, ARRAY_LEN( gameTypes ) );
	int gt = gameTypes[e].gt;
	int key, next;

	// the item resets a value past the list to the first game type, which it draws
	Set( "ui_actualNetGameType", "5" );
	CheckDraw( UI_NETGAMETYPE, gameTypes[e].name, "UI_NETGAMETYPE" );
	Check( !strcmp( CvarString( "ui_netGameType" ), e == v ? value : "0" ), "UI_NETGAMETYPE resets ui_netGameType" );
	Check( atoi( CvarString( "ui_actualNetGameType" ) ) == ( e == v ? 5 : 0 ), "UI_NETGAMETYPE resets ui_actualNetGameType" );
	Set( cvar, value );
	Check( UI_OwnerDrawVisible( UI_SHOW_NETANYTEAMGAME ) == ( gt > GT_TEAM ), "UI_SHOW_NETANYTEAMGAME" );
	Check( UI_OwnerDrawVisible( UI_SHOW_NETANYNONTEAMGAME ) == ( gt <= GT_TEAM ), "UI_SHOW_NETANYNONTEAMGAME" );
	Set( cvar, value );
	Check( UI_FeederCount( FEEDER_ALLMAPS ) == MapsListed( gt, qfalse ), "FEEDER_ALLMAPS count" );

	for ( key = K_MOUSE1; key <= K_MOUSE2; key++ ) {
		Key( UI_NETGAMETYPE, key );
		next = Step( e, ARRAY_LEN( gameTypes ), key );
		Check( atoi( CvarString( "ui_netGameType" ) ) == next, "UI_NETGAMETYPE key steps from the game type shown" );
		Check( atoi( CvarString( "ui_actualNetGameType" ) ) == gameTypes[next].gt, "UI_NETGAMETYPE key sets ui_actualNetGameType" );
	}

	Set( "ui_currentNetMap", "0" );
	RunScript( "StartServer" );
	Check( atoi( CvarString( "g_gametype" ) ) == ( gt < 0 ? 0 : gt > 8 ? 8 : gt ), "StartServer sets g_gametype" );
	CheckMapStarted( 0, "StartServer" );
	RunScript( "voteGame" );
	Check( !strcmp( executed, e == v ? Format( "callvote g_gametype %i\n", gt ) : "" ), "voteGame votes only for a listed game type" );
}

/** ui_joinGameType: the server browser's game type. */
static void TestJoinGameType( long v ) {
	int e = Shown( v, ARRAY_LEN( joinGameTypes ) );
	int key, next;

	CheckDraw( UI_JOINGAMETYPE, joinGameTypes[e].name, "UI_JOINGAMETYPE" );
	Check( !strcmp( CvarString( "ui_joinGameType" ), e == v ? value : "0" ), "UI_JOINGAMETYPE resets ui_joinGameType" );
	Set( cvar, value );
	UI_BuildServerDisplayList( qtrue );
	Check( uiInfo.serverStatus.numDisplayServers == ServersListed( e, 0 ), "the browser lists the game type shown" );
	for ( key = K_MOUSE1; key <= K_MOUSE2; key++ ) {
		Key( UI_JOINGAMETYPE, key );
		next = Step( e, ARRAY_LEN( joinGameTypes ), key );
		Check( atoi( CvarString( "ui_joinGameType" ) ) == next, "UI_JOINGAMETYPE key steps from the game type shown" );
		Check( uiInfo.serverStatus.numDisplayServers == ServersListed( next, 0 ), "UI_JOINGAMETYPE key lists the next game type" );
	}
}

/** ui_currentMap: the skirmish map. The skirmish game type stays Team
 * Deathmatch, which lists the free for all maps. */
static void TestCurrentMap( long v ) {
	int e = Shown( v, ARRAY_LEN( maps ) );
	int gt = GT_TEAM;
	int m, index;

	CheckScores( e, gt, "_UI_Init" );
	// the items reset a value past the list to the first map, which they show
	CheckDraw( UI_MAP_TIMETOBEAT, TimeToBeat( e, gt ), "UI_MAP_TIMETOBEAT" );
	Check( !strcmp( CvarString( "ui_currentMap" ), e == v ? value : "0" ), "UI_MAP_TIMETOBEAT resets ui_currentMap" );
	for ( m = 0; m < uiInfo.mapCount; m++ ) {
		uiInfo.mapList[m].cinematic = -1;
	}
	CheckDraw( UI_MAPCINEMATIC, "", "UI_MAPCINEMATIC" );
	Check( !strcmp( playedCinematic, Format( "%s.roq", maps[e].loadName ) ), "UI_MAPCINEMATIC plays the map's cinematic" );
	Check( !strcmp( CvarString( "ui_currentMap" ), e == v ? value : "0" ), "UI_MAPCINEMATIC resets ui_currentMap" );

	// the tier items read ui_currentMap as a tier's map, and there are no tiers
	CheckSameDraw( UI_TIER_GAMETYPE, "UI_TIER_GAMETYPE" );
	CheckSameDraw( UI_TIER_MAPNAME, "UI_TIER_MAPNAME" );

	// selecting a map stops the shown map's cinematic, as does closing the item
	Set( cvar, value );
	UI_FeederCount( FEEDER_MAPS );
	PlayAll();
	UI_FeederSelection( FEEDER_MAPS, 0 );
	Check( stoppedCinematic == PLAYING + e, "FEEDER_MAPS stops the shown map's cinematic" );
	Set( cvar, value );
	PlayAll();
	UI_StopCinematic( -UI_MAPCINEMATIC );
	Check( stoppedCinematic == PLAYING + e, "UI_MAPCINEMATIC stops the shown map's cinematic" );

	Key( UI_GAMETYPE, K_MOUSE1 );
	CheckScores( e, StepGameType( 3, K_MOUSE1 ), "UI_GAMETYPE key" );
	Set( "ui_gameType", "3" );

	RunScript( "updateSPMenu" );
	index = 0;
	if ( e == v && MapListed( e, gt, qtrue ) ) {
		for ( m = 0; m < e; m++ ) {
			index += MapListed( m, gt, qtrue );
		}
	}
	Check( atoi( CvarString( "ui_mapIndex" ) ) == index, "updateSPMenu finds the map in the skirmish list" );
	Set( "ui_gameType", "3" );

	RunScript( "loadGameInfo" );
	CheckScores( e, gt, "loadGameInfo" );
	uiInfo.demoAvailable = qtrue;
	RunScript( "RunSPDemo" );
	Check( !strcmp( executed, Format( "demo %s_%i\n", maps[e].loadName, gt ) ), "RunSPDemo" );
	Set( "ui_recordSPDemo", "1" );
	RunScript( "SkirmishStart" );
	CheckMapStarted( e, "SkirmishStart" );
	Check( !strcmp( CvarString( "ui_scoreMap" ), maps[e].name ), "SkirmishStart sets ui_scoreMap" );
	Check( !strcmp( CvarString( "ui_recordSPDemoName" ), Format( "%s_%d", maps[e].loadName, gt ) ), "SkirmishStart names the demo" );
	Check( atoi( CvarString( "sv_maxClients" ) ) == maps[e].teamMembers * 2, "SkirmishStart fills the map's teams" );
	Set( "ui_recordSPDemo", "0" );
	Set( "ui_gameType", "1" );	// a tournament against the map's opponent
	RunScript( "SkirmishStart" );
	CheckMapStarted( e, "SkirmishStart" );
	Check( strstr( executed, Format( "addbot %s ", maps[e].opponent ) ) != NULL, "SkirmishStart adds the map's opponent" );
	Set( "ui_gameType", "3" );

	// the postgame time bonus beats the time of the map that was started
	Set( cvar, value );
	Com_sprintf( serverInfo, sizeof( serverInfo ), "\\mapname\\%s\\g_gametype\\%d", maps[e].loadName, gt );
	CheckPostgame( maps[e].times[gt] );
}

/** ui_currentNetMap: the create server map. */
static void TestCurrentNetMap( long v ) {
	int e = Shown( v, ARRAY_LEN( maps ) );
	int m;

	// the items reset a value past the list to the first map, which they show
	Set( cvar, value );
	Draw( UI_MAPPREVIEW );
	Check( drawnShader == uiInfo.mapList[e].levelShot && drawnShader > 0, "UI_MAPPREVIEW draws the map's levelshot" );
	Check( !strcmp( CvarString( "ui_currentNetMap" ), e == v ? value : "0" ), "UI_MAPPREVIEW resets ui_currentNetMap" );
	for ( m = 0; m < uiInfo.mapCount; m++ ) {
		uiInfo.mapList[m].cinematic = -1;
	}
	CheckDraw( UI_STARTMAPCINEMATIC, "", "UI_STARTMAPCINEMATIC" );
	Check( !strcmp( playedCinematic, Format( "%s.roq", maps[e].loadName ) ), "UI_STARTMAPCINEMATIC plays the map's cinematic" );
	CheckDraw( UI_NETMAPCINEMATIC, "", "UI_NETMAPCINEMATIC" );
	Check( !strcmp( CvarString( "ui_currentNetMap" ), e == v ? value : "0" ), "UI_NETMAPCINEMATIC resets ui_currentNetMap" );
	CheckDraw( UI_ALLMAPS_SELECTION, e == v ? maps[e].name : "", "UI_ALLMAPS_SELECTION" );

	Set( cvar, value );
	UI_FeederCount( FEEDER_ALLMAPS );
	PlayAll();
	UI_FeederSelection( FEEDER_ALLMAPS, 0 );
	Check( stoppedCinematic == PLAYING + e, "FEEDER_ALLMAPS stops the shown map's cinematic" );

	RunScript( "StartServer" );
	CheckMapStarted( e, "StartServer" );
	RunScript( "voteMap" );
	Check( !strcmp( executed, e == v ? Format( "callvote map %s\n", maps[e].loadName ) : "" ), "voteMap votes only for a listed map" );
}

/** ui_netSource: the server browser's source. */
static void TestNetSource( long v ) {
	int e = Shown( v, numNetSources );
	int key, next;

	CheckDraw( UI_NETSOURCE, Format( "Source: %s", netSources[e] ), "UI_NETSOURCE" );
	CheckWidth( UI_NETSOURCE, Format( "Source: %s", netSources[e] ), "UI_NETSOURCE" );
	for ( key = K_MOUSE1; key <= K_MOUSE2; key++ ) {
		Key( UI_NETSOURCE, key );
		next = Step( e, numNetSources, key );
		if ( next == AS_MPLAYER ) {	// the item skips Mplayer
			next = Step( next, numNetSources, key );
		}
		Check( atoi( CvarString( "ui_netSource" ) ) == next, "UI_NETSOURCE key steps from the source shown" );
	}
}

/** ui_serverFilterType: the server browser's mod filter, which only its item
 * steps; the item and the browser still show the first filter for any other
 * value. */
static void TestServerFilter( long v ) {
	int e = Shown( v, numServerFilters );
	int key;

	CheckDraw( UI_NETFILTER, Format( "Filter: %s", serverFilters[e].description ), "UI_NETFILTER" );
	CheckWidth( UI_NETFILTER, Format( "Filter: %s", serverFilters[e].description ), "UI_NETFILTER" );
	Set( "ui_joinGameType", "0" );
	Set( cvar, value );
	UI_BuildServerDisplayList( qtrue );
	Check( uiInfo.serverStatus.numDisplayServers == ServersListed( 0, e ), "the browser lists the mod shown" );
	for ( key = K_MOUSE1; key <= K_MOUSE2 && e == v; key++ ) {
		Key( UI_NETFILTER, key );
		Check( ui_serverFilterType.integer == Step( e, numServerFilters, key ), "UI_NETFILTER key steps through the filters" );
		Check( uiInfo.serverStatus.numDisplayServers == ServersListed( 0, ui_serverFilterType.integer ), "UI_NETFILTER key lists the next mod" );
	}
}

/** ui_currentTier: there are no tiers, so every value shows the first. */
static void TestTier( void ) {
	static const int items[] = { UI_TIER, UI_TIERMAP1, UI_TIERMAP2, UI_TIERMAP3, UI_TIER_MAPNAME, UI_TIER_GAMETYPE };
	int i;

	for ( i = 0; i < ARRAY_LEN( items ); i++ ) {
		CheckSameDraw( items[i], Format( "tier item %d", items[i] ) );
	}
}

/** g_spSkill: the skill levels, 1 to 5; anything else shows 1. */
static void TestSkill( long v ) {
	int e = v >= 1 && v <= numSkillLevels ? (int)v : 1;

	CheckDraw( UI_SKILL, skillLevels[e - 1], "UI_SKILL" );
	CheckWidth( UI_SKILL, skillLevels[e - 1], "UI_SKILL" );
	Key( UI_SKILL, K_MOUSE1 );
	Check( atoi( CvarString( "g_spSkill" ) ) == e % numSkillLevels + 1, "UI_SKILL key steps from the level shown" );
	Key( UI_SKILL, K_MOUSE2 );
	Check( atoi( CvarString( "g_spSkill" ) ) == ( e == 1 ? numSkillLevels : e - 1 ), "UI_SKILL key steps from the level shown" );
}

/** ui_blueteam1 or ui_redteam1: a create server slot of a team game, closed
 * (0), human (1) or one of the characters (2 on); a value below 0 shows
 * closed, and one past the characters the first. */
static void TestTeamMember( long v, qboolean blue ) {
	int count = ARRAY_LEN( characters ) + 2;
	int e = v <= 0 ? 0 : v < count ? (int)v : 2;
	const char *text = e == 0 ? "Closed" : e == 1 ? "Human" : characters[e - 2];
	int item = blue ? UI_BLUETEAM1 : UI_REDTEAM1;
	int key;

	Set( "ui_actualNetGameType", "3" );
	CheckDraw( item, text, "the slot" );
	CheckWidth( item, Format( "1. %s", text ), "the slot" );	// the aliases have the characters' names
	for ( key = K_MOUSE1; key <= K_MOUSE2; key++ ) {
		Key( item, key );
		Check( atoi( CvarString( cvar ) ) == Step( e, count, key ), "the slot's key steps from the slot shown" );
	}

	// the server takes the slot's player, and counts it when it is open
	Set( "sv_maxClients", "0" );
	RunScript( "StartServer" );
	Check( atoi( CvarString( "sv_maxClients" ) ) == 2 * PLAYERS_PER_TEAM - 1 + ( v >= 0 ), "StartServer counts the open slots" );
	Check( ( strstr( executed, "addbot" ) != NULL ) == ( e >= 2 ), "StartServer adds a bot for a character slot" );
	if ( e >= 2 ) {
		Check( strstr( executed, Format( "addbot %s ", text ) ) != NULL && strstr( executed, blue ? " Blue\n" : " Red\n" ) != NULL,
			"StartServer adds the slot's character" );
	}
}

/** cg_drawCrosshair: the ten crosshairs; anything else shows the first. */
static void TestCrosshair( long v ) {
	int e = Shown( v, NUM_CROSSHAIRS );

	Check( uiInfo.currentCrosshair == e, "_UI_Init reads the crosshair shown" );
	Draw( UI_CROSSHAIR );
	Check( drawnShader == uiInfo.uiDC.Assets.crosshairShader[e], "UI_CROSSHAIR draws the crosshair" );
	UI_OwnerDrawHandleKey( UI_CROSSHAIR, 0, NULL, K_MOUSE1 );
	Check( atoi( CvarString( cvar ) ) == Step( e, NUM_CROSSHAIRS, K_MOUSE1 ), "UI_CROSSHAIR key steps from the crosshair shown" );
	UI_OwnerDrawHandleKey( UI_CROSSHAIR, 0, NULL, K_MOUSE2 );
	Check( atoi( CvarString( cvar ) ) == e, "UI_CROSSHAIR key steps from the crosshair shown" );
}

/** ui_mapIndex: the skirmish selection's place among the game type's maps;
 * the next skirmish goes to the next map, or the next game type after the
 * last. A value past the list is the first map. */
static void TestMapIndex( long v ) {
	int listed = MapsListed( GT_TEAM, qtrue );
	int e = Shown( v, listed );
	int last = e == listed - 1;

	RunScript( "nextSkirmish" );
	Check( atoi( CvarString( "ui_gameType" ) ) == ( last ? StepGameType( 3, K_MOUSE1 ) : 3 ), "nextSkirmish moves on to the next game type after the last map" );
}

/** The postgame command with the server's g_gametype: a game type the table
 * has no entry for has no time to beat. */
static void TestPostgame( long v ) {
	int m = 1;

	Set( "ui_currentMap", "1" );
	Com_sprintf( serverInfo, sizeof( serverInfo ), "\\mapname\\%s\\g_gametype\\%s", maps[m].loadName, value );
	CheckPostgame( v >= 0 && v < GT_MAX_GAME_TYPE ? maps[m].times[v] : 0 );
}

/** A map's team size from gameinfo.txt: the skirmish sizes the server for
 * both teams and adds a bot for every player but the local one, and a team
 * names TEAM_MEMBERS players. */
static void TestTeamSize( long v ) {
	int members = v < 0 ? 0 : v > TEAM_MEMBERS ? TEAM_MEMBERS : (int)v;
	int bots = 0;
	const char *s;

	cvar = "ui_currentMap";
	value = "0";
	RunScript( "SkirmishStart" );
	CheckMapStarted( 0, "SkirmishStart" );
	Check( atoi( CvarString( "sv_maxClients" ) ) == members * 2, "SkirmishStart sizes the server for both teams" );
	for ( s = strstr( executed, "addbot " ); s; s = strstr( s + 1, "addbot " ) ) {
		bots++;
	}
	Check( bots == members + ( members > 0 ? members - 1 : 0 ), "SkirmishStart adds the teams' bots" );
}

/** The postgame skill bonus: g_spSkill's level, 1 to 5, multiplies the score
 * that is kept as the map's best. */
static void TestSkillBonus( long v ) {
	int skill = v < 1 ? 1 : v > 5 ? 5 : (int)v;
	int m = 1;

	Set( "g_spSkill", value );
	Set( "ui_currentMap", "1" );
	Com_sprintf( serverInfo, sizeof( serverInfo ), "\\mapname\\%s\\g_gametype\\%d", maps[m].loadName, GT_CTF );
	CheckPostgame( maps[m].times[GT_CTF] );
	Check( atoi( CvarString( "ui_scoreSkillBonus" ) ) == skill, "postgame skill bonus" );
	Check( atoi( CvarString( "ui_scoreScore" ) ) == ( atoi( BASE_SCORE ) + ( maps[m].times[GT_CTF] - START_TIME ) * 10 ) * skill,
		"postgame score" );
}

/** No gameinfo.txt yet: the game type and map lists are empty, and every game
 * type selection reads the empty first entry. The keys step as they always
 * did (the skirmish item's previous game type is then -1, the browser's
 * next 0), the browser keeps the servers of game type 0, and the player's
 * saved map is still selected once a menu loads the list. */
static void TestEmptyLists( void ) {
	int key;

	cvar = "ui_gameType";
	CheckDraw( UI_GAMETYPE, "", "UI_GAMETYPE" );
	for ( key = K_MOUSE1; key <= K_MOUSE2; key++ ) {
		Key( UI_GAMETYPE, key );
		Check( atoi( CvarString( "ui_gameType" ) ) == ( key == K_MOUSE1 ? 1 : -1 ), "UI_GAMETYPE key steps from the empty entry" );
		Check( atoi( CvarString( "ui_Q3Model" ) ) == 0, "UI_GAMETYPE key reads the empty entry" );
	}
	cvar = "ui_netGameType";
	CheckDraw( UI_NETGAMETYPE, "", "UI_NETGAMETYPE" );
	for ( key = K_MOUSE1; key <= K_MOUSE2; key++ ) {
		Key( UI_NETGAMETYPE, key );
		Check( atoi( CvarString( "ui_netGameType" ) ) == ( key == K_MOUSE1 ? 0 : -1 ), "UI_NETGAMETYPE key steps from the empty entry" );
		Check( atoi( CvarString( "ui_actualNetGameType" ) ) == 0, "UI_NETGAMETYPE key reads the empty entry" );
	}
	cvar = "ui_joinGameType";
	Set( cvar, value );
	UI_BuildServerDisplayList( qtrue );
	Check( uiInfo.serverStatus.numDisplayServers == 1, "the browser lists game type 0" );
	CheckDraw( UI_JOINGAMETYPE, "", "UI_JOINGAMETYPE" );

	// the list loads with the saved map selected
	cvar = "ui_gameType";
	Check( !strcmp( CvarString( "ui_currentMap" ), "2" ), "the saved map is kept" );
	noGameInfo = qfalse;
	RunScript( "loadGameInfo" );
	CheckScores( 2, gameTypes[Shown( strtol( value, NULL, 10 ), ARRAY_LEN( gameTypes ) )].gt, "loadGameInfo" );
}

/** One cvar and value per process. */
int main( int argc, char **argv ) {
	static const char *cvars[] = { "ui_gameType", "ui_netGameType", "ui_joinGameType", "ui_currentMap", "ui_currentNetMap",
		"ui_netSource", "ui_serverFilterType", "ui_currentTier", "g_spSkill", "ui_blueteam1", "ui_redteam1",
		"cg_drawCrosshair", "ui_mapIndex", "postgame", "emptyLists", "gtEnum", "teamMembers", "skillBonus" };
	char *end;
	long v;
	int i, m;

	if ( argc != 3 ) {
		fprintf( stderr, "usage: %s <cvar> <value>\n", argv[0] );
		return 2;
	}
	cvar = argv[1];
	value = argv[2];
	for ( i = 0; i < ARRAY_LEN( cvars ) && strcmp( cvar, cvars[i] ); i++ ) {
	}
	Check( i < ARRAY_LEN( cvars ), "a cvar under test" );
	v = strtol( value, &end, 10 );
	Check( *value && !*end && v >= INT_MIN && v <= INT_MAX, "a decimal int" );
	if ( !strcmp( argv[1], "gtEnum" ) ) {
		// the Harvester's number, as a pk3's gameinfo.txt can give it; the
		// skirmish and create server menus then select it
		gameTypes[7].gt = (int)v;
		Com_sprintf( install, sizeof( install ), ", Harvester's gtEnum %s", value );
		cvar = "ui_gameType";
		value = "7";
	} else if ( !strcmp( argv[1], "teamMembers" ) ) {
		maps[0].teamMembers = (int)v;
		Com_sprintf( install, sizeof( install ), ", map 0's teamMembers %s", value );
	}

	// gameinfo.txt from the tables above
	Q_strcat( gameinfoText, sizeof( gameinfoText ), "gametypes {\n" );
	for ( i = 0; i < ARRAY_LEN( gameTypes ); i++ ) {
		Q_strcat( gameinfoText, sizeof( gameinfoText ), Format( "{ \"%s\" %d }\n", gameTypes[i].name, gameTypes[i].gt ) );
	}
	Q_strcat( gameinfoText, sizeof( gameinfoText ), "}\njoingametypes {\n" );
	for ( i = 0; i < ARRAY_LEN( joinGameTypes ); i++ ) {
		Q_strcat( gameinfoText, sizeof( gameinfoText ), Format( "{ \"%s\" %d }\n", joinGameTypes[i].name, joinGameTypes[i].gt ) );
	}
	Q_strcat( gameinfoText, sizeof( gameinfoText ), "}\nmaps {\n" );
	for ( m = 0; m < ARRAY_LEN( maps ); m++ ) {
		Q_strcat( gameinfoText, sizeof( gameinfoText ), Format( "{ \"%s\" %s %d \"%s\"", maps[m].name, maps[m].loadName, maps[m].teamMembers, maps[m].opponent ) );
		for ( i = 0; i < GT_MAX_GAME_TYPE; i++ ) {
			if ( maps[m].times[i] ) {
				Q_strcat( gameinfoText, sizeof( gameinfoText ), Format( " %d %d", i, maps[m].times[i] ) );
			}
		}
		Q_strcat( gameinfoText, sizeof( gameinfoText ), " }\n" );
	}
	Q_strcat( gameinfoText, sizeof( gameinfoText ), "}\n" );

	dllEntry( FakeSyscall );
	// _UI_Init reads these, and a server can still set them then
	if ( !strcmp( cvar, "ui_gameType" ) || !strcmp( cvar, "ui_currentMap" ) || !strcmp( cvar, "cg_drawCrosshair" ) ) {
		SystemInfo_Set( cvar, value );
	}
	if ( !strcmp( cvar, "emptyLists" ) ) {
		noGameInfo = qtrue;
		SystemInfo_Set( "ui_currentMap", "2" );
	}
	Init();

	if ( !strcmp( argv[1], "gtEnum" ) ) {
		TestGameType( 7 );
		cvar = "ui_netGameType";
		TestNetGameType( 7 );
	} else if ( !strcmp( argv[1], "teamMembers" ) ) {
		TestTeamSize( v );
	} else if ( !strcmp( cvar, "skillBonus" ) ) {
		TestSkillBonus( v );
	} else if ( !strcmp( cvar, "ui_gameType" ) ) {
		TestGameType( v );
	} else if ( !strcmp( cvar, "ui_netGameType" ) ) {
		TestNetGameType( v );
	} else if ( !strcmp( cvar, "ui_joinGameType" ) ) {
		TestJoinGameType( v );
	} else if ( !strcmp( cvar, "ui_currentMap" ) ) {
		TestCurrentMap( v );
	} else if ( !strcmp( cvar, "ui_currentNetMap" ) ) {
		TestCurrentNetMap( v );
	} else if ( !strcmp( cvar, "ui_netSource" ) ) {
		TestNetSource( v );
	} else if ( !strcmp( cvar, "ui_serverFilterType" ) ) {
		TestServerFilter( v );
	} else if ( !strcmp( cvar, "ui_currentTier" ) ) {
		TestTier();
	} else if ( !strcmp( cvar, "g_spSkill" ) ) {
		TestSkill( v );
	} else if ( !strcmp( cvar, "ui_blueteam1" ) || !strcmp( cvar, "ui_redteam1" ) ) {
		TestTeamMember( v, !strcmp( cvar, "ui_blueteam1" ) );
	} else if ( !strcmp( cvar, "cg_drawCrosshair" ) ) {
		TestCrosshair( v );
	} else if ( !strcmp( cvar, "ui_mapIndex" ) ) {
		TestMapIndex( v );
	} else if ( !strcmp( cvar, "postgame" ) ) {
		TestPostgame( v );
	} else {
		TestEmptyLists();
	}
	printf( "Team Arena UI bounds %s %s (issue #389)\n", cvar, value );
	return 0;
}
