/* Issue #255: drive the real master query and getserversResponse parser; discard other client code at link. */
#include "../code/client/cl_main.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_HEADER		"\xff\xff\xff\xffgetserversResponse"
#define TEST_MAX_PACKET	( 32 + MAX_SERVERSPERPACKET * 7 )

static const char *commandArgs[3];
static byte resolvedIp[4] = { 192, 0, 2, 1 };
static qboolean resolveOk = qtrue;
static netadr_t queried;
static int queries;
static cvar_t quiet;
extern cvar_t *showpackets;

/** Stop on the first master-list state that differs from the bounded protocol. */
static void Check( int ok, const char *message ) {
	if ( !ok ) { fprintf( stderr, "Master list regression failed: %s\n", message ); exit( 1 ); }
}
void QDECL Com_Error( int level, const char *format, ... ) { (void)level; (void)format; Check( 0, "engine error" ); }
void QDECL Com_Printf( const char *format, ... ) { (void)format; }
void QDECL Com_DPrintf( const char *format, ... ) { (void)format; }
void Com_Memcpy( void *out, const void *in, size_t size ) { memcpy( out, in, size ); }
void Com_Memset( void *out, int value, size_t size ) { memset( out, value, size ); }
int Cmd_Argc( void ) { return 3; }
char *Cmd_Argv( int argument ) { return argument >= 0 && argument < 3 ? (char *)commandArgs[argument] : ""; }
float Cvar_VariableValue( const char *name ) { (void)name; return 0; }
/** Resolve the master name to a documentation address; a failed lookup leaves those bytes behind, as the caller's uninitialized netadr_t may. */
qboolean Sys_StringToAdr( const char *name, netadr_t *address ) {
	Check( !strcmp( name, MASTER_SERVER_NAME ), "master name" );
	address->type = NA_IP;
	memcpy( address->ip, resolvedIp, sizeof( resolvedIp ) );
	return resolveOk;
}
/** Record where the real getservers query is sent. */
void Sys_SendPacket( int length, const void *data, netadr_t to ) {
	Check( length > 4 && !memcmp( (const char *)data + 4, "getservers 68", 13 ), "getservers query" );
	queried = to;
	queries++;
}

/** Build an IPv4 source address with a host-order port. */
static netadr_t Address( int a, int b, int c, int d, int port ) {
	netadr_t address;
	memset( &address, 0, sizeof( address ) );
	address.type = NA_IP;
	address.ip[0] = a; address.ip[1] = b; address.ip[2] = c; address.ip[3] = d;
	address.port = BigShort( (short)port );
	return address;
}
/** Run the real console command for master 0 (global) or 1 (mplayer). */
static void Query( const char *master ) {
	commandArgs[0] = "globalservers";
	commandArgs[1] = master;
	commandArgs[2] = "68";
	CL_GlobalServers_f();
}
/** Encode listed server n as 10.x.y.z:27960+n%7, never starting with "EOT". */
static void EncodeServer( byte *out, int n ) {
	int port = 27960 + n % 7;
	out[0] = '\\'; out[1] = 10; out[2] = ( n >> 16 ) & 255; out[3] = ( n >> 8 ) & 255; out[4] = n & 255;
	out[5] = ( port >> 8 ) & 255; out[6] = port & 255;
}
/** Check that a parsed address matches listed server n. */
static int SameServer( const byte *ip, unsigned short port, int n ) {
	byte expected[7];
	EncodeServer( expected, n );
	return !memcmp( ip, expected + 1, 4 ) && port == (unsigned short)BigShort( (short)( 27960 + n % 7 ) );
}
/** Deliver a payload through an exact-sized heap message so any read past cursize is fatal. */
static void Deliver( netadr_t from, const byte *payload, int length ) {
	msg_t msg;
	byte *data = malloc( length );
	Check( data != NULL, "packet allocation" );
	memcpy( data, payload, length );
	memset( &msg, 0, sizeof( msg ) );
	msg.data = data;
	msg.cursize = msg.maxsize = length;
	CL_ServersResponsePacket( from, &msg );
	free( data );
}
/** Send servers first..first+count-1 in one response, optionally ending with the 1.32 EOT marker. */
static void Respond( netadr_t from, int first, int count, qboolean eot ) {
	byte packet[TEST_MAX_PACKET];
	int length = sizeof( TEST_HEADER ) - 1, i;
	memcpy( packet, TEST_HEADER, length );
	for ( i = 0; i < count; i++, length += 7 ) {
		EncodeServer( packet + length, first + i );
	}
	if ( eot ) {
		memcpy( packet + length, "\\EOT\0\0\0", 7 );
		length += 7;
	} else {
		packet[length++] = '\\';
	}
	Deliver( from, packet, length );
}
/** Send a response with a hand-written tail after the header. */
static void RespondRaw( netadr_t from, const char *tail, int tailLength ) {
	byte packet[TEST_MAX_PACKET];
	int length = sizeof( TEST_HEADER ) - 1;
	memcpy( packet, TEST_HEADER, length );
	memcpy( packet + length, tail, tailLength );
	Deliver( from, packet, length + tailLength );
}

