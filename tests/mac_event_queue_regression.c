/* Issues #256, #291 and #18: classic Mac keyboard input and its event queue.
 *
 * #256: Mac OS posts no keyDown/keyUp for modifier keys; their state is only
 * in EventRecord.modifiers, which WaitNextEvent and GetOSEvent also fill in
 * for the null event they return when nothing is pending.  A modifier pressed
 * and released with no other input must give exactly one down and one up, on
 * the poll that sees it, as id's original Sys_SendKeyEvents did.
 *
 * #291: a windowed game in the background still gets null events, and they
 * carry the system-wide modifiers.  After a suspend event, modifiers pressed
 * in the front application must give no key events.  Keys, modifiers and
 * InputSprocket mouse buttons held at suspend must be released once, because
 * their key-ups go to the front application, and modifiers must resync on
 * resume.  An InputSprocket button press still buffered at the suspend event
 * must be drained and released with the rest, and one buffered across the
 * suspend must be flushed on resume, not delivered without its release.
 *
 * #18: when the 256-slot queue fills, the dropped event's Z_Malloc payload
 * must be freed, the newest events kept, and no key release lost, or a key
 * whose press was already delivered stays down.  The overflow is reported
 * once, from Sys_GetEvent, with the number of events dropped, and nothing is
 * printed from inside the queue.
 *
 * The runner extracts the real queue (Sys_QueEvent and its helpers) and
 * Sys_GetEvent from mac_main.c, and Sys_MsecForMacEvent, vkeyToQuakeKey,
 * DoKeyDown, DoKeyUp, Sys_ModifierEvents, DoOSEvent and Sys_SendKeyEvents
 * from mac_event.c, and Sys_SuspendInput, Sys_ResumeInput and Sys_Input from
 * mac_input.c, verbatim (Sys_QueEvent renamed Sys_QueEvent_extracted, so a
 * wrapper can tell when the queue is running).  The Event Manager is a fake
 * that returns scripted events and otherwise null events carrying the current
 * modifiers; InputSprocket is a fake with per-element event queues.
 * The fixture plays the engine: it applies key events to its own key state
 * and frees packet payloads, as Com_EventLoop does. */
#include "../code/game/q_shared.h"
#include "../code/qcommon/qcommon.h"
#include "../code/ui/keycodes.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- fake Event Manager ---- */

typedef unsigned char	Boolean;
typedef unsigned short	EventMask;
typedef unsigned int	KeyMap[4];
typedef void			*WindowPtr;
typedef struct {
	short			what;
	long			message;
	unsigned long	when;
	struct { short v, h; } where;
	unsigned short	modifiers;
} EventRecord;

enum { nullEvent = 0, mouseDown = 1, mouseUp = 2, keyDown = 3, keyUp = 4,
	autoKey = 5, updateEvt = 6, diskEvt = 7, activateEvt = 8, osEvt = 15 };
enum { everyEvent = 0xFFFF, updateMask = 0x0040 };
enum { charCodeMask = 0x000000FF, keyCodeMask = 0x0000FF00 };
enum { btnState = 0x0080, cmdKey = 0x0100, shiftKey = 0x0200, alphaLock = 0x0400,
	optionKey = 0x0800, controlKey = 0x1000 };
enum { suspendResumeMessage = 0x01, resumeFlag = 1 };
#define BitAnd( a, b )	( (a) & (b) )
#define nil			NULL

#define MAX_SCRIPTED	16
static EventRecord		scripted[MAX_SCRIPTED];
static int				scriptedHead, scriptedTail;
static unsigned short	macModifiers = btnState;	/* system-wide; button up */
static unsigned long	macTicks = 1000;

static void PostEvent( short what, long message ) {
	EventRecord *ev = &scripted[scriptedHead++ % MAX_SCRIPTED];
	memset( ev, 0, sizeof( *ev ) );
	ev->what = what;
	ev->message = message;
}

