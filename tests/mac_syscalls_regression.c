/* Issue #258: Retro68's libretro _open_r works out the permission newlib's
 * open flags ask for and then opens every file with fsRdWrPerm, so
 * fopen( path, "rb" ) failed for a Finder-locked file (permErr), a CD
 * (wPrErr), a locked volume or disk image (vLckdErr) and a pk3 another path
 * already had open (opWrErr).  code/mac/mac_syscalls.c replaces it.
 *
 * Each case drives the real _open_r with the flags newlib's __sflags makes
 * for an fopen mode ("rb" is O_RDONLY, "wb" O_WRONLY|O_CREAT|O_TRUNC, "ab"
 * O_WRONLY|O_CREAT|O_APPEND, "r+b" O_RDWR, "w+b" O_RDWR|O_CREAT|O_TRUNC,
 * "wbx" adds O_EXCL) against tests/mac_files_fake.h, and checks the
 * permission it asks for, what it creates and truncates, the descriptor it
 * returns (libretro's refNum + kMacRefNumOffset) and errno on failure.
 * Read-only opens must succeed from locked files and volumes and alongside
 * other opens of the same file; write opens must still fail there. */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include "../code/mac/mac_syscalls.c"

#define FOPEN_R		( O_RDONLY )
#define FOPEN_W		( O_WRONLY | O_CREAT | O_TRUNC )
#define FOPEN_A		( O_WRONLY | O_CREAT | O_APPEND )
#define FOPEN_RPLUS	( O_RDWR )
#define FOPEN_WPLUS	( O_RDWR | O_CREAT | O_TRUNC )
#define FOPEN_WX	( O_WRONLY | O_CREAT | O_TRUNC | O_EXCL )

static const char *currentCase = "setup";
static struct _reent reent;

static void Check( int ok, const char *what ) {
	if ( !ok ) {
		fprintf( stderr, "Mac syscalls regression failed (%s): %s (errno %d, last permission %d)\n",
			currentCase, what, reent._errno, fakeFMLastPermission );
		exit( 1 );
	}
}

static void Begin( const char *name ) {
	currentCase = name;
	FakeFM_Reset();
	reent._errno = 0;
}

static int Open( const char *name, int flags ) {
	return _open_r( &reent, name, flags, 0666 );
}

/* A successful open: the descriptor names an open path with this permission. */
static void CheckOpened( int fd, SInt8 permission ) {
	fakeFMPath_t *path;

	Check( fd >= kMacRefNumOffset, "open failed" );
	path = FakeFM_Path( (short)( fd - kMacRefNumOffset ) );
	Check( path != NULL, "descriptor is not refNum + kMacRefNumOffset" );
	Check( path->permission == permission, "wrong File Manager permission" );
}

static void CheckFailed( int fd, int expectedErrno ) {
	Check( fd == -1, "open succeeded" );
	Check( reent._errno == expectedErrno, "wrong errno" );
	Check( FakeFM_OpenPaths() == 0, "a path was left open" );
}

static void ReadOnlyMedia( int volume, int lockedFile ) {
	int fd;

	FakeFM_AddFile( ":baseq3:pak0.pk3", 1000, lockedFile );
	fakeFMVolume = volume;
	fd = Open( ":baseq3:pak0.pk3", FOPEN_R );
	CheckOpened( fd, fsRdPerm );
	Check( FakeFM_File( ":baseq3:pak0.pk3" )->eof == 1000, "read changed the file" );

	Check( Open( ":baseq3:pak0.pk3", FOPEN_RPLUS ) == -1, "r+ opened read-only data" );
	Check( Open( ":baseq3:pak0.pk3", FOPEN_W ) == -1, "w opened read-only data" );
	Check( FakeFM_File( ":baseq3:pak0.pk3" )->eof == 1000, "failed write open truncated" );
	Check( FakeFM_OpenPaths() == 1, "failed write open left a path" );
}

