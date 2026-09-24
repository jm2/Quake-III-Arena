/* Issue #415: SV_GetChallenge and SV_SpawnServer's checksum feed shift rand() left by 16. RAND_MAX is
   0x7fffffff on glibc and Retro68 newlib, so a signed shift overflows int; the values must stay the
   bits master's wrapped shift gave. Usage: server_challenge_rand <rand1> <rand2> <time>. */
#include "../code/server/sv_client.c"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

serverStatic_t svs;
server_t sv;
vm_t *gvm;
cvar_t *sv_maxclients, *sv_reconnectlimit, *sv_privatePassword, *sv_privateClients, *sv_minPing, *sv_maxPing;
cvar_t *sv_lanForceRate, *sv_strictAuth, *sv_pure, *com_dedicated;
static int randValues[2], randCalls, milliseconds;
static char reply[MAX_MSGLEN];

/** Fail with the generator whose value changed. */
static void Check( int ok, const char *message ) {
	if ( !ok ) { fprintf( stderr, "Server challenge rand regression failed: %s\n", message ); exit( 1 ); }
}
/** The C library's generator, returning the case's two values in turn. */
int rand( void ) {
	Check( randCalls < 2, "more than two rand() calls" );
	return randValues[randCalls++];
}
int Com_Milliseconds( void ) { return milliseconds; }
float Cvar_VariableValue( const char *name ) { (void)name; return 0; }	/* not single player */
qboolean Sys_IsLANAddress( netadr_t adr ) { (void)adr; return qtrue; }	/* answer at once */
qboolean NET_CompareAdr( netadr_t a, netadr_t b ) { return a.type == b.type && !memcmp( a.ip, b.ip, 4 ) && a.port == b.port; }
/** Capture the challengeResponse. */
void QDECL NET_OutOfBandPrint( netsrc_t sock, netadr_t adr, const char *format, ... ) {
	va_list argptr;
	(void)sock; (void)adr;
	va_start( argptr, format ); Q_vsnprintf( reply, sizeof( reply ), format, argptr ); va_end( argptr );
}
void QDECL Com_Error( int level, const char *format, ... ) { (void)level; (void)format; Check( 0, "unexpected Com_Error" ); }
void QDECL Com_Printf( const char *format, ... ) { (void)format; }
void QDECL Com_DPrintf( const char *format, ... ) { (void)format; }

/* The sanitizer's global registration keeps the ucmds table, so its other handlers must link. */
static void Unreachable( void ) { Check( 0, "unrelated server code reached" ); }
char *Cmd_Argv( int arg ) { (void)arg; Unreachable(); return ""; }
int Cmd_Argc( void ) { Unreachable(); return 0; }
void Cmd_TokenizeString( const char *text ) { (void)text; Unreachable(); }
qboolean NET_IsLocalAddress( netadr_t adr ) { (void)adr; Unreachable(); return qfalse; }
qboolean NET_CompareBaseAdr( netadr_t a, netadr_t b ) { (void)a; (void)b; Unreachable(); return qfalse; }
const char *NET_AdrToString( netadr_t a ) { (void)a; Unreachable(); return ""; }
qboolean NET_StringToAdr( const char *s, netadr_t *a ) { (void)s; (void)a; Unreachable(); return qfalse; }
cvar_t *Cvar_Get( const char *name, const char *value, int flags ) { (void)name; (void)value; (void)flags; Unreachable(); return NULL; }
void Netchan_Setup( netsrc_t sock, netchan_t *chan, netadr_t adr, int qport ) {
	(void)sock; (void)chan; (void)adr; (void)qport; Unreachable();
}
void SV_Netchan_FreeQueue( client_t *client ) { (void)client; Unreachable(); }
void QDECL SV_SendServerCommand( client_t *cl, const char *format, ... ) { (void)cl; (void)format; Unreachable(); }
void SV_SetUserinfo( int index, const char *val ) { (void)index; (void)val; Unreachable(); }
sharedEntity_t *SV_GentityNum( int num ) { (void)num; Unreachable(); return NULL; }
void SV_Heartbeat_f( void ) { Unreachable(); }
void SV_BotFreeClient( int clientNum ) { (void)clientNum; Unreachable(); }
void FS_FCloseFile( fileHandle_t f ) { (void)f; Unreachable(); }
void Z_Free( void *ptr ) { (void)ptr; Unreachable(); }
void Com_Memset( void *dest, const int val, const size_t count ) { (void)dest; (void)val; (void)count; Unreachable(); }
int FS_FileIsInPAK( const char *filename, int *pChecksum ) { (void)filename; (void)pChecksum; Unreachable(); return -1; }
const char *FS_LoadedPakPureChecksums( void ) { Unreachable(); return ""; }
void MSG_Init( msg_t *buf, byte *data, int length ) { (void)buf; (void)data; (void)length; Unreachable(); }
void MSG_WriteByte( msg_t *sb, int c ) { (void)sb; (void)c; Unreachable(); }
void MSG_WriteShort( msg_t *sb, int c ) { (void)sb; (void)c; Unreachable(); }
void MSG_WriteLong( msg_t *sb, int c ) { (void)sb; (void)c; Unreachable(); }
void MSG_WriteBigString( msg_t *sb, const char *s ) { (void)sb; (void)s; Unreachable(); }
void MSG_WriteDeltaEntity( msg_t *msg, struct entityState_s *from, struct entityState_s *to, qboolean force ) {
	(void)msg; (void)from; (void)to; (void)force; Unreachable();
}
void SV_UpdateServerCommandsToClient( client_t *client, msg_t *msg ) { (void)client; (void)msg; Unreachable(); }
void SV_SendMessageToClient( msg_t *msg, client_t *client ) { (void)msg; (void)client; Unreachable(); }
void SV_SendClientSnapshot( client_t *client ) { (void)client; Unreachable(); }
int VM_CallArgs( vm_t *vm, int callNum, const int *args, int argCount ) {
	(void)vm; (void)callNum; (void)args; (void)argCount; Unreachable(); return 0;
}
char *VM_CheckedExplicitString( vm_t *vm, int value, qboolean nullable ) { (void)vm; (void)value; (void)nullable; Unreachable(); return NULL; }

