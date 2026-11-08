/* Tab completion's FindMatches with candidates longer than shortestMatch.
 *
 * CL_SystemInfoChanged creates a cvar for every unknown systeminfo key, and
 * a key may be BIG_INFO_KEY bytes long. With two such names matching what
 * was typed, the old cut loop ran to the end of the candidate, reading and
 * zeroing bytes past shortestMatch[MAX_TOKEN_CHARS] into matchCount and
 * completionField. The cut must stop at the first difference or at the end
 * of either string, and leave the common prefix. */
#include "../code/game/q_shared.h"
#include "../code/qcommon/qcommon.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "find_matches_extracted.c"

/* q_shared.c's error paths; nothing here should reach them */
void QDECL Com_Error( int level, const char *fmt, ... ) {
	(void)level;
	fprintf( stderr, "unexpected Com_Error: %s\n", fmt );
	exit( 1 );
}

void QDECL Com_Printf( const char *fmt, ... ) {
	(void)fmt;
}

static void Check( int ok, const char *message ) {
	if ( !ok ) {
		fprintf( stderr, "field complete regression failed: %s\n", message );
		exit( 1 );
	}
}

static void Start( const char *typed ) {
	completionString = typed;
	matchCount = 0;
	shortestMatch[0] = 0;
}

static char *Name( int length, int differAt, char differ ) {
	char *name = malloc( length + 1 );

	Check( name != NULL, "allocates a candidate" );
	memset( name, 'a', length );
	if ( differAt >= 0 ) {
		name[differAt] = differ;
	}
	name[length] = 0;
	return name;
}

int main( void ) {
	char	*first, *second, *third;

	/* names longer than the buffer that differ only past its end */
	first = Name( BIG_INFO_KEY - 1, MAX_TOKEN_CHARS + 100, 'x' );
	second = Name( BIG_INFO_KEY - 1, MAX_TOKEN_CHARS + 100, 'y' );
	third = Name( BIG_INFO_KEY - 1, -1, 0 );
	Start( "a" );
	FindMatches( first );
	FindMatches( second );
	FindMatches( third );
	Check( matchCount == 3, "every long candidate is counted" );
	Check( strlen( shortestMatch ) == MAX_TOKEN_CHARS - 1, "the common prefix fills the buffer" );

	/* a long candidate after a short one, and a short one after a long one */
	Start( "a" );
	FindMatches( "aaab" );
	FindMatches( first );
	Check( matchCount == 2 && !strcmp( shortestMatch, "aaa" ), "a long candidate cuts at the first difference" );
	FindMatches( "aa" );
	Check( matchCount == 3 && !strcmp( shortestMatch, "aa" ), "a shorter candidate cuts at its end" );

	/* ordinary completion: case-insensitive common prefix, non-matches ignored */
	Start( "cl_" );
	FindMatches( "cl_allowDownload" );
	FindMatches( "CL_ALLOWlist" );
	FindMatches( "com_maxfps" );
	Check( matchCount == 2 && !strcmp( shortestMatch, "cl_allow" ), "common prefix of ordinary names" );

	free( first );
	free( second );
	free( third );
	printf( "field complete regression passed\n" );
	return 0;
}
