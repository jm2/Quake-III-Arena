/* Issue #444: the Team Arena cgame appended '/' to the server's team name in a
 * MAX_QPATH stack buffer before it built the team model and skin paths.
 *
 * Built by tests/run_cgame_team_name_tests.sh with the whole native cgame, base
 * and Team Arena (MISSIONPACK), behind a fake syscall table. Each case loads a
 * red and a blue player through the real CG_ParseServerinfo, CG_NewClientInfo
 * and CG_LoadClientInfo, and logs the files the cgame looks for and registers.
 *
 *   normal   no team name and the Kreechurs team: the files and handles of the
 *            1.32c cgame
 *   N        a team name of N characters: no overflow, and as many lookups and
 *            the same files (the plain team colour skins) as a 62 character
 *            name, the longest that fits teamname with its '/'
 */
#include "../code/cgame/cg_local.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void dllEntry( int (QDECL *syscallptr)( int arg, ... ) );

#define MAX_LOG		64
#define LOG_LENGTH	( MAX_QPATH * 2 + 8 )

typedef struct {
	char	lines[MAX_LOG][LOG_LENGTH];
	int		count;
} fileLog_t;

/* sarge as baseq3/pak0.pk3 has it, and Kreechurs team skins */
static const char *gameFiles[] = {
	"models/players/sarge/lower.md3",
	"models/players/sarge/upper.md3",
	"models/players/sarge/head.md3",
	"models/players/sarge/animation.cfg",
	"models/players/sarge/lower_red.skin",
	"models/players/sarge/upper_red.skin",
	"models/players/sarge/head_red.skin",
	"models/players/sarge/icon_red.tga",
	"models/players/sarge/lower_blue.skin",
	"models/players/sarge/upper_blue.skin",
	"models/players/sarge/head_blue.skin",
	"models/players/sarge/icon_blue.tga",
	"models/players/sarge/Kreechurs/lower_red.skin",
	"models/players/sarge/Kreechurs/upper_red.skin",
	"models/players/sarge/Kreechurs/head_red.skin",
	"models/players/sarge/Kreechurs/lower_blue.skin",
	"models/players/sarge/Kreechurs/upper_blue.skin",
	"models/players/sarge/Kreechurs/head_blue.skin",
	NULL
};

/* what the 1.32c cgame looks for when it loads a red sarge with no team name */
static const char *plainRedLog[] = {
	"model models/players/sarge/lower.md3",
	"model models/players/sarge/upper.md3",
	"model models/players/sarge/head.md3",
	"find models/players/sarge/lower_default_red.skin",
	"find models/players/sarge/lower_red.skin",
	"skin models/players/sarge/lower_red.skin",
	"find models/players/sarge/upper_default_red.skin",
	"find models/players/sarge/upper_red.skin",
	"skin models/players/sarge/upper_red.skin",
	"find models/players/sarge/default/head_red.skin",
	"find models/players/sarge/head_red.skin",
	"skin models/players/sarge/head_red.skin",
	"open models/players/sarge/animation.cfg",
	"find models/players/sarge/default/icon_red.skin",
	"find models/players/sarge/icon_red.skin",
	"find models/players/heads/sarge/default/icon_red.skin",
	"find models/players/heads/sarge/icon_red.skin",
	"find models/players/sarge/default/icon_red.tga",
	"find models/players/sarge/icon_red.tga",
	"icon models/players/sarge/icon_red.tga",
	NULL
};

