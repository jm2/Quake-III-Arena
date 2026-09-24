/*
 * Issue #370: sound and music files whose sample format, rate or length the
 * sound code cannot handle. Sounds come from pk3 content, including
 * downloaded pk3s. This test includes the real snd_dma.c and links the real
 * snd_mem.c and snd_adpcm.c, and drives them with each case:
 *
 * - S_StartBackgroundTrack divided the data length by width * channels, so
 *   music with fewer than 8 bits or no channels divided by zero. Music the
 *   mixer cannot play (a width or channel count past 2) never advanced the
 *   music buffer, and a looping track reopened itself forever. A rate of 0,
 *   a tiny rate, and an empty track read nothing forever in
 *   S_UpdateBackgroundTrack; so did a track at half the mixer rate once one
 *   sample of room was left. A negative rate read before the stack buffer,
 *   and a rate past INT_MAX / MAX_RAW_SAMPLES overflowed the read size.
 * - S_LoadSound resampled a rate of 0 to an infinite or NaN sample count, and
 *   a negative one to a negative length. A tiny rate stretched a short file
 *   past the sound buffer pool: SND_malloc then looped forever, because
 *   S_FreeOldestSound cannot free the sound being loaded (or, when that sound
 *   is the default sound in slot 0, freed it under the loader). At 1 Hz a
 *   100,000-sample file overflowed the float-to-int sample count.
 * - The resamplers' samplefrac overflowed an int at 2^23 source samples.
 * - S_AdpcmEncodeSound read the first sample of an empty sound.
 *
 * Each mode runs in its own process under a timeout: the unfixed code loops
 * forever in several of them.
 */
#include "../code/client/snd_dma.c"
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define TEST_SOUND_MEGS	1
#define TEST_BUFFERS	( TEST_SOUND_MEGS * 1536 )			// SND_setup's pool
#define POOL_SAMPLES	( TEST_BUFFERS * SND_CHUNK_SIZE )
#define MAX_SERVED		32
#define MUSIC_NAME		"music/test"
#define MUSIC_FILE		"music/test.wav"
#define DEFAULT_SOUND	"sound/feedback/hit.wav"

typedef struct {
	char			name[MAX_QPATH];
	unsigned char	*bytes;
	int				length;
} servedFile_t;

static const char	*testCase = "setup";
static servedFile_t	served[MAX_SERVED];
static char			lastMessage[256];
static char			messageLog[4096];
static int			clockMsec = 1000;
static int			openFile = -1;		// the served file the music handle reads, -1 when closed
static int			openPosition;

static void Check( int ok, const char *message ) {
	if ( !ok ) {
		fprintf( stderr, "sound rate regression failed: %s: %s\n", testCase, message );
		exit( 1 );
	}
}

/** Keep each message so a rejection can be attributed. */
void QDECL Com_Printf( const char *format, ... ) {
	va_list args;

	va_start( args, format );
	vsnprintf( lastMessage, sizeof( lastMessage ), format, args );
	va_end( args );
	if ( strlen( messageLog ) + strlen( lastMessage ) < sizeof( messageLog ) ) {
		strcat( messageLog, lastMessage );
	}
}
void QDECL Com_DPrintf( const char *format, ... ) { (void)format; }
void QDECL Com_Error( int level, const char *format, ... ) {
	(void)level; (void)format;
	Check( 0, "unexpected Com_Error" );
}
void Com_Memset( void *dest, const int val, const size_t count ) { memset( dest, val, count ); }
/** The clock moves on at every read, so a sound used "now" can be freed a few reads later. */
int Com_Milliseconds( void ) { return clockMsec++; }
cvar_t *Cvar_Get( const char *name, const char *value, int flags ) {
	static cvar_t soundMegs;

	(void)name; (void)value; (void)flags;
	soundMegs.integer = TEST_SOUND_MEGS;
	return &soundMegs;
}
void *Hunk_AllocateTempMemory( int size ) {
	Check( size >= 0, "negative temp allocation" );
	return malloc( size );	// exact size: AddressSanitizer reports a read of even the first sample of an empty one
}
void Hunk_FreeTempMemory( void *buf ) { free( buf ); }

