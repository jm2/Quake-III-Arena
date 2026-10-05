/* A fake File Manager for host regressions of code/mac/mac_syscalls.c, which
 * the regression includes whole; the runner points <reent.h>, <Files.h> and
 * <Errors.h> here.  Types and constants are those of Universal Interfaces 3.4
 * (Retro68 Files.h, MacErrors.h) and newlib's struct _reent, cut to what
 * _open_r uses.
 *
 * The fake follows HFS: every directory is one flat table of files, each
 * with a Finder lock, on one volume that can be software-locked (vLckdErr)
 * or read-only media (wPrErr).  A request for write access to a locked file
 * or volume fails, as does exclusive write access to a file another path
 * can write (opWrErr); fsRdPerm is always granted.  SetEOF needs write
 * access.  fakeFMHOpenDFUnsupported makes HOpenDF return paramErr, as on
 * file systems without it.  Every call is counted and the last permission
 * requested is kept. */
#ifndef MAC_FILES_FAKE_H
#define MAC_FILES_FAKE_H

#include <stdint.h>
#include <string.h>

typedef int16_t			OSErr;
typedef int8_t			SInt8;
typedef uint32_t		OSType;
typedef unsigned char	Str255[256];
typedef const unsigned char	*ConstStr255Param;

struct _reent {
	int	_errno;
};

enum { noErr = 0 };
enum {
	dskFulErr = -34, nsvErr = -35, bdNamErr = -37, tmfoErr = -42, fnfErr = -43,
	wPrErr = -44, fLckdErr = -45, vLckdErr = -46, dupFNErr = -48, opWrErr = -49,
	paramErr = -50, permErr = -54, wrPermErr = -61, dirNFErr = -120,
	afpAccessDenied = -5000
};
enum { fsCurPerm = 0, fsRdPerm = 1, fsWrPerm = 2, fsRdWrPerm = 3, fsRdWrShPerm = 4 };

enum { FAKE_FM_FILES = 8, FAKE_FM_PATHS = 8, FAKE_FM_FIRST_REF = 100 };
enum { FAKE_VOLUME_WRITABLE, FAKE_VOLUME_SOFTWARE_LOCKED, FAKE_VOLUME_READ_ONLY_MEDIA };

typedef struct {
	int		used;
	char	name[256];
	int		locked;
	long	eof;
	OSType	creator, type;
} fakeFMFile_t;

typedef struct {
	int		used;
	int		file;
	SInt8	permission;
} fakeFMPath_t;

static fakeFMFile_t	fakeFMFiles[FAKE_FM_FILES];
static fakeFMPath_t	fakeFMPaths[FAKE_FM_PATHS];
static int			fakeFMVolume;
static int			fakeFMHOpenDFUnsupported;
static int			fakeFMCreates, fakeFMOpenDFs, fakeFMOpens, fakeFMSetEOFs, fakeFMCloses;
static int			fakeFMLastPermission = -1;

const int kMacRefNumOffset = 10;	/* libretro/syscalls.c */

static void FakeFM_Reset( void ) {
	memset( fakeFMFiles, 0, sizeof( fakeFMFiles ) );
	memset( fakeFMPaths, 0, sizeof( fakeFMPaths ) );
	fakeFMVolume = FAKE_VOLUME_WRITABLE;
	fakeFMHOpenDFUnsupported = 0;
	fakeFMCreates = fakeFMOpenDFs = fakeFMOpens = fakeFMSetEOFs = fakeFMCloses = 0;
	fakeFMLastPermission = -1;
}

static int FakeFM_AddFile( const char *name, long eof, int locked ) {
	int i;

	for ( i = 0 ; i < FAKE_FM_FILES ; i++ ) {
		if ( !fakeFMFiles[i].used ) {
			fakeFMFiles[i].used = 1;
			strcpy( fakeFMFiles[i].name, name );
			fakeFMFiles[i].eof = eof;
			fakeFMFiles[i].locked = locked;
			return i;
		}
	}
	return -1;
}

static int FakeFM_Find( ConstStr255Param name ) {
	int i;

	for ( i = 0 ; i < FAKE_FM_FILES ; i++ ) {
		if ( fakeFMFiles[i].used && strlen( fakeFMFiles[i].name ) == name[0] &&
			!memcmp( fakeFMFiles[i].name, name + 1, name[0] ) ) {
			return i;
		}
	}
	return -1;
}

static fakeFMFile_t *FakeFM_File( const char *name ) {
	Str255 pname;
	int i;

	pname[0] = (unsigned char)strlen( name );
	memcpy( pname + 1, name, pname[0] );
	i = FakeFM_Find( pname );
	return i < 0 ? NULL : &fakeFMFiles[i];
}

