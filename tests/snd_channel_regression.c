/* Issue #318: S_StartSound scans only s_channels, and the handle warnings print their messages. */
#include "../code/client/snd_dma.c"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#define TEST_SFX	16

clientStatic_t cls;
static cvar_t showCvar;
static char printed[1024];
static int now;

/** Fail when a channel pick or a warning differs. */
static void Check( int ok, const char *message ) {
	if ( !ok ) { fprintf( stderr, "Sound channel regression failed: %s (printed \"%s\")\n", message, printed ); exit( 1 ); }
}
/** Keep what the sound code prints so the warnings can be compared. */
void QDECL Com_Printf( const char *format, ... ) {
	va_list args;
	size_t used = strlen( printed );
	va_start( args, format );
	vsnprintf( printed + used, sizeof(printed) - used, format, args );
	va_end( args );
}
/** Clear the channel table for S_ChannelSetup. */
void Com_Memset( void *dest, const int val, const size_t count ) { memset( dest, val, count ); }
/** Ignore the channel manager's developer message. */
void QDECL Com_DPrintf( const char *format, ... ) { (void)format; }
/** Fail on any drop; every entity number here is valid. */
void QDECL Com_Error( int level, const char *format, ... ) { (void)level; (void)format; Check( 0, "unexpected engine error" ); }
/** Supply the sound clock the duplicate check compares against. */
int Com_Milliseconds( void ) { return now; }
/** Fail if a start tries to load; every test sound is marked in memory. */
qboolean S_LoadSound( sfx_t *sfx ) { (void)sfx; Check( 0, "unexpected sound load" ); return qfalse; }

/** Start one sound TICK ms after the previous one. */
static void Start( int entityNum, int entchannel, int sfxHandle, int tick ) {
	now += tick;
	printed[0] = 0;
	S_StartSound( NULL, entityNum, entchannel, sfxHandle );
}
/** Require a handle warning to print its full message. */
static void Warning( const char *expected ) {
	Check( !strcmp( printed, expected ), "handle warning lost its message" );
	printed[0] = 0;
}
/** Fill every channel with listener sounds, the last one (channel 0) from LASTENT on LASTCHAN. */
static void FillChannels( int lastEnt, int lastChan ) {
	int i;
	S_ChannelSetup();
	for ( i = 0; i < MAX_CHANNELS - 1; i++ ) Start( listener_number, CHAN_AUTO, i % 12, 50 );
	Start( lastEnt, lastChan, 12, 50 );
	Check( freelist == NULL && s_channels[0].entnum == lastEnt, "all channels busy" );
}

int main( void ) {
	static channel_t before[MAX_CHANNELS];
	vec3_t origin = {1, 2, 3}, velocity = {0, 0, 0};
	channel_t *ch;
	int i;

	s_soundStarted = 1;
	s_show = &showCvar;
	s_numSfx = TEST_SFX;
	for ( i = 0; i < TEST_SFX; i++ ) s_knownSfx[i].inMemory = qtrue;
	listener_number = 0;
	now = 1000;

	/* A normal start takes the free-list head (channel 95); the duplicate scan reads all 96 channels. */
	S_ChannelSetup();
	now += 50;
	S_StartSound( origin, 5, CHAN_WEAPON, 1 );
	ch = &s_channels[MAX_CHANNELS - 1];
	Check( ch->entnum == 5 && ch->thesfx == &s_knownSfx[1] && ch->entchannel == CHAN_WEAPON, "normal start channel" );
	Check( ch->allocTime == now && ch->startSample == START_SAMPLE_IMMEDIATE && ch->master_vol == 127, "normal start timing" );
	Check( ch->leftvol == 127 && ch->rightvol == 127 && !ch->doppler, "normal start volume" );
	Check( ch->fixed_origin && VectorCompare( ch->origin, origin ) && s_knownSfx[1].lastTimeUsed == now, "normal start origin" );
	Check( s_channels[MAX_CHANNELS - 2].thesfx == NULL, "one channel per start" );

	/* The same sound 20 ms later is the double start the scan exists to catch, on the last channel. */
	Start( 5, CHAN_WEAPON, 1, 20 );
	Check( s_channels[MAX_CHANNELS - 2].thesfx == NULL, "double start on the last channel played" );
	/* Five copies may play on one entity; the sixth is refused. */
	for ( i = 0; i < 4; i++ ) Start( 5, CHAN_WEAPON, 1, 50 );
	Check( s_channels[MAX_CHANNELS - 5].thesfx == &s_knownSfx[1], "fifth copy refused" );
	Start( 5, CHAN_WEAPON, 1, 50 );
	Check( s_channels[MAX_CHANNELS - 6].thesfx == NULL, "sixth copy played" );

	/* All channels busy with listener sounds: the oldest (channel 95) is replaced. */
	FillChannels( listener_number, CHAN_AUTO );
	Start( listener_number, CHAN_ITEM, 13, 50 );
	ch = &s_channels[MAX_CHANNELS - 1];
	Check( ch->thesfx == &s_knownSfx[13] && ch->allocTime == now && ch->entchannel == CHAN_ITEM, "oldest listener channel not replaced" );
	Check( !printed[0], "busy listener start printed" );

	/* All channels busy and channel 0 not the listener's: nothing may be replaced. */
	FillChannels( 7, CHAN_ANNOUNCER );
	memcpy( before, s_channels, sizeof(before) );
	Start( 7, CHAN_AUTO, 13, 50 );
	Check( !strcmp( printed, "dropping sound\n" ), "busy start not dropped" );
	Check( !memcmp( before, s_channels, sizeof(before) ), "dropped start changed a channel" );

	/* Every handle warning prints its message, not just the color code. */
	printed[0] = 0;
	S_StartSound( NULL, 0, CHAN_AUTO, TEST_SFX );
	Warning( S_COLOR_YELLOW "S_StartSound: handle 16 out of range\n" );
	S_StartLocalSound( -1, CHAN_LOCAL_SOUND );
	Warning( S_COLOR_YELLOW "S_StartLocalSound: handle -1 out of range\n" );
	S_AddLoopingSound( 0, origin, velocity, TEST_SFX );
	Warning( S_COLOR_YELLOW "S_AddLoopingSound: handle 16 out of range\n" );
	S_AddRealLoopingSound( 0, origin, velocity, -1 );
	Warning( S_COLOR_YELLOW "S_AddRealLoopingSound: handle -1 out of range\n" );

	puts( "Sound channel regressions passed (issue #318)" );
	return 0;
}
