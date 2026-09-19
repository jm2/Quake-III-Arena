/* Actual AAS error forwarding must not reinterpret rendered text. */
#include Q3_FORMAT_AAS_SOURCE

botlib_import_t botimport;
static char captured[2048];
static int prints;

static void Check( int condition, const char *message ) {
	if ( !condition ) {
		fprintf( stderr, "AAS format regression failed: %s\n", message );
		exit( 1 );
	}
}

static void QDECL CapturePrint( int type, char *format, ... ) {
	va_list args;
	const char *text;

	Check( type == PRT_FATAL && !strcmp( format, "%s" ),
		"rendered AAS error forwarded through a literal format" );
	va_start( args, format );
	text = va_arg( args, const char * );
	va_end( args );
	snprintf( captured, sizeof(captured), "%s", text );
	prints++;
}

int main( void ) {
	char longText[2048];
	const char *hostile = "remote %n %08x %% text";

	botimport.Print = CapturePrint;
	AAS_Error( "%s", hostile );
	Check( prints == 1 && !strcmp( captured, hostile ),
		"hostile percent text remains data" );

	memset( longText, 'a', sizeof(longText) );
	longText[sizeof(longText) - 1] = 0;
	AAS_Error( "%s", longText );
	Check( prints == 2 && strlen(captured) == 1023
		&& !memcmp(captured, longText, 1023),
		"AAS diagnostic is bounded and terminated" );

	puts( "AAS error literal forwarding and formatting capacity pass" );
	return 0;
}