static int FindServed( const char *name ) {
	int i;

	for ( i = 0; i < MAX_SERVED; i++ ) {
		if ( served[i].bytes && !strcmp( served[i].name, name ) ) {
			return i;
		}
	}
	return -1;
}
/** Serve a file as an exact-size heap copy: AddressSanitizer reports a read of even one byte past it. */
int FS_ReadFile( const char *qpath, void **buffer ) {
	int i = FindServed( qpath );

	if ( i < 0 ) {
		*buffer = NULL;
		return -1;
	}
	*buffer = malloc( served[i].length );
	memcpy( *buffer, served[i].bytes, served[i].length );
	return served[i].length;
}
void FS_FreeFile( void *buffer ) { free( buffer ); }
/** One music file is open at a time, as handle 1. */
int FS_FOpenFileRead( const char *qpath, fileHandle_t *file, qboolean uniqueFILE ) {
	(void)uniqueFILE;
	Check( openFile < 0, "a music file was opened twice" );
	openFile = FindServed( qpath );
	openPosition = 0;
	*file = openFile < 0 ? 0 : 1;
	return openFile < 0 ? -1 : served[openFile].length;
}
static int ReadOpen( void *buffer, int len, fileHandle_t f ) {
	int left;

	Check( f == 1 && openFile >= 0, "read from a music file that is not open" );
	left = served[openFile].length - openPosition;
	if ( len > left ) {
		len = left;
	}
	memcpy( buffer, served[openFile].bytes + openPosition, len );
	openPosition += len;
	return len;
}
int FS_Read( void *buffer, int len, fileHandle_t f ) { return ReadOpen( buffer, len, f ); }
void FS_FCloseFile( fileHandle_t f ) {
	Check( f == 1 && openFile >= 0, "closed a music file that is not open" );
	openFile = -1;
}
void Sys_BeginStreamedFile( fileHandle_t f, int readahead ) {
	(void)readahead;
	Check( f == 1 && openFile >= 0, "streamed a music file that is not open" );
}
void Sys_EndStreamedFile( fileHandle_t f ) { Check( f == 1 && openFile >= 0, "ended a music stream that is not open" ); }
int Sys_StreamedRead( void *buffer, int size, int count, fileHandle_t f ) { return ReadOpen( buffer, size * count, f ) / size; }

/** A 16-bit sample value for sample index i. */
static short Pattern16( int i ) { return (short)( ( ( i * 37 + 11 ) & 0xffff ) - 0x8000 ); }
/** An 8-bit sample byte for sample index i. */
static unsigned char Pattern8( int i ) { return (unsigned char)( i * 37 + 11 ); }

static void PutLong( unsigned char *p, uint32_t value ) {
	p[0] = value & 255; p[1] = ( value >> 8 ) & 255; p[2] = ( value >> 16 ) & 255; p[3] = value >> 24;
}
static void PutShort( unsigned char *p, uint32_t value ) {
	p[0] = value & 255; p[1] = ( value >> 8 ) & 255;
}
/**
 * Serve a PCM WAV file under 'name': the fmt chunk first, as the music reader requires, then a data chunk of
 * 'values' sample values, 16-bit when 'bits' is 16 and bytes otherwise. 'channels' and 'bits' are stored as
 * the unsigned 16-bit fields.
 */
static void ServeWav( const char *name, int channels, int rate, int bits, int values ) {
	int bytes = bits == 16 ? 2 : 1, dataLength = values * bytes, i, slot = FindServed( name );
	unsigned char *p;

	if ( slot < 0 ) {
		for ( slot = 0; served[slot].bytes; slot++ ) {
			Check( slot + 1 < MAX_SERVED, "too many served files" );
		}
	}
	free( served[slot].bytes );
	snprintf( served[slot].name, sizeof( served[slot].name ), "%s", name );
	served[slot].length = 44 + dataLength;
	served[slot].bytes = p = malloc( served[slot].length );
	memcpy( p, "RIFF", 4 ); PutLong( p + 4, served[slot].length - 8 ); memcpy( p + 8, "WAVE", 4 );
	memcpy( p + 12, "fmt ", 4 ); PutLong( p + 16, 16 );
	PutShort( p + 20, 1 ); PutShort( p + 22, (uint32_t)channels ); PutLong( p + 24, (uint32_t)rate );
	PutLong( p + 28, (uint32_t)rate * (uint32_t)( channels * bytes ) ); PutShort( p + 32, (uint32_t)( channels * bytes ) );
	PutShort( p + 34, (uint32_t)bits );
	memcpy( p + 36, "data", 4 ); PutLong( p + 40, dataLength );
	for ( i = 0; i < values; i++ ) {
		if ( bytes == 2 ) {
			PutShort( p + 44 + i * 2, (uint16_t)Pattern16( i ) );
		} else {
			p[44 + i] = Pattern8( i );
		}
	}
}

