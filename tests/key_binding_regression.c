/* Issue #223 follow-up: high-bit key names and out-of-range keynums must stay inside keys[]. */
#include "../code/client/cl_keys.c"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int cvar_modifiedFlags;

void QDECL Com_Error( int level, const char *error, ... ) {
	va_list args;

	(void)level;
	va_start( args, error );
	vfprintf( stderr, error, args );
	va_end( args );
	fputc( '\n', stderr );
	exit( 2 );
}

/** Ignore "isn't a valid key" and binding listings. */
void QDECL Com_Printf( const char *msg, ... ) {
	(void)msg;
}

char *CopyString( const char *in ) {
	char *out = malloc( strlen( in ) + 1 );

	if ( !out ) {
		Com_Error( ERR_FATAL, "CopyString: out of memory" );
	}
	strcpy( out, in );
	return out;
}

void Z_Free( void *ptr ) {
	free( ptr );
}

static void Check( qboolean condition, const char *message ) {
	if ( !condition ) {
		fprintf( stderr, "Key binding regression failed: %s\n", message );
		exit( 1 );
	}
}

/** A quoted high-bit key name binds, reads back and unbinds through its unsigned keynum. */
static void TestHighBitName( void ) {
	Check( Key_StringToKeynum( "\xE9" ) == 0xE9, "high-bit key name maps to its unsigned keynum" );
	Check( Key_StringToKeynum( "0xe9" ) == 0xE9, "hex key name round-trips" );

	Cmd_TokenizeString( "bind \"\xE9\" \"say x\"" );
	Key_Bind_f();
	Check( keys[0xE9].binding && !strcmp( keys[0xE9].binding, "say x" ), "bind stores the high-bit key" );
	Check( !strcmp( Key_GetBinding( 0xE9 ), "say x" ), "binding reads back" );
	Check( !strcmp( Key_KeynumToString( 0xE9 ), "0xe9" ), "high-bit key is written as hex" );

	Cmd_TokenizeString( "bind \"\xE9\"" );
	Key_Bind_f();
	Cmd_TokenizeString( "unbind \"\xE9\"" );
	Key_Unbind_f();
	Check( keys[0xE9].binding && !keys[0xE9].binding[0], "unbind clears the high-bit key" );
}

/** Direct (UI syscall) keynums outside keys[] are ignored instead of indexing out of bounds. */
static void TestRange( void ) {
	static const int bad[] = { -23, -2, -1, MAX_KEYS, MAX_KEYS + 1, 0x7fffffff };
	qkey_t saved[MAX_KEYS];
	int i;

	memcpy( saved, keys, sizeof( keys ) );
	for ( i = 0; i < (int)( sizeof( bad ) / sizeof( bad[0] ) ); i++ ) {
		Key_SetBinding( bad[i], "say y" );
		Check( !strcmp( Key_GetBinding( bad[i] ), "" ), "out-of-range binding reads empty" );
		Check( !Key_IsDown( bad[i] ), "out-of-range key is never down" );
	}
	Check( !memcmp( saved, keys, sizeof( keys ) ), "out-of-range keynums leave keys[] untouched" );

	Key_SetBinding( MAX_KEYS - 1, "say z" );
	Check( !strcmp( Key_GetBinding( MAX_KEYS - 1 ), "say z" ), "last key still binds" );
}

int main( void ) {
	TestHighBitName();
	TestRange();
	puts( "High-bit and out-of-range key bindings stay inside keys[] (issue #223)" );
	return 0;
}
