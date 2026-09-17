/* Issue #35: count, pad, and dispatch VM calls without relying on varargs ABI. */
#include "../code/qcommon/vm_local.h"
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static vm_t vm, previousVM;
static int expected[MAX_VMMAIN_ARGS], dispatches, vmEvaluations;
static int expectError;
static jmp_buf errorJump;
extern vm_t *lastVM;

static void Check( int ok, const char *message ) {
	if ( !ok ) {
		fprintf( stderr, "VM call regression failed: %s\n", message );
		exit( 1 );
	}
}
void QDECL Com_Error( int level, const char *format, ... ) {
	(void)format;
	Check( expectError && level == ERR_FATAL, "unexpected engine error" );
	longjmp( errorJump, 1 );
}
void QDECL Com_Printf( const char *format, ... ) { (void)format; }
void Com_Memcpy( void *dest, const void *src, size_t length ) {
	memcpy( dest, src, length );
}

static int Dispatch( vm_t *target, int *args ) {
	Check( target == &vm && currentVM == target && lastVM == target,
	       "dispatch VM state" );
	Check( !memcmp( args, expected, sizeof(expected) ), "argument values/padding" );
	dispatches++;
	return 42;
}
int VM_CallInterpreted( vm_t *target, int *args ) { return Dispatch( target, args ); }
int VM_CallCompiled( vm_t *target, int *args ) { return Dispatch( target, args ); }
static int QDECL NativeEntry( int command, int a1, int a2, int a3, int a4,
                             int a5, int a6, int a7, int a8, int a9,
                             int a10, int a11, int a12 ) {
	int args[MAX_VMMAIN_ARGS] = {command, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12};
	return Dispatch( &vm, args );
}
static vm_t *GetVM( void ) { vmEvaluations++; return &vm; }

/** Verify counted dispatch and quiet shutdown re-entry for faulted QVMs. */
static void TestDispatches( void ) {
	int mode, value;
	for ( mode = 0; mode < 3; mode++ ) {
		memset( &vm, 0, sizeof(vm) );
		vm.compiled = mode == 1;
		if ( mode == 2 ) {
			vm.entryPoint = (int (QDECL *)(int, ...))NativeEntry;
		}
		currentVM = &previousVM;
		memset( expected, 0, sizeof(expected) ); expected[0] = 7;
		Check( VM_Call( &vm, 7 ) == 42, "empty call result" );
		Check( currentVM == &previousVM, "previous VM restoration" );
		value = 123;
		expected[1] = value;
		Check( VM_Call( GetVM(), 7, value++ ) == 42, "one-argument result" );
		Check( value == 124 && vmEvaluations == mode + 1, "argument evaluated twice" );
		for ( value = 1; value < MAX_VMMAIN_ARGS; value++ ) {
			expected[value] = -value;
		}
		Check( VM_Call( &vm, 7, -1, -2, -3, -4, -5, -6, -7, -8, -9, -10, -11, -12 ) == 42,
		       "full-argument result" );
	}
	Check( dispatches == 9, "dispatch count" );
	vm.entryPoint = NULL;
	vm.interpretFaulted = qtrue;
	for ( mode = 0; mode < 2; mode++ ) {
		vm.compiled = mode;
		Check( VM_Call( &vm, 7 ) == 0 && dispatches == 9 &&
		       currentVM == &previousVM, "faulted VM re-entry" );
	}
	vm.interpretFaulted = qfalse;
}

static void TestSharedFrame( void ) {
	int args[MAX_VMMAIN_ARGS], i;
	byte image[256], before[256];
	int stack;
	memset( image, 0x5a, sizeof(image) );
	memcpy( before, image, sizeof(image) );
	vm.dataBase = image;
	vm.dataMask = sizeof(image) - 1;
	vm.programStack = sizeof(image);
	vm.stackBottom = 0;
	for ( i = 0; i < MAX_VMMAIN_ARGS; i++ ) args[i] = 100 + i;
	stack = VM_SetupCallFrame( &vm, args );
	Check( stack == (int)sizeof(image) - VM_ENTRY_FRAME_SIZE, "shared frame size" );
	Check( !memcmp( image, before, stack ), "entry overwrote preceding data" );
	Check( *(int *)(image + stack) == -1 && *(int *)(image + stack + 4) == 0,
	       "entry return slots" );
	for ( i = 0; i < MAX_VMMAIN_ARGS; i++ ) {
		Check( *(int *)(image + stack + 8 + i * 4) == args[i], "shared frame arguments" );
	}
	vm.dataBase = NULL;
}

static void RejectCall( vm_t *target, const int *args, int count ) {
	vm_t *before = currentVM;
	int callsBefore = dispatches;
	expectError = 1;
	if ( setjmp( errorJump ) == 0 ) {
		VM_CallArgs( target, 0, args, count );
		Check( 0, "invalid call accepted" );
	}
	expectError = 0;
	Check( dispatches == callsBefore && currentVM == before,
	       "invalid call modified dispatcher state" );
}
int main( void ) {
	TestDispatches();
	TestSharedFrame();
	RejectCall( NULL, expected, 1 );
	RejectCall( &vm, NULL, 1 );
	RejectCall( &vm, expected, -1 );
	RejectCall( &vm, expected, MAX_VMMAIN_ARGS );
	puts( "VM call argument regressions passed (issue #35)" );
	return 0;
}
