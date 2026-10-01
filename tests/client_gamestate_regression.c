/* Issue #40: the gamestate's clientNum becomes CG_INIT's clientNum, which the
 * native cgame uses to index cgs.clientinfo with no VM sandbox around it.
 * Drives the real CL_ParseGamestate (cl_parse.c) over messages written with
 * the real msg.c/huffman.c encoders, then fuzzes everything after a valid
 * header. The rest of the client is discarded at link time.
 * Issue #435: a systeminfo that fills MSG_ReadBigString's buffer is read to
 * its terminator, so the configstring after it still parses. */
#include "../code/client/cl_parse.c"
#include <limits.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

clientActive_t cl;
clientConnection_t clc;
clientStatic_t cls;
cvar_t *cl_shownet;
int cl_connectedToPureServer;

static cvar_t shownet;
static jmp_buf dropJump;
static int expectDrop, drops, dropLevel;
static char dropText[1024];
static int downloads, downloadClientNum, restarts;
static unsigned int seed;

/** Fail with the gamestate contract that broke. */
static void Check( int ok, const char *message ) {
	if ( !ok ) { fprintf( stderr, "Client gamestate regression failed: %s\n", message ); exit( 1 ); }
}

/** ERR_DROP longjmps back to the caller, as Com_Error does to Com_Frame. */
void QDECL Com_Error( int level, const char *format, ... ) {
	va_list args;
	va_start( args, format );
	vsnprintf( dropText, sizeof( dropText ), format, args );
	va_end( args );
	Check( expectDrop, dropText );
	dropLevel = level;
	drops++;
	longjmp( dropJump, 1 );
}
void QDECL Com_Printf( const char *format, ... ) { (void)format; }
void QDECL Com_DPrintf( const char *format, ... ) { (void)format; }
void Com_Memcpy( void *out, const void *in, const size_t size ) { memcpy( out, in, size ); }
void Com_Memset( void *out, const int value, const size_t size ) { memset( out, value, size ); }
void Con_Close( void ) {}
void CL_ClearState( void ) { memset( &cl, 0, sizeof( cl ) ); }
/** The cgame starts from here: CL_InitCGame passes clc.clientNum to CG_INIT. */
void CL_InitDownloads( void ) { downloads++; downloadClientNum = clc.clientNum; }
qboolean FS_ConditionalRestart( int checksumFeed ) { (void)checksumFeed; restarts++; return qfalse; }
void FS_PureServerSetLoadedPaks( const char *sums, const char *names ) { (void)sums; (void)names; }
void FS_PureServerSetReferencedPaks( const char *sums, const char *names ) { (void)sums; (void)names; }
void Cvar_SetCheatState( void ) {}
void Cvar_Set( const char *name, const char *value ) { (void)name; (void)value; }
float Cvar_VariableValue( const char *name ) { (void)name; return 0; }
char *Cvar_VariableString( const char *name ) { (void)name; return ""; }
/* PR #407's CL_SystemInfoChanged also asks for these; every key is a plain one. */
int Cvar_Flags( const char *name ) { (void)name; return 0; }
void Cvar_SetSafe( const char *name, const char *value ) { (void)name; (void)value; }
cvar_t *Cvar_Get( const char *name, const char *value, int flags ) {
	(void)name; (void)value; (void)flags;
	return &shownet;
}

/** Deterministic LCG, so every failure reproduces. */
static int Random( int range ) {
	seed = seed * 1103515245u + 12345u;
	return (int)( ( seed >> 8 ) % (unsigned int)range );
}

/** Begin a gamestate as SV_SendClientGameState writes it, after svc_gamestate. */
static void WriteHeader( msg_t *msg, byte *buffer, int size ) {
	entityState_t nullstate, base;

	MSG_Init( msg, buffer, size );
	MSG_WriteLong( msg, 5 );	// reliable command sequence
	MSG_WriteByte( msg, svc_configstring );
	MSG_WriteShort( msg, CS_SERVERINFO );
	MSG_WriteBigString( msg, "\\sv_maxclients\\8\\mapname\\q3dm1\\g_gametype\\0" );
	MSG_WriteByte( msg, svc_configstring );
	MSG_WriteShort( msg, CS_SYSTEMINFO );
	MSG_WriteBigString( msg, "\\sv_serverid\\7\\sv_pure\\0" );
	memset( &nullstate, 0, sizeof( nullstate ) );
	memset( &base, 0, sizeof( base ) );
	base.number = 42;
	base.eType = ET_GENERAL;
	base.modelindex = 3;
	MSG_WriteByte( msg, svc_baseline );
	MSG_WriteDeltaEntity( msg, &nullstate, &base, qtrue );
}

