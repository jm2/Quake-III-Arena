/* Issues #263 and #10: classic Mac startup initializes the Toolbox itself and
 * keeps the Retro68 console window closed unless it is wanted.
 *
 * #263: Retro68's startup code initializes no Toolbox manager.  The port used
 * to rely on libRetroConsole, whose first printf ran InitGraf, InitFonts,
 * InitWindows and InitMenus.  main must now run id's InitMacStuff sequence
 * (MaxApplZone, MoreMasters, InitGraf, InitFonts, FlushEvents, InitWindows,
 * InitMenus, TEInit, InitDialogs, InitCursor) before it writes anything, calls
 * any Sys_* function or reads the keyboard with GetKeys.
 *
 * #10: the console window opens only when output is wanted (viewlog, the
 * Shift prompt at launch, or errors on stderr).  Default startup must not open
 * it, and its log lines must still reach the crash-dump ring.
 *
 * The runner extracts the real main, Sys_InitToolbox, Sys_LogPrintf,
 * Sys_AppendStartupText, Sys_ReadStartupFile and Sys_StartupError
 * (mac_main.c) and Sys_ConsoleWanted, Sys_ShowConsole and Sys_Print
 * (mac_console.c).  The Toolbox calls are fakes that log their order.  stdio
 * goes through ConsoleWrite and ConsoleRead, which model mac_consolehooks.cc:
 * output creates the window only when Sys_ConsoleWanted accepts it, and input
 * always does.  The window may only be created after the Toolbox is up,
 * because mac_consolehooks.cc no longer initializes it. */
#include "../code/game/q_shared.h"
#include "../code/qcommon/qcommon.h"
#undef Sys_LogPrintf	/* the Mac build has a real one (macintosh) */
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- fake Toolbox ---- */

#define MAX_EVENTS	256
static const char	*events[MAX_EVENTS];
static int			numEvents;
static int			toolboxReady;	/* InitCursor has run */
static int			shiftHeld;

static int EventIndex( const char *name );

static void Event( const char *name ) {
	if ( numEvents < MAX_EVENTS ) {
		events[numEvents++] = name;
	}
}

typedef unsigned int	KeyMap[4];
typedef struct { short what; } EventRecord;
#define everyEvent		( (short)0xFFFF )
static struct { void *thePort; } qd;

static void MaxApplZone( void ) { Event( "MaxApplZone" ); }
static void MoreMasters( void ) { Event( "MoreMasters" ); }
static void Check( int ok, const char *message );
static void InitGraf( void *globalPtr ) {
	Check( globalPtr == &qd.thePort, "InitGraf is passed &qd.thePort" );
	Event( "InitGraf" );
}
static void InitFonts( void ) { Event( "InitFonts" ); }
static void FlushEvents( short mask, short stop ) {
	Check( mask == everyEvent && stop == 0, "FlushEvents( everyEvent, 0 )" );
	Event( "FlushEvents" );
}
static void InitWindows( void ) { Event( "InitWindows" ); }
static void InitMenus( void ) { Event( "InitMenus" ); }
static void TEInit( void ) { Event( "TEInit" ); }
static void InitDialogs( void *ignored ) { Event( "InitDialogs" ); }
static void InitCursor( void ) { Event( "InitCursor" ); toolboxReady = 1; }
static int EventAvail( short mask, EventRecord *event ) {
	Event( "EventAvail" );
	event->what = 0;
	return 0;
}
static void GetKeys( KeyMap keys ) {
	Event( "GetKeys" );
	memset( keys, 0, sizeof( KeyMap ) );
	if ( shiftHeld ) {
		( (unsigned char *)keys )[7] = 0x01;	/* key code 0x38 */
	}
}

/* ---- the console window, as mac_consolehooks.cc ---- */

int Sys_ConsoleWanted( int fd );

static int			windowOpen;
static char			windowText[16384];
static char			ring[16384];		/* what Sys_LogRecord kept */
static int			outputBeforeToolbox;
static const char	*typed;				/* the line typed into the window */

static void OpenWindow( void ) {
	Check( toolboxReady, "the console window is only created after the Toolbox is initialized" );
	Event( "console window" );
	windowOpen = 1;
}

