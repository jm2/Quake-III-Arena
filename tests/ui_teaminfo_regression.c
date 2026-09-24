/* Issues #357 and #344: Team Arena teaminfo.txt and gameinfo.txt lists stay
 * within their arrays, and the per-team head mask never shifts past bit 31. */
#include "../code/ui/ui_main.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SENTINEL 0x5a5a5a5a
#define HEADS 8

static char fixture[MAX_MENUFILE];
static int fixtureLength;
static char teamNameCvar[MAX_CVAR_VALUE_STRING];
static int ignoredWarnings;

/** Fail with a description of the first mismatch. */
static void Check( int ok, const char *what ) {
	if ( !ok ) {
		fprintf( stderr, "UI teaminfo regression failed: %s\n", what );
		exit( 1 );
	}
}

/** Append printf-style text to the served teaminfo.txt or gameinfo.txt. */
static void Add( const char *format, ... ) {
	va_list args;
	int written;
	va_start( args, format );
	written = vsnprintf( fixture + fixtureLength, sizeof( fixture ) - fixtureLength, format, args );
	va_end( args );
	Check( written >= 0 && written < (int)sizeof( fixture ) - fixtureLength, "fixture fits MAX_MENUFILE" );
	fixtureLength += written;
}

/** Start a new served file and an empty uiInfo, as UI_Init does. */
static void Reset( void ) {
	fixtureLength = 0;
	fixture[0] = 0;
	memset( &uiInfo, 0, sizeof( uiInfo ) );
	ignoredWarnings = 0;
}

/** Whether head B<base> has a skin for team T<team>; head 0 has one for every team. */
static int HasSkin( int base, int team ) {
	return base == 0 || ( base + team ) % 3 == 0;
}

/** Serve the fixture for the menu file and the skin table for UI_hasSkinForBase. */
int trap_FS_FOpenFile( const char *qpath, fileHandle_t *f, fsMode_t mode ) {
	int base, team;
	(void)mode;
	if ( f ) {
		*f = 1;
		return fixtureLength;
	}
	if ( sscanf( qpath, "models/players/B%d/T%d/lower_default.skin", &base, &team ) == 2 ) {
		return HasSkin( base, team );
	}
	return 0;
}
/** Copy the served file into GetMenuBuffer's buffer. */
void trap_FS_Read( void *buffer, int len, fileHandle_t f ) {
	(void)f;
	Check( len == fixtureLength, "GetMenuBuffer reads the whole file" );
	memcpy( buffer, fixture, len );
}
/** Nothing to close for the served file. */
void trap_FS_FCloseFile( fileHandle_t f ) { (void)f; }
/** Give every icon and level shot a handle. */
qhandle_t trap_R_RegisterShaderNoMip( const char *name ) { (void)name; return 1; }
/** Fail if GetMenuBuffer rejects the served file. */
void trap_Print( const char *string ) {
	fprintf( stderr, "%s", string );
	Check( 0, "menu file served" );
}
/** Report the team the head list is built for. */
char *UI_Cvar_VariableString( const char *var_name ) {
	return Q_stricmp( var_name, "ui_teamName" ) ? "" : teamNameCvar;
}
/** Count the parsers' warnings for list entries that did not fit. */
void QDECL Com_Printf( const char *msg, ... ) {
	if ( !Q_strncmp( msg, "Too many", 8 ) ) {
		ignoredWarnings++;
	}
}
/** Menu script parsing is kept by the linked ui_shared.c but never runs here. */
int trap_PC_ReadToken( int handle, pc_token_t *pc_token ) {
	(void)handle; (void)pc_token;
	Check( 0, "no menu script parsing" );
	return 0;
}
/** Menu script parsing is kept by the linked ui_shared.c but never runs here. */
int trap_PC_SourceFileAndLine( int handle, char *filename, int *line ) {
	(void)handle; (void)filename; (void)line;
	Check( 0, "no menu script parsing" );
	return 0;
}
/** Fail on engine errors. */
void QDECL Com_Error( int level, const char *error, ... ) {
	(void)level;
	fprintf( stderr, "Unexpected Com_Error: %s\n", error );
	exit( 1 );
}

/** Append a teams block with count teams T00, T01, ... */
static void AddTeams( int count ) {
	int i;
	Add( "teams {\n" );
	for ( i = 0; i < count; i++ ) {
		Add( "\t{ \"T%02d\" \"ui/assets/t%02d\" \"M%02da\" \"M%02db\" \"M%02dc\" \"M%02dd\" \"M%02de\" }\n", i, i, i, i, i, i, i );
	}
	Add( "}\n" );
}

/** Check that the first count teams are stored in file order. */
static void CheckTeams( int count ) {
	int i;
	Check( uiInfo.teamCount == count, "team count" );
	for ( i = 0; i < count; i++ ) {
		Check( !strcmp( uiInfo.teamList[i].teamName, va( "T%02d", i ) ), "team name" );
		Check( !strcmp( uiInfo.teamList[i].imageName, va( "ui/assets/t%02d", i ) ), "team icon" );
		Check( !strcmp( uiInfo.teamList[i].teamMembers[TEAM_MEMBERS - 1], va( "M%02de", i ) ), "team member" );
		Check( uiInfo.teamList[i].cinematic == -1, "team cinematic" );
	}
}

