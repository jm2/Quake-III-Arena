/* Issue #35: common syscall buffers must not escape the VM allocation. */
#include "../code/qcommon/vm_local.h"
#include <limits.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IMAGE_SIZE 128
static vm_t vm;
static byte before[IMAGE_SIZE];
static int expectError;
static jmp_buf errorJump;

static void Check( int ok, const char *message ) {
	if ( !ok ) {
		fprintf( stderr, "VM memory trap regression failed: %s\n", message );
		exit( 1 );
	}
}
void QDECL Com_Error( int level, const char *format, ... ) {
	(void)format;
	Check( expectError && level == ERR_DROP, "unexpected engine error" );
	Check( vm.interpretFaulted && !vm.currentlyInterpreting, "fault state" );
	Check( !memcmp( before, vm.dataBase, IMAGE_SIZE ), "rejection modified data" );
	longjmp( errorJump, 1 );
}
void Com_Memset( void *dest, int value, size_t length ) { memset( dest, value, length ); }

static void Reset( void ) {
	vm.interpretFaulted = qfalse;
	vm.currentlyInterpreting = qtrue;
	memset( vm.dataBase, 'x', IMAGE_SIZE );
}
static void Reject( int op, int dest, int source, int length ) {
	vm.interpretFaulted = qfalse;
	vm.currentlyInterpreting = qtrue;
	memcpy( before, vm.dataBase, IMAGE_SIZE );
	expectError = 1;
	if ( setjmp( errorJump ) == 0 ) {
		if ( op == 0 ) VM_MemoryFill( dest, source, length );
		else if ( op == 1 ) VM_MemoryCopy( dest, source, length );
		else VM_StringCopy( dest, source, length );
		Check( 0, "invalid buffer accepted" );
	}
	expectError = 0;
}

static void TestBuffers( void ) {
	int op;
	Reset();
	VM_MemoryFill( IMAGE_SIZE - 4, 0x5a, 4 );
	Check( !memcmp( vm.dataBase + IMAGE_SIZE - 4, "ZZZZ", 4 ), "exact-end fill" );
	VM_MemoryCopy( 4, IMAGE_SIZE - 4, 4 );
	Check( !memcmp( vm.dataBase + 4, "ZZZZ", 4 ), "exact-end source" );
	VM_MemoryCopy( IMAGE_SIZE - 4, 4, 4 );
	VM_MemoryFill( IMAGE_SIZE + 4, 0x61, 4 );
	Check( !memcmp( vm.dataBase + 4, "aaaa", 4 ), "masked fill" );
	VM_MemoryFill( -1, 0x62, 1 );
	Check( vm.dataBase[IMAGE_SIZE - 1] == 'b', "masked last byte" );
	memcpy( vm.dataBase + 4, "abcdef", 6 );
	VM_MemoryCopy( 6, 4, 6 );
	Check( !memcmp( vm.dataBase + 6, "abcdef", 6 ), "overlap copy" );
	VM_MemoryCopy( 4, 6, 6 );
	Check( !memcmp( vm.dataBase + 4, "abcdef", 6 ), "reverse overlap copy" );
	for ( op = 0; op < 3; op++ ) {
		Reset();
		Reject( op, 0, 4, 1 );
		Reject( op, IMAGE_SIZE - 1, 4, 2 );
		Reject( op, 4, 4, -1 );
		Reject( op, 4, 4, INT_MIN );
		Reject( op, 4, 4, INT_MAX );
		if ( op ) {
			Reject( op, 4, 0, 1 );
			Reject( op, 4, IMAGE_SIZE - 1, 2 );
		}
	}
	Reset();
	memcpy( before, vm.dataBase, IMAGE_SIZE );
	VM_MemoryFill( 0, 0, 0 ); VM_MemoryCopy( 0, 0, 0 );
	Check( VM_StringCopy( 0, 0, 0 ) == 0 &&
	       !memcmp( before, vm.dataBase, IMAGE_SIZE ), "zero-length operations" );
}

