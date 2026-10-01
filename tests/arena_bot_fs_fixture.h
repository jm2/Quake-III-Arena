/* Issue #440: the module filesystem the arena and bot loaders of the Team Arena
 * UI, the base q3_ui and the game run against. It lists the .arena and .bot
 * names in scripts as FS_GetFileList packs them, serves each file's text and
 * records every path a loader opens. A pk3 entry name is up to 255 bytes, so the
 * lists hold names of 119 bytes (the longest that fits after "scripts/" in the
 * loaders' 128-byte filename), 120 and 250 bytes between normal names.
 * Include it after the module source. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FIXTURE_MAX_FILES 16
#define FIXTURE_MAX_OPENS 16
#define FIXTURE_PATH_SIZE 300

typedef struct {
	char path[FIXTURE_PATH_SIZE];
	const char *text;
} fixtureFile_t;

static fixtureFile_t fixtureFiles[FIXTURE_MAX_FILES];
static int fixtureFileCount;
static const char *fixtureArenaList[FIXTURE_MAX_FILES];
static int fixtureArenaCount;
static const char *fixtureBotList[FIXTURE_MAX_FILES];
static int fixtureBotCount;
static char fixtureOpened[FIXTURE_MAX_OPENS][FIXTURE_PATH_SIZE];
static int fixtureOpenCount;
static int fixtureOpenFile = -1;
static int fixtureTooLongReports;

/** Fail with a description of the first mismatch. */
static void Check( int ok, const char *what ) {
	if ( !ok ) {
		fprintf( stderr, "arena and bot file name regression failed: %s\n", what );
		exit( 1 );
	}
}

/** Serve text as the file at path. */
static void FixtureAddFile( const char *path, const char *text ) {
	Check( fixtureFileCount < FIXTURE_MAX_FILES && strlen( path ) < FIXTURE_PATH_SIZE, "fixture file fits" );
	strcpy( fixtureFiles[fixtureFileCount].path, path );
	fixtureFiles[fixtureFileCount].text = text;
	fixtureFileCount++;
}

/** FS_GetFileList: the listed .arena or .bot names in scripts, each
 * NUL-terminated, packed from the start of listbuf. */
int trap_FS_GetFileList( const char *path, const char *extension, char *listbuf, int bufsize ) {
	const char *const *names;
	int i, count, len, used;

	Check( !strcmp( path, "scripts" ), "listed directory" );
	if ( !strcmp( extension, ".arena" ) ) {
		names = fixtureArenaList;
		count = fixtureArenaCount;
	} else {
		Check( !strcmp( extension, ".bot" ), "listed extension" );
		names = fixtureBotList;
		count = fixtureBotCount;
	}
	used = 0;
	for ( i = 0; i < count; i++ ) {
		len = strlen( names[i] ) + 1;
		Check( used + len < bufsize, "listed names fit" );
		memcpy( listbuf + used, names[i], len );
		used += len;
	}
	return count;
}

/** FS_FOpenFile: record the path and open the file served there, if any. */
int trap_FS_FOpenFile( const char *qpath, fileHandle_t *f, fsMode_t mode ) {
	int i;

	Check( mode == FS_READ, "files are read" );
	Check( fixtureOpenFile < 0, "one file open at a time" );
	Check( fixtureOpenCount < FIXTURE_MAX_OPENS && strlen( qpath ) < FIXTURE_PATH_SIZE, "opened path fits the record" );
	strcpy( fixtureOpened[fixtureOpenCount++], qpath );
	for ( i = 0; i < fixtureFileCount; i++ ) {
		if ( !strcmp( fixtureFiles[i].path, qpath ) ) {
			fixtureOpenFile = i;
			*f = i + 1;
			return strlen( fixtureFiles[i].text );
		}
	}
	*f = 0;
	return -1;
}

