/* Issue #35: returned strings belong to the requested VM, not currentVM. */
#include "../code/qcommon/vm_local.h"
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

#define IMAGE_SIZE 128
static vm_t owner, other;
static byte before[IMAGE_SIZE];
static int expectError;
static jmp_buf errorJump;

/** Stop on incorrect ownership, termination, or fault behavior. */
static void Check( int ok, const char *message ) {
	if ( !ok ) { fprintf( stderr, "VM returned-string regression failed: %s\n", message ); exit( 1 ); }
}
/** Catch expected drops and require only the owning VM to be faulted. */
void QDECL Com_Error( int level, const char *format, ... ) {
	(void)format;
	Check( expectError && level == ERR_DROP, "unexpected engine error" );
	Check( owner.interpretFaulted && !owner.currentlyInterpreting &&
	       !other.interpretFaulted && currentVM == &other, "wrong VM faulted or context changed" );
	Check( !memcmp( before, owner.dataBase, IMAGE_SIZE ), "rejection modified owner image" );
	longjmp( errorJump, 1 );
}
/** Native pointer checks do not execute vmMain. */
static int QDECL NativeEntry( int command, ... ) { (void)command; return 0; }
/** Require a missing terminator or required NULL to fault before native use. */
static void Reject( int value, qboolean nullable ) {
	owner.interpretFaulted = qfalse; owner.currentlyInterpreting = qtrue;
	memcpy( before, owner.dataBase, IMAGE_SIZE ); expectError = 1;
	if ( setjmp( errorJump ) == 0 ) {
		VM_CheckedExplicitString( &owner, value, nullable );
		Check( 0, "invalid returned string accepted" );
	}
	expectError = 0;
}
/** Cover exact image boundaries, aliases, nullable success, and trusted native pointers. */
int main( void ) {
	char *text;
	owner.dataBase = malloc( IMAGE_SIZE ); other.dataBase = malloc( IMAGE_SIZE );
	Check( owner.dataBase && other.dataBase, "allocation" );
	owner.dataMask = other.dataMask = IMAGE_SIZE - 1; currentVM = &other;
	memset( owner.dataBase, 'x', IMAGE_SIZE ); memset( other.dataBase, 0, IMAGE_SIZE );
	Check( VM_CheckedExplicitString( &owner, 0, qtrue ) == NULL && currentVM == &other,
	       "nullable successful connection" );
	Reject( 0, qfalse ); Reject( IMAGE_SIZE - 4, qtrue ); Reject( -1, qtrue );
	owner.interpretFaulted = qfalse; owner.currentlyInterpreting = qfalse;
	owner.dataBase[IMAGE_SIZE - 1] = 0;
	text = VM_CheckedExplicitString( &owner, IMAGE_SIZE - 4, qtrue );
	Check( text == (char *)owner.dataBase + IMAGE_SIZE - 4 && !strcmp(text, "xxx"),
	       "terminated owner boundary" );
	Check( VM_CheckedExplicitString( &owner, -1, qtrue ) == (char *)owner.dataBase + IMAGE_SIZE - 1,
	       "empty masked final byte" );
	Check( VM_CheckedExplicitString( &owner, IMAGE_SIZE + IMAGE_SIZE - 4, qtrue ) == text,
	       "positive masked owner pointer" );
	Check( VM_CheckedExplicitString( &owner, INT_MIN + IMAGE_SIZE - 4, qtrue ) == text,
	       "negative masked owner pointer" );
	Check( currentVM == &other && !owner.interpretFaulted && !other.interpretFaulted, "valid conversion changed context" );
	owner.entryPoint = NativeEntry;
	Check( (unsigned long)VM_CheckedExplicitString( &owner, -1, qtrue ) == (unsigned int)-1,
	       "trusted native 32-bit pointer" );
	Check( VM_CheckedExplicitString( &owner, 0, qtrue ) == NULL, "native successful connection" );
	free( owner.dataBase ); free( other.dataBase );
	puts( "VM returned-string ownership regressions passed (issue #35)" );
	return 0;
}
