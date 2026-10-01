/* Issue #450: the game, cgame and UI modules are linked natively, and their
 * print and error functions formatted into fixed buffers with vsprintf. Text
 * that expanded past the buffer overflowed the native stack (the 1024-byte
 * buffers) or a static buffer (COM_ParseError/Warning and PC_SourceWarning/
 * Error at 4096, va at 32000). Callers pass text a client, a server or a map
 * controls: a vsay echoed on a dedicated server, a map's entity tokens, a
 * server's command name or game version string.
 * The runner extracts each function verbatim (with a #line so reports name its
 * real source) into the file Q3_TEST_BODIES names and builds this fixture once
 * per module, linking the real q_shared.c where the functions need it. The
 * traps they print through are replaced here to record the text passed.
 * Each function formats a short message, then text that expands to one byte
 * less than its buffer, exactly its buffer, and several times its buffer: the
 * first two must arrive byte-identical, the others cut after the buffer's last
 * byte and terminated. */
#include "../code/game/q_shared.h"
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FORMAT "%s|%d\n"	/* every call formats a payload and ARG */
#define ARG 7
#define FORMAT_EXTRA 3		/* what FORMAT adds to the payload: "|7\n" */

#define PARSE_NAME "scripts/test.shader"
#define SOURCE_HANDLE 5
#define SOURCE_FILE "ui/test.menu"
#define SOURCE_LINE 42
#define SOURCE_LINE_TEXT "42"

static char captured[65536];
static int capturedLength;
static int captures;
static jmp_buf errorJump;
static const char *current = "";

/** Fail with the function under test and what went wrong. */
static void Check( int ok, const char *what ) {
	if ( !ok ) {
		fprintf( stderr, "module print bounds regression failed: %s %s: %s\n",
			Q3_TEST_MODULE, current, what );
		exit( 1 );
	}
}

/** Record the text a trap was passed; strlen reads to its terminator. */
static void Capture( const char *text ) {
	size_t length = strlen( text );

	Check( length < sizeof( captured ), "the text fits the capture" );
	memcpy( captured, text, length + 1 );
	capturedLength = (int)length;
	captures++;
}

typedef struct {
	const char	*name;
	void		(*call)( const char *payload );	/* the function with FORMAT */
	int			size;		/* the function's buffer */
	int			longText;	/* the "several times the buffer" case */
	qboolean	error;		/* the call ends in trap_Error */
	const char	*prefix;	/* what the recorded text holds before ... */
	const char	*suffix;	/* ... and after the formatted (cut) text */
} printer_t;

#if defined( Q3_TEST_GAME )

void trap_Printf( const char *text ) { Capture( text ); }
void trap_Error( const char *text ) { Capture( text ); longjmp( errorJump, 1 ); }
void QDECL G_Printf( const char *fmt, ... );
void QDECL G_Error( const char *fmt, ... );

#include Q3_TEST_BODIES

static void Call_G_Printf( const char *s ) { G_Printf( FORMAT, s, ARG ); }
static void Call_G_Error( const char *s ) { G_Error( FORMAT, s, ARG ); }
static void Call_Com_Printf( const char *s ) { Com_Printf( FORMAT, s, ARG ); }
static void Call_Com_Error( const char *s ) { Com_Error( ERR_DROP, FORMAT, s, ARG ); }

static const printer_t printers[] = {
	{ "G_Printf", Call_G_Printf, 1024, 4000, qfalse, "", "" },
	{ "G_Error", Call_G_Error, 1024, 4000, qtrue, "", "" },
	{ "Com_Printf", Call_Com_Printf, 1024, 4000, qfalse, "", "" },
	{ "Com_Error", Call_Com_Error, 1024, 4000, qtrue, "", "" },
};

#elif defined( Q3_TEST_CGAME )

void trap_Print( const char *text ) { Capture( text ); }
void trap_Error( const char *text ) { Capture( text ); longjmp( errorJump, 1 ); }
void QDECL CG_Printf( const char *msg, ... );
void QDECL CG_Error( const char *msg, ... );

#include Q3_TEST_BODIES

static void Call_CG_Printf( const char *s ) { CG_Printf( FORMAT, s, ARG ); }
static void Call_CG_Error( const char *s ) { CG_Error( FORMAT, s, ARG ); }
static void Call_Com_Printf( const char *s ) { Com_Printf( FORMAT, s, ARG ); }
static void Call_Com_Error( const char *s ) { Com_Error( ERR_DROP, FORMAT, s, ARG ); }

static const printer_t printers[] = {
	{ "CG_Printf", Call_CG_Printf, 1024, 4000, qfalse, "", "" },
	{ "CG_Error", Call_CG_Error, 1024, 4000, qtrue, "", "" },
	{ "Com_Printf", Call_Com_Printf, 1024, 4000, qfalse, "", "" },
	{ "Com_Error", Call_Com_Error, 1024, 4000, qtrue, "", "" },
};

#elif defined( Q3_TEST_UI )	/* q3_ui or Team Arena ui_atoms.c */

void trap_Print( const char *text ) { Capture( text ); }
void trap_Error( const char *text ) { Capture( text ); longjmp( errorJump, 1 ); }

#include Q3_TEST_BODIES

static void Call_Com_Printf( const char *s ) { Com_Printf( FORMAT, s, ARG ); }
static void Call_Com_Error( const char *s ) { Com_Error( ERR_DROP, FORMAT, s, ARG ); }

static const printer_t printers[] = {
	{ "Com_Printf", Call_Com_Printf, 1024, 4000, qfalse, "", "" },
	{ "Com_Error", Call_Com_Error, 1024, 4000, qtrue, "", "" },
};

#elif defined( Q3_TEST_SHARED )	/* q_shared.c and Team Arena ui_shared.c */