/** FS_Read: the open file's text. */
void trap_FS_Read( void *buffer, int len, fileHandle_t f ) {
	Check( fixtureOpenFile >= 0 && f == fixtureOpenFile + 1, "read an open file" );
	Check( len == (int)strlen( fixtureFiles[fixtureOpenFile].text ), "read the whole file" );
	memcpy( buffer, fixtureFiles[fixtureOpenFile].text, len );
}

/** FS_FCloseFile: close the open file. */
void trap_FS_FCloseFile( fileHandle_t f ) {
	Check( fixtureOpenFile >= 0 && f == fixtureOpenFile + 1, "close an open file" );
	fixtureOpenFile = -1;
}

/** g_arenasFile and g_botsFile are empty, so scripts/arenas.txt and
 * scripts/bots.txt load first. */
void trap_Cvar_Register( vmCvar_t *vmCvar, const char *varName, const char *defaultValue, int flags ) {
	(void)defaultValue; (void)flags;
	Check( !strcmp( varName, "g_arenasFile" ) || !strcmp( varName, "g_botsFile" ), "registered cvar" );
	memset( vmCvar, 0, sizeof( *vmCvar ) );
}

/** The module's console print: count the names reported as too long. */
static void FixturePrint( const char *text ) {
	if ( strstr( text, "file name too long: scripts/" ) ) {
		fixtureTooLongReports++;
	}
}

/** Fail on engine errors. */
void QDECL Com_Error( int level, const char *error, ... ) {
	(void)level;
	fprintf( stderr, "Unexpected Com_Error: %s\n", error );
	exit( 1 );
}
/** The served files parse and every formatted name fits, so nothing reports a
 * malformed info file or a Com_sprintf overflow. */
void QDECL Com_Printf( const char *msg, ... ) {
	fprintf( stderr, "Unexpected Com_Printf: %s", msg );
	exit( 1 );
}

/* The served names: a normal name, the three long ones, another normal name. */
static char fixtureArena119[120], fixtureArena120[121], fixtureArena250[251];
static char fixtureBot119[120], fixtureBot120[121], fixtureBot250[251];

/** Fill name with length bytes that end in extension. */
static void FixtureName( char *name, int length, const char *extension ) {
	int stem = length - (int)strlen( extension );

	memset( name, 'n', stem );
	strcpy( name + stem, extension );
	Check( (int)strlen( name ) == length, "fixture name length" );
}

/** Serve name in scripts/ with text. */
static void FixtureAddScript( const char *name, const char *text ) {
	char path[FIXTURE_PATH_SIZE];

	Check( strlen( "scripts/" ) + strlen( name ) < sizeof( path ), "fixture path fits" );
	strcpy( path, "scripts/" );
	strcat( path, name );
	FixtureAddFile( path, text );
}

/** Serve arenas.txt, bots.txt and the listed .arena and .bot files; every file
 * holds one info with its own map or name. */
