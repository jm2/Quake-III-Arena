/* Issue #35: the FS traps of the game, cgame and ui modules only reach file
 * handles their own module opened.  Retail passed any handle number from a
 * QVM straight to FS_Read2, FS_Write, FS_Seek and FS_FCloseFile, so a module
 * could read, move or close the engine's handles or another module's, and an
 * FS_SEEK trap with a bad origin on a FILE handle was an ERR_FATAL.
 *
 * The real files.c serves a directory search path and the home path in a
 * scratch directory.  Handles are opened the way the traps open them, through
 * FS_VM_OpenFile, and by the engine itself; every trap is then tried with
 * every owner and with handles that are missing, closed and reused. */
#include <limits.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../code/qcommon/files.c"

qboolean com_fullyInitialized;
cvar_t *com_journal;
fileHandle_t com_journalDataFile;
static cvar_t debugVar, restrictVar, copyVar, homeVar;
static directory_t directory;
static searchpath_t search;
static const int owners[] = { FS_OWNER_GAME, FS_OWNER_CGAME, FS_OWNER_UI };
#define NUM_OWNERS ( (int)( sizeof( owners ) / sizeof( owners[0] ) ) )

static void Check( int condition, const char *message ) {
	if ( !condition ) {
		fprintf( stderr, "FS VM owner regression failed: %s\n", message );
		exit( 1 );
	}
}

void QDECL Com_Error( int level, const char *format, ... ) {
	char text[1024];
	va_list args;
	va_start( args, format );
	vsnprintf( text, sizeof( text ), format, args );
	va_end( args );
	fprintf( stderr, "FS VM owner regression failed: engine %s: %s\n",
		level == ERR_FATAL ? "ERR_FATAL" : "error", text );
	exit( 1 );
}
void QDECL Com_Printf( const char *format, ... ) { (void)format; }
void QDECL Com_DPrintf( const char *format, ... ) { (void)format; }
void *Z_Malloc( int size ) { void *p = calloc( 1, size ? size : 1 ); Check( p != NULL, "zone allocation" ); return p; }
void Z_Free( void *p ) { free( p ); }
void *Hunk_AllocateTempMemory( int size ) { void *p = malloc( size ? size : 1 ); Check( p != NULL, "temp allocation" ); return p; }
void Hunk_FreeTempMemory( void *p ) { free( p ); }
void Sys_Mkdir( const char *path ) { mkdir( path, 0700 ); }
void S_ClearSoundBuffer( void ) {}
/* code/mac/mac_main.c: no read-ahead thread, streamed reads are plain reads */
void Sys_BeginStreamedFile( fileHandle_t f, int readAhead ) { (void)f; (void)readAhead; }
void Sys_EndStreamedFile( fileHandle_t f ) { (void)f; }
int Sys_StreamedRead( void *buffer, int size, int count, fileHandle_t f ) { return FS_Read( buffer, size * count, f ); }
void Sys_StreamSeek( fileHandle_t f, int offset, int origin ) { FS_Seek( f, offset, origin ); }
#ifndef Com_Memset
void Com_Memset( void *out, int value, size_t size ) { memset( out, value, size ); }
#endif
#ifndef Com_Memcpy
void Com_Memcpy( void *out, const void *in, size_t size ) { memcpy( out, in, size ); }
#endif