/* These print through the module's Com_Printf; this one records the whole
 * text, so what is checked is the cut made by the function under test. */
void QDECL Com_Printf( const char *msg, ... ) {
	va_list argptr;
	static char text[sizeof( captured )];
	int length;

	va_start( argptr, msg );
	length = vsnprintf( text, sizeof( text ), msg, argptr );
	va_end( argptr );
	Check( length >= 0 && length < (int)sizeof( text ), "Com_Printf's text fits the capture" );
	Capture( text );
}

void QDECL Com_Error( int level, const char *error, ... ) {
	(void)level;
	Check( 0, error );
}

int trap_PC_SourceFileAndLine( int handle, char *filename, int *line ) {
	Check( handle == SOURCE_HANDLE, "the caller's handle reaches the engine" );
	strcpy( filename, SOURCE_FILE );
	*line = SOURCE_LINE;
	return qtrue;
}

void PC_SourceWarning( int handle, char *format, ... );
void PC_SourceError( int handle, char *format, ... );

#include Q3_TEST_BODIES

static void Call_COM_ParseError( const char *s ) { COM_ParseError( FORMAT, s, ARG ); }
static void Call_COM_ParseWarning( const char *s ) { COM_ParseWarning( FORMAT, s, ARG ); }
static void Call_PC_SourceWarning( const char *s ) { PC_SourceWarning( SOURCE_HANDLE, FORMAT, s, ARG ); }
static void Call_PC_SourceError( const char *s ) { PC_SourceError( SOURCE_HANDLE, FORMAT, s, ARG ); }
/* va returns its text; it alternates between two buffers, so call it twice */
static void Call_va( const char *s ) {
	char *first = va( FORMAT, s, ARG );
	char *second;

	Capture( first );
	captures = 0;
	second = va( FORMAT, s, ARG );
	Check( second != first, "va alternates its buffers" );
	Check( strlen( first ) == (size_t)capturedLength && !memcmp( first, captured, capturedLength ),
		"va's second call leaves the first text alone" );
	Capture( second );
}

static const printer_t printers[] = {
	{ "COM_ParseError", Call_COM_ParseError, 4096, 16000, qfalse,
		"ERROR: " PARSE_NAME ", line 0: ", "\n" },
	{ "COM_ParseWarning", Call_COM_ParseWarning, 4096, 16000, qfalse,
		"WARNING: " PARSE_NAME ", line 0: ", "\n" },
	{ "PC_SourceWarning", Call_PC_SourceWarning, 4096, 16000, qfalse,
		S_COLOR_YELLOW "WARNING: " SOURCE_FILE ", line " SOURCE_LINE_TEXT ": ", "\n" },
	{ "PC_SourceError", Call_PC_SourceError, 4096, 16000, qfalse,
		S_COLOR_RED "ERROR: " SOURCE_FILE ", line " SOURCE_LINE_TEXT ": ", "\n" },
	{ "va", Call_va, 32000, 40000, qfalse, "", "" },
};

#else
#error "define the module under test"
#endif

static char payload[65536];
static char expanded[65536];
static char expected[65536];

/** Call the function; qtrue if it ended in trap_Error. */
static qboolean Invoke( const printer_t *printer ) {
	captures = 0;
	if ( setjmp( errorJump ) ) {
		return qtrue;
	}
	printer->call( payload );
	return qfalse;
}

/** Format text that expands to length bytes (0: a short message). */
static void Run( const printer_t *printer, int length ) {
	int expandedLength, kept, n;
	int i;

	if ( length ) {
		Check( length > FORMAT_EXTRA && length < (int)sizeof( payload ), "the case fits the payload" );
		for ( i = 0; i < length - FORMAT_EXTRA; i++ ) {
			payload[i] = 'a' + i % 26;
		}
		payload[i] = '\0';
	} else {
		strcpy( payload, "Player said: hello, world" );
	}

	expandedLength = snprintf( expanded, sizeof( expanded ), FORMAT, payload, ARG );
	Check( expandedLength >= 0 && expandedLength < (int)sizeof( expanded ), "the case fits the expansion" );
	Check( !length || expandedLength == length, "the case expands to its length" );
	kept = expandedLength < printer->size ? expandedLength : printer->size - 1;
	n = snprintf( expected, sizeof( expected ), "%s%.*s%s", printer->prefix, kept, expanded, printer->suffix );
	Check( n >= 0 && n < (int)sizeof( expected ), "the expected text fits" );

	Check( Invoke( printer ) == printer->error, printer->error ? "the error ends in trap_Error" : "the print returns" );
	Check( captures == 1, "the text reaches the trap once" );
	Check( capturedLength == n, length >= printer->size
		? "the text is cut after the buffer's last byte" : "text that fits arrives whole" );
	Check( !memcmp( captured, expected, n + 1 ), "the text arrives byte-identical up to the cut" );
}

int main( int argc, char **argv ) {
	const printer_t *printer = NULL;
	size_t i;

	for ( i = 0; argc == 2 && i < sizeof( printers ) / sizeof( printers[0] ); i++ ) {
		if ( !strcmp( argv[1], printers[i].name ) ) {
			printer = &printers[i];
		}
	}
	if ( !printer ) {
		fprintf( stderr, "usage: %s <function>\n", argv[0] );
		return 2;
	}
	current = printer->name;
#if defined( Q3_TEST_SHARED )
	COM_BeginParseSession( PARSE_NAME );
#endif

	Run( printer, 0 );
	Run( printer, printer->size - 1 );
	Run( printer, printer->size );
	Run( printer, printer->longText );
	printf( "module print bounds: %s %s cuts at %d bytes\n", Q3_TEST_MODULE, printer->name, printer->size - 1 );
	return 0;
}
