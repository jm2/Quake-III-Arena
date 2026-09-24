/* Issues #277 and #265: the classic Mac Open Transport network code.
 *
 * #277: Sys_StringToAdr handed any host name to OTInitDNSAddress, which copies
 * it into the 256-byte DNSAddress.fName on the stack with no bound, and
 * NET_StringToAdr passes names of up to 1023 characters.  Names of 256
 * characters or more must now fail with a message and never reach OT; names
 * of up to 255 must resolve as before.
 *
 * #265: Sys_GetPacket ignored OTRcvUData's T_MORE flag, so a datagram larger
 * than its MAX_MSGLEN buffer came out as a truncated packet followed by a
 * second "packet" holding the rest, with no source address.  Such a datagram
 * must be read to its end and dropped whole with the "Oversize packet from"
 * message the Unix and Win32 backends print, and, as there, so must one that
 * exactly fills the buffer.  Other packets must come through byte for byte.
 * Com_EventLoop's own oversize-packet branch must free its event, as every
 * other branch does.
 *
 * The runner extracts the real Sys_StringToAdr and Sys_GetPacket (mac_net.c)
 * and Com_EventLoop (common.c) verbatim; tests/mac_ot_fake.h stands in for
 * Open Transport and the functions they call are replaced here.  Each case
 * runs in its own process: on master the long names overflow dnsAddr, which
 * AddressSanitizer stops at. */
#include "../code/game/q_shared.h"
#include "../code/qcommon/qcommon.h"
#include "mac_ot_fake.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

static const UInt8 serverIP[4] = { 192, 246, 40, 70 };
static const UInt8 serverPort[2] = { 0x6d, 0x38 };	/* 27960 */
static const UInt8 clientIP[4] = { 10, 0, 0, 2 };
static const UInt8 clientPort[2] = { 0x6d, 0x39 };	/* 27961 */

static const char *currentCase = "setup";
static char printed[16384];
static int handleOTErrors;

static void Check( int ok, const char *what ) {
	if ( !ok ) {
		fprintf( stderr, "Mac net regression failed (%s): %s\nprinted: %s\n", currentCase, what, printed );
		exit( 1 );
	}
}

static void Unexpected( const char *name ) {
	fprintf( stderr, "Mac net regression reached %s (%s)\n", name, currentCase );
	exit( 1 );
}

/* How many times text was printed since the case began. */
static int Printed( const char *text ) {
	const char *p;
	int count = 0;

	for ( p = strstr( printed, text ) ; p ; p = strstr( p + 1, text ) ) {
		count++;
	}
	return count;
}

void QDECL Com_Printf( const char *fmt, ... ) {
	size_t length = strlen( printed );
	va_list ap;

	va_start( ap, fmt );
	vsnprintf( printed + length, sizeof( printed ) - length, fmt, ap );
	va_end( ap );
}

void QDECL Com_Error( int level, const char *error, ... ) {
	(void)level;
	Unexpected( error );
}

const char *NET_AdrToString( netadr_t a ) {
	static char s[64];

	snprintf( s, sizeof( s ), "%i.%i.%i.%i:%i", a.ip[0], a.ip[1], a.ip[2], a.ip[3],
		( ((byte *)&a.port)[0] << 8 ) | ((byte *)&a.port)[1] );
	return s;
}

/* mac_net.c */
static EndpointRef endpoint = FAKE_OT_ENDPOINT;
static EndpointRef resolverEndpoint = FAKE_OT_RESOLVER;

void HandleOTError( EndpointRef ep, int err, const char *func ) {
	(void)ep; (void)err; (void)func;
	handleOTErrors++;
}

#include "mac_net_extracted.c"

/* ---------- #277: Sys_StringToAdr ---------- */

/* A name of the given length must resolve to the fake's address, reaching OT
 * intact, or (when it does not fit DNSAddress.fName) fail before OT sees it. */
static void CheckHostName( const char *name, qboolean fits ) {
	netadr_t a;

	memset( &a, 0, sizeof( a ) );
	printed[0] = 0;
	fakeOTInitDNSCalls = fakeOTResolveCalls = 0;
	memset( fakeOTResolvedName, 0, sizeof( fakeOTResolvedName ) );
	memcpy( &fakeOTResolvedHost, serverIP, 4 );

	if ( fits ) {
		Check( Sys_StringToAdr( name, &a ), "the name resolves" );
		Check( fakeOTInitDNSCalls == 1 && fakeOTResolveCalls == 1, "OT resolves the name once" );
		Check( !strcmp( fakeOTResolvedName, name ), "OT is asked for the whole name" );
		Check( a.type == NA_IP && !memcmp( a.ip, serverIP, 4 ), "the address is OT's" );
		Check( !printed[0], "nothing is printed" );
	} else {
		Check( !Sys_StringToAdr( name, &a ), "the name is refused" );
		Check( fakeOTInitDNSCalls == 0 && fakeOTResolveCalls == 0, "OT never sees the name" );
		Check( !strncmp( printed, "Sys_StringToAdr: ", 17 ) && Printed( "\n" ) == 1,
			"one Sys_StringToAdr message is printed" );
	}
	Check( handleOTErrors == 0, "no OT error is reported" );
}

