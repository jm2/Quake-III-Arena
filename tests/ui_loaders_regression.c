/* Issue #12: the Team Arena UI's _UI_Init skipped UI_BuildQ3Model_List and
 * UI_LoadBots ("SKIPPED: Causes freeze", from the 1690188f bring-up), so the
 * player setup head list and the add-bot list were empty.
 *
 * run_ui_loaders_tests.sh builds the real Team Arena ui module (the code/ui
 * sources but ui_syscalls.c, with Q3_STATIC, UI_MODULE and MISSIONPACK, as
 * CMakeLists.txt builds ui_mp_obj), a weak default for every trap of
 * ui_syscalls.c, and this file twice:
 *   -DQ3_TEST_ENGINE  the real files.c over a pk3 install, behind engine stubs
 *   (module flags)    the traps _UI_Init uses, served by that filesystem, and
 *                     main, which runs the real _UI_Init once
 * After _UI_Init the head list must hold exactly the heads retail's rules find
 * (icon_<skin>.tga in each models/players directory, but not icon_red or
 * icon_blue; icon_default is the bare model), each registered as a shader,
 * and the bot list exactly the bots of scripts/bots.txt and the .bot scripts,
 * with no UI memory or string pool overflow, no info file error, and within
 * the time and zone budgets below.
 *
 * usage: ui_loaders_regression <install> <heads> <bots>
 *   heads, bots: a comma-separated list of the names expected (any order),
 *   or "#<n>" for a count alone, as when the list stops at MAX_PLAYERMODELS. */
#ifdef Q3_TEST_ENGINE
/* The demo's pak0.pk3 has no productid.txt; restricted mode is not this test's business. */
#define PRE_RELEASE_DEMO
#include <dirent.h>
#include "../code/qcommon/files.c"

#define ZONE_BUDGET ( 16 * 1024 * 1024 )	/* the target's default com_zoneMegs */

qboolean com_fullyInitialized;
cvar_t *com_journal;
fileHandle_t com_journalDataFile;
static cvar_t *cvars[32];
static int numCvars;
static char install[MAX_OSPATH];
static long zoneLive, zonePeak;
static char printed[1 << 16];

static void EngineCheck( int ok, const char *what ) {
	if ( !ok ) {
		fprintf( stderr, "UI loaders regression failed (engine): %s\n", what );
		exit( 1 );
	}
}

/* Everything the engine and the module print, for FixturePrinted. */
void QDECL Com_Printf( const char *format, ... ) {
	size_t used = strlen( printed );
	va_list args;

	va_start( args, format );
	if ( used < sizeof( printed ) - 1 ) {
		vsnprintf( printed + used, sizeof( printed ) - used, format, args );
	}
	va_end( args );
}
void QDECL Com_DPrintf( const char *format, ... ) { (void)format; }
void QDECL Com_FlightRecord( const char *format, ... ) { (void)format; }
void QDECL Com_Error( int level, const char *format, ... ) {
	va_list args;

	va_start( args, format );
	fprintf( stderr, "Com_Error %d: ", level );
	vfprintf( stderr, format, args );
	va_end( args );
	EngineCheck( 0, "unexpected engine error" );
}
const char *FixturePrinted( void ) { return printed; }
long FixtureZonePeak( void ) { return zonePeak; }