int main( void ) {
	netadr_t master, stranger, loopback;
	int n, sent;

	showpackets = &quiet;
	master = Address( 192, 0, 2, 1, PORT_MASTER );
	stranger = Address( 198, 51, 100, 7, PORT_MASTER );

	// nothing is accepted before any master was queried
	Respond( master, 0, 3, qtrue );
	Check( cls.numglobalservers == 0 && cls.globalServers[0].adr.type == 0, "unsolicited list before query" );

	// the real query goes to the resolved master and arms only that address
	Query( "0" );
	Check( queries == 1 && NET_CompareAdr( queried, master ), "query destination" );
	Check( cls.numglobalservers == -1 && cls.pingUpdateSource == AS_GLOBAL, "query reset" );

	// responses from other hosts, other ports, or loopback leave the pending list untouched
	Respond( stranger, 0, 3, qtrue );
	Respond( Address( 192, 0, 2, 1, PORT_MASTER + 1 ), 0, 3, qtrue );
	memset( &loopback, 0, sizeof( loopback ) );
	loopback.type = NA_LOOPBACK;
	Respond( loopback, 0, 3, qtrue );
	Check( cls.numglobalservers == -1 && cls.numGlobalServerAddresses == 0, "non-master response ignored" );

	// a normal 1.32 listing split over two packets populates the browser
	Respond( master, 0, 2, qfalse );
	Respond( master, 2, 3, qtrue );
	Check( cls.numglobalservers == 5 && cls.numGlobalServerAddresses == 0, "normal listing count" );
	for ( n = 0; n < 5; n++ ) {
		Check( cls.globalServers[n].adr.type == NA_IP && cls.globalServers[n].ping == -1 &&
			SameServer( cls.globalServers[n].adr.ip, cls.globalServers[n].adr.port, n ), "normal listing address" );
	}
	Respond( stranger, 100, 3, qtrue );
	Check( cls.numglobalservers == 5, "stranger appended to normal listing" );

	// packets ending at a separator, a partial EOT, or a truncated entry never read past cursize
	Query( "0" );
	Respond( master, 0, 1, qfalse );
	Check( cls.numglobalservers == 1 && SameServer( cls.globalServers[0].adr.ip, cls.globalServers[0].adr.port, 0 ),
		"separator at packet end" );
	RespondRaw( master, "\\\x0a\0\0\x01\x6d\x38\\E", 9 );
	RespondRaw( master, "\\\x0a\0\0\x02\x6d\x38\\EO", 10 );
	RespondRaw( master, "\\\x0a\0\0\x03\x6d\x38", 7 );
	RespondRaw( master, "\\\x0a\0\0\x04\x6d", 6 );
	RespondRaw( master, "", 0 );
	RespondRaw( master, "\\", 1 );
	Check( cls.numglobalservers == 3, "truncated tails" );

	// more than two lists' worth of servers, misaligned so one packet crosses the overflow list end
	Query( "0" );
	cls.numfavoriteservers = 7;
	for ( n = 0; n < MAX_OTHER_SERVERS; n++ ) {
		cls.favoriteServers[n].adr = Address( 203, 0, 113, n, 27960 );
		cls.favoriteServers[n].ping = 1000 + n;
	}
	Respond( master, 0, MAX_SERVERSPERPACKET - 1, qfalse );
	for ( sent = MAX_SERVERSPERPACKET - 1; sent < 2 * MAX_GLOBAL_SERVERS + 3 * MAX_SERVERSPERPACKET; sent += MAX_SERVERSPERPACKET ) {
		Respond( master, sent, MAX_SERVERSPERPACKET, qtrue );
		Check( cls.numglobalservers <= MAX_GLOBAL_SERVERS && cls.numGlobalServerAddresses <= MAX_GLOBAL_SERVERS,
			"list counts stay bounded" );
	}
	Check( cls.numglobalservers == MAX_GLOBAL_SERVERS && cls.numGlobalServerAddresses == MAX_GLOBAL_SERVERS,
		"both lists saturate exactly" );
	Check( SameServer( cls.globalServers[MAX_GLOBAL_SERVERS - 1].adr.ip, cls.globalServers[MAX_GLOBAL_SERVERS - 1].adr.port,
		MAX_GLOBAL_SERVERS - 1 ), "last main-list entry" );
	Check( SameServer( cls.globalServerAddresses[0].ip, cls.globalServerAddresses[0].port, MAX_GLOBAL_SERVERS ) &&
		SameServer( cls.globalServerAddresses[MAX_GLOBAL_SERVERS - 1].ip, cls.globalServerAddresses[MAX_GLOBAL_SERVERS - 1].port,
		2 * MAX_GLOBAL_SERVERS - 1 ), "overflow-list entries" );
	Check( cls.numfavoriteservers == 7, "favorite count intact" );
	for ( n = 0; n < MAX_OTHER_SERVERS; n++ ) {
		Check( NET_CompareAdr( cls.favoriteServers[n].adr, Address( 203, 0, 113, n, 27960 ) ) &&
			cls.favoriteServers[n].ping == 1000 + n, "favorite entries intact" );
	}

	// the mplayer list stays bounded by its own array
	Query( "1" );
	Check( cls.nummplayerservers == -1 && cls.pingUpdateSource == AS_MPLAYER, "mplayer query reset" );
	Respond( stranger, 0, 3, qtrue );
	Check( cls.nummplayerservers == -1, "stranger mplayer list ignored" );
	Respond( master, 0, MAX_SERVERSPERPACKET, qtrue );
	Check( cls.nummplayerservers == MAX_OTHER_SERVERS && cls.numGlobalServerAddresses == MAX_GLOBAL_SERVERS &&
		cls.numfavoriteservers == 7, "mplayer list bounded" );

	// a later query to a new master address retires the old one
	resolvedIp[3] = 2;
	Query( "0" );
	Respond( master, 0, 3, qtrue );
	Check( cls.numglobalservers == -1, "previous master ignored after requery" );
	Respond( Address( 192, 0, 2, 2, PORT_MASTER ), 0, 3, qtrue );
	Check( cls.numglobalservers == 3 && cls.numGlobalServerAddresses == 0, "new master accepted" );

	// a failed master lookup accepts no list at all
	resolveOk = qfalse;
	Query( "0" );
	Respond( Address( 192, 0, 2, 2, PORT_MASTER ), 0, 3, qtrue );
	Check( cls.numglobalservers == -1, "list accepted after failed lookup" );

	puts( "Master server list regressions passed (issue #255)" );
	return 0;
}
