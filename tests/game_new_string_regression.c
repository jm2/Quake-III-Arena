/*
 * Issue #454: G_NewString (code/game/g_spawn.c) copies a map entity value
 * into G_Alloc memory and turns "\n" into a linefeed: a backslash takes the
 * character after it, and gives a linefeed for 'n' and a backslash for
 * anything else. Its loop let a backslash that ends the value take the
 * terminating NUL as that character, so the copy got the backslash and no
 * terminator. G_ParseField makes such a copy of every string key of every
 * entity (classname, target, targetname, message, team, shader names, ...),
 * so any map, a downloaded one too, gave the server unterminated strings.
 * G_Alloc does not clear its pool, and the native game module keeps the last
 * level's bytes in it, so strlen, Q_stricmp and trap_SetConfigstring read on
 * past the copy into whatever follows it.
 *
 * This test links the real g_spawn.c. Its G_Alloc gives every copy a heap
 * block of exactly the size G_NewString asks for, filled with nonzero bytes
 * as a reused pool is, so a copy without a terminator inside its block fails
 * the test, and a read past the block fails AddressSanitizer. Entity strings
 * go through the real G_ParseSpawnVars (fed by a trap_GetEntityToken that
 * parses like the server's) and G_ParseField, as G_SpawnGEntityFromSpawnVars
 * does, with values that end in "\", "\\", "\\\" and "\n", a lone "\", plain
 * text and an empty value, at lengths that fill a 32-byte pool slot exactly
 * and at the 1023-character token limit (where the server's token cut can
 * leave a backslash last). Each copy must be terminated inside its block and
 * read as expected, and G_SpawnString must still give the raw value. Then
 * G_NewString runs on every string of up to 8 characters drawn from '\\',
 * 'n' and 'x' next to a copy of retail's loop (git show
 * dbe4ddb:code/game/g_spawn.c): each copy must be the bytes retail wrote, and
 * end there. Retail wrote no terminator exactly when the value ends in an
 * unpaired backslash; for every other value the copy is retail's, byte for
 * byte, so only that case changes, and it now reads as retail wrote it, with
 * the backslash last.
 */
#include "../code/game/g_local.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

level_locals_t	level;

static const char	*testCase = "setup";

static void Fail( const char *message ) {
	fprintf( stderr, "G_NewString regression failed: %s: %s\n", testCase, message );
	exit( 1 );
}
static void Unexpected( const char *name ) {
	fprintf( stderr, "G_NewString regression failed: %s: unexpected call to %s\n", testCase, name );
	exit( 1 );
}

/* --- engine traps and game functions the spawn path reaches --- */
static const char	*entityParsePoint;

/** As the server's G_GET_ENTITY_TOKEN: the next COM_Parse token, false at the end of the string. */
qboolean trap_GetEntityToken( char *buffer, int bufferSize ) {
	const char *s = COM_Parse( (char **)&entityParsePoint );

	Q_strncpyz( buffer, s, bufferSize );
	return entityParsePoint || s[0];
}

/*
 * Every G_Alloc block, so each copy can be checked against the size it was
 * given. The pool's bytes are never cleared, so the blocks start nonzero.
 */
#define MAX_TEST_ALLOCS	16
static char	*allocBlocks[MAX_TEST_ALLOCS];
static int	allocSizes[MAX_TEST_ALLOCS];
static int	numAllocs;