static void ClearMessages( void ) { messageLog[0] = '\0'; }
static int Printed( const char *message ) { return strstr( messageLog, message ) != NULL; }

/** The free sound buffers, as S_DisplayFreeMemory reports them. */
static int FreeBuffers( void ) {
	int freeBytes, totalBytes;

	S_DisplayFreeMemory();
	Check( sscanf( lastMessage, "%d bytes free sound buffer memory, %d total used", &freeBytes, &totalBytes ) == 2, "no free memory report" );
	return freeBytes / (int)sizeof( sndBuffer );
}

/** Start the sound system over as a level load does: a fresh buffer pool, with the default sound in slot 0. */
static void StartSounds( int speed, int defaultSamples ) {
	dma.speed = speed;
	s_soundStarted = 1;
	s_numSfx = 0;
	ServeWav( DEFAULT_SOUND, 1, 22050, 16, defaultSamples );
	S_BeginRegistration();
	Check( s_numSfx == 1 && s_knownSfx[0].inMemory && !s_knownSfx[0].defaultSound, "the default sound did not load" );
}

/** The number of buffers in a sound's chain, failing on a chain longer than the pool. */
static int ChainLength( const sfx_t *sfx ) {
	sndBuffer *chunk;
	int n = 0;

	for ( chunk = sfx->soundData; chunk; chunk = chunk->next ) {
		Check( ++n <= TEST_BUFFERS, "a sound buffer chain loops" );
	}
	return n;
}
static short LoadedSample( const sfx_t *sfx, int i ) {
	sndBuffer *chunk = sfx->soundData;
	int n;

	for ( n = i / SND_CHUNK_SIZE; n > 0; n-- ) {
		chunk = chunk->next;
	}
	return chunk->sndChunk[i & ( SND_CHUNK_SIZE - 1 )];
}
/** Require a loaded in-memory sound of 'length' samples whose sample i is source sample ( i * fracstep ) >> 8. */
static void CheckLoaded( const sfx_t *sfx, int length, int fracstep, int bits ) {
	sndBuffer *chunk;
	int i, src, expect;

	Check( sfx->inMemory && !sfx->defaultSound && sfx->soundCompressionMethod == 0, "the sound did not load" );
	Check( sfx->soundLength == length, "wrong loaded length" );
	Check( ChainLength( sfx ) == ( length + SND_CHUNK_SIZE - 1 ) / SND_CHUNK_SIZE, "wrong number of sound buffers" );
	chunk = sfx->soundData;
	for ( i = 0; i < length; i++ ) {
		if ( i && !( i & ( SND_CHUNK_SIZE - 1 ) ) ) {
			chunk = chunk->next;
		}
		src = (int)( ( (long long)i * fracstep ) >> 8 );
		expect = bits == 16 ? Pattern16( src ) : ( Pattern8( src ) - 128 ) * 256;
		Check( chunk->sndChunk[i & ( SND_CHUNK_SIZE - 1 )] == expect, "wrong loaded sample" );
	}
}
/** Register a sound that must be refused: the default sound's handle, no buffers used, and 'message' printed. */
static void Refuse( const char *name, const char *message ) {
	int before = FreeBuffers();
	char expect[128];
	sfx_t *sfx;

	ClearMessages();
	Check( S_RegisterSound( name, qfalse ) == 0, "the sound was accepted" );
	sfx = &s_knownSfx[s_numSfx - 1];
	Check( !strcmp( sfx->soundName, name ) && sfx->defaultSound, "the sound is not the default sound" );
	Check( !sfx->soundData && !sfx->soundLength, "the refused sound kept data" );
	snprintf( expect, sizeof( expect ), "%s %s", name, message );
	Check( Printed( expect ), "the refusal was not reported" );
	Check( FreeBuffers() == before && s_knownSfx[0].inMemory, "the refused sound used or freed sound buffers" );
}

/*
===============================================================================

music

===============================================================================
*/

static void StartMusic( int speed ) {
	static cvar_t musicVolume;

	dma.speed = speed;
	s_soundStarted = 1;
	s_soundMuted = qfalse;
	musicVolume.value = 0.75f;		// S_UpdateBackgroundTrack's volume stays 0.5: an integer volume of 128
	s_musicVolume = &musicVolume;
	S_StopBackgroundTrack();
	s_soundtime = 0;
	s_rawend = 0;
}