static Boolean NextEvent( EventRecord *event ) {
	macTicks++;
	if ( scriptedTail < scriptedHead ) {
		*event = scripted[scriptedTail++ % MAX_SCRIPTED];
		event->when = macTicks;
		event->modifiers = macModifiers;
		return 1;
	}
	/* a null event: when and modifiers are still filled in */
	memset( event, 0, sizeof( *event ) );
	event->what = nullEvent;
	event->when = macTicks;
	event->modifiers = macModifiers;
	return 0;
}

static Boolean WaitNextEvent( EventMask mask, EventRecord *event, unsigned long sleep, void *rgn ) {
	(void)mask; (void)sleep; (void)rgn;
	return NextEvent( event );
}
static Boolean GetOSEvent( EventMask mask, EventRecord *event ) {
	(void)mask;
	return NextEvent( event );
}
static unsigned long TickCount( void ) { return macTicks; }
static void GetKeys( KeyMap keys ) { memset( keys, 0, sizeof( KeyMap ) ); }
static void SysBeep( short duration ) { (void)duration; }
static void ShowCursor( void ) { }
static void HideCursor( void ) { }

/* ---- fake InputSprocket: one mouse, two axes and three buttons ---- */

typedef unsigned int	UInt32;
typedef int				OSStatus;
enum { false = 0, true = 1 };	/* MacTypes.h */
typedef int				ISpElementReference;	/* index into ispQueue */
typedef struct {
	unsigned long long	when;
	ISpElementReference	element;
	UInt32				refCon;
	UInt32				data;
} ISpElementEvent;

#define ISP_ELEMENTS	5
#define ISP_QUEUE		16
static UInt32		ispQueue[ISP_ELEMENTS][ISP_QUEUE];
static int			ispQueued[ISP_ELEMENTS];
static qboolean		ispStarted, ispSuspended;
static int			ispPressAtSuspend;	/* a press that lands as ISp suspends */
static int			suspendCalls, resumeCalls, flushCalls;
static const char	*currentCase;
static int			failures;

static void IspEvent( int button, int down ) {
	int element = button - K_MOUSE1 + 2;
	if ( ispQueued[element] < ISP_QUEUE ) {
		ispQueue[element][ispQueued[element]++] = down;
	}
}

static void IspNeedsStartup( const char *call ) {
	if ( !ispStarted ) {
		fprintf( stderr, "FAIL %s: %s without ISpStartup\n", currentCase, call );
		failures++;
	}
}

static OSStatus ISpSuspend( void ) {
	IspNeedsStartup( "ISpSuspend" );
	suspendCalls++;
	ispSuspended = qtrue;
	if ( ispPressAtSuspend ) {
		IspEvent( ispPressAtSuspend, 1 );
		ispPressAtSuspend = 0;
	}
	return 0;
}
static OSStatus ISpResume( void ) {
	IspNeedsStartup( "ISpResume" );
	resumeCalls++;
	ispSuspended = qfalse;
	return 0;
}
static OSStatus ISpShutdown( void ) { ispStarted = qfalse; return 0; }
static OSStatus ISpElement_GetNextEvent( ISpElementReference element, UInt32 size,
		ISpElementEvent *event, Boolean *wasEvent ) {
	*wasEvent = 0;
	if ( !ispStarted || ispSuspended || size != sizeof( *event ) || !ispQueued[element] ) {
		return 0;
	}
	memset( event, 0, sizeof( *event ) );
	event->element = element;
	event->data = ispQueue[element][0];
	memmove( ispQueue[element], ispQueue[element] + 1, --ispQueued[element] * sizeof( UInt32 ) );
	*wasEvent = 1;
	return 0;
}
static OSStatus ISpElement_GetSimpleState( ISpElementReference element, UInt32 *state ) {
	(void)element;
	*state = 0;
	return 0;
}
static OSStatus ISpElement_Flush( ISpElementReference element ) {
	if ( !ispStarted ) {
		fprintf( stderr, "FAIL %s: ISpElement_Flush without InputSprocket\n", currentCase );
		failures++;
		return -50;	/* paramErr */
	}
	flushCalls++;
	ispQueued[element] = 0;
	return 0;
}

