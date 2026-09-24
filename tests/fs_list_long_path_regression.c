/* Issue #398: FS_ReturnPath strcpy'd the caller's list path into its 256-byte
 * zpath stack buffer. The path reaches FS_ListFilteredFiles unbounded from
 * trap_FS_GetFileList in the UI and game modules (interpreted mod QVMs too)
 * and from the "dir" console command, so a 400-character path wrote 144 bytes
 * past zpath.
 *
 * run_fs_list_long_path_tests.sh builds this file three ways around the real
 * files.c and unzip.c:
 *   (default)       FS_GetFileList, FS_ListFiles, FS_ListFilteredFiles, dir
 *   -DQ3_TEST_UI    the real CL_UISystemCalls UI_FS_GETFILELIST trap
 *   -DQ3_TEST_GAME  the real SV_GameSystemCalls G_FS_GETFILELIST trap
 * Each lists paths of 255, 256, 400 and 4096 characters, and each path's loose
 * directory holds hit.cfg. 255 characters still lists it. 256 and longer list
 * nothing and never reach the directory scan; through a trap they never reach
 * FS_GetFileList. The pk3 has names of up to 255 characters, and listings of
 * normal and long legal paths stay as before. The default build also calls
 * FS_ReturnPath itself with those paths: its copy stays inside zpath even
 * without the refusal in front of it. */
#if defined(Q3_TEST_UI)
#include "../code/client/cl_ui.c"
#elif defined(Q3_TEST_GAME)
#include "../code/server/sv_game.c"
#include "../code/qcommon/vm_local.h"
#endif
/* Renamed so the fixture's FS_GetFileList can count what a trap forwards. */
#define FS_GetFileList Real_FS_GetFileList
#include "../code/qcommon/files.c"
#undef FS_GetFileList
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NUM_LENGTHS 4
static const int lengths[NUM_LENGTHS] = { 255, 256, 400, 4096 };
static char *longPaths[NUM_LENGTHS], *nearDir, *edgeDir;
static char base[MAX_OSPATH], servedDir[8192];
static const char *servedPath;
static int zoneLive, scans, forwarded;
static cvar_t zeroVar;
static searchpath_t packPath, dirPath;
static directory_t dirEntry;
qboolean com_fullyInitialized;
cvar_t *com_journal;
fileHandle_t com_journalDataFile;

static void Expect( int ok, const char *message ) {
	if ( !ok ) {
		fprintf( stderr, "FS list long path regression failed: %s\n", message );
		exit( 1 );
	}
}

/** Count the list calls that reach the filesystem, then list for real. */
int FS_GetFileList( const char *path, const char *extension, char *listbuf, int bufsize ) {
	forwarded++;
	return Real_FS_GetFileList( path, extension, listbuf, bufsize );
}

/* Filesystem imports. */
void *Z_Malloc( int size ) {
	void *p = calloc( 1, size ? size : 1 );
	Expect( p != NULL, "zone allocation" );
	zoneLive++;
	return p;
}
void Z_Free( void *p ) {
	Expect( p != NULL, "zone free of NULL" );
	zoneLive--;
	free( p );
}
char *CopyString( const char *in ) {
	char *out = Z_Malloc( strlen( in ) + 1 );
	strcpy( out, in );
	return out;
}
void *Hunk_AllocateTempMemory( int size ) { return malloc( size ? size : 1 ); }
void Hunk_FreeTempMemory( void *p ) { free( p ); }
void QDECL Com_FlightRecord( const char *format, ... ) { (void)format; }
/* Kept by the linker for the file traps and reads; listing never opens a file. */
void S_ClearSoundBuffer( void ) { Expect( 0, "S_ClearSoundBuffer" ); }
void Sys_BeginStreamedFile( fileHandle_t f, int readahead ) { Expect( 0, "Sys_BeginStreamedFile" ); }
void Sys_EndStreamedFile( fileHandle_t f ) { (void)f; Expect( 0, "Sys_EndStreamedFile" ); }
int Sys_StreamedRead( void *buffer, int size, int count, fileHandle_t f ) { Expect( 0, "Sys_StreamedRead" ); return 0; }
void Sys_StreamSeek( fileHandle_t f, int offset, int origin ) { Expect( 0, "Sys_StreamSeek" ); }
void Sys_Mkdir( const char *path ) { Expect( 0, "Sys_Mkdir" ); }

