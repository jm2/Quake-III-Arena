#include "q_shared.h"

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
		fprintf( stderr, "q_shared regression failed: %s\n", message );
		exit( 1 );
	}
}

static void TestStripExtension( void ) {
	char output[MAX_QPATH];
	char inplace[MAX_QPATH];

	COM_StripExtension( "maps/q3dm1.bsp", output, sizeof(output) );
	Check( !strcmp( output, "maps/q3dm1" ), "normal extension" );

	Q_strncpyz( inplace, "textures/base.wall.tga", sizeof(inplace) );
	COM_StripExtension( inplace, inplace, sizeof(inplace) );
	Check( !strcmp( inplace, "textures/base.wall" ), "in-place extension" );

	COM_StripExtension( "dir.with.dot/file", output, sizeof(output) );
	Check( !strcmp( output, "dir.with.dot/file" ), "dot before slash" );

	COM_StripExtension( "abcdef.ext", output, 5 );
	Check( !strcmp( output, "abcd" ), "truncated destination" );
}

static void TestBoundedFormatting( void ) {
	char output[8];

	Com_sprintf( output, sizeof(output), "%s", "1234567890" );
	Check( output[sizeof(output) - 1] == '\0', "formatter termination" );
	Check( !strcmp( output, "1234567" ), "formatter truncation" );
}

static void TestTokenTermination( void ) {
	char input[MAX_TOKEN_CHARS + 32];
	char *cursor;
	char *token;
	int i;

	for ( i = 0; i < MAX_TOKEN_CHARS + 10; i++ ) {
		input[i] = 'a';
	}
	input[i++] = ' ';
	input[i] = '\0';
	cursor = input;

	token = COM_ParseExt( &cursor, qtrue );
	Check( strlen(token) == MAX_TOKEN_CHARS - 1, "token length cap" );
	Check( token[MAX_TOKEN_CHARS - 1] == '\0', "token termination" );
}

int main( void ) {
	TestStripExtension();
	TestBoundedFormatting();
	TestTokenTermination();
	puts( "q_shared portable regressions passed" );
	return 0;
}