/* and for the Kreechurs team: team skins first, the team icon is missing */
static const char *teamRedLog[] = {
	"model models/players/sarge/lower.md3",
	"model models/players/sarge/upper.md3",
	"model models/players/sarge/head.md3",
	"find models/players/sarge/Kreechurs/lower_default_red.skin",
	"find models/players/sarge/Kreechurs/lower_red.skin",
	"skin models/players/sarge/Kreechurs/lower_red.skin",
	"find models/players/sarge/Kreechurs/upper_default_red.skin",
	"find models/players/sarge/Kreechurs/upper_red.skin",
	"skin models/players/sarge/Kreechurs/upper_red.skin",
	"find models/players/sarge/default/Kreechurs/head_red.skin",
	"find models/players/sarge/Kreechurs/head_red.skin",
	"skin models/players/sarge/Kreechurs/head_red.skin",
	"open models/players/sarge/animation.cfg",
	"find models/players/sarge/default/Kreechurs/icon_red.skin",
	"find models/players/sarge/Kreechurs/icon_red.skin",
	"find models/players/sarge/default/icon_red.skin",
	"find models/players/sarge/icon_red.skin",
	"find models/players/heads/sarge/default/Kreechurs/icon_red.skin",
	"find models/players/heads/sarge/Kreechurs/icon_red.skin",
	"find models/players/heads/sarge/default/icon_red.skin",
	"find models/players/heads/sarge/icon_red.skin",
	"find models/players/sarge/default/Kreechurs/icon_red.tga",
	"find models/players/sarge/Kreechurs/icon_red.tga",
	"find models/players/sarge/default/icon_red.tga",
	"find models/players/sarge/icon_red.tga",
	"icon models/players/sarge/icon_red.tga",
	NULL
};

static fileLog_t fileLog;
static char animationText[1024];

/** Fail with the case that broke. */
static void Check( int ok, const char *message, const char *teamName ) {
	if ( !ok ) {
		fprintf( stderr, "Cgame team name regression failed: %s (team name \"%s\", %i characters)\n",
			message, teamName, (int)strlen( teamName ) );
		exit( 1 );
	}
}

/** The handle the fake renderer gives a game file, 0 when it is missing. */
static int FileHandle( const char *path ) {
	int i;

	for ( i = 0; gameFiles[i]; i++ ) {
		if ( !strcmp( gameFiles[i], path ) ) {
			return i + 1;
		}
	}
	return 0;
}

/** Log one lookup or registration and return the file's handle. */
static int LogFile( const char *kind, const char *path ) {
	if ( fileLog.count < MAX_LOG ) {
		snprintf( fileLog.lines[fileLog.count], LOG_LENGTH, "%s %s", kind, path );
	}
	fileLog.count++;
	return FileHandle( path );
}

/** Serve the client syscalls CG_ParseServerinfo and CG_NewClientInfo reach. */
static int QDECL FakeSyscall( int command, ... ) {
	va_list ap;
	int result = 0;

	va_start( ap, command );
	switch ( command ) {
	case CG_ERROR:
		Check( 0, va_arg( ap, const char * ), "" );
		break;
	case CG_MEMORY_REMAINING:
		result = 64 * 1024 * 1024;	// no forced deferred models
		break;
	case CG_CVAR_SET: {
		const char *name = va_arg( ap, const char * );
		const char *value = va_arg( ap, const char * );
#ifdef MISSIONPACK
		// what trap_Cvar_Update gives the cgame's vmCvars on the next frame
		if ( !Q_stricmp( name, "g_redteam" ) ) {
			Q_strncpyz( cg_redTeamName.string, value, sizeof( cg_redTeamName.string ) );
		} else if ( !Q_stricmp( name, "g_blueteam" ) ) {
			Q_strncpyz( cg_blueTeamName.string, value, sizeof( cg_blueTeamName.string ) );
		}
#else
		(void)name; (void)value;
#endif
		break;
	}
	case CG_FS_FOPENFILE: {
		const char *path = va_arg( ap, const char * );
		fileHandle_t *f = va_arg( ap, fileHandle_t * );
		int handle = LogFile( f ? "open" : "find", path );
		if ( f ) {
			*f = handle;
		}
		if ( !handle ) {
			result = -1;
		} else if ( !strcmp( path, "models/players/sarge/animation.cfg" ) ) {
			result = strlen( animationText );
		} else {
			result = 1;
		}
		break;
	}
	case CG_FS_READ: {
		void *buffer = va_arg( ap, void * );
		int length = va_arg( ap, int );
		memcpy( buffer, animationText, length );
		break;
	}
	case CG_R_REGISTERMODEL:
		result = LogFile( "model", va_arg( ap, const char * ) );
		break;
	case CG_R_REGISTERSKIN:
		result = LogFile( "skin", va_arg( ap, const char * ) );
		break;
	case CG_R_REGISTERSHADERNOMIP:
		result = LogFile( "icon", va_arg( ap, const char * ) );
		break;
	case CG_S_REGISTERSOUND:
		result = 1;
		break;
	}
	va_end( ap );
	return result;
}