/* Filters here are "*suffix" and extensions a suffix, as the tests use them. */
static int EndsWith( const char *name, const char *suffix ) {
	size_t n = strlen( name ), s = strlen( suffix );
	return n >= s && !Q_stricmp( name + n - s, suffix );
}
int Com_FilterPath( char *filter, char *name, int casesensitive ) {
	(void)casesensitive;
	Expect( filter[0] == '*', "fixture filter form" );
	return EndsWith( name, filter + 1 );
}

/** The loose game directory: hit.cfg in the served directory and nothing anywhere else. */
char **Sys_ListFiles( const char *directory, const char *extension, char *filter, int *numfiles, qboolean wantsubs ) {
	static const char loose[] = "hit.cfg";
	char **list;
	(void)wantsubs;

	scans++;
	*numfiles = 0;
	if ( !servedPath || strcmp( directory, servedDir ) ||
	     !( filter ? Com_FilterPath( filter, (char *)loose, qfalse ) : EndsWith( loose, extension ) ) ) {
		return NULL;
	}
	list = malloc( 2 * sizeof( *list ) );
	Expect( list != NULL, "loose list allocation" );
	list[0] = strdup( loose );
	list[1] = NULL;
	*numfiles = 1;
	return list;
}
void Sys_FreeFileList( char **list ) {
	int i;
	for ( i = 0; list && list[i]; i++ ) {
		free( list[i] );
	}
	free( list );
}

/** A qpath of exactly length characters: letters from shift on, a '/' every 100, and no "..", ':' or trailing '/'. */
static char *LongPath( int length, int shift ) {
	char *path = malloc( length + 1 );
	int i;
	Expect( path != NULL, "path allocation" );
	for ( i = 0; i < length; i++ ) {
		path[i] = i % 100 == 99 ? '/' : 'a' + ( i + shift ) % 26;
	}
	if ( path[length - 1] == '/' ) {
		path[length - 1] = 'z';
	}
	path[length] = 0;
	return path;
}

/** Put hit.cfg in the loose directory of path (NULL: nowhere). */
static void Serve( const char *path ) {
	servedPath = path;
	if ( path ) {
		Expect( snprintf( servedDir, sizeof( servedDir ), "%s/%s/%s", base, BASEGAME, path ) < (int)sizeof( servedDir ),
		        "served directory fits" );
	}
}

/** Search order pk3 -> base/baseq3 directory, as FS_Startup builds it. */
static void Mount( const char *pk3Path, const char *workDir ) {
	int i;
	zeroVar.string = "0";
	fs_debug = fs_restrict = &zeroVar;
	Q_strncpyz( fs_gamedir, BASEGAME, sizeof( fs_gamedir ) );
	Expect( snprintf( base, sizeof( base ), "%s/q3", workDir ) < (int)sizeof( base ), "base path fits" );
	Q_strncpyz( dirEntry.path, base, sizeof( dirEntry.path ) );
	Q_strncpyz( dirEntry.gamedir, BASEGAME, sizeof( dirEntry.gamedir ) );
	dirPath.dir = &dirEntry;
	packPath.pack = FS_LoadZipFile( (char *)pk3Path, "pak0" );
	Expect( packPath.pack != NULL, "fixture pk3 mounts" );
	packPath.next = &dirPath;
	fs_searchpaths = &packPath;
	for ( i = 0; i < NUM_LENGTHS; i++ ) {
		longPaths[i] = LongPath( lengths[i], 0 );
	}
	/* The runner's pk3 holds <nearDir>/near.cfg and <edgeDir>/edge.cfg, a 255-character
	 * name. A list path also matches names it only prefixes, so neither prefixes the other. */
	nearDir = LongPath( 240, 0 );
	edgeDir = LongPath( 246, 13 );
}

