/* A compressed sound can loop, and a loop's position runs past the end of its
 * chunks: doppler scales the offset beyond soundLength, and painting crosses
 * the last chunk.  Both must wrap as S_PaintChannelFrom16 does, not follow a
 * NULL chunk.  Actual S_PaintChannelFromADPCM; the decoder is stubbed. */
#include "../code/client/snd_mix.c"
#include <stdio.h>
#include <stdlib.h>

short				*sfxScratchBuffer;
sfx_t				*sfxScratchPointer;
int					sfxScratchIndex;
static sndBuffer	chunks[2];
static sfx_t		sound;
static channel_t	channel;
static int			decoded[8];
static int			numDecoded;

static void Check( int ok, const char *message ) {
	if ( !ok ) {
		fprintf( stderr, "ADPCM paint regression failed: %s\n", message );
		exit( 1 );
	}
}
/** Keep which chunk was decoded; a NULL chunk is a failure, not a crash. */
void S_AdpcmGetSamples( sndBuffer *chunk, short *to ) {
	Check( chunk == &chunks[0] || chunk == &chunks[1], "decoded a chunk that is not the sound's" );
	Check( numDecoded < 8, "decoded too many chunks" );
	decoded[numDecoded++] = chunk - chunks;
	memset( to, 0, SND_CHUNK_SIZE * 4 * sizeof( short ) );
}
void S_AdpcmEncodeSound( sfx_t *sfx, short *samples ) { (void)sfx; (void)samples; }
void QDECL Com_Printf( const char *format, ... ) { (void)format; }
void QDECL Com_DPrintf( const char *format, ... ) { (void)format; }
void QDECL Com_Error( int level, const char *format, ... ) { (void)level; Check( 0, format ); }

static void Reset( void ) {
	numDecoded = 0;
	sfxScratchIndex = -1;
	sfxScratchPointer = NULL;
}
int main( void ) {
	sfxScratchBuffer = malloc( SND_CHUNK_SIZE * 4 * sizeof( short ) );
	chunks[0].next = &chunks[1];
	sound.soundData = &chunks[0];
	sound.soundLength = SND_CHUNK_SIZE * 8;

	/* inside the sound: the second chunk, whose index is remembered */
	Reset();
	S_PaintChannelFromADPCM( &channel, &sound, 1, SND_CHUNK_SIZE * 4 + 1, 0 );
	Check( numDecoded == 1 && decoded[0] == 1 && sfxScratchIndex == 1, "second chunk" );

	/* doppler scales the offset past the end, and it wraps to the first */
	Reset();
	channel.doppler = qtrue;
	channel.oldDopplerScale = 4;
	S_PaintChannelFromADPCM( &channel, &sound, 1, SND_CHUNK_SIZE * 2 + 1, 0 );
	Check( numDecoded == 1 && decoded[0] == 0 && sfxScratchIndex == 0, "a scaled offset wraps" );

	/* painting over the end of the last chunk wraps too */
	Reset();
	channel.doppler = qfalse;
	S_PaintChannelFromADPCM( &channel, &sound, 2, SND_CHUNK_SIZE * 8 - 1, 0 );
	Check( numDecoded == 2 && decoded[0] == 1 && decoded[1] == 0 && sfxScratchIndex == 0, "painting wraps" );

	puts( "ADPCM paint regressions passed" );
	return 0;
}
