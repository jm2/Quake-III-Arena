/* Q_rsqrt (code/game/q_math.c) reinterprets a float's bits as an integer.
 * It read them through a long, which is 64 bits on LP64 hosts: the read took
 * four bytes past the float, and the result depended on them. The PowerPC
 * target's long is 32 bits, so only host builds were affected. One Newton
 * step leaves a relative error under 0.18 %. */
#include "../code/game/q_shared.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

void QDECL Com_Error( int level, const char *fmt, ... ) { (void)level; fprintf( stderr, "Com_Error: %s\n", fmt ); exit( 1 ); }
void QDECL Com_Printf( const char *fmt, ... ) { (void)fmt; }

int main( void ) {
	static const float values[] = { 1e-6f, 0.01f, 0.25f, 1.0f, 2.0f, 4.0f, 10.0f, 12345.0f, 1e6f, 3e30f };
	int i;

	for ( i = 0; i < (int)( sizeof( values ) / sizeof( values[0] ) ); i++ ) {
		volatile float x = values[i];
		double want = 1.0 / sqrt( x );
		double got = Q_rsqrt( x );
		if ( fabs( got - want ) > 0.0018 * want ) {
			fprintf( stderr, "q_rsqrt regression failed: Q_rsqrt(%g) = %g, want %g\n", x, got, want );
			return 1;
		}
	}
	puts( "q_rsqrt regression passed" );
	return 0;
}