/** Append one configstring as the gamestate stores it. */
static void SetConfigString( int index, const char *text ) {
	int length = strlen( text ) + 1;

	Check( cgs.gameState.dataCount + length <= MAX_GAMESTATE_CHARS, "fixture gamestate full", text );
	cgs.gameState.stringOffsets[index] = cgs.gameState.dataCount;
	memcpy( cgs.gameState.stringData + cgs.gameState.dataCount, text, length );
	cgs.gameState.dataCount += length;
}

/** Load a sarge on team with the server's team name, or with the g_redteam and
    g_blueteam cvars set to cvarName (a server's systeminfo can set them too). */
static clientInfo_t *LoadClient( int team, const char *serverName, const char *cvarName ) {
	memset( &cg, 0, sizeof( cg ) );
	memset( &cgs, 0, sizeof( cgs ) );
	memset( &fileLog, 0, sizeof( fileLog ) );
	cgs.gameState.dataCount = 1;
	cg.loading = qtrue;

	SetConfigString( CS_SERVERINFO, va( "\\g_gametype\\%i\\sv_maxclients\\8\\mapname\\q3dm1"
		"\\g_redTeam\\%s\\g_blueTeam\\%s", GT_TEAM, serverName, serverName ) );
	CG_ParseServerinfo();
	Check( cgs.gametype == GT_TEAM, "serverinfo game type", serverName );
#ifdef MISSIONPACK
	if ( cvarName ) {
		Q_strncpyz( cg_redTeamName.string, cvarName, sizeof( cg_redTeamName.string ) );
		Q_strncpyz( cg_blueTeamName.string, cvarName, sizeof( cg_blueTeamName.string ) );
	}
#else
	(void)cvarName;
#endif
	SetConfigString( CS_PLAYERS + 3, va( "n\\Player\\t\\%i\\model\\sarge\\hmodel\\sarge\\c1\\4\\c2\\5\\hc\\100", team ) );
	CG_NewClientInfo( 3 );
	return &cgs.clientinfo[3];
}

/** Require the player models and the skins of prefix ("" or a team folder). */
static void CheckLoaded( const clientInfo_t *ci, int team, const char *prefix, const char *teamName ) {
	const char *color = team == TEAM_BLUE ? "blue" : "red";

	Check( ci->infoValid && ci->team == team && !ci->deferred, "client info not loaded", teamName );
	Check( ci->legsModel == FileHandle( "models/players/sarge/lower.md3" ) &&
		ci->torsoModel == FileHandle( "models/players/sarge/upper.md3" ) &&
		ci->headModel == FileHandle( "models/players/sarge/head.md3" ), "player models", teamName );
	Check( ci->legsSkin == FileHandle( va( "models/players/sarge/%slower_%s.skin", prefix, color ) ) &&
		ci->torsoSkin == FileHandle( va( "models/players/sarge/%supper_%s.skin", prefix, color ) ) &&
		ci->headSkin == FileHandle( va( "models/players/sarge/%shead_%s.skin", prefix, color ) ),
		"player skins", teamName );
	Check( ci->modelIcon == FileHandle( va( "models/players/sarge/icon_%s.tga", color ) ), "player icon", teamName );
	Check( ci->animations[LEGS_WALK].numFrames == 1, "animation.cfg", teamName );
	Check( fileLog.count <= MAX_LOG, "fixture log full", teamName );
}

/** Require the logged lookups and registrations to be expected, line by line. */
static void CheckLog( const char **expected, const char *teamName ) {
	int i;

	for ( i = 0; expected[i] && i < fileLog.count; i++ ) {
		if ( strcmp( fileLog.lines[i], expected[i] ) ) {
			fprintf( stderr, "line %i: \"%s\", 1.32c: \"%s\"\n", i, fileLog.lines[i], expected[i] );
			Check( 0, "file lookups differ from 1.32c", teamName );
		}
	}
	Check( !expected[i] && i == fileLog.count, "number of file lookups differs from 1.32c", teamName );
}

