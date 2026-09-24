/* Issue #38: getstatus, getinfo, getchallenge and rcon are rate limited with ioquake3's leaky buckets: 10 at once and
 * one a second more for each source address, and for the three queries 10 at once and one every 100 msec more for all
 * sources together; bad rcon passwords share their own 10 at once and one a second more. A flooding source only loses
 * its own quota, a spoofed flood cannot make the server an amplifier, a bad-rcon flood cannot lock out the admin, and a
 * clock that jumps back or passes INT_MAX neither disables nor locks the limits. Replies within the limits are the
 * retail bytes. Usage: server_rate_limit <case>. */
#include "../code/server/sv_main.c"
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#define PASSWORD	"sesame"
#define HOSTNAME	"Mac OS 9 test server"
#define TABLE		16384	/* ioquake3's MAX_BUCKETS */
#define OOB			"\xff\xff\xff\xff"
#define SERVERINFO	"\\sv_hostname\\" HOSTNAME "\\sv_maxclients\\8\\g_gametype\\0\\protocol\\68\\mapname\\q3dm17" \
	"\\version\\Q3 1.32c ppc-mac\\sv_privateClients\\0"
/* Info_SetValueForKey prepends, so SVC_Info's keys come out in reverse order. */
#define INFO_REPLY( challenge )	OOB "infoResponse\n\\pure\\1\\gametype\\0\\sv_maxclients\\8\\clients\\1" \
	"\\mapname\\q3dm17\\hostname\\" HOSTNAME "\\protocol\\68\\challenge\\" challenge
#define STATUS_REPLY			OOB "statusResponse\n" SERVERINFO "\n5 48 \"Visor\"\n"
#define MASTER_STATUS_REPLY		OOB "statusResponse\n\\challenge\\m4st3r" SERVERINFO "\n5 48 \"Visor\"\n"
#define CHALLENGE_REPLY			OOB "challengeResponse 1234"
#define RCON					"rcon " PASSWORD " sv_hostname"
#define RCON_REPLY				OOB "print\n\"sv_hostname\" is:\"" HOSTNAME "^7\" default:\"noname^7\"\n"
#define BAD_RCON				"rcon guess sv_hostname"
#define BAD_RCON_REPLY			OOB "print\nBad rconpassword.\n"

cvar_t *com_cl_running, *com_sv_running, *cl_shownet;	/* no client or game: rcon runs only cvar commands */
static cvar_t maxclients = { .integer = 8 }, zero = { .string = "" }, hostname = { .string = HOSTNAME };
static cvar_t mapname = { .string = "q3dm17" }, pure = { .integer = 1 }, rconPassword = { .string = PASSWORD };
static client_t clients[8];
static playerState_t players[8];
static int now, replies, connects, executed, consolePrints;
static char reply[MAX_MSGLEN];
static netadr_t replyTo;

