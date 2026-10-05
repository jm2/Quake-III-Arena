/* Issue #14: the engine side of cin_startup_regression.c.
 *
 * The real files.c, unzip.c, cmd.c and cvar.c, with a zone that is fatal when
 * it runs out, as common.c's is.  They are built apart from cl_cin.c because
 * unzip.c and cl_cin.c both include client.h, which has no include guard. */
#include <stdarg.h>
#include "../code/qcommon/files.c"
#include "../code/qcommon/unzip.c"
#include "../code/qcommon/cmd.c"
#include "../code/qcommon/cvar.c"

#define MAX_OWNERS	4096

qboolean com_fullyInitialized;
cvar_t *com_journal, *com_cl_running, *com_sv_running;
fileHandle_t com_journalDataFile;
long cinZoneBytes, cinZonePeak, cinZoneBudget = LONG_MAX;

static cvar_t debugVar, restrictVar, copyVar;
static searchpath_t search;
static struct { void *p; int size; qboolean tracked; } owners[MAX_OWNERS];
static int untracked;
static const char *mountedPk3;

void CinFail( const char *message ) {
	fprintf( stderr, "cinematic startup regression failed: %s\n", message );
	exit( 1 );
}
static void Check( int condition, const char *message ) { if ( !condition ) CinFail( message ); }

void QDECL Com_Error( int level, const char *format, ... ) {
	char text[1024];
	va_list args;
	va_start( args, format );
	vsnprintf( text, sizeof(text), format, args );
	va_end( args );
	fprintf( stderr, "cinematic startup regression failed: engine %s: %s\n",
			 level == ERR_FATAL ? "ERR_FATAL" : "error", text );
	exit( 1 );
}
void QDECL Com_Printf( const char *format, ... ) { (void)format; }
void QDECL Com_DPrintf( const char *format, ... ) { (void)format; }
void Com_Memset( void *out, int value, size_t size ) { memset( out, value, size ); }
void Com_Memcpy( void *out, const void *in, size_t size ) { memcpy( out, in, size ); }

/* A zone with cinZoneBudget bytes in use at most: running out is fatal, as in common.c. */
void *Z_Malloc( int size ) {
	int i;
	Check( size >= 0, "non-negative zone request" );
	if ( !untracked && cinZoneBytes + size > cinZoneBudget ) {
		Com_Error( ERR_FATAL, "Z_Malloc: failed on allocation of %i bytes from the main zone", size );
	}
	for ( i = 0; i < MAX_OWNERS; i++ ) {
		if ( !owners[i].p ) {
			owners[i].p = calloc( 1, size ? size : 1 );
			Check( owners[i].p != NULL, "host allocation" );
			owners[i].size = size;
			owners[i].tracked = !untracked;
			if ( owners[i].tracked ) {
				cinZoneBytes += size;
				if ( cinZoneBytes > cinZonePeak ) cinZonePeak = cinZoneBytes;
			}
			return owners[i].p;
		}
	}
	CinFail( "zone owner table full" );
	return NULL;
}
void Z_Free( void *p ) {
	int i;
	for ( i = 0; i < MAX_OWNERS; i++ ) {
		if ( owners[i].p == p && p ) {
			if ( owners[i].tracked ) cinZoneBytes -= owners[i].size;
			free( p );
			owners[i].p = NULL;
			return;
		}
	}
	CinFail( "zone free of an unowned pointer" );
}
/* Cvar and command strings come from the small-block zone; only file streams are budgeted. */
char *CopyString( const char *in ) {
	char *out;
	untracked++;
	out = Z_Malloc( strlen( in ) + 1 );
	untracked--;
	strcpy( out, in );
	return out;
}
void *S_Malloc( int size ) {
	void *p;
	untracked++;
	p = Z_Malloc( size );
	untracked--;
	return p;
}
/* cmdlist and cvarlist are registered, never run. */
int Com_Filter( char *filter, char *name, int casesensitive ) {
	(void)filter; (void)name; (void)casesensitive;
	CinFail( "unexpected list filter" );
	return 0;
}
void *Hunk_AllocateTempMemory( int size ) { return malloc( size ? size : 1 ); }
void Hunk_FreeTempMemory( void *p ) { free( p ); }
void Hunk_ClearTempMemory( void ) {}
void Sys_Mkdir( const char *p ) { (void)p; }
void S_ClearSoundBuffer( void ) { CinFail( "unexpected write-mode open" ); }

/* The Mac port's synchronous stream shims (code/mac/mac_main.c). */
void Sys_BeginStreamedFile( fileHandle_t f, int readAhead ) { (void)f; (void)readAhead; }
void Sys_EndStreamedFile( fileHandle_t f ) { (void)f; }
int Sys_StreamedRead( void *buffer, int size, int count, fileHandle_t f ) {
	return FS_Read( buffer, size * count, f );
}
void Sys_StreamSeek( fileHandle_t f, int offset, int origin ) { FS_Seek( f, offset, origin ); }

/* Commands beyond the registered ones are not part of startup. */
qboolean CL_GameCommand( void ) { return qfalse; }
qboolean SV_GameCommand( void ) { return qfalse; }
qboolean UI_GameCommand( void ) { return qfalse; }
void CL_ForwardCommandToServer( const char *string ) {
	fprintf( stderr, "%s\n", string );
	CinFail( "unknown command" );
}

/* Mount one pk3 as the only search path, and start the command and cvar systems. */
void CinMount( const char *pk3 ) {
	fs_debug = &debugVar;
	fs_restrict = &restrictVar;
	fs_copyfiles = &copyVar;
	fs_searchpaths = &search;
	search.pack = FS_LoadZipFile( (char *)pk3, "test.pk3" );
	Check( search.pack != NULL, "pk3 mounts" );
	mountedPk3 = pk3;
	Cbuf_Init();
	Cvar_Init();
	Cmd_Init();
}

/* The whole entry through a direct unzip handle, outside the modelled zone; NULL if absent. */
byte *CinEntryBytes( const char *name, long *size ) {
	unzFile z;
	unz_file_info info;
	byte *bytes = NULL;
	untracked++;
	z = unzOpen( (char *)mountedPk3 );
	Check( z && unzGoToFirstFile( z ) == UNZ_OK, "reference archive opens" );
	*size = -1;
	if ( unzLocateFile( z, name, 2 ) == UNZ_OK ) {
		Check( unzGetCurrentFileInfo( z, &info, NULL, 0, NULL, 0, NULL, 0 ) == UNZ_OK, "reference entry info" );
		bytes = malloc( info.uncompressed_size ? info.uncompressed_size : 1 );
		Check( bytes && unzOpenCurrentFile( z ) == UNZ_OK &&
			   unzReadCurrentFile( z, bytes, info.uncompressed_size ) == (int)info.uncompressed_size,
			   "reference entry reads" );
		unzCloseCurrentFile( z );
		*size = info.uncompressed_size;
	}
	unzClose( z );
	untracked--;
	return bytes;
}

/* No pk3 file handle is left open. */
qboolean CinAllFilesClosed( void ) {
	int i;
	for ( i = 0; i < MAX_FILE_HANDLES; i++ ) if ( fsh[i].handleFiles.file.o ) return qfalse;
	return qtrue;
}