/* Every module other than the owner gets nothing from the handle and leaves it as it was. */
static void Foreign( fileHandle_t f, int owner, const char *expected ) {
	fileHandleData_t saved;
	char bytes[16];
	long position;
	int i, origin;

	memcpy( &saved, &fsh[f], sizeof( saved ) );
	position = ftell( fsh[f].handleFiles.file.o );
	for ( i = 0 ; i < NUM_OWNERS ; i++ ) {
		if ( owners[i] == owner ) {
			continue;
		}
		memset( bytes, 0, sizeof( bytes ) );
		Check( FS_VM_ReadFile( bytes, 4, f, owners[i] ) == 0 && !bytes[0], "foreign read returns nothing" );
		Check( FS_VM_WriteFile( "evil", 4, f, owners[i] ) == 0, "foreign write is refused" );
		for ( origin = -1 ; origin <= FS_SEEK_SET + 1 ; origin++ ) {
			Check( FS_VM_SeekFile( f, 0, origin, owners[i] ) == -1, "foreign seek is refused" );
		}
		FS_VM_CloseFile( f, owners[i] );
		Check( !memcmp( &saved, &fsh[f], sizeof( saved ) ) && ftell( fsh[f].handleFiles.file.o ) == position,
			"foreign traps leave the handle open and in place" );
	}
	if ( expected ) {
		memset( bytes, 0, sizeof( bytes ) );
		Check( FS_Read( bytes, strlen( expected ), f ) == (int)strlen( expected ) && !strcmp( bytes, expected ),
			"the owner still reads its data" );
	}
}

static void Missing( void ) {
	const int handles[] = { INT_MIN, -1, 0, 7, MAX_FILE_HANDLES, INT_MAX };
	char byte = 0;
	int i, j;

	for ( i = 0 ; i < (int)( sizeof( handles ) / sizeof( handles[0] ) ) ; i++ ) {
		for ( j = 0 ; j < NUM_OWNERS ; j++ ) {
			Check( FS_VM_ReadFile( &byte, 1, handles[i], owners[j] ) == 0 && !byte, "missing read" );
			Check( FS_VM_WriteFile( "x", 1, handles[i], owners[j] ) == 0, "missing write" );
			Check( FS_VM_SeekFile( handles[i], 0, FS_SEEK_SET, owners[j] ) == -1, "missing seek" );
			FS_VM_CloseFile( handles[i], owners[j] );
		}
	}
}

static void WriteFixture( const char *root, const char *name, const char *text ) {
	char path[MAX_OSPATH];
	FILE *file;

	snprintf( path, sizeof( path ), "%s/baseq3/%s", root, name );
	file = fopen( path, "wb" );
	Check( file && fwrite( text, 1, strlen( text ), file ) == strlen( text ) && !fclose( file ), "fixture file" );
}

