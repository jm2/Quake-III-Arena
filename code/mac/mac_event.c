#include "../client/client.h"
#include "mac_local.h"

void DoMenuCommand(long	menuAndItem);
void DoDrag(WindowPtr	myWindow,Point	mouseloc);
void DoGoAwayBox(WindowPtr myWindow, Point mouseloc);
void DoCloseWindow(WindowPtr myWindow);
void DoKeyDown(EventRecord *event);
void DoDiskEvent(EventRecord *event);
void DoOSEvent(EventRecord *event);
void DoUpdate(WindowPtr myWindow);
void DoActivate(WindowPtr myWindow, int myModifiers);
void DoAboutBox(void);
void DoMenuCommand(long	menuAndItem);
void DoMouseDown(EventRecord *event);
void DoMouseUp(EventRecord *event);
void DoMenuAdjust(void);
void DoKeyUp(EventRecord *event);

/*
================
Sys_MsecForMacEvent

Q3 event records take time in msec,
so convert the mac event record when
(60ths) to msec.  The base values
are updated ever frame, so this
is guaranteed to not drift.
=================
*/
int	Sys_MsecForMacEvent( void ) {
	int		tics;
	
	tics = sys_lastEventTic - sys_ticBase;
	
	return sys_msecBase + tics * 16;
}




void DoMouseDown(EventRecord *event)
{	
	int			myPart;
	WindowPtr	myWindow;
	Point		point;

	myPart = FindWindow(event->where, &myWindow);
	
	switch(myPart)
	{
		case inMenuBar:
			DrawMenuBar();
			DoMenuCommand(MenuSelect(event->where));
		break;
		case inSysWindow:
			SystemClick(event, myWindow);
		break;
		case inDrag:
			DoDrag(myWindow, event->where);
			
			// update the vid_xpos / vid_ypos cvars
			point.h = 0;
			point.v = 0;
			LocalToGlobal( &point );
			Cvar_SetValue( "vid_xpos", point.h );
			Cvar_SetValue( "vid_ypos", point.v );
			return;
		break;
		case inGoAway:
			DoGoAwayBox(myWindow, event->where);
		break;

		case inContent:
			if (myWindow != FrontWindow())
			{
				SelectWindow(myWindow);
			}
		break;
	}
}

void DoMouseUp(EventRecord *event)
{
}

void DoDrag(WindowPtr myWindow, Point mouseloc)
{
	Rect	dragBounds;
	
	dragBounds = (**GetGrayRgn()).rgnBBox;
	DragWindow(myWindow,mouseloc,&dragBounds);

	aglUpdateContext(aglGetCurrentContext());
}


void DoGoAwayBox(WindowPtr myWindow, Point mouseloc)
{
	if(TrackGoAway(myWindow,mouseloc))
	{ 
		DoCloseWindow(myWindow); 
	}
}

void DoCloseWindow(WindowPtr myWindow)
{
}

void DoMenuAdjust(void)
{
}

// 0 = no Quake key for this virtual key (previously the literal '?', which
// made unmapped keys type/bind a question mark).
int	vkeyToQuakeKey[256] = {
/*0x00*/	'a', 's', 'd', 'f', 'h', 'g', 'z', 'x',
/*0x08*/	'c', 'v', 0, 'b', 'q', 'w', 'e', 'r',
/*0x10*/	'y', 't', '1', '2', '3', '4', '6', '5',
/*0x18*/	'=', '9', '7', '-', '8', '0', ']', 'o',
/*0x20*/	'u', '[', 'i', 'p', K_ENTER, 'l', 'j', '\'',
/*0x28*/	'k', ';', '\\', ',', '/', 'n', 'm', '.',
/*0x30*/	K_TAB, K_SPACE, '`', K_BACKSPACE, 0, K_ESCAPE, 0, K_COMMAND,
/*0x38*/	K_SHIFT, K_CAPSLOCK, K_ALT, K_CTRL, 0, 0, 0, 0,
/*0x40*/	0, K_KP_DEL, 0, K_KP_STAR, 0, K_KP_PLUS, 0, K_KP_NUMLOCK,
/*0x48*/	0, 0, 0, K_KP_SLASH, K_KP_ENTER, 0, K_KP_MINUS, 0,
/*0x50*/	0, K_KP_EQUALS, K_KP_INS, K_KP_END, K_KP_DOWNARROW, K_KP_PGDN, K_KP_LEFTARROW, K_KP_5,
/*0x58*/	K_KP_RIGHTARROW, K_KP_HOME, 0, K_KP_UPARROW, K_KP_PGUP, 0, 0, 0,
/*0x60*/	K_F5, K_F6, K_F7, K_F3, K_F8, K_F9, 0, K_F11,
/*0x68*/	0, K_F13, 0, K_F14, 0, K_F10, 0, K_F12,
/*0x70*/	0, K_F15, K_INS, K_HOME, K_PGUP, K_DEL, K_F4, K_END,
/*0x78*/	K_F2, K_PGDN, K_F1, K_LEFTARROW, K_RIGHTARROW, K_DOWNARROW, K_UPARROW, K_POWER
};

