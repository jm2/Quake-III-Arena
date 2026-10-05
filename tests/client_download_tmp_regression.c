/* Review of #494 (issue #327): the Mac's open() and rename() now refuse a
 * path component longer than HFS's 31 bytes, and CL_BeginDownload named
 * the download's temporary file "<pk3 name>.tmp".  A pk3 whose name is 28
 * to 31 bytes is a valid HFS name, but its temporary name was 32 to 35
 * bytes, so the download failed with "Could not create".
 *
 * The real CL_BeginDownload (cl_main.c, built with the Classic Mac OS
 * PATH_SEP ':' under -DQ3_TEST_HFS) names the files; the steps
 * CL_ParseDownload takes with them (FS_SV_FOpenFileWrite of the temporary
 * name, FS_FCloseFile at the end, then the trusted FS_SV_Rename to the pk3)
 * are replayed against the real _open_r and _rename_r of
 * code/mac/mac_syscalls.c on tests/mac_files_fake.h, with the qpath
 * "baseq3/<leaf>" turned into ":baseq3:<leaf>" as FS_BuildOSPath does on
 * the Mac.  The File Manager either refuses leaves longer than 31 bytes
 * (bdNamErr) or cuts them to 31 (the truncation #327 fears).  Pk3 leaves
 * of 27, 28, 30 and 31 bytes must all end as the pk3, with no temporary
 * file left, a temporary leaf of at most 31 bytes that differs from the
 * pk3's, and a 27-byte name keeping "<name>.tmp".  Built without
 * Q3_TEST_HFS (PATH_SEP '/'), every name must still get "<name>.tmp", as
 * in 1.32c.
 *
 * Built twice: with -DQ3_TEST_MAC_SIDE this file is the File Manager side
 * (mac_syscalls.c and the fake, whose types clash with q_shared.h's);
 * without it, the client side, linked with that object. */
#ifdef Q3_TEST_MAC_SIDE

#include <errno.h>
#include <fcntl.h>
#include "../code/mac/mac_syscalls.c"

static struct _reent reent;

void MacFS_Reset( int nameLimit ) {
	FakeFM_Reset();
	fakeFMNameLimit = nameLimit;
	FakeFM_AddDir( ":baseq3:", FAKE_FM_VREFNUM );
}

/* fopen( path, "wb" ) and a write of eof bytes; 0 or the errno. */
int MacFS_Create( const char *path, long eof ) {
	int fd;

	reent._errno = 0;
	fd = _open_r( &reent, path, O_WRONLY | O_CREAT | O_TRUNC, 0666 );
	if ( fd < 0 ) {
		return reent._errno ? reent._errno : -1;
	}
	FakeFM_File( path )->eof = eof;
	FSClose( (short)( fd - kMacRefNumOffset ) );
	return 0;
}

int MacFS_Rename( const char *from, const char *to ) {
	reent._errno = 0;
	if ( _rename_r( &reent, from, to ) ) {
		return reent._errno ? reent._errno : -1;
	}
	return 0;
}

/* The file's length, or -1 if there is none. */
long MacFS_Length( const char *path ) {
	fakeFMFile_t *file = FakeFM_File( path );

	return file ? file->eof : -1;
}

int MacFS_Files( void ) {
	int i, count = 0;

	for ( i = 0 ; i < FAKE_FM_FILES ; i++ ) {
		count += fakeFMFiles[i].used;
	}
	return count;
}

int MacFS_OpenPaths( void ) {
	return FakeFM_OpenPaths();
}

#else

#include "../code/game/q_shared.h"
#ifdef Q3_TEST_HFS
#undef PATH_SEP
#define PATH_SEP ':'
#endif
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../code/client/cl_main.c"

void MacFS_Reset( int nameLimit );
int MacFS_Create( const char *path, long eof );
int MacFS_Rename( const char *from, const char *to );
long MacFS_Length( const char *path );
int MacFS_Files( void );
int MacFS_OpenPaths( void );

#define DOWNLOAD_SIZE	4321

cvar_t *com_cl_running;
vm_t *uivm;
int cl_connectedToPureServer;

static const char *currentLeaf = "setup";
static int nameLimit, opens, closes, renames;
static char openedPath[MAX_OSPATH];

static void Check( int ok, const char *message ) {
	if ( !ok ) {
		fprintf( stderr, "Client download temp name regression failed (%s, File Manager %s): %s "
			"(temp \"%s\")\n", currentLeaf, nameLimit ? "cuts at 31" : "refuses past 31",
			message, clc.downloadTempName );
		exit( 1 );
	}
}

/* FS_BuildOSPath on the Mac, relative to the default directory. */
static void MacPath( char *ospath, const char *qpath ) {
	char *s;

	Com_sprintf( ospath, MAX_OSPATH, ":%s", qpath );
	for ( s = ospath ; *s ; s++ ) {
		if ( *s == '/' ) {
			*s = ':';
		}
	}
}