static void Case( const char *name ) {
	int fd, fd2;
	char longName[300];

	Begin( name );
	if ( !strcmp( name, "read" ) ) {
		FakeFM_AddFile( ":baseq3:pak0.pk3", 1000, 0 );
		fd = Open( ":baseq3:pak0.pk3", FOPEN_R );
		CheckOpened( fd, fsRdPerm );
		Check( fakeFMCreates == 0 && fakeFMSetEOFs == 0, "read created or truncated" );
		Check( FakeFM_File( ":baseq3:pak0.pk3" )->eof == 1000, "read changed the file" );
		Check( fakeFMOpenDFs == 1 && fakeFMOpens == 0, "read did not use HOpenDF" );
	} else if ( !strcmp( name, "read-locked-file" ) ) {
		ReadOnlyMedia( FAKE_VOLUME_WRITABLE, 1 );
		Check( reent._errno == EACCES, "write to a locked file is not EACCES" );
	} else if ( !strcmp( name, "read-locked-volume" ) ) {
		ReadOnlyMedia( FAKE_VOLUME_SOFTWARE_LOCKED, 0 );
		Check( reent._errno == EROFS, "write to a locked volume is not EROFS" );
	} else if ( !strcmp( name, "read-cd" ) ) {
		ReadOnlyMedia( FAKE_VOLUME_READ_ONLY_MEDIA, 0 );
		Check( reent._errno == EROFS, "write to read-only media is not EROFS" );
	} else if ( !strcmp( name, "read-twice" ) ) {
		// unzReOpen opens the pk3 again while its shared handle stays open
		FakeFM_AddFile( ":baseq3:pak0.pk3", 1000, 0 );
		fd = Open( ":baseq3:pak0.pk3", FOPEN_R );
		fd2 = Open( ":baseq3:pak0.pk3", FOPEN_R );
		CheckOpened( fd, fsRdPerm );
		CheckOpened( fd2, fsRdPerm );
		Check( fd != fd2, "both opens share a path" );
		// reading alongside a writer, and writing alongside readers
		fd = Open( ":baseq3:pak0.pk3", FOPEN_RPLUS );
		CheckOpened( fd, fsRdWrPerm );
		fd2 = Open( ":baseq3:pak0.pk3", FOPEN_R );
		CheckOpened( fd2, fsRdPerm );
		// a second writer is still refused
		Check( Open( ":baseq3:pak0.pk3", FOPEN_A ) == -1 && reent._errno == EACCES, "second writer not refused" );
		Check( FakeFM_OpenPaths() == 4, "refused writer left a path" );
	} else if ( !strcmp( name, "read-missing" ) ) {
		CheckFailed( Open( ":baseq3:autoexec.cfg", FOPEN_R ), ENOENT );
		Check( fakeFMCreates == 0 && FakeFM_File( ":baseq3:autoexec.cfg" ) == NULL, "read created a file" );
	} else if ( !strcmp( name, "read-truncate" ) ) {
		// O_TRUNC needs write access, which a read-only open does not have
		FakeFM_AddFile( ":q3config.cfg", 500, 1 );
		fd = Open( ":q3config.cfg", O_RDONLY | O_TRUNC );
		CheckOpened( fd, fsRdPerm );
		Check( fakeFMSetEOFs == 0 && FakeFM_File( ":q3config.cfg" )->eof == 500, "read-only open truncated" );
	} else if ( !strcmp( name, "write" ) ) {
		FakeFM_AddFile( ":q3config.cfg", 500, 0 );
		fd = Open( ":q3config.cfg", FOPEN_W );
		CheckOpened( fd, fsWrPerm );
		Check( FakeFM_File( ":q3config.cfg" )->eof == 0, "w did not truncate" );
	} else if ( !strcmp( name, "write-new" ) ) {
		fd = Open( ":q3config.cfg", FOPEN_W );
		CheckOpened( fd, fsWrPerm );
		Check( FakeFM_File( ":q3config.cfg" ) != NULL, "w did not create" );
		Check( FakeFM_File( ":q3config.cfg" )->type == 'TEXT' &&
			FakeFM_File( ":q3config.cfg" )->creator == 0x3F3F3F3F /* '????' */, "w created with another type or creator" );
	} else if ( !strcmp( name, "write-locked" ) ) {
		FakeFM_AddFile( ":q3config.cfg", 500, 1 );
		CheckFailed( Open( ":q3config.cfg", FOPEN_W ), EACCES );
		Check( FakeFM_File( ":q3config.cfg" )->eof == 500, "locked file truncated" );
		FakeFM_File( ":q3config.cfg" )->locked = 0;
		fakeFMVolume = FAKE_VOLUME_SOFTWARE_LOCKED;
		CheckFailed( Open( ":q3config.cfg", FOPEN_W ), EROFS );
		CheckFailed( Open( ":new.cfg", FOPEN_W ), EROFS );
		Check( FakeFM_File( ":q3config.cfg" )->eof == 500, "file on a locked volume truncated" );
	} else if ( !strcmp( name, "append" ) ) {
		FakeFM_AddFile( ":qconsole.log", 700, 0 );
		fd = Open( ":qconsole.log", FOPEN_A );
		CheckOpened( fd, fsWrPerm );
		Check( fakeFMSetEOFs == 0 && FakeFM_File( ":qconsole.log" )->eof == 700, "a truncated" );
		fd = Open( ":games.log", FOPEN_A );
		CheckOpened( fd, fsWrPerm );
		Check( FakeFM_File( ":games.log" ) != NULL, "a did not create" );
	} else if ( !strcmp( name, "update" ) ) {
		FakeFM_AddFile( ":demo.dm_68", 900, 0 );
		fd = Open( ":demo.dm_68", FOPEN_RPLUS );
		CheckOpened( fd, fsRdWrPerm );
		Check( fakeFMCreates == 0 && FakeFM_File( ":demo.dm_68" )->eof == 900, "r+ created or truncated" );
		FSClose( (short)( fd - kMacRefNumOffset ) );
		CheckFailed( Open( ":missing.dm_68", FOPEN_RPLUS ), ENOENT );
		Check( FakeFM_File( ":missing.dm_68" ) == NULL, "r+ created a file" );
		fd = Open( ":demo.dm_68", FOPEN_WPLUS );
		CheckOpened( fd, fsRdWrPerm );
		Check( FakeFM_File( ":demo.dm_68" )->eof == 0, "w+ did not truncate" );
	} else if ( !strcmp( name, "exclusive" ) ) {
		FakeFM_AddFile( ":screenshot.tga", 300, 0 );
		CheckFailed( Open( ":screenshot.tga", FOPEN_WX ), EEXIST );
		Check( FakeFM_File( ":screenshot.tga" )->eof == 300, "O_EXCL truncated an existing file" );
		fd = Open( ":screenshot0001.tga", FOPEN_WX );
		CheckOpened( fd, fsWrPerm );
	} else if ( !strcmp( name, "hopen-fallback" ) ) {
		// HOpenDF returns paramErr on file systems without it
		FakeFM_AddFile( ":baseq3:pak0.pk3", 1000, 1 );
		fakeFMHOpenDFUnsupported = 1;
		fd = Open( ":baseq3:pak0.pk3", FOPEN_R );
		CheckOpened( fd, fsRdPerm );
		Check( fakeFMOpenDFs == 1 && fakeFMOpens == 1, "no HOpen fallback" );
	} else if ( !strcmp( name, "long-name" ) ) {
		memset( longName, 'a', 255 );
		longName[0] = ':';
		longName[255] = 0;
		FakeFM_AddFile( longName, 10, 0 );
		fd = Open( longName, FOPEN_R );
		CheckOpened( fd, fsRdPerm );
		FSClose( (short)( fd - kMacRefNumOffset ) );
		longName[255] = 'a';
		longName[256] = 0;
		fakeFMOpenDFs = fakeFMOpens = 0;
		CheckFailed( Open( longName, FOPEN_R ), ENAMETOOLONG );
		Check( fakeFMOpenDFs == 0 && fakeFMOpens == 0 && fakeFMCreates == 0, "an over-long name reached the File Manager" );
	} else {
		fprintf( stderr, "unknown case %s\n", name );
		exit( 2 );
	}
}

int main( int argc, char **argv ) {
	int i;

	if ( argc < 2 ) {
		fprintf( stderr, "usage: %s case...\n", argv[0] );
		return 2;
	}
	for ( i = 1 ; i < argc ; i++ ) {
		Case( argv[i] );
	}
	printf( "Mac syscalls regression passed: %d cases\n", argc - 1 );
	return 0;
}