static void TestStrings( void ) {
	Reset();
	vm.dataBase[IMAGE_SIZE - 1] = 0;
	Check( VM_StringCopy( 4, IMAGE_SIZE - 1, 8 ) == 4, "VM string return address" );
	Check( !memcmp( vm.dataBase + 4, "\0\0\0\0\0\0\0\0", 8 ), "short source padding" );
	Reset();
	memcpy( vm.dataBase + IMAGE_SIZE - 4, "abc", 4 );
	Check( VM_StringCopy( IMAGE_SIZE + 4, IMAGE_SIZE - 4, 8 ) == IMAGE_SIZE + 4,
	       "masked destination return" );
	Check( !memcmp( vm.dataBase + 4, "abc\0\0\0\0\0", 8 ), "terminated source boundary" );
	Reset();
	memcpy( vm.dataBase + IMAGE_SIZE - 4, "abcd", 4 );
	VM_StringCopy( 4, IMAGE_SIZE - 4, 4 );
	Check( !memcmp( vm.dataBase + 4, "abcd", 4 ), "unterminated count boundary" );
	Reject( 2, 4, IMAGE_SIZE - 4, 5 );
	Reset();
	memcpy( vm.dataBase + 4, "abcdef", 7 );
	VM_StringCopy( 6, 4, 8 );
	Check( !memcmp( vm.dataBase + 6, "abcdef\0\0", 8 ), "overlapping string" );
}
/* Check typed pointer failures before native code consumes the VM data. */
static void RejectChecked( int op, int value, int length, int extra, qboolean nullable ) {
	vm.interpretFaulted = qfalse;
	vm.currentlyInterpreting = qtrue;
	memcpy( before, vm.dataBase, IMAGE_SIZE );
	expectError = 1;
	if ( setjmp( errorJump ) == 0 ) {
		if ( op == 0 ) VM_CheckedArgPtr( value, length, extra, nullable );
		else if ( op == 1 ) VM_CheckedArgString( value, nullable );
		else if ( op == 2 ) VM_CheckedArgArray( value, length, extra );
		else VM_CheckedStringBuffer( value, length, nullable );
		Check( 0, "invalid typed argument accepted" );
	}
	expectError = 0;
}

static void TestCheckedArguments( void ) {
	Reset();
	Check( VM_CheckedArgPtr( IMAGE_SIZE - 4, 4, 4, qfalse ) == vm.dataBase + IMAGE_SIZE - 4,
	       "aligned structure at image end" );
	Check( VM_CheckedArgPtr( IMAGE_SIZE + 4, 4, 4, qfalse ) == vm.dataBase + 4,
	       "typed pointer preserves legacy masking" );
	Check( VM_CheckedArgPtr( IMAGE_SIZE - 1, 1, 1, qfalse ) == vm.dataBase + IMAGE_SIZE - 1,
	       "byte buffer need not align" );
	Check( VM_CheckedArgPtr( 0, 4, 4, qtrue ) == NULL, "nullable structure query" );
	RejectChecked( 0, 0, 4, 4, qfalse );
	RejectChecked( 0, 5, 4, 4, qfalse );
	RejectChecked( 0, IMAGE_SIZE - 4, 5, 4, qfalse );
	RejectChecked( 0, 4, -1, 1, qfalse );
	RejectChecked( 0, 4, INT_MAX, 1, qfalse );

	Check( VM_CheckedArgArray( IMAGE_SIZE - 8, 2, 4 ) == vm.dataBase + IMAGE_SIZE - 8,
	       "exact-end vertex array" );
	Check( VM_CheckedArgArray( 0, 0, 4 ) == NULL, "empty array" );
	RejectChecked( 2, 4, -1, 4, qfalse );
	RejectChecked( 2, 4, INT_MAX, 4, qfalse );
	RejectChecked( 2, 4, INT_MIN, 4, qfalse );
	RejectChecked( 2, 4, 1, 0, qfalse );
	RejectChecked( 2, 5, 1, 4, qfalse );
	RejectChecked( 2, IMAGE_SIZE - 4, 2, 4, qfalse );

	Check( VM_CheckedArgString( 0, qtrue ) == NULL, "nullable string reset" );
	RejectChecked( 1, 0, 0, 0, qfalse );
	RejectChecked( 1, IMAGE_SIZE - 4, 0, 0, qfalse );
	vm.dataBase[IMAGE_SIZE - 1] = 0;
	Check( VM_CheckedArgString( IMAGE_SIZE - 1, qfalse ) == (char *)vm.dataBase + IMAGE_SIZE - 1,
	       "empty string in final byte" );
	Check( VM_CheckedArgString( IMAGE_SIZE + IMAGE_SIZE - 4, qfalse ) ==
	       (char *)vm.dataBase + IMAGE_SIZE - 4, "terminated masked string" );

	Check( VM_CheckedStringBuffer( IMAGE_SIZE - 1, 1, qfalse ) == vm.dataBase + IMAGE_SIZE - 1,
	       "terminator-sized output" );
	Check( VM_CheckedStringBuffer( 0, 0, qtrue ) == NULL, "nullable string query" );
	RejectChecked( 3, 0, 1, 0, qfalse );
	RejectChecked( 3, 4, 0, 0, qfalse );
	RejectChecked( 3, 0, -1, 0, qtrue );
	RejectChecked( 3, IMAGE_SIZE - 1, 2, 0, qfalse );
}

int main( void ) {
	vm.dataBase = malloc( IMAGE_SIZE );
	Check( vm.dataBase != NULL, "allocation" );
	vm.dataMask = IMAGE_SIZE - 1;
	currentVM = &vm;
	TestBuffers();
	TestStrings();
	TestCheckedArguments();
	free( vm.dataBase );
	puts( "VM syscall memory trap regressions passed (issue #35)" );
	return 0;
}
