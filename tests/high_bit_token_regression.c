/* Issue #223: bytes >= 0x80 must tokenize as retail 1.32c (signed char) did. */
#include "../code/qcommon/cmd.c"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void QDECL Com_Error( int level, const char *error, ... ) {
	va_list args;

	(void)level;
	va_start( args, error );
	vfprintf( stderr, error, args );
	va_end( args );
	fputc( '\n', stderr );
	exit( 2 );
}

void QDECL Com_Printf( const char *msg, ... ) {
	va_list args;

	va_start( args, msg );
	vfprintf( stderr, msg, args );
	va_end( args );
}

static void Check( qboolean condition, const char *message ) {
	if ( !condition ) {
		fprintf( stderr, "High-bit token regression failed: %s\n", message );
		exit( 1 );
	}
}

/** Retail SkipWhitespace/word loops treat every byte >= 0x80 as whitespace. */
static void TestComParse( void ) {
	char text[] = "\xE9" "t" "\xE9" " map";
	char quoted[] = "\"" "\xE9" "t" "\xE9" "\" x";
	char *cursor;

	cursor = text;
	Check( !strcmp( COM_Parse( &cursor ), "t" ), "COM_Parse splits a word at high-bit bytes" );
	Check( !strcmp( COM_Parse( &cursor ), "map" ), "COM_Parse skips leading high-bit bytes" );
	Check( !strcmp( COM_Parse( &cursor ), "" ) && !cursor, "COM_Parse ends after the last word" );

	cursor = "\x80\xFF";
	Check( !strcmp( COM_Parse( &cursor ), "" ) && !cursor, "COM_Parse treats high-bit-only input as empty" );

	cursor = quoted;
	Check( !strcmp( COM_Parse( &cursor ), "\xE9" "t" "\xE9" ), "COM_Parse keeps high-bit bytes inside quotes" );
	Check( !strcmp( COM_Parse( &cursor ), "x" ), "COM_Parse continues after a quoted high-bit token" );
}

/** Retail Cmd_TokenizeString splits client commands at bytes >= 0x80. */
static void TestCmdTokenize( void ) {
	Cmd_TokenizeString( "say " "\xE9" "t" "\xE9" " map" );
	Check( Cmd_Argc() == 3, "Cmd_TokenizeString high-bit argc" );
	Check( !strcmp( Cmd_Argv( 0 ), "say" ) && !strcmp( Cmd_Argv( 1 ), "t" ) && !strcmp( Cmd_Argv( 2 ), "map" ),
		"Cmd_TokenizeString high-bit argv" );

	Cmd_TokenizeString( "a" "\xFF" "b" );
	Check( Cmd_Argc() == 2 && !strcmp( Cmd_Argv( 0 ), "a" ) && !strcmp( Cmd_Argv( 1 ), "b" ),
		"Cmd_TokenizeString splits a word at a high-bit byte" );

	Cmd_TokenizeString( "\x80\xFF" );
	Check( Cmd_Argc() == 0, "Cmd_TokenizeString treats high-bit-only input as empty" );

	Cmd_TokenizeString( "say \"" "\xE9" "t" "\xE9" "\"" );
	Check( Cmd_Argc() == 2 && !strcmp( Cmd_Argv( 1 ), "\xE9" "t" "\xE9" ),
		"Cmd_TokenizeString keeps high-bit bytes inside quotes" );
	Check( !strcmp( Cmd_Cmd(), "say \"" "\xE9" "t" "\xE9" "\"" ), "Cmd_Cmd keeps the original bytes" );
}

/** Info strings only split on backslashes, so high-bit values round-trip. */
static void TestInfo( void ) {
	char info[MAX_INFO_STRING] = "\\model\\sarge";

	Info_SetValueForKey( info, "name", "\xE9" "t" "\xE9" );
	Check( !strcmp( Info_ValueForKey( info, "name" ), "\xE9" "t" "\xE9" ), "Info value keeps high-bit bytes" );
	Check( !strcmp( Info_ValueForKey( info, "model" ), "sarge" ), "Info neighbours survive high-bit values" );
}

int main( void ) {
	TestComParse();
	TestCmdTokenize();
	TestInfo();
	puts( "High-bit tokenization matches retail signed-char 1.32c (issue #223)" );
	return 0;
}
