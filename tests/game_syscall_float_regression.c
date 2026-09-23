/* Issue #242: every float trap argument crosses the int-only syscall ABI via PASSFLOAT. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../code/game/g_syscalls.c"

static int lastArgs[3];

static int QDECL RecordSyscall( int arg, ... ) {
	va_list ap;
	int i;

	lastArgs[0] = arg;
	va_start( ap, arg );
	for ( i = 1; i < 3; i++ ) {	/* the mutate trap passes exactly two arguments */
		lastArgs[i] = va_arg( ap, int );
	}
	va_end( ap );
	return 0;
}

static int FloatBits( float value ) {
	int bits;
	memcpy( &bits, &value, sizeof( bits ) );
	return bits;
}

static void Check( int condition, const char *message ) {
	if ( !condition ) {
		fprintf( stderr, "FAIL: %s\n", message );
		exit( 1 );
	}
}

int main( void ) {
	static const float ranges[] = { 0.25f, 1.0f, 3.5f, -2.0f };
	int i;

	dllEntry( RecordSyscall );
	for ( i = 0; i < (int)( sizeof( ranges ) / sizeof( ranges[0] ) ); i++ ) {
		memset( lastArgs, 0, sizeof( lastArgs ) );
		trap_BotMutateGoalFuzzyLogic( 7, ranges[i] );
		Check( lastArgs[0] == BOTLIB_AI_MUTATE_GOAL_FUZZY_LOGIC, "mutate syscall number" );
		Check( lastArgs[1] == 7, "goal state argument" );
		Check( lastArgs[2] == FloatBits( ranges[i] ), "mutation range reaches the syscall as float bits" );
	}
	printf( "game syscall float regression passed\n" );
	return 0;
}
