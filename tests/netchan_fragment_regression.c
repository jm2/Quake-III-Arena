/* Actual netchan fragment transmit/reassembly with exact-size receive buffers. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef Q3_HUFFMAN_SOURCE
#define Q3_HUFFMAN_SOURCE "../code/qcommon/huffman.c"
#endif
#include Q3_HUFFMAN_SOURCE
#ifndef Q3_MESSAGE_SOURCE
#define Q3_MESSAGE_SOURCE "../code/qcommon/msg.c"
#endif
#include Q3_MESSAGE_SOURCE
#ifndef Q3_NETCHAN_SOURCE
#define Q3_NETCHAN_SOURCE "../code/qcommon/net_chan.c"
#endif
#include Q3_NETCHAN_SOURCE

#define GUARD 16

typedef struct {
	byte data[MAX_MSGLEN];
	byte after[GUARD];
} guardedMsg_t;

cvar_t *cl_shownet;

static cvar_t silentPackets;
static cvar_t silentDrops;
static cvar_t testQport;
static byte payload[MAX_MSGLEN];
static byte lastPacket[MAX_MSGLEN_BUF];

static void Check( int condition, const char *message ) {
	if ( !condition ) {
		fprintf( stderr, "Netchan fragment regression failed: %s\n", message );
		exit( 1 );
	}
}

void QDECL Com_Error( int level, const char *format, ... ) {
	(void)level;
	(void)format;
	Check( 0, "unexpected engine error" );
}

void QDECL Com_Printf( const char *format, ... ) {
	(void)format;
}

void Com_Memset( void *dest, const int val, const size_t count ) {
	memset( dest, val, count );
}

void Com_Memcpy( void *dest, const void *src, const size_t count ) {
	memcpy( dest, src, count );
}

void Sys_SendPacket( int length, const void *data, netadr_t to ) {
	(void)length;
	(void)data;
	(void)to;
	Check( 0, "netchan test traffic stays on the loopback channel" );
}

static void ChannelsInit( netchan_t *sender, netchan_t *receiver, netsrc_t senderSock ) {
	netadr_t address;

	Com_Memset( &silentPackets, 0, sizeof(silentPackets) );
	Com_Memset( &silentDrops, 0, sizeof(silentDrops) );
	Com_Memset( &testQport, 0, sizeof(testQport) );
	testQport.integer = 27960;
	showpackets = &silentPackets;
	showdrop = &silentDrops;
	qport = &testQport;

	Com_Memset( &address, 0, sizeof(address) );
	address.type = NA_LOOPBACK;
	Netchan_Setup( senderSock, sender, address, testQport.integer );
	Netchan_Setup( senderSock ^ 1, receiver, address, testQport.integer );
}

/*
Transmits one message of the given length through the real fragmenting
Netchan_Transmit, then feeds every loopback packet through NET_GetLoopPacket
and Netchan_Process on the caller's receive message, the way Com_EventLoop
does.  Returns qtrue if the final fragment was accepted.
*/
static qboolean Deliver( netchan_t *sender, netchan_t *receiver, msg_t *msg,
	int length, int expectedPackets ) {
	netadr_t from;
	qboolean accepted = qfalse;
	int sequence = sender->outgoingSequence;
	int incoming = receiver->incomingSequence;
	int packets = 0;
	int i;

	for ( i = 0; i < length; i++ ) {
		payload[i] = (byte)( i * 131 + sequence * 7 + 3 );
	}
	Check( length >= FRAGMENT_SIZE, "test messages are fragmented" );
	Netchan_Transmit( sender, length, payload );
	for ( ;; ) {
		Check( NET_GetLoopPacket( receiver->sock, &from, msg ), "fragment reaches the loopback receiver" );
		packets++;
		Check( msg->cursize > 0 && msg->cursize <= msg->maxsize, "raw fragment fits the receive message" );
		Com_Memcpy( lastPacket, msg->data, msg->cursize );
		accepted = Netchan_Process( receiver, msg );
		Check( msg->cursize <= msg->maxsize, "cursize never exceeds maxsize" );
		if ( accepted ) {
			break;
		}
		Check( !memcmp( lastPacket, msg->data, msg->cursize ),
			"unaccepted fragment leaves the receive message untouched" );
		if ( !sender->unsentFragments ) {
			break;
		}
		Netchan_TransmitNextFragment( sender );
	}
	Check( !sender->unsentFragments && sender->outgoingSequence == sequence + 1,
		"sender finished the fragmented message" );
	Check( !NET_GetLoopPacket( receiver->sock, &from, msg ), "no extra fragments" );
	Check( packets == expectedPackets, "fragment count matches FRAGMENT_SIZE framing" );

	if ( accepted ) {
		Check( msg->cursize == length + 4, "reassembled length includes the sequence" );
		Check( msg->data[0] == ( sequence & 255 ) && msg->data[1] == ( ( sequence >> 8 ) & 255 ) &&
			msg->data[2] == ( ( sequence >> 16 ) & 255 ) && msg->data[3] == ( ( sequence >> 24 ) & 255 ),
			"reassembled message keeps the little-endian sequence" );
		Check( !memcmp( msg->data + 4, payload, length ), "reassembled payload round trip" );
		Check( msg->readcount == 4 && msg->bit == 32, "reader positioned past the sequence" );
		Check( receiver->incomingSequence == sequence && receiver->fragmentLength == 0,
			"accepted message is acknowledged" );
	} else {
		Check( receiver->incomingSequence == incoming, "rejected message is not acknowledged" );
	}
	return accepted;
}