static void ConsoleWrite( int fd, const char *text ) {
	if ( !toolboxReady ) {
		outputBeforeToolbox = 1;
	}
	Event( "output" );
	if ( !windowOpen ) {
		if ( !Sys_ConsoleWanted( fd ) ) {
			return;
		}
		OpenWindow();
	}
	Q_strcat( windowText, sizeof( windowText ), text );
}

static void ConsoleRead( void ) {
	if ( !windowOpen ) {
		OpenWindow();
	}
}

static int FakePrintf( const char *fmt, ... ) {
	char	text[4096];
	va_list	argptr;

	va_start( argptr, fmt );
	vsnprintf( text, sizeof( text ), fmt, argptr );
	va_end( argptr );
	ConsoleWrite( 1, text );
	return strlen( text );
}

static int FakeFprintf( FILE *f, const char *fmt, ... ) {
	char	text[4096];
	va_list	argptr;

	va_start( argptr, fmt );
	vsnprintf( text, sizeof( text ), fmt, argptr );
	va_end( argptr );
	ConsoleWrite( f == stderr ? 2 : 1, text );
	return strlen( text );
}

static int FakeFflush( FILE *f ) {
	return 0;
}

static char *FakeFgets( char *buf, int size, FILE *f ) {
	int	i;

	Check( f == stdin, "fgets reads stdin" );
	ConsoleRead();
	if ( !typed || !*typed ) {
		return NULL;
	}
	for ( i = 0 ; i < size - 1 && *typed ; ) {
		buf[i++] = *typed;
		if ( *typed++ == '\n' ) {
			break;
		}
	}
	buf[i] = '\0';
	return buf;
}

static int FakeGetchar( void ) {
	ConsoleRead();
	if ( typed && *typed ) {
		return (unsigned char)*typed++;
	}
	return '\n';	/* Return */
}

/* ---- the rest of main's world ---- */

static jmp_buf		comInitDone;
static int			comInitCalls;
static int			viewlog;
static const char	*staticModulesError;
static char			comInitCommandLine[MAX_STRING_CHARS];
static char			cwd[4096];		/* Sys_GetCwd: the application folder */

void Sys_LogRecord( const char *text ) {
	Q_strcat( ring, sizeof( ring ), text );
}

static const char *VM_InitStaticModules( void *modules, int count ) {
	Event( "VM_InitStaticModules" );
	return staticModulesError;
}
static int	sys_staticModules[1];
#define SYS_STATIC_MODULES	1

static char *Sys_GetCwd( void ) {
	Event( "Sys_GetCwd" );
	return cwd;
}

void Com_FlightRecord( const char *fmt, ... ) {
}

void Debug_Breadcrumb( int color ) {
}

void Com_Frame( void ) {
	Check( 0, "Com_Init returns to the test" );
}

void Sys_ShowConsole( int level, qboolean quitOnClose );
void Sys_LogPrintf( const char *fmt, ... );

/* Stands in for Com_Init: some startup logging, then CL_Init's
 * Sys_ShowConsole( viewlog ), then more console output. */
void Com_Init( char *commandLine ) {
	Event( "Com_Init" );
	comInitCalls++;
	Q_strncpyz( comInitCommandLine, commandLine, sizeof( comInitCommandLine ) );
	Sys_LogPrintf( "Com_Init: Sys_Init\n" );
	Sys_Print( "engine: before viewlog\n" );
	Sys_ShowConsole( viewlog, qfalse );
	Sys_Print( "engine: after viewlog\n" );
	longjmp( comInitDone, 1 );
}

void QDECL Com_Error( int level, const char *fmt, ... ) {
	fprintf( stderr, "mac toolbox init regression: Com_Error: %s\n", fmt );
	exit( 1 );
}

void QDECL Com_Printf( const char *fmt, ... ) {
}

static qboolean	consoleDisplayed;	/* mac_console.c's */

#define printf		FakePrintf
#define fprintf		FakeFprintf
#define fflush		FakeFflush
#define fgets		FakeFgets
#define getchar		FakeGetchar
#define main		Sys_TestMacMain
#include "mac_main_extracted.c"
#include "mac_console_extracted.c"
#undef main
#undef printf
#undef fprintf
#undef fflush
#undef fgets
#undef getchar

/* ---- the tests ---- */

static const char *currentCase = "setup";

