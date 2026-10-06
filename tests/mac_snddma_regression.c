/* Issues #264 and #5: the classic Mac Sound Manager backend.
 *
 * #264: mac_snddma.c queued every 128-frame buffer with an ExtSoundHeader
 * whose sampleRate was rate22khz (0x56EE8BA3, 22254.54545 Hz, the old Mac
 * hardware rate) while it told the mixer dma.speed = 22050.  The mixer
 * resamples every sound to dma.speed and counts time in dma.speed frames per
 * second off SNDDMA_GetDMAPos, so all audio played about 0.93% sharp and fast
 * and the DMA position ran ahead of the mixer's clock.
 *
 * #5: every Sound Manager result was ignored.  A failed SndDoCommand in
 * SNDDMA_Init left a channel open with nothing queued while S_Init went on as
 * if sound worked, and one at interrupt time stopped the sound without a
 * word; NewSndCallBackUPP and SndNewChannel failures were silent; a failed
 * SndDisposeChannel still freed the UPP the live channel calls through; a
 * second SNDDMA_Init leaked the open channel; a callback that ran as
 * SNDDMA_Shutdown began requeued onto the channel being disposed; and a quit
 * that skipped CL_Shutdown left the channel, with its interrupt-time callback
 * into the application, open at exit.
 *
 * The fixture #includes the real mac_snddma.c against mac_sound_fake.h and
 * plays the channel with a fake Sound Manager: each bufferCmd takes
 * numFrames / sampleRate seconds of simulated time, as Sound Manager 3.x does
 * (it rate-converts the header's rate to the output hardware), and each
 * callBackCmd then runs the callback through its UPP, which queues the next
 * chunk.  Channels and UPPs are heap blocks freed on disposal, so ASan
 * catches a call through a stale one, and the fake can fail any of the
 * calls.  Sys_Quit is the real one from mac_main.c, extracted by the runner. */
#include <setjmp.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../code/mac/mac_snddma.c"

dma_t	dma;

static int			failures;
static const char	*currentCase = "";

static void Check( int ok, const char *message ) {
	if ( !ok ) {
		fprintf( stderr, "FAIL: %s: %s\n", currentCase, message );
		failures++;
	}
}

/* ---- the engine ---- */

static int	inInterrupt;	/* a callBackCmd's callback is running */
static int	warnings;
static char	lastPrint[1024];

void QDECL Com_Printf( const char *fmt, ... ) {
	va_list	argptr;

	Check( !inInterrupt, "nothing prints at interrupt time" );
	va_start( argptr, fmt );
	vsnprintf( lastPrint, sizeof( lastPrint ), fmt, argptr );
	va_end( argptr );
	if ( strstr( lastPrint, "WARNING" ) ) {
		warnings++;
	}
}

/* ---- fake Sound Manager ---- */

#define	QUEUE_SIZE				128		/* stdQLength */
#define	notEnoughHardwareErr	-201
#define	queueFull				-203
#define	badChannel				-205

struct FakeSndCallBackUPP {
	SndCallBackProcPtr	proc;
};

typedef struct {
	SndChannel		chan;		/* first: the SndChannelPtr the backend gets */
	SndCommand		queue[QUEUE_SIZE];
	int				head, tail;
	int				disposing;
} FakeChannel;

static FakeChannel	*openChannel;
static FakeChannel	*zombieChannel;	/* SndDisposeChannel failed on it */
static int			uppsNew, uppsDisposed;
static int			channelsNew, channelsDisposed;
static int			doCommands;
static int			badCommands;
static int			callbacks;

/* failure injection */
static int			failNewUPP;
static OSErr		failNewChannel;
static int			failDoCommandAt;	/* the doCommands count that fails */
static OSErr		failDispose;
static int			callbackAsDisposeStarts;

SndCallBackUPP NewSndCallBackUPP( SndCallBackProcPtr userRoutine ) {
	SndCallBackUPP	upp;

	if ( failNewUPP ) {
		return NULL;
	}
	upp = malloc( sizeof( *upp ) );
	upp->proc = userRoutine;
	uppsNew++;
	return upp;
}

void DisposeSndCallBackUPP( SndCallBackUPP userUPP ) {
	Check( userUPP != NULL, "DisposeSndCallBackUPP gets a UPP" );
	free( userUPP );
	uppsDisposed++;
}

OSErr SndNewChannel( SndChannelPtr *chan, short synth, long init, SndCallBackUPP userRoutine ) {
	Check( synth == sampledSynth, "SndNewChannel gets sampledSynth" );
	Check( init == initStereo, "SndNewChannel gets initStereo" );
	Check( *chan == NULL, "SndNewChannel allocates the channel" );
	Check( userRoutine != NULL, "SndNewChannel gets the callback UPP" );
	Check( openChannel == NULL, "no second channel opens while one is open" );
	if ( failNewChannel ) {
		return failNewChannel;
	}
	openChannel = calloc( 1, sizeof( *openChannel ) );
	openChannel->chan.callBack = userRoutine;
	channelsNew++;
	*chan = &openChannel->chan;
	return 0;
}