void Com_Memset( void *out, const int value, const size_t size ) { memset( out, value, size ); }
void Com_Memcpy( void *out, const void *in, const size_t size ) { memcpy( out, in, size ); }
/* Z_Malloc with a size header, so the live total and its peak are known. */
void *Z_Malloc( int size ) {
	long *p;

	EngineCheck( size >= 0, "non-negative zone request" );
	p = calloc( 1, sizeof( long ) * 2 + size );
	EngineCheck( p != NULL, "zone allocation" );
	p[0] = size;
	zoneLive += size;
	if ( zoneLive > zonePeak ) {
		zonePeak = zoneLive;
	}
	EngineCheck( zoneLive <= ZONE_BUDGET, "the filesystem stays within the zone budget" );
	return p + 2;
}
void Z_Free( void *ptr ) {
	long *p = (long *)ptr - 2;

	EngineCheck( ptr != NULL, "zone free of NULL" );
	zoneLive -= p[0];
	free( p );
}
char *CopyString( const char *in ) {
	char *out = Z_Malloc( (int)strlen( in ) + 1 );
	strcpy( out, in );
	return out;
}
void *Hunk_AllocateTempMemory( int size ) { return malloc( size ? size : 1 ); }
void Hunk_FreeTempMemory( void *p ) { free( p ); }
void Hunk_ClearTempMemory( void ) {}
int Com_FilterPath( char *filter, char *name, int casesensitive ) {
	(void)filter; (void)name; (void)casesensitive;
	EngineCheck( 0, "unexpected filtered list" );
	return 0;
}
void Com_ReadCDKey( const char *filename ) { (void)filename; }
void Com_AppendCDKey( const char *filename ) { (void)filename; }
void Com_StartupVariable( const char *match ) { (void)match; }
void S_ClearSoundBuffer( void ) {}
void Sys_BeginStreamedFile( fileHandle_t f, int readAhead ) { (void)f; (void)readAhead; }
void Sys_EndStreamedFile( fileHandle_t f ) { (void)f; }
int Sys_StreamedRead( void *buffer, int size, int count, fileHandle_t f ) {
	return FS_Read( buffer, size * count, f );
}
void Sys_Mkdir( const char *path ) { (void)path; }
char *Sys_DefaultCDPath( void ) { return ""; }
char *Sys_DefaultInstallPath( void ) { return install; }
char *Sys_DefaultHomePath( void ) { return ""; }
/* Only the install's game directories hold files: list their pk3s (one each,
 * so paksort's order is moot); every other directory is empty. */
char **Sys_ListFiles( const char *directory, const char *extension, char *filter, int *numfiles, qboolean wantsubs ) {
	char **list = Z_Malloc( 4 * sizeof( *list ) );
	struct dirent *entry;
	DIR *dir;
	size_t n;

	(void)wantsubs;
	EngineCheck( !filter, "no filtered scan" );
	*numfiles = 0;
	dir = !Q_stricmp( extension, ".pk3" ) ? opendir( directory ) : NULL;
	while ( dir && ( entry = readdir( dir ) ) != NULL ) {
		n = strlen( entry->d_name );
		if ( n > 4 && !strcmp( entry->d_name + n - 4, ".pk3" ) ) {
			EngineCheck( *numfiles < 3, "bounded pk3 list" );
			list[( *numfiles )++] = CopyString( entry->d_name );
		}
	}
	if ( dir ) {
		closedir( dir );
	}
	return list;
}
void Sys_FreeFileList( char **list ) {
	int i;

	for ( i = 0; list && list[i]; i++ ) {
		Z_Free( list[i] );
	}
	if ( list ) {
		Z_Free( list );
	}
}
void Cmd_AddCommand( const char *name, xcommand_t function ) { (void)name; (void)function; }
void Cmd_RemoveCommand( const char *name ) { (void)name; }
int Cmd_Argc( void ) { return 0; }
char *Cmd_Argv( int arg ) { (void)arg; return ""; }
void Cmd_TokenizeString( const char *text ) { (void)text; }
qboolean Com_SafeMode( void ) { return qfalse; }

