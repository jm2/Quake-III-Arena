/* Issue #389: a config or the console can set the renderer's r_mode,
 * r_fullscreen and r_allowExtensions to anything (and a server can, before the
 * renderer registers them), and the base q3_ui System Setup menu makes them
 * the values of spin controls, which draw the name at their value. A mode the
 * menu lists, and 0 or 1, must act as they always did; a mode past the list
 * shows 640x480, as a negative mode always has, and any other switch value
 * shows on, as the renderer reads a nonzero value. The texture detail slider
 * (3 - r_picmip) clamps to 0 to 3, and a NaN, which its clamp let through to
 * an int conversion, to full detail, as the renderer reads r_picmip "nan". */
#include "../code/q3_ui/ui_video.c"
#include "ui_cvar_syscalls.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *cvar, *value;

/** Fail with the cvar and value under test and the contract that broke. */
static void Check( int ok, const char *what ) {
	if ( !ok ) {
		fprintf( stderr, "q3_ui video regression failed (%s %s): %s\n", cvar, value, what );
		exit( 1 );
	}
}

/** r_picmip: the texture detail slider shows 3 - r_picmip, from 0 to 3, and
 * applies it back. */
static void TestPicmip( void ) {
	float v = strcmp( value, "nan" ) ? (float)strtol( value, NULL, 10 ) : 0;
	float shown = !strcmp( value, "nan" ) || 3 - v > 3 ? 3 : 3 - v < 0 ? 0 : 3 - v;

	GraphicsOptions_MenuInit();
	Check( s_graphicsoptions.tq.curvalue == shown, "the slider shows the texture detail" );
	GraphicsOptions_MenuDraw();
	GraphicsOptions_ApplyChanges( NULL, QM_ACTIVATED );
	Check( Cvar_VariableValue( "r_picmip" ) == 3 - shown, "the texture detail is applied" );
}

/** One cvar and value per process: the menu shows its entry, draws its name,
 * steps from it and applies it. */
int main( int argc, char **argv ) {
	static const char *names[] = { "r_mode", "r_fullscreen", "r_allowExtensions", "r_picmip" };
	menulist_s *items[3];
	menulist_s *item;
	char *end;
	long v;
	int i, shown, next;

	if ( argc != 3 ) {
		fprintf( stderr, "usage: %s r_mode|r_fullscreen|r_allowExtensions|r_picmip <value>\n", argv[0] );
		return 2;
	}
	cvar = argv[1];
	value = argv[2];
	v = strtol( value, &end, 10 );
	Check( ( *value && !*end && v >= INT_MIN && v <= INT_MAX ) || !strcmp( value, "nan" ), "a decimal int or nan" );
	items[0] = &s_graphicsoptions.mode;
	items[1] = &s_graphicsoptions.fs;
	items[2] = &s_graphicsoptions.allow_extensions;
	for ( i = 0; i < 4 && strcmp( cvar, names[i] ); i++ ) {
	}
	Check( i < 4, "r_mode, r_fullscreen, r_allowExtensions or r_picmip" );

	dllEntry( FakeSyscall );
	// the renderer's settings: 640x480 in a window, with extensions
	SystemInfo_Set( "r_mode", "3" );
	SystemInfo_Set( "r_fullscreen", "0" );
	SystemInfo_Set( "r_allowExtensions", "1" );
	SystemInfo_Set( "r_picmip", "1" );
	SystemInfo_Set( cvar, value );
	if ( i == 3 ) {
		TestPicmip();
		printf( "q3_ui video menu bounds %s %s (issue #389)\n", cvar, value );
		return 0;
	}
	GraphicsOptions_MenuInit();
	item = items[i];
	if ( item == &s_graphicsoptions.mode ) {
		Check( item->numitems == 12, "the menu lists 12 video modes" );
		shown = v >= 0 && v < item->numitems ? (int)v : 3;
	} else {
		Check( item->numitems == 2, "the switch is off or on" );
		shown = v != 0;
	}
	Check( item->curvalue == shown, "the item shows the entry" );

	// the menu draws the entry's name (SpinControl_Draw)
	GraphicsOptions_MenuDraw();
	Check( item->curvalue == shown, "the item draws the entry" );

	// a click steps to the next entry, which the menu applies
	Menu_SetCursorToItem( &s_graphicsoptions.menu, item );
	Menu_DefaultKey( &s_graphicsoptions.menu, K_MOUSE1 );
	next = ( shown + 1 ) % item->numitems;
	Check( item->curvalue == next, "a click steps from the entry shown" );
	GraphicsOptions_MenuDraw();
	Check( !( s_graphicsoptions.apply.generic.flags & QMF_HIDDEN ), "the step can be applied" );
	executed[0] = '\0';
	GraphicsOptions_ApplyChanges( NULL, QM_ACTIVATED );
	Check( (int)Cvar_VariableValue( cvar ) == next, "the step is applied" );
	Check( !strcmp( executed, "vid_restart\n" ), "applying restarts the renderer" );
	printf( "q3_ui video menu bounds %s %s (issue #389)\n", cvar, value );
	return 0;
}