static void HostNameOfLength( int length, qboolean fits ) {
	static char name[MAX_STRING_CHARS];
	int i;

	Check( length < (int)sizeof( name ), "the name fits NET_StringToAdr's copy" );
	for ( i = 0 ; i < length ; i++ ) {
		name[i] = ( i % 64 == 63 ) ? '.' : 'a' + i % 26;	/* 63-character labels */
	}
	name[length] = 0;
	CheckHostName( name, fits );
}

/* ---------- #265: Sys_GetPacket ---------- */

static UInt8 datagramData[4][MAX_MSGLEN * 4];

static const UInt8 *Datagram( int slot, int length ) {
	int i;

	Check( length <= (int)sizeof( datagramData[slot] ), "the datagram fits the fixture" );
	for ( i = 0 ; i < length ; i++ ) {
		datagramData[slot][i] = (UInt8)( i * 31 + slot * 7 + ( i >> 8 ) );
	}
	return datagramData[slot];
}

static void ResetEndpoint( void ) {
	fakeOTQueued = fakeOTNext = fakeOTOffset = fakeOTRcvCalls = 0;
	printed[0] = 0;
}

/* Receive into a MAX_MSGLEN buffer as mac_main.c's Sys_GetEvent does. */
static qboolean GetPacket( netadr_t *from, msg_t *msg ) {
	static byte buffer[MAX_MSGLEN];

	memset( from, 0, sizeof( *from ) );
	memset( msg, 0, sizeof( *msg ) );
	msg->data = buffer;
	msg->maxsize = sizeof( buffer );
	return Sys_GetPacket( from, msg );
}

/* The next packet must be this datagram, whole and from this address. */
static void ExpectPacket( const UInt8 *data, int length, const UInt8 ip[4], const UInt8 port[2] ) {
	netadr_t from;
	msg_t msg;

	Check( GetPacket( &from, &msg ), "the datagram is received" );
	Check( msg.cursize == length && !memcmp( msg.data, data, length ), "the packet is the datagram byte for byte" );
	Check( from.type == NA_IP && !memcmp( from.ip, ip, 4 ) && !memcmp( &from.port, port, 2 ),
		"the packet has the datagram's source address" );
}

/* The next datagram must be read to its end and dropped with one message. */
static void ExpectOversize( int queued ) {
	netadr_t from;
	msg_t msg;

	Check( !GetPacket( &from, &msg ), "an oversize datagram is dropped" );
	Check( fakeOTNext == queued + 1 && fakeOTOffset == 0, "the whole datagram is read from the endpoint" );
	Check( Printed( "Oversize packet from 10.0.0.2:27961\n" ) == 1 && Printed( "\n" ) == 1,
		"one Oversize packet message names the sender" );
}

static void ExpectNoPacket( void ) {
	netadr_t from;
	msg_t msg;

	Check( !GetPacket( &from, &msg ), "an empty endpoint gives no packet" );
}

static void NormalPackets( void ) {
	static const int lengths[] = { 1, 1400, MAX_MSGLEN - 1 };
	int i;

	ResetEndpoint();
	for ( i = 0 ; i < 3 ; i++ ) {
		FakeOT_QueueDatagram( Datagram( i, lengths[i] ), lengths[i],
			i & 1 ? clientIP : serverIP, i & 1 ? clientPort : serverPort );
	}
	for ( i = 0 ; i < 3 ; i++ ) {
		ExpectPacket( datagramData[i], lengths[i], i & 1 ? clientIP : serverIP, i & 1 ? clientPort : serverPort );
	}
	ExpectNoPacket();
	Check( fakeOTRcvCalls == 4, "each packet takes one OTRcvUData call" );
	Check( !printed[0] && handleOTErrors == 0, "nothing is printed" );
}

/* An oversize datagram is dropped and the packet after it arrives intact. */
static void OversizeThenNormal( int length ) {
	const UInt8 *next = Datagram( 1, 1400 );

	ResetEndpoint();
	FakeOT_QueueDatagram( Datagram( 0, length ), length, clientIP, clientPort );
	FakeOT_QueueDatagram( next, 1400, serverIP, serverPort );
	ExpectOversize( 0 );
	ExpectPacket( next, 1400, serverIP, serverPort );
	ExpectNoPacket();
	Check( handleOTErrors == 0, "no OT error is reported" );
}

/* ---------- Com_EventLoop's oversize branch ---------- */

static cvar_t dropsim, svRunning;
cvar_t *com_dropsim = &dropsim;
cvar_t *com_sv_running = &svRunning;

static sysEvent_t events[4];
static int eventCount, eventNext, outstanding, delivered;

static byte PayloadByte( int i, int payload ) {
	return (byte)( i * 13 + payload );
}

/* The queue lets go of each block as it hands the event out, so a block
 * Com_EventLoop fails to free is a leak LeakSanitizer reports. */
sysEvent_t Com_GetEvent( void ) {
	sysEvent_t ev;

	if ( eventNext < eventCount ) {
		ev = events[eventNext];
		events[eventNext++].evPtr = NULL;
		return ev;
	}
	memset( &ev, 0, sizeof( ev ) );
	ev.evType = SE_NONE;
	return ev;
}