/** Music files with widths, channel counts and rates the streamer cannot play are refused before any is read. */
static void TestMusicFormat( void ) {
	static const struct {
		const char	*what;
		int			channels, rate, bits;
	} refused[] = {
		{ "4 bits per sample", 2, 22050, 4 },			// width 0: the sample count divided by zero
		{ "0 bits per sample", 1, 22050, 0 },
		{ "-8 bits per sample", 2, 22050, 0xfff8 },		// width -1
		{ "no channels", 0, 22050, 16 },				// divided by zero
		{ "-1 channels", 0xffff, 22050, 16 },
		{ "24 bits per sample", 2, 22050, 24 },			// S_RawSamples plays nothing: a looping track reopens forever
		{ "3 channels", 3, 22050, 16 },
		{ "a rate of 0", 2, 0, 16 },					// reads nothing forever
		{ "a rate of -1", 2, -1, 16 },
		{ "a rate of -22050", 2, -22050, 16 },			// steps backwards through the read buffer
		{ "a rate of INT_MIN", 2, INT_MIN, 16 },
		{ "a rate of 131072", 2, 131072, 16 },			// the rate times MAX_RAW_SAMPLES overflows
		{ "a rate of INT_MAX", 2, INT_MAX, 16 },
	};
	int i;

	for ( i = 0; i < (int)( sizeof( refused ) / sizeof( refused[0] ) ); i++ ) {
		testCase = refused[i].what;
		StartMusic( 22050 );
		ServeWav( MUSIC_FILE, refused[i].channels, refused[i].rate, refused[i].bits, 64 );
		ClearMessages();
		S_StartBackgroundTrack( MUSIC_NAME, MUSIC_NAME );
		Check( !s_backgroundFile && openFile < 0, "the music track was accepted" );
		Check( Printed( "Unsupported sample format in music file " MUSIC_FILE "\n" ), "the refusal was not reported" );
		S_UpdateBackgroundTrack();
		Check( s_rawend == 0, "a refused track added music" );
	}
}

/** Require the music buffer to hold output samples [from, to) of a 16-bit stereo track played 'repeat' times each. */
static void CheckMusic( int from, int to, int repeat ) {
	int i, frame;

	for ( i = from; i < to; i++ ) {
		frame = i / repeat;
		Check( s_rawsamples[i & ( MAX_RAW_SAMPLES - 1 )].left == Pattern16( frame * 2 ) * 128
			&& s_rawsamples[i & ( MAX_RAW_SAMPLES - 1 )].right == Pattern16( frame * 2 + 1 ) * 128, "wrong music sample" );
	}
}

/** Stock 22 kHz stereo music, looping, and the fastest rate accepted, stream as before. */
static void TestMusicStream( void ) {
	int i;

	testCase = "22050 Hz 16-bit stereo music";
	StartMusic( 22050 );
	ServeWav( MUSIC_FILE, 2, 22050, 16, 20000 * 2 );
	S_StartBackgroundTrack( MUSIC_NAME, MUSIC_NAME );
	Check( s_backgroundFile && s_backgroundInfo.samples == 20000 && s_backgroundSamples == 20000, "the track did not start" );
	S_UpdateBackgroundTrack();
	Check( s_rawend == MAX_RAW_SAMPLES, "the music buffer was not filled" );
	CheckMusic( 0, MAX_RAW_SAMPLES, 1 );
	s_soundtime += 5;
	S_UpdateBackgroundTrack();
	Check( s_rawend == MAX_RAW_SAMPLES + 5, "the music buffer was not refilled" );
	CheckMusic( MAX_RAW_SAMPLES, MAX_RAW_SAMPLES + 5, 1 );

	testCase = "a short looping track";	// the restart passes the loop name back in: it must not be copied onto itself
	StartMusic( 22050 );
	ServeWav( MUSIC_FILE, 2, 22050, 16, 1000 * 2 );
	S_StartBackgroundTrack( MUSIC_NAME, MUSIC_NAME );
	S_UpdateBackgroundTrack();
	Check( s_backgroundFile && s_rawend == MAX_RAW_SAMPLES, "the looping track did not fill the music buffer" );
	for ( i = 0; i < MAX_RAW_SAMPLES; i++ ) {
		Check( s_rawsamples[i].left == Pattern16( i % 1000 * 2 ) * 128, "the track did not loop from its start" );
	}
	Check( !strcmp( s_backgroundLoop, MUSIC_NAME ), "the loop name changed" );

	testCase = "8-bit mono music at 11025 Hz";
	StartMusic( 22050 );
	ServeWav( MUSIC_FILE, 1, 11025, 8, 20000 );
	S_StartBackgroundTrack( MUSIC_NAME, MUSIC_NAME );
	Check( s_backgroundFile && s_backgroundSamples == 20000, "the track did not start" );
	S_UpdateBackgroundTrack();
	Check( s_rawend == MAX_RAW_SAMPLES, "the music buffer was not filled" );

	testCase = "music at 131071 Hz, the fastest rate accepted";
	StartMusic( 11025 );
	ServeWav( MUSIC_FILE, 2, 131071, 16, 250000 * 2 );
	S_StartBackgroundTrack( MUSIC_NAME, MUSIC_NAME );
	Check( s_backgroundFile && s_backgroundSamples == 250000, "the track did not start" );
	S_UpdateBackgroundTrack();
	Check( s_rawend == MAX_RAW_SAMPLES, "the music buffer was not filled" );
	S_StopBackgroundTrack();
}