/** No team name and a normal team name load what the 1.32c cgame loads. */
static void TestNormalNames( void ) {
	CheckLoaded( LoadClient( TEAM_RED, "", NULL ), TEAM_RED, "", "" );
	CheckLog( plainRedLog, "" );
	CheckLoaded( LoadClient( TEAM_BLUE, "", NULL ), TEAM_BLUE, "", "" );

#ifdef MISSIONPACK
	CheckLoaded( LoadClient( TEAM_RED, "Kreechurs", NULL ), TEAM_RED, "Kreechurs/", "Kreechurs" );
	CheckLog( teamRedLog, "Kreechurs" );
	CheckLoaded( LoadClient( TEAM_BLUE, "Kreechurs", NULL ), TEAM_BLUE, "Kreechurs/", "Kreechurs" );
	CheckLoaded( LoadClient( TEAM_RED, "", "Kreechurs" ), TEAM_RED, "Kreechurs/", "Kreechurs" );
	CheckLog( teamRedLog, "Kreechurs" );
	puts( "Team Arena cgame team name regressions passed: no team name and Kreechurs load the 1.32c files (issue #444)" );
#else
	// the Quake3 cgame has no team skins
	CheckLoaded( LoadClient( TEAM_RED, "Kreechurs", NULL ), TEAM_RED, "", "Kreechurs" );
	CheckLog( plainRedLog, "Kreechurs" );
	puts( "Cgame team name regressions passed: Kreechurs loads the 1.32c files (issue #444)" );
#endif
}

/** A team name of length characters, from serverinfo and from the cvars. */
static void TestLongName( int length ) {
	static char name[MAX_CVAR_VALUE_STRING], baseline[MAX_CVAR_VALUE_STRING];
	static fileLog_t baselineLog;
	static const int teams[2] = { TEAM_RED, TEAM_BLUE };
	int i, source, line;

	Check( length > 0 && length < MAX_CVAR_VALUE_STRING, "fixture team name length", "" );
	memset( name, 'R', length );
	name[length] = 0;
	memset( baseline, 'R', 62 );	// the longest name that fits with its slash
	baseline[62] = 0;

	for ( i = 0; i < 2; i++ ) {
		// every team path is longer than MAX_QPATH - 1, which the renderer refuses
		// and Com_sprintf cuts the skin paths to: no team file is found, and the
		// cgame loads the plain team colour skins
		CheckLoaded( LoadClient( teams[i], baseline, NULL ), teams[i], "", baseline );
		baselineLog = fileLog;
		for ( source = 0; source < 2; source++ ) {
#ifndef MISSIONPACK
			if ( source ) {
				break;		// no g_redteam or g_blueteam cvar
			}
#endif
			// serverinfo keeps MAX_QPATH - 1 characters; the cvars keep up to 255
			CheckLoaded( LoadClient( teams[i], source ? "" : name, source ? name : NULL ), teams[i], "", name );
			Check( fileLog.count == baselineLog.count, "number of file lookups differs from 62 characters", name );
			for ( line = 0; line < fileLog.count; line++ ) {
				Check( !strncmp( fileLog.lines[line], "find ", 5 ) ||
					!strcmp( fileLog.lines[line], baselineLog.lines[line] ),
					"registered files differ from 62 characters", name );
			}
		}
	}
	printf( "%s team name regressions passed: %i characters (issue #444)\n",
#ifdef MISSIONPACK
		"Team Arena cgame",
#else
		"Cgame",
#endif
		length );
}

int main( int argc, char **argv ) {
	int i;

	if ( argc != 2 ) {
		fprintf( stderr, "usage: %s normal|LENGTH\n", argv[0] );
		return 2;
	}
	for ( i = 0; i < TORSO_GETFLAG; i++ ) {
		Q_strcat( animationText, sizeof( animationText ), "0 1 0 20\n" );
	}
	dllEntry( FakeSyscall );
	if ( !strcmp( argv[1], "normal" ) ) {
		TestNormalNames();
	} else {
		TestLongName( atoi( argv[1] ) );
	}
	return 0;
}
