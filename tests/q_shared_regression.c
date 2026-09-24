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

/* Issue #384: exec "", writeconfig "" and a map without music
 * (S_StartBackgroundTrack( "", "" )) pass "", which has no last character. */
static void TestDefaultExtension( void ) {
	static const char *const cases[][3] = {
		{ "", ".cfg", ".cfg" },
		{ "", ".wav", ".wav" },
		{ "a", ".cfg", "a.cfg" },
		{ "q3config", ".cfg", "q3config.cfg" },
		{ "autoexec.cfg", ".cfg", "autoexec.cfg" },
		{ "music/sonic1", ".wav", "music/sonic1.wav" },
		{ "maps.v2/q3dm1", ".cfg", "maps.v2/q3dm1.cfg" },
		{ NULL, NULL, NULL }
	};
	char path[MAX_QPATH];
	int i;

	for ( i = 0; cases[i][0]; i++ ) {
		Q_strncpyz( path, cases[i][0], sizeof(path) );
		COM_DefaultExtension( path, sizeof(path), cases[i][1] );
		Check( !strcmp( path, cases[i][2] ), "default extension" );
	}
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

/* Issue #240: the big variant carries systeminfo values up to BIG_INFO_VALUE. */
static void TestBigInfoValues( void ) {
	static char info[BIG_INFO_STRING];
	static char value[BIG_INFO_VALUE + 1];
	static char small[MAX_INFO_STRING];

	memset( value, 'p', 4000 );
	value[4000] = 0;
	info[0] = 0;
	Info_SetValueForKey_Big( info, "sv_paks", value );
	Check( strlen( Info_ValueForKey( info, "sv_paks" ) ) == 4000,
		"4000-character big info value round-trips" );
	Check( !strcmp( Info_ValueForKey( info, "sv_paks" ), value ), "big info value bytes" );

	Info_SetValueForKey_Big( info, "sv_pakNames", "pak0 pak1" );
	Check( !strcmp( Info_ValueForKey( info, "sv_pakNames" ), "pak0 pak1" ), "second big key" );
	Check( strlen( Info_ValueForKey( info, "sv_paks" ) ) == 4000, "first big key kept" );

	memset( value, 'q', BIG_INFO_VALUE );
	value[BIG_INFO_VALUE] = 0;
	Info_SetValueForKey_Big( info, "sv_paks", value );
	Check( strlen( Info_ValueForKey( info, "sv_paks" ) ) == 4000,
		"BIG_INFO_VALUE-length value is rejected and the previous value kept" );
	Check( strlen( info ) < BIG_INFO_STRING, "big info string stays bounded" );

	memset( value, 'r', 5000 );
	value[5000] = 0;
	info[0] = 0;
	Info_SetValueForKey_Big( info, "a", value );
	Info_SetValueForKey_Big( info, "b", value );
	Check( strlen( Info_ValueForKey( info, "a" ) ) == 5000, "first 5000-character value stored" );
	Check( !strcmp( Info_ValueForKey( info, "b" ), "" ), "value exceeding total big capacity is refused" );

	memset( value, 's', MAX_INFO_VALUE );
	value[MAX_INFO_VALUE] = 0;
	small[0] = 0;
	Info_SetValueForKey( small, "name", value );
	Check( !strcmp( Info_ValueForKey( small, "name" ), "" ), "small info keeps MAX_INFO_VALUE limit" );
}

int main( void ) {
	TestStripExtension();
	TestDefaultExtension();
	TestBoundedFormatting();
	TestTokenTermination();
	TestBigInfoValues();
	puts( "q_shared portable regressions passed" );
	return 0;
}
