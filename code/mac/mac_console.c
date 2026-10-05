#include <stdio.h>
#include "../client/client.h"
#include "mac_local.h"
#include <DriverServices.h>

#define	CONSOLE_MASK	1023
static char	consoleChars[CONSOLE_MASK+1];
static int consoleHead, consoleTail;
static qboolean consoleDisplayed;

/*
==================
Sys_InitConsole
==================
*/
void	Sys_InitConsole( void ) {
    // StdCLib should handle console initialization if linked.
}

/*
==================
Sys_ShowConsole
==================
*/
void	Sys_ShowConsole( int level, qboolean quitOnClose ) {
	
	if ( level ) {
		consoleDisplayed = qtrue;
		printf( "\n" );
	} else {
		consoleDisplayed = qfalse;
	}	
}


/*
==================
Sys_ConsoleWanted

Asked by _consolewrite (mac_consolehooks.cc) before output to fd opens the
console window. Errors on stderr always open it. Other output opens it only
once the console is displayed (viewlog, or the Shift prompt at launch).
Hidden output still reaches the crash ring.
==================
*/
int Sys_ConsoleWanted( int fd ) {
	return fd == 2 || consoleDisplayed;
}


/*
================
Sys_Print

This is called for all console output, even if the game is running
full screen and the dedicated console window is hidden.
================
*/
void	Sys_Print( const char *text ) {
	// Always record into the crash-dump ring buffer, even with the
	// on-screen console hidden (viewlog 0) — otherwise the post-mortem
	// dumps contain none of the engine's console output.
	Sys_LogRecord( text );

	if ( !consoleDisplayed ) {
		return;
	}
	printf( "%s", text );
}


/*
==================
Sys_ConsoleEvent
==================
*/
qboolean Sys_ConsoleEvent( EventRecord *event ) {
    // SIOUX handling removed. 
    // If we need input, we'd need to poll stdin or similar if supported, 
    // or handle system events that map to console if using a specific library.
    // For now, just return false as we rely on the game loop for main input.
    return qfalse;
}


/*
================
Sys_ConsoleInput

Checks for a complete line of text typed in at the console.
Return NULL if a complete line is not ready.
================
*/
char *Sys_ConsoleInput( void ) {
    // Simple stdin polling not implemented effectively here without blocking or 
    // knowing how StdCLib maps input events in the game loop.
    // Returning NULL disables dedicated server console input for now.
	return NULL;
}