/** Fail with the violated rate-limit property. */
static void Check( int ok, const char *message ) {
	if ( !ok ) { fprintf( stderr, "Server rate limit regression failed: %s\n", message ); exit( 1 ); }
}
void QDECL Com_Error( int level, const char *format, ... ) { (void)level; (void)format; Check( 0, "Com_Error" ); }
void QDECL Com_DPrintf( const char *format, ... ) { (void)format; }
/** common.c's redirect: while an rcon runs, prints collect in its reply buffer. */
static char *redirectBuffer;
static int redirectSize;
static void ( *redirectFlush )( char *buffer );
void Com_BeginRedirect( char *buffer, int buffersize, void ( *flush )( char *buffer ) ) {
	redirectBuffer = buffer; redirectSize = buffersize; redirectFlush = flush; *buffer = 0;
}
void Com_EndRedirect( void ) {
	if ( redirectFlush ) redirectFlush( redirectBuffer );
	redirectBuffer = NULL; redirectSize = 0; redirectFlush = NULL;
}
void QDECL Com_Printf( const char *format, ... ) {
	char text[MAXPRINTMSG];
	va_list argptr;
	va_start( argptr, format ); Q_vsnprintf( text, sizeof( text ), format, argptr ); va_end( argptr );
	if ( redirectBuffer ) {
		Check( strlen( redirectBuffer ) + strlen( text ) < (size_t)redirectSize, "rcon output split" );
		strcat( redirectBuffer, text );
		return;
	}
	consolePrints++;	/* "Bad rcon from ..." */
}
void Com_Memset( void *dest, const int val, const size_t count ) { memset( dest, val, count ); }
void Com_Memcpy( void *dest, const void *src, const size_t count ) { memcpy( dest, src, count ); }
/** The engine's millisecond clock, which the test moves; retail's global 500 msec rcon gate read Com_Milliseconds. */
int Sys_Milliseconds( void ) { return now; }
int Com_Milliseconds( void ) { return now; }
const char *NET_AdrToString( netadr_t a ) {
	static char s[64];
	Com_sprintf( s, sizeof( s ), "%i.%i.%i.%i", a.ip[0], a.ip[1], a.ip[2], a.ip[3] );
	return s;
}
/** FFA multiplayer, not single player, no fs_restrict and no fs_game. */
float Cvar_VariableValue( const char *name ) { (void)name; return 0; }
char *Cvar_VariableString( const char *name ) { (void)name; return ""; }
char *Cvar_InfoString( int bit ) {
	static char info[MAX_INFO_STRING];
	Check( bit == CVAR_SERVERINFO, "serverinfo" );
	strcpy( info, SERVERINFO );
	return info;
}
playerState_t *SV_GameClientNum( int num ) { return &players[num]; }
/** "rcon <password> sv_hostname" prints the cvar as Cvar_Command does. */
qboolean Cvar_Command( void ) {
	Check( !strcmp( Cmd_Argv( 0 ), "sv_hostname" ), "rcon ran another command" );
	Com_Printf( "\"%s\" is:\"%s" S_COLOR_WHITE "\" default:\"%s" S_COLOR_WHITE "\"\n", "sv_hostname", HOSTNAME, "noname" );
	executed++;
	return qtrue;
}
static void Unreachable( void ) { Check( 0, "rcon command reached the client or game" ); }
qboolean CL_GameCommand( void ) { Unreachable(); return qfalse; }
qboolean SV_GameCommand( void ) { Unreachable(); return qfalse; }
qboolean UI_GameCommand( void ) { Unreachable(); return qfalse; }
void CL_ForwardCommandToServer( const char *string ) { (void)string; Unreachable(); }
/** Capture the datagram as NET_OutOfBandPrint sends it. */
void QDECL NET_OutOfBandPrint( netsrc_t sock, netadr_t adr, const char *format, ... ) {
	va_list argptr;
	Check( sock == NS_SERVER, "reply socket" );
	memcpy( reply, OOB, 4 );
	va_start( argptr, format ); Q_vsnprintf( reply + 4, sizeof( reply ) - 4, format, argptr ); va_end( argptr );
	replyTo = adr;
	replies++;
}
/** sv_client.c's SV_GetChallenge answers a LAN client, or one whose authorize server timed out, at once. */
void SV_GetChallenge( netadr_t from ) { NET_OutOfBandPrint( NS_SERVER, from, "challengeResponse %i", 1234 ); }
void SV_DirectConnect( netadr_t from ) { (void)from; connects++; }
void SV_AuthorizeIpPacket( netadr_t from ) { (void)from; Check( 0, "ipAuthorize from a client" ); }

static netadr_t Address( int a, int b, int c, int d ) {
	netadr_t adr;
	memset( &adr, 0, sizeof( adr ) );
	adr.type = NA_IP; adr.ip[0] = a; adr.ip[1] = b; adr.ip[2] = c; adr.ip[3] = d; adr.port = 0x6d38;
	return adr;
}
/** Source n of a spoofed flood from many addresses. */
static netadr_t Spoofed( int n ) { return Address( 10, ( n >> 16 ) & 255, ( n >> 8 ) & 255, n & 255 ); }
/** Move the clock; like Sys_Milliseconds, it wraps past INT_MAX. */
static void Advance( int msec ) { now = (int)( (unsigned)now + (unsigned)msec ); }

