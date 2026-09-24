/* Issue #345: every gamestate server_gamestate_budget_regression.c sent is
 * parsed by the real CL_ParseGamestate (cl_parse.c), as a client gets it
 * from the netchan: it must fit MAX_GAMESTATE_CHARS, carry the server's
 * systeminfo whole and end with the netchan's svc_EOF, and the client must
 * see the pure lists the server kept (none, in degraded pure mode). */
#include "../code/client/cl_parse.c"
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
static char dropText[1024], pureSums[BIG_INFO_STRING], pureNames[BIG_INFO_STRING];
static int gamestate, downloads;

/** Fail with the gamestate that broke the contract. */
static void Check( int ok, const char *message ) {
	if ( !ok ) { fprintf( stderr, "Client gamestate budget regression failed (gamestate %d): %s\n", gamestate, message ); exit( 1 ); }
}
/** ERR_DROP is the client refusing the gamestate. */
void QDECL Com_Error( int level, const char *format, ... ) {
	va_list args;
	(void)level;
	va_start( args, format ); vsnprintf( dropText, sizeof( dropText ), format, args ); va_end( args );
	longjmp( dropJump, 1 );
}
void QDECL Com_Printf( const char *format, ... ) { (void)format; }
void QDECL Com_DPrintf( const char *format, ... ) { (void)format; }
void Com_Memcpy( void *out, const void *in, const size_t size ) { memcpy( out, in, size ); }
void Com_Memset( void *out, const int value, const size_t size ) { memset( out, value, size ); }
void Con_Close( void ) {}
void CL_ClearState( void ) { memset( &cl, 0, sizeof( cl ) ); }
void CL_InitDownloads( void ) { downloads++; }
qboolean FS_ConditionalRestart( int checksumFeed ) { (void)checksumFeed; return qfalse; }
/** The lists a client restricts its pk3s to. */
void FS_PureServerSetLoadedPaks( const char *sums, const char *names ) {
	Q_strncpyz( pureSums, sums, sizeof( pureSums ) );
	Q_strncpyz( pureNames, names, sizeof( pureNames ) );
}
void FS_PureServerSetReferencedPaks( const char *sums, const char *names ) { (void)sums; (void)names; }
void Cvar_SetCheatState( void ) {}
void Cvar_Set( const char *name, const char *value ) { (void)name; (void)value; }
float Cvar_VariableValue( const char *name ) { (void)name; return 0; }
char *Cvar_VariableString( const char *name ) { (void)name; return ""; }
int Cvar_Flags( const char *name ) { (void)name; return 0; }
void Cvar_SetSafe( const char *name, const char *value ) { (void)name; (void)value; }
cvar_t *Cvar_Get( const char *name, const char *value, int flags ) { (void)name; (void)value; (void)flags; return &shownet; }

int main( int argc, char **argv ) {
	static byte data[MAX_MSGLEN];
	static char systemInfo[BIG_INFO_STRING];
	FILE *in;
	msg_t msg;
	int length, full = 0, degraded = 0;

	cl_shownet = &shownet;
	Check( argc == 2, "usage: client-gamestate-budget <gamestates>" );
	in = fopen( argv[1], "rb" );
	Check( in != NULL, "open the gamestates" );
	while ( fread( &length, sizeof( length ), 1, in ) == 1 ) {
		gamestate++;
		Check( length > 0 && length <= MAX_MSGLEN && fread( data, 1, length, in ) == (size_t)length, "message fits a client's buffer" );
		MSG_Init( &msg, data, sizeof( data ) );	/* sets up the Huffman tables */
		msg.cursize = length;
		Check( fread( &length, sizeof( length ), 1, in ) == 1 && length >= 0 && length < BIG_INFO_STRING
			&& fread( systemInfo, 1, length, in ) == (size_t)length, "record" );
		systemInfo[length] = 0;

		/* CL_ParseServerMessage: the acknowledge, then svc_gamestate, then svc_EOF */
		memset( &clc, 0, sizeof( clc ) );
		pureSums[0] = pureNames[0] = 0;
		downloads = 0;
		MSG_Bitstream( &msg );
		MSG_BeginReading( &msg );
		MSG_ReadLong( &msg );
		Check( MSG_ReadByte( &msg ) == svc_gamestate, "gamestate command" );
		if ( setjmp( dropJump ) ) {
			Check( 0, dropText );
		}
		CL_ParseGamestate( &msg );
		Check( downloads == 1 && cl.gameState.dataCount <= MAX_GAMESTATE_CHARS, "gamestate accepted inside MAX_GAMESTATE_CHARS" );
		Check( MSG_ReadByte( &msg ) == svc_EOF && msg.readcount <= msg.cursize, "message ends with the netchan's svc_EOF" );
		Check( !strcmp( cl.gameState.stringData + cl.gameState.stringOffsets[CS_SYSTEMINFO], systemInfo ), "server's systeminfo arrives whole" );
		Check( !strcmp( pureSums, Info_ValueForKey( systemInfo, "sv_paks" ) )
			&& !strcmp( pureNames, Info_ValueForKey( systemInfo, "sv_pakNames" ) ), "client sees the pure lists the server kept" );
		if ( pureSums[0] ) full++; else degraded++;
	}
	fclose( in );
	Check( gamestate > 100 && full > 0 && degraded > 0, "the server sent full and degraded pure gamestates" );
	printf( "Client parsed %d gamestates: %d with sv_paks, %d without (degraded or not pure)\n", gamestate, full, degraded );
	return 0;
}
