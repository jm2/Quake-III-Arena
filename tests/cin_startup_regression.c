/* Issue #14: the startup idlogo/intro cinematics play as in retail 1.32c.
 *
 * The real cl_cin.c, cmd.c, cvar.c, files.c and unzip.c run together: the
 * movies stream out of a pk3 through the command buffer, the "cinematic"
 * console command and the nextmap chain, with a modelled zone that is fatal
 * when it runs out, as common.c's is.  Each movie's frames and audio samples
 * are counted independently from the bytes of its pk3 entry. */
#include "../code/client/cl_cin.c"

#define STREAM_HANDLE_ZONE	(128 * 1024)	/* unz_s, entry reader, 64 KiB read buffer, inflate */
#define INFLATE_BLOCK_ZONE	(8 * 1024)		/* code lengths and codes, per deflate block */
#define MOVIE_ZONE_FREE		(1024 * 1024)	/* less than any whole startup movie */
#define MAX_PLAYS			8

/* cin_startup_fs.c */
extern long cinZoneBytes, cinZonePeak, cinZoneBudget;
void CinFail( const char *message );
void CinMount( const char *pk3 );
byte *CinEntryBytes( const char *name, long *size );
qboolean CinAllFilesClosed( void );

cvar_t *com_timescale, *cl_inGameVideo;
clientStatic_t cls;
glconfig_t glConfig;
vm_t *uivm;
refexport_t re;
int s_paintedtime, s_rawend, s_soundtime;

typedef struct {
	char name[MAX_QPATH];
	int handle, vqFrames, samples, rawSamples, dirtyDraws, decoded;
	long size, played, zoneBefore, zonePeak;
	qboolean finished;
} play_t;
static play_t plays[MAX_PLAYS];
static int numPlays, uiCinematics, msec;
static play_t *current;

static void Fail( const char *message ) { CinFail( message ); }
static void Check( int condition, const char *message ) { if ( !condition ) Fail( message ); }

/* Wall time keeps moving while the console command waits for the first frame. */
int CL_ScaledMilliseconds( void ) { return msec++; }
void Con_Close( void ) {}
void S_StopAllSounds( void ) {}
void S_Update( void ) {}
void S_RawSamples( int samples, int rate, int width, int channels, const byte *data, float volume ) {
	(void)data;
	Check( current && rate == 22050 && width == 2 && (channels == 1 || channels == 2) && volume == 1.0f,
		   "cinematic audio format" );
	current->rawSamples += samples;
}
void SCR_AdjustFrom640( float *x, float *y, float *w, float *h ) { (void)x; (void)y; (void)w; (void)h; }
static void DrawStretchRaw( int x, int y, int w, int h, int cols, int rows, const byte *data, int client, qboolean dirty ) {
	(void)x; (void)y; (void)w; (void)h; (void)data; (void)client;
	Check( current && cols > 0 && rows > 0, "cinematic draw size" );
	if ( dirty ) current->dirtyDraws++;
}
#ifdef VM_Call
int VM_CallArgs( vm_t *vm, int command, const int *args, int count ) { (void)vm; (void)command; (void)args; (void)count; return 0; }
#else
int QDECL VM_Call( vm_t *vm, int command, ... ) { (void)vm; (void)command; return 0; }
#endif

/* Count VQ frames and decoded audio samples straight from the chunk stream. */
static void CountChunks( const byte *p, const byte *end, int count, play_t *play ) {
	while ( count-- && end - p >= 8 ) {
		unsigned id = p[0] | p[1] << 8, flags = p[6] | p[7] << 8;
		unsigned long size = p[2] | p[3] << 8 | p[4] << 16 | (unsigned long)p[5] << 24;
		p += 8;
		Check( size <= (unsigned long)(end - p), "reference chunk fits" );
		if ( id == ROQ_QUAD_VQ ) play->vqFrames++;
		else if ( id == ZA_SOUND_MONO ) play->samples += size;
		else if ( id == ZA_SOUND_STEREO ) play->samples += size / 2;
		else if ( id == ROQ_PACKET ) CountChunks( p, p + size, flags, play );
		p += size;
	}
}

