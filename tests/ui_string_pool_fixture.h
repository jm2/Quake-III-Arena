/* Issues #49 and #467: helpers for the Team Arena UI string pool fixtures.
 * String_Alloc (code/ui/ui_shared.c) returns NULL once a new string no longer
 * fits its pool; strings already in the pool are still found and returned.
 * Include it after the module source. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** Fail with a description of the first mismatch. */
static void Check( int ok, const char *what ) {
	if ( !ok ) {
		fprintf( stderr, "UI string pool regression failed: %s\n", what );
		exit( 1 );
	}
}

/** Read every byte of s, as the menu text painter and the renderer's shader
 * lookup do with list entries; a NULL entry stops the run under ASan. */
static size_t ReadString( const char *s ) {
	volatile size_t length = strlen( s );
	return length;
}

/** Fill the string pool with new strings, longest first, until not even a new
 * one-character string fits. Afterwards String_Alloc returns NULL for every
 * string that is not already in the pool. The filler strings use only the
 * punctuation below, so they never match a string a test loads. */
static void FillStringPool( void ) {
	static const char digits[] = "~!#$%&*+-=?@^_|";
	static char text[1025];
	int length, i;
	long serial, value, limit;

	for ( length = 1024; length > 0; length /= 2 ) {
		limit = 1;
		for ( i = 0; i < length && i < 7; i++ ) {
			limit *= sizeof( digits ) - 1;
		}
		for ( serial = 0; serial < limit; serial++ ) {
			value = serial;
			for ( i = 0; i < length; i++ ) {
				text[i] = digits[value % ( sizeof( digits ) - 1 )];
				value /= sizeof( digits ) - 1;
			}
			text[length] = 0;
			if ( !String_Alloc( text ) ) {
				break;
			}
		}
	}
	Check( String_Alloc( "Z" ) == NULL, "pool takes no new one-character string" );
	Check( String_Alloc( "" ) != NULL && *String_Alloc( "" ) == 0, "the empty string needs no pool space" );
}