/* Queue a packet event as mac_main.c does: a netadr_t, then the payload. */
static void QueuePacketEvent( int payload ) {
	sysEvent_t *ev = &events[eventCount++];
	netadr_t *block = malloc( sizeof( netadr_t ) + payload );
	byte *data = (byte *)( block + 1 );
	int i;

	Check( block != NULL, "the event block is allocated" );
	memset( block, 0, sizeof( *block ) );
	block->type = NA_IP;
	memcpy( block->ip, clientIP, 4 );
	memcpy( &block->port, clientPort, 2 );
	for ( i = 0 ; i < payload ; i++ ) {
		data[i] = PayloadByte( i, payload );
	}
	memset( ev, 0, sizeof( *ev ) );
	ev->evType = SE_PACKET;
	ev->evPtrLength = sizeof( netadr_t ) + payload;
	ev->evPtr = block;
	outstanding++;
}

/* AddressSanitizer reports a block freed twice or never allocated. */
void Z_Free( void *ptr ) {
	free( ptr );
	outstanding--;
}

/* The client must get the event Com_GetEvent last handed out, intact. */
void CL_PacketEvent( netadr_t from, msg_t *msg ) {
	int length = events[eventNext - 1].evPtrLength - (int)sizeof( netadr_t );
	int i;

	Check( from.type == NA_IP && !memcmp( from.ip, clientIP, 4 ), "the packet has its sender" );
	Check( msg->cursize == length, "the packet has its length" );
	for ( i = 0 ; i < length ; i++ ) {
		Check( msg->data[i] == PayloadByte( i, length ), "the packet is delivered byte for byte" );
	}
	delivered++;
}

void Com_RunAndTimeServerPacket( netadr_t *evFrom, msg_t *buf ) { Unexpected( __func__ ); }
qboolean NET_GetLoopPacket( netsrc_t sock, netadr_t *net_from, msg_t *net_message ) { return qfalse; }
void CL_KeyEvent( int key, qboolean down, unsigned time ) { Unexpected( __func__ ); }
void CL_CharEvent( int key ) { Unexpected( __func__ ); }
void CL_MouseEvent( int dx, int dy, int time ) { Unexpected( __func__ ); }
void CL_JoystickEvent( int axis, int value, int time ) { Unexpected( __func__ ); }
void Cbuf_AddText( const char *text ) { Unexpected( __func__ ); }
float Q_random( int *seed ) { Unexpected( __func__ ); return 0; }
void MSG_Init( msg_t *buf, byte *data, int length ) {
	memset( buf, 0, sizeof( *buf ) );
	buf->data = data;
	buf->maxsize = length;
}
void Com_Memcpy( void *dest, const void *src, const size_t count ) { memcpy( dest, src, count ); }

#include "com_event_loop_extracted.c"

/* One packet past the netchan buffer, one that fills it and a small one: the
 * first is dropped with its message, the others are delivered, and every
 * event block is freed once. */
static void EventLoopOversize( void ) {
	QueuePacketEvent( MAX_MSGLEN_BUF + 1 );
	QueuePacketEvent( MAX_MSGLEN_BUF );
	QueuePacketEvent( 1400 );
	printed[0] = 0;

	Com_EventLoop();
	Check( eventNext == 3 && delivered == 2, "the two packets that fit are delivered" );
	Check( Printed( "Com_EventLoop: oversize packet\n" ) == 1, "the oversize packet is reported once" );
	Check( outstanding == 0, "every event block is freed" );
}

int main( int argc, char **argv ) {
	if ( argc != 2 ) {
		fprintf( stderr, "usage: %s case\n", argv[0] );
		return 2;
	}
	currentCase = argv[1];

	if ( !strcmp( currentCase, "host-normal" ) ) {
		CheckHostName( "idnewt", qtrue );
		CheckHostName( "192.246.40.70", qtrue );
	} else if ( !strcmp( currentCase, "host-255" ) ) {
		HostNameOfLength( 255, qtrue );
	} else if ( !strcmp( currentCase, "host-256" ) ) {
		HostNameOfLength( 256, qfalse );
	} else if ( !strcmp( currentCase, "host-1023" ) ) {
		HostNameOfLength( MAX_STRING_CHARS - 1, qfalse );
	} else if ( !strcmp( currentCase, "packet-normal" ) ) {
		NormalPackets();
	} else if ( !strcmp( currentCase, "packet-split" ) ) {
		OversizeThenNormal( MAX_MSGLEN + 1 );		/* two pieces */
		OversizeThenNormal( MAX_MSGLEN * 2 );		/* two full pieces */
		OversizeThenNormal( MAX_MSGLEN * 3 + 5 );	/* four pieces */
	} else if ( !strcmp( currentCase, "packet-full" ) ) {
		OversizeThenNormal( MAX_MSGLEN );
	} else if ( !strcmp( currentCase, "event-oversize" ) ) {
		EventLoopOversize();
	} else {
		fprintf( stderr, "unknown case %s\n", currentCase );
		return 2;
	}
	return 0;
}