int main( int argc, char **argv ) {
	char root[MAX_OSPATH], path[MAX_OSPATH], bytes[32];
	fileHandle_t engine, module, writer, reused;
	int i, origin;

	Check( argc == 2, "scratch directory argument" );
	Q_strncpyz( root, argv[1], sizeof( root ) );
	snprintf( path, sizeof( path ), "%s/baseq3", root );
	Check( !mkdir( path, 0700 ), "scratch game directory" );
	WriteFixture( root, "engine.txt", "engine data\n" );
	WriteFixture( root, "module.txt", "module data\n" );

	Q_strncpyz( directory.path, root, sizeof( directory.path ) );
	Q_strncpyz( directory.gamedir, BASEGAME, sizeof( directory.gamedir ) );
	search.dir = &directory;
	fs_searchpaths = &search;
	fs_debug = &debugVar;
	fs_restrict = &restrictVar;
	fs_copyfiles = &copyVar;
	homeVar.string = root;
	fs_homepath = &homeVar;
	Q_strncpyz( fs_gamedir, BASEGAME, sizeof( fs_gamedir ) );

	Missing();
	for ( i = 0 ; i < NUM_OWNERS ; i++ ) {
		/* an engine handle (the journal, a demo, a log) belongs to no module */
		Check( FS_FOpenFileRead( "engine.txt", &engine, qtrue ) == 12 && engine > 0, "engine handle" );
		Check( FS_Read( bytes, 3, engine ) == 3, "engine partial read" );
		Foreign( engine, 0, "ine data\n" );

		/* a module's handle only answers to that module */
		Check( FS_VM_OpenFile( "module.txt", NULL, FS_READ, owners[i] ) == qtrue, "existence query" );
		Check( FS_VM_OpenFile( "missing.txt", &module, FS_READ, owners[i] ) == -1 && module == 0, "missing file" );
		Check( FS_VM_OpenFile( "module.txt", &module, FS_READ, owners[i] ) == 12 && module > 0 && module != engine,
			"module read handle" );
		memset( bytes, 0, sizeof( bytes ) );
		Check( FS_VM_ReadFile( bytes, 3, module, owners[i] ) == 3 && !strcmp( bytes, "mod" ), "owner reads" );
		Foreign( module, owners[i], NULL );
		Check( FS_VM_SeekFile( module, 0, FS_SEEK_SET, owners[i] ) == 0, "owner seeks" );
		memset( bytes, 0, sizeof( bytes ) );
		Check( FS_VM_ReadFile( bytes, 12, module, owners[i] ) == 12 && !strcmp( bytes, "module data\n" ),
			"owner rereads" );
		FS_VM_CloseFile( module, owners[i] );
		Check( !fsh[module].handleFiles.file.o && !fsh[module].owner, "owner closes and the slot is cleared" );

		/* the engine reuses the closed slot: the module's stale number reaches nothing */
		Check( FS_FOpenFileRead( "engine.txt", &reused, qtrue ) == 12 && reused == module, "engine reuses the slot" );
		Foreign( reused, 0, "engine data\n" );
		FS_FCloseFile( reused );
		FS_FCloseFile( engine );

		/* a write handle: a bad seek origin is refused instead of ERR_FATAL */
		Check( FS_VM_OpenFile( "written.txt", &writer, FS_WRITE, owners[i] ) == 0 && writer > 0, "module write handle" );
		Check( FS_VM_WriteFile( "written data\n", 13, writer, owners[i] ) == 13, "owner writes" );
		for ( origin = -2 ; origin <= FS_SEEK_SET + 2 ; origin++ ) {
			if ( origin == FS_SEEK_CUR || origin == FS_SEEK_END || origin == FS_SEEK_SET ) {
				continue;
			}
			Check( FS_VM_SeekFile( writer, 0, origin, owners[i] ) == -1, "bad origin is refused" );
		}
		Check( FS_VM_SeekFile( writer, 0, INT_MIN, owners[i] ) == -1 &&
			FS_VM_SeekFile( writer, 0, INT_MAX, owners[i] ) == -1, "extreme origin is refused" );
		Check( FS_FTell( writer ) == 13, "refused seeks leave the position" );
		Check( FS_VM_SeekFile( writer, 7, FS_SEEK_SET, owners[i] ) == 0 && FS_FTell( writer ) == 7, "owner seeks a write handle" );
		Foreign( writer, owners[i], NULL );
		FS_VM_CloseFile( writer, owners[i] );
		Check( !fsh[writer].handleFiles.file.o, "owner closes its write handle" );

		/* appends are tagged as well */
		Check( FS_VM_OpenFile( "written.txt", &writer, FS_APPEND_SYNC, owners[i] ) == 0 && writer > 0, "append handle" );
		Foreign( writer, owners[i], NULL );
		FS_VM_CloseFile( writer, owners[i] );
		Missing();
	}
	for ( i = 0 ; i < MAX_FILE_HANDLES ; i++ ) {
		Check( !fsh[i].handleFiles.file.o, "every handle closed" );
	}
	snprintf( path, sizeof( path ), "%s/baseq3/written.txt", root );
	Check( !unlink( path ), "written file" );
	snprintf( path, sizeof( path ), "%s/baseq3/engine.txt", root );
	unlink( path );
	snprintf( path, sizeof( path ), "%s/baseq3/module.txt", root );
	unlink( path );
	snprintf( path, sizeof( path ), "%s/baseq3", root );
	rmdir( path );
	puts( "FS VM handle owner regression passed (issue #35)" );
	return 0;
}