static fakeFMPath_t *FakeFM_Path( short refNum ) {
	int i = refNum - FAKE_FM_FIRST_REF;

	if ( i < 0 || i >= FAKE_FM_PATHS || !fakeFMPaths[i].used ) {
		return NULL;
	}
	return &fakeFMPaths[i];
}

static int FakeFM_OpenPaths( void ) {
	int i, count = 0;

	for ( i = 0 ; i < FAKE_FM_PATHS ; i++ ) {
		count += fakeFMPaths[i].used;
	}
	return count;
}

static OSErr FakeFM_VolumeWriteErr( void ) {
	if ( fakeFMVolume == FAKE_VOLUME_READ_ONLY_MEDIA ) {
		return wPrErr;
	}
	if ( fakeFMVolume == FAKE_VOLUME_SOFTWARE_LOCKED ) {
		return vLckdErr;
	}
	return noErr;
}

static OSErr HCreate( short vRefNum, long dirID, ConstStr255Param fileName, OSType creator, OSType fileType ) {
	OSErr err;
	char name[256];
	int i;

	fakeFMCreates++;
	if ( vRefNum != 0 || dirID != 0 ) {
		return paramErr;
	}
	if ( ( err = FakeFM_VolumeWriteErr() ) != noErr ) {
		return err;
	}
	if ( FakeFM_Find( fileName ) >= 0 ) {
		return dupFNErr;
	}
	memcpy( name, fileName + 1, fileName[0] );
	name[fileName[0]] = 0;
	i = FakeFM_AddFile( name, 0, 0 );
	if ( i < 0 ) {
		return dskFulErr;
	}
	fakeFMFiles[i].creator = creator;
	fakeFMFiles[i].type = fileType;
	return noErr;
}

static OSErr FakeFM_Open( ConstStr255Param fileName, SInt8 permission, short *refNum ) {
	int writes = permission != fsRdPerm;
	OSErr err;
	int file, i;

	fakeFMLastPermission = permission;
	file = FakeFM_Find( fileName );
	if ( file < 0 ) {
		return fnfErr;
	}
	if ( writes ) {
		if ( ( err = FakeFM_VolumeWriteErr() ) != noErr ) {
			return err;
		}
		if ( fakeFMFiles[file].locked ) {
			return permErr;
		}
		for ( i = 0 ; i < FAKE_FM_PATHS ; i++ ) {
			if ( fakeFMPaths[i].used && fakeFMPaths[i].file == file &&
				fakeFMPaths[i].permission != fsRdPerm &&
				!( permission == fsRdWrShPerm && fakeFMPaths[i].permission == fsRdWrShPerm ) ) {
				*refNum = (short)( FAKE_FM_FIRST_REF + i );
				return opWrErr;
			}
		}
	}
	for ( i = 0 ; i < FAKE_FM_PATHS ; i++ ) {
		if ( !fakeFMPaths[i].used ) {
			fakeFMPaths[i].used = 1;
			fakeFMPaths[i].file = file;
			fakeFMPaths[i].permission = permission;
			*refNum = (short)( FAKE_FM_FIRST_REF + i );
			return noErr;
		}
	}
	return tmfoErr;
}

static OSErr HOpenDF( short vRefNum, long dirID, ConstStr255Param fileName, SInt8 permission, short *refNum ) {
	fakeFMOpenDFs++;
	if ( fakeFMHOpenDFUnsupported ) {
		return paramErr;
	}
	if ( vRefNum != 0 || dirID != 0 ) {
		return paramErr;
	}
	return FakeFM_Open( fileName, permission, refNum );
}

static OSErr HOpen( short vRefNum, long dirID, ConstStr255Param fileName, SInt8 permission, short *refNum ) {
	fakeFMOpens++;
	if ( vRefNum != 0 || dirID != 0 ) {
		return paramErr;
	}
	return FakeFM_Open( fileName, permission, refNum );
}

static OSErr SetEOF( short refNum, long logEOF ) {
	fakeFMPath_t *path = FakeFM_Path( refNum );

	fakeFMSetEOFs++;
	if ( !path ) {
		return paramErr;
	}
	if ( path->permission == fsRdPerm ) {
		return wrPermErr;
	}
	fakeFMFiles[path->file].eof = logEOF;
	return noErr;
}

static OSErr FSClose( short refNum ) {
	fakeFMPath_t *path = FakeFM_Path( refNum );

	fakeFMCloses++;
	if ( !path ) {
		return paramErr;
	}
	path->used = 0;
	return noErr;
}

#endif