void DoKeyDown(EventRecord *event)
{
	int		myCharCode;
	int		myKeyCode;
	int		quakeKey;

	myCharCode	= BitAnd(event->message,charCodeMask);
	myKeyCode = ( event->message & keyCodeMask ) >> 8;

	quakeKey = vkeyToQuakeKey[ myKeyCode ];
	if ( quakeKey ) {
		Sys_QueEvent( Sys_MsecForMacEvent(), SE_KEY, quakeKey, 1, 0, NULL );
	}
	Sys_QueEvent( Sys_MsecForMacEvent(), SE_CHAR, myCharCode, 0, 0, NULL );
}

void DoKeyUp(EventRecord *event)
{
	int		myKeyCode;
	int		quakeKey;

	myKeyCode = ( event->message & keyCodeMask ) >> 8;

	quakeKey = vkeyToQuakeKey[ myKeyCode ];
	if ( quakeKey ) {
		Sys_QueEvent( Sys_MsecForMacEvent(), SE_KEY, quakeKey, 0, 0, NULL );
	}
}

/*
==================
Sys_ModifierEvents
==================
*/
void Sys_ModifierEvents( int modifiers ) {
	static int		oldModifiers;
	int				changed;
	int				i;

	typedef struct {
		int		bit;
		int		keyCode;
	} modifierKey_t;

	static modifierKey_t	keys[] = {
		{ 128, K_MOUSE1 },
		{ 256, K_COMMAND },
		{ 512, K_SHIFT },
		{1024, K_CAPSLOCK },
		{2048, K_ALT },
		{4096, K_CTRL },
		{-1, -1 }
	};
	
	changed = modifiers ^ oldModifiers;

	for ( i = 0 ; keys[i].bit != -1 ; i++ ) {
		// if we have input sprockets running, ignore mouse events we
		// get from the debug passthrough driver
		if ( inputActive && keys[i].keyCode == K_MOUSE1 ) {
			continue;
		}

		if ( changed & keys[i].bit ) {
			int down = !!( modifiers & keys[i].bit );
			// btnState (bit 0x80) is SET when the mouse button is UP,
			// opposite to the modifier-key bits.
			if ( keys[i].bit == 128 ) {
				down = !down;
			}
			Sys_QueEvent( Sys_MsecForMacEvent(),
			SE_KEY, keys[i].keyCode, down, 0, NULL );
		}
	}

	oldModifiers = modifiers;
}


void DoDiskEvent(EventRecord	*event)
{

}

void	DoOSEvent(EventRecord	*event)
{
	// Suspend/resume: release InputSprocket and show the cursor when the
	// user switches away (cmd-tab to Finder/console), re-grab on return.
	// Previously empty, which left the cursor hidden and ISp capturing
	// input while the game was in the background.
	if ( ( ( event->message >> 24 ) & 0xFF ) == suspendResumeMessage ) {
		if ( event->message & resumeFlag ) {
			inputSystemSuspended = qfalse;
			Sys_ResumeInput();
		} else {
			inputSystemSuspended = qtrue;
			Sys_SuspendInput();
		}
	}
}
// mac_event.c

static qboolean ignoreUpdateEvents = qfalse;

/*
void DoUpdate(WindowPtr	myWindow)
{ 
	GrafPtr		origPort;
	AGLContext	ctx;
    
    // DEBUG: Always act on the main window.
    // The event.message seems to contain garbage or a different handle on Mac OS 9?
    // "mismatch 0x3d700001 vs 0x4aa53d70"
    WindowPtr targetWindow = (WindowPtr)sys_gl.drawable;

    if ( !targetWindow ) {
        return;
    }

	GetPort(&origPort);
	SetPort(targetWindow);
	BeginUpdate(targetWindow);	
    EndUpdate(targetWindow);
	
	// Only update context if one exists (may not during early init)
	ctx = aglGetCurrentContext();

	if (ctx != NULL) {
		aglUpdateContext(ctx);
	}
	
	SetPort(origPort);
}
*/
//
// DoUpdate
//
void DoUpdate(WindowPtr	myWindow)
{
	GrafPtr		origPort;
	AGLContext	ctx;

    if ( !myWindow ) {
        return;
    }

	GetPort(&origPort);
	SetPort(myWindow);
	BeginUpdate(myWindow);
	EndUpdate(myWindow);

	// Keep AGL in sync when the game window itself took the update. This
	// was disabled while event.message held garbage window pointers; that
	// was the mac68k struct-packing bug, fixed at the header level now.
	ctx = aglGetCurrentContext();
	if (ctx != NULL && (void*)myWindow == (void*)sys_gl.drawable) {
		aglUpdateContext(ctx);
	}

	SetPort(origPort);
}

