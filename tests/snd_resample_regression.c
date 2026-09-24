/*
 * Issue #344: ResampleSfx and ResampleSfxRaw (code/client/snd_mem.c) widen an
 * 8-bit WAV sample with ( byte - 128 ) << 8, which left-shifts a negative
 * value, undefined in C, for every byte below 128. Sounds come from pk3
 * content, including downloaded pk3s. Retro68 GCC at -O0 simply shifts the
 * bits.
 *
 * This test loads 8-bit mono WAV files holding every byte value through the
 * real S_LoadSound, GetWavinfo and both resamplers: the in-memory chunk path
 * (ResampleSfx) and the ADPCM path (ResampleSfxRaw, with the encoder stubbed to
 * capture its input), at 11025, 22050 and 44100 Hz into the 22050 Hz mixer.
 * The files are long enough to fill more than one sound buffer chunk. Under
 * -fsanitize=shift master aborts on the first byte below 128. Every sample
 * must be byte * 256 - 32768, the value master stores with -fwrapv.
 */
#include "../code/client/snd_mem.c"
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// five passes over all 256 byte values: more than one SND_CHUNK_SIZE chunk
#define WAV_SAMPLES	( 5 * 256 )
#define WAV_BYTES	( 44 + WAV_SAMPLES )

dma_t dma;

static const char		*testCase = "setup";
static unsigned char	wavFile[WAV_BYTES];
static short			encodedSamples[4 * WAV_SAMPLES];
static int				encodedLength;
static qboolean			loading;

static void Fail( const char *message ) {
	fprintf( stderr, "sound resample regression failed: %s: %s\n", testCase, message );
	exit( 1 );
}

/** SND_setup reports the buffer size; the WAV loader prints only when it rejects a file. */
void QDECL Com_Printf( const char *format, ... ) {
	(void)format;
	if ( loading ) {
		Fail( "unexpected WAV loader message" );
	}
}
void QDECL Com_DPrintf( const char *format, ... ) { (void)format; }
void Com_Memset( void *dest, const int val, const size_t count ) { memset( dest, val, count ); }
int Com_Milliseconds( void ) { return 0; }
/** One megabyte of sound buffers is far more than these files need. */
cvar_t *Cvar_Get( const char *name, const char *value, int flags ) {
	static cvar_t soundMegs;

	(void)name; (void)value; (void)flags;
	soundMegs.integer = 1;
	return &soundMegs;
}
void S_FreeOldestSound( void ) { Fail( "the sound buffer pool ran out" ); }
/** Keep the ADPCM encoder's input: the resampled 16-bit samples. */
void S_AdpcmEncodeSound( sfx_t *sfx, short *samples ) {
	if ( sfx->soundLength < 0 || sfx->soundLength > (int)( sizeof( encodedSamples ) / sizeof( encodedSamples[0] ) ) ) {
		Fail( "resampled length out of range" );
	}
	memcpy( encodedSamples, samples, sfx->soundLength * sizeof( short ) );
	encodedLength = sfx->soundLength;
}
void *Hunk_AllocateTempMemory( int size ) {
	if ( size <= 0 ) {
		Fail( "bad temp allocation" );
	}
	return malloc( size );
}
void Hunk_FreeTempMemory( void *buf ) { free( buf ); }
/** Serve the WAV file as an exact-size heap copy: AddressSanitizer reports a read of even one byte past it. */
int FS_ReadFile( const char *qpath, void **buffer ) {
	void *copy = malloc( WAV_BYTES );

	(void)qpath;
	memcpy( copy, wavFile, WAV_BYTES );
	*buffer = copy;
	return WAV_BYTES;
}
void FS_FreeFile( void *buffer ) { free( buffer ); }

