/* Issue #267: with the game at the root of a volume (a CD, or a disk image
 * named "Quake3"), Sys_GetCwd returned the bare volume name "Quake3".  HFS
 * reads a colon-free name as an item in the default directory, not as the
 * volume, so Sys_ListFiles( fs_basepath ) found nothing: the Mods menu and
 * dir listings were empty, while "Quake3:baseq3:pak0.pk3" still resolved.
 *
 * A volume root is now "Quake3:", and FS_BuildOSPath and Sys_JoinHFSPath
 * (the MacQuake3Parms.txt path) add no second colon after it ("Quake3::"
 * would name the volume's parent).  A folder ("Macintosh HD:Games:Quake3")
 * is unchanged, and a hand-set colon-free fs_basepath lists its volume.
 *
 * The runner extracts the real Sys_GetCwd, Sys_JoinHFSPath, PathToFSSpec,
 * Sys_GetDirectoryID, Sys_ListFilteredDirectory, Sys_ListFiles and
 * PStringToCString (mac_main.c), FS_ReplaceSeparators and FS_BuildOSPath
 * (files.c, with PATH_SEP ':') and Com_StringContains, Com_Filter and
 * Com_FilterPath (common.c).  The fake File Manager below keeps a catalog of
 * two volumes and resolves paths as HFS does: a name with a colon after its
 * first character is absolute and starts with a volume name, a name without
 * one (or starting with ':') is relative to the default directory, and each
 * colon after another walks up.  mac_files_fake.h, which mac_syscalls.c's
 * regression uses, has no directory entries, indexed enumeration or
 * directory IDs by name, which Sys_GetCwd and Sys_ListFiles need.
 *
 * Sys_GetCwd caches its result, so the runner starts one process per case:
 *   root    the application at the root of the volume "Quake3"
 *   folder  the application in "Macintosh HD:Games:Quake3"
 *   getvol  the Process Manager returns an empty FSSpec (GetVol fallback) */
#include "../code/game/q_shared.h"
#include "../code/qcommon/qcommon.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#undef PATH_SEP
#define PATH_SEP ':'

/* ---- fake Toolbox ---- */

typedef short			OSErr;
typedef signed char		SInt8;
typedef unsigned char	Str255[256];
typedef unsigned char	Str63[64];
typedef unsigned char	*StringPtr;
typedef const unsigned char	*ConstStr255Param;

enum { noErr = 0, nsvErr = -35, bdNamErr = -37, fnfErr = -43, paramErr = -50, dirNFErr = -120 };
enum { fsRtParID = 1, fsRtDirID = 2 };
enum { ioDirMask = 0x10 };

typedef struct FSSpec {
	short	vRefNum;
	long	parID;
	Str63	name;
} FSSpec;

/* HFileInfo and DirInfo overlay as in Files.h: ioDirID and ioDrDirID, and
 * ioFlParID and ioDrParID, are the same fields */
typedef struct HFileInfo {
	StringPtr	ioNamePtr;
	short		ioVRefNum;
	short		ioFDirIndex;
	SInt8		ioFlAttrib;
	long		ioDirID;
	long		ioFlParID;
} HFileInfo;

typedef struct DirInfo {
	StringPtr	ioNamePtr;
	short		ioVRefNum;
	short		ioFDirIndex;
	SInt8		ioFlAttrib;
	long		ioDrDirID;
	long		ioDrParID;
} DirInfo;

typedef union CInfoPBRec {
	HFileInfo	hFileInfo;
	DirInfo		dirInfo;
} CInfoPBRec;

typedef struct { unsigned long highLongOfPSN, lowLongOfPSN; } ProcessSerialNumber;
typedef struct {
	unsigned long	processInfoLength;
	StringPtr		processName;
	FSSpec			*processAppSpec;
} ProcessInfoRec;

typedef struct {
	short		vRefNum;
	long		id;			/* directory ID, or file number */
	long		parID;		/* fsRtParID for a volume's root */
	const char	*name;
	int			isDir;
} fakeEntry_t;

#define HD	-1
#define CD	-2