/* CL_Init's "cinematic" command, recording what each run of it plays. */
static void Cinematic_f( void ) {
	byte *bytes;
	Check( numPlays < MAX_PLAYS, "play table" );
	current = &plays[numPlays++];
	memset( current, 0, sizeof(*current) );
	Com_sprintf( current->name, sizeof(current->name), "video/%s", Cmd_Argv( 1 ) );
	bytes = CinEntryBytes( current->name, &current->size );
	if ( bytes ) CountChunks( bytes + 8, bytes + current->size, -1, current );
	free( bytes );
	current->zoneBefore = cinZonePeak = cinZoneBytes;
	cinZoneBudget = cinZoneBytes + MOVIE_ZONE_FREE;
	CL_PlayCinematic_f();
	current->handle = CL_handle;
}
/* CL_Disconnect_f's cinematic stop; CL_KeyEvent turns any key during a cinematic into ESC, which runs it. */
static void Disconnect_f( void ) { SCR_StopCinematic(); }
static void UICinematics_f( void ) { uiCinematics++; }

/* Com_Init's default action when no + commands are given (common.c, unchanged from retail). */
static void DefaultStartupAction( void ) {
	cvar_t *com_introPlayed = Cvar_Get( "com_introplayed", "0", CVAR_ARCHIVE );
	Cbuf_AddText ("cinematic idlogo.RoQ\n");
	if( !com_introPlayed->integer ) {
		Cvar_Set( com_introPlayed->name, "1" );
		Cvar_Set( "nextmap", "cinematic intro.RoQ" );
	}
}

/* Com_Frame: the command buffer, then CL_Frame's SCR_RunCinematic and the screen update. */
static void Frames( int stopAfter ) {
	int frame;
	for ( frame = 0; frame < 100000; frame++ ) {
		play_t *before = current;
		Cbuf_Execute();
		if ( cls.state != CA_CINEMATIC && current == before && (!current || current->finished) ) return;
		if ( current && !current->finished ) {
			int h = current->handle;
			SCR_RunCinematic();
			if ( h >= 0 && cinTable[h].numQuads > current->decoded ) current->decoded = cinTable[h].numQuads;
			if ( cls.state == CA_CINEMATIC ) SCR_DrawCinematic();
			if ( cinZonePeak - current->zoneBefore > current->zonePeak ) current->zonePeak = cinZonePeak - current->zoneBefore;
			if ( stopAfter && current->decoded >= stopAfter && cls.state == CA_CINEMATIC ) {
				stopAfter = 0;
				Disconnect_f();
			}
			if ( h < 0 || CL_handle != h || !cinTable[h].fileName[0] ) {
				current->played = h >= 0 ? cinTable[h].RoQPlayed : 0;
				current->finished = qtrue;
				Check( cinZoneBytes == current->zoneBefore, "a finished movie returns its zone memory" );
			}
		}
		msec += 16;
	}
	Fail( "startup cinematics never finished" );
}

static void CheckPlayedThrough( const play_t *play, const char *name ) {
	char text[256];
	Com_sprintf( text, sizeof(text), "video/%s", name );
	if ( Q_stricmp( play->name, text ) ) { fprintf( stderr, "%s instead of %s\n", play->name, text ); Fail( "wrong movie" ); }
	if ( play->handle < 0 ) { fprintf( stderr, "%s\n", name ); Fail( "startup cinematic was not played" ); }
	printf( "  %-12s %7ld bytes, %4d frames, %7d samples, %4d dirty draws, peak zone +%ld\n",
			name, play->size, play->decoded, play->rawSamples, play->dirtyDraws, play->zonePeak );
	Check( play->played == play->size, "movie streamed to its last byte" );
	Check( play->vqFrames > 0 && play->decoded == play->vqFrames, "every VQ frame decoded, as retail" );
	Check( play->rawSamples == play->samples, "every audio sample submitted" );
	Check( play->dirtyDraws >= play->vqFrames - 1, "every frame reached the screen" );
	Check( play->zonePeak <= STREAM_HANDLE_ZONE + INFLATE_BLOCK_ZONE, "zone use stays one pk3 stream" );
}

static void Launch( const char *introPlayed, int stopAfter ) {
	Cvar_Set( "com_introplayed", introPlayed );
	Cvar_Set( "nextmap", "" );
	memset( &cls, 0, sizeof(cls) );
	numPlays = uiCinematics = 0;
	current = NULL;
	DefaultStartupAction();
	Check( !strcmp( Cvar_VariableString( "com_introplayed" ), "1" ), "com_introplayed is 1 after startup" );
	Frames( stopAfter );
	Check( numPlays > 0 && plays[0].handle >= 0, "startup cinematic idlogo.RoQ was not played" );
	Check( cls.state == CA_DISCONNECTED && CL_handle == -1, "startup leaves the cinematic for the menu" );
	Check( !Cvar_VariableString( "nextmap" )[0], "nextmap is consumed" );
	Check( CinAllFilesClosed(), "every movie closes its pk3 stream" );
	Check( !(Cvar_Flags( "com_introplayed" ) & CVAR_ROM) && (Cvar_Flags( "com_introplayed" ) & CVAR_ARCHIVE),
		   "com_introplayed is archived" );
}

