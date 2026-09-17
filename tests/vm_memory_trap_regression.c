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
int main( void ) {
	vm.dataBase = malloc( IMAGE_SIZE );
	Check( vm.dataBase != NULL, "allocation" );
	vm.dataMask = IMAGE_SIZE - 1;
	currentVM = &vm;
	TestBuffers();
	TestStrings();
	free( vm.dataBase );
	puts( "VM syscall memory trap regressions passed (issue #35)" );
	return 0;
}