OSErr SndDoCommand( SndChannelPtr chan, const SndCommand *cmd, Boolean noWait ) {
	FakeChannel	*fc = (FakeChannel *)chan;

	Check( noWait, "SndDoCommand never waits (it runs at interrupt time)" );
	if ( !fc || fc != openChannel || fc->disposing ) {
		badCommands++;
		return badChannel;
	}
	if ( ++doCommands == failDoCommandAt ) {
		return queueFull;
	}
	if ( fc->tail - fc->head >= QUEUE_SIZE ) {
		badCommands++;
		return queueFull;
	}
	fc->queue[fc->tail++ % QUEUE_SIZE] = *cmd;
	return 0;
}

static void RunCallback( FakeChannel *fc, SndCommand *cmd ) {
	inInterrupt = 1;
	fc->chan.callBack->proc( &fc->chan, cmd );
	inInterrupt = 0;
	callbacks++;
}

OSErr SndDisposeChannel( SndChannelPtr chan, Boolean quietNow ) {
	FakeChannel	*fc = (FakeChannel *)chan;
	SndCommand	cmd;
	int			drained;

	Check( fc != NULL && fc == openChannel, "SndDisposeChannel gets the open channel" );
	if ( !fc || fc != openChannel ) {
		return badChannel;
	}
	Check( quietNow, "SndDisposeChannel is quietNow, so no queued callBackCmd runs" );
	fc->disposing = 1;
	if ( callbackAsDisposeStarts ) {
		// the sound interrupt fires just as the disposal begins
		cmd.cmd = callBackCmd;
		cmd.param1 = 0;
		cmd.param2 = 0;
		RunCallback( fc, &cmd );
	}
	if ( failDispose ) {
		zombieChannel = fc;		// still playing, still calling back
		openChannel = NULL;
		return failDispose;
	}
	if ( !quietNow ) {
		// the Sound Manager plays out the queue first
		for ( drained = 0 ; fc->head < fc->tail && drained < 1000 ; drained++ ) {
			cmd = fc->queue[fc->head++ % QUEUE_SIZE];
			if ( cmd.cmd == callBackCmd ) {
				RunCallback( fc, &cmd );
			}
		}
		Check( fc->head == fc->tail, "the channel's queue drains (the disposal does not hang)" );
	}
	free( fc );
	openChannel = NULL;
	channelsDisposed++;
	return 0;
}

/* plays up to count commands off the open channel, returns how many */
static int Play( int count ) {
	SndCommand	cmd;
	int			played;

	for ( played = 0 ; played < count && openChannel && openChannel->head < openChannel->tail ; played++ ) {
		cmd = openChannel->queue[openChannel->head++ % QUEUE_SIZE];
		if ( cmd.cmd == callBackCmd ) {
			RunCallback( openChannel, &cmd );
		}
	}
	return played;
}

static void Reset( const char *name ) {
	currentCase = name;
	failNewUPP = 0;
	failNewChannel = 0;
	failDoCommandAt = 0;
	failDispose = 0;
	callbackAsDisposeStarts = 0;
	warnings = 0;
	lastPrint[0] = '\0';
	badCommands = 0;
}

static void CheckReleased( void ) {
	Check( openChannel == NULL, "no channel is open" );
	Check( channelsNew == channelsDisposed, "every channel opened is disposed" );
	Check( uppsNew == uppsDisposed, "every callback UPP is disposed" );
	Check( s_sndChan == NULL && s_callbackUPP == NULL, "the backend holds no channel or UPP" );
	Check( badCommands == 0, "every SndDoCommand went to an open channel" );
}

/* ---- Sys_Quit (mac_main.c) ---- */

static jmp_buf	exited;
static int		exitStatus;

static void Sys_ShutdownInput( void ) { }
static void Sys_ShutdownNetworking( void ) { }
static void Sys_DumpRetroLogs( const char *fileName ) { }
static void FakeExit( int status ) __attribute__(( noreturn ));
static void FakeExit( int status ) {
	exitStatus = status;
	longjmp( exited, 1 );
}

#define exit	FakeExit
#include "mac_quit_extracted.c"
#undef exit

static void Quit( void ) {
	exitStatus = -1;
	if ( !setjmp( exited ) ) {
		Sys_Quit();
		Check( 0, "Sys_Quit does not return" );
	}
	Check( exitStatus == 0, "Sys_Quit exits with 0" );
}