/* mac_input.c's globals, set up as Sys_InitInput leaves them */
qboolean			inputSuspended;
static UInt32		numDevices = 1;
static UInt32		numElements[1] = { ISP_ELEMENTS };
static ISpElementReference	elements[1][ISP_ELEMENTS] = { { 0, 1, 2, 3, 4 } };
static cvar_t		noMouseCvar, dedicatedCvar;
static cvar_t		*in_nomouse = &noMouseCvar;
cvar_t				*com_dedicated = &dedicatedCvar;

/* ---- the rest of the Mac port, as far as these functions reach ---- */

static struct { qboolean isFullscreen; } glConfig;
static cvar_t		waitNextEventCvar;
static cvar_t		*sys_waitNextEvent = &waitNextEventCvar;
static qboolean		ignoreUpdateEvents;
int					sys_ticBase, sys_msecBase, sys_lastEventTic;
qboolean			inputActive;
qboolean			inputSystemSuspended;

void Sys_QueEvent( int time, sysEventType_t type, int value, int value2, int ptrLength, void *ptr );
void Sys_QueEvent_extracted( int time, sysEventType_t type, int value, int value2, int ptrLength, void *ptr );
void Sys_ReleaseKeys( void );
void Sys_SendKeyEvents( void );
void Sys_ModifierEvents( int modifiers );
void Sys_Input( void );
void Sys_SuspendInput( void );
void Sys_ResumeInput( void );
void Sys_ShutdownInput( void ) {
	ShowCursor();
	ISpShutdown();
	inputActive = qfalse;
}
static qboolean Sys_ConsoleEvent( EventRecord *event ) { (void)event; return qfalse; }
void DoMouseDown( EventRecord *event ) { (void)event; }
void DoMouseUp( EventRecord *event ) { (void)event; }
void DoUpdate( WindowPtr window ) { (void)window; }
void DoDiskEvent( EventRecord *event ) { (void)event; }
void DoActivate( WindowPtr window, int modifiers ) { (void)window; (void)modifiers; }
static qboolean Sys_GetPacket( netadr_t *from, msg_t *msg ) { (void)from; (void)msg; return qfalse; }
void MSG_Init( msg_t *buf, byte *data, int length ) {
	memset( buf, 0, sizeof( *buf ) );
	buf->data = data;
	buf->maxsize = length;
}
void Com_DumpFlightRecord( const char *name ) { (void)name; }
static void Sys_DumpRetroLogs( const char *name ) { (void)name; }

static int			milliseconds = 5000;
int Sys_Milliseconds( void ) { return milliseconds; }

/* zone allocations, counted */
static int			zoneAllocs, zoneFrees;
void *Z_Malloc( int size ) {
	zoneAllocs++;
	return calloc( 1, size );
}
void Z_Free( void *ptr ) {
	zoneFrees++;
	free( ptr );
}

/* Com_Printf must not run inside the queue (#18): it can reach the
 * console and the log while the queue is mid-update. */
static int			insideQueue, printedInsideQueue;
void Sys_QueEvent( int time, sysEventType_t type, int value, int value2, int ptrLength, void *ptr ) {
	insideQueue++;
	Sys_QueEvent_extracted( time, type, value, value2, ptrLength, ptr );
	insideQueue--;
}

static char			printed[16384];
void QDECL Com_Printf( const char *fmt, ... ) {
	va_list	ap;
	size_t	len = strlen( printed );

	if ( insideQueue && !printedInsideQueue ) {
		fprintf( stderr, "FAIL %s: Com_Printf from inside Sys_QueEvent\n", currentCase );
		failures++;
		printedInsideQueue = 1;
	}
	va_start( ap, fmt );
	vsnprintf( printed + len, sizeof( printed ) - len, fmt, ap );
	va_end( ap );
}

#include "mac_event_extracted.c"
#include "mac_main_extracted.c"
#include "mac_input_extracted.c"

/* ---- the engine side ---- */

static int			keyDownState[256];
static int			downEvents[256], upEvents[256];
static int			packetsSeen, lastPacket;

