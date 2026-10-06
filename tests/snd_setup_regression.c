/*
 * Issue #230: SND_setup took the sound buffer pool (com_soundMegs * 1536 buffers, about 24 MB at
 * retail's default) from malloc without checking it, then wrote the free list through it. A failed
 * allocation wrote that list downward from NULL plus the pool's size: over the system heap and low
 * memory on Mac OS 9, which does not protect them. com_soundMegs 0 wrote the list's end past a
 * zero-byte block, and a negative value asked for an impossible size.
 *
 * This test includes the real snd_dma.c and links the real snd_mem.c (built with malloc and free
 * renamed to this test's), snd_mix.c, snd_adpcm.c and snd_wavelet.c. Each mode makes the pool, the
 * scratch buffer or com_soundMegs fail, and requires S_BeginRegistration to leave sound off, as
 * s_initsound 0 does: the DMA backend shut down, a warning printed, nothing left allocated, and the
 * sound calls a level makes doing nothing. Then sound starts again with the pool available.
 */
#include "../code/client/snd_dma.c"
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define TEST_SOUND_MEGS	1
#define DEFAULT_SOUND	"sound/feedback/hit.wav"
#define POOL_BYTES		( (size_t)TEST_SOUND_MEGS * 1536 * sizeof( sndBuffer ) )
#define SCRATCH_BYTES	( (size_t)SND_CHUNK_SIZE * sizeof( short ) * 4 )

clientStatic_t cls;

static const char		*testCase = "setup";
static char				messageLog[8192];
static int				soundMegs = TEST_SOUND_MEGS;
static int				failPool, failScratch;
static int				liveBlocks, mallocCalls, shutdownCalls;
static unsigned char	*wavFile;
static int				wavLength;

static void Check( int ok, const char *message ) {
	if ( !ok ) {
		fprintf( stderr, "sound setup regression failed: %s: %s\n", testCase, message );
		exit( 1 );
	}
}

/** snd_mem.c's malloc: fails the allocation a mode asks for, and counts what is left allocated. */
void *SetupMalloc( size_t size ) {
	void *block;

	mallocCalls++;
	if ( ( failPool && size == POOL_BYTES ) || ( failScratch && size == SCRATCH_BYTES ) ) {
		return NULL;
	}
	block = malloc( size );
	if ( block ) {
		liveBlocks++;
	}
	return block;
}
void SetupFree( void *block ) {
	if ( block ) {
		liveBlocks--;
	}
	free( block );
}

