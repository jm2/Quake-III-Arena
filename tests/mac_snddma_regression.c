/* Issue #264: the classic Mac Sound Manager backend must play at the rate it
 * reports to the mixer.
 *
 * mac_snddma.c queued every 128-frame buffer with an ExtSoundHeader whose
 * sampleRate was rate22khz (0x56EE8BA3, 22254.54545 Hz, the old Mac hardware
 * rate) while it told the mixer dma.speed = 22050.  The mixer resamples every
 * sound to dma.speed and counts time in dma.speed frames per second off
 * SNDDMA_GetDMAPos, so all audio played about 0.93% sharp and fast and the
 * DMA position ran ahead of the mixer's clock.
 *
 * The fixture #includes the real mac_snddma.c against mac_sound_fake.h and
 * plays the channel with a fake Sound Manager: each bufferCmd takes
 * numFrames / sampleRate seconds of simulated time, as Sound Manager 3.x does
 * (it rate-converts the header's rate to the output hardware), and each
 * callBackCmd then runs the callback, which queues the next chunk.  It checks
 * that every header's sampleRate is exactly dma.speed in Fixed, and that over
 * 30 simulated seconds the unwrapped SNDDMA_GetDMAPos advance matches the
 * dma.speed frames per second the mixer assumes, to within the one chunk the
 * position runs ahead of playback. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../code/mac/mac_snddma.c"

dma_t	dma;

static int	failures;

static void Check( int ok, const char *message ) {
	if ( !ok ) {
		fprintf( stderr, "FAIL: %s\n", message );
		failures++;
	}
}

/* ---- fake Sound Manager ---- */

#define QUEUE_SIZE	64
static SndChannel		fakeChannel;
static SndCommand		queue[QUEUE_SIZE];
static int				queueHead, queueTail;
static int				channelOpen;
static int				upps;
static int				badCommands;

SndCallBackUPP NewSndCallBackUPP( void (*userRoutine)( SndChannelPtr chan, SndCommand *cmd ) ) {
	upps++;
	return userRoutine;
}

void DisposeSndCallBackUPP( SndCallBackUPP userUPP ) {
	upps--;
}

OSErr SndNewChannel( SndChannelPtr *chan, short synth, long init, SndCallBackUPP userRoutine ) {
	Check( synth == sampledSynth, "SndNewChannel gets sampledSynth" );
	Check( init == initStereo, "SndNewChannel gets initStereo" );
	memset( &fakeChannel, 0, sizeof( fakeChannel ) );
	fakeChannel.callBack = userRoutine;
	queueHead = queueTail = 0;
	channelOpen = 1;
	*chan = &fakeChannel;
	return 0;
}

OSErr SndDoCommand( SndChannelPtr chan, const SndCommand *cmd, Boolean noWait ) {
	if ( chan != &fakeChannel || !channelOpen || queueTail - queueHead >= QUEUE_SIZE ) {
		badCommands++;
		return -1;
	}
	queue[queueTail++ % QUEUE_SIZE] = *cmd;
	return 0;
}

OSErr SndDisposeChannel( SndChannelPtr chan, Boolean quietNow ) {
	Check( chan == &fakeChannel && channelOpen, "SndDisposeChannel gets the open channel" );
	channelOpen = 0;
	return 0;
}

/* ---- simulation ---- */

#define SIM_SECONDS	30

int main( void ) {
	double			now;		/* simulated seconds */
	double			mixerFrames;
	long			playedFrames;
	long			dmaSamples;	/* unwrapped SNDDMA_GetDMAPos advance */
	int				lastPos, pos;
	int				buffers;
	int				rateOk;
	SndCommand		cmd;
	ExtSoundHeader	header;
	char			message[256];

	Check( SNDDMA_Init() == qtrue, "SNDDMA_Init succeeds" );
	Check( dma.channels == 2 && dma.samplebits == 16, "the backend is 16-bit stereo" );
	Check( dma.speed > 0, "dma.speed is set" );
	if ( failures ) {
		return 1;
	}

	now = 0;
	playedFrames = 0;
	dmaSamples = 0;
	buffers = 0;
	rateOk = 1;
	lastPos = SNDDMA_GetDMAPos();
	while ( now < SIM_SECONDS ) {
		if ( queueHead == queueTail ) {
			Check( 0, "the channel queue ran dry" );
			break;
		}
		cmd = queue[queueHead++ % QUEUE_SIZE];
		if ( cmd.cmd == bufferCmd ) {
			// the Sound Manager reads the header when it starts the buffer
			Check( cmd.param2 == (long)(int)(intptr_t)&s_sndHeader, "bufferCmd points at s_sndHeader" );
			header = s_sndHeader;
			Check( header.encode == extSH && header.numChannels == (unsigned long)dma.channels
				&& header.sampleSize == (unsigned short)dma.samplebits, "the header matches dma's format" );
			if ( header.sampleRate != ( (UnsignedFixed)dma.speed << 16 ) ) {
				if ( rateOk ) {
					snprintf( message, sizeof( message ),
						"bufferCmd sampleRate 0x%08lX (%.5f Hz) is not dma.speed %d (0x%08lX)",
						(unsigned long)header.sampleRate, header.sampleRate / 65536.0, dma.speed,
						(unsigned long)dma.speed << 16 );
					Check( 0, message );
				}
				rateOk = 0;
			}
			if ( header.sampleRate == 0 || header.numFrames == 0 ) {
				Check( 0, "the header has a rate and frames" );
				break;
			}
			now += header.numFrames * 65536.0 / header.sampleRate;
			playedFrames += header.numFrames;
			buffers++;
		} else if ( cmd.cmd == callBackCmd ) {
			fakeChannel.callBack( &fakeChannel, &cmd );
			pos = SNDDMA_GetDMAPos();
			Check( pos >= 0 && pos < dma.samples, "SNDDMA_GetDMAPos stays inside the ring" );
			dmaSamples += ( pos - lastPos + dma.samples ) % dma.samples;
			lastPos = pos;
		} else {
			Check( 0, "only bufferCmd and callBackCmd are queued" );
			break;
		}
	}

	mixerFrames = (double)dma.speed * now;
	printf( "%d buffers, %.4f s simulated: %ld frames played, DMA position advanced %ld frames, "
		"mixer expects %.0f frames at dma.speed %d\n",
		buffers, now, playedFrames, dmaSamples / dma.channels, mixerFrames, dma.speed );
	// the position is that of the next chunk queued, one chunk ahead of playback
	if ( fabs( dmaSamples / dma.channels - mixerFrames ) > SUBMISSION_CHUNK / dma.channels + 1 ) {
		snprintf( message, sizeof( message ),
			"after %.4f s the DMA position advanced %ld frames but the mixer counts %.0f (%+.3f%%)",
			now, dmaSamples / dma.channels, mixerFrames,
			100.0 * ( dmaSamples / dma.channels - mixerFrames ) / mixerFrames );
		Check( 0, message );
	}

	SNDDMA_Shutdown();
	Check( !channelOpen, "SNDDMA_Shutdown disposes the channel" );
	Check( upps == 0, "SNDDMA_Shutdown disposes the callback UPP" );
	Check( badCommands == 0, "every SndDoCommand went to the open channel" );

	if ( failures ) {
		fprintf( stderr, "mac_snddma_regression: %d failure(s)\n", failures );
		return 1;
	}
	printf( "mac_snddma_regression: passed\n" );
	return 0;
}