static void Check( int ok, const char *message ) {
	if ( !ok ) {
		fprintf( stderr, "FAIL %s: %s\n", currentCase, message );
		failures++;
	}
}

static void ClearCounts( void ) {
	memset( downEvents, 0, sizeof( downEvents ) );
	memset( upEvents, 0, sizeof( upEvents ) );
}

/* one Com_EventLoop pass: poll and drain until the queue is empty */
static void Frame( void ) {
	sysEvent_t	ev;
	int			guard;

	milliseconds += 16;
	for ( guard = 0 ; guard < 4096 ; guard++ ) {
		ev = Sys_GetEvent();
		if ( ev.evType == SE_NONE ) {
			return;
		}
		if ( ev.evType == SE_KEY && ev.evValue >= 0 && ev.evValue < 256 ) {
			keyDownState[ev.evValue] = ev.evValue2 != 0;
			if ( ev.evValue2 ) {
				downEvents[ev.evValue]++;
			} else {
				upEvents[ev.evValue]++;
			}
		}
		if ( ev.evType == SE_PACKET ) {
			packetsSeen++;
			lastPacket = *(int *)ev.evPtr;
		}
		if ( ev.evPtr ) {
			Z_Free( ev.evPtr );
		}
	}
	Check( 0, "the queue never drained" );
}

static int AnyKeyDown( void ) {
	int i;
	for ( i = 0 ; i < 256 ; i++ ) {
		if ( keyDownState[i] ) {
			return 1;
		}
	}
	return 0;
}

static int Occurrences( const char *text, const char *word ) {
	int count = 0;
	while ( ( text = strstr( text, word ) ) != NULL ) {
		count++;
		text += strlen( word );
	}
	return count;
}

static void Suspend( void ) { PostEvent( osEvt, (long)suspendResumeMessage << 24 ); }
static void Resume( void ) { PostEvent( osEvt, ( (long)suspendResumeMessage << 24 ) | resumeFlag ); }

/* #256 */
static void ModifierAlone( qboolean fullscreen ) {
	static const struct { int bit, key; } mods[] = {
		{ controlKey, K_CTRL }, { shiftKey, K_SHIFT }, { optionKey, K_ALT }, { cmdKey, K_COMMAND }
	};
	int i;

	glConfig.isFullscreen = fullscreen;	/* GetOSEvent, or WaitNextEvent on the desktop */
	Frame();
	for ( i = 0 ; i < (int)( sizeof( mods ) / sizeof( mods[0] ) ) ; i++ ) {
		ClearCounts();
		macModifiers = btnState | mods[i].bit;
		Frame();
		Check( keyDownState[mods[i].key], "a modifier pressed alone is down after one frame" );
		Check( downEvents[mods[i].key] == 1, "a modifier pressed alone gives one down event" );
		Frame();
		Check( downEvents[mods[i].key] == 1, "a held modifier gives no further events" );
		macModifiers = btnState;
		Frame();
		Check( !keyDownState[mods[i].key], "a modifier released alone is up after one frame" );
		Check( upEvents[mods[i].key] == 1, "a modifier released alone gives one up event" );
	}
	Check( !AnyKeyDown(), "nothing is left down" );
}

/* #291: presses made in the front application */
static void BackgroundModifiers( void ) {
	Frame();
	Suspend();
	Frame();
	Check( suspendCalls == 1, "the suspend event suspends InputSprocket" );
	ClearCounts();
	macModifiers = btnState | controlKey | shiftKey | optionKey;
	Frame();
	Frame();
	Check( !downEvents[K_CTRL] && !downEvents[K_SHIFT] && !downEvents[K_ALT],
		"modifiers pressed in the background give no key events" );
	Check( !AnyKeyDown(), "nothing is down while in the background" );
	macModifiers = btnState;
	Frame();
	Check( !upEvents[K_CTRL] && !upEvents[K_SHIFT] && !upEvents[K_ALT],
		"modifiers released in the background give no key events" );
	/* back to the game with Shift held: it resyncs on resume */
	macModifiers = btnState | shiftKey;
	Resume();
	Frame();
	Frame();
	Check( resumeCalls == 1, "the resume event resumes InputSprocket" );
	Check( keyDownState[K_SHIFT] && downEvents[K_SHIFT] == 1, "a modifier held at resume is down once" );
	macModifiers = btnState;
	Frame();
	Check( !keyDownState[K_SHIFT] && upEvents[K_SHIFT] == 1, "and released once" );
}

