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
 * other opens of the same file; write opens must still fail there.
 *
 * Issue #259: libretro's _rename_r and _unlink_r only return -1, so every
 * download was finished by copying the whole .pk3.tmp and the .tmp was
 * never removed.  The rename-* and unlink-* cases drive the real _rename_r
 * and _unlink_r: within a directory and across directories, onto an
 * existing file (replaced, with its old contents gone, or left as it was if
 * the rename fails), with locked files, open files, locked volumes, missing
 * sources and directories, two volumes, and names too long for a Str255 or
 * an HFS leaf.  A rename either completes or leaves both names as they
 * were.
 *
 * Review of #492: the rename onto an existing file found "the same file" by
 * comparing names, so on a file system that folds two names into one entry
 * (HFS truncating leaves past 31 bytes, issue #327) it exchanged the file
 * with itself and deleted it.  rename-same-entry checks the catalog ID
 * comparison that replaced it; long-leaf checks that a path component
 * longer than 31 bytes never reaches the File Manager, which might
 * truncate it; rename-exists-exchange-fails checks that only paramErr and
 * wrgVolTypErr from FSpExchangeFiles fall back to deleting dest. */
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

#define LONG_STEM	"q3dm17_the_longest_yard_rmx"	/* 27 bytes */

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

static int Rename( const char *from, const char *to ) {
	return _rename_r( &reent, from, to );
}

static int Unlink( const char *name ) {
	return _unlink_r( &reent, name );
}

/* The file exists with this length (its contents in the fake), or not at all. */
static void CheckFile( const char *name, long eof, const char *what ) {
	fakeFMFile_t *file = FakeFM_File( name );

	Check( eof < 0 ? file == NULL : file != NULL && file->eof == eof, what );
}

static void CheckRenamed( int result, const char *from, const char *to, long eof ) {
	Check( result == 0, "rename failed" );
	CheckFile( from, -1, "the old name is still there" );
	CheckFile( to, eof, "the new name does not hold the old file" );
}

static void CheckRenameFailed( int result, int expectedErrno ) {
	Check( result == -1, "rename succeeded" );
	Check( reent._errno == expectedErrno, "wrong errno" );
}

/* The rename cases; the files are left for the caller to check. */
static int RenameCase( const char *name ) {
	char longName[300];
	int fd;

	if ( !strcmp( name, "rename" ) ) {
		// the download finalisation: .pk3.tmp -> .pk3 in the same directory
		FakeFM_AddFile( ":baseq3:map_dl.pk3.tmp", 1234, 0 );
		CheckRenamed( Rename( ":baseq3:map_dl.pk3.tmp", ":baseq3:map_dl.pk3" ),
			":baseq3:map_dl.pk3.tmp", ":baseq3:map_dl.pk3", 1234 );
		Check( fakeFMMoves == 0 && fakeFMExchanges == 0 && fakeFMDeletes == 0, "a plain rename moved, exchanged or deleted" );
		Check( fakeFMCreates == 0 && fakeFMOpenDFs == 0, "rename copied" );
		// a full path, as fs_homepath makes on the Mac, and the same file
		FakeFM_AddFile( "Macintosh HD:Quake3:baseq3:q3config.cfg", 50, 0 );
		CheckRenamed( Rename( "Macintosh HD:Quake3:baseq3:q3config.cfg", "Macintosh HD:Quake3:baseq3:old.cfg" ),
			"Macintosh HD:Quake3:baseq3:q3config.cfg", "Macintosh HD:Quake3:baseq3:old.cfg", 50 );
		Check( Rename( "Macintosh HD:Quake3:baseq3:old.cfg", "Macintosh HD:Quake3:baseq3:old.cfg" ) == 0, "rename onto itself failed" );
		CheckFile( "Macintosh HD:Quake3:baseq3:old.cfg", 50, "rename onto itself lost the file" );
		// HFS names ignore case: a change of case renames the same file
		Check( Rename( "Macintosh HD:Quake3:baseq3:old.cfg", "Macintosh HD:Quake3:baseq3:OLD.cfg" ) == 0, "change of case failed" );
		Check( !strcmp( FakeFM_File( "Macintosh HD:Quake3:baseq3:old.cfg" )->name, "Macintosh HD:Quake3:baseq3:OLD.cfg" ), "change of case not made" );
		Check( fakeFMExchanges == 0 && fakeFMDeletes == 0, "rename onto the same file exchanged or deleted it" );
	} else if ( !strcmp( name, "rename-cross-dir" ) ) {
		FakeFM_AddFile( ":baseq3:demos:demo.tmp", 900, 0 );
		FakeFM_AddDir( ":baseq3:", FAKE_FM_VREFNUM );
		CheckRenamed( Rename( ":baseq3:demos:demo.tmp", ":baseq3:demo0001.dm_68" ),
			":baseq3:demos:demo.tmp", ":baseq3:demo0001.dm_68", 900 );
		Check( fakeFMMoves == 1 && fakeFMRenames == 1, "not a CatMove and an HRename" );
		// the same leaf: only a move
		FakeFM_AddDir( ":baseq3:screenshots:", FAKE_FM_VREFNUM );
		CheckRenamed( Rename( ":baseq3:demo0001.dm_68", ":baseq3:screenshots:demo0001.dm_68" ),
			":baseq3:demo0001.dm_68", ":baseq3:screenshots:demo0001.dm_68", 900 );
		Check( fakeFMMoves == 2 && fakeFMRenames == 1, "a move with the same leaf renamed" );
		// into a missing directory: nothing moves
		CheckRenameFailed( Rename( ":baseq3:screenshots:demo0001.dm_68", ":baseq3:missing:demo.dm_68" ), ENOENT );
		CheckFile( ":baseq3:screenshots:demo0001.dm_68", 900, "a failed move lost the source" );
		// onto a name the destination directory already has for another file
		// the source's leaf: the move is refused and nothing changes
		FakeFM_AddFile( ":baseq3:demo0001.dm_68", 5, 0 );
		FakeFM_AddFile( ":baseq3:screenshots:other.dm_68", 6, 0 );
		FakeFM_AddDir( ":baseq3:demos:", FAKE_FM_VREFNUM );
		CheckRenameFailed( Rename( ":baseq3:screenshots:demo0001.dm_68", ":baseq3:new.dm_68" ), EEXIST );
		CheckFile( ":baseq3:screenshots:demo0001.dm_68", 900, "a refused move lost the source" );
		CheckFile( ":baseq3:new.dm_68", -1, "a refused move made the destination" );
		CheckFile( ":baseq3:demo0001.dm_68", 5, "a refused move changed the other file" );
	} else if ( !strcmp( name, "rename-exists" ) ) {
		// POSIX: an existing destination is replaced in one step
		FakeFM_AddFile( ":baseq3:map_dl.pk3.tmp", 1234, 0 );
		FakeFM_AddFile( ":baseq3:map_dl.pk3", 999, 0 );
		CheckRenamed( Rename( ":baseq3:map_dl.pk3.tmp", ":baseq3:map_dl.pk3" ),
			":baseq3:map_dl.pk3.tmp", ":baseq3:map_dl.pk3", 1234 );
		Check( fakeFMExchanges == 1 && fakeFMDeletes == 1, "not replaced by FSpExchangeFiles" );
		// across directories
		FakeFM_AddFile( ":baseq3:demos:demo.tmp", 900, 0 );
		FakeFM_AddFile( ":baseq3:demo.dm_68", 800, 0 );
		CheckRenamed( Rename( ":baseq3:demos:demo.tmp", ":baseq3:demo.dm_68" ),
			":baseq3:demos:demo.tmp", ":baseq3:demo.dm_68", 900 );
		Check( FakeFM_File( ":baseq3:demos:demo.tmp" ) == NULL && fakeFMMoves == 0, "cross-directory replace moved" );
		// a source left open stays open on the moved contents
		FakeFM_AddFile( ":baseq3:a.cfg", 11, 0 );
		FakeFM_AddFile( ":baseq3:b.cfg", 22, 0 );
		fd = Open( ":baseq3:a.cfg", FOPEN_R );
		CheckRenamed( Rename( ":baseq3:a.cfg", ":baseq3:b.cfg" ), ":baseq3:a.cfg", ":baseq3:b.cfg", 11 );
		Check( fakeFMFiles[FakeFM_Path( (short)( fd - kMacRefNumOffset ) )->file].eof == 11, "the open path lost its contents" );
	} else if ( !strcmp( name, "rename-exists-no-exchange" ) ) {
		// volumes without FSpExchangeFiles: delete the destination, then rename
		fakeFMExchangeErr = paramErr;
		FakeFM_AddFile( ":baseq3:map_dl.pk3.tmp", 1234, 0 );
		FakeFM_AddFile( ":baseq3:map_dl.pk3", 999, 0 );
		CheckRenamed( Rename( ":baseq3:map_dl.pk3.tmp", ":baseq3:map_dl.pk3" ),
			":baseq3:map_dl.pk3.tmp", ":baseq3:map_dl.pk3", 1234 );
		Check( fakeFMExchanges == 1 && fakeFMDeletes == 1 && fakeFMRenames == 1, "not deleted and renamed" );
		FakeFM_AddFile( ":baseq3:demos:demo.tmp", 900, 0 );
		FakeFM_AddFile( ":baseq3:demo.dm_68", 800, 0 );
		CheckRenamed( Rename( ":baseq3:demos:demo.tmp", ":baseq3:demo.dm_68" ),
			":baseq3:demos:demo.tmp", ":baseq3:demo.dm_68", 900 );
		// a destination that cannot be deleted stays, and so does the source
		FakeFM_AddFile( ":baseq3:a.cfg", 11, 0 );
		FakeFM_AddFile( ":baseq3:b.cfg", 22, 1 );
		CheckRenameFailed( Rename( ":baseq3:a.cfg", ":baseq3:b.cfg" ), EACCES );
		CheckFile( ":baseq3:a.cfg", 11, "the source changed" );
		CheckFile( ":baseq3:b.cfg", 22, "the locked destination changed" );
		// wrgVolTypErr falls back too
		FakeFM_File( ":baseq3:b.cfg" )->locked = 0;
		fakeFMExchangeErr = wrgVolTypErr;
		CheckRenamed( Rename( ":baseq3:a.cfg", ":baseq3:b.cfg" ), ":baseq3:a.cfg", ":baseq3:b.cfg", 11 );
	} else if ( !strcmp( name, "rename-exists-exchange-fails" ) ) {
		// any other exchange failure leaves both files: dest is not deleted
		FakeFM_AddFile( ":baseq3:map_dl.pk3.tmp", 1234, 0 );
		FakeFM_AddFile( ":baseq3:map_dl.pk3", 999, 0 );
		fakeFMExchangeErr = ioErr;
		CheckRenameFailed( Rename( ":baseq3:map_dl.pk3.tmp", ":baseq3:map_dl.pk3" ), EIO );
		fakeFMExchangeErr = afpAccessDenied;
		CheckRenameFailed( Rename( ":baseq3:map_dl.pk3.tmp", ":baseq3:map_dl.pk3" ), EACCES );
		CheckFile( ":baseq3:map_dl.pk3.tmp", 1234, "a failed exchange changed the source" );
		CheckFile( ":baseq3:map_dl.pk3", 999, "a failed exchange deleted the destination" );
		Check( fakeFMExchanges == 2 && fakeFMDeletes == 0 && fakeFMRenames == 0 && fakeFMMoves == 0,
			"a failed exchange went on to delete or rename" );
	} else if ( !strcmp( name, "rename-same-entry" ) ) {
		// a file system that keeps 12 bytes of a leaf: two names, one file
		fakeFMNameLimit = 12;
		FakeFM_AddFile( ":baseq3:map_download", 1234, 0 );
		Check( FakeFM_File( ":baseq3:map_download.pk3" ) == FakeFM_File( ":baseq3:map_download.tmp" ), "the names do not fold" );
		Check( Rename( ":baseq3:map_download.tmp", ":baseq3:map_download.pk3" ) == 0, "renaming a file onto itself failed" );
		CheckFile( ":baseq3:map_download", 1234, "renaming a file onto itself lost it" );
		// on a volume without FSpExchangeFiles the fallback would delete dest
		fakeFMExchangeErr = paramErr;
		Check( Rename( ":baseq3:map_download.tmp", ":baseq3:map_download.pk3" ) == 0, "renaming a file onto itself failed" );
		CheckFile( ":baseq3:map_download", 1234, "renaming a file onto itself lost it" );
		Check( fakeFMExchanges == 0 && fakeFMDeletes == 0 && fakeFMRenames == 0, "a file was renamed onto itself" );
		Check( fakeFMCatInfos == 4, "the entries were not compared by catalog ID" );
		// two files whose names differ only past the limit are still one file,
		// and two different files are still replaced
		fakeFMExchangeErr = noErr;
		FakeFM_AddFile( ":baseq3:other.cfg", 22, 0 );
		CheckRenamed( Rename( ":baseq3:map_download.tmp", ":baseq3:other.cfg" ),
			":baseq3:map_download", ":baseq3:other.cfg", 1234 );
	} else if ( !strcmp( name, "rename-exists-busy" ) ) {
		// the destination is open: its old contents cannot be deleted, so the
		// exchange is undone and both files keep their contents
		FakeFM_AddFile( ":baseq3:map_dl.pk3.tmp", 1234, 0 );
		FakeFM_AddFile( ":baseq3:map_dl.pk3", 999, 0 );
		fd = Open( ":baseq3:map_dl.pk3", FOPEN_R );
		CheckRenameFailed( Rename( ":baseq3:map_dl.pk3.tmp", ":baseq3:map_dl.pk3" ), EBUSY );
		CheckFile( ":baseq3:map_dl.pk3.tmp", 1234, "the source changed" );
		CheckFile( ":baseq3:map_dl.pk3", 999, "the open destination changed" );
		Check( FakeFM_Path( (short)( fd - kMacRefNumOffset ) )->file == FakeFM_File( ":baseq3:map_dl.pk3" ) - fakeFMFiles,
			"the open path no longer reads the destination" );
		Check( fakeFMExchanges == 2, "the exchange was not undone" );
	} else if ( !strcmp( name, "rename-locked" ) ) {
		FakeFM_AddFile( ":baseq3:a.cfg", 11, 1 );
		CheckRenameFailed( Rename( ":baseq3:a.cfg", ":baseq3:b.cfg" ), EACCES );
		CheckFile( ":baseq3:a.cfg", 11, "a locked file was renamed" );
		// across directories the move is undone when the rename is refused
		FakeFM_AddDir( ":baseq3:demos:", FAKE_FM_VREFNUM );
		CheckRenameFailed( Rename( ":baseq3:a.cfg", ":baseq3:demos:b.cfg" ), EACCES );
		CheckFile( ":baseq3:a.cfg", 11, "the move of a locked file was not undone" );
		CheckFile( ":baseq3:demos:a.cfg", -1, "the move of a locked file was left" );
		Check( fakeFMMoves == 2, "the move was not undone by a second move" );
		// a locked destination is not replaced
		FakeFM_File( ":baseq3:a.cfg" )->locked = 0;
		FakeFM_AddFile( ":baseq3:b.cfg", 22, 1 );
		CheckRenameFailed( Rename( ":baseq3:a.cfg", ":baseq3:b.cfg" ), EACCES );
		CheckFile( ":baseq3:a.cfg", 11, "the source changed" );
		CheckFile( ":baseq3:b.cfg", 22, "a locked destination was replaced" );
		// a locked volume or CD
		FakeFM_File( ":baseq3:b.cfg" )->locked = 0;
		fakeFMVolume = FAKE_VOLUME_SOFTWARE_LOCKED;
		CheckRenameFailed( Rename( ":baseq3:a.cfg", ":baseq3:c.cfg" ), EROFS );
		CheckRenameFailed( Rename( ":baseq3:a.cfg", ":baseq3:b.cfg" ), EROFS );
		fakeFMVolume = FAKE_VOLUME_READ_ONLY_MEDIA;
		CheckRenameFailed( Rename( ":baseq3:a.cfg", ":baseq3:demos:a.cfg" ), EROFS );
		CheckFile( ":baseq3:a.cfg", 11, "a file on a locked volume changed" );
		CheckFile( ":baseq3:b.cfg", 22, "a file on a locked volume changed" );
	} else if ( !strcmp( name, "rename-missing" ) ) {
		FakeFM_AddFile( ":baseq3:b.cfg", 22, 0 );
		CheckRenameFailed( Rename( ":baseq3:a.cfg", ":baseq3:b.cfg" ), ENOENT );
		CheckRenameFailed( Rename( ":missing:a.cfg", ":baseq3:c.cfg" ), ENOENT );
		CheckFile( ":baseq3:b.cfg", 22, "a missing source changed the destination" );
		Check( fakeFMRenames == 0 && fakeFMDeletes == 0 && fakeFMExchanges == 0 && fakeFMMoves == 0, "a missing source reached a rename" );
	} else if ( !strcmp( name, "rename-other-volume" ) ) {
		FakeFM_AddFile( ":baseq3:a.cfg", 11, 0 );
		FakeFM_AddDir( "Other:", -2 );
		CheckRenameFailed( Rename( ":baseq3:a.cfg", "Other:a.cfg" ), EXDEV );
		CheckFile( ":baseq3:a.cfg", 11, "a cross-volume rename changed the source" );
	} else if ( !strcmp( name, "rename-long-name" ) ) {
		FakeFM_AddFile( ":baseq3:a.cfg", 11, 0 );
		memset( longName, 'a', 256 );
		longName[0] = ':';
		longName[256] = 0;
		CheckRenameFailed( Rename( ":baseq3:a.cfg", longName ), ENAMETOOLONG );
		CheckRenameFailed( Rename( longName, ":baseq3:a.cfg" ), ENAMETOOLONG );
		Check( fakeFMRenames == 0 && fakeFMMoves == 0 && fakeFMDeletes == 0 && fakeFMExchanges == 0,
			"an over-long name reached the File Manager" );
		// a leaf longer than HFS's 31 characters
		CheckRenameFailed( Rename( ":baseq3:a.cfg", ":baseq3:a_map_with_a_rather_long_name.pk3" ), ENAMETOOLONG );
		CheckFile( ":baseq3:a.cfg", 11, "a refused long leaf changed the source" );
		Check( fakeFMRenames == 0 && fakeFMMoves == 0 && fakeFMDeletes == 0 && fakeFMExchanges == 0,
			"a long leaf reached the File Manager" );
	} else {
		return 0;
	}
	Check( FakeFM_OpenPaths() <= 1, "rename left a path open" );
	return 1;
}

static int UnlinkCase( const char *name ) {
	char longName[300];
	int fd;

	if ( !strcmp( name, "unlink" ) ) {
		// FS_Remove's remove( ".pk3.tmp" ) after a copied download
		FakeFM_AddFile( ":baseq3:map_dl.pk3.tmp", 1234, 0 );
		FakeFM_AddFile( "Macintosh HD:Quake3:baseq3:map_dl.pk3.tmp", 1234, 0 );
		Check( Unlink( ":baseq3:map_dl.pk3.tmp" ) == 0, "unlink failed" );
		CheckFile( ":baseq3:map_dl.pk3.tmp", -1, "unlink left the file" );
		Check( Unlink( "Macintosh HD:Quake3:baseq3:map_dl.pk3.tmp" ) == 0, "unlink of a full path failed" );
		CheckFile( "Macintosh HD:Quake3:baseq3:map_dl.pk3.tmp", -1, "unlink left the file" );
		Check( Unlink( ":baseq3:map_dl.pk3.tmp" ) == -1 && reent._errno == ENOENT, "a second unlink is not ENOENT" );
		Check( Unlink( ":missing:map_dl.pk3.tmp" ) == -1 && reent._errno == ENOENT, "unlink in a missing directory is not ENOENT" );
	} else if ( !strcmp( name, "unlink-locked" ) ) {
		FakeFM_AddFile( ":baseq3:a.cfg", 11, 1 );
		Check( Unlink( ":baseq3:a.cfg" ) == -1 && reent._errno == EACCES, "unlink of a locked file is not EACCES" );
		FakeFM_File( ":baseq3:a.cfg" )->locked = 0;
		fd = Open( ":baseq3:a.cfg", FOPEN_R );
		Check( Unlink( ":baseq3:a.cfg" ) == -1 && reent._errno == EBUSY, "unlink of an open file is not EBUSY" );
		FSClose( (short)( fd - kMacRefNumOffset ) );
		fakeFMVolume = FAKE_VOLUME_SOFTWARE_LOCKED;
		Check( Unlink( ":baseq3:a.cfg" ) == -1 && reent._errno == EROFS, "unlink on a locked volume is not EROFS" );
		CheckFile( ":baseq3:a.cfg", 11, "a refused unlink deleted the file" );
	} else if ( !strcmp( name, "unlink-long-name" ) ) {
		memset( longName, 'a', 256 );
		longName[0] = ':';
		longName[256] = 0;
		Check( Unlink( longName ) == -1 && reent._errno == ENAMETOOLONG, "unlink of an over-long name is not ENAMETOOLONG" );
		Check( fakeFMDeletes == 0, "an over-long name reached the File Manager" );
	} else {
		return 0;
	}
	Check( FakeFM_OpenPaths() == 0, "unlink left a path open" );
	return 1;
}

static void Case( const char *name ) {
	int fd, fd2;
	char longName[300];

	Begin( name );
	if ( RenameCase( name ) || UnlinkCase( name ) ) {
		return;
	}
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
		// 255 bytes in components of 31
		for ( fd = 0 ; fd < 255 ; fd++ ) {
			longName[fd] = fd % 32 ? 'a' : ':';
		}
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
	} else if ( !strcmp( name, "long-leaf" ) ) {
		// issue #327: if the File Manager cuts leaves to 31 bytes, a 32-byte
		// component must not reach it
		fakeFMNameLimit = 31;
		CheckFailed( Open( ":baseq3:" LONG_STEM ".qvm.pk3.tmp", FOPEN_W ), ENAMETOOLONG );
		Check( FakeFM_File( ":baseq3:" LONG_STEM ".qvm" ) == NULL, "a long leaf created a cut name" );
		FakeFM_AddFile( ":baseq3:" LONG_STEM ".qvm", 10, 0 );
		CheckFailed( Open( ":baseq3:" LONG_STEM ".qvm.pk3.tmp", FOPEN_R ), ENAMETOOLONG );
		CheckFailed( Open( ":baseq3:" LONG_STEM ".qvm.pk3.tmp", FOPEN_A ), ENAMETOOLONG );
		Check( Rename( ":baseq3:" LONG_STEM ".qvm.pk3.tmp", ":baseq3:" LONG_STEM ".qvm.pk3" ) == -1 &&
			reent._errno == ENAMETOOLONG, "a rename of two long leaves is not ENAMETOOLONG" );
		Check( Rename( ":baseq3:" LONG_STEM ".qvm", ":baseq3:" LONG_STEM ".qvm.pk3" ) == -1 &&
			reent._errno == ENAMETOOLONG, "a rename to a long leaf is not ENAMETOOLONG" );
		Check( Unlink( ":baseq3:" LONG_STEM ".qvm.pk3.tmp" ) == -1 && reent._errno == ENAMETOOLONG,
			"an unlink of a long leaf is not ENAMETOOLONG" );
		CheckFile( ":baseq3:" LONG_STEM ".qvm", 10, "a long leaf changed the file it would be cut to" );
		// a directory too
		CheckFailed( Open( ":baseq3:" LONG_STEM ".qvm.pk3:a.cfg", FOPEN_W ), ENAMETOOLONG );
		// 31 bytes are passed on
		fd = Open( ":baseq3:" LONG_STEM ".qvm", FOPEN_R );
		CheckOpened( fd, fsRdPerm );
		Check( fakeFMCreates == 0 && fakeFMOpenDFs == 1 && fakeFMRenames == 0 && fakeFMDeletes == 0 &&
			fakeFMExchanges == 0 && fakeFMMoves == 0, "a long leaf reached the File Manager" );
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