void *G_Alloc( int size ) {
	char *p;

	if ( size <= 0 || numAllocs == MAX_TEST_ALLOCS ) {
		Fail( "unexpected G_Alloc" );
	}
	p = malloc( size );
	if ( !p ) {
		Fail( "out of memory" );
	}
	memset( p, 'Q', size );
	allocBlocks[numAllocs] = p;
	allocSizes[numAllocs] = size;
	numAllocs++;
	return p;
}
static void FreeAllocs( void ) {
	while ( numAllocs > 0 ) {
		numAllocs--;
		free( allocBlocks[numAllocs] );
	}
}
void QDECL G_Printf( const char *fmt, ... ) { (void)fmt; Unexpected( "G_Printf" ); }
void QDECL G_Error( const char *fmt, ... ) { (void)fmt; Unexpected( "G_Error" ); }
void QDECL Com_Error( int level, const char *error, ... ) { (void)level; (void)error; Unexpected( "Com_Error" ); }
void QDECL Com_Printf( const char *msg, ... ) { (void)msg; Unexpected( "Com_Printf" ); }

/* g_spawn.c does not export these through g_local.h. */
qboolean G_ParseSpawnVars( void );
void G_ParseField( const char *key, const char *value, gentity_t *ent );

/**
 * A copy must sit at the start of its own block of strlen( value ) + 1
 * bytes, be terminated inside it and read as expected.
 */
static void CheckCopy( const char *copy, const char *value, const char *expected ) {
	int i;

	for ( i = 0 ; i < numAllocs ; i++ ) {
		if ( allocBlocks[i] == copy ) {
			break;
		}
	}
	if ( i == numAllocs ) {
		Fail( "the copy is not a G_Alloc block" );
	}
	if ( allocSizes[i] != (int)strlen( value ) + 1 ) {
		Fail( "G_NewString asked for a block of another size" );
	}
	if ( !memchr( copy, 0, allocSizes[i] ) ) {
		Fail( "the copy has no terminator inside its block" );
	}
	if ( strcmp( copy, expected ) ) {
		fprintf( stderr, "copy \"%s\", expected \"%s\"\n", copy, expected );
		Fail( "the copy reads differently" );
	}
}

/* --- map entity values --- */
typedef struct {
	const char	*value;		// as the map has it, between the quotes
	const char	*expected;	// G_NewString's copy
} valueCase_t;

static const valueCase_t valueCases[] = {
	// plain text and an empty value are copied as they are
	{ "Welcome to the arena", "Welcome to the arena" },
	{ "", "" },
	{ "x", "x" },
	// "\n" gives a linefeed, and a backslash before anything else a backslash
	{ "first line\\nsecond line", "first line\nsecond line" },
	{ "ends in a linefeed\\n", "ends in a linefeed\n" },
	{ "\\n", "\n" },
	{ "\\n\\n", "\n\n" },
	{ "a\\xb", "a\\b" },
	{ "\\\\n", "\\n" },
	{ "ends in a pair\\\\", "ends in a pair\\" },
	{ "\\\\", "\\" },
	{ "C:\\\\maps\\\\", "C:\\maps\\" },
	// a backslash that ends the value, which took the terminator: it is now
	// copied as it is, as retail wrote it
	{ "\\", "\\" },
	{ "abc\\", "abc\\" },
	{ "abc\\\\\\", "abc\\\\" },
	{ "a linefeed and\\n\\", "a linefeed and\n\\" },
	{ "\\x\\", "\\\\" },
	{ "textures/base_wall/\\", "textures/base_wall/\\" },
};
#define NUM_VALUE_CASES	( sizeof( valueCases ) / sizeof( valueCases[0] ) )

/* The string keys every value goes into; each one is a G_NewString copy. */
#define NUM_STRING_KEYS	3
static const char	*stringKeys[NUM_STRING_KEYS] = { "message", "targetname", "team" };

/**
 * Parse one entity with the value under each string key, as
 * G_SpawnEntitiesFromString and G_SpawnGEntityFromSpawnVars do, and check
 * every copy and G_SpawnString's raw value.
 */