/**
 * Music at half the Mac's 22050 Hz mixer rate plays each sample twice. When the mixer moves on by an odd
 * number of samples, one sample of room is left, too little for another file sample: the update must wait
 * for the next one instead of reading nothing forever.
 */
static void TestMusicSlow( void ) {
	testCase = "11025 Hz music with one sample of room left";
	StartMusic( 22050 );
	ServeWav( MUSIC_FILE, 2, 11025, 16, 20000 * 2 );
	S_StartBackgroundTrack( MUSIC_NAME, MUSIC_NAME );
	Check( s_backgroundFile != 0, "the track did not start" );
	S_UpdateBackgroundTrack();
	Check( s_rawend == MAX_RAW_SAMPLES, "the music buffer was not filled" );
	CheckMusic( 0, MAX_RAW_SAMPLES, 2 );
	s_soundtime += 3;
	S_UpdateBackgroundTrack();
	Check( s_rawend == MAX_RAW_SAMPLES + 2, "wrong refill with one sample of room left" );
	s_soundtime += 1;
	S_UpdateBackgroundTrack();
	Check( s_rawend == MAX_RAW_SAMPLES + 4, "the music buffer was not refilled" );
	CheckMusic( MAX_RAW_SAMPLES, MAX_RAW_SAMPLES + 4, 2 );
	S_StopBackgroundTrack();
}

/** A 1 Hz track is accepted but never has a whole sample to add to the buffer. */
static void TestMusicTiny( void ) {
	testCase = "1 Hz music";
	StartMusic( 22050 );
	ServeWav( MUSIC_FILE, 2, 1, 16, 64 );
	S_StartBackgroundTrack( MUSIC_NAME, MUSIC_NAME );
	Check( s_backgroundFile != 0, "the track did not start" );
	S_UpdateBackgroundTrack();
	Check( s_rawend == 0 && s_backgroundSamples == 32, "a 1 Hz track added music" );
	S_StopBackgroundTrack();
}

/** A looping track with a data chunk shorter than one sample frame has no sample to add, and no end to reach. */
static void TestMusicEmpty( void ) {
	testCase = "looping music with no whole sample";
	StartMusic( 22050 );
	ServeWav( MUSIC_FILE, 2, 22050, 16, 1 );
	S_StartBackgroundTrack( MUSIC_NAME, MUSIC_NAME );
	Check( s_backgroundFile && s_backgroundSamples == 0, "the track did not start" );
	S_UpdateBackgroundTrack();
	Check( s_rawend == 0, "an empty track added music" );
	S_StopBackgroundTrack();
}

/*
===============================================================================

sound effects

===============================================================================
*/

/** Sample rates of 0 and below are refused: there is no step to resample by. */
static void TestSfxRate( void ) {
	static const int rates[] = { 0, -1, -22050, INT_MIN };
	char name[MAX_QPATH];
	int i;

	testCase = "setup";
	StartSounds( 22050, SND_CHUNK_SIZE );
	for ( i = 0; i < (int)( sizeof( rates ) / sizeof( rates[0] ) ); i++ ) {
		snprintf( name, sizeof( name ), "sound/rate%d.wav", i );
		testCase = name;
		ServeWav( name, 1, rates[i], 16, 1000 );
		Refuse( name, "has an unsupported rate or length\n" );
	}
	testCase = "an empty sound at 0 Hz";	// 0 samples / 0 Hz was a NaN sample count
	ServeWav( "sound/empty0.wav", 1, 0, 16, 0 );
	Refuse( "sound/empty0.wav", "has an unsupported rate or length\n" );
}

/**
 * A sound whose resampled length passes the whole buffer pool is refused before it takes a buffer; one that
 * fills the pool exactly still loads, freeing the default sound for its last buffer.
 */
