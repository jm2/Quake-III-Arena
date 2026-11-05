/* code/mac/mac_bootlog.c: the log has to be readable after a freeze and a
 * hard reset, when nothing closed it.  The fake File Manager
 * (tests/mac_bootlog_fake.h) loses what HFS would lose, so each case below
 * reads back what a reset leaves. */
#include <stdio.h>
#include <stdlib.h>
#include "../code/mac/mac_bootlog.c"

#define LOG_NAME		"Disk:Game:q3boot.log"
#define PREVIOUS_NAME	"Disk:Game:q3boot.prev.log"

FakeFile	fakeFiles[3];
int			fakeLocked, fakeDiskFullAt, fakeWriteError, fakeFlushError, fakeSyncs, fakeHOpenDFMissing;
static short	openFile = -1;
static int		fakeTime = 1000;
static int		failures;

int Sys_Milliseconds( void ) { return fakeTime; }

static void Check( int condition, const char *what ) {
	if ( !condition ) {
		printf( "FAILED: %s\n", what );
		failures++;
	}
}

static void CString( char *out, ConstStr255Param name ) {
	memcpy( out, name + 1, name[0] );
	out[name[0]] = 0;
}

static FakeFile *Find( const char *name ) {
	int i;
	for ( i = 0 ; i < 3 ; i++ ) {
		if ( fakeFiles[i].exists && !strcmp( fakeFiles[i].name, name ) ) {
			return &fakeFiles[i];
		}
	}
	return NULL;
}

OSErr HCreate( short v, long d, ConstStr255Param name, OSType creator, OSType type ) {
	char	path[256];
	int		i;

	CString( path, name );
	if ( fakeLocked ) {
		return wPrErr;
	}
	if ( Find( path ) ) {
		return dupFNErr;
	}
	for ( i = 0 ; i < 3 ; i++ ) {
		if ( !fakeFiles[i].exists ) {
			fakeFiles[i].exists = 1;
			strcpy( fakeFiles[i].name, path );
			fakeFiles[i].length = fakeFiles[i].catalogLength = 0;
			fakeFiles[i].type = type;
			return noErr;
		}
	}
	return ioErr;
}
OSErr HDelete( short v, long d, ConstStr255Param name ) {
	char		path[256];
	FakeFile	*f;

	CString( path, name );
	f = Find( path );
	if ( !f ) {
		return fnfErr;
	}
	f->exists = 0;
	return noErr;
}
OSErr HRename( short v, long d, ConstStr255Param oldName, ConstStr255Param newName ) {
	char		path[256], leaf[256];
	FakeFile	*f;

	CString( path, oldName );
	CString( leaf, newName );
	f = Find( path );
	if ( !f ) {
		return fnfErr;
	}
	strcpy( strrchr( f->name, ':' ) + 1, leaf );
	return noErr;
}
OSErr HOpen( short v, long d, ConstStr255Param name, SInt8 permission, short *ref ) {
	char		path[256];
	FakeFile	*f;

	CString( path, name );
	f = Find( path );
	if ( !f ) {
		return fnfErr;
	}
	*ref = 100 + (short)( f - fakeFiles );
	openFile = *ref;
	return noErr;
}
OSErr HOpenDF( short v, long d, ConstStr255Param name, SInt8 permission, short *ref ) {
	return fakeHOpenDFMissing ? paramErr : HOpen( v, d, name, permission, ref );
}
static FakeFile *Open( short ref ) {
	Check( ref == openFile, "call uses the open file" );
	return &fakeFiles[ref - 100];
}
OSErr SetEOF( short ref, long length ) {
	FakeFile *f = Open( ref );
	f->length = length;
	return noErr;
}
static OSErr Store( FakeFile *f, long position, const void *buffer, long count ) {
	if ( fakeDiskFullAt && position + count > fakeDiskFullAt ) {
		return dskFulErr;
	}
	memcpy( f->cache + position, buffer, count );
	if ( position + count > f->length ) {
		f->length = position + count;
	}
	return noErr;
}
OSErr FSWrite( short ref, long *count, const void *buffer ) {
	FakeFile	*f = Open( ref );
	OSErr		err = Store( f, f->length, buffer, *count );
	return err;
}
OSErr GetVRefNum( short ref, short *volume ) { *volume = -1; return noErr; }
static void Commit( FakeFile *f, long length ) {
	memcpy( f->disk, f->cache, length );
	f->catalogLength = length;
}
OSErr FlushVol( const unsigned char *name, short volume ) {
	int i;
	for ( i = 0 ; i < 3 ; i++ ) {
		if ( fakeFiles[i].exists ) {
			Commit( &fakeFiles[i], fakeFiles[i].length );
		}
	}
	return noErr;
}
OSErr FSClose( short ref ) {
	FakeFile *f = Open( ref );
	Commit( f, f->length );
	openFile = -1;
	return noErr;
}
OSErr PBWriteSync( ParamBlockRec *pb ) {
	Check( pb->ioParam.ioPosMode & fsFromStart, "writes are positioned" );
	if ( fakeWriteError ) {
		return ioErr;
	}
	return Store( Open( pb->ioParam.ioRefNum ), pb->ioParam.ioPosOffset, pb->ioParam.ioBuffer, pb->ioParam.ioReqCount );
}
OSErr PBFlushFileSync( ParamBlockRec *pb ) {
	FakeFile *f = Open( pb->ioParam.ioRefNum );

	if ( fakeFlushError ) {
		return ioErr;
	}
	fakeSyncs++;
	memcpy( f->disk, f->cache, f->catalogLength );	// the length stays as the catalog has it
	return noErr;
}