/** Deliver a connectionless packet the way SV_PacketEvent does; return whether it was answered. */
static int Send( netadr_t from, const char *line ) {
	byte data[MAX_MSGLEN];
	msg_t msg;
	int before = replies;
	memset( &msg, 0, sizeof( msg ) );
	msg.data = data; msg.maxsize = sizeof( data );
	memcpy( data, OOB, 4 ); memcpy( data + 4, line, strlen( line ) ); msg.cursize = 4 + (int)strlen( line );
	reply[0] = 0;
	SV_ConnectionlessPacket( from, &msg );
	Check( replies - before <= 1, "more than one reply datagram" );
	if ( replies > before ) {
		Check( replyTo.type == from.type && !memcmp( replyTo.ip, from.ip, 4 ) && !memcmp( replyTo.ipx, from.ipx, 10 )
		       && replyTo.port == from.port, "reply sent to another address" );
	}
	return replies - before;
}
/** Deliver a request that must be answered with the retail reply bytes. */
static void Expect( netadr_t from, const char *line, const char *expected, const char *message ) {
	Check( Send( from, line ) == 1, message );
	Check( !strcmp( reply, expected ), "reply bytes changed" );
}
/** Deliver a client's connect as NET_OutOfBandData does, Huffman coded after the 12-byte header. */
static void Connect( netadr_t from ) {
	static const char line[] = OOB "connect \"\\challenge\\1234\\qport\\4711\\protocol\\68\\name\\Visor\"";
	byte data[MAX_MSGLEN];
	msg_t msg;
	memset( &msg, 0, sizeof( msg ) );
	msg.data = data; msg.maxsize = sizeof( data );
	memcpy( data, line, sizeof( line ) - 1 ); msg.cursize = sizeof( line ) - 1;
	Huff_Compress( &msg, 12 );
	SV_ConnectionlessPacket( from, &msg );
}

/** Retail clients, masters, monitors and admins at their normal rates are all answered, with the retail bytes. */
static void NormalUse( void ) {
	netadr_t client = Address( 198, 51, 100, 7 ), master = Address( 192, 0, 2, 50 ), monitor = Address( 203, 0, 113, 80 );
	netadr_t admin = Address( 198, 51, 100, 200 ), local;
	int i, j;

	memset( &local, 0, sizeof( local ) ); local.type = NA_LOOPBACK;
	/* A LAN refresh broadcasts "getinfo xxx" twice (CL_LocalServers_f), then the browser pings the server. */
	Expect( client, "getinfo xxx", INFO_REPLY( "xxx" ), "LAN refresh" );
	Expect( client, "getinfo xxx", INFO_REPLY( "xxx" ), "LAN refresh repeat" );
	Advance( 15 ); Expect( client, "getinfo xxx", INFO_REPLY( "xxx" ), "browser ping" );
	/* Team Arena's server info panel asks for the status, and CL_ServerStatus resends it after 750 msec. */
	Advance( 1000 ); Expect( client, "getstatus", STATUS_REPLY, "server status" );
	Advance( 750 ); Expect( client, "getstatus", STATUS_REPLY, "server status resend" );
	Advance( 3250 );
	for ( i = 0; i < 3; i++ ) Expect( client, "getinfo xxx", INFO_REPLY( "xxx" ), "second refresh" );
	/* /connect: CL_CheckForResend sends getchallenge every 3 s until it is answered, then connect likewise. */
	for ( i = 0; i < 3; i++ ) { Advance( 3000 ); Expect( client, "getchallenge", CHALLENGE_REPLY, "getchallenge resend" ); }
	for ( i = 0; i < 20; i++ ) Connect( client );
	Check( connects == 20, "connect rate limited" );
	/* A second later the local client and eight other players' browsers are answered together. */
	Advance( 1000 );
	Expect( local, "getinfo xxx", INFO_REPLY( "xxx" ), "loopback getinfo" );
	Expect( local, "getstatus", STATUS_REPLY, "loopback getstatus" );
	for ( i = 1; i <= 8; i++ ) Expect( Address( 198, 51, 101, i ), "getinfo xxx", INFO_REPLY( "xxx" ), "browser burst" );
	/* An admin types rcon commands, as fast as the retail 500 msec gate let them through. */
	for ( i = 0; i < 5; i++ ) { Advance( 500 ); Expect( admin, RCON, RCON_REPLY, "admin rcon" ); }
	Check( executed == 5, "rcon not executed" );
	/* Heartbeats every 300 s draw a dpmaster getinfo and an id master getstatus, while qstat polls every 2 s. */
	for ( i = 0; i < 3; i++ ) {
		Expect( master, "getinfo m4st3r", INFO_REPLY( "m4st3r" ), "master getinfo" );
		Expect( master, "getstatus m4st3r", MASTER_STATUS_REPLY, "master getstatus" );
		for ( j = 0; j < 150; j++ ) { Advance( 2000 ); Expect( monitor, "getstatus", STATUS_REPLY, "monitor getstatus" ); }
	}
}