static const fakeEntry_t fakeCatalog[] = {
	{ HD, fsRtDirID, fsRtParID, "Macintosh HD", 1 },
	{ HD, 16, fsRtDirID, "Games", 1 },
	{ HD, 17, 16, "Quake3", 1 },
	{ HD, 18, 17, "baseq3", 1 },
	{ HD, 19, 17, "mymod", 1 },
	{ HD, 100, 17, "Quake3 PPC", 0 },
	{ HD, 101, 17, "MacQuake3Parms.txt", 0 },
	{ HD, 102, 18, "pak0.pk3", 0 },
	{ HD, 103, 18, "q3config.cfg", 0 },
	{ HD, 104, 19, "zz-mod.pk3", 0 },
	{ HD, 105, fsRtDirID, "Read Me", 0 },

	{ CD, fsRtDirID, fsRtParID, "Quake3", 1 },
	{ CD, 16, fsRtDirID, "baseq3", 1 },
	{ CD, 17, fsRtDirID, "missionpack", 1 },
	{ CD, 100, fsRtDirID, "Quake3 PPC", 0 },
	{ CD, 101, fsRtDirID, "MacQuake3Parms.txt", 0 },
	{ CD, 102, 16, "pak0.pk3", 0 },
	{ CD, 103, 16, "pak1.pk3", 0 },
	{ CD, 104, 17, "pak0.pk3", 0 },
};
#define FAKE_ENTRIES	(int)( sizeof( fakeCatalog ) / sizeof( fakeCatalog[0] ) )

static short	defaultVRefNum;		/* the default directory, which the */
static long		defaultDirID;		/* Finder makes the application's folder */
static FSSpec	appSpec;			/* what GetProcessInformation returns */

static int FakeFM_Lookup( short vRefNum, long dirID, const char *name, size_t length ) {
	int i;

	for ( i = 0 ; i < FAKE_ENTRIES ; i++ ) {
		if ( fakeCatalog[i].vRefNum == vRefNum && fakeCatalog[i].parID == dirID &&
			strlen( fakeCatalog[i].name ) == length &&
			!strncasecmp( fakeCatalog[i].name, name, length ) ) {
			return i;
		}
	}
	return -1;
}

static int FakeFM_Dir( short vRefNum, long dirID ) {
	int i;

	for ( i = 0 ; i < FAKE_ENTRIES ; i++ ) {
		if ( fakeCatalog[i].vRefNum == vRefNum && fakeCatalog[i].isDir &&
			fakeCatalog[i].id == dirID ) {
			return i;
		}
	}
	return -1;
}

static void FakeFM_PString( StringPtr dest, const char *s, size_t length ) {
	dest[0] = (unsigned char)length;
	memcpy( dest + 1, s, length );
}

static OSErr FSMakeFSSpec( short vRefNum, long dirID, ConstStr255Param fileName, FSSpec *spec ) {
	char path[256], *s, *e;
	short vol;
	long dir;
	int entry, i;

	if ( vRefNum != 0 || dirID != 0 || !fileName[0] ) {
		return paramErr;
	}
	memcpy( path, fileName + 1, fileName[0] );
	path[fileName[0]] = 0;

	s = path;
	e = strchr( path, ':' );
	if ( e && e != path ) {
		/* a full path: the first name is a volume's */
		for ( i = 0 ; i < FAKE_ENTRIES ; i++ ) {
			if ( fakeCatalog[i].parID == fsRtParID &&
				strlen( fakeCatalog[i].name ) == (size_t)( e - path ) &&
				!strncasecmp( fakeCatalog[i].name, path, e - path ) ) {
				break;
			}
		}
		if ( i == FAKE_ENTRIES ) {
			return nsvErr;
		}
		vol = fakeCatalog[i].vRefNum;
		dir = fsRtParID;
	} else {
		/* a partial path, or a bare name: in the default directory */
		vol = defaultVRefNum;
		dir = defaultDirID;
		if ( *s == ':' ) {
			s++;
		}
	}

	for ( ;; ) {
		e = strchr( s, ':' );
		if ( !e ) {
			e = s + strlen( s );
		}
		if ( e == s ) {
			/* a colon after a colon: the parent */
			entry = FakeFM_Dir( vol, dir );
			if ( entry < 0 || fakeCatalog[entry].parID == fsRtParID ) {
				return dirNFErr;
			}
			dir = fakeCatalog[entry].parID;
			s = e + 1;
			continue;
		}
		if ( e - s > 31 ) {
			return bdNamErr;
		}
		entry = FakeFM_Lookup( vol, dir, s, e - s );
		if ( !*e || !e[1] ) {
			/* the last name, perhaps with a directory's trailing colon */
			memset( spec, 0, sizeof( *spec ) );
			spec->vRefNum = vol;
			spec->parID = dir;
			FakeFM_PString( spec->name, s, e - s );
			if ( entry >= 0 && *e && !fakeCatalog[entry].isDir ) {
				return dirNFErr;
			}
			return entry < 0 ? fnfErr : noErr;
		}
		if ( entry < 0 || !fakeCatalog[entry].isDir ) {
			return dirNFErr;
		}
		dir = fakeCatalog[entry].id;
		s = e + 1;
	}
}