void FakeFM_Reset( void ) {
	memset( fakeFiles, 0, sizeof( fakeFiles ) );
	fakeLocked = fakeDiskFullAt = fakeWriteError = fakeFlushError = fakeSyncs = fakeHOpenDFMissing = 0;
}
void FakeFM_HardReset( void ) {
	int i;
	for ( i = 0 ; i < 3 ; i++ ) {
		fakeFiles[i].length = fakeFiles[i].catalogLength;
		memcpy( fakeFiles[i].cache, fakeFiles[i].disk, fakeFiles[i].length );
	}
	/* the machine restarted, so the log's globals are gone */
	logRefNum = 0;
	openFile = -1;
	pendingLength = 0;
	writing = 0;
}

/* The file as a C string, for strstr: blanks stop no one, zero bytes cannot occur in it. */
static const char *Text( const char *name ) {
	static char	text[FAKE_FILE_CAPACITY + 1];
	FakeFile	*f = Find( name );

	if ( !f ) {
		return "";
	}
	memcpy( text, f->disk, f->catalogLength );
	text[f->catalogLength] = 0;
	return text;
}

static void Start( void ) {
	FakeFM_Reset();
	Check( Sys_BootLogOpen( "Disk:Game" ), "opens" );
}

static void SurvivesReset( void ) {
	const char *text;

	Start();
	Check( Find( LOG_NAME )->catalogLength == BOOTLOG_SIZE, "the full length is in the catalog from the start" );
	Check( Find( LOG_NAME )->type == 'TEXT', "a text file" );
	Sys_BootLogWrite( "first line\n" );
	fakeTime = 12345;
	Sys_BootLogWrite( "partial " );
	Sys_BootLogWrite( "second line\n" );
	FakeFM_HardReset();
	text = Text( LOG_NAME );
	Check( strstr( text, "[     1.000] first line\n" ) != NULL, "the first line survives a reset, with its time" );
	Check( strstr( text, "[    12.345] partial second line\n" ) != NULL, "a line in two writes survives, stamped once" );
	Check( strstr( text, "second line\n<<< end of log >>>\n" ) != NULL
		|| strstr( text, "second line\n\n<<< end of log >>>\n" ) != NULL, "the end is marked after the newest text" );
	fakeTime = 1000;
}

static void KeepsPrevious( void ) {
	Start();
	Sys_BootLogWrite( "the run that froze\n" );
	FakeFM_HardReset();
	Check( Sys_BootLogOpen( "Disk:Game" ), "reopens after a reset" );
	Sys_BootLogWrite( "the next run\n" );
	FakeFM_HardReset();
	Check( strstr( Text( PREVIOUS_NAME ), "the run that froze\n" ) != NULL, "a restart keeps the log of the freeze" );
	Check( !strstr( Text( LOG_NAME ), "the run that froze\n" ), "the new log starts empty" );
	Check( strstr( Text( LOG_NAME ), "the next run\n" ) != NULL, "the new log has the new run" );
	/* a third run replaces the older log, and keeps the one that matters */
	Check( Sys_BootLogOpen( "Disk:Game" ), "opens a third time" );
	FakeFM_HardReset();
	Check( strstr( Text( PREVIOUS_NAME ), "the next run\n" ) != NULL, "only one previous log is kept" );
}

static void VolumeRoot( void ) {
	FakeFM_Reset();
	Check( Sys_BootLogOpen( "Disk:" ), "opens at a volume root" );
	Check( Find( "Disk:q3boot.log" ) != NULL, "a root directory ends in the colon it already has" );
	Sys_BootLogClose();
}