/** One source flooding any of the commands gets its burst of 10, then one a second. */
static void SourceFlood( void ) {
	static const char *const lines[] = { "getstatus", "getinfo xxx", "getchallenge", RCON, BAD_RCON };
	netadr_t flooder = Address( 198, 51, 100, 66 );
	int c, i, answered;

	for ( c = 0; c < 5; c++ ) {
		Advance( 60000 );	/* every bucket has drained */
		for ( i = answered = 0; i < 50; i++ ) answered += Send( flooder, lines[c] );
		Check( answered == 10, "flood not limited to its burst" );
		for ( i = answered = 0; i < 300; i++ ) { Advance( 10 ); answered += Send( flooder, lines[c] ); }
		Check( answered == 3, "flood not limited to one a second" );
	}
	Check( executed == 13, "rcon with the password not executed within the limit" );
}

/** While one source floods, another is answered every time. */
static void OtherSource( void ) {
	static const char *const lines[] = { "getstatus", "getinfo xxx", "getchallenge", BAD_RCON };
	static const char *const expected[] = { STATUS_REPLY, INFO_REPLY( "xxx" ), CHALLENGE_REPLY };
	netadr_t flooder = Address( 198, 51, 100, 66 ), other = Address( 198, 51, 100, 67 ), ipx, otherIpx, local;
	int t, answered = 0, served = 0;

	memset( &ipx, 0, sizeof( ipx ) ); ipx.type = NA_IPX; memcpy( ipx.ipx, "\x00\x00\x00\x01\x00\x10\x5a\x01\x02\x03", 10 );
	otherIpx = ipx; otherIpx.ipx[9] = 4;
	for ( t = 0; t < 5000; t += 2 ) {
		answered += Send( flooder, lines[( t / 2 ) % 4] );
		if ( t % 500 == 100 ) {
			Expect( other, lines[served % 3], expected[served % 3], "second source starved by the flood" );
			served++;
		}
		Advance( 2 );
	}
	Check( answered == 10 + 4, "flooding source got more than its own quota" );
	/* The same for IPX sources, told apart by their whole address. */
	Advance( 60000 );
	for ( t = answered = 0; t < 50; t++ ) answered += Send( ipx, "getinfo xxx" );
	Check( answered == 10, "IPX flood not limited" );
	Advance( 100 );
	Expect( otherIpx, "getinfo xxx", INFO_REPLY( "xxx" ), "second IPX source starved" );
	/* Loopback and 192.0.2.130 hash to the same chain, but are different sources. */
	Advance( 60000 );
	memset( &local, 0, sizeof( local ) ); local.type = NA_LOOPBACK;
	for ( t = answered = 0; t < 50; t++ ) answered += Send( local, "getinfo xxx" );
	Check( answered == 10, "loopback flood not limited" );
	Advance( 100 );
	Expect( Address( 192, 0, 2, 130 ), "getinfo xxx", INFO_REPLY( "xxx" ), "source in loopback's hash chain starved" );
}

/** A spoofed flood from many addresses gets 10 replies at once and one every 100 msec, however it is spread. */
static void GlobalCap( void ) {
	static const char *const lines[] = { "getstatus", "getinfo xxx", "getchallenge" };
	int t, i, n = 0, answered = 0;

	for ( t = 0; t < 1000; t++ ) {
		for ( i = 0; i < 5; i++, n++ ) answered += Send( Spoofed( n ), lines[n % 3] );
		Advance( 1 );
	}
	Check( answered == 10 + 9, "many-address flood not limited globally" );
	/* It does not lock the server: once the flood stops, a new source is answered. */
	Advance( 1000 );
	Expect( Address( 198, 51, 100, 9 ), "getinfo xxx", INFO_REPLY( "xxx" ), "global limit stuck after the flood" );
}

/** A clock that jumps back or passes INT_MAX neither disables nor locks the limits. */
static void Clock( void ) {
	static const int jumps[] = { -1, -5000, -300000 };
	static const char *const lines[] = { "getstatus", "getinfo xxx", "getchallenge" };
	netadr_t flooder = Address( 198, 51, 100, 66 );
	int j, i, t, n = 0, answered;

	for ( j = 0; j < 3; j++ ) {
		Advance( 60000 );
		for ( i = answered = 0; i < 50; i++ ) answered += Send( flooder, "getinfo xxx" );
		Check( answered == 10, "flood before the jump" );
		/* A clock that went back restarts the buckets: one more burst, then the limit again. */
		Advance( jumps[j] );
		for ( i = answered = 0; i < 50; i++ ) answered += Send( flooder, "getinfo xxx" );
		Check( answered == 10, "clock jumping back disabled or locked the limit" );
		for ( i = answered = 0; i < 300; i++ ) { Advance( 10 ); answered += Send( flooder, "getinfo xxx" ); }
		Check( answered == 3, "limit after the clock jumped back" );
	}
	/* Sys_Milliseconds passes INT_MAX after 24.8 days and wraps; the buckets keep leaking at their rate. */
	now = INT_MAX - 1500;
	for ( i = answered = 0; i < 50; i++ ) answered += Send( flooder, "getstatus" );
	Check( answered == 10, "flood before INT_MAX" );
	for ( i = answered = 0; i < 400; i++ ) { Advance( 10 ); answered += Send( flooder, "getstatus" ); }
	Check( now < 0 && answered == 4, "clock passing INT_MAX disabled or locked the per-address limit" );
	now = INT_MAX - 500;
	for ( t = answered = 0; t < 1000; t++ ) {
		for ( i = 0; i < 5; i++, n++ ) answered += Send( Spoofed( n ), lines[n % 3] );
		Advance( 1 );
	}
	Check( now < 0 && answered == 10 + 9, "clock passing INT_MAX disabled or locked the global limit" );
}

