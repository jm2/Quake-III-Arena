/* CVE-2002-0059: a tree error other than a data error freed the code length
 * table and left the state in BTREE or DTREE, where inflate_blocks_reset freed
 * it again.  A dynamic block that needs a table over MANY gets Z_MEM_ERROR
 * from data alone.  Actual inflate; only the allocator is the test's. */
#include "../code/qcommon/unzip.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The deflate data of a zip entry that needs too large a table. */
static const byte badTree[] = {
	5,224,1,9,0,0,0,0,32,213,255,213,2,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};
static int live, count, failAt;

static void Check( int ok, const char *message ) {
	if ( !ok ) {
		fprintf( stderr, "Inflate tree regression failed: %s\n", message );
		exit( 1 );
	}
}
void *Z_Malloc( int size ) {
	if ( ++count == failAt ) {
		return NULL;
	}
	live++;
	return malloc( size );
}
void Z_Free( void *pointer ) {
	live--;
	free( pointer );
}
void Com_Memset( void *d, const int v, const size_t n ) { memset( d, v, n ); }
void Com_Memcpy( void *d, const void *s, const size_t n ) { memcpy( d, s, n ); }
void QDECL Com_Printf( const char *format, ... ) { (void)format; }
void QDECL Com_Error( int level, const char *format, ... ) { (void)level; Check( 0, format ); }

/** Inflate the stream, end it, and hand back inflate's result. */
static int Run( int allocationToFail ) {
	static byte output[4096];
	z_stream stream;
	int result;

	memset( &stream, 0, sizeof( stream ) );
	count = 0;
	failAt = allocationToFail;
	if ( inflateInit2( &stream, -MAX_WBITS ) != Z_OK ) {
		return Z_MEM_ERROR;
	}
	stream.next_in = (byte *)badTree;
	stream.avail_in = sizeof( badTree );
	stream.next_out = output;
	stream.avail_out = sizeof( output );
	result = inflate( &stream, Z_SYNC_FLUSH );
	Check( inflateEnd( &stream ) == Z_OK, "inflateEnd" );
	Check( live == 0, "every allocation is freed once" );
	return result;
}
int main( void ) {
	int i, allocations;

	Check( Run( 0 ) == Z_MEM_ERROR, "an oversized tree is out of table space" );
	allocations = count;
	/* every allocation failing in turn reaches the BTREE path too */
	for ( i = 1 ; i <= allocations ; i++ ) {
		Run( i );
	}
	puts( "Inflate tree regressions passed" );
	return 0;
}