static void LongPath( void ) {
	char directory[300];

	FakeFM_Reset();
	memset( directory, 'x', sizeof( directory ) - 1 );
	directory[sizeof( directory ) - 1] = 0;
	Check( !Sys_BootLogOpen( directory ), "a path over 255 bytes means no log" );
	Sys_BootLogWrite( "ignored\n" );
	Check( !fakeSyncs, "no log, no writes" );
}

static void Wraps( void ) {
	char		line[1024];
	const char	*text, *end;
	int			i;

	Start();
	Sys_BootLogWrite( "startup\n" );
	memset( line, 'y', sizeof( line ) - 2 );
	line[sizeof( line ) - 2] = '\n';
	line[sizeof( line ) - 1] = 0;
	/* past the end, so the log has wrapped and is partway through again */
	for ( i = 0 ; i < 2500 ; i++ ) {
		fakeTime += 1000;
		Sys_BootLogWrite( line );
	}
	Check( logWrapped, "the log wrapped" );
	Sys_BootLogWrite( "the last words\n" );
	FakeFM_HardReset();
	text = Text( LOG_NAME );
	Check( Find( LOG_NAME )->catalogLength == BOOTLOG_SIZE, "wrapping never changes the length" );
	Check( strstr( text, "startup\n" ) != NULL, "the startup stays" );
	Check( strstr( text, "the last words\n" ) != NULL, "the newest text is there" );
	end = strstr( text, "<<< end of log >>>" );
	Check( end && !strstr( end + 1, "<<< end of log >>>" ), "one end marker, the newest" );
	Check( end > text + BOOTLOG_KEEP, "the wrap kept the startup region" );
	fakeTime = 1000;
}

static void RateLimit( void ) {
	int i;

	Start();
	for ( i = 0 ; i < 1000 ; i++ ) {
		Sys_BootLogWrite( "a flood\n" );
	}
	Check( fakeSyncs <= BOOTLOG_MAX_SYNCS_PER_SECOND + 2, "a flood does not cost a disk write per line" );
	fakeTime += 1000;
	Sys_BootLogWrite( "after the flood\n" );
	Sys_BootLogFlush();
	FakeFM_HardReset();
	Check( strstr( Text( LOG_NAME ), "after the flood\n" ) != NULL, "held lines go out with the next one" );
	fakeTime = 1000;
}

static void Failures( void ) {
	FakeFM_Reset();
	fakeLocked = 1;
	Check( !Sys_BootLogOpen( "Disk:Game" ), "a locked volume means no log" );
	Sys_BootLogWrite( "ignored\n" );

	FakeFM_Reset();
	fakeDiskFullAt = 100000;
	Check( !Sys_BootLogOpen( "Disk:Game" ), "no room for the full length means no log" );
	Check( !Find( LOG_NAME ), "and no half-made file" );

	Start();
	fakeWriteError = 1;
	Sys_BootLogWrite( "fails\n" );
	Sys_BootLogWrite( "stopped\n" );
	Check( !logRefNum, "a write error stops the log" );
	Sys_BootLogClose();		/* nothing left to close */

	Start();
	fakeFlushError = 1;
	Sys_BootLogWrite( "fails\n" );
	Check( !logRefNum, "a flush error stops the log" );

	FakeFM_Reset();
	fakeHOpenDFMissing = 1;
	Check( Sys_BootLogOpen( "Disk:Game" ), "HOpen serves file systems without HOpenDF" );
	Sys_BootLogClose();
}

static void CleanClose( void ) {
	const char	*text;
	long		length;

	Start();
	Sys_BootLogWrite( "a whole run\n" );
	Sys_BootLogWrite( "without a newline" );
	Sys_BootLogClose();
	Check( !logRefNum, "closed" );
	Sys_BootLogClose();
	Sys_BootLogWrite( "after the close\n" );
	length = Find( LOG_NAME )->catalogLength;
	Check( length < 200, "a quit trims the blanks" );
	FakeFM_HardReset();
	text = Text( LOG_NAME );
	Check( strstr( text, "without a newline" ) != NULL, "a line without a newline is not lost" );
	Check( !strstr( text, "after the close" ), "nothing after the close" );
	Check( !strstr( text, "<<< end of log >>>" ), "a quit leaves no end marker" );
}

int main( void ) {
	SurvivesReset();
	KeepsPrevious();
	VolumeRoot();
	LongPath();
	Wraps();
	RateLimit();
	Failures();
	CleanClose();
	if ( failures ) {
		return 1;
	}
	puts( "Mac boot log survives a hard reset, keeps the last run, wraps and fails soft" );
	return 0;
}