/* ---- #264: the playback rate ---- */

#define SIM_SECONDS	30

static void TestRate( void ) {
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

	Reset( "rate" );
	Check( SNDDMA_Init() == qtrue, "SNDDMA_Init succeeds" );
	Check( dma.channels == 2 && dma.samplebits == 16, "the backend is 16-bit stereo" );
	Check( dma.speed > 0, "dma.speed is set" );
	if ( failures || !openChannel ) {
		return;
	}

	now = 0;
	playedFrames = 0;
	dmaSamples = 0;
	buffers = 0;
	rateOk = 1;
	lastPos = SNDDMA_GetDMAPos();
	while ( now < SIM_SECONDS ) {
		if ( openChannel->head == openChannel->tail ) {
			Check( 0, "the channel queue ran dry" );
			break;
		}
		cmd = openChannel->queue[openChannel->head++ % QUEUE_SIZE];
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
			RunCallback( openChannel, &cmd );
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
	CheckReleased();
	Check( warnings == 0, "a clean run prints no warning" );
}

/* ---- #5: failures in SNDDMA_Init ---- */

static void InitFails( void ) {
	const char	*name = currentCase;

	Check( SNDDMA_Init() == qfalse, "SNDDMA_Init fails, so S_Init leaves sound off" );
	Check( warnings == 1, "SNDDMA_Init prints a warning" );
	CheckReleased();
	SNDDMA_Shutdown();		// S_Shutdown does not call it, but it must be harmless
	CheckReleased();

	// and the next snd_restart works
	Reset( name );
	Check( SNDDMA_Init() == qtrue, "SNDDMA_Init succeeds once the failure is gone" );
	Check( Play( 40 ) == 40, "the channel plays after a failed SNDDMA_Init" );
	SNDDMA_Shutdown();
	CheckReleased();
}

static void TestInitFailures( void ) {
	Reset( "NewSndCallBackUPP fails" );
	failNewUPP = 1;
	InitFails();

	Reset( "SndNewChannel fails" );
	failNewChannel = notEnoughHardwareErr;
	InitFails();

	Reset( "the first bufferCmd fails" );
	failDoCommandAt = doCommands + 1;
	InitFails();

	Reset( "the first callBackCmd fails" );
	failDoCommandAt = doCommands + 2;
	InitFails();
}

/* ---- #5: SndDoCommand fails at interrupt time ---- */

static void InterruptFails( const char *name, int which ) {
	int		pos;

	Reset( name );
	Check( SNDDMA_Init() == qtrue, "SNDDMA_Init succeeds" );
	Play( 20 );
	failDoCommandAt = doCommands + which;
	Play( 1000 );
	Check( openChannel && openChannel->head == openChannel->tail, "the chain of chunks stops" );
	Check( warnings == 0, "the callback itself does not print" );
	pos = SNDDMA_GetDMAPos();
	Check( warnings == 1 && strstr( lastPrint, "snd_restart" ) != NULL,
		"SNDDMA_GetDMAPos reports the error, with the way out" );
	Check( SNDDMA_GetDMAPos() == pos && warnings == 1, "the error is reported once and the position holds" );
	SNDDMA_Shutdown();
	CheckReleased();

	// snd_restart clears it
	Reset( name );
	Check( SNDDMA_Init() == qtrue, "snd_restart reopens the channel" );
	Check( Play( 40 ) == 40, "and it plays" );
	SNDDMA_GetDMAPos();
	Check( warnings == 0, "the old error is not reported again" );
	SNDDMA_Shutdown();
	CheckReleased();
}

static void TestInterruptFailures( void ) {
	InterruptFails( "a bufferCmd fails in the callback", 1 );
	InterruptFails( "a callBackCmd fails in the callback", 2 );
}

/* ---- #5: repeated snd_restart ---- */

#define RESTARTS	50

static void TestRestarts( void ) {
	int		i;
	int		uppsBefore, channelsBefore;
	int		callbacksBefore;
	int		failuresBefore;

	Reset( "repeated snd_restart" );
	failuresBefore = failures;
	uppsBefore = uppsNew;
	channelsBefore = channelsNew;
	for ( i = 0 ; i < RESTARTS ; i++ ) {
		// CL_Snd_Restart_f: S_Shutdown, then S_Init
		Check( SNDDMA_Init() == qtrue, "SNDDMA_Init succeeds" );
		Check( Play( 2 + i % 13 ) > 0, "the channel plays" );
		callbackAsDisposeStarts = i & 1;
		SNDDMA_Shutdown();
		callbackAsDisposeStarts = 0;
		CheckReleased();
		callbacksBefore = callbacks;
		SNDDMA_Shutdown();		// CL_Shutdown and Sys_Quit both get here
		Check( callbacks == callbacksBefore, "no callback runs after the dispose" );
		CheckReleased();
		if ( failures != failuresBefore ) {
			break;
		}
	}
	Check( uppsNew - uppsBefore == RESTARTS && channelsNew - channelsBefore == RESTARTS,
		"one channel and one UPP per cycle" );

	// SNDDMA_Init with the channel still open closes it first
	Reset( "SNDDMA_Init twice" );
	Check( SNDDMA_Init() == qtrue, "the first SNDDMA_Init succeeds" );
	Play( 7 );
	Check( SNDDMA_Init() == qtrue, "the second SNDDMA_Init succeeds" );
	Check( channelsNew - channelsDisposed == 1 && uppsNew - uppsDisposed == 1,
		"only one channel and one UPP stay open" );
	Check( Play( 40 ) == 40, "the new channel plays" );
	SNDDMA_Shutdown();
	CheckReleased();
	Check( warnings == 0, "restarts print no warning" );
}

/* ---- #5: quit ---- */

static void TestQuit( void ) {
	int		disposed;

	// a quit that skipped CL_Shutdown (Com_Quit_f during an error)
	Reset( "Sys_Quit with sound open" );
	Check( SNDDMA_Init() == qtrue, "SNDDMA_Init succeeds" );
	Play( 30 );
	Quit();
	CheckReleased();

	// the ordinary quit: CL_Shutdown's S_Shutdown, then Sys_Quit
	Reset( "Sys_Quit after S_Shutdown" );
	Check( SNDDMA_Init() == qtrue, "SNDDMA_Init succeeds" );
	Play( 30 );
	SNDDMA_Shutdown();
	disposed = channelsDisposed + uppsDisposed;
	Quit();
	Check( channelsDisposed + uppsDisposed == disposed, "Sys_Quit disposes nothing twice" );
	CheckReleased();

	// sound never started (s_initsound 0)
	Reset( "Sys_Quit without sound" );
	Quit();
	CheckReleased();
}

/* ---- #5: SndDisposeChannel fails ---- */

static void TestDisposeFailure( void ) {
	int			disposed;
	int			zombieUPPLive;
	int			tail;
	SndCommand	cmd;

	Reset( "SndDisposeChannel fails" );
	Check( SNDDMA_Init() == qtrue, "SNDDMA_Init succeeds" );
	Play( 20 );
	disposed = uppsDisposed;
	failDispose = badChannel;
	SNDDMA_Shutdown();
	Check( warnings == 1, "SNDDMA_Shutdown prints a warning" );
	zombieUPPLive = uppsDisposed == disposed;
	Check( zombieUPPLive, "the UPP of a channel that may still call back is not disposed" );
	Check( s_sndChan == NULL && s_callbackUPP == NULL, "the backend forgets the channel and its UPP" );

	// the channel calls back once more: through a live UPP, without requeuing
	if ( zombieChannel && zombieUPPLive ) {
		cmd.cmd = callBackCmd;
		cmd.param1 = 0;
		cmd.param2 = 0;
		zombieChannel->disposing = 0;
		openChannel = zombieChannel;
		tail = zombieChannel->tail;
		RunCallback( zombieChannel, &cmd );
		Check( zombieChannel->tail == tail, "the forgotten channel is not fed" );
		openChannel = NULL;
	}

	// a later SNDDMA_Shutdown or SNDDMA_Init must not dispose the forgotten UPP
	Reset( "SndDisposeChannel fails" );
	SNDDMA_Shutdown();
	Check( uppsDisposed == disposed, "a second SNDDMA_Shutdown leaves the forgotten UPP alone" );
	Check( SNDDMA_Init() == qtrue, "SNDDMA_Init opens a new channel" );
	Check( uppsDisposed == disposed, "SNDDMA_Init leaves the forgotten UPP alone" );
	SNDDMA_Shutdown();

	// free what the Mac would have leaked
	if ( zombieChannel ) {
		if ( zombieUPPLive ) {
			free( zombieChannel->chan.callBack );
			uppsDisposed++;
		}
		free( zombieChannel );
		zombieChannel = NULL;
		channelsDisposed++;
	}
	CheckReleased();
}

int main( void ) {
	TestRate();
	TestInitFailures();
	TestInterruptFailures();
	TestRestarts();
	TestQuit();
	TestDisposeFailure();

	if ( failures ) {
		fprintf( stderr, "mac_snddma_regression: %d failure(s)\n", failures );
		return 1;
	}
	printf( "mac_snddma_regression: passed (%d channels and %d UPPs opened and disposed, %d callbacks)\n",
		channelsNew, uppsNew, callbacks );
	return 0;
}
