/* Issue #30: with stereo enabled, SCR_UpdateScreen began a frame for each eye
 * (SCR_DrawScreenField( STEREO_LEFT ), then STEREO_RIGHT) but drew the cgame,
 * cinematic, UI and console only once, after both, with STEREO_CENTER. A
 * debugging commit had moved that drawing out of SCR_DrawScreenField. Retail
 * draws a whole field per eye, each after its own BeginFrame and with its own
 * eye, and ends the frame once.
 * The runner extracts the real SCR_DrawScreenField and SCR_UpdateScreen from
 * code/client/cl_scrn.c verbatim (with a #line so reports name that file) into
 * scr_stereo_bodies.c, included below. The renderer, UI VM, cgame and console
 * calls they make are replaced here to record each call as a word in a log,
 * which every case compares with the sequence retail draws. */
#include "../code/client/client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

clientStatic_t	cls;
refexport_t		re;
vm_t			*uivm;
qboolean		scr_initialized;
cvar_t			*cl_timegraph;
cvar_t			*cl_debuggraph;
cvar_t			*cl_debugMove;
cvar_t			*com_speeds;
int				time_frontend;
int				time_backend;
vec4_t			g_color_table[8];

static cvar_t	timegraph, debuggraph, debugMove, speeds;
static char		uiToken;		/* uivm only has to be non-NULL */
static int		uiFullscreen;
static char		calls[4096];

static void Fail( const char *what ) {
	fprintf( stderr, "screen stereo regression failed: %s\n", what );
	exit( 1 );
}

static void Record( const char *word ) {
	size_t length = strlen( calls );

	if ( length + strlen( word ) + 2 > sizeof( calls ) ) {
		Fail( "the call log overflowed" );
	}
	if ( length ) {
		calls[length++] = ' ';
	}
	strcpy( calls + length, word );
}

static const char *Eye( stereoFrame_t frame ) {
	switch ( frame ) {
	case STEREO_CENTER: return "C";
	case STEREO_LEFT: return "L";
	case STEREO_RIGHT: return "R";
	}
	Fail( "a stereo frame that is not center, left or right" );
	return "";
}

void QDECL Com_Error( int code, const char *fmt, ... ) {
	(void)code;
	fprintf( stderr, "screen stereo regression: unexpected Com_Error: %s\n", fmt );
	exit( 1 );
}

/* renderer */
static void Fake_BeginFrame( stereoFrame_t stereoFrame ) {
	char word[16];

	/* RE_BeginFrame (code/renderer/tr_cmds.c) refuses any other eye */
	if ( cls.glconfig.stereoEnabled ? stereoFrame == STEREO_CENTER : stereoFrame != STEREO_CENTER ) {
		Fail( "BeginFrame got an eye the renderer's stereo setting refuses" );
	}
	snprintf( word, sizeof( word ), "begin:%s", Eye( stereoFrame ) );
	Record( word );
}

static void Fake_EndFrame( int *frontEndMsec, int *backEndMsec ) {
	if ( speeds.integer ? ( frontEndMsec != &time_frontend || backEndMsec != &time_backend )
		: ( frontEndMsec || backEndMsec ) ) {
		Fail( "EndFrame got the wrong timing pointers for com_speeds" );
	}
	Record( "end" );
}

static void Fake_SetColor( const float *rgba ) {
	(void)rgba;
}

static void Fake_DrawStretchPic( float x, float y, float w, float h,
	float s1, float t1, float s2, float t2, qhandle_t hShader ) {
	(void)s1; (void)t1; (void)s2; (void)t2; (void)hShader;
	if ( x != 0 || y != 0 || w != cls.glconfig.vidWidth || h != cls.glconfig.vidHeight ) {
		Fail( "the side clear covers the whole screen" );
	}
	Record( "clear" );
}