int main( int argc, char **argv ) {
	static cvar_t timescale, video;
	int handle, i;
	if ( argc < 2 ) Fail( "usage: <pk3> [real]" );
	timescale.value = 1;
	video.integer = 1;
	com_timescale = &timescale;
	cl_inGameVideo = &video;
	glConfig.maxTextureSize = 1024;
	re.DrawStretchRaw = DrawStretchRaw;
	CinMount( argv[1] );
	Cmd_AddCommand( "cinematic", Cinematic_f );
	Cmd_AddCommand( "disconnect", Disconnect_f );
	Cmd_AddCommand( "ui_cinematics", UICinematics_f );

	if ( argc > 2 && !strcmp( argv[2], "real" ) ) {
		/* The demo pak0 has idlogo.RoQ only; retail startup plays it, then the
		 * missing intro.RoQ fails to open and the menu comes up. */
		Launch( "0", 0 );
		Check( numPlays == 2, "first launch asks for idlogo, then intro" );
		CheckPlayedThrough( &plays[0], "idlogo.RoQ" );
		Check( plays[1].handle == -1 && !Q_stricmp( plays[1].name, "video/intro.RoQ" ), "missing intro.RoQ is skipped" );
		Launch( "1", 0 );
		Check( numPlays == 1, "later launches play idlogo only" );
		CheckPlayedThrough( &plays[0], "idlogo.RoQ" );
		puts( "real idlogo.RoQ startup cinematic passed (issue #14)" );
		return 0;
	}

	/* First launch: idlogo, then intro through nextmap, then the menu. */
	Launch( "0", 0 );
	Check( numPlays == 2, "first launch plays idlogo, then intro" );
	CheckPlayedThrough( &plays[0], "idlogo.RoQ" );
	CheckPlayedThrough( &plays[1], "intro.RoQ" );
	/* Later launches: com_introplayed is archived as 1, so idlogo only. */
	Launch( "1", 0 );
	Check( numPlays == 1, "later launches play idlogo only" );
	CheckPlayedThrough( &plays[0], "idlogo.RoQ" );
	/* A key press during idlogo stops it, and retail still runs nextmap's intro. */
	Launch( "0", 5 );
	Check( numPlays == 2 && plays[0].handle >= 0 && plays[0].decoded == 5 && plays[0].played < plays[0].size,
		   "idlogo stops early" );
	CheckPlayedThrough( &plays[1], "intro.RoQ" );
	/* The q3_ui cinematics menu replays idlogo and returns to the menu. */
	memset( &cls, 0, sizeof(cls) );
	numPlays = 0;
	current = NULL;
	Cvar_Set( "nextmap", "ui_cinematics 0" );
	Cbuf_ExecuteText( EXEC_APPEND, "disconnect; cinematic idlogo.RoQ\n" );
	Frames( 0 );
	Check( numPlays == 1 && uiCinematics == 1 && cls.state == CA_DISCONNECTED, "menu replay returns to ui_cinematics" );
	CheckPlayedThrough( &plays[0], "idlogo.RoQ" );
	/* A mod's movie whose name contains "intro" plays and loops like any other. */
	current = &plays[0];
	memset( current, 0, sizeof(*current) );
	current->zoneBefore = cinZoneBytes;
	handle = CIN_PlayCinematic( "myintro.RoQ", 0, 0, 64, 64, CIN_loop );
	Check( handle >= 0, "a mod movie named *intro* plays" );
	for ( i = 0; i < 200; i++, msec += 16 ) Check( CIN_RunCinematic( handle ) == FMV_PLAY, "looping mod movie keeps playing" );
	Check( CIN_StopCinematic( handle ) == FMV_EOF, "mod movie stops" );
	Check( cinZoneBytes == plays[0].zoneBefore, "a stopped mod movie returns its zone memory" );
	puts( "startup idlogo/intro cinematic regressions passed (issue #14)" );
	return 0;
}