static void TestSfxPool( void ) {
	float stepscale;
	int handle, length;

	testCase = "the issue's 600 samples at 1 Hz";		// 13,230,000 samples: SND_malloc looped forever
	StartSounds( 22050, SND_CHUNK_SIZE );
	ServeWav( "sound/tiny.wav", 1, 1, 16, 600 );
	Refuse( "sound/tiny.wav", "does not fit in sound memory\n" );

	testCase = "100,000 samples at 1 Hz";				// past INT_MAX samples
	ServeWav( "sound/tinylong.wav", 1, 1, 16, 100000 );
	Refuse( "sound/tinylong.wav", "does not fit in sound memory\n" );

	testCase = "one sample past the pool at 22050 Hz";
	ServeWav( "sound/over.wav", 1, 22050, 16, POOL_SAMPLES + 1 );
	Refuse( "sound/over.wav", "does not fit in sound memory\n" );

	testCase = "71 samples at 1 Hz, which fit";
	ServeWav( "sound/tinyfits.wav", 1, 1, 16, 71 );
	stepscale = (float)1 / 22050;
	length = (int)( 71 / stepscale );
	Check( length > POOL_SAMPLES - 8 * SND_CHUNK_SIZE && length <= POOL_SAMPLES - SND_CHUNK_SIZE, "the fixture does not nearly fill the pool" );
	handle = S_RegisterSound( "sound/tinyfits.wav", qfalse );
	Check( handle > 0, "the sound did not register" );
	CheckLoaded( &s_knownSfx[handle], length, 0, 16 );

	testCase = "exactly the pool at 22050 Hz";
	StartSounds( 22050, SND_CHUNK_SIZE );
	ServeWav( "sound/pool.wav", 1, 22050, 16, POOL_SAMPLES );
	Check( S_RegisterSound( "sound/pool.wav", qfalse ) == 1, "the sound did not register" );
	CheckLoaded( &s_knownSfx[1], POOL_SAMPLES, 256, 16 );
	Check( !s_knownSfx[0].inMemory && !s_knownSfx[0].soundData, "the default sound was not freed for the last buffer" );
	Check( FreeBuffers() == 0, "the pool is not full" );
}

/**
 * With no other sound left in memory, SND_malloc returns NULL instead of looping, and a loader it fails
 * releases the buffers it took. Nothing but this test holds buffers outside a sound; with the pool bound
 * above, a loader can only reach this when it does.
 */
static void TestSfxMalloc( void ) {
	static sndBuffer *held[TEST_BUFFERS];
	int i, count;
	sfx_t sfx;

	testCase = "a drained pool";
	StartSounds( 22050, SND_CHUNK_SIZE );
	count = FreeBuffers() + 1;
	Check( count == TEST_BUFFERS, "wrong pool size" );
	for ( i = 0; i < count; i++ ) {
		held[i] = SND_malloc();
		Check( held[i] != NULL, "a buffer was not allocated" );
	}
	Check( !s_knownSfx[0].inMemory && !s_knownSfx[0].soundData, "the default sound was not freed for the last buffer" );
	Check( SND_malloc() == NULL, "SND_malloc did not fail on a drained pool" );
	for ( i = 0; i < count; i++ ) {
		SND_free( held[i] );
	}
	Check( FreeBuffers() == TEST_BUFFERS, "the drained buffers did not come back" );

	testCase = "a load that runs out of buffers";
	StartSounds( 22050, SND_CHUNK_SIZE );
	count = FreeBuffers() - 10;
	for ( i = 0; i < count; i++ ) {
		held[i] = SND_malloc();
	}
	ServeWav( "sound/twenty.wav", 1, 22050, 16, 20 * SND_CHUNK_SIZE );
	ClearMessages();
	Check( S_RegisterSound( "sound/twenty.wav", qfalse ) == 0, "the sound was accepted" );
	Check( s_knownSfx[1].defaultSound && !s_knownSfx[1].soundData && !s_knownSfx[1].soundLength, "the sound kept data" );
	Check( Printed( "sound/twenty.wav does not fit in sound memory\n" ), "the failure was not reported" );
	Check( FreeBuffers() == 11 && !s_knownSfx[0].inMemory, "the sound's buffers were not released" );

	testCase = "an ADPCM encode that runs out of buffers";
	ServeWav( "sound/adpcm.wav", 1, 22050, 16, 12 * SND_CHUNK_SIZE_BYTE * 2 );
	memset( &sfx, 0, sizeof( sfx ) );
	snprintf( sfx.soundName, sizeof( sfx.soundName ), "%s", "sound/adpcm.wav" );
	sfx.soundCompressed = qtrue;
	ClearMessages();
	Check( !S_LoadSound( &sfx ), "the sound was accepted" );
	Check( !sfx.soundData && !sfx.soundLength, "the sound kept data" );
	Check( Printed( "sound/adpcm.wav does not fit in sound memory\n" ), "the failure was not reported" );
	Check( FreeBuffers() == 11, "the sound's buffers were not released" );
	for ( i = 0; i < count; i++ ) {
		SND_free( held[i] );
	}
}