static cvar_t *FindCvar( const char *name, const char *value ) {
	cvar_t *var;
	int i;

	for ( i = 0; i < numCvars; i++ ) {
		if ( !strcmp( cvars[i]->name, name ) ) {
			return cvars[i];
		}
	}
	EngineCheck( numCvars < (int)( sizeof( cvars ) / sizeof( cvars[0] ) ), "bounded cvars" );
	var = calloc( 1, sizeof( *var ) );
	EngineCheck( var != NULL, "cvar allocation" );
	var->name = strdup( name );
	var->string = strdup( value );
	var->integer = atoi( value );
	cvars[numCvars++] = var;
	return var;
}
cvar_t *Cvar_Get( const char *name, const char *value, int flags ) {
	cvar_t *var = FindCvar( name, value );
	var->flags |= flags;
	return var;
}
void Cvar_Set( const char *name, const char *value ) {
	cvar_t *var = FindCvar( name, value );
	free( var->string );
	var->string = strdup( value );
	var->integer = atoi( value );
	var->modified = qtrue;
}

/** Start the real filesystem over <dir>/baseq3. */
void FixtureMount( const char *dir ) {
	Q_strncpyz( install, dir, sizeof( install ) );
	FS_InitFilesystem();
	EngineCheck( !strcmp( fs_gamedir, BASEGAME ), "the install mounts baseq3" );
}
void FixtureUnmount( void ) {
	FS_Shutdown( qtrue );
	EngineCheck( zoneLive == 0, "the filesystem releases its zone" );
}

#else /* the module side */

#include "../code/ui/ui_local.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/time.h>

#define TIME_BUDGET_MSEC 30000			/* the whole _UI_Init, under the sanitizers */
#define RSS_BUDGET_KB ( 1024L * 1024 )	/* the process, sanitizer shadow included */
#define MAX_SHADERS 1024

/* The real engine filesystem (the -DQ3_TEST_ENGINE object), as cl_ui.c calls it. */
void FixtureMount( const char *dir );
void FixtureUnmount( void );
const char *FixturePrinted( void );
long FixtureZonePeak( void );
int FS_GetFileList( const char *path, const char *extension, char *listbuf, int bufsize );
int FS_FOpenFileByMode( const char *qpath, fileHandle_t *f, fsMode_t mode );
int FS_Read2( void *buffer, int len, fileHandle_t f );
void FS_FCloseFile( fileHandle_t f );

extern int ui_numBots;	/* ui_gameinfo.c */
void _UI_Init( qboolean inGameLoad );	/* ui_main.c, which UI_vmMain( UI_INIT ) calls */

static char shaders[MAX_SHADERS][MAX_QPATH];
static int numShaders, fileLists;

static void Check( int ok, const char *what ) {
	if ( !ok ) {
		fprintf( stderr, "UI loaders regression failed: %s\n", what );
		exit( 1 );
	}
}

/* Any other trap reached returns 0, as an unregistered handle or empty value. */
void Q3T_DefaultTrap( const char *name ) { (void)name; }