/* ioFDirIndex < 0: the directory ioDrDirID; 0: the item named ioNamePtr in
 * ioDirID; > 0: the ioFDirIndex'th item in ioDirID */
static OSErr PBGetCatInfoSync( CInfoPBRec *pb ) {
	short vol = pb->hFileInfo.ioVRefNum ? pb->hFileInfo.ioVRefNum : defaultVRefNum;
	int entry, i, n;

	if ( pb->hFileInfo.ioFDirIndex < 0 ) {
		entry = FakeFM_Dir( vol, pb->dirInfo.ioDrDirID );
	} else if ( pb->hFileInfo.ioFDirIndex == 0 ) {
		if ( !pb->hFileInfo.ioNamePtr || !pb->hFileInfo.ioNamePtr[0] ) {
			return paramErr;
		}
		entry = FakeFM_Lookup( vol, pb->hFileInfo.ioDirID,
			(const char *)pb->hFileInfo.ioNamePtr + 1, pb->hFileInfo.ioNamePtr[0] );
	} else {
		if ( FakeFM_Dir( vol, pb->hFileInfo.ioDirID ) < 0 ) {
			return dirNFErr;
		}
		for ( entry = -1, i = n = 0 ; i < FAKE_ENTRIES ; i++ ) {
			if ( fakeCatalog[i].vRefNum == vol && fakeCatalog[i].parID == pb->hFileInfo.ioDirID &&
				++n == pb->hFileInfo.ioFDirIndex ) {
				entry = i;
				break;
			}
		}
	}
	if ( entry < 0 ) {
		return fnfErr;
	}
	if ( pb->hFileInfo.ioNamePtr && pb->hFileInfo.ioFDirIndex != 0 ) {
		FakeFM_PString( pb->hFileInfo.ioNamePtr, fakeCatalog[entry].name,
			strlen( fakeCatalog[entry].name ) );
	}
	pb->hFileInfo.ioFlAttrib = fakeCatalog[entry].isDir ? ioDirMask : 0;
	pb->hFileInfo.ioDirID = fakeCatalog[entry].id;
	pb->hFileInfo.ioFlParID = fakeCatalog[entry].parID;
	return noErr;
}

static OSErr HGetVol( StringPtr volName, short *vRefNum, long *dirID ) {
	*vRefNum = defaultVRefNum;
	*dirID = defaultDirID;
	return noErr;
}

static OSErr GetVol( StringPtr volName, short *vRefNum ) {
	int entry = FakeFM_Dir( defaultVRefNum, fsRtDirID );

	FakeFM_PString( volName, fakeCatalog[entry].name, strlen( fakeCatalog[entry].name ) );
	*vRefNum = defaultVRefNum;
	return noErr;
}

static OSErr GetCurrentProcess( ProcessSerialNumber *psn ) {
	psn->highLongOfPSN = 0;
	psn->lowLongOfPSN = 2;	/* kCurrentProcess */
	return noErr;
}

static OSErr GetProcessInformation( const ProcessSerialNumber *psn, ProcessInfoRec *info ) {
	*info->processAppSpec = appSpec;
	return noErr;
}

/* ---- the rest of the engine ---- */

static int	failures;

