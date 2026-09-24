/*
 * Issue #400: S_FreeOldestSound freed the least recently used sound in memory without asking whether a
 * channel or a looping sound still played it. S_PaintChannels went on painting that channel with the
 * sound's unchanged length, and S_PaintChannelFrom16 read its samples through the NULL buffer chain. A
 * paged-out sound whose file then failed to reload (removed, or replaced by a stereo WAV or one at 0 Hz)
 * also kept its length with no data, and crashed the mixer the same way when it was started again.
 *
 * This test includes the real snd_dma.c and links the real snd_mem.c, snd_mix.c, snd_adpcm.c and
 * snd_wavelet.c. Each mode fills the buffer pool while channels and looping sounds play, loads another
 * sound, and paints with S_PaintChannels, comparing each output sample with the sounds' own samples.
 * The last mode checks that sounds nothing plays are still freed oldest first, and page back in.
 */
#include "../code/client/snd_dma.c"
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define TEST_SOUND_MEGS	1
#define DMA_FRAMES		8192							// stereo frames in the DMA buffer
#define MAX_SERVED		16
#define DEFAULT_SOUND	"sound/feedback/hit.wav"
#define LISTENER		0								// the listener's own sounds play at full volume
#define FULL_VOLUME		127
// Each test sound stops short of filling its last buffer. S_PaintChannelFrom16 steps its chunk pointer
// past the last buffer of a sound that fills it exactly, which UBSan reports although nothing is read.
#define LENGTH( chunks )	( ( chunks ) * SND_CHUNK_SIZE - 100 )

typedef struct {
	char			name[MAX_QPATH];
	unsigned char	*bytes;
	int				length;
} servedFile_t;

/** A sound the mixer should be painting: a channel's from its start, or a loop's at the paint time modulo its length. */
typedef struct {
	int		seed;			// the sound's samples
	int		length;
	int		start;			// the paint time of a channel's first sample, -1 for a loop
	int		volume;			// the channel's left and right volume
} voice_t;

clientStatic_t cls;

static const char	*testCase = "setup";
static servedFile_t	served[MAX_SERVED];
static char			lastMessage[256];
static char			messageLog[4096];
static int			clockMsec = 1000;
static cvar_t		volumeCvar, testsoundCvar, showCvar, dopplerCvar;
static short		*dmaOut;
static vec3_t		listenerAxis[3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };

static void Check( int ok, const char *message ) {
	if ( !ok ) {
		fprintf( stderr, "sound eviction regression failed: %s: %s\n", testCase, message );
		exit( 1 );
	}
}

/** Keep each message so a refusal can be attributed. */
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
	exit( 1 );
}
void Com_Memset( void *dest, const int val, const size_t count ) { memset( dest, val, count ); }
/** The clock moves on at every read. */
int Com_Milliseconds( void ) { return clockMsec++; }
cvar_t *Cvar_Get( const char *name, const char *value, int flags ) {
	static cvar_t soundMegs;

	(void)name; (void)value; (void)flags;
	soundMegs.integer = TEST_SOUND_MEGS;
	return &soundMegs;
}
void *Hunk_AllocateTempMemory( int size ) { return malloc( size ); }
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
/** Serve a file as an exact-size heap copy. */
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

/** Sample i of the sound with the given seed: each test sound's samples differ from every other's. */
static short Sample( int seed, int i ) { return (short)( ( ( i * 37 + seed * 4099 + 11 ) & 0xffff ) - 0x8000 ); }

