/* Issue #302: native Info_Print field bounds and in-place Info_RemoveKey(_Big). */
#include "q_shared.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void Info_Print( const char *s );	/* qcommon.h */
void Info_RemoveKey_Big( char *s, const char *key );	/* q_shared.h spells it _big */
#include Q3_INFO_PRINT_SOURCE

static char printed[4 * BIG_INFO_STRING];
static size_t printedLength;

static void Check( int condition, const char *message ) {
	if ( !condition ) {
		fprintf( stderr, "Info string regression failed: %s\n", message );
		exit( 1 );
	}
}

void QDECL Com_Error( int level, const char *error, ... ) {
	va_list args;

	(void)level;
	va_start( args, error );
	vfprintf( stderr, error, args );
	va_end( args );
	fputc( '\n', stderr );
	exit( 2 );
}

/* Capture every console print so the exact Info_Print layout can be checked. */
void QDECL Com_Printf( const char *msg, ... ) {
	va_list args;
	int length;

	va_start( args, msg );
	length = vsnprintf( printed + printedLength, sizeof(printed) - printedLength, msg, args );
	va_end( args );
	Check( length >= 0 && (size_t)length < sizeof(printed) - printedLength, "print capture capacity" );
	printedLength += (size_t)length;
}

void Com_Memset( void *dest, const int val, const size_t count ) {
	memset( dest, val, count );
}

/* Exact-size heap copies so ASan also sees any read past the terminator. */
static char *Format( const char *format, ... ) {
	va_list args;
	char *text;
	int length;

	va_start( args, format );
	length = vsnprintf( NULL, 0, format, args );
	va_end( args );
	Check( length >= 0, "format length" );
	text = malloc( (size_t)length + 1 );
	Check( text != NULL, "format allocation" );
	va_start( args, format );
	vsnprintf( text, (size_t)length + 1, format, args );
	va_end( args );
	return text;
}

static char *Run( char c, size_t length ) {
	char *text = malloc( length + 1 );

	Check( text != NULL, "field allocation" );
	memset( text, c, length );
	text[length] = 0;
	return text;
}

static void ExpectPrint( const char *info, const char *expected, const char *message ) {
	char *copy = Format( "%s", info );

	printedLength = 0;
	printed[0] = 0;
	Info_Print( copy );
	if ( strcmp( printed, expected ) ) {
		fprintf( stderr, "printed %lu bytes, expected %lu\n",
			(unsigned long)strlen( printed ), (unsigned long)strlen( expected ) );
	}
	Check( !strcmp( printed, expected ), message );
	free( copy );
}

static void TestInfoPrintLayout( void ) {
	ExpectPrint( "\\name\\UnnamedPlayer\\rate\\25000",
		"name                UnnamedPlayer\nrate                25000\n", "short keys pad to 20 columns" );
	ExpectPrint( "name\\sarge", "name                sarge\n", "leading separator is optional" );
	ExpectPrint( "\\abcdefghijklmnopqrs\\1\\abcdefghijklmnopqrst\\2",
		"abcdefghijklmnopqrs 1\nabcdefghijklmnopqrst2\n", "20-character keys are printed unpadded" );
	ExpectPrint( "\\a\\\\b\\2", "a                   \nb                   2\n", "empty value" );
	ExpectPrint( "\\name\\x\\lonely", "name                x\nlonely              MISSING VALUE\n",
		"missing value" );
	ExpectPrint( "", "", "empty info string" );
}

/* Legal userinfo fields reach MAX_INFO_VALUE - 1; big info fields reach BIG_INFO_VALUE - 1. */
static void TestInfoPrintLongFields( void ) {
	static const size_t lengths[] = { 699, MAX_INFO_VALUE - 1, BIG_INFO_VALUE - 1 };
	size_t i;

	for ( i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++ ) {
		char *field = Run( 'v', lengths[i] );
		char *info = Format( "\\name\\%s\\rate\\25000", field );
		char *expected = Format( "name                %s\nrate                25000\n", field );

		ExpectPrint( info, expected, "long value prints intact" );
		free( info );
		free( expected );

		info = Format( "\\%s\\x\\model\\sarge", field );
		expected = Format( "%sx\nmodel               sarge\n", field );
		ExpectPrint( info, expected, "long key prints intact" );
		free( info );
		free( expected );
		free( field );
	}
}