/* Full FRAGMENT_SIZE fragments plus one shorter, possibly empty, final fragment. */
static int PacketsFor( int length ) {
	return length / FRAGMENT_SIZE + 1;
}

/*
A peer controls the total fragment length.  In a receive message that is
exactly MAX_MSGLEN bytes (the retail Com_EventLoop buffer), MAX_MSGLEN - 4
bytes still fit after the sequence; anything larger must be dropped before
the copy, and the channel must keep working afterwards.
*/
static void ExactReceiveBuffer( int length, qboolean expectAccept ) {
	netchan_t sender, receiver;
	msg_t msg;
	byte *data;

	ChannelsInit( &sender, &receiver, NS_SERVER );
	data = malloc( MAX_MSGLEN );	/* Logical end against the ASan redzone. */
	Check( data != NULL, "allocate exact receive buffer" );
	MSG_Init( &msg, data, MAX_MSGLEN );

	Check( Deliver( &sender, &receiver, &msg, length, PacketsFor( length ) ) == expectAccept,
		expectAccept ? "largest fitting message reassembles" : "oversized reassembly is rejected" );
	if ( !expectAccept ) {
		Check( Deliver( &sender, &receiver, &msg, MAX_MSGLEN - 4, PacketsFor( MAX_MSGLEN - 4 ) ),
			"channel recovers after a rejected reassembly" );
	}
	free( data );
}

static void GuardedReceiveBuffer( int length ) {
	netchan_t sender, receiver;
	guardedMsg_t *guarded;
	msg_t msg;
	int i;

	ChannelsInit( &sender, &receiver, NS_SERVER );
	guarded = malloc( sizeof(*guarded) );
	Check( guarded != NULL, "allocate guarded receive buffer" );
	memset( guarded->after, 0xa5, sizeof(guarded->after) );
	MSG_Init( &msg, guarded->data, sizeof(guarded->data) );

	Check( !Deliver( &sender, &receiver, &msg, length, PacketsFor( length ) ),
		"oversized reassembly is rejected in a guarded buffer" );
	for ( i = 0; i < GUARD; i++ ) {
		Check( guarded->after[i] == 0xa5, "no byte written past the receive buffer" );
	}
	free( guarded );
}

/*
A legitimate maximum-size message (MAX_MSGLEN bytes, the largest a 1.32c
sender's Netchan_Transmit accepts) must still reassemble in the
MAX_MSGLEN_BUF receive buffer that Com_EventLoop now provides.
*/
static void EndToEnd( netsrc_t senderSock, int length ) {
	netchan_t sender, receiver;
	msg_t msg;
	byte *data;

	ChannelsInit( &sender, &receiver, senderSock );
	data = malloc( MAX_MSGLEN_BUF );
	Check( data != NULL, "allocate netchan receive buffer" );
	MSG_Init( &msg, data, MAX_MSGLEN_BUF );

	Check( Deliver( &sender, &receiver, &msg, length, PacketsFor( length ) ),
		"legitimate fragmented message reassembles" );
	/* A following message on the same channel still works. */
	Check( Deliver( &sender, &receiver, &msg, FRAGMENT_SIZE + 1, 2 ),
		"next fragmented message reassembles" );
	free( data );
}

int main( int argc, char **argv ) {
	int kind;
	Check( argc == 2, "one isolated case" );
	kind = atoi( argv[1] );
	if ( kind == 0 ) ExactReceiveBuffer( MAX_MSGLEN - 4, qtrue );
	else if ( kind == 1 ) ExactReceiveBuffer( MAX_MSGLEN - 3, qfalse );
	else if ( kind == 2 ) ExactReceiveBuffer( MAX_MSGLEN, qfalse );
	else if ( kind == 3 ) GuardedReceiveBuffer( MAX_MSGLEN - 3 );
	else if ( kind == 4 ) GuardedReceiveBuffer( MAX_MSGLEN );
	else if ( kind == 5 ) EndToEnd( NS_SERVER, MAX_MSGLEN );
	else if ( kind == 6 ) EndToEnd( NS_CLIENT, MAX_MSGLEN );
	else EndToEnd( NS_CLIENT, 12 * FRAGMENT_SIZE );
	return 0;
}