static void SpawnValue( const char *value, const char *expected ) {
	static char	entityString[8192];
	gentity_t	ent;
	char		*s;
	char		*copies[NUM_STRING_KEYS];
	int			i;

	snprintf( entityString, sizeof( entityString ),
		"{\n\"classname\" \"target_print\"\n\"%s\" \"%s\"\n\"%s\" \"%s\"\n\"%s\" \"%s\"\n}\n",
		stringKeys[0], value, stringKeys[1], value, stringKeys[2], value );

	memset( &level, 0, sizeof( level ) );
	memset( &ent, 0, sizeof( ent ) );
	level.spawning = qtrue;
	entityParsePoint = entityString;
	if ( !G_ParseSpawnVars() ) {
		Fail( "G_ParseSpawnVars found no entity" );
	}
	if ( level.numSpawnVars != 1 + NUM_STRING_KEYS ) {
		Fail( "G_ParseSpawnVars found other keys" );
	}
	for ( i = 0 ; i < level.numSpawnVars ; i++ ) {
		G_ParseField( level.spawnVars[i][0], level.spawnVars[i][1], &ent );
	}

	CheckCopy( ent.classname, "target_print", "target_print" );
	copies[0] = ent.message;
	copies[1] = ent.targetname;
	copies[2] = ent.team;
	for ( i = 0 ; i < NUM_STRING_KEYS ; i++ ) {
		CheckCopy( copies[i], value, expected );
		// the spawn functions' G_SpawnString still reads the raw value
		if ( !G_SpawnString( stringKeys[i], "default", &s ) || strcmp( s, value ) ) {
			Fail( "G_SpawnString changed the raw value" );
		}
	}
	if ( numAllocs != 1 + NUM_STRING_KEYS ) {
		Fail( "G_ParseField made other copies" );
	}
	if ( G_ParseSpawnVars() ) {
		Fail( "the entity string held more than one entity" );
	}
	FreeAllocs();
}

static void TestMapValues( void ) {
	static char	value[2048], expected[2048];
	static const int	lengths[] = { 31, 32, 63, 64, 1023 };
	char		name[128];
	size_t		c;
	int			l;

	for ( c = 0 ; c < NUM_VALUE_CASES ; c++ ) {
		snprintf( name, sizeof( name ), "map value \"%s\"", valueCases[c].value );
		testCase = name;
		SpawnValue( valueCases[c].value, valueCases[c].expected );
	}

	// long values whose copy fills a 32-byte pool slot exactly, or not, up
	// to the token limit, with and without a backslash last
	for ( c = 0 ; c < sizeof( lengths ) / sizeof( lengths[0] ) ; c++ ) {
		l = lengths[c];
		snprintf( name, sizeof( name ), "%d-character value ending in a backslash", l );
		testCase = name;
		memset( value, 'a', l - 1 );
		value[l - 1] = '\\';
		value[l] = 0;
		SpawnValue( value, value );

		snprintf( name, sizeof( name ), "%d-character value ending in \\n", l );
		testCase = name;
		memset( value, 'a', l - 2 );
		value[l - 2] = '\\';
		value[l - 1] = 'n';
		value[l] = 0;
		memcpy( expected, value, l - 2 );
		expected[l - 2] = '\n';
		expected[l - 1] = 0;
		SpawnValue( value, expected );
	}

	// the server's token stops at 1023 characters, so a longer value with a
	// backslash at that place reaches G_NewString with the backslash last
	testCase = "value cut after a backslash at the token limit";
	memset( value, 'a', MAX_TOKEN_CHARS - 2 );
	strcpy( value + MAX_TOKEN_CHARS - 2, "\\nmore text" );
	snprintf( expected, sizeof( expected ), "%.*s\\", MAX_TOKEN_CHARS - 2, value );
	{
		gentity_t	ent;
		char		entityString[2048];

		snprintf( entityString, sizeof( entityString ), "{\n\"message\" \"%s\"\n}\n", value );
		memset( &level, 0, sizeof( level ) );
		memset( &ent, 0, sizeof( ent ) );
		level.spawning = qtrue;
		entityParsePoint = entityString;
		if ( !G_ParseSpawnVars() || level.numSpawnVars != 1 ) {
			Fail( "G_ParseSpawnVars did not find the one key" );
		}
		if ( strcmp( level.spawnVars[0][1], expected ) ) {
			Fail( "the server's token was not cut after the backslash" );
		}
		G_ParseField( level.spawnVars[0][0], level.spawnVars[0][1], &ent );
		CheckCopy( ent.message, expected, expected );
		FreeAllocs();
	}
}