/* Fields longer than any info string are truncated without losing the next pair. */
static void TestInfoPrintOversizedFields( void ) {
	char *field = Run( 'o', BIG_INFO_VALUE + 100 );
	char *info = Format( "\\name\\%s\\rate\\25000", field );
	char *expected = Format( "name                %.*s\nrate                25000\n", BIG_INFO_VALUE - 1, field );

	ExpectPrint( info, expected, "oversized value truncates safely" );
	free( info );
	free( expected );

	info = Format( "\\%s\\x\\model\\sarge", field );
	expected = Format( "%.*sx\nmodel               sarge\n", BIG_INFO_KEY - 1, field );
	ExpectPrint( info, expected, "oversized key truncates safely" );
	free( info );
	free( expected );
	free( field );
}

static void ExpectRemove( const char *before, const char *key, const char *after, const char *message ) {
	char info[MAX_INFO_STRING];

	Q_strncpyz( info, before, sizeof(info) );
	Info_RemoveKey( info, key );
	Check( !strcmp( info, after ), message );
}

/* Removing any non-final pair shifts the tail down over itself. */
static void TestRemoveKey( void ) {
	char info[MAX_INFO_STRING];

	ExpectRemove( "\\a\\1\\name\\x\\model\\sarge/default\\rate\\25000", "name",
		"\\a\\1\\model\\sarge/default\\rate\\25000", "middle key removed" );
	ExpectRemove( "\\name\\x\\model\\sarge/default\\rate\\25000", "name",
		"\\model\\sarge/default\\rate\\25000", "first key removed" );
	ExpectRemove( "\\a\\1\\model\\sarge/default\\rate\\25000", "rate",
		"\\a\\1\\model\\sarge/default", "last key removed" );
	ExpectRemove( "\\a\\1\\b\\2", "missing", "\\a\\1\\b\\2", "missing key leaves info unchanged" );

	Q_strncpyz( info, "\\a\\1\\name\\x\\model\\sarge/default\\rate\\25000", sizeof(info) );
	Info_SetValueForKey( info, "name", "UnnamedPlayer" );
	Check( !strcmp( info, "\\name\\UnnamedPlayer\\a\\1\\model\\sarge/default\\rate\\25000" ),
		"replacing a middle key" );
}

static void TestRemoveKeyBig( void ) {
	static char info[BIG_INFO_STRING];
	static char expected[BIG_INFO_STRING];
	char *head = Run( 'h', 1000 );
	char *middle = Run( 'm', 3000 );
	char *tail = Run( 't', 3000 );

	Com_sprintf( info, sizeof(info), "\\a\\%s\\sv_paks\\%s\\sv_pakNames\\%s", head, middle, tail );
	Com_sprintf( expected, sizeof(expected), "\\a\\%s\\sv_pakNames\\%s", head, tail );
	Info_RemoveKey_Big( info, "sv_paks" );
	Check( !strcmp( info, expected ), "big middle key removed" );

	Com_sprintf( info, sizeof(info), "\\a\\%s\\sv_paks\\%s\\sv_pakNames\\%s", head, middle, tail );
	Com_sprintf( expected, sizeof(expected), "\\sv_paks\\%s\\sv_pakNames\\%s", middle, tail );
	Info_RemoveKey_Big( info, "a" );
	Check( !strcmp( info, expected ), "big first key removed" );

	Com_sprintf( info, sizeof(info), "\\a\\%s\\sv_paks\\%s\\sv_pakNames\\%s", head, middle, tail );
	Com_sprintf( expected, sizeof(expected), "\\a\\%s\\sv_pakNames\\%s\\sv_paks\\1 2 3", head, tail );
	Info_SetValueForKey_Big( info, "sv_paks", "1 2 3" );
	Check( !strcmp( info, expected ), "replacing a big middle key" );

	free( head );
	free( middle );
	free( tail );
}

int main( void ) {
	TestInfoPrintLayout();
	TestInfoPrintLongFields();
	TestInfoPrintOversizedFields();
	TestRemoveKey();
	TestRemoveKeyBig();
	puts( "Info string regressions passed" );
	return 0;
}