static void Check( int ok, const char *message ) {
	if ( !ok ) {
		int	i;

		fprintf( stderr, "mac toolbox init regression failed (%s): %s\nevents:", currentCase, message );
		for ( i = 0 ; i < numEvents ; i++ ) {
			fprintf( stderr, " %s", events[i] );
		}
		fprintf( stderr, "\n" );
		exit( 1 );
	}
}

static int EventIndex( const char *name ) {
	int	i;

	for ( i = 0 ; i < numEvents ; i++ ) {
		if ( !strcmp( events[i], name ) ) {
			return i;
		}
	}
	return -1;
}

static void WriteParms( const char *data ) {
	char	path[4096];
	FILE	*f;

	snprintf( path, sizeof( path ), "%s:MacQuake3Parms.txt", cwd );
	if ( !data ) {
		remove( path );
		return;
	}
	f = fopen( path, "wb" );
	Check( f != NULL, "write the parameters file" );
	fwrite( data, 1, strlen( data ), f );
	fclose( f );
}

static int RunMain( const char *name, int shift, const char *line, const char *parms, int log ) {
	static char	*argv[] = { "Quake3", NULL };
	int			result;

	currentCase = name;
	numEvents = 0;
	toolboxReady = 0;
	shiftHeld = shift;
	typed = line;
	viewlog = log;
	staticModulesError = NULL;
	windowOpen = 0;
	windowText[0] = '\0';
	ring[0] = '\0';
	outputBeforeToolbox = 0;
	consoleDisplayed = qfalse;
	comInitCalls = 0;
	comInitCommandLine[0] = '\0';
	WriteParms( parms );

	if ( setjmp( comInitDone ) ) {
		return -1;	/* reached Com_Init */
	}
	result = Sys_TestMacMain( 1, argv );
	return result;
}

/* main's own Toolbox initialization comes first, in InitMacStuff's order */
static void CheckToolboxFirst( void ) {
	static const char	*order[] = {
		"MaxApplZone", "MoreMasters", "InitGraf", "InitFonts", "FlushEvents",
		"InitWindows", "InitMenus", "TEInit", "InitDialogs", "InitCursor"
	};
	int					i;

	Check( !outputBeforeToolbox, "no output before the Toolbox is initialized" );
	for ( i = 0 ; i < (int)( sizeof( order ) / sizeof( order[0] ) ) ; i++ ) {
		Check( numEvents > i && !strcmp( events[i], order[i] ),
			"main starts with MaxApplZone, MoreMasters, InitGraf, InitFonts, FlushEvents, "
			"InitWindows, InitMenus, TEInit, InitDialogs, InitCursor" );
	}
	Check( EventIndex( "EventAvail" ) > EventIndex( "InitCursor" ),
		"EventAvail brings the application to the front after the Toolbox is up" );
	Check( EventIndex( "VM_InitStaticModules" ) > EventIndex( "InitCursor" ),
		"VM_InitStaticModules runs after the Toolbox is initialized" );
}

static void CheckGetKeysAfterToolbox( void ) {
	Check( EventIndex( "GetKeys" ) > EventIndex( "InitCursor" ), "GetKeys runs after the Toolbox is initialized" );
}

