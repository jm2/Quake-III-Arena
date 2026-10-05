// mac_syscalls.c
// newlib's open() for the File Manager, replacing the one in Retro68's libretro
//
// libretro's _open_r (Retro68 83b9c8d2c5, libretro/syscalls.c) works out the
// permission the caller asked for and then opens every file with fsRdWrPerm.
// That fails for a Finder-locked file (permErr), a CD or locked volume
// (wPrErr, vLckdErr) and a second open of a file another path already has
// open for writing (opWrErr), so fopen( path, "rb" ) could not read retail
// data from read-only media, or open a pk3 that was already open (issue #258).
//
// The linker takes this definition: the AIX-style XCOFF linker ignores a
// second definition that comes from an archive member (bfd/xcofflink.c,
// "a redefinition in an object contained in an archive"), and this object
// always precedes libretrocrt.a on the link line.  libretro's other syscalls
// (_read_r, _write_r, _close_r, _lseek_r...) still come from libretrocrt.a,
// so file descriptors keep its encoding: refNum + kMacRefNumOffset.
// newlib's stdio implements O_APPEND itself by seeking to the end before
// each write.

#include <reent.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <Files.h>
#include <Errors.h>

extern const int kMacRefNumOffset;	// libretro/syscalls.c

static int Mac_OSErrToErrno( OSErr err ) {
	switch ( err ) {
	case fnfErr:
	case dirNFErr:
	case nsvErr:
		return ENOENT;
	case dupFNErr:
		return EEXIST;
	case wPrErr:
	case vLckdErr:
		return EROFS;
	case permErr:
	case fLckdErr:
	case opWrErr:
	case afpAccessDenied:
		return EACCES;
	case tmfoErr:
		return EMFILE;
	case dskFulErr:
		return ENOSPC;
	case bdNamErr:
		return EINVAL;
	default:
		return EIO;
	}
}

int _open_r( struct _reent *reent, const char *name, int flags, int mode ) {
	Str255	pname;
	size_t	length;
	SInt8	permission;
	short	ref;
	OSErr	err;

	(void)mode;

	length = strlen( name );
	if ( length > 255 ) {
		reent->_errno = ENAMETOOLONG;
		return -1;
	}
	pname[0] = (unsigned char)length;
	memcpy( pname + 1, name, length );

	switch ( flags & O_ACCMODE ) {
	case O_RDONLY:
		permission = fsRdPerm;
		break;
	case O_WRONLY:
		permission = fsWrPerm;
		break;
	case O_RDWR:
		permission = fsRdWrPerm;
		break;
	default:
		reent->_errno = EINVAL;
		return -1;
	}

	if ( flags & O_CREAT ) {
		// the type and creator ('????') libretro gives new files
		err = HCreate( 0, 0, pname, 0x3F3F3F3F, 'TEXT' );
		if ( err != noErr && ( err != dupFNErr || ( flags & O_EXCL ) ) ) {
			reent->_errno = Mac_OSErrToErrno( err );
			return -1;
		}
	}

	err = HOpenDF( 0, 0, pname, permission, &ref );
	if ( err == paramErr ) {
		// file systems without HOpenDF
		err = HOpen( 0, 0, pname, permission, &ref );
	}
	if ( err != noErr ) {
		reent->_errno = Mac_OSErrToErrno( err );
		return -1;
	}

	if ( ( flags & O_TRUNC ) && permission != fsRdPerm ) {
		err = SetEOF( ref, 0 );
		if ( err != noErr ) {
			FSClose( ref );
			reent->_errno = Mac_OSErrToErrno( err );
			return -1;
		}
	}

	return ref + kMacRefNumOffset;
}