/** More teams, characters or aliases than their arrays hold: the extra entries are
 * ignored with a warning, nothing past each array changes, and later sections load. */
static void TestTeamInfoLists( void ) {
	int i;

	/* 70 teams: teamList[64] overlaps numGameTypes and gameTypes[0]. */
	Reset();
	uiInfo.numGameTypes = SENTINEL;
	uiInfo.gameTypes[0].gtEnum = SENTINEL;
	AddTeams( MAX_TEAMS + 6 );
	Add( "characters {\n\t{ \"C00\" \"male\" }\n\t{ \"C01\" \"female\" }\n}\n" );
	Add( "aliases {\n\t{ \"A00\" \"C00\" \"a\" }\n}\n" );
	UI_ParseTeamInfo( "teaminfo.txt" );
	CheckTeams( MAX_TEAMS );
	Check( uiInfo.numGameTypes == SENTINEL && uiInfo.gameTypes[0].gameType == NULL
		&& uiInfo.gameTypes[0].gtEnum == SENTINEL, "memory after teamList" );
	Check( uiInfo.characterCount == 2 && !strcmp( uiInfo.characterList[1].base, "Janet" ), "characters after full teams" );
	Check( uiInfo.aliasCount == 1 && !strcmp( uiInfo.aliasList[0].ai, "C00" ), "aliases after full teams" );
	Check( ignoredWarnings == 6, "one warning per ignored team" );

	/* Exactly MAX_TEAMS teams fit without a warning. */
	Reset();
	AddTeams( MAX_TEAMS );
	UI_ParseTeamInfo( "teaminfo.txt" );
	CheckTeams( MAX_TEAMS );
	Check( ignoredWarnings == 0, "no warning for a full team list" );

	/* 70 characters: characterList[64] overlaps aliasCount and aliasList[0]. */
	Reset();
	Add( "characters {\n" );
	for ( i = 0; i < MAX_HEADS + 6; i++ ) {
		Add( "\t{ \"C%02d\" \"B%02d\" }\n", i, i );
	}
	Add( "}\naliases {\n\t{ \"A00\" \"C00\" \"d\" }\n}\n" );
	UI_ParseTeamInfo( "teaminfo.txt" );
	Check( uiInfo.characterCount == MAX_HEADS, "character count" );
	for ( i = 0; i < MAX_HEADS; i++ ) {
		Check( !strcmp( uiInfo.characterList[i].name, va( "C%02d", i ) ), "character name" );
		Check( !strcmp( uiInfo.characterList[i].base, va( "B%02d", i ) ), "character base" );
		Check( uiInfo.characterList[i].headImage == -1, "character head image" );
	}
	Check( uiInfo.aliasCount == 1 && !strcmp( uiInfo.aliasList[0].name, "A00" )
		&& !strcmp( uiInfo.aliasList[0].action, "d" ), "aliases after full characters" );
	Check( ignoredWarnings == 6, "one warning per ignored character" );

	/* 70 aliases: aliasList[64] overlaps teamCount and teamList[0]. */
	Reset();
	Add( "aliases {\n" );
	for ( i = 0; i < MAX_ALIASES + 6; i++ ) {
		Add( "\t{ \"A%02d\" \"C%02d\" \"o\" }\n", i, i );
	}
	Add( "}\n" );
	AddTeams( 1 );
	UI_ParseTeamInfo( "teaminfo.txt" );
	Check( uiInfo.aliasCount == MAX_ALIASES, "alias count" );
	for ( i = 0; i < MAX_ALIASES; i++ ) {
		Check( !strcmp( uiInfo.aliasList[i].name, va( "A%02d", i ) ), "alias name" );
		Check( !strcmp( uiInfo.aliasList[i].ai, va( "C%02d", i ) ), "alias ai" );
	}
	CheckTeams( 1 );
	Check( ignoredWarnings == 6, "one warning per ignored alias" );
}