/* Engine imports. */
void QDECL Com_Error( int level, const char *format, ... ) {
	(void)level;
	Check( 0, format );
	exit( 1 );
}
void QDECL Com_Printf( const char *format, ... ) { (void)format; }
void QDECL Com_DPrintf( const char *format, ... ) { (void)format; }
void Com_Memcpy( void *out, const void *in, size_t size ) { memcpy( out, in, size ); }
void Com_Memset( void *out, int value, size_t size ) { memset( out, value, size ); }
void Cvar_Set( const char *name, const char *value ) { (void)name; (void)value; }
void Cvar_SetValue( const char *name, float value ) { (void)name; (void)value; }
fileHandle_t FS_SV_FOpenFileWrite( const char *filename ) {
	opens++;
	MacPath( openedPath, filename );
	return MacFS_Create( openedPath, DOWNLOAD_SIZE ) ? 0 : 1;
}
void FS_FCloseFile( fileHandle_t f ) {
	Check( f == 1, "closed the download" );
	closes++;
}
void FS_SV_Rename( const char *from, const char *to, qboolean safe ) {
	char fromPath[MAX_OSPATH], toPath[MAX_OSPATH];

	Check( !safe, "the download finalisation is trusted" );
	renames++;
	MacPath( fromPath, from );
	MacPath( toPath, to );
	if ( MacFS_Rename( fromPath, toPath ) ) {
		Check( 0, "renaming the download to its pk3 failed" );
	}
}
/* Kept by the linker for cl_main.c's other functions; no test reaches them. */
int FS_Read( void *buffer, int len, fileHandle_t f ) { (void)buffer; (void)len; (void)f; Check( 0, "FS_Read" ); return 0; }
int FS_Write( const void *buffer, int len, fileHandle_t f ) { (void)buffer; (void)f; Check( 0, "FS_Write" ); return len; }
int FS_FOpenFileRead( const char *qpath, fileHandle_t *file, qboolean uniqueFILE ) {
	(void)qpath; (void)uniqueFILE; *file = 0; Check( 0, "FS_FOpenFileRead" ); return -1;
}

/* The leaf of a qpath. */
static const char *Leaf( const char *qpath ) {
	const char *s = strrchr( qpath, '/' );

	return s ? s + 1 : qpath;
}

/* CL_BeginDownload of "baseq3/<leaf>", then the files as CL_ParseDownload
 * makes them. */
static void Download( const char *leaf ) {
	char localName[MAX_OSPATH], pk3Path[MAX_OSPATH], tempPath[MAX_OSPATH];
	fileHandle_t f;

	currentLeaf = leaf;
	MacFS_Reset( nameLimit );
	opens = closes = renames = 0;
	clc.reliableSequence = clc.reliableAcknowledge = 0;
	Com_sprintf( localName, sizeof( localName ), "baseq3/%s", leaf );

	Check( CL_BeginDownload( localName, localName ), "download refused" );
	Check( !strcmp( clc.downloadName, localName ), "download name" );
	Check( Q_stricmp( Leaf( clc.downloadTempName ), leaf ) != 0, "temporary name is the pk3's" );
	Check( !strncmp( clc.downloadTempName, "baseq3/", 7 ), "temporary file in another directory" );
	Check( strlen( clc.downloadTempName ) > 4 &&
		!strcmp( clc.downloadTempName + strlen( clc.downloadTempName ) - 4, ".tmp" ), "temporary name ends in .tmp" );
	if ( strlen( leaf ) + 4 <= 31 ) {
		Check( !strcmp( clc.downloadTempName, va( "%s.tmp", localName ) ), "a name that fits keeps <name>.tmp" );
	}

	// CL_ParseDownload: block 0 opens the temporary file; the empty block
	// closes it and renames it to the pk3
	f = FS_SV_FOpenFileWrite( clc.downloadTempName );
	Check( f != 0, "Could not create the temporary file" );
	FS_FCloseFile( f );
	FS_SV_Rename( clc.downloadTempName, clc.downloadName, qfalse );

	MacPath( pk3Path, clc.downloadName );
	MacPath( tempPath, clc.downloadTempName );
	Check( !strcmp( openedPath, tempPath ), "wrote the temporary file" );
	Check( MacFS_Length( pk3Path ) == DOWNLOAD_SIZE, "the pk3 does not hold the download" );
	Check( MacFS_Length( tempPath ) == -1, "the temporary file is still there" );
	Check( MacFS_Files() == 1, "other files left behind" );
	Check( MacFS_OpenPaths() == 0, "a path was left open" );
	Check( opens == 1 && closes == 1 && renames == 1, "one open, close and rename" );
	Check( strlen( Leaf( clc.downloadTempName ) ) <= 31, "temporary leaf longer than HFS's 31 bytes" );
}

int main( void ) {
	static const char *const leaves[] = {
		"map_cpm22_tourney_v2_fx.pk3",		/* 27 */
		"map_cpm22_tourney_v2_fix.pk3",		/* 28 */
		"q3dm17_the_longest_yard_rm.pk3",	/* 30 */
		"q3dm17_the_longest_yard_rmx.pk3",	/* 31 */
		NULL
	};
	int i;

#ifdef Q3_TEST_HFS
	for ( nameLimit = 0 ; nameLimit <= 31 ; nameLimit += 31 ) {
		for ( i = 0 ; leaves[i] ; i++ ) {
			Download( leaves[i] );
		}
	}
	puts( "Mac downloads of 27- to 31-byte pk3 names use a temporary name of at most 31 bytes (issue #327)" );
#else
	for ( i = 0 ; leaves[i] ; i++ ) {
		currentLeaf = leaves[i];
		Check( CL_BeginDownload( va( "baseq3/%s", leaves[i] ), va( "baseq3/%s", leaves[i] ) ), "download refused" );
		Check( !strcmp( clc.downloadTempName, va( "baseq3/%s.tmp", leaves[i] ) ), "host temporary name is <name>.tmp" );
	}
	puts( "Host downloads keep the 1.32c temporary name <name>.tmp" );
#endif
	return 0;
}

#endif