/** Finish the gamestate with the client number the server assigned. */
static void WriteTrailer( msg_t *msg, int clientNum ) {
	MSG_WriteByte( msg, svc_EOF );
	MSG_WriteLong( msg, clientNum );
	MSG_WriteLong( msg, 0x1234 );	// checksum feed
}

/**
 * Parse one gamestate from a heap copy sized to the message, so a read past
 * its end is an ASan error. Returns 1 if it was dropped with ERR_DROP.
 */
static int Parse( const msg_t *written, int dropOk ) {
	msg_t msg;
	byte *data;
	int dropped;

	Check( !written->overflowed && written->cursize > 0, "test message fits" );
	data = malloc( written->cursize );
	Check( data != NULL, "allocation" );
	memcpy( data, written->data, written->cursize );
	memset( &msg, 0, sizeof( msg ) );
	msg.data = data;
	msg.maxsize = msg.cursize = written->cursize;
	MSG_BeginReading( &msg );

	memset( &clc, 0, sizeof( clc ) );
	downloads = restarts = 0;
	downloadClientNum = -12345;
	dropText[0] = 0;
	expectDrop = dropOk;
	dropped = setjmp( dropJump );
	if ( !dropped ) {
		CL_ParseGamestate( &msg );
	}
	expectDrop = 0;
	free( data );
	if ( dropped ) {
		Check( dropLevel == ERR_DROP, "gamestate errors drop the connection" );
		Check( !downloads && !restarts, "a dropped gamestate never starts the cgame" );
	}
	return dropped;
}

/** Every parsed configstring stays inside the gamestate string buffer. */
static void CheckGameState( void ) {
	int i;

	Check( cl.gameState.dataCount >= 1 && cl.gameState.dataCount <= MAX_GAMESTATE_CHARS, "gamestate size" );
	for ( i = 0; i < MAX_CONFIGSTRINGS; i++ ) {
		Check( cl.gameState.stringOffsets[i] >= 0 && cl.gameState.stringOffsets[i] < cl.gameState.dataCount,
			"configstring offset" );
	}
}

/** clientNum: every value that cannot index cgs.clientinfo is a controlled drop. */
static void TestClientNums( void ) {
	static const int accepted[] = { 0, 1, MAX_CLIENTS / 2, MAX_CLIENTS - 1 };
	static const int rejected[] = { -1, MAX_CLIENTS, INT_MIN, INT_MAX, 255, 256, -MAX_CLIENTS, 1 << 16 };
	static byte buffer[MAX_MSGLEN];
	msg_t msg;
	int i;

	for ( i = 0; i < (int)( sizeof( accepted ) / sizeof( accepted[0] ) ); i++ ) {
		WriteHeader( &msg, buffer, sizeof( buffer ) );
		WriteTrailer( &msg, accepted[i] );
		Check( !Parse( &msg, 0 ), "valid clientNum accepted" );
		Check( clc.clientNum == accepted[i], "valid clientNum kept" );
		Check( downloads == 1 && downloadClientNum == accepted[i], "valid clientNum reaches the cgame start" );
		Check( restarts == 1 && clc.checksumFeed == 0x1234 && clc.serverCommandSequence == 5, "rest of the gamestate read" );
		Check( cl.serverId == 7, "systeminfo parsed" );
		Check( !strcmp( cl.gameState.stringData + cl.gameState.stringOffsets[CS_SERVERINFO],
			"\\sv_maxclients\\8\\mapname\\q3dm1\\g_gametype\\0" ), "serverinfo configstring" );
		Check( cl.entityBaselines[42].number == 42 && cl.entityBaselines[42].modelindex == 3, "baseline" );
		CheckGameState();
	}

	for ( i = 0; i < (int)( sizeof( rejected ) / sizeof( rejected[0] ) ); i++ ) {
		WriteHeader( &msg, buffer, sizeof( buffer ) );
		WriteTrailer( &msg, rejected[i] );
		Check( Parse( &msg, 1 ), "out-of-range clientNum dropped" );
		Check( strstr( dropText, "clientNum" ) != NULL, "dropped for its clientNum" );
		Check( cl.serverId == 0, "systeminfo not applied from a rejected gamestate" );
	}
}