static void Unmount( void ) {
	int i;
	unzClose( packPath.pack->handle );
	Z_Free( packPath.pack->buildBuffer );
	Z_Free( packPath.pack );
	packPath.pack = NULL;
	fs_searchpaths = NULL;
	for ( i = 0; i < NUM_LENGTHS; i++ ) {
		free( longPaths[i] );
	}
	free( nearDir );
	free( edgeDir );
	Expect( !zoneLive, "zone released" );
}

/** The count names packed in list, each followed by '\n'. */
static char *Joined( const char *list, int count ) {
	static char got[1024];
	int i;
	got[0] = 0;
	for ( i = 0; i < count; i++, list += strlen( list ) + 1 ) {
		Q_strcat( got, sizeof( got ), list );
		Q_strcat( got, sizeof( got ), "\n" );
	}
	return got;
}

#if defined(Q3_TEST_UI) || defined(Q3_TEST_GAME)
#if defined(Q3_TEST_UI)
#define Q3_CLIENT_SYSCALL_REAL_FS
#include "client_syscall_stubs.h"
#define LIST_TRAP UI_FS_GETFILELIST
#define SystemCalls CL_UISystemCalls
#define MODULE "UI"
#else
static vm_t vm;
vm_t *gvm = &vm;
server_t sv;
serverStatic_t svs;
cvar_t *sv_maxclients;
int bot_enable;

/** The game dispatcher reaches no other engine service here. */
static void Unexpected( const char *name ) {
	fprintf( stderr, "FS list long path regression reached %s\n", name );
	exit( 1 );
}
void QDECL Com_Error( int level, const char *format, ... ) {
	(void)level; (void)format;
	Expect( 0, "unexpected engine error" );
}
void QDECL Com_Printf( const char *format, ... ) { (void)format; }
void QDECL Com_DPrintf( const char *format, ... ) { (void)format; }
void Com_Memcpy( void *dest, const void *src, const size_t count ) { memcpy( dest, src, count ); }
void Com_Memset( void *dest, const int val, const size_t count ) { memset( dest, val, count ); }
int BotImport_DebugPolygonCreate( int color, int numPoints, vec3_t *points ) { Unexpected( __func__ ); return 0; }
void BotImport_DebugPolygonDelete( int id ) { Unexpected( __func__ ); }
void Cbuf_ExecuteText( int exec_when, const char *text ) { Unexpected( __func__ ); }
void CM_AdjustAreaPortalState( int area1, int area2, qboolean open ) { Unexpected( __func__ ); }
qboolean CM_AreasConnected( int area1, int area2 ) { Unexpected( __func__ ); return qfalse; }
byte *CM_ClusterPVS( int cluster ) { Unexpected( __func__ ); return NULL; }
int Cmd_Argc( void ) { Unexpected( __func__ ); return 0; }
void Cmd_ArgvBuffer( int arg, char *buffer, int bufferLength ) { Unexpected( __func__ ); }
char *CM_EntityString( void ) { Unexpected( __func__ ); return NULL; }
clipHandle_t CM_InlineModel( int index ) { Unexpected( __func__ ); return 0; }
int CM_LeafArea( int leafnum ) { Unexpected( __func__ ); return 0; }
int CM_LeafCluster( int leafnum ) { Unexpected( __func__ ); return 0; }
void CM_ModelBounds( clipHandle_t model, vec3_t mins, vec3_t maxs ) { Unexpected( __func__ ); }
int CM_PointLeafnum( const vec3_t p ) { Unexpected( __func__ ); return 0; }
void CM_TransformedBoxTrace( trace_t *results, const vec3_t start, const vec3_t end, vec3_t mins, vec3_t maxs,
	clipHandle_t model, int brushmask, const vec3_t origin, const vec3_t angles, int capsule ) { Unexpected( __func__ ); }
