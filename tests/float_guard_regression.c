/* The renderer's float guards (tr_local.h): R_FiniteFloat, R_FloatToIntValid,
 * R_FloatToInt and R_Ftol keep every finite in-range value's native
 * conversion and turn NaN, infinities and values outside int into 0 (#115).
 * #249 made R_FiniteFloat a union read instead of an out-of-line Com_Memcpy
 * into a volatile; this pins that nothing else changed, every float bit
 * pattern's exponent test included.  With an argument, it also times a
 * diffuse-lighting-shaped loop against an unguarded one. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
typedef int qboolean;
enum { qfalse, qtrue };
#define ID_INLINE inline
#define myftol( x ) ( (int)( x ) )
#include "float_guard_extracted.h"

static int failures;
static void Check( int ok, const char *what, float value ) {
	if ( !ok ) {
		fprintf( stderr, "float guard regression failed: %s (%g)\n", what, value );
		failures++;
	}
}

static float FromBits( uint32_t bits ) {
	float f;
	memcpy( &f, &bits, sizeof( f ) );
	return f;
}

int main( int argc, char **argv ) {
	static const float	finite[] = { 0.0f, -0.0f, 1.5f, -1.5f, 254.84f, 1e-40f, 3.4028235e38f, -3.4028235e38f };
	uint64_t			bits;
	int					i;

	for ( i = 0 ; i < (int)( sizeof( finite ) / sizeof( finite[0] ) ) ; i++ ) {
		Check( R_FiniteFloat( finite[i] ), "a finite value is finite", finite[i] );
	}
	Check( !R_FiniteFloat( INFINITY ) && !R_FiniteFloat( -INFINITY ), "infinities are not finite", INFINITY );
	Check( !R_FiniteFloat( NAN ) && !R_FiniteFloat( FromBits( 0xffc00001u ) ), "NaNs are not finite", NAN );

	/* the exponent test, for every bit pattern */
	for ( bits = 0 ; bits <= 0xffffffffu ; bits += 0x7f ) {
		float	f = FromBits( (uint32_t)bits );
		Check( R_FiniteFloat( f ) == ( isfinite( f ) != 0 ), "R_FiniteFloat is isfinite", f );
	}

	Check( R_FloatToInt( 254.84f ) == 254 && R_FloatToInt( -1.5f ) == -1, "in range keeps the native conversion", 254.84f );
	Check( R_FloatToInt( 2147483520.0f ) == 2147483520, "the largest float below 2^31 converts", 2147483520.0f );
	Check( R_FloatToInt( 2147483648.0f ) == 0 && R_FloatToInt( -2147483904.0f ) == 0, "outside int is 0", 2147483648.0f );
	Check( R_FloatToInt( -2147483648.0f ) == INT32_MIN, "-2^31 converts", -2147483648.0f );
	Check( R_FloatToInt( NAN ) == 0 && R_FloatToInt( INFINITY ) == 0 && R_FloatToInt( -INFINITY ) == 0,
		"NaN and infinities are 0", NAN );
	Check( R_Ftol( 7.9f ) == 7 && R_Ftol( NAN ) == 0, "R_Ftol is the same with myftol", 7.9f );

	if ( argc > 1 ) {
		enum { VERTS = 12288, FRAMES = 2000 };
		static float	light[VERTS];
		static int		out[VERTS];
		struct timespec	t0, t1, t2;
		int				f, v;

		for ( v = 0 ; v < VERTS ; v++ ) {
			light[v] = (float)( v % 300 ) * 0.97f;
		}
		clock_gettime( CLOCK_MONOTONIC, &t0 );
		for ( f = 0 ; f < FRAMES ; f++ ) {
			for ( v = 0 ; v < VERTS ; v++ ) {
				out[v] = myftol( light[v] + f );
			}
		}
		clock_gettime( CLOCK_MONOTONIC, &t1 );
		for ( f = 0 ; f < FRAMES ; f++ ) {
			for ( v = 0 ; v < VERTS ; v++ ) {
				out[v] = R_Ftol( light[v] + f );
			}
		}
		clock_gettime( CLOCK_MONOTONIC, &t2 );
		printf( "unguarded %.2f ns/vertex, guarded %.2f ns/vertex (%d)\n",
			( ( t1.tv_sec - t0.tv_sec ) * 1e9 + ( t1.tv_nsec - t0.tv_nsec ) ) / ( (double)VERTS * FRAMES ),
			( ( t2.tv_sec - t1.tv_sec ) * 1e9 + ( t2.tv_nsec - t1.tv_nsec ) ) / ( (double)VERTS * FRAMES ),
			out[VERTS / 2] );
	}
	if ( failures ) {
		return 1;
	}
	puts( "float guard regression passed" );
	return 0;
}
