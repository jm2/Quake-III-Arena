/* Issue #426: Q_rand's LCG must not overflow a signed int natively, and must
 * keep the wrapped 32-bit sequence the QVMs, demos and shotgun spread use.
 *
 * Built by tests/run_q_rand_tests.sh against the real code/game/q_math.c with
 * -fsanitize=signed-integer-overflow -fno-sanitize-recover=all, so the old
 * signed multiply aborts on the first overflowing step.
 */
#include "../code/game/q_shared.h"
#include "../code/game/bg_public.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef char q_rand_int_is_32_bits[sizeof(int) == 4 ? 1 : -1];

#define Q_RAND_STEPS	4096

/** Fail with the seed and step that diverged from the uint32_t sequence. */
static void Check( int ok, const char *message, int seed, int step ) {
	if ( !ok ) {
		fprintf( stderr, "Q_rand regression failed: %s (seed 0x%08x, step %i)\n",
			message, (unsigned)seed, step );
		exit( 1 );
	}
}

/** The wrapped retail sequence, computed where overflow is defined. */
static uint32_t ReferenceRand( uint32_t *seed ) {
	*seed = 69069u * *seed + 1u;
	return *seed;
}

/** Q_rand, Q_random and Q_crandom must match the reference bit for bit. */
static void CheckSeed( int seed, int steps ) {
	uint32_t	ref;
	int			randSeed, randomSeed, crandomSeed;
	int			step, value;
	float		expected;

	ref = (uint32_t)seed;
	randSeed = randomSeed = crandomSeed = seed;
	for ( step = 0 ; step < steps ; step++ ) {
		ReferenceRand( &ref );

		value = Q_rand( &randSeed );
		Check( (uint32_t)value == ref, "Q_rand return value", seed, step );
		Check( (uint32_t)randSeed == ref, "Q_rand seed", seed, step );

		expected = ( ref & 0xffff ) / (float)0x10000;
		Check( Q_random( &randomSeed ) == expected, "Q_random value", seed, step );
		Check( (uint32_t)randomSeed == ref, "Q_random seed", seed, step );

		expected = 2.0 * ( expected - 0.5 );
		Check( Q_crandom( &crandomSeed ) == expected, "Q_crandom value", seed, step );
		Check( (uint32_t)crandomSeed == ref, "Q_crandom seed", seed, step );
	}
}

/** Pin the first values, independent of the reference implementation. */
static void CheckKnownValues( void ) {
	static const struct {
		uint32_t	seed;
		uint32_t	values[3];
	} known[] = {
		{ 0x00000000u, { 0x00000001u, 0x00010dceu, 0x1c5983f7u } },
		{ 0x00000092u, { 0x0099deebu, 0x2a747130u, 0x5a2e1371u } },	// CG_SmokePuff
		{ 0x7fffffffu, { 0x7ffef234u, 0x63a897a5u, 0xf559d022u } },
		{ 0x80000000u, { 0x80000001u, 0x80010dceu, 0x9c5983f7u } },
		{ 0xffffffffu, { 0xfffef234u, 0xe3a897a5u, 0x7559d022u } },
	};
	int		i, step, seed;

	for ( i = 0 ; i < (int)( sizeof( known ) / sizeof( known[0] ) ) ; i++ ) {
		seed = (int)known[i].seed;
		for ( step = 0 ; step < 3 ; step++ ) {
			Check( (uint32_t)Q_rand( &seed ) == known[i].values[step], "known value",
				(int)known[i].seed, step );
		}
	}
}

int main( void ) {
	static const int seeds[] = {
		0, 1, -1, 0x7fffffff, INT_MIN, INT_MIN + 1, 0x92, 69069, 0x12345678, -123456789
	};
	int		i;

	CheckKnownValues();
	for ( i = 0 ; i < (int)( sizeof( seeds ) / sizeof( seeds[0] ) ) ; i++ ) {
		CheckSeed( seeds[i], Q_RAND_STEPS );
	}
	// every shotgun spread seed (eventParm & 255): two Q_crandom per pellet
	for ( i = 0 ; i < 256 ; i++ ) {
		CheckSeed( i, DEFAULT_SHOTGUN_COUNT * 2 );
	}

	printf( "Q_rand regression passed\n" );
	return 0;
}
