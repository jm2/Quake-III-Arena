/* A server can set color1 through systeminfo (issue #379), and the base q3_ui
 * Player Settings menu shows it as the effects color. The cgame draws the color
 * that atoi reads from it (cg_players.c CG_ColorFromString), so the menu must
 * read it the same way: "1e1" is drawn blue and "0.5" and "0x3" white, which
 * the float value showed as white, blue and (with glibc's atof) cyan. */
#include "../code/q3_ui/ui_playersettings.c"
#include "ui_cvar_syscalls.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *value;

/** Fail with the value under test and the contract that broke. */
static void Check( int ok, const char *what ) {
	if ( !ok ) {
		fprintf( stderr, "q3_ui color1 regression failed (color1 %s): %s\n", value, what );
		exit( 1 );
	}
}

/** One color1 string per process, with the game color (1-7, 7 is white) the
 * cgame draws for it: the menu's effects color is that one. */
int main( int argc, char **argv ) {
	int color;

	if ( argc != 3 ) {
		fprintf( stderr, "usage: %s <color1> <game color drawn>\n", argv[0] );
		return 2;
	}
	value = argv[1];
	color = atoi( argv[2] );
	Check( color >= 1 && color <= 7, "a game color" );

	dllEntry( FakeSyscall );
	SystemInfo_Set( "color1", value );
	UI_PlayerSettingsMenu();
	Check( uitogamecode[s_playersettings.effects.curvalue] == color, "effects color" );
	printf( "q3_ui shows color1 %s as game color %d, as the cgame draws it\n", value, color );
	return 0;
}
