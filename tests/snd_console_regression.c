/*
 * Issue #394: snd_dma.c's console output and play command.
 *
 * - s_show 2 printed the int channel volumes with "%f %f %s", so printf read
 *   a double for each int. On the PowerPC target a double vararg takes two
 *   argument words, so the "%f"s ate both volumes and the name and the "%s"
 *   read the next stray word as a pointer; on x86-64 the "%s" reads the first
 *   volume (127) as a pointer. Either way it crashes or prints garbage.
 * - play built "<name>.wav" from its first argument for every argument, so
 *   "play a b c" started a.wav and never b.wav or c.wav.
 * - soundinfo printed the DMA buffer pointer with "0x%x".
 *
 * This test includes the real snd_dma.c and links the real cmd.c, so the
 * console command is tokenized as typed. The declarations below give the
 * engine's print functions printf format checking in this file only, and the
 * runner builds with -Werror=format: a format whose arguments do not match
 * fails the build. The output text is also compared at run time.
 */
#include <stdarg.h>
#include <stdio.h>

void Com_Printf( const char *fmt, ... ) __attribute__(( format( printf, 1, 2 ) ));
void Com_DPrintf( const char *fmt, ... ) __attribute__(( format( printf, 1, 2 ) ));
void Com_Error( int code, const char *fmt, ... ) __attribute__(( format( printf, 2, 3 ) ));
void Com_sprintf( char *dest, int size, const char *fmt, ... ) __attribute__(( format( printf, 3, 4 ) ));

#include "../code/client/snd_dma.c"
#include <stdlib.h>
#include <string.h>

#define MAX_LOADS	16

static cvar_t showCvar, mixaheadCvar, mixPreStepCvar;
static char printed[4096];
static char loaded[MAX_LOADS][MAX_QPATH];
static int numLoaded;
static sndBuffer sampleData;

/** Fail with what the sound code printed. */
static void Check( int ok, const char *message ) {
	if ( !ok ) {
		fprintf( stderr, "Sound console regression failed: %s (printed \"%s\")\n", message, printed );
		exit( 1 );
	}
}
/** Keep everything the sound code prints. */
void QDECL Com_Printf( const char *format, ... ) {
	va_list args;
	size_t used = strlen( printed );

	va_start( args, format );
	vsnprintf( printed + used, sizeof( printed ) - used, format, args );
	va_end( args );
}
void QDECL Com_DPrintf( const char *format, ... ) { (void)format; }
void QDECL Com_Error( int level, const char *format, ... ) { (void)level; (void)format; Check( 0, "unexpected Com_Error" ); }
void Com_Memset( void *dest, const int val, const size_t count ) { memset( dest, val, count ); }
/** A fixed clock: a second start of one sound inside 50 ms is dropped as a double start. */
int Com_Milliseconds( void ) { return 1000; }
char *CopyString( const char *in ) { return strcpy( malloc( strlen( in ) + 1 ), in ); }
void Z_Free( void *ptr ) { free( ptr ); }
/** Record each sound play registers; every name loads. */
qboolean S_LoadSound( sfx_t *sfx ) {
	Check( numLoaded < MAX_LOADS, "too many sound loads" );
	Q_strncpyz( loaded[numLoaded++], sfx->soundName, MAX_QPATH );
	sfx->soundData = &sampleData;
	sfx->soundLength = SND_CHUNK_SIZE;
	return qtrue;
}
/* S_Update mixes after printing: no device and no music here. */
int SNDDMA_GetDMAPos( void ) { return 0; }
void SNDDMA_BeginPainting( void ) {}
void SNDDMA_Submit( void ) {}
void S_PaintChannels( int endtime ) { (void)endtime; }
void Snd_Memset( void *dest, const int val, const size_t count ) { memset( dest, val, count ); }
int FS_FOpenFileRead( const char *qpath, fileHandle_t *file, qboolean uniqueFILE ) {
	(void)qpath; (void)file; (void)uniqueFILE;
	Check( 0, "unexpected music open" );
	return -1;
}
int FS_Read( void *buffer, int len, fileHandle_t f ) { (void)buffer; (void)len; (void)f; Check( 0, "unexpected music read" ); return 0; }
void FS_FCloseFile( fileHandle_t f ) { (void)f; Check( 0, "unexpected music close" ); }
void Sys_BeginStreamedFile( fileHandle_t f, int readahead ) { (void)f; (void)readahead; Check( 0, "unexpected music stream" ); }
void Sys_EndStreamedFile( fileHandle_t f ) { (void)f; Check( 0, "unexpected music stream" ); }
int Sys_StreamedRead( void *buffer, int size, int count, fileHandle_t f ) {
	(void)buffer; (void)size; (void)count; (void)f;
	Check( 0, "unexpected music read" );
	return 0;
}

