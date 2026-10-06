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
 * resume.
 *
 * #18: when the 256-slot queue fills, the dropped event's Z_Malloc payload
 * must be freed, the newest events kept, and no key release lost, or a key
 * whose press was already delivered stays down.  The overflow is reported
 * once, from Sys_GetEvent, with the number of events dropped.
 *
 * The runner extracts the real queue (Sys_QueEvent and its helpers) and
 * Sys_GetEvent from mac_main.c, and Sys_MsecForMacEvent, vkeyToQuakeKey,
 * DoKeyDown, DoKeyUp, Sys_ModifierEvents, DoOSEvent and Sys_SendKeyEvents
 * from mac_event.c, verbatim.  The Event Manager is a fake that returns
 * scripted events and otherwise null events carrying the current modifiers.
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

/* ---- the rest of the Mac port, as far as these functions reach ---- */

static struct { qboolean isFullscreen; } glConfig;
static cvar_t		waitNextEventCvar;
static cvar_t		*sys_waitNextEvent = &waitNextEventCvar;
static qboolean		ignoreUpdateEvents;
int					sys_ticBase, sys_msecBase, sys_lastEventTic;
qboolean			inputActive;
qboolean			inputSystemSuspended;
static int			suspendCalls, resumeCalls;

/* Sys_Input: InputSprocket mouse buttons, queued as mac_input.c does */
static int			ispButton, ispButtonDown;

void Sys_QueEvent( int time, sysEventType_t type, int value, int value2, int ptrLength, void *ptr );
void Sys_ReleaseKeys( void );
void Sys_SendKeyEvents( void );
void Sys_ModifierEvents( int modifiers );

static void Sys_Input( void ) {
	if ( inputSystemSuspended || !ispButton ) {
		return;
	}
	Sys_QueEvent( 0, SE_KEY, ispButton, ispButtonDown, 0, NULL );
	ispButton = 0;
}
void Sys_SuspendInput( void ) { suspendCalls++; }
void Sys_ResumeInput( void ) { resumeCalls++; }
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

static char			printed[16384];
void QDECL Com_Printf( const char *fmt, ... ) {
	va_list	ap;
	size_t	len = strlen( printed );

	va_start( ap, fmt );
	vsnprintf( printed + len, sizeof( printed ) - len, fmt, ap );
	va_end( ap );
}

#include "mac_event_extracted.c"
#include "mac_main_extracted.c"

/* ---- the engine side ---- */

static const char	*currentCase;
static int			failures;
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
	ispButton = K_MOUSE2;
	ispButtonDown = 1;
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

	if ( !strcmp( currentCase, "modifier-alone" ) ) {
		ModifierAlone( qfalse );
	} else if ( !strcmp( currentCase, "modifier-alone-fullscreen" ) ) {
		ModifierAlone( qtrue );
	} else if ( !strcmp( currentCase, "background-modifiers" ) ) {
		BackgroundModifiers();
	} else if ( !strcmp( currentCase, "suspend-releases" ) ) {
		SuspendReleases();
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
