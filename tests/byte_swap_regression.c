/* Issue #333: LongSwap and the WAV reader's GetLittleLong shifted a byte into the int sign bit. */
#include "../code/client/snd_mem.c"
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

qint64 Long64Swap( qint64 ll );

static char lastMessage[256];

/** Fail with the input that produced a wrong result. */
static void Check( int ok, const char *message, uint32_t bits ) {
	if ( !ok ) {
		fprintf( stderr, "Byte swap regression failed: %s (input 0x%08x)\n", message, (unsigned int)bits );
		exit( 1 );
	}
}
/** Keep the WAV loader's last diagnostic so a rejection can be attributed. */
void QDECL Com_Printf( const char *format, ... ) {
	va_list args;

	va_start( args, format );
	vsnprintf( lastMessage, sizeof(lastMessage), format, args );
	va_end( args );
}
/** Supply the memset used by GetWavinfo. */
void Com_Memset( void *dest, const int val, const size_t count ) { memset( dest, val, count ); }

/** Reference swap: reverse the bytes of the object representation with memcpy. */
static int RefLongSwap( int value ) {
	unsigned char in[4], out[4];
	int i;

	memcpy( in, &value, 4 );
	for ( i = 0; i < 4; i++ ) { out[i] = in[3 - i]; }
	memcpy( &value, out, 4 );
	return value;
}
static short RefShortSwap( short value ) {
	unsigned char in[2], out[2];

	memcpy( in, &value, 2 );
	out[0] = in[1];
	out[1] = in[0];
	memcpy( &value, out, 2 );
	return value;
}
/** The int whose two's complement bits are 'bits', independent of host byte order. */
static int IntFromBits( uint32_t bits ) {
	int value;

	memcpy( &value, &bits, 4 );
	return value;
}
static uint32_t BitsFromFloat( float value ) {
	uint32_t bits;

	memcpy( &bits, &value, 4 );
	return bits;
}
static void StoreLittle32( unsigned char *out, uint32_t bits ) {
	out[0] = bits & 255; out[1] = ( bits >> 8 ) & 255; out[2] = ( bits >> 16 ) & 255; out[3] = bits >> 24;
}

/** Check every 32-bit swapper against the memcpy reference for one bit pattern. */
static void CheckLong( uint32_t bits ) {
	int value = IntFromBits( bits ), swapped, wireValue;
	unsigned char wire[4];
	float in, out;
	uint32_t expectFloat;

	swapped = LongSwap( value );
	Check( swapped == RefLongSwap( value ), "LongSwap equals a memcpy byte reversal", bits );
	Check( LongSwap( swapped ) == value, "LongSwap round trip", bits );

	// little-endian and big-endian file words decode to the same value on any host
	StoreLittle32( wire, bits );
	memcpy( &wireValue, wire, 4 );
	Check( LittleLong( wireValue ) == value, "LittleLong decodes little-endian bytes", bits );
	wire[0] = bits >> 24; wire[1] = ( bits >> 16 ) & 255; wire[2] = ( bits >> 8 ) & 255; wire[3] = bits & 255;
	memcpy( &wireValue, wire, 4 );
	Check( BigLong( wireValue ) == value, "BigLong decodes big-endian bytes", bits );

	// a float result is compared by bits unless it is a NaN, whose payload a host FPU may quiet
	memcpy( &in, &bits, 4 );
	out = FloatSwap( &in );
	expectFloat = (uint32_t)RefLongSwap( value );
	if ( ( expectFloat & 0x7f800000u ) == 0x7f800000u && ( expectFloat & 0x007fffffu ) ) {
		Check( isnan( out ), "FloatSwap keeps NaN patterns NaN", bits );
	} else {
		Check( BitsFromFloat( out ) == expectFloat, "FloatSwap equals a memcpy byte reversal", bits );
	}
}

/** The 0x00/0x7f/0x80/0xff value in every byte, each byte value in each position, the limits and a sweep. */
static void TestLongSwap( void ) {
	static const unsigned char edges[4] = { 0x00, 0x7f, 0x80, 0xff };
	static const int limits[] = { 0, 1, -1, INT_MIN, INT_MAX, INT_MIN + 1, INT_MAX - 1, 0x80, 0xff, 0x8000, 0x800000 };
	uint32_t bits, seed = 0x2545f491u;
	int b0, b1, b2, b3, position, fill, v;
	size_t i;

	for ( b0 = 0; b0 < 4; b0++ ) for ( b1 = 0; b1 < 4; b1++ ) for ( b2 = 0; b2 < 4; b2++ ) for ( b3 = 0; b3 < 4; b3++ ) {
		CheckLong( (uint32_t)edges[b0] | (uint32_t)edges[b1] << 8 | (uint32_t)edges[b2] << 16 | (uint32_t)edges[b3] << 24 );
	}
	for ( position = 0; position < 4; position++ ) for ( fill = 0; fill < 4; fill++ ) for ( v = 0; v < 256; v++ ) {
		bits = edges[fill] * 0x01010101u;
		bits = ( bits & ~( 0xffu << ( position * 8 ) ) ) | (uint32_t)v << ( position * 8 );
		CheckLong( bits );
	}
	for ( i = 0; i < sizeof(limits) / sizeof(limits[0]); i++ ) {
		memcpy( &bits, &limits[i], 4 );
		CheckLong( bits );
	}
	Check( LongSwap( INT_MIN ) == 0x80, "LongSwap(INT_MIN)", 0x80000000u );
	Check( LongSwap( 0x80 ) == INT_MIN, "LongSwap(0x80)", 0x80u );
	Check( LongSwap( INT_MAX ) == IntFromBits( 0xffffff7fu ), "LongSwap(INT_MAX)", 0x7fffffffu );
	Check( LongSwap( -1 ) == -1, "LongSwap(-1)", 0xffffffffu );
	for ( i = 0; i < ( 1u << 20 ); i++ ) {
		seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
		CheckLong( seed );
	}
}