/** The channel playing NAME, requiring exactly one. */
static channel_t *Playing( const char *name ) {
	channel_t *found = NULL;
	int i;

	for ( i = 0; i < MAX_CHANNELS; i++ ) {
		if ( s_channels[i].thesfx && !strcmp( s_channels[i].thesfx->soundName, name ) ) {
			Check( !found, "a sound started on two channels" );
			found = &s_channels[i];
		}
	}
	Check( found != NULL, "a named sound did not start" );
	return found;
}
/** Run a console command line through the real tokenizer. */
static void Run( const char *text, void (*command)( void ) ) {
	printed[0] = 0;
	Cmd_TokenizeString( text );
	command();
}

/** play starts each named sound once, adding .wav only to names without an extension. */
static void TestPlay( void ) {
	char expected[256];

	s_show->integer = 1;
	Run( "play a b c", S_Play_f );
	Check( numLoaded == 3, "play did not register each argument" );
	Check( !strcmp( loaded[0], "a.wav" ) && !strcmp( loaded[1], "b.wav" ) && !strcmp( loaded[2], "c.wav" ),
		"play registered the wrong names" );
	Check( Playing( "a.wav" ) == &s_channels[MAX_CHANNELS - 1], "a.wav is not on the first free channel" );
	Check( Playing( "b.wav" ) == &s_channels[MAX_CHANNELS - 2], "b.wav is not on the second free channel" );
	Check( Playing( "c.wav" ) == &s_channels[MAX_CHANNELS - 3], "c.wav is not on the third free channel" );
	Check( s_channels[MAX_CHANNELS - 4].thesfx == NULL, "play started an extra sound" );
	Com_sprintf( expected, sizeof( expected ), "%i : a.wav\n%i : b.wav\n%i : c.wav\n",
		s_paintedtime, s_paintedtime, s_paintedtime );
	Check( !strcmp( printed, expected ), "s_show 1 did not list each started sound once" );

	s_show->integer = 0;
	Run( "play d.wav e", S_Play_f );
	Check( numLoaded == 5 && !strcmp( loaded[3], "d.wav" ) && !strcmp( loaded[4], "e.wav" ),
		"play built a name from the wrong argument" );
	Playing( "d.wav" );
	Playing( "e.wav" );
	Check( !printed[0], "play printed with s_show 0" );
}

/** s_show 2 lists each audible channel's int volumes and sound name. */
static void TestShow( void ) {
	channel_t *ch = Playing( "b.wav" );

	ch->leftvol = 90;
	ch->rightvol = 0;
	Playing( "c.wav" )->leftvol = 0;
	Playing( "c.wav" )->rightvol = 0;		// silent: not listed
	Playing( "d.wav" )->rightvol = 31;
	s_show->integer = 2;
	s_paintedtime = 4321;
	printed[0] = 0;
	S_Update();
	Check( !strcmp( printed, "127 127 e.wav\n127 31 d.wav\n90 0 b.wav\n127 127 a.wav\n----(4)---- painted: 4321\n" ),
		"s_show 2 printed the wrong channel list" );
	s_show->integer = 0;
}

/** soundinfo prints the DMA buffer as a pointer: a stack address needs more than 32 bits on a 64-bit host. */
static void TestSoundInfo( void ) {
	byte dmaBuffer[64];
	char expected[512];

	dma.buffer = dmaBuffer;
	Com_sprintf( expected, sizeof( expected ),
		"----- Sound Info -----\n%5d stereo\n%5d samples\n%5d samplebits\n%5d submission_chunk\n"
		"%5d speed\n%p dma buffer\nNo background file.\n----------------------\n",
		dma.channels - 1, dma.samples, dma.samplebits, dma.submission_chunk, dma.speed, (void *)dmaBuffer );
	Run( "soundinfo", S_SoundInfo_f );
	Check( !strcmp( printed, expected ), "soundinfo printed the wrong text" );
	dma.buffer = NULL;
}

int main( void ) {
	mixaheadCvar.value = 0.2f;
	mixPreStepCvar.value = 0.05f;
	s_show = &showCvar;
	s_mixahead = &mixaheadCvar;
	s_mixPreStep = &mixPreStepCvar;
	dma.channels = 2;
	dma.samples = 16384;
	dma.samplebits = 16;
	dma.submission_chunk = 1;
	dma.speed = 22050;
	s_soundStarted = 1;
	listener_number = 0;
	S_ChannelSetup();
	S_RegisterSound( "sound/feedback/hit.wav", qfalse );	// S_BeginRegistration's handle 0, which play skips
	numLoaded = 0;

	TestPlay();
	TestShow();
	TestSoundInfo();
	puts( "Sound console regressions passed (issue #394)" );
	return 0;
}
