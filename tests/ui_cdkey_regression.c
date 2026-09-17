/* Call the actual private CD-key routine; discard other client code at link. */
#include "../code/client/cl_ui.c"
#include <stdlib.h>
#include <string.h>

char cl_cdkey[34] = "0123456789ABCDEFfedcba9876543210";
int cvar_modifiedFlags;
static cvar_t filesystem;
static int uniqueKey;

/** Supply byte copying for the real private UI CD-key routine. */
void Com_Memcpy( void *dest, const void *src, size_t length ) { memcpy( dest, src, length ); }
/** Fail on engine errors; all tested buffer sizes are valid. */
void QDECL Com_Error( int level, const char *format, ... ) {
	(void)level; (void)format;
	fprintf( stderr, "Unexpected UI CD-key error\n" ); exit( 1 );
}
/** Select base or mod CD-key behavior through the filesystem cvar. */
cvar_t *Cvar_Get( const char *name, const char *value, int flags ) {
	(void)name; (void)value; (void)flags; return &filesystem;
}
/** Supply the UI unique-key capability without executing a QVM. */
int VM_CallArgs( vm_t *target, int command, const int *args, int count ) {
	(void)target; (void)command; (void)args; (void)count; return uniqueKey;
}
/** Fail immediately when key bytes or output termination differ. */
static void Check( int ok ) {
	if ( !ok ) { fprintf( stderr, "UI CD-key buffer regression failed\n" ); exit( 1 ); }
}
/** Exercise base and unique mod keys through exact-sized buffers from one to twenty bytes. */
int main( void ) {
	int size, mod, unique;
	uivm = (vm_t *)&uniqueKey;
	for ( mod = 0; mod < 2; mod++ ) {
		filesystem.string = mod ? "testmod" : "";
		for ( unique = 0; unique < 2; unique++ ) {
			uniqueKey = unique;
			for ( size = 1; size <= 20; size++ ) {
				char *output = malloc( size );
				int length = size <= 17 ? size - 1 : 16;
				Check( output != NULL );
				memset( output, 0x5a, size );
				CLUI_GetCDKey( output, size );
				Check( !memcmp( output, cl_cdkey + (mod && unique ? 16 : 0), length ) );
				Check( output[length] == 0 );
				free( output );
			}
		}
	}
	puts( "UI CD-key buffer regressions passed (issue #35)" );
	return 0;
}
