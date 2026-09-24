/* Issue #379: a server can set cg_drawCrosshair to anything through systeminfo,
 * and the base q3_ui Game Options menu indexes its crosshair shaders with it. */
#include "../code/q3_ui/ui_preferences.c"
#include "ui_cvar_syscalls.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *value;

/** Fail with the value under test and the contract that broke. */
static void Check( int ok, const char *what ) {
	if ( !ok ) {
		fprintf( stderr, "q3_ui crosshair regression failed (cg_drawCrosshair %s): %s\n", value, what );
		exit( 1 );
	}
}

/** One cg_drawCrosshair value per process: the menu shows the crosshair the
 * cgame draws for it, none for a negative value. */
int main( int argc, char **argv ) {
	char *end;
	long number;
	int expected;

	if ( argc != 2 ) {
		fprintf( stderr, "usage: %s <cg_drawCrosshair>\n", argv[0] );
		return 2;
	}
	value = argv[1];
	number = strtol( value, &end, 10 );
	Check( *value && !*end && number >= INT_MIN && number <= INT_MAX, "a decimal int" );
	expected = number < 0 ? 0 : (int)( number % NUM_CROSSHAIRS );

	dllEntry( FakeSyscall );
	SystemInfo_Set( "cg_drawCrosshair", value );
	Preferences_MenuInit();

	// the item draws the selected crosshair after its label, nothing for none
	drawnShader = -1;
	Crosshair_Draw( &s_preferences.crosshair );
	Check( s_preferences.crosshair.curvalue == expected, "crosshair item" );
	Check( expected ? drawnShader == s_preferences.crosshairShader[expected] :
		drawnShader != s_preferences.crosshairShader[0], "crosshair drawn" );

	// and steps to the next one
	Preferences_Event( &s_preferences.crosshair, QM_ACTIVATED );
	Check( Cvar_VariableValue( "cg_drawCrosshair" ) == ( expected + 1 ) % NUM_CROSSHAIRS, "next crosshair" );
	printf( "q3_ui bounds cg_drawCrosshair %s from systeminfo (issue #379)\n", value );
	return 0;
}