void QDECL Com_Printf( const char *format, ... ) {
	char	message[512];
	va_list	args;

	va_start( args, format );
	vsnprintf( message, sizeof( message ), format, args );
	va_end( args );
	if ( strlen( messageLog ) + strlen( message ) < sizeof( messageLog ) ) {
		strcat( messageLog, message );
	}
}
void QDECL Com_DPrintf( const char *format, ... ) { (void)format; }
void QDECL Com_Error( int level, const char *format, ... ) {
	char	message[512];
	va_list	args;

	va_start( args, format );
	vsnprintf( message, sizeof( message ), format, args );
	va_end( args );
	fprintf( stderr, "Com_Error %d: %s\n", level, message );
	Check( 0, "unexpected Com_Error" );
	exit( 1 );
}
void Com_Memset( void *dest, const int val, const size_t count ) { memset( dest, val, count ); }
int Com_Milliseconds( void ) { static int msec = 1000; return msec++; }
cvar_t *Cvar_Get( const char *name, const char *value, int flags ) {
	static cvar_t cvar;

	(void)value; (void)flags;
	Check( !strcmp( name, "com_soundMegs" ), "an unexpected cvar" );
	cvar.integer = soundMegs;
	return &cvar;
}
void *Hunk_AllocateTempMemory( int size ) { return malloc( size ); }
void Hunk_FreeTempMemory( void *buf ) { free( buf ); }
int FS_ReadFile( const char *qpath, void **buffer ) {
	if ( strcmp( qpath, DEFAULT_SOUND ) ) {
		*buffer = NULL;
		return -1;
	}
	*buffer = malloc( wavLength );
	memcpy( *buffer, wavFile, wavLength );
	return wavLength;
}
void FS_FreeFile( void *buffer ) { free( buffer ); }
void SNDDMA_Shutdown( void ) { shutdownCalls++; }
void Cmd_RemoveCommand( const char *name ) { (void)name; }
// Nothing reaches the DMA buffer or the music stream with sound off.
#define UNREACHED( what )	Check( 0, what " was reached with sound off" )
int SNDDMA_GetDMAPos( void ) { UNREACHED( "SNDDMA_GetDMAPos" ); return 0; }
void SNDDMA_BeginPainting( void ) { UNREACHED( "SNDDMA_BeginPainting" ); }
void SNDDMA_Submit( void ) { UNREACHED( "SNDDMA_Submit" ); }
void Snd_Memset( void *dest, const int val, const size_t count ) { (void)dest; (void)val; (void)count; UNREACHED( "Snd_Memset" ); }
int FS_FOpenFileRead( const char *qpath, fileHandle_t *file, qboolean uniqueFILE ) {
	(void)qpath; (void)file; (void)uniqueFILE; UNREACHED( "FS_FOpenFileRead" ); return -1;
}
int FS_Read( void *buffer, int len, fileHandle_t f ) { (void)buffer; (void)len; (void)f; UNREACHED( "FS_Read" ); return 0; }
void FS_FCloseFile( fileHandle_t f ) { (void)f; UNREACHED( "FS_FCloseFile" ); }
void Sys_BeginStreamedFile( fileHandle_t f, int readahead ) { (void)f; (void)readahead; UNREACHED( "Sys_BeginStreamedFile" ); }
void Sys_EndStreamedFile( fileHandle_t f ) { (void)f; UNREACHED( "Sys_EndStreamedFile" ); }
int Sys_StreamedRead( void *buffer, int size, int count, fileHandle_t f ) {
	(void)buffer; (void)size; (void)count; (void)f; UNREACHED( "Sys_StreamedRead" ); return 0;
}

static void PutLong( unsigned char *p, uint32_t value ) {
	p[0] = value & 255; p[1] = ( value >> 8 ) & 255; p[2] = ( value >> 16 ) & 255; p[3] = value >> 24;
}
static void PutShort( unsigned char *p, uint32_t value ) {
	p[0] = value & 255; p[1] = ( value >> 8 ) & 255;
}
/** A 22050 Hz mono 16-bit WAV file of 'values' samples, served as the default sound. */
static void ServeDefaultSound( int values ) {
	int i;

	wavLength = 44 + values * 2;
	wavFile = malloc( wavLength );
	memcpy( wavFile, "RIFF", 4 ); PutLong( wavFile + 4, wavLength - 8 ); memcpy( wavFile + 8, "WAVE", 4 );
	memcpy( wavFile + 12, "fmt ", 4 ); PutLong( wavFile + 16, 16 );
	PutShort( wavFile + 20, 1 ); PutShort( wavFile + 22, 1 ); PutLong( wavFile + 24, 22050 );
	PutLong( wavFile + 28, 22050 * 2 ); PutShort( wavFile + 32, 2 ); PutShort( wavFile + 34, 16 );
	memcpy( wavFile + 36, "data", 4 ); PutLong( wavFile + 40, values * 2 );
	for ( i = 0; i < values; i++ ) {
		PutShort( wavFile + 44 + i * 2, (uint32_t)( i * 37 ) );
	}
}

/** What S_Init leaves when SNDDMA_Init succeeds: sound started and muted until registration. */
static void StartSound( void ) {
	dma.speed = 22050;
	dma.channels = 2;
	dma.samplebits = 16;
	s_soundStarted = 1;
	s_soundMuted = 1;
	messageLog[0] = '\0';
}