static void Check( int ok, const char *what ) {
	if ( !ok ) {
		fprintf( stderr, "FAIL: %s\n", what );
		failures++;
	}
}

static void CheckString( const char *got, const char *want, const char *what ) {
	if ( strcmp( got, want ) ) {
		fprintf( stderr, "FAIL: %s: got \"%s\", want \"%s\"\n", what, got, want );
		failures++;
	}
}

void QDECL Sys_Error( const char *error, ... ) {
	va_list argptr;

	va_start( argptr, error );
	vfprintf( stderr, error, argptr );
	va_end( argptr );
	fputc( '\n', stderr );
	exit( 1 );
}

void QDECL Com_Error( int level, const char *error, ... ) {
	va_list argptr;

	va_start( argptr, error );
	vfprintf( stderr, error, argptr );
	va_end( argptr );
	fputc( '\n', stderr );
	exit( 1 );
}

void QDECL Com_Printf( const char *fmt, ... ) {
}

void Com_FlightRecord( const char *fmt, ... ) {
}

char *CopyString( const char *in ) {
	char *out = malloc( strlen( in ) + 1 );

	strcpy( out, in );
	return out;
}

#ifdef ZONE_DEBUG
void *Z_MallocDebug( int size, char *label, char *file, int line ) {
	return calloc( 1, size );
}
#else
void *Z_Malloc( int size ) {
	return calloc( 1, size );
}
#endif

static char	fs_gamedir[MAX_OSPATH] = "baseq3";

#include "mac_volume_root_extracted.c"

/* ---- the tests ---- */

/* the File Manager resolves path to an existing item */
static int Resolves( const char *path ) {
	Str255 pname;
	FSSpec spec;

	FakeFM_PString( pname, path, strlen( path ) );
	return FSMakeFSSpec( 0, 0, pname, &spec ) == noErr;
}

/* Sys_ListFiles( directory, ... ) as a sorted, comma-separated string */
static int CompareNames( const void *a, const void *b ) {
	return strcmp( *(char * const *)a, *(char * const *)b );
}

static const char *List( const char *directory, const char *extension, char *filter, qboolean wantsubs ) {
	static char result[1024];
	char **list;
	int count, i;

	result[0] = 0;
	list = Sys_ListFiles( directory, extension, filter, &count, wantsubs );
	if ( !list ) {
		return count ? "(null list with files)" : "";
	}
	qsort( list, count, sizeof( *list ), CompareNames );
	for ( i = 0 ; i < count ; i++ ) {
		if ( i ) {
			Q_strcat( result, sizeof( result ), "," );
		}
		Q_strcat( result, sizeof( result ), list[i] );
		free( list[i] );
	}
	free( list );
	return result;
}

static void CheckBase( const char *base, const char *mods, const char *paks,
                       const char *filtered, const char *parmsVolumeOrFolder ) {
	char parms[MAX_OSPATH * 2 + sizeof( "MacQuake3Parms.txt" )];
	char want[MAX_OSPATH * 2];
	char what[256];

	/* FS_AddGameDirectory and FS_FOpenFileRead */
	Com_sprintf( want, sizeof( want ), "%sbaseq3:pak0.pk3", parmsVolumeOrFolder );
	CheckString( FS_BuildOSPath( base, "baseq3", "pak0.pk3" ), want, "the pk3 path" );
	Check( Resolves( FS_BuildOSPath( base, "baseq3", "pak0.pk3" ) ), "the pk3 path resolves" );
	Com_sprintf( want, sizeof( want ), "%sbaseq3:", parmsVolumeOrFolder );
	CheckString( FS_BuildOSPath( base, "baseq3", "" ), want, "the game directory path" );
	Com_sprintf( want, sizeof( want ), "%sbaseq3:maps:q3dm1.bsp", parmsVolumeOrFolder );
	CheckString( FS_BuildOSPath( base, "", "maps/q3dm1.bsp" ), want, "a qpath under fs_gamedir" );

	/* FS_GetModList and FS_AddGameDirectory's pk3 scan */
	Com_sprintf( what, sizeof( what ), "the directories in \"%s\" (the Mods menu)", base );
	CheckString( List( base, NULL, NULL, qtrue ), mods, what );
	Com_sprintf( what, sizeof( what ), "the directories in \"%s\" (extension \"/\")", base );
	CheckString( List( base, "/", NULL, qfalse ), mods, what );
	CheckString( List( FS_BuildOSPath( base, "baseq3", "" ), ".pk3", NULL, qfalse ), paks,
		"the pk3s in baseq3" );
	Com_sprintf( what, sizeof( what ), "a filtered listing of \"%s\"", base );
	CheckString( List( base, NULL, "*.pk3", qfalse ), filtered, what );

	/* main's MacQuake3Parms.txt */
	Sys_JoinHFSPath( parms, sizeof( parms ), base, "MacQuake3Parms.txt" );
	Com_sprintf( want, sizeof( want ), "%sMacQuake3Parms.txt", parmsVolumeOrFolder );
	CheckString( parms, want, "the parameters file path" );
	Check( Resolves( parms ), "the parameters file path resolves" );
}