int main( int argc, char **argv ) {
	int		result;
	char	longLine[1200];
	char	longFile[1200];

	if ( argc != 2 ) {
		fprintf( stderr, "usage: %s <scratch directory>\n", argv[0] );
		return 2;
	}
	/* main joins HFS style, so the file is <dir>/app:MacQuake3Parms.txt */
	snprintf( cwd, sizeof( cwd ), "%s/app", argv[1] );

	/* default launch: no Shift, no parameters file, viewlog 0 */
	result = RunMain( "default launch", 0, NULL, NULL, 0 );
	Check( result == -1 && comInitCalls == 1, "Com_Init is reached" );
	CheckToolboxFirst();
	CheckGetKeysAfterToolbox();
	Check( EventIndex( "output" ) > EventIndex( "InitCursor" ), "there is startup output, after the Toolbox" );
	Check( !windowOpen, "no console window with viewlog 0 and no error" );
	Check( strstr( ring, "main: START\n" ) != NULL, "the crash ring has main: START" );
	Check( strstr( ring, "main: command line: \n" ) != NULL, "the crash ring has the command line" );
	Check( strstr( ring, "Com_Init: Sys_Init\n" ) != NULL, "the crash ring has Com_Init's log" );
	Check( strstr( ring, "engine: after viewlog\n" ) != NULL, "the crash ring has hidden engine output" );
	Check( comInitCommandLine[0] == '\0', "empty command line" );

	/* a parameters file changes nothing about the window */
	result = RunMain( "parameters file", 0, NULL, "+set fs_game mymod\r\nsafe\r\n", 0 );
	Check( result == -1, "Com_Init is reached" );
	CheckToolboxFirst();
	CheckGetKeysAfterToolbox();
	Check( !windowOpen, "no console window with a parameters file" );
	Check( !strcmp( comInitCommandLine, "+set fs_game mymod\nsafe\n" ), "the file reaches Com_Init" );
	Check( strstr( ring, "main: startup parameters read from" ) != NULL, "the crash ring has the file's path" );

	/* viewlog 1 opens the window when Com_Init shows the console */
	result = RunMain( "viewlog 1", 0, NULL, NULL, 1 );
	Check( result == -1, "Com_Init is reached" );
	CheckToolboxFirst();
	Check( windowOpen, "viewlog 1 opens the console window" );
	Check( EventIndex( "console window" ) > EventIndex( "Com_Init" ), "the window opens at Sys_ShowConsole" );
	Check( strstr( windowText, "engine: after viewlog\n" ) != NULL, "engine output shows with viewlog 1" );
	Check( strstr( windowText, "engine: before viewlog\n" ) == NULL, "output before viewlog stays hidden" );

	/* Shift held: the prompt is shown in the window and the typed line used */
	result = RunMain( "Shift prompt", 1, "+set s_initsound 1\n", "+set fs_game ignored\n", 0 );
	Check( result == -1, "Com_Init is reached" );
	CheckToolboxFirst();
	CheckGetKeysAfterToolbox();
	Check( windowOpen, "Shift opens the console window" );
	Check( EventIndex( "console window" ) > EventIndex( "GetKeys" ), "the window opens after GetKeys" );
	Check( strstr( windowText, "Quake 3 startup parameters" ) != NULL, "the prompt is in the window" );
	Check( !strcmp( comInitCommandLine, "+set s_initsound 1" ), "the typed line reaches Com_Init" );

	/* #24/#486: an overlong parameters file stops before Com_Init, in the window */
	memset( longFile, 'a', sizeof( longFile ) - 1 );
	longFile[sizeof( longFile ) - 1] = '\0';
	result = RunMain( "overlong parameters file", 0, NULL, longFile, 0 );
	Check( result == 1 && comInitCalls == 0, "main returns 1 before Com_Init" );
	CheckToolboxFirst();
	Check( windowOpen, "the error opens the console window" );
	Check( strstr( windowText, "exceeds 1023 bytes" ) != NULL, "the error is in the window" );
	Check( strstr( windowText, "Press Return to quit." ) != NULL, "the Return prompt is in the window" );

	/* the same for an overlong typed line */
	memset( longLine, 'b', sizeof( longLine ) - 2 );
	longLine[sizeof( longLine ) - 2] = '\n';
	longLine[sizeof( longLine ) - 1] = '\0';
	result = RunMain( "overlong typed line", 1, longLine, NULL, 0 );
	Check( result == 1 && comInitCalls == 0, "main returns 1 before Com_Init" );
	CheckToolboxFirst();
	Check( strstr( windowText, "startup parameters exceed 1023 bytes" ) != NULL, "the error is in the window" );

	/* a static module failure is reported in the window, after the Toolbox */
	numEvents = 0;
	currentCase = "static module error";
	WriteParms( NULL );
	toolboxReady = 0;
	windowOpen = 0;
	windowText[0] = '\0';
	outputBeforeToolbox = 0;
	consoleDisplayed = qfalse;
	staticModulesError = "static module failure";
	{
		static char	*args[] = { "Quake3", NULL };

		if ( !setjmp( comInitDone ) ) {
			result = Sys_TestMacMain( 1, args );
		} else {
			result = -1;
		}
	}
	Check( result == 1, "main returns 1" );
	Check( !outputBeforeToolbox, "no output before the Toolbox is initialized" );
	Check( EventIndex( "VM_InitStaticModules" ) > EventIndex( "InitCursor" ), "the Toolbox is initialized first" );
	Check( windowOpen && strstr( windowText, "static module failure" ) != NULL, "the error is in the window" );
	staticModulesError = NULL;

	printf( "mac toolbox init regression passed\n" );
	return 0;
}