/* UI VM */
int VM_CallArgs( vm_t *vm, int callNum, const int *args, int argCount ) {
	char word[32];

	if ( vm != uivm || !vm ) {
		Fail( "VM_Call goes to the UI VM" );
	}
	switch ( callNum ) {
	case UI_IS_FULLSCREEN:
		return uiFullscreen;
	case UI_REFRESH:
		if ( argCount != 1 || args[0] != cls.realtime ) {
			Fail( "UI_REFRESH gets cls.realtime" );
		}
		Record( "ui" );
		return 0;
	case UI_DRAW_CONNECT_SCREEN:
		if ( argCount != 1 ) {
			Fail( "UI_DRAW_CONNECT_SCREEN gets one argument" );
		}
		snprintf( word, sizeof( word ), "connect:%d", args[0] );
		Record( word );
		return 0;
	case UI_SET_ACTIVE_MENU:
		if ( argCount != 1 || args[0] != UIMENU_MAIN ) {
			Fail( "UI_SET_ACTIVE_MENU forces the main menu" );
		}
		Record( "menu" );
		return 0;
	}
	Fail( "an unexpected UI VM call" );
	return 0;
}

/* the rest of the client */
void CL_CGameRendering( stereoFrame_t stereo ) {
	char word[16];

	snprintf( word, sizeof( word ), "cgame:%s", Eye( stereo ) );
	Record( word );
}

void SCR_DrawCinematic( void ) {
	Record( "cinematic" );
}

void S_StopAllSounds( void ) {
	Record( "stopsounds" );
}

void Con_DrawConsole( void ) {
	Record( "console" );
}

void SCR_DrawDemoRecording( void ) {
	Record( "demo" );
}

void SCR_DrawDebugGraph( void ) {
	Record( "graph" );
}

#include "scr_stereo_bodies.c"

typedef struct {
	const char		*name;
	qboolean		stereo;
	connstate_t		state;
	int				ui;			/* 0: no UI VM, 1: UI VM, 2: fullscreen UI */
	int				keyCatchers;
	int				wide;		/* wider than 4:3, so the sides are cleared */
	int				debugGraph;
	int				speeds;
	qboolean		initialized;
	const char		*expected;
} screenCase_t;

