/* Actual message and adaptive-Huffman bodies with physical guard storage. */
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
#define CAPACITY 128

typedef struct {
	byte before[GUARD];
	byte data[CAPACITY];
	byte after[GUARD];
} guarded_t;

cvar_t *cl_shownet;

static int sentPackets;
static int sentLength;
static byte sentData[MAX_MSGLEN * 2];

static void Check( int condition, const char *message ) {
	if ( !condition ) {
		fprintf( stderr, "Message/Huffman regression failed: %s\n", message );
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
	Check( length >= 0 && length <= (int)sizeof(sentData), "captured packet length" );
	sentPackets++;
	sentLength = length;
	Com_Memcpy( sentData, data, length );
}

static void GuardInit( guarded_t *guarded ) {
	memset( guarded, 0xa5, sizeof(*guarded) );
}

static void Guards( const guarded_t *guarded ) {
	int i;
	for ( i = 0; i < GUARD; i++ ) {
		Check( guarded->before[i] == 0xa5, "leading physical guard" );
		Check( guarded->after[i] == 0xa5, "trailing physical guard" );
	}
}

static void OOBExact( int bits ) {
	msg_t msg;
	int bytes = bits >> 3;
	int value = bits == 8 ? 0x5a : bits == 16 ? 0x3412 : 0x78563412;
	byte *base, *data;

	base = malloc( bytes + 1 );
	Check( base != NULL, "allocate exact OOB window" );
	data = base + 1; /* Unaligned, with the logical end against the ASan redzone. */
	memset( data, 0xa5, bytes );
	Com_Memset( &msg, 0, sizeof(msg) );
	msg.data = data;
	msg.maxsize = bytes;
	msg.oob = qtrue;
	MSG_WriteBits( &msg, value, bits );
	Check( !msg.overflowed && msg.cursize == bytes && msg.bit == bits,
		"exact OOB write" );
	Check( data[0] == (value & 255), "little-endian first OOB byte" );
	if ( bytes > 1 ) Check( data[1] == ((value >> 8) & 255), "little-endian second OOB byte" );
	if ( bytes > 2 ) Check( data[2] == ((value >> 16) & 255) && data[3] == ((value >> 24) & 255),
		"little-endian complete OOB long" );
	MSG_BeginReadingOOB( &msg );
	Check( MSG_ReadBits( &msg, bits ) == value && msg.readcount == bytes && msg.bit == bits,
		"exact OOB read" );
	free( base );
}

static void OOBShort( int reading ) {
	static const int widths[] = { 8, 16, 32 };
	int i;
	for ( i = 0; i < 3; i++ ) {
		msg_t msg;
		int bytes = widths[i] >> 3;
		byte *base, *data;
		base = malloc( bytes );
		Check( base != NULL, "allocate short OOB window" );
		data = base + 1;
		memset( data, 0, bytes - 1 );
		Com_Memset( &msg, 0, sizeof(msg) );
		msg.data = data;
		msg.maxsize = bytes;
		msg.oob = qtrue;
		if ( reading ) {
			msg.cursize = bytes - 1;
			Check( MSG_ReadBits( &msg, widths[i] ) == 0 &&
				msg.readcount == msg.cursize + 1,
				"one-byte-short OOB read rejects before access" );
		} else {
			msg.maxsize = bytes - 1;
			MSG_WriteBits( &msg, 0x78563412, widths[i] );
			Check( msg.overflowed && msg.cursize == 0 && msg.bit == 0,
				"one-byte-short OOB write rejects before access" );
		}
		free( base );
	}
}

static void BitBoundary( int reading ) {
	msg_t msg;
	byte *base, *data;
	base = malloc( 2 );
	Check( base != NULL, "allocate exact bit window" );
	data = base + 1;
	Com_Memset( &msg, 0, sizeof(msg) );
	msg.data = data;
	msg.maxsize = 1;
	if ( reading ) {
		data[0] = 0x80;
		msg.cursize = 1;
		msg.bit = 7;
		msg.readcount = 1;
		Check( MSG_ReadBits( &msg, 1 ) == 1 && msg.bit == 8 && msg.readcount == 2,
			"last readable message bit remains compatible" );
		Check( MSG_ReadBits( &msg, 1 ) == 0 && msg.bit == 8 && msg.readcount == 2,
			"read after exact end remains stable" );
	} else {
		MSG_WriteBits( &msg, 0x55, 7 );
		Check( !msg.overflowed && msg.bit == 7 && msg.cursize == 1,
			"one-bit-short message write" );
		MSG_WriteBits( &msg, 1, 1 );
		Check( msg.overflowed && msg.bit == 7 && msg.cursize == 1 && data[0] == 0x55,
			"exact-capacity message write sets overflow" );
	}
	free( base );
}

static void SignedBits( void ) {
	guarded_t guarded;
	msg_t msg;
	GuardInit( &guarded );
	MSG_Init( &msg, guarded.data, CAPACITY );
	MSG_WriteBits( &msg, -7, -5 );
	Check( !msg.overflowed, "signed bit write" );
	MSG_BeginReading( &msg );
	Check( MSG_ReadBits( &msg, -5 ) == -7, "signed bit round trip" );
	Guards( &guarded );
}

static const byte goldenPayload[] = {
	'c','o','n','n','e','c','t',' ', '\\','p','r','o','t','o','c','o','l','\\','6','8',
	'\\','q','p','o','r','t','\\','2','7','9','6','0',0
};

static const byte goldenCompressed[] = {
	0x00,0x21,0xc6,0xec,0xb1,0x6b,0x4c,0x3f,0x2e,0x42,0x00,0x3a,0xc7,
	0x61,0x38,0xdd,0x24,0x14,0x9b,0x05,0x6c,0x07,0xc7,0x37,0x8e,0x87,
	0xa3,0x0c,0x4c,0x1f,0x76,0x83,0xd3,0x28,0x0c,0x1b,0x00
};

static void HuffmanGolden( int dump ) {
	guarded_t guarded;
	msg_t msg;
	byte original[CAPACITY];
	int i;
	const int offset = 12;
	GuardInit( &guarded );
	for ( i = 0; i < offset; i++ ) guarded.data[i] = (byte)(0x80 + i);
	Com_Memcpy( guarded.data + offset, goldenPayload, sizeof(goldenPayload) );
	Com_Memcpy( original, guarded.data, offset + sizeof(goldenPayload) );
	Com_Memset( &msg, 0, sizeof(msg) );
	msg.data = guarded.data;
	msg.maxsize = CAPACITY;
	msg.cursize = offset + sizeof(goldenPayload);
	Huff_Compress( &msg, offset );
	Check( !msg.overflowed && msg.cursize == offset + (int)sizeof(goldenCompressed),
		"valid commercial Huffman compression" );
	Check( !memcmp( guarded.data + offset, goldenCompressed, sizeof(goldenCompressed) ),
		"commercial Huffman protocol golden" );
	if ( dump ) {
		printf( "compressed=%d\n", msg.cursize - offset );
		for ( i = offset; i < msg.cursize; i++ ) printf( "%s0x%02x", i == offset ? "" : ",", guarded.data[i] );
		putchar( '\n' );
	}
	Huff_Decompress( &msg, offset );
	Check( !msg.overflowed && msg.cursize == offset + (int)sizeof(goldenPayload),
		"valid commercial Huffman decompression length" );
	Check( !memcmp( guarded.data, original, msg.cursize ),
		"valid commercial Huffman round trip" );
	Guards( &guarded );
}

static void HuffmanPadding( void ) {
	msg_t msg;
	byte poison[CAPACITY] = { 0x41, 0x41 };
	byte target[CAPACITY] = { 0x41 };
	static const byte expected[] = { 0x00, 0x01, 0x82, 0x00 };

	Com_Memset( &msg, 0, sizeof(msg) );
	msg.data = poison;
	msg.maxsize = CAPACITY;
	msg.cursize = 2;
	Huff_Compress( &msg, 0 );
	Check( !msg.overflowed && msg.cursize == 4 && poison[3] != 0,
		"prepare nonzero Huffman padding scratch" );

	Com_Memset( &msg, 0, sizeof(msg) );
	msg.data = target;
	msg.maxsize = CAPACITY;
	msg.cursize = 1;
	Huff_Compress( &msg, 0 );
	Check( !msg.overflowed && msg.cursize == (int)sizeof(expected),
		"byte-aligned Huffman packet length" );
	Check( !memcmp( target, expected, sizeof(expected) ),
		"byte-aligned Huffman padding is deterministic" );
}

static void OutOfBandDataBoundary( int kind ) {
	static cvar_t silentPackets;
	netadr_t address;
	byte large[MAX_MSGLEN * 2 - 4];
	msg_t msg;
	int i;

	Com_Memset( &address, 0, sizeof(address) );
	address.type = NA_IP;
	Com_Memset( &silentPackets, 0, sizeof(silentPackets) );
	showpackets = &silentPackets;
	sentPackets = 0;
	sentLength = 0;

	if ( kind == 0 ) {
		NET_OutOfBandData( NS_CLIENT, address, (byte *)goldenPayload, sizeof(goldenPayload) );
		Check( sentPackets == 1 && sentLength > 12, "valid compressed OOB data is sent" );
		Com_Memset( &msg, 0, sizeof(msg) );
		msg.data = sentData;
		msg.maxsize = sizeof(sentData);
		msg.cursize = sentLength;
		Huff_Decompress( &msg, 12 );
		Check( !msg.overflowed && msg.cursize == 4 + (int)sizeof(goldenPayload),
			"valid compressed OOB data length round trip" );
		for ( i = 0; i < 4; i++ ) Check( sentData[i] == 0xff, "OOB marker remains compatible" );
		Check( !memcmp( sentData + 4, goldenPayload, sizeof(goldenPayload) ),
			"valid compressed OOB data payload round trip" );
	} else if ( kind == 1 ) {
		NET_OutOfBandData( NS_CLIENT, address, NULL, 1 );
		Check( sentPackets == 0, "null OOB data rejects before access" );
	} else if ( kind == 2 ) {
		NET_OutOfBandData( NS_CLIENT, address, large, -1 );
		Check( sentPackets == 0, "negative OOB data length rejects" );
	} else if ( kind == 3 ) {
		NET_OutOfBandData( NS_CLIENT, address, large, (int)sizeof(large) + 1 );
		Check( sentPackets == 0, "oversized OOB data rejects before copy" );
	} else {
		for ( i = 0; i < (int)sizeof(large); i++ ) large[i] = (byte)(i * 73 + 19);
		NET_OutOfBandData( NS_CLIENT, address, large, sizeof(large) );
		Check( sentPackets == 0, "expanded OOB compression is not sent" );
	}
}

static void CompressBoundary( int kind ) {
	guarded_t guarded;
	msg_t msg;
	byte before[CAPACITY];
	const int offset = 12;
	int i;
	GuardInit( &guarded );
	for ( i = 0; i < CAPACITY; i++ ) guarded.data[i] = (byte)(i * 73 + 19);
	Com_Memcpy( before, guarded.data, CAPACITY );
	Com_Memset( &msg, 0, sizeof(msg) );
	msg.data = guarded.data;
	msg.maxsize = CAPACITY;
	msg.cursize = kind == 0 ? CAPACITY : offset + 4;
	if ( kind == 0 ) {
		Huff_Compress( &msg, offset );
		Check( msg.overflowed && msg.cursize == CAPACITY &&
			!memcmp( before, guarded.data, CAPACITY ),
			"expanded Huffman output rejects transactionally" );
	} else if ( kind == 1 ) {
		Huff_Compress( &msg, -1 );
		Check( msg.overflowed && msg.cursize == offset + 4 &&
			!memcmp( before, guarded.data, CAPACITY ),
			"negative Huffman compression offset rejects" );
	} else {
		Huff_Compress( &msg, msg.cursize + 1 );
		Check( msg.overflowed && msg.cursize == offset + 4 &&
			!memcmp( before, guarded.data, CAPACITY ),
			"past-end Huffman compression offset rejects" );
	}
	Guards( &guarded );
}

static void DecompressBoundary( int kind ) {
	guarded_t guarded;
	msg_t msg;
	byte before[CAPACITY];
	const int offset = 12;
	GuardInit( &guarded );
	Com_Memset( &msg, 0, sizeof(msg) );
	msg.data = guarded.data;
	msg.maxsize = CAPACITY;
	msg.cursize = offset;
	if ( kind == 0 ) {
		Huff_Decompress( &msg, offset );
		Check( !msg.overflowed && msg.cursize == offset, "empty compressed suffix is unchanged" );
	} else if ( kind == 1 ) {
		guarded.data[offset] = 0;
		msg.cursize = offset + 1;
		Com_Memcpy( before, guarded.data, CAPACITY );
		Huff_Decompress( &msg, offset );
		Check( msg.overflowed && msg.cursize == offset && !memcmp( before, guarded.data, CAPACITY ),
			"one-byte Huffman header rejects transactionally" );
	} else if ( kind == 2 ) {
		guarded.data[offset] = 0; guarded.data[offset + 1] = 1;
		msg.cursize = offset + 2;
		Com_Memcpy( before, guarded.data, CAPACITY );
		Huff_Decompress( &msg, offset );
		Check( msg.overflowed && msg.cursize == offset && !memcmp( before, guarded.data, CAPACITY ),
			"missing initial NYT byte rejects transactionally" );
	} else if ( kind == 3 ) {
		guarded.data[offset] = 0; guarded.data[offset + 1] = 4; guarded.data[offset + 2] = 0x41;
		msg.cursize = offset + 3;
		Com_Memcpy( before, guarded.data, CAPACITY );
		Huff_Decompress( &msg, offset );
		Check( msg.overflowed && msg.cursize == offset && !memcmp( before, guarded.data, CAPACITY ),
			"truncated adaptive symbol rejects transactionally" );
	} else if ( kind == 4 ) {
		guarded.data[offset] = 0; guarded.data[offset + 1] = 0;
		msg.cursize = offset + 2;
		Huff_Decompress( &msg, offset );
		Check( !msg.overflowed && msg.cursize == offset,
			"zero-symbol Huffman packet remains valid" );
	} else if ( kind == 5 ) {
		msg.cursize = 1;
		Huff_Decompress( &msg, -1 );
		Check( msg.overflowed && msg.cursize == 0, "negative Huffman offset rejects" );
	} else if ( kind == 6 ) {
		msg.cursize = offset;
		Huff_Decompress( &msg, offset + 1 );
		Check( msg.overflowed && msg.cursize == 0, "past-end Huffman offset rejects" );
	} else {
		msg.maxsize = offset;
		msg.cursize = offset + 1;
		Huff_Decompress( &msg, offset );
		Check( msg.overflowed && msg.cursize == 0, "oversized physical message state rejects" );
	}
	Guards( &guarded );
}

int main( int argc, char **argv ) {
	int kind;
	Check( argc == 2, "one isolated case" );
	kind = atoi( argv[1] );
	if ( kind == 0 ) OOBExact( 8 );
	else if ( kind == 1 ) OOBExact( 16 );
	else if ( kind == 2 ) OOBExact( 32 );
	else if ( kind == 3 ) OOBShort( 0 );
	else if ( kind == 4 ) OOBShort( 1 );
	else if ( kind == 5 ) BitBoundary( 0 );
	else if ( kind == 6 ) BitBoundary( 1 );
	else if ( kind == 7 ) SignedBits();
	else if ( kind == 8 ) HuffmanGolden( 0 );
	else if ( kind <= 16 ) DecompressBoundary( kind - 9 );
	else if ( kind <= 19 ) CompressBoundary( kind - 17 );
	else if ( kind <= 24 ) OutOfBandDataBoundary( kind - 20 );
	else HuffmanPadding();
	return 0;
}