/* --- every short value, next to retail's loop --- */

/**
 * Retail 1.32c's G_NewString loop (git show dbe4ddb:code/game/g_spawn.c),
 * writing into out, which holds strlen( string ) + 1 bytes. Returns how many
 * bytes it wrote.
 */
static int RetailNewString( const char *string, char *out ) {
	char	*new_p;
	int		i,l;

	l = strlen(string) + 1;

	new_p = out;

	// turn \n into a real linefeed
	for ( i=0 ; i< l ; i++ ) {
		if (string[i] == '\\' && i < l-1) {
			i++;
			if (string[i] == 'n') {
				*new_p++ = '\n';
			} else {
				*new_p++ = '\\';
			}
		} else {
			*new_p++ = string[i];
		}
	}

	return new_p - out;
}

/** True when the value ends in an odd run of backslashes, whose last one has nothing after it to take. */
static qboolean EndsInUnpairedBackslash( const char *string ) {
	int l, run;

	l = strlen( string );
	for ( run = 0 ; run < l && string[l - 1 - run] == '\\' ; run++ ) {
	}
	return ( run & 1 ) ? qtrue : qfalse;
}

#define MAX_SHORT_LENGTH	8

static void TestShortValues( void ) {
	static const char	alphabet[] = { '\\', 'n', 'x' };
	char		value[MAX_SHORT_LENGTH + 1];
	char		retail[MAX_SHORT_LENGTH + 2];
	char		*copy;
	int			digits[MAX_SHORT_LENGTH];
	int			length, written, i, values, changed;
	qboolean	retailTerminated;

	testCase = "short values";
	values = changed = 0;
	for ( length = 0 ; length <= MAX_SHORT_LENGTH ; length++ ) {
		memset( digits, 0, sizeof( digits ) );
		while ( 1 ) {
			for ( i = 0 ; i < length ; i++ ) {
				value[i] = alphabet[digits[i]];
			}
			value[length] = 0;

			memset( retail, 'Q', sizeof( retail ) );
			written = RetailNewString( value, retail );
			if ( written > length + 1 ) {
				Fail( "retail's loop wrote past its block" );
			}
			retailTerminated = memchr( retail, 0, written ) ? qtrue : qfalse;
			if ( retailTerminated == EndsInUnpairedBackslash( value ) ) {
				fprintf( stderr, "value \"%s\"\n", value );
				Fail( "retail's terminator does not follow the trailing backslash rule" );
			}
			if ( !retailTerminated ) {
				// the bytes retail wrote, then the terminator it left out
				retail[written] = 0;
				changed++;
			}

			copy = G_NewString( value );
			CheckCopy( copy, value, retail );
			if ( retailTerminated && memcmp( copy, retail, written ) ) {
				fprintf( stderr, "value \"%s\"\n", value );
				Fail( "the copy is not retail's, byte for byte" );
			}
			FreeAllocs();
			values++;

			for ( i = 0 ; i < length && ++digits[i] == (int)sizeof( alphabet ) ; i++ ) {
				digits[i] = 0;
			}
			if ( i == length ) {
				break;
			}
		}
	}
	printf( "G_NewString copies %d short values as retail does, %d of them ending in an unpaired backslash now terminated\n",
		values, changed );
}

int main( void ) {
	TestMapValues();
	TestShortValues();
	printf( "G_NewString terminates %d map values, long values and a cut token through G_ParseSpawnVars and G_ParseField (issue #454)\n",
		(int)NUM_VALUE_CASES );
	return 0;
}