int main( int argc, char **argv ) {
	const char *scenario = argc == 2 ? argv[1] : "";
	char *cwd;

	if ( !strcmp( scenario, "root" ) ) {
		appSpec.vRefNum = CD;
		appSpec.parID = fsRtDirID;
		FakeFM_PString( appSpec.name, "Quake3 PPC", 10 );
		defaultVRefNum = CD;
		defaultDirID = fsRtDirID;
	} else if ( !strcmp( scenario, "getvol" ) ) {
		memset( &appSpec, 0, sizeof( appSpec ) );
		defaultVRefNum = CD;
		defaultDirID = fsRtDirID;
	} else if ( !strcmp( scenario, "folder" ) ) {
		appSpec.vRefNum = HD;
		appSpec.parID = 17;
		FakeFM_PString( appSpec.name, "Quake3 PPC", 10 );
		defaultVRefNum = HD;
		defaultDirID = 17;
	} else {
		fprintf( stderr, "usage: %s root|folder|getvol\n", argv[0] );
		return 2;
	}

	cwd = Sys_GetCwd();
	if ( strcmp( scenario, "folder" ) ) {
		/* a volume root, from the catalog walk or GetVol */
		CheckString( cwd, "Quake3:", "Sys_GetCwd at a volume root" );
		CheckBase( cwd, "baseq3,missionpack", "pak0.pk3,pak1.pk3",
			"baseq3/pak0.pk3,baseq3/pak1.pk3,missionpack/pak0.pk3", "Quake3:" );
		Check( !Resolves( "Quake3::baseq3:pak0.pk3" ), "the fake walks up on a doubled colon" );
		Check( !Resolves( "Quake3" ), "the fake reads a bare name in the default directory" );

		/* a hand-set fs_basepath without the colon still lists the volume
		 * (and FS_BuildOSPath gives it the one colon it needs) */
		CheckString( List( "Quake3", NULL, NULL, qtrue ), "baseq3,missionpack",
			"the directories in a hand-set \"Quake3\"" );
		CheckString( FS_BuildOSPath( "Quake3", "baseq3", "pak0.pk3" ), "Quake3:baseq3:pak0.pk3",
			"the pk3 path under a hand-set \"Quake3\"" );
	} else {
		/* a folder keeps its form: no trailing colon */
		CheckString( cwd, "Macintosh HD:Games:Quake3", "Sys_GetCwd in a folder" );
		CheckBase( cwd, "baseq3,mymod", "pak0.pk3",
			"baseq3/pak0.pk3,mymod/zz-mod.pk3", "Macintosh HD:Games:Quake3:" );
		CheckString( List( "Macintosh HD:", NULL, NULL, qtrue ), "Games",
			"the directories at the root of Macintosh HD" );
	}

	/* the current directory, as fs_cdpath's "" lists it */
	CheckString( List( "", ".pk3", NULL, qfalse ), "", "no pk3s in the application folder" );

	if ( failures ) {
		fprintf( stderr, "%s: %d failure(s)\n", scenario, failures );
		return 1;
	}
	printf( "mac volume root (%s): Sys_GetCwd \"%s\" passed\n", scenario, cwd );
	return 0;
}