static void FixtureServe( void ) {
	FixtureName( fixtureArena119, 119, ".arena" );
	FixtureName( fixtureArena120, 120, ".arena" );
	FixtureName( fixtureArena250, 250, ".arena" );
	FixtureName( fixtureBot119, 119, ".bot" );
	FixtureName( fixtureBot120, 120, ".bot" );
	FixtureName( fixtureBot250, 250, ".bot" );

	fixtureFileCount = 0;
	FixtureAddFile( "scripts/arenas.txt", "{\nmap \"q3dm1\"\nlongname \"Arena Gate\"\ntype \"ffa tourney\"\n}\n" );
	FixtureAddScript( "first.arena", "{ map \"first\" longname \"First\" type \"ffa\" }" );
	FixtureAddScript( fixtureArena119, "{ map \"fits\" longname \"Fits\" type \"ctf\" }" );
	FixtureAddScript( fixtureArena120, "{ map \"long120\" longname \"Long 120\" type \"ffa\" }" );
	FixtureAddScript( fixtureArena250, "{ map \"long250\" longname \"Long 250\" type \"ffa\" }" );
	FixtureAddScript( "last.arena", "{ map \"last\" longname \"Last\" type \"tourney\" }" );
	FixtureAddFile( "scripts/bots.txt", "{\nname Sarge\nmodel sarge\naifile bots/sarge_c.c\n}\n" );
	FixtureAddScript( "first.bot", "{ name First model visor aifile bots/default_c.c }" );
	FixtureAddScript( fixtureBot119, "{ name Fits model visor aifile bots/default_c.c }" );
	FixtureAddScript( fixtureBot120, "{ name Long120 model visor aifile bots/default_c.c }" );
	FixtureAddScript( fixtureBot250, "{ name Long250 model visor aifile bots/default_c.c }" );
	FixtureAddScript( "last.bot", "{ name Last model visor aifile bots/default_c.c }" );

	fixtureArenaCount = 0;
	fixtureArenaList[fixtureArenaCount++] = "first.arena";
	fixtureArenaList[fixtureArenaCount++] = fixtureArena119;
	fixtureArenaList[fixtureArenaCount++] = fixtureArena120;
	fixtureArenaList[fixtureArenaCount++] = fixtureArena250;
	fixtureArenaList[fixtureArenaCount++] = "last.arena";
	fixtureBotCount = 0;
	fixtureBotList[fixtureBotCount++] = "first.bot";
	fixtureBotList[fixtureBotCount++] = fixtureBot119;
	fixtureBotList[fixtureBotCount++] = fixtureBot120;
	fixtureBotList[fixtureBotCount++] = fixtureBot250;
	fixtureBotList[fixtureBotCount++] = "last.bot";
}

/** Forget the opened paths and reports before running a loader. */
static void FixtureBeginLoad( void ) {
	fixtureOpenCount = 0;
	fixtureTooLongReports = 0;
	fixtureOpenFile = -1;
}

/** The loader opened scripts/<first>, scripts/<fits> and scripts/<last> after
 * txt, in list order, and reported the two names too long for filename. */
static void FixtureCheckOpened( const char *txt, const char *first, const char *fits, const char *last, const char *what ) {
	char expected[4][FIXTURE_PATH_SIZE];
	int i;

	strcpy( expected[0], txt );
	strcpy( expected[1], "scripts/" );
	strcat( expected[1], first );
	strcpy( expected[2], "scripts/" );
	strcat( expected[2], fits );
	strcpy( expected[3], "scripts/" );
	strcat( expected[3], last );
	for ( i = 0; i < fixtureOpenCount; i++ ) {
		if ( i >= 4 || strcmp( fixtureOpened[i], expected[i] ) ) {
			fprintf( stderr, "%s: open %d is \"%s\" (%d bytes)\n", what, i, fixtureOpened[i], (int)strlen( fixtureOpened[i] ) );
			Check( 0, what );
		}
	}
	Check( fixtureOpenCount == 4, what );
	Check( fixtureOpenFile < 0, "every opened file is closed" );
	Check( fixtureTooLongReports == 2, "both names too long for filename are reported" );
}

/** The loaded infos hold key values expected[0..count), in order. */
static void FixtureCheckInfos( char **infos, int loaded, const char *key, const char *const *expected, int count, const char *what ) {
	int i;

	Check( loaded == count, what );
	for ( i = 0; i < count; i++ ) {
		if ( strcmp( Info_ValueForKey( infos[i], key ), expected[i] ) ) {
			fprintf( stderr, "%s %d: %s \"%s\", expected \"%s\"\n", what, i, key, Info_ValueForKey( infos[i], key ), expected[i] );
			Check( 0, what );
		}
	}
}

/* The maps and bot names the loaders keep: arenas.txt or bots.txt, then the
 * listed files whose names fit, in list order. */
static const char *const fixtureArenaMaps[] = { "q3dm1", "first", "fits", "last" };
static const char *const fixtureBotNames[] = { "Sarge", "First", "Fits", "Last" };
#define FIXTURE_COUNT( a ) ( (int)( sizeof( a ) / sizeof( ( a )[0] ) ) )