/**
 * The default sound in slot 0 is what S_FreeOldestSound frees when no other sound is older than now. When it
 * is itself being reloaded under pressure, its partial buffers must stay; a sound used just now is freed once
 * the clock moves on instead.
 */
static void TestSfxReload( void ) {
	sfx_t *hit = &s_knownSfx[0];
	int i;

	testCase = "reloading the default sound with the pool full";
	StartSounds( 22050, 2 * SND_CHUNK_SIZE );
	ServeWav( "sound/big.wav", 1, 22050, 16, ( TEST_BUFFERS - 2 ) * SND_CHUNK_SIZE );
	Check( S_RegisterSound( "sound/big.wav", qfalse ) == 1 && FreeBuffers() == 0, "the pool was not filled" );
	s_knownSfx[1].lastTimeUsed = clockMsec + 1000;
	S_FreeOldestSound();
	Check( !hit->inMemory && !hit->soundData && FreeBuffers() == 2, "the default sound was not freed" );
	ServeWav( "sound/small.wav", 1, 22050, 16, SND_CHUNK_SIZE );
	Check( S_RegisterSound( "sound/small.wav", qfalse ) == 2 && FreeBuffers() == 1, "the small sound did not load" );
	s_knownSfx[1].lastTimeUsed = s_knownSfx[2].lastTimeUsed = clockMsec + 1000;

	S_memoryLoad( hit );	// the reload S_StartSound does for a sound not in memory
	Check( hit->inMemory && !hit->defaultSound && hit->soundLength == 2 * SND_CHUNK_SIZE, "the default sound did not reload" );
	Check( ChainLength( hit ) == 2, "the default sound lost the buffers it was loading into" );
	for ( i = 0; i < hit->soundLength; i++ ) {
		Check( LoadedSample( hit, i ) == Pattern16( i ), "wrong reloaded sample" );
	}
	Check( !s_knownSfx[1].inMemory && s_knownSfx[2].inMemory, "the oldest sound was not the one freed" );
}

/** Sources of 2^23 samples, where samplefrac overflowed an int, are refused; one fewer loads through both resamplers. */
static void TestSfxLength( void ) {
	sfx_t sfx;
	int i;

	testCase = "2^23 - 1 8-bit samples at 64 times the mixer rate";
	StartSounds( 22050, SND_CHUNK_SIZE );
	ServeWav( "sound/long.wav", 1, 64 * 22050, 8, ( 1 << 23 ) - 1 );
	Check( S_RegisterSound( "sound/long.wav", qfalse ) == 1, "the sound did not register" );
	CheckLoaded( &s_knownSfx[1], ( ( 1 << 23 ) - 1 ) / 64, 64 * 256, 8 );

	testCase = "2^23 - 1 8-bit samples, ADPCM";
	memset( &sfx, 0, sizeof( sfx ) );
	snprintf( sfx.soundName, sizeof( sfx.soundName ), "%s", "sound/long.wav" );
	sfx.soundCompressed = qtrue;
	Check( S_LoadSound( &sfx ) && sfx.soundCompressionMethod == 1 && sfx.soundLength == ( ( 1 << 23 ) - 1 ) / 64, "the ADPCM load failed" );
	Check( ChainLength( &sfx ) == ( sfx.soundLength + SND_CHUNK_SIZE_BYTE * 2 - 1 ) / ( SND_CHUNK_SIZE_BYTE * 2 ), "wrong number of ADPCM buffers" );

	testCase = "2^23 8-bit samples at 64 times the mixer rate";
	ServeWav( "sound/toolong.wav", 1, 64 * 22050, 8, 1 << 23 );
	Refuse( "sound/toolong.wav", "has an unsupported rate or length\n" );

	testCase = "2^23 8-bit samples, ADPCM";
	memset( &sfx, 0, sizeof( sfx ) );
	snprintf( sfx.soundName, sizeof( sfx.soundName ), "%s", "sound/toolong.wav" );
	sfx.soundCompressed = qtrue;
	Check( !S_LoadSound( &sfx ) && !sfx.soundData && !sfx.soundLength, "the ADPCM load was accepted" );
	for ( i = 0; i < MAX_SERVED; i++ ) {
		free( served[i].bytes );
		served[i].bytes = NULL;
	}
}