static void PutLong( unsigned char *p, uint32_t value ) {
	p[0] = value & 255; p[1] = ( value >> 8 ) & 255; p[2] = ( value >> 16 ) & 255; p[3] = value >> 24;
}
static void PutShort( unsigned char *p, int value ) {
	p[0] = value & 255; p[1] = ( value >> 8 ) & 255;
}
/** An 8-bit mono PCM file whose samples run through every byte value in turn. */
static void BuildWav( int rate ) {
	int i;

	memcpy( wavFile, "RIFF", 4 ); PutLong( wavFile + 4, WAV_BYTES - 8 ); memcpy( wavFile + 8, "WAVE", 4 );
	memcpy( wavFile + 12, "fmt ", 4 ); PutLong( wavFile + 16, 16 );
	PutShort( wavFile + 20, 1 ); PutShort( wavFile + 22, 1 ); PutLong( wavFile + 24, rate ); PutLong( wavFile + 28, rate );
	PutShort( wavFile + 32, 1 ); PutShort( wavFile + 34, 8 );
	memcpy( wavFile + 36, "data", 4 ); PutLong( wavFile + 40, WAV_SAMPLES );
	for ( i = 0 ; i < WAV_SAMPLES ; i++ ) {
		wavFile[44 + i] = (unsigned char)( i * 37 + 11 );	// 37 is odd, so each run of 256 holds every byte
	}
}
/** The 16-bit sample for output sample i: the file's byte at the resampled position, re-centred and scaled. */
static int Expected( int i, int rate ) {
	int srcsample = ( i * (int)( (float)rate / dma.speed * 256 ) ) >> 8;

	return wavFile[44 + srcsample] * 256 - 32768;
}

/** S_LoadSound, which must not print. */
static qboolean LoadSound( sfx_t *sfx ) {
	qboolean loaded;

	loading = qtrue;
	loaded = S_LoadSound( sfx );
	loading = qfalse;
	return loaded;
}

/** Load the file once through each resampler and check every sample. */
static void TestRate( int rate ) {
	char		name[64];
	sndBuffer	*chunk, *next;
	sfx_t		sfx;
	int			i, outcount = (int)( WAV_SAMPLES / ( (float)rate / dma.speed ) );

	BuildWav( rate );

	snprintf( name, sizeof( name ), "%d Hz 8-bit, ResampleSfx", rate );
	testCase = name;
	memset( &sfx, 0, sizeof( sfx ) );
	snprintf( sfx.soundName, sizeof( sfx.soundName ), "%s", "sound/test8.wav" );
	if ( !LoadSound( &sfx ) || sfx.soundCompressionMethod != 0 || sfx.soundLength != outcount ) {
		Fail( "the sound did not load in memory at the resampled length" );
	}
	chunk = sfx.soundData;
	for ( i = 0 ; i < sfx.soundLength ; i++ ) {
		if ( i && !( i & ( SND_CHUNK_SIZE - 1 ) ) ) {
			chunk = chunk->next;
		}
		if ( chunk->sndChunk[i & ( SND_CHUNK_SIZE - 1 )] != Expected( i, rate ) ) {
			fprintf( stderr, "sample %d is %d, expected %d\n", i, chunk->sndChunk[i & ( SND_CHUNK_SIZE - 1 )], Expected( i, rate ) );
			Fail( "wrong sample" );
		}
	}
	for ( chunk = sfx.soundData ; chunk ; chunk = next ) {
		next = chunk->next;
		SND_free( chunk );
	}

	snprintf( name, sizeof( name ), "%d Hz 8-bit, ResampleSfxRaw", rate );
	testCase = name;
	memset( &sfx, 0, sizeof( sfx ) );
	snprintf( sfx.soundName, sizeof( sfx.soundName ), "%s", "sound/test8.wav" );
	sfx.soundCompressed = qtrue;
	encodedLength = -1;
	if ( !LoadSound( &sfx ) || sfx.soundCompressionMethod != 1 || encodedLength != outcount ) {
		Fail( "the sound did not reach the ADPCM encoder at the resampled length" );
	}
	for ( i = 0 ; i < encodedLength ; i++ ) {
		if ( encodedSamples[i] != Expected( i, rate ) ) {
			fprintf( stderr, "sample %d is %d, expected %d\n", i, encodedSamples[i], Expected( i, rate ) );
			Fail( "wrong sample" );
		}
	}
}

int main( void ) {
	dma.speed = 22050;
	SND_setup();
	TestRate( 11025 );
	TestRate( 22050 );
	TestRate( 44100 );
	puts( "8-bit WAV samples resample to byte * 256 - 32768 in both resamplers (issue #344)" );
	return 0;
}