/** A bad-rcon flood gets 10 replies at once and one a second, and never locks out the admin elsewhere. */
static void BadRcon( void ) {
	netadr_t admin = Address( 198, 51, 100, 200 ), guesser = Address( 198, 51, 100, 66 );
	int t, answered = 0, prints;

	for ( t = 0; t < 3000; t += 2 ) {
		answered += Send( Spoofed( t % 1000 ), BAD_RCON );
		if ( t % 500 == 0 ) Expect( admin, RCON, RCON_REPLY, "bad-rcon flood locked out the admin" );
		Advance( 2 );
	}
	Check( executed == 6, "admin rcon not executed" );
	Check( answered == 10 + 2, "many-address bad-rcon flood not limited" );
	/* One source guessing passwords: its own limit, and the admin still gets in. */
	Advance( 60000 );
	prints = consolePrints;
	for ( t = answered = 0; t < 3000; t += 2 ) {
		answered += Send( guesser, BAD_RCON );
		if ( t % 500 == 250 ) Expect( admin, RCON, RCON_REPLY, "password guesser locked out the admin" );
		Advance( 2 );
	}
	Check( answered == 10 + 2 && consolePrints - prints == 10 + 2 + 6, "password guesses not limited" );
	Advance( 60000 );
	Expect( guesser, BAD_RCON, BAD_RCON_REPLY, "bad rcon within the limit" );
}

/** The table is fixed: a flood of new addresses fills it, then waits for buckets to expire, before and after the
 * clock wraps negative (ioquake3 only reclaims buckets with a positive time). */
static void Table( void ) {
	netadr_t late = Address( 198, 51, 100, 9 );
	int round, n;

	now = INT_MAX - 4000;
	for ( round = 0; round < 2; round++ ) {
		for ( n = 0; n < TABLE; n++ ) Send( Spoofed( n ), "getinfo xxx" );
		Advance( 1000 );	/* the global bucket has refilled */
		Check( Send( late, "getinfo xxx" ) == 0, "source past the full table answered" );
		Check( Send( Spoofed( 5 ), "getinfo xxx" ) == 1, "source in the table not answered" );
		Advance( 10001 );	/* round 0 passes INT_MAX */
		Expect( late, "getinfo xxx", INFO_REPLY( "xxx" ), "expired buckets not reclaimed" );
		Advance( 60000 );	/* round 1 runs at negative times */
		Check( now < 0, "clock did not wrap" );
	}
}

int main( int argc, char **argv ) {
	int test;

	Check( argc == 2, "usage: server_rate_limit <case>" );
	test = atoi( argv[1] );
	sv_maxclients = &maxclients; sv_privateClients = &zero; sv_hostname = &hostname; sv_mapname = &mapname;
	sv_gametype = &zero; sv_pure = &pure; sv_minPing = &zero; sv_maxPing = &zero; sv_rconPassword = &rconPassword;
	svs.clients = clients;
	clients[0].state = CS_ACTIVE; clients[0].ping = 48; Q_strncpyz( clients[0].name, "Visor", sizeof( clients[0].name ) );
	players[0].persistant[PERS_SCORE] = 5;
	now = 100000;
	switch ( test ) {
	case 0: NormalUse(); break;
	case 1: SourceFlood(); break;
	case 2: OtherSource(); break;
	case 3: GlobalCap(); break;
	case 4: Clock(); break;
	case 5: BadRcon(); break;
	case 6: Table(); break;
	default: Check( 0, "unknown case" );
	}
	printf( "Server rate limit regression %d passed (issue #38)\n", test );
	return 0;
}