/** SV_SpawnServer's checksum feed statements, extracted from sv_init.c by the runner. */
static void SpawnChecksumFeed( void ) {
#include Q3_SV_CHECKSUM_FEED
}

/** What master's signed ( rand() << 16 ) ^ rand() ^ time wrapped to, in either call order. */
static int IsWrapped( int value, unsigned high, unsigned low, unsigned time ) {
	return value == (int)( ( high << 16 ) ^ low ^ time ) || value == (int)( ( low << 16 ) ^ high ^ time );
}

int main( int argc, char **argv ) {
	netadr_t from;
	unsigned first, second, time;
	char expected[64];

	Check( argc == 4, "usage: server_challenge_rand <rand1> <rand2> <time>" );
	first = (unsigned)strtoul( argv[1], NULL, 0 ); second = (unsigned)strtoul( argv[2], NULL, 0 );
	time = (unsigned)strtoul( argv[3], NULL, 0 );
	Check( first <= 0x7fffffff && second <= 0x7fffffff && time <= 0x7fffffff, "values past RAND_MAX or INT_MAX" );

	/* A new address takes the oldest challenge slot, 0, and a LAN client gets it at once. */
	memset( &from, 0, sizeof( from ) );
	from.type = NA_IP; from.ip[0] = 192; from.ip[1] = 168; from.ip[2] = 1; from.ip[3] = 2; from.port = BigShort( 27960 );
	svs.time = (int)time;
	randValues[0] = (int)first; randValues[1] = (int)second; randCalls = 0;
	SV_GetChallenge( from );
	Check( randCalls == 2, "SV_GetChallenge rand() calls" );
	Check( IsWrapped( svs.challenges[0].challenge, first, second, time ), "SV_GetChallenge challenge bits" );
	Com_sprintf( expected, sizeof( expected ), "challengeResponse %i", svs.challenges[0].challenge );
	Check( !strcmp( reply, expected ), "challengeResponse text" );

	milliseconds = (int)time;
	randValues[0] = (int)first; randValues[1] = (int)second; randCalls = 0;
	SpawnChecksumFeed();
	Check( randCalls == 2, "SV_SpawnServer rand() calls" );
	Check( IsWrapped( sv.checksumFeed, first, second, time ), "SV_SpawnServer checksum feed bits" );

	printf( "Server challenge rand regressions passed (issue #415): %s %s %s -> %i, %i\n",
	        argv[1], argv[2], argv[3], svs.challenges[0].challenge, sv.checksumFeed );
	return 0;
}