/**
 * Malformed gamestates: random bytes (decoded or raw Huffman) after a valid
 * header, and random well-formed records with any clientNum. Each one either
 * drops or starts the cgame with a clientNum inside [0, MAX_CLIENTS).
 */
static void TestFuzz( void ) {
	static byte buffer[MAX_MSGLEN];
	msg_t msg;
	int iteration, i, count, clientNum, index, wellFormed, dropped;
	int accepted = 0, rejected = 0, clientNumDrops = 0;
	char text[64];
	entityState_t nullstate, base;

	seed = 40;
	memset( &nullstate, 0, sizeof( nullstate ) );
	for ( iteration = 0; iteration < 4000; iteration++ ) {
		WriteHeader( &msg, buffer, sizeof( buffer ) );
		clientNum = 0;
		wellFormed = 0;
		switch ( iteration % 3 ) {
		case 0:	// random bytes through the Huffman encoder
			count = Random( 512 );
			for ( i = 0; i < count; i++ ) {
				MSG_WriteByte( &msg, Random( 256 ) );
			}
			break;
		case 1:	// random bytes on the wire after the header
			count = Random( 512 );
			for ( i = 0; i < count && msg.cursize < msg.maxsize; i++ ) {
				msg.data[msg.cursize++] = (byte)Random( 256 );
			}
			break;
		default:	// random records, mostly well-formed, then any clientNum
			wellFormed = 1;
			count = Random( 24 );
			for ( i = 0; i < count; i++ ) {
				if ( Random( 2 ) ) {
					MSG_WriteByte( &msg, svc_configstring );
					index = Random( 32 ) ? Random( MAX_CONFIGSTRINGS ) : Random( 65536 ) - 32768;
					if ( index < 0 || index >= MAX_CONFIGSTRINGS ) {
						wellFormed = 0;
					}
					MSG_WriteShort( &msg, index );
					Com_sprintf( text, sizeof( text ), "\\k%i\\%i", Random( 1000 ), Random( 100000 ) );
					MSG_WriteBigString( &msg, text );
				} else {
					memset( &base, 0, sizeof( base ) );
					base.number = Random( MAX_GENTITIES );
					base.eType = Random( 256 );
					base.modelindex = Random( 256 );
					base.clientNum = Random( 256 );
					base.weapon = Random( 256 );
					MSG_WriteByte( &msg, svc_baseline );
					MSG_WriteDeltaEntity( &msg, &nullstate, &base, qtrue );
				}
			}
			clientNum = Random( 2 ) ? Random( MAX_CLIENTS ) : (int)( ( (unsigned int)Random( 65536 ) << 16 ) | (unsigned int)Random( 65536 ) );
			WriteTrailer( &msg, clientNum );
			count = Random( 8 );
			for ( i = 0; i < count; i++ ) {
				MSG_WriteByte( &msg, Random( 256 ) );
			}
			break;
		}

		dropped = Parse( &msg, 1 );
		if ( dropped ) {
			rejected++;
			if ( strstr( dropText, "clientNum" ) ) {
				clientNumDrops++;
				Check( clc.clientNum < 0 || clc.clientNum >= MAX_CLIENTS, "valid clientNum dropped" );
			}
		} else {
			accepted++;
			Check( downloads == 1, "accepted gamestate starts the cgame" );
			Check( downloadClientNum >= 0 && downloadClientNum < MAX_CLIENTS, "fuzzed clientNum reaches the cgame" );
			CheckGameState();
		}
		if ( wellFormed ) {
			// the outcome depends only on the clientNum
			if ( clientNum >= 0 && clientNum < MAX_CLIENTS ) {
				Check( !dropped && downloadClientNum == clientNum, "well-formed gamestate accepted" );
			} else {
				Check( dropped && strstr( dropText, "clientNum" ), "well-formed gamestate with a bad clientNum dropped" );
			}
		}
	}
	Check( accepted > 400 && rejected > 2000 && clientNumDrops > 400, "fuzz reached both outcomes" );
	printf( "Gamestate fuzz: %d accepted, %d dropped (%d for clientNum)\n", accepted, rejected, clientNumDrops );
}