/** An empty ADPCM sound has no first sample for the encoder to start from. */
static void TestAdpcmEmpty( void ) {
	sfx_t sfx;

	testCase = "an empty ADPCM sound";
	StartSounds( 22050, SND_CHUNK_SIZE );
	ServeWav( "sound/empty.wav", 1, 22050, 16, 0 );
	memset( &sfx, 0, sizeof( sfx ) );
	snprintf( sfx.soundName, sizeof( sfx.soundName ), "%s", "sound/empty.wav" );
	sfx.soundCompressed = qtrue;
	Check( S_LoadSound( &sfx ) && sfx.soundCompressionMethod == 1, "the empty sound did not load" );
	Check( !sfx.soundData && !sfx.soundLength, "the empty sound holds data" );
}

/** The rates the retail sounds use load through S_RegisterSound as before at each mixer rate. */
static void TestValid( void ) {
	static const int speeds[] = { 11025, 22050, 44100 };
	static const int rates[] = { 11025, 22050 };
	char name[MAX_QPATH];
	float stepscale;
	int s, r, bits;

	for ( s = 0; s < 3; s++ ) {
		for ( r = 0; r < 2; r++ ) {
			for ( bits = 8; bits <= 16; bits += 8 ) {
				snprintf( name, sizeof( name ), "sound/valid%d_%d_%d.wav", speeds[s], rates[r], bits );
				testCase = name;
				StartSounds( speeds[s], SND_CHUNK_SIZE );
				ServeWav( name, 1, rates[r], bits, 5000 );
				Check( S_RegisterSound( name, qfalse ) == 1, "the sound did not register" );
				stepscale = (float)rates[r] / speeds[s];
				CheckLoaded( &s_knownSfx[1], (int)( 5000 / stepscale ), (int)( stepscale * 256 ), bits );
			}
		}
	}
}

int main( int argc, char **argv ) {
	const char *mode = argc > 1 ? argv[1] : "";

	if ( !strcmp( mode, "music-format" ) ) { TestMusicFormat(); puts( "Music of 0 or negative bits, channels or rate, or more than the mixer plays, is refused (issue #370)" ); }
	else if ( !strcmp( mode, "music-stream" ) ) { TestMusicStream(); puts( "Stock-format music streams as before, up to the fastest rate accepted (issue #370)" ); }
	else if ( !strcmp( mode, "music-slow" ) ) { TestMusicSlow(); puts( "Half-rate music with one sample of room left waits instead of looping (issue #370)" ); }
	else if ( !strcmp( mode, "music-tiny" ) ) { TestMusicTiny(); puts( "1 Hz music never has a sample to add, and does not loop (issue #370)" ); }
	else if ( !strcmp( mode, "music-empty" ) ) { TestMusicEmpty(); puts( "Looping music with no whole sample does not loop forever (issue #370)" ); }
	else if ( !strcmp( mode, "sfx-rate" ) ) { TestSfxRate(); puts( "Sounds at 0 Hz or below are refused before resampling (issue #370)" ); }
	else if ( !strcmp( mode, "sfx-pool" ) ) { TestSfxPool(); puts( "Sounds that resample past the buffer pool are refused; one that fills it loads (issue #370)" ); }
	else if ( !strcmp( mode, "sfx-malloc" ) ) { TestSfxMalloc(); puts( "SND_malloc fails on a drained pool, and loaders release what they took (issue #370)" ); }
	else if ( !strcmp( mode, "sfx-reload" ) ) { TestSfxReload(); puts( "Reloading the default sound under pressure keeps its buffers (issue #370)" ); }
	else if ( !strcmp( mode, "sfx-length" ) ) { TestSfxLength(); puts( "Sources of 2^23 samples are refused before samplefrac overflows (issue #370)" ); }
	else if ( !strcmp( mode, "adpcm-empty" ) ) { TestAdpcmEmpty(); puts( "An empty ADPCM sound encodes without reading a sample (issue #370)" ); }
	else if ( !strcmp( mode, "valid" ) ) { TestValid(); puts( "Retail sample rates load as before at 11025, 22050 and 44100 Hz (issue #370)" ); }
	else { Check( 0, "unknown mode" ); }
	return 0;
}