static const screenCase_t cases[] = {
	/* one centered field without stereo */
	{ "mono active", qfalse, CA_ACTIVE, 1, 0, 0, 0, 0, qtrue,
		"begin:C cgame:C demo console end" },
	{ "mono active, wide, com_speeds", qfalse, CA_ACTIVE, 1, 0, 1, 0, 1, qtrue,
		"begin:C cgame:C demo console end" },
	{ "mono loading, wide", qfalse, CA_LOADING, 1, 0, 1, 0, 0, qtrue,
		"begin:C clear cgame:C ui connect:1 console end" },
	{ "mono primed", qfalse, CA_PRIMED, 1, 0, 0, 0, 0, qtrue,
		"begin:C cgame:C ui connect:1 console end" },
	{ "mono connecting", qfalse, CA_CONNECTING, 1, 0, 0, 0, 0, qtrue,
		"begin:C ui connect:0 console end" },
	{ "mono disconnected", qfalse, CA_DISCONNECTED, 1, 0, 0, 0, 0, qtrue,
		"begin:C stopsounds menu console end" },
	{ "mono cinematic", qfalse, CA_CINEMATIC, 1, 0, 0, 0, 0, qtrue,
		"begin:C cinematic console end" },
	{ "mono fullscreen menu over a game", qfalse, CA_ACTIVE, 2, KEYCATCH_UI, 0, 0, 0, qtrue,
		"begin:C ui console end" },
	{ "mono menu over a game, debug graph", qfalse, CA_ACTIVE, 1, KEYCATCH_UI, 0, 1, 0, qtrue,
		"begin:C cgame:C demo ui console graph end" },
	/* without a UI VM (startup, vid_restart) the console still draws */
	{ "mono without a UI VM", qfalse, CA_DISCONNECTED, 0, KEYCATCH_UI, 0, 0, 0, qtrue,
		"begin:C console end" },
	{ "mono before SCR_Init", qfalse, CA_ACTIVE, 1, 0, 0, 0, 0, qfalse,
		"" },

	/* a whole field per eye, left then right, and one EndFrame */
	{ "stereo active", qtrue, CA_ACTIVE, 1, 0, 0, 0, 0, qtrue,
		"begin:L cgame:L demo console begin:R cgame:R demo console end" },
	{ "stereo active, com_speeds", qtrue, CA_ACTIVE, 1, 0, 0, 0, 1, qtrue,
		"begin:L cgame:L demo console begin:R cgame:R demo console end" },
	{ "stereo loading, wide", qtrue, CA_LOADING, 1, 0, 1, 0, 0, qtrue,
		"begin:L clear cgame:L ui connect:1 console "
		"begin:R clear cgame:R ui connect:1 console end" },
	{ "stereo primed", qtrue, CA_PRIMED, 1, 0, 0, 0, 0, qtrue,
		"begin:L cgame:L ui connect:1 console begin:R cgame:R ui connect:1 console end" },
	{ "stereo connected", qtrue, CA_CONNECTED, 1, 0, 0, 0, 0, qtrue,
		"begin:L ui connect:0 console begin:R ui connect:0 console end" },
	{ "stereo disconnected", qtrue, CA_DISCONNECTED, 1, 0, 0, 0, 0, qtrue,
		"begin:L stopsounds menu console begin:R stopsounds menu console end" },
	{ "stereo cinematic, wide", qtrue, CA_CINEMATIC, 1, 0, 1, 0, 0, qtrue,
		"begin:L clear cinematic console begin:R clear cinematic console end" },
	{ "stereo fullscreen menu over a game", qtrue, CA_ACTIVE, 2, KEYCATCH_UI, 0, 0, 0, qtrue,
		"begin:L ui console begin:R ui console end" },
	{ "stereo menu over a game, debug graph", qtrue, CA_ACTIVE, 1, KEYCATCH_UI, 0, 1, 0, qtrue,
		"begin:L cgame:L demo ui console graph begin:R cgame:R demo ui console graph end" },
	{ "stereo without a UI VM", qtrue, CA_DISCONNECTED, 0, KEYCATCH_UI, 0, 0, 0, qtrue,
		"begin:L console begin:R console end" },
};

int main( void ) {
	size_t i;
	int failed = 0;

	cl_timegraph = &timegraph;
	cl_debuggraph = &debuggraph;
	cl_debugMove = &debugMove;
	com_speeds = &speeds;
	re.BeginFrame = Fake_BeginFrame;
	re.EndFrame = Fake_EndFrame;
	re.SetColor = Fake_SetColor;
	re.DrawStretchPic = Fake_DrawStretchPic;

	for ( i = 0; i < sizeof( cases ) / sizeof( cases[0] ); i++ ) {
		const screenCase_t *c = &cases[i];

		memset( &cls, 0, sizeof( cls ) );
		cls.state = c->state;
		cls.keyCatchers = c->keyCatchers;
		cls.realtime = 12345 + (int)i;
		cls.glconfig.stereoEnabled = c->stereo;
		cls.glconfig.vidWidth = c->wide ? 1280 : 640;
		cls.glconfig.vidHeight = c->wide ? 720 : 480;
		uivm = c->ui ? (vm_t *)&uiToken : NULL;
		uiFullscreen = c->ui == 2;
		debuggraph.integer = c->debugGraph;
		speeds.integer = c->speeds;
		scr_initialized = c->initialized;
		calls[0] = '\0';

		SCR_UpdateScreen();

		if ( strcmp( calls, c->expected ) ) {
			fprintf( stderr, "%s:\n  expected: %s\n  drawn:    %s\n", c->name, c->expected, calls );
			failed++;
		}
	}
	if ( failed ) {
		fprintf( stderr, "screen stereo regression failed: %d of %d cases\n",
			failed, (int)( sizeof( cases ) / sizeof( cases[0] ) ) );
		return 1;
	}
	printf( "screen stereo regression passed: %d cases\n", (int)( sizeof( cases ) / sizeof( cases[0] ) ) );
	return 0;
}
