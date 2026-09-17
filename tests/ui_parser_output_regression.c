/* Issue #35: exercise the real parser filename output used by UI QVMs. */
#include "../code/botlib/l_precomp.c"
#include <stdlib.h>
#include <string.h>

botlib_import_t botimport;
/** Supply allocation for parser dependencies retained by sanitizer global metadata. */
void *GetMemory( unsigned long size ) { return malloc( size ); }
/** Supply zeroed allocation for the linked script reader. */
void *GetClearedMemory( unsigned long size ) { return calloc( 1, size ); }
/** Release parser dependency allocations if they are used. */
void FreeMemory( void *memory ) { free( memory ); }
/** Supply byte copying for the linked parser dependencies. */
void Com_Memcpy( void *dest, const void *src, size_t length ) { memcpy( dest, src, length ); }
/** Supply byte filling for the linked parser dependencies. */
void Com_Memset( void *dest, int value, size_t length ) { memset( dest, value, length ); }

/** Discard diagnostics from parser dependencies outside this output fixture. */
void QDECL Com_Printf( const char *format, ... ) { (void)format; }

/** Fail on engine errors during valid parser filename output. */
void QDECL Com_Error( int level, const char *format, ... ) {
	(void)level; (void)format;
	fprintf( stderr, "Unexpected parser output error\n" ); exit( 1 );
}
/** Fail when parser output, termination, or handle rejection differs. */
static void Check( int ok ) {
	if ( !ok ) { fprintf( stderr, "Parser filename output regression failed\n" ); exit( 1 ); }
}
/** Exercise real filename output with short and long paths and invalid source handles. */
int main( void ) {
	static source_t source;
	static script_t script;
	const int lengths[] = {0, 1, MAX_QPATH - 1, MAX_QPATH, 1000};
	char *output = malloc( MAX_QPATH );
	int i, line;
	Check( output != NULL );
	sourceFiles[1] = &source;
	source.scriptstack = &script;
	script.line = 42;
	for ( i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++ ) {
		int copied = lengths[i] < MAX_QPATH ? lengths[i] : MAX_QPATH - 1;
		memset( source.filename, 'a', lengths[i] );
		source.filename[lengths[i]] = 0;
		memset( output, 0x5a, MAX_QPATH );
		line = -1;
		Check( PC_SourceFileAndLine( 1, output, &line ) == qtrue );
		Check( !memcmp( output, source.filename, copied ) && output[copied] == 0 );
		Check( line == 42 );
	}
	source.scriptstack = NULL;
	Check( PC_SourceFileAndLine( 1, output, &line ) == qtrue && line == 0 );
	memset( output, 0x5a, MAX_QPATH );
	line = -1;
	Check( PC_SourceFileAndLine( 0, output, &line ) == qfalse );
	Check( PC_SourceFileAndLine( -1, output, &line ) == qfalse );
	Check( PC_SourceFileAndLine( MAX_SOURCEFILES, output, &line ) == qfalse );
	Check( PC_SourceFileAndLine( 2, output, &line ) == qfalse );
	Check( line == -1 );
	for ( i = 0; i < MAX_QPATH; i++ ) Check( output[i] == 0x5a );
	free( output );
	puts( "UI parser filename output regressions passed (issue #35)" );
	return 0;
}
