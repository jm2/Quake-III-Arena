/*
 * Actual netchan and message bodies covering protocol-68 compatibility and
 * the explicitly negotiated protocol-69 challenge binding.
 */
#include <limits.h>
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

static int sentPackets;
static int sentLength;
static byte sentData[MAX_MSGLEN * 2];
cvar_t *cl_shownet;

static void Check( int condition, const char *message ) {
	if ( !condition ) {
		fprintf( stderr, "Network challenge regression failed: %s\n", message );
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
	(void)to;
	Check( length >= 0 && length <= (int)sizeof(sentData),
		"captured packet length" );
	sentPackets++;
	sentLength = length;
	Com_Memcpy( sentData, data, length );
}

static void ResetNetwork( void ) {
	static cvar_t silent;
	static cvar_t port;

	Com_Memset( &silent, 0, sizeof(silent) );
	Com_Memset( &port, 0, sizeof(port) );
	port.integer = 27960;
	showpackets = &silent;
	showdrop = &silent;
	qport = &port;
	sentPackets = 0;
	sentLength = 0;
	Com_Memset( sentData, 0, sizeof(sentData) );
}

static netadr_t TestAddress( void ) {
	netadr_t address;

	Com_Memset( &address, 0, sizeof(address) );
	address.type = NA_IP;
	address.ip[0] = 192;
	address.ip[1] = 0;
	address.ip[2] = 2;
	address.ip[3] = 10;
	address.port = BigShort( 27960 );
	return address;
}

static unsigned ReadLittleLong( const byte *data ) {
	return (unsigned)data[0] |
		( (unsigned)data[1] << 8 ) |
		( (unsigned)data[2] << 16 ) |
		( (unsigned)data[3] << 24 );
}

static unsigned ReadLittleShort( const byte *data ) {
	return (unsigned)data[0] | ( (unsigned)data[1] << 8 );
}

static void CapturedMessage( msg_t *msg, int length ) {
	Com_Memset( msg, 0, sizeof(*msg) );
	msg->data = sentData;
	msg->maxsize = sizeof(sentData);
	msg->cursize = length;
}

static void ParseIntegerCases( void ) {
	int value;

	Check( Netchan_ParseInteger( "0", &value ) && value == 0,
		"parse zero" );
	Check( Netchan_ParseInteger( "+2147483647", &value ) && value == INT_MAX,
		"parse positive limit" );
	Check( Netchan_ParseInteger( "-2147483648", &value ) && value == -INT_MAX - 1,
		"parse negative limit" );
	Check( !Netchan_ParseInteger( "", &value ) &&
		!Netchan_ParseInteger( "-", &value ) &&
		!Netchan_ParseInteger( "1x", &value ) &&
		!Netchan_ParseInteger( "2147483648", &value ) &&
		!Netchan_ParseInteger( "-2147483649", &value ),
		"reject malformed and overflowing integers" );
}

static void ResponsePolicyCases( void ) {
	const int expected = 0x12345678;

	Check( Netchan_ChallengeResponseValid( qfalse, qfalse, qtrue,
		expected, expected ), "secure response accepts matching echo" );
	Check( !Netchan_ChallengeResponseValid( qfalse, qtrue, qfalse,
		expected, 0 ), "secure response rejects absent echo" );
	Check( !Netchan_ChallengeResponseValid( qfalse, qtrue, qtrue,
		expected, expected + 1 ), "secure response rejects wrong echo" );
	Check( Netchan_ChallengeResponseValid( qtrue, qtrue, qfalse,
		expected, 0 ), "legacy response accepts exact requested address" );
	Check( !Netchan_ChallengeResponseValid( qtrue, qtrue, qtrue,
		expected, expected + 1 ),
		"legacy response rejects a supplied mismatched echo" );
	Check( !Netchan_ChallengeResponseValid( qtrue, qfalse, qfalse,
		expected, 0 ), "legacy response rejects unbound proxy handoff" );
	Check( Netchan_ChallengeResponseValid( qtrue, qfalse, qtrue,
		expected, expected ), "legacy response permits nonce-bound proxy handoff" );
	Check( Netchan_ConnectResponseValid( qfalse, qtrue, expected, expected ),
		"secure connect accepts matching challenge" );
	Check( !Netchan_ConnectResponseValid( qfalse, qfalse, expected, 0 ) &&
		!Netchan_ConnectResponseValid( qfalse, qtrue, expected, expected + 1 ),
		"secure connect rejects absent and wrong challenges" );
	Check( Netchan_ConnectResponseValid( qtrue, qfalse, expected, 0 ),
		"legacy connect accepts stock bare response" );
	Check( Netchan_ConnectResponseValid( qtrue, qtrue, expected, expected ) &&
		!Netchan_ConnectResponseValid( qtrue, qtrue, expected, expected + 1 ),
		"legacy connect validates a supplied challenge" );
}

static void LegacyClientPacket( void ) {
	static const byte payload[] = { 0x21, 0x43, 0x65 };
	netchan_t sender;
	netchan_t receiver;
	netadr_t address;
	msg_t msg;

	ResetNetwork();
	address = TestAddress();
	Netchan_Setup( NS_CLIENT, &sender, address, 27960, 0x12345678, qtrue );
	Netchan_Transmit( &sender, sizeof(payload), payload );
	Check( sentPackets == 1 && sentLength == 6 + (int)sizeof(payload),
		"legacy client packet retains commercial header length" );
	Check( ReadLittleLong( sentData ) == 1 &&
		ReadLittleShort( sentData + 4 ) == 27960 &&
		!memcmp( sentData + 6, payload, sizeof(payload) ),
		"legacy client packet retains commercial wire bytes" );

	Netchan_Setup( NS_SERVER, &receiver, address, 27960, 0x7fffffff, qtrue );
	CapturedMessage( &msg, sentLength );
	Check( Netchan_Process( &receiver, &msg ) && msg.readcount == 6 &&
		receiver.incomingSequence == 1,
		"legacy server accepts stock packet without checksum" );
}

static void SecureClientPacket( void ) {
	static const byte payload[] = { 0xaa, 0xbb, 0xcc };
	const int challenge = 0x12345678;
	netchan_t sender;
	netchan_t receiver;
	netadr_t address;
	msg_t msg;

	ResetNetwork();
	address = TestAddress();
	Netchan_Setup( NS_CLIENT, &sender, address, 27960, challenge, qfalse );
	sender.outgoingSequence = 2;
	Netchan_Transmit( &sender, sizeof(payload), payload );
	Check( sentPackets == 1 && sentLength == 10 + (int)sizeof(payload),
		"secure client packet includes checksum" );
	Check( ReadLittleLong( sentData ) == 2 &&
		ReadLittleShort( sentData + 4 ) == 27960 &&
		ReadLittleLong( sentData + 6 ) ==
			Netchan_GenerateChecksum( challenge, 2 ) &&
		!memcmp( sentData + 10, payload, sizeof(payload) ),
		"secure client packet header is challenge-bound" );

	Netchan_Setup( NS_SERVER, &receiver, address, 27960, challenge, qfalse );
	receiver.incomingSequence = 1;
	CapturedMessage( &msg, sentLength );
	Check( Netchan_Process( &receiver, &msg ) && msg.readcount == 10 &&
		receiver.incomingSequence == 2,
		"secure server accepts matching checksum" );
}

static void WrongChecksumPacket( void ) {
	static const byte payload[] = { 1, 2, 3 };
	netchan_t sender;
	netchan_t receiver;
	netadr_t address;
	msg_t msg;

	ResetNetwork();
	address = TestAddress();
	Netchan_Setup( NS_CLIENT, &sender, address, 27960, 0x12345678, qfalse );
	sender.outgoingSequence = 2;
	Netchan_Transmit( &sender, sizeof(payload), payload );

	Netchan_Setup( NS_SERVER, &receiver, address, 27960, 0x23456789, qfalse );
	receiver.incomingSequence = 1;
	CapturedMessage( &msg, sentLength );
	Check( !Netchan_Process( &receiver, &msg ) &&
		receiver.incomingSequence == 1 && receiver.fragmentLength == 0,
		"wrong challenge checksum rejects before channel mutation" );
}

static void TruncatedChecksumPacket( void ) {
	netchan_t receiver;
	netadr_t address;
	msg_t msg;

	ResetNetwork();
	address = TestAddress();
	Netchan_Setup( NS_SERVER, &receiver, address, 27960, 0x12345678, qfalse );
	sentData[0] = 2;
	sentData[4] = 0x38;
	sentData[5] = 0x6d;
	CapturedMessage( &msg, 9 );
	Check( !Netchan_Process( &receiver, &msg ) &&
		receiver.incomingSequence == 0,
		"truncated secure checksum rejects" );
}

static void SecureServerPacket( void ) {
	static const byte payload[] = { 0x12, 0x34 };
	const int challenge = -123456789;
	netchan_t sender;
	netchan_t receiver;
	netadr_t address;
	msg_t msg;

	ResetNetwork();
	address = TestAddress();
	Netchan_Setup( NS_SERVER, &sender, address, 27960, challenge, qfalse );
	sender.outgoingSequence = 3;
	Netchan_Transmit( &sender, sizeof(payload), payload );
	Check( sentLength == 8 + (int)sizeof(payload) &&
		ReadLittleLong( sentData ) == 3 &&
		ReadLittleLong( sentData + 4 ) ==
			Netchan_GenerateChecksum( challenge, 3 ) &&
		!memcmp( sentData + 8, payload, sizeof(payload) ),
		"secure server packet has checksum without qport" );

	Netchan_Setup( NS_CLIENT, &receiver, address, 27960, challenge, qfalse );
	receiver.incomingSequence = 2;
	CapturedMessage( &msg, sentLength );
	Check( Netchan_Process( &receiver, &msg ) && msg.readcount == 8,
		"secure client accepts matching server checksum" );
}

static void SecureFragmentPacket( void ) {
	byte payload[FRAGMENT_SIZE];
	const int challenge = 0x10203040;
	netchan_t sender;
	netchan_t receiver;
	netadr_t address;
	msg_t msg;

	ResetNetwork();
	Com_Memset( payload, 0x5a, sizeof(payload) );
	address = TestAddress();
	Netchan_Setup( NS_CLIENT, &sender, address, 27960, challenge, qfalse );
	sender.outgoingSequence = 4;
	Netchan_Transmit( &sender, sizeof(payload), payload );
	Check( sentPackets == 1 && ReadLittleLong( sentData ) == ( 4U | FRAGMENT_BIT ) &&
		ReadLittleLong( sentData + 6 ) ==
			Netchan_GenerateChecksum( challenge, 4 ),
		"secure fragment header is challenge-bound" );

	Netchan_Setup( NS_SERVER, &receiver, address, 27960, challenge, qfalse );
	receiver.incomingSequence = 3;
	CapturedMessage( &msg, sentLength );
	Check( !Netchan_Process( &receiver, &msg ) &&
		receiver.fragmentSequence == 4 &&
		receiver.fragmentLength == FRAGMENT_SIZE,
		"valid secure first fragment is retained" );

	Netchan_TransmitNextFragment( &sender );
	Check( sentPackets == 2 && !sender.unsentFragments &&
		ReadLittleLong( sentData ) == ( 4U | FRAGMENT_BIT ) &&
		ReadLittleLong( sentData + 6 ) ==
			Netchan_GenerateChecksum( challenge, 4 ) &&
		ReadLittleShort( sentData + 10 ) == FRAGMENT_SIZE &&
		ReadLittleShort( sentData + 12 ) == 0,
		"secure final fragment retains sequence and checksum" );
	CapturedMessage( &msg, sentLength );
	Check( Netchan_Process( &receiver, &msg ) &&
		receiver.incomingSequence == 4 && msg.readcount == 4 &&
		msg.cursize == FRAGMENT_SIZE + 4 &&
		!memcmp( msg.data + 4, payload, sizeof(payload) ),
		"secure fragments reconstruct the complete payload" );
}

static void ChecksumArithmetic( void ) {
	unsigned expected;

	expected = (unsigned)INT_MAX ^
		( (unsigned)INT_MAX * (unsigned)INT_MAX );
	Check( Netchan_GenerateChecksum( INT_MAX, INT_MAX ) == expected,
		"checksum multiplication wraps modulo 2^32" );
	Check( Netchan_GenerateChecksum( -1, 1 ) == 0,
		"upstream sequence-one checksum remains interoperable" );
}

int main( int argc, char **argv ) {
	int kind;

	Check( argc == 2, "one isolated case" );
	kind = atoi( argv[1] );
	if ( kind == 0 ) ParseIntegerCases();
	else if ( kind == 1 ) ResponsePolicyCases();
	else if ( kind == 2 ) LegacyClientPacket();
	else if ( kind == 3 ) SecureClientPacket();
	else if ( kind == 4 ) WrongChecksumPacket();
	else if ( kind == 5 ) TruncatedChecksumPacket();
	else if ( kind == 6 ) SecureServerPacket();
	else if ( kind == 7 ) SecureFragmentPacket();
	else if ( kind == 8 ) ChecksumArithmetic();
	else Check( 0, "known case" );
	return 0;
}
