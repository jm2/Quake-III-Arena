/* Actual Com_Error formatting with platform callbacks isolated. */
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wint-to-pointer-cast"
#pragma clang diagnostic ignored "-Wnull-dereference"
#pragma clang diagnostic ignored "-Wpointer-to-int-cast"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"
#endif
#include Q3_FORMAT_COMMON_SOURCE
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

static int clockValue;
static int cvarSets;
static int disconnects;
static int flushes;

static void Check( int condition, const char *message ) {
	if ( !condition ) {
		fprintf( stderr, "Format error regression failed: %s\n", message );
		exit( 1 );
	}
}

void FS_PureServerSetLoadedPaks( const char *pakSums, const char *pakNames ) {
	Check( !pakSums[0] && !pakNames[0], "pure pak reset" );
}

int Sys_Milliseconds( void ) {
	clockValue += 1000;
	return clockValue;
}

void Cvar_Set( const char *name, const char *value ) {
	Check( !strcmp( name, "com_errorMessage" ) && value == com_errorMessage,
		"complete error cvar publication" );
	cvarSets++;
}

void CL_Disconnect( qboolean showMainMenu ) {
	Check( showMainMenu, "server disconnect returns to menu" );
	disconnects++;
}

void CL_FlushMemory( void ) {
	flushes++;
}

void QDECL Com_Printf( const char *format, ... ) {
	(void)format;
}

void SV_Shutdown( char *finalMessage ) {
	(void)finalMessage;
}

void CL_CDDialog( void ) {
	Check( qfalse, "unexpected CD dialog" );
}

void CL_Shutdown( void ) {
	Check( qfalse, "unexpected client shutdown" );
}

void Com_Shutdown( void ) {
	Check( qfalse, "unexpected common shutdown" );
}

void QDECL Sys_Error( const char *format, ... ) {
	(void)format;
	Check( qfalse, "unexpected platform fatal error" );
}

char *QDECL va( char *format, ... ) {
	static char text[MAXPRINTMSG];
	va_list args;

	va_start( args, format );
	vsnprintf( text, sizeof(text), format, args );
	va_end( args );
	text[sizeof(text) - 1] = 0;
	return text;
}

static void Run( const char *input, int expectedLength ) {
	int priorCvars = cvarSets;
	int priorDisconnects = disconnects;
	int priorFlushes = flushes;

	if ( setjmp( abortframe ) == 0 ) {
		Com_Error( ERR_SERVERDISCONNECT, "%s", input );
		Check( qfalse, "server disconnect did not abort the frame" );
	}

	Check( !com_errorEntered && (int)strlen(com_errorMessage) == expectedLength,
		"bounded, terminated engine error" );
	Check( !memcmp( com_errorMessage, input, expectedLength ),
		"engine error retains the fitting prefix" );
	Check( cvarSets == priorCvars + 1 && disconnects == priorDisconnects + 1
		&& flushes == priorFlushes + 1,
		"one error publication and disconnect" );
}

int main( void ) {
	const char *hostile = "hostile %n %08x %s text";
	char *text;
	int i;

	com_buildScript = NULL;
	com_cl_running = NULL;
	Run( hostile, strlen(hostile) );

	text = malloc( MAXPRINTMSG * 2 + 1 );
	Check( text != NULL, "long format allocation" );
	for ( i = 0; i < MAXPRINTMSG * 2; i++ ) {
		text[i] = (char)('a' + i % 26);
	}
	text[MAXPRINTMSG * 2] = 0;

	text[MAXPRINTMSG - 1] = 0;
	Run( text, MAXPRINTMSG - 1 );
	text[MAXPRINTMSG - 1] = (char)('a' + (MAXPRINTMSG - 1) % 26);
	text[MAXPRINTMSG] = 0;
	Run( text, MAXPRINTMSG - 1 );
	text[MAXPRINTMSG] = (char)('a' + MAXPRINTMSG % 26);
	text[MAXPRINTMSG * 2] = 0;
	Run( text, MAXPRINTMSG - 1 );

	free( text );
	puts( "Engine error literal text, exact capacity, truncation and termination pass" );
	return 0;
}
