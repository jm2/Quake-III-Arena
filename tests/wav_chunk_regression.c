/* Issue #347: the real WAV chunk walker bounds every chunk length by the bytes left in the file. */
#include "../code/client/snd_mem.c"
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define WAV_BYTES	256

typedef struct {
	unsigned char	bytes[WAV_BYTES];
	int				length;
} wavFixture_t;

typedef struct {
	const char	*message;	// GetWavinfo's diagnostic, "" when it accepts the file
	int			format, channels, rate, width, samples, dataofs;
	qboolean	loads;		// S_LoadSound's result
} wavExpect_t;

/* Chunk lengths that end past the file: the issue's INT_MAX, an even length that needs no pad
   arithmetic but still points about 2 GB past the buffer, and one byte past the buffer end. */
#define OVER_LENGTHS	3
static const char *overNames[OVER_LENGTHS] = { "0x7fffffff", "0x7ffffffe", "one byte past the end" };

static const short sampleValues[] = { 0x1234, -2, 0x7fff, -0x8000, 0x00ff, -0x0100, 1, 0 };

dma_t dma;

static char lastMessage[256];
static const wavFixture_t *servedWav;

/** Fail with the fixture that produced the wrong result. */
static void Check( int ok, const char *fixture, const char *message ) {
	if ( !ok ) {
		fprintf( stderr, "WAV chunk regression failed: %s: %s\n", fixture, message );
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
void QDECL Com_DPrintf( const char *format, ... ) { (void)format; }
void Com_Memset( void *dest, const int val, const size_t count ) { memset( dest, val, count ); }
int Com_Milliseconds( void ) { return 0; }
/** One megabyte of sound buffers is far more than these fixtures need. */
cvar_t *Cvar_Get( const char *name, const char *value, int flags ) {
	static cvar_t soundMegs;

	(void)name; (void)value; (void)flags;
	soundMegs.integer = 1;
	return &soundMegs;
}
void S_FreeOldestSound( void ) { Check( 0, "S_FreeOldestSound", "the sound buffer pool ran out" ); }
void S_AdpcmEncodeSound( sfx_t *sfx, short *samples ) { (void)sfx; (void)samples; Check( 0, sfx->soundName, "unexpected ADPCM encode" ); }
void *Hunk_AllocateTempMemory( int size ) {
	Check( size >= 0, "Hunk_AllocateTempMemory", "negative temp allocation" );
	return malloc( size ? size : 1 );
}
void Hunk_FreeTempMemory( void *buf ) { free( buf ); }
/** Serve the current fixture as an exact-size heap copy: AddressSanitizer reports a read of even one byte past it. */
int FS_ReadFile( const char *qpath, void **buffer ) {
	void *copy = malloc( servedWav->length );

	(void)qpath;
	memcpy( copy, servedWav->bytes, servedWav->length );
	*buffer = copy;
	return servedWav->length;
}
void FS_FreeFile( void *buffer ) { free( buffer ); }

static void Put( wavFixture_t *wav, const void *data, int length ) {
	Check( wav->length + length <= WAV_BYTES, "fixture", "fixture too large" );
	memcpy( wav->bytes + wav->length, data, length );
	wav->length += length;
}
static void PutLong( wavFixture_t *wav, uint32_t value ) {
	unsigned char little[4];

	little[0] = value & 255; little[1] = ( value >> 8 ) & 255; little[2] = ( value >> 16 ) & 255; little[3] = value >> 24;
	Put( wav, little, 4 );
}
static void PutShort( wavFixture_t *wav, int value ) {
	unsigned char little[2];

	little[0] = value & 255; little[1] = ( value >> 8 ) & 255;
	Put( wav, little, 2 );
}
/** A chunk header; returns its offset so the length can be patched once the file is complete. */
static int PutHeader( wavFixture_t *wav, const char *id, uint32_t length ) {
	int offset = wav->length;

	Put( wav, id, 4 );
	PutLong( wav, length );
	return offset;
}
static void PatchLength( wavFixture_t *wav, int offset, uint32_t length ) {
	int end = wav->length;

	wav->length = offset + 4;
	PutLong( wav, length );
	wav->length = end;
}
/** The RIFF header, with its length patched by Finish. */
static void StartWav( wavFixture_t *wav ) {
	wav->length = 0;
	PutHeader( wav, "RIFF", 0 );
	Put( wav, "WAVE", 4 );
}
static void Finish( wavFixture_t *wav ) { PatchLength( wav, 0, (uint32_t)( wav->length - 8 ) ); }
/** A 16-bit mono 22050 Hz PCM fmt chunk. */
static void PutFmt( wavFixture_t *wav ) {
	PutHeader( wav, "fmt ", 16 );
	PutShort( wav, 1 );
	PutShort( wav, 1 );
	PutLong( wav, 22050 );
	PutLong( wav, 44100 );
	PutShort( wav, 2 );
	PutShort( wav, 16 );
}
static void PutSamples( wavFixture_t *wav, int count ) {
	int i;

	for ( i = 0; i < count; i++ ) {
		PutShort( wav, sampleValues[i % ( sizeof(sampleValues) / sizeof(sampleValues[0]) )] );
	}
}
/** One of the over-long lengths for a chunk whose header is at 'offset' and which runs to the end of the file. */
static uint32_t OverLength( const wavFixture_t *wav, int offset, int kind ) {
	if ( kind == 0 ) {
		return 0x7fffffffu;
	}
	if ( kind == 1 ) {
		return 0x7ffffffeu;
	}
	return (uint32_t)( wav->length - offset - 8 + 1 );
}

static const wavExpect_t acceptFour = { "", 1, 1, 22050, 2, 4, 44, qtrue };
static const wavExpect_t missingRiff = { "Missing RIFF/WAVE chunks\n", 0, 0, 0, 0, 0, 0, qfalse };
static const wavExpect_t missingFmt = { "Missing fmt chunk\n", 0, 0, 0, 0, 0, 0, qfalse };
static const wavExpect_t missingData = { "Missing data chunk\n", 1, 1, 22050, 2, 0, 0, qtrue };

/**
 * Parse one fixture with the real GetWavinfo from an exact-size heap copy, require the walker to
 * stay inside it and the expected result, then load it through the real S_LoadSound and resampler.
 */
static void Load( const char *fixture, const wavFixture_t *wav, const wavExpect_t *expect ) {
	unsigned char *copy = malloc( wav->length );
	uintptr_t start = (uintptr_t)copy, end = start + wav->length;
	sndBuffer *chunk, *next;
	wavinfo_t info;
	sfx_t sfx;
	int i;

	memcpy( copy, wav->bytes, wav->length );
	lastMessage[0] = '\0';
	info = GetWavinfo( (char *)fixture, copy, wav->length );
	Check( (uintptr_t)last_chunk >= start && (uintptr_t)last_chunk <= end, fixture, "the chunk walker left the file" );
	Check( !data_p || ( (uintptr_t)data_p >= start && (uintptr_t)data_p <= end ), fixture, "the read position left the file" );
	Check( !strcmp( lastMessage, expect->message ), fixture, lastMessage[0] ? lastMessage : "the file was accepted" );
	Check( info.format == expect->format && info.channels == expect->channels && info.rate == expect->rate
		&& info.width == expect->width, fixture, "wrong format fields" );
	Check( info.samples == expect->samples && info.dataofs == expect->dataofs, fixture, "wrong sample count or data offset" );
	Check( (long long)info.dataofs + (long long)info.samples * info.width <= wav->length, fixture, "the samples run past the file" );
	free( copy );

	memset( &sfx, 0, sizeof(sfx) );
	snprintf( sfx.soundName, sizeof(sfx.soundName), "%s", fixture );
	servedWav = wav;
	Check( S_LoadSound( &sfx ) == expect->loads, fixture, "wrong S_LoadSound result" );
	if ( expect->loads ) {
		Check( sfx.soundLength == expect->samples, fixture, "wrong loaded sample count" );
		chunk = sfx.soundData;
		for ( i = 0; i < sfx.soundLength; i++ ) {
			Check( chunk->sndChunk[i] == sampleValues[i % ( sizeof(sampleValues) / sizeof(sampleValues[0]) )], fixture, "wrong loaded sample" );
		}
		for ( chunk = sfx.soundData; chunk; chunk = next ) {
			next = chunk->next;
			SND_free( chunk );
		}
	}
}

/** Well-formed files, including the layouts the retail demo sounds use, parse as before. */
static void TestValid( void ) {
	wavFixture_t wav;
	wavExpect_t expect;

	StartWav( &wav ); PutFmt( &wav ); PutHeader( &wav, "data", 8 ); PutSamples( &wav, 4 ); Finish( &wav );
	Load( "fmt and data", &wav, &acceptFour );

	// three retail sounds end with an odd data length and no pad byte
	StartWav( &wav ); PutFmt( &wav ); PutHeader( &wav, "data", 9 ); PutSamples( &wav, 4 ); Put( &wav, "", 1 ); Finish( &wav );
	Load( "odd data at the end of the file", &wav, &acceptFour );

	// five retail sounds carry a LIST chunk after the data
	StartWav( &wav ); PutFmt( &wav ); PutHeader( &wav, "data", 8 ); PutSamples( &wav, 4 );
	PutHeader( &wav, "LIST", 4 ); Put( &wav, "INFO", 4 ); Finish( &wav );
	Load( "LIST after data", &wav, &acceptFour );

	expect = acceptFour;
	expect.dataofs = 44 + 14;
	StartWav( &wav ); PutFmt( &wav ); PutHeader( &wav, "LIST", 5 ); Put( &wav, "INFO", 5 ); Put( &wav, "", 1 );
	PutHeader( &wav, "data", 8 ); PutSamples( &wav, 4 ); Finish( &wav );
	Load( "odd LIST with its pad byte before data", &wav, &expect );

	expect.dataofs = 44 + 12;
	StartWav( &wav ); PutHeader( &wav, "LIST", 4 ); Put( &wav, "INFO", 4 ); PutFmt( &wav );
	PutHeader( &wav, "data", 8 ); PutSamples( &wav, 4 ); Finish( &wav );
	Load( "LIST before fmt", &wav, &expect );
}

/** The RIFF length is not used past its sign, so an over-long one (a truncated or streamed file) still parses. */
static void TestRiff( void ) {
	char fixture[96];
	wavFixture_t wav;
	int kind;

	for ( kind = 0; kind < OVER_LENGTHS; kind++ ) {
		StartWav( &wav ); PutFmt( &wav ); PutHeader( &wav, "data", 8 ); PutSamples( &wav, 4 );
		PatchLength( &wav, 0, OverLength( &wav, 0, kind ) );
		snprintf( fixture, sizeof(fixture), "RIFF length %s", overNames[kind] );
		Load( fixture, &wav, &acceptFour );
	}

	// files too short to hold the WAVE tag
	wav.length = 0;
	PutHeader( &wav, "RIFF", 0 );
	Load( "RIFF header only", &wav, &missingRiff );
	Put( &wav, "WAV", 3 );
	PatchLength( &wav, 0, 4 );
	Load( "RIFF with three bytes of WAVE", &wav, &missingRiff );
}

/** An over-long LIST before fmt swallows the rest of the file: the fmt chunk is missing. */
static void TestListBeforeFmt( void ) {
	char fixture[96];
	wavFixture_t wav;
	int kind, list;

	for ( kind = 0; kind < OVER_LENGTHS; kind++ ) {
		StartWav( &wav ); list = PutHeader( &wav, "LIST", 0 ); Put( &wav, "INFO", 4 ); PutFmt( &wav );
		PutHeader( &wav, "data", 8 ); PutSamples( &wav, 4 ); Finish( &wav );
		PatchLength( &wav, list, OverLength( &wav, list, kind ) );
		snprintf( fixture, sizeof(fixture), "LIST length %s before fmt", overNames[kind] );
		Load( fixture, &wav, &missingFmt );
	}
}

/** An over-long LIST between fmt and data: the data chunk is missing. */
static void TestListBeforeData( void ) {
	char fixture[96];
	wavFixture_t wav;
	int kind, list;

	for ( kind = 0; kind < OVER_LENGTHS; kind++ ) {
		StartWav( &wav ); PutFmt( &wav ); list = PutHeader( &wav, "LIST", 0 ); Put( &wav, "INFO", 4 );
		PutHeader( &wav, "data", 8 ); PutSamples( &wav, 4 ); Finish( &wav );
		PatchLength( &wav, list, OverLength( &wav, list, kind ) );
		snprintf( fixture, sizeof(fixture), "LIST length %s before data", overNames[kind] );
		Load( fixture, &wav, &missingData );
	}
}

/** An over-long data chunk ends with the file: only the samples in the file load. */
static void TestData( void ) {
	char fixture[96];
	wavFixture_t wav;
	int kind, data;

	for ( kind = 0; kind < OVER_LENGTHS; kind++ ) {
		StartWav( &wav ); PutFmt( &wav ); data = PutHeader( &wav, "data", 0 ); PutSamples( &wav, 4 ); Put( &wav, "", 1 );
		Finish( &wav );
		PatchLength( &wav, data, OverLength( &wav, data, kind ) );
		snprintf( fixture, sizeof(fixture), "data length %s", overNames[kind] );
		Load( fixture, &wav, &acceptFour );
	}
}

/** Files that end inside the fmt fields or inside a chunk header. */
static void TestTruncated( void ) {
	char fixture[96];
	wavFixture_t wav;
	int kind, fmt;

	for ( kind = 0; kind < OVER_LENGTHS; kind++ ) {
		StartWav( &wav ); PutFmt( &wav ); wav.length -= 6; Finish( &wav );
		fmt = 12;
		PatchLength( &wav, fmt, OverLength( &wav, fmt, kind ) );
		snprintf( fixture, sizeof(fixture), "fmt length %s with 10 format bytes", overNames[kind] );
		Load( fixture, &wav, &missingFmt );
	}

	StartWav( &wav ); PutFmt( &wav ); Put( &wav, "LIST", 4 ); PutShort( &wav, 8 ); Finish( &wav );
	Load( "six bytes of a chunk header after fmt", &wav, &missingData );
}

int main( int argc, char **argv ) {
	const char *mode = argc > 1 ? argv[1] : "all";
	int all = !strcmp( mode, "all" );

	Check( all || !strcmp( mode, "valid" ) || !strcmp( mode, "riff" ) || !strcmp( mode, "list-fmt" ) || !strcmp( mode, "list-data" )
		|| !strcmp( mode, "data" ) || !strcmp( mode, "truncated" ), mode, "unknown mode" );
	dma.speed = 22050;
	SND_setup();
	if ( all || !strcmp( mode, "valid" ) ) { TestValid(); puts( "Well-formed WAV layouts parse and load unchanged" ); }
	if ( all || !strcmp( mode, "riff" ) ) { TestRiff(); puts( "Over-long and short RIFF headers stay inside the file (issue #347)" ); }
	if ( all || !strcmp( mode, "list-fmt" ) ) { TestListBeforeFmt(); puts( "An over-long LIST before fmt is rejected inside the file (issue #347)" ); }
	if ( all || !strcmp( mode, "list-data" ) ) { TestListBeforeData(); puts( "An over-long LIST before data is rejected inside the file (issue #347)" ); }
	if ( all || !strcmp( mode, "data" ) ) { TestData(); puts( "An over-long data chunk loads only the samples in the file (issue #347)" ); }
	if ( all || !strcmp( mode, "truncated" ) ) { TestTruncated(); puts( "Truncated fmt fields and chunk headers are rejected inside the file (issue #347)" ); }
	return 0;
}
