/* Issue #415: CL_RequestMotd shifts rand() left by 16 for the update server challenge. RAND_MAX is
   0x7fffffff on glibc and Retro68 newlib, so a signed shift overflows int; the value must stay the
   bits master's wrapped shift gave. Usage: client_challenge_rand <rand1> <rand2> <milliseconds>. */
#include "../code/client/cl_main.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

cvar_t *com_version;
static cvar_t motd, version, quiet;
extern cvar_t *showpackets;
static int randValues[2], randCalls, milliseconds, sends;
static char sent[MAX_MSGLEN];

/** Fail with the value that changed. */
static void Check( int ok, const char *message ) {
	if ( !ok ) { fprintf( stderr, "Client challenge rand regression failed: %s\n", message ); exit( 1 ); }
}
/** The C library's generator, returning the case's two values in turn. */
int rand( void ) {
	Check( randCalls < 2, "more than two rand() calls" );
	return randValues[randCalls++];
}
int Com_Milliseconds( void ) { return milliseconds; }
void QDECL Com_Error( int level, const char *format, ... ) { (void)level; (void)format; Check( 0, "engine error" ); }
void QDECL Com_Printf( const char *format, ... ) { (void)format; }
void QDECL Com_DPrintf( const char *format, ... ) { (void)format; }
void Com_Memcpy( void *out, const void *in, size_t size ) { memcpy( out, in, size ); }
void Com_Memset( void *out, int value, size_t size ) { memset( out, value, size ); }
/** Resolve the update server to a documentation address. */
qboolean Sys_StringToAdr( const char *name, netadr_t *address ) {
	Check( !strcmp( name, UPDATE_SERVER_NAME ), "update server name" );
	memset( address, 0, sizeof( *address ) );
	address->type = NA_IP; address->ip[0] = 192; address->ip[1] = 0; address->ip[2] = 2; address->ip[3] = 27;
	return qtrue;
}
/** Record the getmotd request. */
void Sys_SendPacket( int length, const void *data, netadr_t to ) {
	(void)to;
	Check( length > 0 && length < (int)sizeof( sent ), "packet length" );
	memcpy( sent, data, length ); sent[length] = 0;
	sends++;
}

/** Whether value is what master's signed ( rand() << 16 ) ^ rand() ^ time wrapped to, in either call order. */
static int IsWrapped( int value, unsigned high, unsigned low, unsigned time ) {
	return value == (int)( ( high << 16 ) ^ low ^ time ) || value == (int)( ( low << 16 ) ^ high ^ time );
}

int main( int argc, char **argv ) {
	unsigned first, second, time;
	char expected[64], *end;

	Check( argc == 4, "usage: client_challenge_rand <rand1> <rand2> <milliseconds>" );
	first = (unsigned)strtoul( argv[1], NULL, 0 ); second = (unsigned)strtoul( argv[2], NULL, 0 );
	time = (unsigned)strtoul( argv[3], NULL, 0 );
	Check( first <= 0x7fffffff && second <= 0x7fffffff && time <= 0x7fffffff, "values past RAND_MAX or INT_MAX" );

	showpackets = &quiet;
	motd.integer = 1; cl_motd = &motd;
	version.string = "Q3 1.32c test"; com_version = &version;
	milliseconds = (int)time;
	randValues[0] = (int)first; randValues[1] = (int)second; randCalls = 0;
	CL_RequestMotd();
	Check( randCalls == 2, "CL_RequestMotd rand() calls" );
	Check( IsWrapped( atoi( cls.updateChallenge ), first, second, time ), "update challenge bits" );
	Com_sprintf( expected, sizeof( expected ), "%i", atoi( cls.updateChallenge ) );
	Check( !strcmp( cls.updateChallenge, expected ), "update challenge text" );
	/* the request is getmotd "<info>"\n */
	Check( sends == 1 && !strncmp( sent, "\xff\xff\xff\xffgetmotd \"", 13 ) && ( end = strrchr( sent, '"' ) ) > sent + 12,
	       "getmotd request" );
	*end = 0;
	Check( !strcmp( Info_ValueForKey( sent + 13, "challenge" ), cls.updateChallenge ), "getmotd challenge" );

	printf( "Client challenge rand regressions passed (issue #415): %s %s %s -> %s\n",
	        argv[1], argv[2], argv[3], cls.updateChallenge );
	return 0;
}