void DoActivate( WindowPtr myWindow, int myModifiers) {

}

void DoAboutBox( void ) {
	DialogPtr	myDialog;
	short		itemHit;

	myDialog = GetNewDialog(kAboutDialog, nil, (WindowPtr) -1);
	ModalDialog(nil, &itemHit);
	DisposeDialog(myDialog);
}

void DoMenuCommand( long menuAndItem ) {
	int			myMenuNum;
	int			myItemNum;
	int			myResult;
	Str255		myDAName;
	WindowPtr	myWindow;
	
	myMenuNum	= HiWord(menuAndItem);
	myItemNum	= LoWord(menuAndItem);
	
	GetPort(&myWindow);
	
	switch (myMenuNum)  {
	case mApple:
		switch( myItemNum ) {
		case iAbout: 
			DoAboutBox();
			break;
		default: 
			GetMenuItemText(GetMenuHandle(mApple), myItemNum, myDAName);
			myResult = OpenDeskAcc(myDAName);
			break;
		}
		break;
	case mFile:
		switch (myItemNum) {
		case iQuit:
			Com_Quit_f();
			break;
		}
		break;
	}
	
	HiliteMenu(0);
}

void TestTime( EventRecord *ev ) {
	int		msec;
	int		tics;
	static int startTics, startMsec;
	
	msec = Sys_Milliseconds();
	tics = ev->when;
	
	if ( !startTics || ev->what == mouseDown ) {
		startTics = tics;
		startMsec = msec;
	}
	
	msec -= startMsec;
	tics -= startTics;
	
	if ( !tics ) {
		return;
	}
	Com_Printf( "%i msec to tic\n", msec / tics );
}

/*
==================
Sys_SendKeyEvents
==================
*/
void Sys_SendKeyEvents (void) {
	Boolean		   gotEvent;
	EventRecord	   event;
    EventMask      mask = everyEvent;

    if (ignoreUpdateEvents) {
        mask &= ~updateMask;
    }
	
	//Sys_LogPrintf("Sys_SendKeyEvents: Start\n");
	if ( !glConfig.isFullscreen || sys_waitNextEvent->value ) {
		// this call involves 68k code and task switching.
		// do it on the desktop, or if they explicitly ask for
		// it when fullscreen
		gotEvent = WaitNextEvent(mask, &event, 0, nil);
	} else {
		gotEvent = GetOSEvent( mask, &event );
	}
	//Sys_LogPrintf( "Sys_SendKeyEvents: Event check done, gotEvent=%d what=%d\n", gotEvent, event.what );
	
	// generate faked events from modifer changes
	Sys_ModifierEvents( event.modifiers );

	sys_lastEventTic = event.when;

	if ( !gotEvent ) {
		return;
	}
    
    //Sys_LogPrintf("Sys_SendKeyEvents: Processing event types=%d\n", event.what);

	if ( Sys_ConsoleEvent(&event) ) {
		return;
	}
	switch(event.what)
	{
		case mouseDown:
            //Sys_LogPrintf("Sys_SendKeyEvents: mouseDown\n");
			DoMouseDown(&event);
		break;
		case mouseUp:
            //Sys_LogPrintf("Sys_SendKeyEvents: mouseUp\n");
			DoMouseUp(&event);
		break;
		case keyDown:
            //Sys_LogPrintf("Sys_SendKeyEvents: keyDown\n");
			DoKeyDown(&event);
		break;
		case keyUp:
            //Sys_LogPrintf("Sys_SendKeyEvents: keyUp\n");
			DoKeyUp(&event);
		break;
		case autoKey:
            //Sys_LogPrintf("Sys_SendKeyEvents: autoKey\n");
			DoKeyDown(&event);
		break;
		case updateEvt:
            Sys_LogPrintf("Sys_SendKeyEvents: updateEvt\n");
			DoUpdate((WindowPtr) event.message);
            Sys_LogPrintf("Sys_SendKeyEvents: updateEvt done\n");
		break;
		case diskEvt:
            //Sys_LogPrintf("Sys_SendKeyEvents: diskEvt\n");
			DoDiskEvent(&event);
		break;
		case activateEvt:
            Sys_LogPrintf("Sys_SendKeyEvents: activateEvt\n");
			DoActivate((WindowPtr) event.message, event.modifiers);
            Sys_LogPrintf("Sys_SendKeyEvents: activateEvt done\n");
		break;
		case osEvt:
            Sys_LogPrintf("Sys_SendKeyEvents: osEvt\n");
			DoOSEvent(&event);
            Sys_LogPrintf("Sys_SendKeyEvents: osEvt done\n");
		break;
		default:
            //Sys_LogPrintf("Sys_SendKeyEvents: default %d\n", event.what);
		break;
	}
    //Sys_LogPrintf("Sys_SendKeyEvents: Done\n");
}