/** Require the registration that just ran to have left sound off and nothing allocated. */
static void CheckDisabled( const char *warning ) {
	vec3_t origin = { 0, 0, 0 };

	Check( !s_soundStarted, "sound stayed started without a buffer pool" );
	Check( shutdownCalls == 1, "the DMA backend was not shut down once" );
	Check( strstr( messageLog, warning ) != NULL, "no warning says why sound is off" );
	Check( strstr( messageLog, "sound disabled" ) != NULL, "the warning does not say sound is off" );
	Check( !strstr( messageLog, "Sound memory manager started" ), "the manager reported a start" );
	Check( liveBlocks == 0, "a failed setup left memory allocated" );
	Check( s_numSfx == 0, "a sound was registered without a pool" );

	// what a level does next must do nothing
	Check( S_RegisterSound( DEFAULT_SOUND, qfalse ) == 0, "a sound registered with sound off" );
	S_StartLocalSound( 0, CHAN_LOCAL_SOUND );
	S_StartSound( origin, 0, CHAN_AUTO, 0 );
	S_AddLoopingSound( 0, origin, origin, 0 );
	S_ClearLoopingSounds( qtrue );
	S_Update();
	S_StopAllSounds();
	S_Shutdown();
	Check( shutdownCalls == 1, "a second shutdown reached the DMA backend" );
}

/** Start sound again, as snd_restart does, with every allocation available. */
static void CheckRestart( void ) {
	int freeBytes, totalBytes;

	failPool = failScratch = 0;
	soundMegs = TEST_SOUND_MEGS;
	StartSound();
	S_BeginRegistration();
	Check( s_soundStarted, "sound did not start with the pool available" );
	Check( strstr( messageLog, "Sound memory manager started" ) != NULL, "the manager did not start" );
	Check( liveBlocks == 2, "the pool and the scratch buffer are not both allocated" );
	Check( s_numSfx == 1 && s_knownSfx[0].inMemory && !s_knownSfx[0].defaultSound && s_knownSfx[0].soundData,
		"the default sound did not load into the pool" );
	messageLog[0] = '\0';
	S_DisplayFreeMemory();
	Check( sscanf( messageLog, "%d bytes free sound buffer memory, %d total used", &freeBytes, &totalBytes ) == 2,
		"no free memory report" );
	Check( freeBytes == (int)( POOL_BYTES - sizeof( sndBuffer ) ), "the pool does not hold every buffer but the one used" );
}

int main( int argc, char **argv ) {
	const char *mode = argc > 1 ? argv[1] : "";

	testCase = mode;
	ServeDefaultSound( 512 );
	StartSound();
	if ( !strcmp( mode, "pool" ) ) {
		char warning[128];

		failPool = 1;
		S_BeginRegistration();
		snprintf( warning, sizeof( warning ), "could not allocate %d KB of sound memory (com_soundMegs %d)",
			(int)( POOL_BYTES / 1024 ), TEST_SOUND_MEGS );
		CheckDisabled( warning );
		CheckRestart();
		puts( "A failed sound pool allocation leaves sound off instead of writing through NULL (issue #230)" );
	} else if ( !strcmp( mode, "scratch" ) ) {
		failScratch = 1;
		S_BeginRegistration();
		CheckDisabled( "could not allocate" );
		CheckRestart();
		puts( "A failed scratch buffer allocation frees the pool and leaves sound off (issue #230)" );
	} else if ( !strcmp( mode, "range" ) ) {
		static const int megs[] = { 0, -1, 100000 };
		int i;

		for ( i = 0; i < (int)( sizeof( megs ) / sizeof( megs[0] ) ); i++ ) {
			soundMegs = megs[i];
			shutdownCalls = 0;
			mallocCalls = 0;
			StartSound();
			S_BeginRegistration();
			CheckDisabled( "is out of range" );
			Check( mallocCalls == 0, "an out-of-range com_soundMegs reached malloc" );
		}
		CheckRestart();
		puts( "An out-of-range com_soundMegs leaves sound off without allocating (issue #230)" );
	} else {
		Check( 0, "unknown mode" );
	}
	return 0;
}