/* #291: keys held across Cmd-Tab */
static void SuspendReleases( void ) {
	Frame();
	macModifiers = btnState | controlKey;
	PostEvent( keyDown, ( 0x0D << 8 ) | 'w' );	/* the W key */
	IspEvent( K_MOUSE2, 1 );
	Frame();
	Check( keyDownState['w'] && keyDownState[K_CTRL] && keyDownState[K_MOUSE2], "W, Ctrl and mouse 2 are down" );
	ClearCounts();
	/* Cmd-Tab: the suspend event arrives with Ctrl (and Cmd) still held */
	macModifiers = btnState | controlKey | cmdKey;
	Suspend();
	Frame();
	Check( !AnyKeyDown(), "every key held at suspend is released" );
	Check( upEvents['w'] == 1 && upEvents[K_CTRL] == 1 && upEvents[K_MOUSE2] == 1,
		"each held key is released once" );
	/* their real key-ups go to the front application; the game sees none */
	macModifiers = btnState;
	Frame();
	Check( upEvents[K_CTRL] == 1 && upEvents[K_COMMAND] <= 1, "no second release while in the background" );
	Check( !AnyKeyDown(), "nothing is down while in the background" );
}

/* #291: an InputSprocket click buffered when the suspend event arrives */
static void IspPressAtSuspend( void ) {
	Frame();
	ClearCounts();
	/* pressed after the last Sys_Input, in the poll that sees the suspend */
	IspEvent( K_MOUSE2, 1 );
	Suspend();
	Frame();
	Check( downEvents[K_MOUSE2] == 1 && upEvents[K_MOUSE2] == 1,
		"a press buffered at suspend is delivered and released" );
	Check( !AnyKeyDown(), "nothing is down while in the background" );
	Resume();
	Frame();
	Frame();
	Check( resumeCalls == 1, "the resume event resumes InputSprocket" );
	Check( !keyDownState[K_MOUSE2] && downEvents[K_MOUSE2] == 1,
		"the press is not delivered again after resume" );
	Check( !AnyKeyDown(), "nothing is left down after resume" );
}

/* #291: a click that lands after the drain, as InputSprocket suspends */
static void IspPressDuringSuspend( void ) {
	Frame();
	ClearCounts();
	ispPressAtSuspend = K_MOUSE3;
	Suspend();
	Frame();
	Check( suspendCalls == 1, "the suspend event suspends InputSprocket" );
	Resume();
	Frame();
	Frame();
	Check( flushCalls > 0, "the button queues are flushed on resume" );
	Check( !downEvents[K_MOUSE3] && !keyDownState[K_MOUSE3],
		"a press buffered across the suspend is not delivered after resume" );
	Check( !AnyKeyDown(), "nothing is left down after resume" );
}

/* #291: suspend and resume without InputSprocket (in_nomouse, or no ISp) */
static void NoInputSprocket( void ) {
	inputActive = qfalse;
	ispStarted = qfalse;
	IspEvent( K_MOUSE2, 1 );
	Frame();
	Suspend();
	Frame();
	Resume();
	Frame();
	Frame();
	Check( !suspendCalls && !resumeCalls, "InputSprocket is not suspended or resumed without it" );
	Check( flushCalls == 0, "nothing is flushed without InputSprocket" );
	Check( !downEvents[K_MOUSE2] && !AnyKeyDown(), "no button events without InputSprocket" );
}