/**
 * Issue #435: a systeminfo of BIG_INFO_STRING - 1 chars, which MSG_WriteBigString
 * sends and an ioquake3, Quake3e or retail server can build, is read up to and
 * past its terminator: the configstring after it and the rest of the gamestate
 * parse. A longer one, which no server writes, keeps what fits and still ends
 * at its terminator.
 */
static void TestBigSystemInfo( void ) {
	static const int lengths[] = { BIG_INFO_STRING - 2, BIG_INFO_STRING - 1, BIG_INFO_STRING, BIG_INFO_STRING + 1, BIG_INFO_STRING + 40 };
	static byte buffer[MAX_MSGLEN];
	static char systemInfo[BIG_INFO_STRING + 64];
	entityState_t nullstate, base;
	msg_t msg;
	int i, length, prefix;

	memset( &nullstate, 0, sizeof( nullstate ) );
	for ( i = 0; i < (int)( sizeof( lengths ) / sizeof( lengths[0] ) ); i++ ) {
		/* the short keys, then a pk3 name list that fills the string */
		length = lengths[i];
		Q_strncpyz( systemInfo, "\\sv_serverid\\435\\sv_pure\\1\\sv_paks\\1234 \\sv_pakNames\\", sizeof( systemInfo ) );
		prefix = strlen( systemInfo );
		memset( systemInfo + prefix, 'p', length - prefix );
		systemInfo[length] = 0;

		MSG_Init( &msg, buffer, sizeof( buffer ) );
		MSG_WriteLong( &msg, 5 );	// reliable command sequence
		MSG_WriteByte( &msg, svc_configstring );
		MSG_WriteShort( &msg, CS_SYSTEMINFO );
		if ( length < BIG_INFO_STRING ) {
			MSG_WriteBigString( &msg, systemInfo );
		} else {
			MSG_WriteData( &msg, systemInfo, length + 1 );	// MSG_WriteBigString sends these empty
		}
		MSG_WriteByte( &msg, svc_configstring );
		MSG_WriteShort( &msg, CS_SERVERINFO );
		MSG_WriteBigString( &msg, "\\sv_maxclients\\8\\mapname\\q3dm17" );
		memset( &base, 0, sizeof( base ) );
		base.number = 435;
		base.eType = ET_GENERAL;
		base.modelindex = 7;
		MSG_WriteByte( &msg, svc_baseline );
		MSG_WriteDeltaEntity( &msg, &nullstate, &base, qtrue );
		WriteTrailer( &msg, 3 );

		Check( !Parse( &msg, 0 ), "gamestate with a systeminfo that fills the buffer accepted" );
		systemInfo[BIG_INFO_STRING - 1] = 0;	// all that fits
		Check( !strcmp( cl.gameState.stringData + cl.gameState.stringOffsets[CS_SYSTEMINFO], systemInfo ),
			"systeminfo kept up to BIG_INFO_STRING - 1 chars" );
		Check( !strcmp( cl.gameState.stringData + cl.gameState.stringOffsets[CS_SERVERINFO],
			"\\sv_maxclients\\8\\mapname\\q3dm17" ), "configstring after the systeminfo" );
		Check( cl.entityBaselines[435].number == 435 && cl.entityBaselines[435].modelindex == 7, "baseline after the systeminfo" );
		Check( clc.clientNum == 3 && downloads == 1 && clc.checksumFeed == 0x1234, "rest of the gamestate read" );
		Check( cl.serverId == 435, "systeminfo parsed" );
		CheckGameState();
	}
	puts( "Systeminfo of BIG_INFO_STRING - 2 to BIG_INFO_STRING + 40 chars parsed (issue #435)" );
}

int main( void ) {
	memset( &shownet, 0, sizeof( shownet ) );
	cl_shownet = &shownet;
	TestClientNums();
	TestBigSystemInfo();
	TestFuzz();
	puts( "Client gamestate regressions passed (issue #40)" );
	return 0;
}
