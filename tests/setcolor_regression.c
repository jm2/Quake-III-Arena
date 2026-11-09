/* RB_SetColor (tr_backend.c): a module's 2D color, any float, becomes a byte
 * without an undefined float-to-byte conversion, and every byte the Mac
 * build gave is kept. Retro68's gcc converts color * 255 with fctiwz, which
 * saturates to an int, and stores the int's low byte: so 254.84 gives
 * 64984 & 0xff, 1e30 gives 0x7fffffff's 255, and -1e30 and NaN give
 * 0x80000000's 0. Ported from the ETL-OS9 sanitizer run (ET PR #48). */
#include <math.h>
#include <stdio.h>
typedef unsigned char byte;
typedef struct { int commandId; float color[4]; } setColorCommand_t;
static struct { byte color2D[4]; } backEnd;
#include "setcolor_extracted.c"

static int fails;
static void Check( float c, int want ) {
	setColorCommand_t	cmd = { 0, { c, c, c, c } };
	const void			*next = RB_SetColor( &cmd );
	int					i;

	for ( i = 0 ; i < 4 ; i++ ) {
		if ( backEnd.color2D[i] != want ) {
			fprintf( stderr, "set color regression failed: %g gave %d in channel %d, want %d\n",
				c, backEnd.color2D[i], i, want );
			fails++;
		}
	}
	if ( next != (const void *)( &cmd + 1 ) ) {
		fprintf( stderr, "set color regression failed: the next command is not after this one\n" );
		fails++;
	}
}

int main( void ) {
	Check( 0, 0 );
	Check( 0.5f, 127 );
	Check( 1, 255 );
	Check( 254.84f, 216 );		/* ET: Legacy's health bar: 64984 kept to its low byte */
	Check( -0.5f, 129 );		/* -127 kept to its low byte */
	Check( 8421504.0f, 128 );	/* 2147483520, still an int */
	Check( 1e30f, 255 );
	Check( -1e30f, 0 );
	Check( INFINITY, 255 );
	Check( -INFINITY, 0 );
	Check( NAN, 0 );
	if ( fails ) {
		return 1;
	}
	puts( "set color regression passed" );
	return 0;
}