int Com_Milliseconds( void ) { Unexpected( __func__ ); return 0; }
int Com_RealTime( qtime_t *qtime ) { Unexpected( __func__ ); return 0; }
cvar_t *Cvar_Get( const char *var_name, const char *value, int flags ) { Unexpected( __func__ ); return NULL; }
char *Cvar_InfoString( int bit ) { Unexpected( __func__ ); return NULL; }
void Cvar_Register( vmCvar_t *vmCvar, const char *varName, const char *defaultValue, int flags ) { Unexpected( __func__ ); }
void Cvar_Set( const char *var_name, const char *value ) { Unexpected( __func__ ); }
void Cvar_SetSafe( const char *var_name, const char *value ) { Unexpected( __func__ ); }
void Cvar_Update( vmCvar_t *vmCvar ) { Unexpected( __func__ ); }
int Cvar_VariableIntegerValue( const char *var_name ) { Unexpected( __func__ ); return 0; }
void Cvar_VariableStringBuffer( const char *var_name, char *buffer, int bufsize ) { Unexpected( __func__ ); }
float Cvar_VariableValue( const char *var_name ) { Unexpected( __func__ ); return 0; }
qboolean EA_ClientValid( int client ) { Unexpected( __func__ ); return qfalse; }
int SV_AreaEntities( const vec3_t mins, const vec3_t maxs, int *entityList, int maxcount ) { Unexpected( __func__ ); return 0; }
int SV_BotAllocateClient( void ) { Unexpected( __func__ ); return -1; }
void SV_BotFreeClient( int clientNum ) { Unexpected( __func__ ); }
int SV_BotGetConsoleMessage( int client, char *buf, int size ) { Unexpected( __func__ ); return 0; }
int SV_BotGetSnapshotEntity( int client, int ent ) { Unexpected( __func__ ); return -1; }
int SV_BotLibSetup( void ) { Unexpected( __func__ ); return 0; }
int SV_BotLibShutdown( void ) { Unexpected( __func__ ); return 0; }
void SV_ClientThink( client_t *cl, usercmd_t *cmd ) { Unexpected( __func__ ); }
clipHandle_t SV_ClipHandleForEntity( const sharedEntity_t *ent ) { Unexpected( __func__ ); return 0; }
void SV_DropClient( client_t *drop, const char *reason ) { Unexpected( __func__ ); }
void SV_GetConfigstring( int index, char *buffer, int bufferSize ) { Unexpected( __func__ ); }
void SV_GetUserinfo( int index, char *buffer, int bufferSize ) { Unexpected( __func__ ); }
void SV_LinkEntity( sharedEntity_t *ent ) { Unexpected( __func__ ); }
int SV_PointContents( const vec3_t p, int passEntityNum ) { Unexpected( __func__ ); return 0; }
void QDECL SV_SendServerCommand( client_t *cl, const char *fmt, ... ) { Unexpected( __func__ ); }
void SV_SetConfigstring( int index, const char *val ) { Unexpected( __func__ ); }
void SV_SetUserinfo( int index, const char *val ) { Unexpected( __func__ ); }
void SV_Trace( trace_t *results, const vec3_t start, vec3_t mins, vec3_t maxs, const vec3_t end,
	int passEntityNum, int contentmask, int capsule ) { Unexpected( __func__ ); }
void SV_UnlinkEntity( sharedEntity_t *ent ) { Unexpected( __func__ ); }
int Sys_Milliseconds( void ) { Unexpected( __func__ ); return 0; }
void Sys_SnapVector( float *v ) { Unexpected( __func__ ); }
int VM_CallCompiled( vm_t *target, int *args ) { Unexpected( __func__ ); return 0; }
int VM_CallInterpreted( vm_t *target, int *args ) { Unexpected( __func__ ); return 0; }
#define LIST_TRAP G_FS_GETFILELIST
#define SystemCalls SV_GameSystemCalls
#define MODULE "game"
#endif