/* #18: a key release and then more events than the queue holds */
static void Overflow( void ) {
	int i, *payload;

	Frame();
	Sys_QueEvent( 0, SE_KEY, K_SPACE, qtrue, 0, NULL );
	Frame();
	Check( keyDownState[K_SPACE], "Space is down" );
	printed[0] = 0;
	ClearCounts();
	Sys_QueEvent( 0, SE_KEY, K_SPACE, qfalse, 0, NULL );
	for ( i = 1 ; i <= 300 ; i++ ) {
		payload = Z_Malloc( sizeof( *payload ) );
		*payload = i;
		Sys_QueEvent( 0, SE_PACKET, 0, 0, sizeof( *payload ), payload );
	}
	Frame();
	Check( !keyDownState[K_SPACE] && upEvents[K_SPACE] == 1, "the queued Space release survives the overflow" );
	Check( zoneAllocs == zoneFrees, "every packet payload is freed, delivered or dropped" );
	Check( lastPacket == 300, "the newest packet is kept" );
	Check( packetsSeen == 254, "the queue keeps 255 events" );
	Check( strstr( printed, "Sys_QueEvent: overflow, dropped 46 events\n" ) != NULL,
		"the overflow is reported with the number dropped" );
	Check( Occurrences( printed, "overflow" ) == 1, "the overflow is reported once" );
	Frame();
	Check( !AnyKeyDown(), "nothing is left down" );
}

/* #18: keys mashed while the engine is not draining the queue */
static void OverflowReleases( void ) {
	int i;

	Frame();
	Sys_QueEvent( 0, SE_KEY, 'a', qtrue, 0, NULL );
	Sys_QueEvent( 0, SE_KEY, 'b', qtrue, 0, NULL );
	Frame();
	Check( keyDownState['a'] && keyDownState['b'], "A and B are down" );
	/* A released, then B released over and over: a queue of releases only */
	Sys_QueEvent( 0, SE_KEY, 'a', qfalse, 0, NULL );
	for ( i = 0 ; i < 300 ; i++ ) {
		Sys_QueEvent( 0, SE_KEY, 'b', qfalse, 0, NULL );
	}
	Frame();
	Check( !keyDownState['a'], "A's release survives a queue full of releases" );
	Check( !keyDownState['b'], "B is released" );
	/* and B pressed and released 150 times */
	Sys_QueEvent( 0, SE_KEY, 'a', qtrue, 0, NULL );
	Frame();
	Sys_QueEvent( 0, SE_KEY, 'a', qfalse, 0, NULL );
	for ( i = 0 ; i < 150 ; i++ ) {
		Sys_QueEvent( 0, SE_KEY, 'b', qtrue, 0, NULL );
		Sys_QueEvent( 0, SE_KEY, 'b', qfalse, 0, NULL );
	}
	Frame();
	Check( !AnyKeyDown(), "nothing is left down after mashing keys" );
}

int main( int argc, char **argv ) {
	if ( argc != 2 ) {
		fprintf( stderr, "usage: %s case\n", argv[0] );
		return 2;
	}
	currentCase = argv[1];
	inputActive = qtrue;	/* Sys_InitInput found a mouse */
	ispStarted = qtrue;

	if ( !strcmp( currentCase, "modifier-alone" ) ) {
		ModifierAlone( qfalse );
	} else if ( !strcmp( currentCase, "modifier-alone-fullscreen" ) ) {
		ModifierAlone( qtrue );
	} else if ( !strcmp( currentCase, "background-modifiers" ) ) {
		BackgroundModifiers();
	} else if ( !strcmp( currentCase, "suspend-releases" ) ) {
		SuspendReleases();
	} else if ( !strcmp( currentCase, "isp-press-at-suspend" ) ) {
		IspPressAtSuspend();
	} else if ( !strcmp( currentCase, "isp-press-during-suspend" ) ) {
		IspPressDuringSuspend();
	} else if ( !strcmp( currentCase, "no-isp" ) ) {
		NoInputSprocket();
	} else if ( !strcmp( currentCase, "overflow" ) ) {
		Overflow();
	} else if ( !strcmp( currentCase, "overflow-releases" ) ) {
		OverflowReleases();
	} else {
		fprintf( stderr, "unknown case %s\n", currentCase );
		return 2;
	}
	if ( failures ) {
		return 1;
	}
	printf( "%s: ok\n", currentCase );
	return 0;
}