/** More game types or maps than their arrays hold in gameinfo.txt. */
static void TestGameInfoLists( void ) {
	int i;

	/* joinGameTypes[16] overlaps redBlue, gameTypes[16] overlaps numJoinGameTypes and
	 * joinGameTypes[0], and mapList[128] overlaps tierCount. */
	Reset();
	uiInfo.redBlue = SENTINEL;
	uiInfo.tierCount = SENTINEL;
	Add( "joingametypes {\n" );
	for ( i = 0; i < MAX_GAMETYPES + 4; i++ ) {
		Add( "\t{ \"J%02d\" %d }\n", i, 100 + i );
	}
	Add( "}\ngametypes {\n" );
	for ( i = 0; i < MAX_GAMETYPES + 4; i++ ) {
		Add( "\t{ \"G%02d\" %d }\n", i, 200 + i );
	}
	Add( "}\nmaps {\n" );
	for ( i = 0; i < MAX_MAPS + 12; i++ ) {
		Add( "\t{ \"M%03d\" \"m%03d\" 2 \"T00\" 0 %d 3 %d }\n", i, i, 1000 + i, 3000 + i );
	}
	Add( "}\n" );
	UI_ParseGameInfo( "gameinfo.txt" );

	Check( uiInfo.numJoinGameTypes == MAX_GAMETYPES, "net game type count" );
	Check( uiInfo.numGameTypes == MAX_GAMETYPES, "game type count" );
	for ( i = 0; i < MAX_GAMETYPES; i++ ) {
		Check( !strcmp( uiInfo.joinGameTypes[i].gameType, va( "J%02d", i ) )
			&& uiInfo.joinGameTypes[i].gtEnum == 100 + i, "net game type" );
		Check( !strcmp( uiInfo.gameTypes[i].gameType, va( "G%02d", i ) )
			&& uiInfo.gameTypes[i].gtEnum == 200 + i, "game type" );
	}
	Check( uiInfo.redBlue == SENTINEL, "memory after joinGameTypes" );
	Check( uiInfo.mapCount == MAX_MAPS, "map count" );
	for ( i = 0; i < MAX_MAPS; i++ ) {
		Check( !strcmp( uiInfo.mapList[i].mapName, va( "M%03d", i ) ), "map name" );
		Check( !strcmp( uiInfo.mapList[i].mapLoadName, va( "m%03d", i ) ), "map load name" );
		Check( uiInfo.mapList[i].typeBits == ( ( 1 << 0 ) | ( 1 << 3 ) ), "map game types" );
		Check( uiInfo.mapList[i].timeToBeat[0] == 1000 + i && uiInfo.mapList[i].timeToBeat[3] == 3000 + i, "map times" );
	}
	Check( uiInfo.tierCount == SENTINEL, "memory after mapList" );
	Check( ignoredWarnings == 4 + 4 + 12, "one warning per ignored game type or map" );
}

/** Whether head C<head> is listed for team when stored teams were parsed. A head is
 * listed for teams 0-31 it has a skin for, as in retail. Teams 32 and up list no
 * head, the retail PowerPC result of 1 << 32..63. Team stored stands for a name that
 * is no team, which selects team 0. */
static int Listed( int head, int team, int stored ) {
	if ( team == stored ) {
		team = 0;
	}
	return team < 32 && HasSkin( head, team );
}

/** Build the head list for every team of a teaminfo.txt with teams teams through the
 * real UI_ParseTeamInfo and UI_HeadCountByTeam. */
static void TestHeadMask( int teams ) {
	int stored = teams < MAX_TEAMS ? teams : MAX_TEAMS;
	int team, i, expected;

	Reset();
	AddTeams( teams );
	Add( "characters {\n" );
	for ( i = 0; i < HEADS; i++ ) {
		Add( "\t{ \"C%d\" \"B%d\" }\n", i, i );
	}
	/* aliases for a member of teams 0, 9, ... 63, so the alias path runs too */
	Add( "}\naliases {\n" );
	for ( i = 0; i < HEADS; i++ ) {
		Add( "\t{ \"M%02da\" \"C%d\" \"a\" }\n", i * 9, i );
	}
	Add( "}\n" );
	UI_ParseTeamInfo( "teaminfo.txt" );
	CheckTeams( stored );
	Check( uiInfo.characterCount == HEADS && uiInfo.aliasCount == HEADS, "heads and aliases" );
	Check( ignoredWarnings == teams - stored, "one warning per ignored team" );
	for ( i = 0; i < HEADS; i++ ) {
		uiInfo.characterList[i].headImage = 1;
	}

	for ( team = 0; team <= stored; team++ ) {
		Q_strncpyz( teamNameCvar, team < stored ? va( "T%02d", team ) : "none", sizeof( teamNameCvar ) );
		expected = 0;
		for ( i = 0; i < HEADS; i++ ) {
			expected += Listed( i, team, stored );
		}
		Check( UI_HeadCountByTeam() == expected, "head count for team" );
		for ( i = 0; i < HEADS; i++ ) {
			Check( uiInfo.characterList[i].active == ( Listed( i, team, stored ) ? qtrue : qfalse ), "head listed for team" );
		}
	}
}

/** Run one scenario per process: UI_HeadCountByTeam builds its mask only once. */
int main( int argc, char **argv ) {
	if ( argc == 2 && !strcmp( argv[1], "lists" ) ) {
		TestTeamInfoLists();
		TestGameInfoLists();
		puts( "UI teaminfo/gameinfo list bounds regressions passed (issue #357)" );
		return 0;
	}
	if ( argc == 3 && !strcmp( argv[1], "heads" ) ) {
		TestHeadMask( atoi( argv[2] ) );
		printf( "UI head team mask regression passed for %s teams (issue #344)\n", argv[2] );
		return 0;
	}
	fprintf( stderr, "usage: %s lists | heads <teams>\n", argv[0] );
	return 2;
}