/* The interpreted module's image: its extension, path and list buffer. */
#define QVM_IMAGE_SIZE 16384
#define EXT_OFS 16
#define PATH_OFS 64
#define LIST_OFS 8192
#define LIST_SIZE 1024
static byte snapshot[QVM_IMAGE_SIZE];

/** Make the list trap as an interpreted QVM does; nothing outside its list buffer may change. */
static int Trap( const char *path, const char *extension ) {
	int args[5] = { LIST_TRAP, PATH_OFS, EXT_OFS, LIST_OFS, LIST_SIZE };
	int count;

	Expect( PATH_OFS + (int)strlen( path ) < LIST_OFS, "path fits below the list buffer" );
	memset( vm.dataBase, 0x5a, QVM_IMAGE_SIZE );
	strcpy( (char *)vm.dataBase + EXT_OFS, extension );
	strcpy( (char *)vm.dataBase + PATH_OFS, path );
	memcpy( snapshot, vm.dataBase, QVM_IMAGE_SIZE );
	vm.currentlyInterpreting = qtrue;
	vm.interpretFaulted = qfalse;
	count = SystemCalls( args );
	Expect( !vm.interpretFaulted, "the list trap faulted the module" );
	Expect( !memcmp( snapshot, vm.dataBase, LIST_OFS ) &&
	        !memcmp( snapshot + LIST_OFS + LIST_SIZE, vm.dataBase + LIST_OFS + LIST_SIZE,
	                 QVM_IMAGE_SIZE - LIST_OFS - LIST_SIZE ), "the list trap wrote outside its buffer" );
	return count;
}

static void TrapList( const char *path, const char *extension, int want, const char *names, const char *message ) {
	int calls = forwarded, count = Trap( path, extension );
	if ( count != want || strcmp( Joined( (char *)vm.dataBase + LIST_OFS, count ), names ) ) {
		fprintf( stderr, "path \"%s\" extension \"%s\": %d names\n%s", path, extension, count,
		         Joined( (char *)vm.dataBase + LIST_OFS, count ) );
		Expect( 0, message );
	}
	Expect( forwarded == calls + 1, "a list path that fits reaches FS_GetFileList once" );
}

/** Normal list traps, then each long path through the trap. */
static void Run( void ) {
	int i, calls, scanned;

	vm.dataBase = calloc( 1, QVM_IMAGE_SIZE );
	Expect( vm.dataBase != NULL, "module image allocation" );
	vm.dataMask = QVM_IMAGE_SIZE - 1;
	currentVM = &vm;

	TrapList( "scripts", ".shader", 2, "base_wall.shader\nsfx.shader\n", "a pk3 directory lists as before" );
	TrapList( "levelshots/", "jpg", 1, "q3dm1.jpg\n", "a trailing separator lists as before" );
	TrapList( nearDir, "cfg", 1, "near.cfg\n", "a 240-character pk3 directory lists as before" );
	TrapList( edgeDir, "cfg", 1, "edge.cfg\n", "the directory of a 255-character pk3 name lists as before" );
	for ( i = 0; i < NUM_LENGTHS; i++ ) {
		Serve( longPaths[i] );
		scanned = scans;
		if ( lengths[i] < MAX_ZPATH ) {
			TrapList( longPaths[i], "cfg", 1, "hit.cfg\n", "a 255-character path lists its directory" );
			Expect( scans == scanned + 1, "a 255-character path is scanned" );
		} else {
			calls = forwarded;
			Expect( Trap( longPaths[i], "cfg" ) == 0, "a path of MAX_ZPATH or more lists nothing" );
			Expect( vm.dataBase[LIST_OFS] == 0 &&
			        !memcmp( vm.dataBase + LIST_OFS + 1, snapshot + LIST_OFS + 1, LIST_SIZE - 1 ),
			        "a refused list is empty and the rest of its buffer untouched" );
			Expect( forwarded == calls, "a path of MAX_ZPATH or more never reaches FS_GetFileList" );
			Expect( scans == scanned, "a path of MAX_ZPATH or more is never scanned" );
		}
	}
	Serve( NULL );
	free( vm.dataBase );
	printf( "The %s list trap refuses paths of 256, 400 and 4096 characters and lists as before (issue #398)\n", MODULE );
}