void trap_Print( const char *string ) { Com_Printf( "%s", string ); }
void trap_Error( const char *string ) {
	fprintf( stderr, "trap_Error: %s\n", string );
	Check( 0, "the ui module raised an error" );
}
int trap_Milliseconds( void ) {
	struct timeval now;
	gettimeofday( &now, NULL );
	return (int)( now.tv_sec % 1000000 * 1000 + now.tv_usec / 1000 );
}
/* Every cvar keeps its default: g_botsFile is empty, so bots come from scripts/bots.txt. */
void trap_Cvar_Register( vmCvar_t *cvar, const char *var_name, const char *value, int flags ) {
	(void)var_name; (void)flags;
	if ( cvar ) {
		memset( cvar, 0, sizeof( *cvar ) );
		Q_strncpyz( cvar->string, value, sizeof( cvar->string ) );
		cvar->value = atof( value );
		cvar->integer = atoi( value );
	}
}
void trap_Cvar_VariableStringBuffer( const char *var_name, char *buffer, int bufsize ) {
	(void)var_name;
	if ( bufsize > 0 ) {
		buffer[0] = 0;
	}
}
void trap_GetGlconfig( glconfig_t *glconfig ) {
	memset( glconfig, 0, sizeof( *glconfig ) );
	glconfig->vidWidth = 640;
	glconfig->vidHeight = 480;
}
/* The menus are not this test's business: menus.txt is an empty script. */
int trap_PC_LoadSource( const char *filename ) { (void)filename; return 1; }
int trap_PC_FreeSource( int handle ) { (void)handle; return 0; }
int trap_PC_ReadToken( int handle, pc_token_t *pc_token ) {
	(void)handle;
	memset( pc_token, 0, sizeof( *pc_token ) );
	return 0;
}
/* The UI_FS_* traps, as CL_UISystemCalls serves them. */
int trap_FS_FOpenFile( const char *qpath, fileHandle_t *f, fsMode_t mode ) {
	return FS_FOpenFileByMode( qpath, f, mode );
}
void trap_FS_Read( void *buffer, int len, fileHandle_t f ) { FS_Read2( buffer, len, f ); }
void trap_FS_FCloseFile( fileHandle_t f ) { FS_FCloseFile( f ); }
int trap_FS_GetFileList( const char *path, const char *extension, char *listbuf, int bufsize ) {
	fileLists++;
	return FS_GetFileList( path, extension, listbuf, bufsize );
}
/* Each shader name gets its own handle, so a head icon can be traced back. */
qhandle_t trap_R_RegisterShaderNoMip( const char *name ) {
	int i;

	for ( i = 0; i < numShaders; i++ ) {
		if ( !Q_stricmp( shaders[i], name ) ) {
			return i + 1;
		}
	}
	Check( numShaders < MAX_SHADERS && strlen( name ) < MAX_QPATH, "bounded shader registrations" );
	Q_strncpyz( shaders[numShaders], name, MAX_QPATH );
	return ++numShaders;
}

/** Whether a comma-separated list holds name (case-insensitively). */
static int Listed( const char *list, const char *name ) {
	size_t n = strlen( name );
	const char *s;

	for ( s = list; *s; s = strchr( s, ',' ) ? strchr( s, ',' ) + 1 : s + strlen( s ) ) {
		if ( !Q_stricmpn( s, name, (int)n ) && ( s[n] == ',' || !s[n] ) ) {
			return 1;
		}
	}
	return 0;
}
static int Entries( const char *list ) {
	int n = *list ? 1 : 0;

	for ( ; *list; list++ ) {
		n += *list == ',';
	}
	return n;
}
/** found[0..count) must be the expected list, or count the expected "#n". */
static void CheckNames( const char *what, const char *expected, const char **found, int count ) {
	int i, j;

	if ( expected[0] == '#' ) {
		if ( count != atoi( expected + 1 ) ) {
			fprintf( stderr, "%s: %d found, expected %s\n", what, count, expected + 1 );
			Check( 0, what );
		}
		return;
	}
	if ( count != Entries( expected ) ) {
		fprintf( stderr, "%s: %d found, expected %d (%s)\n", what, count, Entries( expected ), expected );
		for ( i = 0; i < count; i++ ) {
			fprintf( stderr, "  found %s\n", found[i] );
		}
		Check( 0, what );
	}
	for ( i = 0; i < count; i++ ) {
		if ( !Listed( expected, found[i] ) ) {
			fprintf( stderr, "%s: unexpected \"%s\"\n", what, found[i] );
			Check( 0, what );
		}
		for ( j = 0; j < i; j++ ) {
			if ( !Q_stricmp( found[i], found[j] ) ) {
				fprintf( stderr, "%s: \"%s\" is listed at %d and %d\n", what, found[i], j, i );
				Check( 0, "no name is listed twice" );
			}
		}
	}
}

