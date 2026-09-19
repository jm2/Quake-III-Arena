/* Download block-length handling through the production cl_parse.c body. */
#ifndef Q3_CLIENT_PARSE_SOURCE
#define Q3_CLIENT_PARSE_SOURCE "../code/client/cl_parse.c"
#endif
#include Q3_CLIENT_PARSE_SOURCE

#include <setjmp.h>

clientActive_t cl;
clientConnection_t clc;
clientStatic_t cls;
cvar_t *cl_shownet;

static jmp_buf errorJump;
static int expectError;
static int errors;
static int shortValues[2];
static int shortReads;
static int longValue;
static int longReads;
static int dataReads;
static int dataBytes;
static int opens;
static int writes;
static int closes;
static int renames;
static int packets;
static int nextDownloads;
static char reliable[MAX_STRING_CHARS];

static void Check( int condition, const char *message ) {
	if ( !condition ) {
		fprintf( stderr, "Download parse regression failed: %s\n", message );
		exit( 1 );
	}
}

void QDECL Com_Error( int level, const char *format, ... ) {
	(void)format;
	Check( expectError && level == ERR_DROP, "unexpected engine error" );
	errors++;
	longjmp( errorJump, 1 );
}

void QDECL Com_Printf( const char *format, ... ) {
	(void)format;
}

void QDECL Com_DPrintf( const char *format, ... ) {
	(void)format;
}

void CL_AddReliableCommand( const char *command ) {
	Q_strncpyz( reliable, command, sizeof(reliable) );
}

void Cvar_Set( const char *name, const char *value ) {
	(void)name;
	(void)value;
}

void Cvar_SetValue( const char *name, float value ) {
	(void)name;
	(void)value;
}

int MSG_ReadShort( msg_t *msg ) {
	(void)msg;
	Check( shortReads < 2, "bounded scripted short read" );
	return shortValues[shortReads++];
}

int MSG_ReadLong( msg_t *msg ) {
	(void)msg;
	longReads++;
	return longValue;
}

char *MSG_ReadString( msg_t *msg ) {
	(void)msg;
	return "server download error";
}

void MSG_ReadData( msg_t *msg, void *buffer, int size ) {
	(void)msg;
	Check( size >= 0 && size <= MAX_MSGLEN, "bounded payload read" );
	memset( buffer, 0x5a, size );
	dataReads++;
	dataBytes = size;
}

fileHandle_t FS_SV_FOpenFileWrite( const char *filename ) {
	Check( !strcmp( filename, clc.downloadTempName ), "complete temporary name" );
	opens++;
	return 7;
}

int FS_Write( const void *buffer, int length, fileHandle_t file ) {
	const unsigned char *bytes = buffer;
	Check( file == 7 && length >= 0 && length <= MAX_MSGLEN,
		"bounded file write" );
	if ( length ) {
		Check( bytes[0] == 0x5a && bytes[length - 1] == 0x5a,
			"complete parsed payload" );
	}
	writes++;
	return length;
}

void FS_FCloseFile( fileHandle_t file ) {
	Check( file == 7, "known download handle close" );
	closes++;
}

void FS_SV_Rename( const char *from, const char *to ) {
	Check( !strcmp( from, "mod/pak.pk3.tmp" )
		&& !strcmp( to, "mod/pak.pk3" ), "complete rename pair" );
	renames++;
}

void CL_WritePacket( void ) {
	packets++;
}

void CL_NextDownload( void ) {
	nextDownloads++;
}

static void Reset( int block, int size ) {
	memset( &clc, 0, sizeof(clc) );
	memset( &cls, 0, sizeof(cls) );
	shortValues[0] = block;
	shortValues[1] = size;
	shortReads = longReads = dataReads = dataBytes = 0;
	opens = writes = closes = renames = packets = nextDownloads = 0;
	reliable[0] = 0;
	errors = expectError = 0;
}

static void RejectSize( int size, const char *message ) {
	msg_t msg;

	Reset( 1, size );
	Q_strncpyz( clc.downloadTempName, "mod/pak.pk3.tmp",
		sizeof(clc.downloadTempName) );
	expectError = 1;
	if ( setjmp( errorJump ) == 0 ) {
		CL_ParseDownload( &msg );
		Check( qfalse, message );
	}
	expectError = 0;
	Check( errors == 1 && shortReads == 2 && !dataReads && !opens
		&& !writes && !reliable[0],
		"invalid chunk rejects before payload and filesystem access" );
}

int main( void ) {
	msg_t msg;

	Reset( 0, 0 );
	CL_ParseDownload( &msg );
	Check( !shortReads && !strcmp( reliable, "stopdl" ) && !opens,
		"unsolicited download rejected before message access" );

	RejectSize( -1, "negative chunk accepted" );
	RejectSize( MAX_MSGLEN + 1, "oversized chunk accepted" );

	Reset( 0, 0 );
	Q_strncpyz( clc.downloadTempName, "mod/pak.pk3.tmp",
		sizeof(clc.downloadTempName) );
	longValue = -1;
	expectError = 1;
	if ( setjmp( errorJump ) == 0 ) {
		CL_ParseDownload( &msg );
		Check( qfalse, "negative advertised size accepted" );
	}
	expectError = 0;
	Check( errors == 1 && shortReads == 1 && longReads == 1
		&& !dataReads && !opens, "negative file size rejects before chunk read" );

	Reset( 0, 4 );
	Q_strncpyz( clc.downloadTempName, "mod/pak.pk3.tmp",
		sizeof(clc.downloadTempName) );
	Q_strncpyz( clc.downloadName, "mod/pak.pk3", sizeof(clc.downloadName) );
	longValue = 4;
	CL_ParseDownload( &msg );
	Check( longReads == 1 && dataReads == 1 && dataBytes == 4
		&& opens == 1 && writes == 1 && clc.download == 7
		&& clc.downloadBlock == 1 && clc.downloadCount == 4
		&& !strcmp( reliable, "nextdl 0" ), "valid first block" );

	Reset( 2, 4 );
	Q_strncpyz( clc.downloadTempName, "mod/pak.pk3.tmp",
		sizeof(clc.downloadTempName) );
	CL_ParseDownload( &msg );
	Check( dataReads == 1 && dataBytes == 4 && !opens && !writes
		&& !reliable[0], "out-of-order block ignored after bounded read" );

	Reset( 1, MAX_MSGLEN );
	Q_strncpyz( clc.downloadTempName, "mod/pak.pk3.tmp",
		sizeof(clc.downloadTempName) );
	clc.download = 7;
	clc.downloadBlock = 1;
	CL_ParseDownload( &msg );
	Check( dataBytes == MAX_MSGLEN && writes == 1
		&& clc.downloadBlock == 2 && clc.downloadCount == MAX_MSGLEN,
		"exact-capacity chunk" );

	Reset( 1, 0 );
	Q_strncpyz( clc.downloadTempName, "mod/pak.pk3.tmp",
		sizeof(clc.downloadTempName) );
	Q_strncpyz( clc.downloadName, "mod/pak.pk3", sizeof(clc.downloadName) );
	clc.download = 7;
	clc.downloadBlock = 1;
	CL_ParseDownload( &msg );
	Check( closes == 1 && renames == 1 && packets == 2 && nextDownloads == 1
		&& !clc.download && !clc.downloadName[0]
		&& !clc.downloadTempName[0] && !strcmp( reliable, "nextdl 1" ),
		"zero-length EOF closes and advances" );

	puts( "Download chunk lengths, ordering, payload capacity and EOF pass" );
	return 0;
}