/** Every 16-bit input, through ShortSwap and the host's LittleShort/BigShort mapping. */
static void TestShortSwap( void ) {
	unsigned int v;
	short value, wireValue;
	unsigned char wire[2];

	for ( v = 0; v < 0x10000u; v++ ) {
		uint16_t bits = (uint16_t)v;

		memcpy( &value, &bits, 2 );
		Check( ShortSwap( value ) == RefShortSwap( value ), "ShortSwap equals a memcpy byte reversal", v );
		Check( ShortSwap( ShortSwap( value ) ) == value, "ShortSwap round trip", v );
		wire[0] = v & 255; wire[1] = v >> 8;
		memcpy( &wireValue, wire, 2 );
		Check( LittleShort( wireValue ) == value, "LittleShort decodes little-endian bytes", v );
		wire[0] = v >> 8; wire[1] = v & 255;
		memcpy( &wireValue, wire, 2 );
		Check( BigShort( wireValue ) == value, "BigShort decodes big-endian bytes", v );
	}
}

/** Long64Swap moves bytes only; keep it pinned to the same reference. */
static void TestLong64Swap( void ) {
	static const uint32_t words[] = { 0, 0x7f, 0x80, 0xff, 0x80000000u, 0x7fffffffu, 0xffffffffu, 0x01234567u, 0x80ff7f00u };
	size_t i, j;
	int k;

	for ( i = 0; i < sizeof(words) / sizeof(words[0]); i++ ) for ( j = 0; j < sizeof(words) / sizeof(words[0]); j++ ) {
		unsigned char in[8], expect[8];
		qint64 value, swapped;

		StoreLittle32( in, words[i] );
		StoreLittle32( in + 4, words[j] );
		for ( k = 0; k < 8; k++ ) { expect[k] = in[7 - k]; }
		memcpy( &value, in, 8 );
		swapped = Long64Swap( value );
		Check( !memcmp( &swapped, expect, 8 ), "Long64Swap equals a memcpy byte reversal", words[i] ^ words[j] );
	}
}

/** A PCM WAV with a 16-bit mono fmt chunk and a data chunk of the given length. */
static void BuildWav( unsigned char *wav, uint32_t rate, uint32_t dataLength ) {
	static const unsigned char head[36] = {
		'R','I','F','F', 44,0,0,0, 'W','A','V','E',
		'f','m','t',' ', 16,0,0,0, 1,0, 1,0, 0,0,0,0, 0,0,0,0, 2,0, 16,0
	};

	memcpy( wav, head, sizeof(head) );
	StoreLittle32( wav + 24, rate );
	memcpy( wav + 36, "data", 4 );
	StoreLittle32( wav + 40, dataLength );
	memset( wav + 44, 0, 8 );
}

/** GetWavinfo's GetLittleLong reads fmt rates and chunk lengths with the top byte set. */
static void TestWavLittleLong( void ) {
	static const unsigned char tops[] = { 0x00, 0x7f, 0x80, 0xfe, 0xff };
	unsigned char wav[52];
	wavinfo_t info;
	uint32_t rate;
	size_t i;

	BuildWav( wav, 22050, 8 );
	info = GetWavinfo( "valid.wav", wav, sizeof(wav) );
	Check( info.format == 1 && info.channels == 1 && info.rate == 22050 && info.width == 2 && info.samples == 4 && info.dataofs == 44,
		"a valid PCM WAV is parsed", 22050 );

	for ( i = 0; i < sizeof(tops) / sizeof(tops[0]); i++ ) {
		rate = (uint32_t)tops[i] << 24 | 0x00ac44u;
		BuildWav( wav, rate, 8 );
		info = GetWavinfo( "rate.wav", wav, sizeof(wav) );
		Check( info.rate == IntFromBits( rate ) && info.samples == 4, "the fmt rate decodes every top byte", rate );
	}

	// a length of 2^31 or more reads as negative, and FindNextChunk stops at it
	for ( i = 0; i < sizeof(tops) / sizeof(tops[0]); i++ ) {
		uint32_t length = (uint32_t)tops[i] << 24 | 8u;

		if ( tops[i] < 0x80 ) {
			continue;	// positive lengths are bounded by the file size, not rejected (#347, run_wav_chunk_tests)
		}
		BuildWav( wav, 22050, length );
		lastMessage[0] = '\0';
		info = GetWavinfo( "length.wav", wav, sizeof(wav) );
		Check( info.rate == 22050 && info.samples == 0 && info.dataofs == 0 && !strcmp( lastMessage, "Missing data chunk\n" ),
			"a data chunk length of 2^31 or more is rejected", length );
	}
}

int main( int argc, char **argv ) {
	const char *mode = argc > 1 ? argv[1] : "all";

	if ( !strcmp( mode, "swap" ) || !strcmp( mode, "all" ) ) {
		TestLongSwap();
		TestShortSwap();
		TestLong64Swap();
		puts( "LongSwap, ShortSwap, FloatSwap and Long64Swap match a memcpy byte reversal for every tested input (issue #333)" );
	}
	if ( !strcmp( mode, "wav" ) || !strcmp( mode, "all" ) ) {
		TestWavLittleLong();
		puts( "WAV GetLittleLong decodes and rejects lengths with the top byte set (issue #333)" );
	}
	return 0;
}