#else
/* The console: dir's arguments and what it prints. */
static char printed[16384];
static const char *dirArgv[3];
static int dirArgc;

void QDECL Com_Error( int level, const char *format, ... ) {
	(void)level; (void)format;
	Expect( 0, "unexpected engine error" );
}
void QDECL Com_Printf( const char *format, ... ) {
	size_t used = strlen( printed );
	va_list ap;
	va_start( ap, format );
	Expect( vsnprintf( printed + used, sizeof( printed ) - used, format, ap ) < (int)( sizeof( printed ) - used ),
	        "console output fits" );
	va_end( ap );
}
void QDECL Com_DPrintf( const char *format, ... ) { (void)format; }
void Com_Memset( void *out, int value, size_t size ) { memset( out, value, size ); }
void Com_Memcpy( void *out, const void *in, size_t size ) { memcpy( out, in, size ); }
int Cmd_Argc( void ) { return dirArgc; }
char *Cmd_Argv( int arg ) { return arg < dirArgc ? (char *)dirArgv[arg] : ""; }

/** A caller's path in a block of exactly strlen + 1 bytes. */
static char *Exact( const char *text ) {
	char *copy = malloc( strlen( text ) + 1 );
	Expect( copy != NULL, "path allocation" );
	strcpy( copy, text );
	return copy;
}

/** FS_GetFileList as the modules' trap reaches it. */
static void GetFileList( const char *text, const char *extension, int want, const char *names, const char *message ) {
	char listbuf[512], *path = Exact( text );
	int count;

	memset( listbuf, 0x5a, sizeof( listbuf ) );
	count = FS_GetFileList( path, extension, listbuf, sizeof( listbuf ) );
	if ( count != want || strcmp( Joined( listbuf, count ), names ) ) {
		fprintf( stderr, "path \"%s\" extension \"%s\": %d names\n%s", path, extension, count, Joined( listbuf, count ) );
		Expect( 0, message );
	}
	if ( !count ) {
		Expect( listbuf[0] == 0, "an empty list is terminated" );
	}
	free( path );
}

/** The dir command prints its header and then each name. */
static void Dir( const char *path, const char *extension, const char *names, const char *message ) {
	size_t size = strlen( path ) + strlen( extension ) + strlen( names ) + 64;
	char *want = malloc( size );

	Expect( want != NULL, "dir output allocation" );
	snprintf( want, size, "Directory of %s %s\n---------------\n%s", path, extension, names );
	printed[0] = 0;
	dirArgv[0] = "dir";
	dirArgv[1] = path;
	dirArgv[2] = extension;
	dirArgc = 3;
	FS_Dir_f();
	if ( strcmp( printed, want ) ) {
		fprintf( stderr, "want:\n%sgot:\n%s", want, printed );
		Expect( 0, message );
	}
	free( want );
}

/** FS_ReturnPath on its own, for pk3 names and list paths: it returns the offset of the
 *  last separator and the separator count, and writes only the directory part, bounded
 *  to zpath and with nothing after its terminator (it runs once per pk3 entry). */