/** Each head's icon is models/players/<model>/icon_<skin or default>, registered once. */
static void CheckHeadIcons( void ) {
	char icon[MAX_QPATH];
	const char *name, *slash;
	int i;

	for ( i = 0; i < uiInfo.q3HeadCount; i++ ) {
		name = uiInfo.q3HeadNames[i];
		slash = strchr( name, '/' );
		if ( slash ) {
			Com_sprintf( icon, sizeof( icon ), "models/players/%.*s/icon_%s", (int)( slash - name ), name, slash + 1 );
		} else {
			Com_sprintf( icon, sizeof( icon ), "models/players/%s/icon_default", name );
		}
		Check( uiInfo.q3HeadIcons[i] > 0 && uiInfo.q3HeadIcons[i] <= numShaders &&
			!Q_stricmp( shaders[uiInfo.q3HeadIcons[i] - 1], icon ), "each head registers its own icon" );
	}
}

/** String_Report's "<n> bytes out of <m> used" for the named pool. */
static void CheckPool( const char *pool ) {
	const char *line = strstr( FixturePrinted(), pool );
	int used, size;

	Check( line && sscanf( strstr( line, "full, " ) + 6, "%d bytes out of %d used", &used, &size ) == 2,
		"String_Report reports the pool" );
	printf( "  %s %d of %d bytes\n", pool, used, size );
	Check( used >= 0 && used < size, "the pool has room left" );
}

int main( int argc, char **argv ) {
	static const char *const errors[] = {
		"Max infos exceeded", "file too large", "Missing { in info file", "Unexpected end of info file",
		"UI_Alloc: Failure", "file not found: scripts/", "file name too long"
	};
	static char botNames[MAX_BOTS][MAX_QPATH];
	const char *heads[MAX_PLAYERMODELS], *bots[MAX_BOTS];
	struct rusage usage;
	int i, start, elapsed;

	Check( argc == 4, "usage: ui_loaders_regression install heads bots" );
	FixtureMount( argv[1] );
	start = trap_Milliseconds();
	_UI_Init( qfalse );
	elapsed = trap_Milliseconds() - start;

	for ( i = 0; i < uiInfo.q3HeadCount; i++ ) {
		heads[i] = uiInfo.q3HeadNames[i];
	}
	CheckNames( "heads", argv[2], heads, uiInfo.q3HeadCount );
	CheckHeadIcons();
	Check( ui_numBots <= MAX_BOTS, "bounded bot list" );
	for ( i = 0; i < ui_numBots; i++ ) {
		Check( UI_GetBotInfoByNumber( i ) != NULL, "each bot has its info" );
		/* Info_ValueForKey returns one of two static buffers */
		Q_strncpyz( botNames[i], Info_ValueForKey( UI_GetBotInfoByNumber( i ), "name" ), MAX_QPATH );
		bots[i] = botNames[i];
	}
	CheckNames( "bots", argv[3], bots, ui_numBots );
	Check( UI_GetNumBots() == ui_numBots, "UI_GetNumBots matches the list" );

	Check( !UI_OutOfMemory(), "the UI memory pool did not overflow" );
	String_Report();
	CheckPool( "String Pool" );
	CheckPool( "Memory Pool" );
	for ( i = 0; i < (int)( sizeof( errors ) / sizeof( errors[0] ) ); i++ ) {
		if ( strstr( FixturePrinted(), errors[i] ) ) {
			fprintf( stderr, "%s", FixturePrinted() );
			Check( 0, errors[i] );
		}
	}
	Check( elapsed >= 0 && elapsed < TIME_BUDGET_MSEC, "_UI_Init finishes within the time budget" );
	Check( !getrusage( RUSAGE_SELF, &usage ) && usage.ru_maxrss < RSS_BUDGET_KB,
		"the process stays within the memory budget" );
	FixtureUnmount();
	printf( "UI loaders: %d heads, %d bots, %d file lists, %d shaders, zone peak %ld bytes, %d ms (issue #12)\n",
		uiInfo.q3HeadCount, ui_numBots, fileLists, numShaders, FixtureZonePeak(), elapsed );
	return 0;
}
#endif
