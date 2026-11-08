/* A script or source file must not be able to end the game by filling the
 * zone. botimport.GetMemory is Z_TagMalloc, which raises ERR_FATAL when no
 * block fits. Every definition body token is a 1.1 KB copy however short its
 * text, so a menu or bot file of a few thousand short #define lines asks for
 * megabytes (the live token cap allows 4.3 MB, on top of the definitions
 * themselves), and it can also fragment the zone. The parser's file-driven allocations now come from
 * botimport.TryGetMemory (Z_TryTagMalloc), and the parse fails instead. Here
 * GetMemory and TryGetMemory share one budget as the zone does; GetMemory
 * running out stands for the fatal error. Links the real l_memory.c,
 * l_precomp.c and l_script.c. */
#include "../code/game/q_shared.h"
#include "../code/game/botlib.h"
#include "../code/botlib/be_interface.h"
#include "../code/botlib/l_memory.h"
#include "../code/botlib/l_script.h"
#include "../code/botlib/l_precomp.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

botlib_import_t botimport;
botlib_globals_t botlibglobals;

#define ZONE_BUDGET		( 1 << 20 )

static long liveBytes;
static int errors;

static void Check( int ok, const char *message ) {
	if ( !ok ) { fprintf( stderr, "bot parse zone regression failed: %s\n", message ); exit( 1 ); }
}
void Com_Memset( void *out, int value, size_t size ) { memset( out, value, size ); }
void Com_Memcpy( void *out, const void *in, size_t size ) { memcpy( out, in, size ); }
void QDECL Com_Error( int level, const char *fmt, ... ) { (void)level; (void)fmt; Check( 0, "unexpected Com_Error" ); }
void QDECL Com_Printf( const char *fmt, ... ) { (void)fmt; }

static void *Take( int size ) {
	long *block;
	if ( size <= 0 || liveBytes + size > ZONE_BUDGET ) return NULL;
	block = malloc( sizeof( long ) + size );
	Check( block != NULL, "host allocation" );
	*block = size; liveBytes += size;
	return block + 1;
}
static void *FatalGetMemory( int size ) {
	void *p = Take( size );
	Check( p != NULL, "the fatal allocator ran out: a file would have ended the game" );
	return p;
}
static void *TryGetMemoryImport( int size ) { return Take( size ); }
static void FreeMemoryImport( void *p ) { long *block = (long *)p - 1; liveBytes -= *block; free( block ); }
static int AvailableMemoryImport( void ) { return ZONE_BUDGET - (int)liveBytes; }
static void QDECL Print( int type, char *fmt, ... ) { va_list ap; if ( getenv( "Q3_TEST_VERBOSE" ) ) { va_start( ap, fmt ); vfprintf( stderr, fmt, ap ); va_end( ap ); } if ( type == PRT_ERROR || type == PRT_FATAL ) errors++; }

/* Read every token; return how many, or -1 when the parse failed. */
static int Parse( const char *text, const char *expectLast ) {
	source_t *source = LoadSourceMemory( (char *)text, (int)strlen( text ), "zone-test" );
	token_t token;
	int count = 0, ok = 1;
	char last[MAX_TOKEN] = "";
	Check( source != NULL, "the source loads" );
	errors = 0;
	while ( PC_ReadToken( source, &token ) ) { count++; Q_strncpyz( last, token.string, sizeof( last ) ); }
	if ( errors ) ok = 0;
	FreeSource( source );
	if ( expectLast ) Check( !strcmp( last, expectLast ), "the last token" );
	return ok ? count : -1;
}

int main( void ) {
	static char text[64 << 10];
	int i, used = 0;

	botimport.GetMemory = FatalGetMemory;
	botimport.TryGetMemory = TryGetMemoryImport;
	botimport.FreeMemory = FreeMemoryImport;
	botimport.AvailableMemory = AvailableMemoryImport;
	botimport.Print = Print;

	/* an ordinary file still parses */
	Check( Parse( "#define VALUE 42\nfoo VALUE\n", "42" ) == 2 && liveBytes == 0, "an ordinary source parses and frees" );

	/* 3000 one-token definitions: a 45 KB file asking for over 3 MB, three times the zone */
	for ( i = 0; i < 3000; i++ ) used += snprintf( text + used, sizeof( text ) - used, "#define D%04d a\n", i );
	snprintf( text + used, sizeof( text ) - used, "after\n" );
	Check( Parse( text, NULL ) == -1, "a source that fills the zone fails its parse" );
	Check( liveBytes == 0, "the failed parse frees everything" );

	/* the script buffer fits but its punctuation table does not: a file can
	   steer the zone's free space into that window (an #include loads a new
	   script), and the load must fail, not the game */
	{
		static char small[] = "a b c\n";
		long reserve = ZONE_BUDGET - (long)( sizeof( script_t ) + strlen( small ) + 1 + sizeof( unsigned long ) ) - 16;
		script_t *script;
		liveBytes += reserve;
		script = LoadScriptMemory( small, (int)strlen( small ), "window" );
		liveBytes -= reserve;
		Check( !script && liveBytes == 0, "a punctuation table that does not fit fails the load" );
	}

	/* and the zone is usable again */
	Check( Parse( "#define VALUE 7\nVALUE\n", "7" ) == 1 && liveBytes == 0, "parsing works after the failure" );
	puts( "bot parse zone regression passed" );
	return 0;
}