static void ReturnPath( const char *name ) {
	char *zpath = malloc( MAX_ZPATH );	/* exact size, so ASan sees any write past it */
	const char *s;
	int len = 0, depth = 0, got, gotDepth = -1, copied;

	Expect( zpath != NULL, "zpath allocation" );
	for ( s = name; *s; s++ ) {
		if ( *s == '/' || *s == '\\' ) {
			len = s - name;
			depth++;
		}
	}
	copied = len < MAX_ZPATH ? len : MAX_ZPATH - 1;
	memset( zpath, 0x5a, MAX_ZPATH );
	got = FS_ReturnPath( name, zpath, &gotDepth );
	Expect( got == len && gotDepth == depth, "FS_ReturnPath returns the last separator and the depth" );
	Expect( !strncmp( zpath, name, copied ) && zpath[copied] == 0, "FS_ReturnPath keeps the directory part" );
	for ( s = zpath + copied + 1; s < zpath + MAX_ZPATH; s++ ) {
		Expect( *s == 0x5a, "FS_ReturnPath writes nothing after the directory part" );
	}
	free( zpath );
}

/** Normal listings, then each long path through every listing entry point. */
static void Run( void ) {
	char **list, *path;
	int i, n, fits, scanned;

	GetFileList( "scripts", ".shader", 2, "base_wall.shader\nsfx.shader\n", "a pk3 directory lists as before" );
	GetFileList( "levelshots/", "jpg", 1, "q3dm1.jpg\n", "a trailing separator lists as before" );
	GetFileList( nearDir, "cfg", 1, "near.cfg\n", "a 240-character pk3 directory lists as before" );
	GetFileList( edgeDir, "cfg", 1, "edge.cfg\n", "the directory of a 255-character pk3 name lists as before" );
	Dir( "scripts", "shader", "base_wall.shader\nsfx.shader\n", "dir lists a pk3 directory as before" );

	for ( i = 0; i < NUM_LENGTHS; i++ ) {
		path = Exact( longPaths[i] );
		fits = lengths[i] < MAX_ZPATH;
		Serve( path );
		scanned = scans;

		GetFileList( path, "cfg", fits, fits ? "hit.cfg\n" : "", fits ?
		             "a 255-character path lists its directory" : "a path of MAX_ZPATH or more lists nothing" );

		n = -1;
		list = FS_ListFiles( path, ".cfg", &n );
		Expect( fits ? n == 1 && list && !strcmp( list[0], "hit.cfg" ) && !list[1] : n == 0 && !list,
		        "FS_ListFiles lists a 255-character path and refuses longer ones" );
		FS_FreeFileList( list );

		/* a filter matches every pk3 name as fdir does, then the scanned directory */
		n = -1;
		list = FS_ListFilteredFiles( path, "", "*.cfg", &n );
		Expect( fits ? n == 3 && list && !strcmp( list[2], "hit.cfg" ) && !list[3] : n == 0 && !list,
		        "FS_ListFilteredFiles with a filter lists a 255-character path and refuses longer ones" );
		FS_FreeFileList( list );

		Dir( path, "cfg", fits ? "hit.cfg\n" : "", "dir lists a 255-character path and nothing for longer ones" );
		Expect( scans == scanned + ( fits ? 4 : 0 ), "only a path shorter than MAX_ZPATH reaches the directory scan" );
		free( path );
	}
	Serve( NULL );

	ReturnPath( "scripts/base_wall.shader" );
	ReturnPath( "levelshots/" );
	ReturnPath( "default.cfg" );
	ReturnPath( "" );
	ReturnPath( edgeDir );
	for ( i = 0; i < NUM_LENGTHS; i++ ) {
		ReturnPath( longPaths[i] );
	}
	puts( "FS listings refuse paths of 256, 400 and 4096 characters and list as before (issue #398)" );
}
#endif

int main( int argc, char **argv ) {
	Expect( argc == 3, "usage: fs-list-long-path pak0.pk3 workdir" );
	Mount( argv[1], argv[2] );
	Run();
	Unmount();
	return 0;
}
