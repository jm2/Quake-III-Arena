/* Exercise the actual QVM formatter body with host varargs and sanitizers. */
#define Q3_FORMATTER_TEST 1
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-non-prototype"
#endif
#include Q3_FORMATTER_SOURCE
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
#ifdef Q_vsnprintf
#error QVM formatter regression fell back to the platform formatter macro
#endif

typedef struct {
	unsigned char before[16];
	char output[8];
	unsigned char after[16];
} guardedOutput_t;

static void Check( int condition, const char *message ) {
	if ( !condition ) {
		fprintf( stderr, "QVM formatter regression failed: %s\n", message );
		exit( 1 );
	}
}

static int Format( char *output, size_t size, const char *format, ... ) {
	va_list args;
	int result;

	va_start( args, format );
	result = Q_vsnprintf( output, size, format, args );
	va_end( args );
	return result;
}

static void CheckGuard( const unsigned char *guard, size_t size ) {
	size_t i;

	for ( i = 0; i < size; i++ ) {
		Check( guard[i] == 0xa5, "formatter changed an adjacent byte" );
	}
}

int main( void ) {
	guardedOutput_t guarded;
	char exact[11];
	char one[1] = { 'x' };
	char formats[128];
	char marker = 'z';
	int length;

	memset( &guarded, 0xa5, sizeof(guarded) );
	length = Format( guarded.output, sizeof(guarded.output), "%s", "1234567890" );
	Check( length == 10, "truncation returns the complete output length" );
	Check( !strcmp( guarded.output, "1234567" ), "truncation retains the fitting prefix" );
	CheckGuard( guarded.before, sizeof(guarded.before) );
	CheckGuard( guarded.after, sizeof(guarded.after) );

	length = Format( exact, sizeof(exact), "%s", "1234567890" );
	Check( length == 10 && !strcmp( exact, "1234567890" ),
		"exact-capacity output includes its terminator" );

	length = Format( one, sizeof(one), "%s", "abc" );
	Check( length == 3 && one[0] == '\0', "one-byte output is terminated" );
	length = Format( &marker, 0, "%s", "abc" );
	Check( length == 3 && marker == 'z', "zero capacity performs no write" );
	length = Format( NULL, 0, "value %d", 12 );
	Check( length == 8, "NULL zero-capacity sizing is supported" );
	length = Format( formats, sizeof(formats), "%s", (char *)NULL );
	Check( length == 6 && !strcmp( formats, "<NULL>" ),
		"QVM formatter body handles a NULL string" );

	length = Format(
		formats, sizeof(formats),
		"%s|%d|%u|%08x|%-5s|%.3s|%.2f|%c|%%",
		"text", -12, 34u, 0x2au, "x", "abcdef", 1.25, 'Q'
	);
	Check( length == strlen(formats), "untruncated return length matches output" );
	Check( !strcmp( formats, "text|-12|34|0000002a|x    |abc|1.25|Q|%" ),
		"legacy integer, string, width, precision and float formats" );
	length = Format( formats, sizeof(formats), "%d|%u", INT_MIN, UINT_MAX );
	Check( length == 22 && !strcmp( formats, "-2147483648|4294967295" ),
		"full signed and unsigned 32-bit integer magnitudes" );

	puts( "QVM bounded formatter capacity and format goldens pass" );
	return 0;
}