static void PutLong( unsigned char *p, uint32_t value ) {
	p[0] = value & 255; p[1] = ( value >> 8 ) & 255; p[2] = ( value >> 16 ) & 255; p[3] = value >> 24;
}
static void PutShort( unsigned char *p, uint32_t value ) {
	p[0] = value & 255; p[1] = ( value >> 8 ) & 255;
}
/** Serve a 16-bit PCM WAV file under 'name' holding 'values' samples of the sound with the given seed. */
static void ServeWav( const char *name, int channels, int rate, int values, int seed ) {
	int dataLength = values * 2, i, slot = FindServed( name );
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
	PutLong( p + 28, (uint32_t)rate * (uint32_t)( channels * 2 ) ); PutShort( p + 32, (uint32_t)( channels * 2 ) );
	PutShort( p + 34, 16 );
	memcpy( p + 36, "data", 4 ); PutLong( p + 40, dataLength );
	for ( i = 0; i < values; i++ ) {
		PutShort( p + 44 + i * 2, (uint16_t)Sample( seed, i ) );
	}
}
static void Unserve( const char *name ) {
	int slot = FindServed( name );

	Check( slot >= 0, "removed a file that is not served" );
	free( served[slot].bytes );
	served[slot].bytes = NULL;
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

/** Require a sound to be in memory with all 'length' samples of the sound with the given seed. */
static void CheckIntact( const sfx_t *sfx, int seed, int length ) {
	sndBuffer *chunk = sfx->soundData;
	int i;

	Check( sfx->inMemory && !sfx->defaultSound && sfx->soundLength == length, "a sound lost its length" );
	for ( i = 0; i < length; i++ ) {
		if ( i && !( i & ( SND_CHUNK_SIZE - 1 ) ) ) {
			chunk = chunk->next;
		}
		Check( chunk != NULL, "a sound lost its buffers" );
		Check( chunk->sndChunk[i & ( SND_CHUNK_SIZE - 1 )] == Sample( seed, i ), "a sound lost its samples" );
	}
	Check( !chunk || !chunk->next, "a sound has buffers past its length" );
}
/** Require a sound to have been paged out. */
static void CheckFreed( const sfx_t *sfx ) {
	Check( !sfx->inMemory && !sfx->soundData, "a sound was not freed" );
}

/** Start the sound system over as a level load does: a fresh buffer pool, the default sound in slot 0, nothing playing. */
static void StartSounds( void ) {
	dma.speed = 22050;
	dma.channels = 2;
	dma.samplebits = 16;
	dma.samples = DMA_FRAMES * 2;
	if ( !dmaOut ) {
		dmaOut = malloc( DMA_FRAMES * 2 * sizeof( short ) );	// exact size: AddressSanitizer reports a write past it
	}
	memset( dmaOut, 0, DMA_FRAMES * 2 * sizeof( short ) );
	dma.buffer = (byte *)dmaOut;
	volumeCvar.value = 1.0f;
	s_volume = &volumeCvar;
	s_testsound = &testsoundCvar;
	s_show = &showCvar;
	s_doppler = &dopplerCvar;

	s_soundStarted = 1;
	s_numSfx = 0;
	ServeWav( DEFAULT_SOUND, 1, 22050, LENGTH( 1 ), 0 );
	S_BeginRegistration();
	CheckIntact( &s_knownSfx[0], 0, LENGTH( 1 ) );
	S_ChannelSetup();
	Com_Memset( loopSounds, 0, sizeof( loopSounds ) );
	numLoopChannels = 0;
	s_paintedtime = 0;
	s_rawend = 0;
	S_Respatialize( LISTENER, vec3_origin, listenerAxis, 0 );
}

/** Serve a 22050 Hz mono sound that takes 'chunks' buffers under 'name', and register it. */
static sfx_t *Register( const char *name, int seed, int chunks ) {
	sfxHandle_t handle;

	ServeWav( name, 1, 22050, LENGTH( chunks ), seed );
	handle = S_RegisterSound( name, qfalse );
	Check( handle > 0, "a sound did not register" );
	CheckIntact( &s_knownSfx[handle], seed, LENGTH( chunks ) );
	return &s_knownSfx[handle];
}

/** Start a sound on a new channel of the listener's, as S_StartLocalSound does; it plays from the next mix. */
static channel_t *Play( sfx_t *sfx ) {
	int i;

	clockMsec += 100;
	S_StartSound( NULL, LISTENER, CHAN_AUTO, sfx - s_knownSfx );
	for ( i = 0; i < MAX_CHANNELS; i++ ) {
		if ( s_channels[i].thesfx == sfx && s_channels[i].startSample == START_SAMPLE_IMMEDIATE ) {
			return &s_channels[i];
		}
	}
	Check( 0, "a sound did not start" );
	return NULL;
}

/** Update the listener, which rebuilds the loop channels, as cgame does at the end of each frame. */
static void Respatialize( void ) {
	S_Respatialize( LISTENER, vec3_origin, listenerAxis, 0 );
}
/** The volume the mixer plays a sound's loop channel at. */
static int LoopVolume( const sfx_t *sfx ) {
	int i;

	for ( i = 0; i < numLoopChannels; i++ ) {
		if ( loop_channels[i].thesfx == sfx ) {
			Check( loop_channels[i].leftvol > 0 && loop_channels[i].leftvol == loop_channels[i].rightvol, "wrong loop volume" );
			return loop_channels[i].leftvol;
		}
	}
	Check( 0, "a looping sound has no loop channel" );
	return 0;
}

/** Mix the next 'count' samples as S_Update_ does, and require them to be the given voices' samples. */
static void Mix( int count, const voice_t *voices, int numVoices ) {
	int from = s_paintedtime, t, v, k, value;
	short *out;

	S_ScanChannelStarts();
	S_PaintChannels( from + count );
	Check( s_paintedtime == from + count, "the mixer did not paint" );
	for ( t = from; t < from + count; t++ ) {
		value = 0;
		for ( v = 0; v < numVoices; v++ ) {
			k = voices[v].start < 0 ? t % voices[v].length : t - voices[v].start;
			if ( k >= 0 && k < voices[v].length ) {
				value += ( Sample( voices[v].seed, k ) * ( voices[v].volume * 255 ) ) >> 8;
			}
		}
		value >>= 8;
		if ( value > 0x7fff ) {
			value = 0x7fff;
		} else if ( value < -32768 ) {
			value = -32768;
		}
		out = &dmaOut[( t & ( DMA_FRAMES - 1 ) ) * 2];
		Check( out[0] == value && out[1] == value, "wrong painted sample" );
	}
}

/** A voice for a channel started at the current paint time. */
static voice_t ChannelVoice( int seed, int length ) {
	voice_t voice;

	voice.seed = seed; voice.length = length; voice.start = s_paintedtime; voice.volume = FULL_VOLUME;
	return voice;
}
static voice_t LoopVoice( const sfx_t *sfx, int seed ) {
	voice_t voice;

	voice.seed = seed; voice.length = sfx->soundLength; voice.start = -1; voice.volume = LoopVolume( sfx );
	return voice;
}

/*
===============================================================================

tests

===============================================================================
*/

/** The issue: a channel plays the oldest sound in memory when a load finds the pool full. */
static void TestChannel( void ) {
	voice_t voices[1];
	sfx_t *playing, *filler;
	int i;

	testCase = "a channel plays the oldest sound";
	StartSounds();
	playing = Register( "sound/playing.wav", 1, 4 );
	Play( playing );
	voices[0] = ChannelVoice( 1, LENGTH( 4 ) );
	Mix( 1000, voices, 1 );
	filler = Register( "sound/filler.wav", 2, FreeBuffers() );
	Check( FreeBuffers() == 0, "the pool is not full" );

	Register( "sound/next.wav", 3, 2 );				// SND_malloc has to free a sound for it
	Mix( 1000, voices, 1 );
	CheckIntact( playing, 1, LENGTH( 4 ) );
	CheckFreed( filler );
	for ( i = 0; i < 3; i++ ) {
		Mix( 1000, voices, 1 );						// to past the sound's end
	}
	Mix( 1000, NULL, 0 );
	for ( i = 0; i < MAX_CHANNELS; i++ ) {
		Check( !s_channels[i].thesfx, "a channel still plays a sound that ended" );
	}
}

/**
 * Looping sounds: one cgame adds each frame, a real looping sound between S_ClearLoopingSounds and the
 * S_Respatialize that rebuilds the loop channels, a looping sound stopped since that the mixer still paints
 * until the next S_Respatialize, and the oldest sound on a channel and looping at once.
 */
static void TestLoop( void ) {
	static const vec3_t origin = { 0, 0, 0 }, velocity = { 0, 0, 0 };
	voice_t voices[2];
	sfx_t *looping, *filler;
	int i;

	testCase = "a looping sound is the oldest sound";
	StartSounds();
	looping = Register( "sound/loop.wav", 4, 3 );
	S_AddLoopingSound( 5, origin, velocity, looping - s_knownSfx );
	Respatialize();
	voices[0] = LoopVoice( looping, 4 );
	Mix( 1000, voices, 1 );
	filler = Register( "sound/filler.wav", 5, FreeBuffers() );
	Register( "sound/next.wav", 6, 2 );
	CheckIntact( looping, 4, LENGTH( 3 ) );
	CheckFreed( filler );
	for ( i = 0; i < 4; i++ ) {
		Mix( 1000, voices, 1 );
	}

	testCase = "a real looping sound before the loop channels are rebuilt";
	StartSounds();
	looping = Register( "sound/loop.wav", 7, 3 );
	S_AddRealLoopingSound( 6, origin, velocity, looping - s_knownSfx );
	Respatialize();
	voices[0] = LoopVoice( looping, 7 );
	Mix( 1000, voices, 1 );
	S_ClearLoopingSounds( qfalse );					// the start of a cgame frame: the real loop stays active
	Check( loopSounds[6].active && numLoopChannels == 0, "the real looping sound was cleared" );
	filler = Register( "sound/filler.wav", 8, FreeBuffers() );
	Register( "sound/next.wav", 9, 2 );
	CheckIntact( looping, 7, LENGTH( 3 ) );
	CheckFreed( filler );
	Respatialize();
	Mix( 4000, voices, 1 );

	testCase = "a stopped looping sound still on a loop channel";
	StartSounds();
	looping = Register( "sound/loop.wav", 10, 3 );
	S_AddLoopingSound( 7, origin, velocity, looping - s_knownSfx );
	Respatialize();
	voices[0] = LoopVoice( looping, 10 );
	S_StopLoopingSound( 7 );
	Check( !loopSounds[7].active && numLoopChannels == 1, "the loop channel was not kept" );
	filler = Register( "sound/filler.wav", 11, FreeBuffers() );
	Register( "sound/next.wav", 12, 2 );
	CheckIntact( looping, 10, LENGTH( 3 ) );
	CheckFreed( filler );
	Mix( 4000, voices, 1 );

	testCase = "the oldest sound on a channel and looping";
	StartSounds();
	looping = Register( "sound/both.wav", 13, 3 );
	Play( looping );
	voices[0] = ChannelVoice( 13, LENGTH( 3 ) );
	S_AddLoopingSound( 8, origin, velocity, looping - s_knownSfx );
	Respatialize();
	voices[1] = LoopVoice( looping, 13 );
	Mix( 1000, voices, 2 );
	filler = Register( "sound/filler.wav", 14, FreeBuffers() );
	Register( "sound/next.wav", 15, 2 );
	CheckIntact( looping, 13, LENGTH( 3 ) );
	CheckFreed( filler );
	Respatialize();
	for ( i = 0; i < 4; i++ ) {
		Mix( 1000, voices, 2 );
	}
}

/**
 * When every sound in memory is playing, the default sound in slot 0 included, a load fails cleanly
 * instead of freeing one; once they stop, the same load frees the oldest as before.
 */
static void TestFull( void ) {
	voice_t voices[2];
	sfx_t *hit = &s_knownSfx[0], *big;
	sfxHandle_t handle;

	testCase = "every sound in memory is playing";
	StartSounds();
	big = Register( "sound/big.wav", 16, FreeBuffers() );
	Check( FreeBuffers() == 0, "the pool is not full" );
	Play( big );
	voices[0] = ChannelVoice( 16, big->soundLength );
	Play( hit );
	voices[1] = ChannelVoice( 0, LENGTH( 1 ) );
	Mix( 500, voices, 2 );

	ServeWav( "sound/one.wav", 1, 22050, LENGTH( 1 ), 17 );
	ClearMessages();
	handle = S_RegisterSound( "sound/one.wav", qfalse );
	Mix( 2000, voices, 2 );
	Check( handle == 0, "a playing sound was freed for a load" );
	Check( Printed( "sound/one.wav does not fit in sound memory\n" ), "the failure was not reported" );
	CheckIntact( big, 16, big->soundLength );
	CheckIntact( hit, 0, LENGTH( 1 ) );

	testCase = "every sound in memory has stopped";
	S_ChannelSetup();								// what S_StopAllSounds does to the channels
	ServeWav( "sound/two.wav", 1, 22050, LENGTH( 1 ), 18 );	// a failed name stays the default sound, as in retail
	handle = S_RegisterSound( "sound/two.wav", qfalse );
	Check( handle > 0, "the load failed with nothing playing" );
	CheckIntact( &s_knownSfx[handle], 18, LENGTH( 1 ) );
	CheckFreed( big );
	CheckIntact( hit, 0, LENGTH( 1 ) );
}

/**
 * The extra case: a sound paged out while nothing played it, whose file then fails to reload, is started
 * again while another channel plays. It must play nothing, and the other channel must play on.
 */
static void TestReload( void ) {
	static const struct {
		const char	*what;
		int			channels, rate;		// 0 channels: the file is removed
		const char	*message;
	} failures[] = {
		{ "a paged-out sound replaced by a stereo WAV", 2, 22050, "sound/paged.wav is a stereo wav file\n" },
		{ "a paged-out sound whose file was removed", 0, 0, NULL },
		{ "a paged-out sound replaced by a 0 Hz WAV", 1, 0, "sound/paged.wav has an unsupported rate or length\n" },
	};
	voice_t voices[1];
	sfx_t *paged, *live;
	channel_t *ch;
	int f, i;

	for ( f = 0; f < (int)( sizeof( failures ) / sizeof( failures[0] ) ); f++ ) {
		testCase = failures[f].what;
		StartSounds();
		paged = Register( "sound/paged.wav", 18, 4 );
		live = Register( "sound/live.wav", 19, 8 );
		Play( live );
		voices[0] = ChannelVoice( 19, LENGTH( 8 ) );
		Mix( 500, voices, 1 );
		Register( "sound/filler.wav", 20, FreeBuffers() );
		Register( "sound/next.wav", 21, 2 );		// pages out the oldest sound nothing plays
		CheckFreed( paged );
		CheckIntact( live, 19, LENGTH( 8 ) );
		Check( paged->soundLength == LENGTH( 4 ), "a paged-out sound lost its length" );	// as retail

		if ( failures[f].channels ) {
			ServeWav( "sound/paged.wav", failures[f].channels, failures[f].rate, LENGTH( 4 ), 18 );
		} else {
			Unserve( "sound/paged.wav" );
		}
		ClearMessages();
		ch = Play( paged );
		Check( !failures[f].message || Printed( failures[f].message ), "the reload failure was not reported" );
		Mix( 1000, voices, 1 );						// the failed sound plays nothing; the live one plays on
		Check( paged->inMemory && paged->defaultSound, "the failed reload is not the default sound" );
		Check( !paged->soundData && !paged->soundLength, "the failed reload kept a length with no data" );
		Mix( 1000, voices, 1 );
		Check( !ch->thesfx, "the failed sound's channel was not freed" );
		for ( i = 0; i < 5; i++ ) {
			Mix( 1000, voices, 1 );
		}
	}
}

/**
 * The mixer's own guard: a channel whose sound has no data, as a sound paged out under a playing channel
 * had before this fix, paints nothing, as the loop channels already did.
 */
static void TestMixer( void ) {
	voice_t voices[2];
	sfx_t *lost, *live;
	sndBuffer *chunk, *next;

	testCase = "a channel whose sound has no data";
	StartSounds();
	lost = Register( "sound/lost.wav", 22, 4 );
	live = Register( "sound/live.wav", 23, 8 );
	Play( lost );
	voices[0] = ChannelVoice( 22, LENGTH( 4 ) );
	Play( live );
	voices[1] = ChannelVoice( 23, LENGTH( 8 ) );
	Mix( 1500, voices, 2 );
	for ( chunk = lost->soundData; chunk; chunk = next ) {		// what S_FreeOldestSound did to it
		next = chunk->next;
		SND_free( chunk );
	}
	lost->soundData = NULL;
	lost->inMemory = qfalse;
	Mix( 4000, voices + 1, 1 );
}

/**
 * Sounds nothing plays are still freed least recently used first, a sound whose channel has ended among
 * them, and a paged-out sound reloads and plays when it is started again.
 */
static void TestUnused( void ) {
	voice_t voices[1];
	sfx_t *a, *b, *c, *filler, *d, *e, *f;
	int i;

	testCase = "sounds nothing plays";
	StartSounds();
	a = Register( "sound/a.wav", 24, 4 );
	b = Register( "sound/b.wav", 25, 4 );
	c = Register( "sound/c.wav", 26, 4 );
	Play( a );										// now used after b and c
	voices[0] = ChannelVoice( 24, LENGTH( 4 ) );
	Mix( 1000, voices, 1 );
	filler = Register( "sound/filler.wav", 27, FreeBuffers() );

	d = Register( "sound/d.wav", 28, 4 );
	CheckFreed( b );
	CheckIntact( c, 26, LENGTH( 4 ) );
	CheckIntact( a, 24, LENGTH( 4 ) );
	CheckIntact( filler, 27, filler->soundLength );
	e = Register( "sound/e.wav", 29, 4 );
	CheckFreed( c );
	CheckIntact( a, 24, LENGTH( 4 ) );

	for ( i = 0; i < 4; i++ ) {
		Mix( 1000, voices, 1 );						// a ends, and its channel is freed
	}
	f = Register( "sound/f.wav", 30, 4 );
	CheckFreed( a );
	CheckIntact( filler, 27, filler->soundLength );
	CheckIntact( d, 28, LENGTH( 4 ) );
	CheckIntact( e, 29, LENGTH( 4 ) );
	CheckIntact( f, 30, LENGTH( 4 ) );

	testCase = "a paged-out sound started again";
	Play( b );										// reloads b, which frees the filler, now the oldest
	CheckIntact( b, 25, LENGTH( 4 ) );
	CheckFreed( filler );
	CheckIntact( d, 28, LENGTH( 4 ) );
	voices[0] = ChannelVoice( 25, LENGTH( 4 ) );
	for ( i = 0; i < 5; i++ ) {
		Mix( 1000, voices, 1 );
	}
}

int main( int argc, char **argv ) {
	const char *mode = argc > 1 ? argv[1] : "";

	if ( !strcmp( mode, "channel" ) ) { TestChannel(); puts( "A load does not free the sound a channel plays (issue #400)" ); }
	else if ( !strcmp( mode, "loop" ) ) { TestLoop(); puts( "A load does not free a looping sound, active or still on a loop channel (issue #400)" ); }
	else if ( !strcmp( mode, "full" ) ) { TestFull(); puts( "A load fails cleanly when every sound in memory is playing (issue #400)" ); }
	else if ( !strcmp( mode, "reload" ) ) { TestReload(); puts( "A paged-out sound that fails to reload plays nothing (issue #400)" ); }
	else if ( !strcmp( mode, "mixer" ) ) { TestMixer(); puts( "The mixer skips a channel whose sound has no data (issue #400)" ); }
	else if ( !strcmp( mode, "unused" ) ) { TestUnused(); puts( "Sounds nothing plays are freed oldest first and page back in (issue #400)" ); }
	else { Check( 0, "unknown mode" ); }
	return 0;
}
