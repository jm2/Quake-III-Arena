/* Issue #35: execute synthetic QVMs through the actual interpreter. */
#include "../code/qcommon/vm_local.h"
#include <limits.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IMAGE_SIZE 1024
static byte code[4096];
static int codeLength, instructionCount;
static vm_t vm;
static void *codeAllocation;
static jmp_buf errorJump;
static int expectError, errorCount, syscalls, recurseSyscall, corruptReturn;
static char lastError[128];

static void Check( int ok, const char *message ) {
	if ( !ok ) {
		fprintf( stderr, "QVM runtime regression failed: %s (%s)\n", message, lastError );
		exit( 1 );
	}
}

void QDECL Com_Error( int level, const char *format, ... ) {
	va_list ap;
	va_start( ap, format );
	vsnprintf( lastError, sizeof(lastError), format, ap );
	va_end( ap );
	Check( expectError && level == ERR_DROP, "unexpected engine error" );
	Check( vm.interpretFaulted && !vm.currentlyInterpreting, "fault state" );
	// Com_Error shuts down modules before longjmp. Re-entry must not execute.
	Check( VM_CallInterpreted( &vm, NULL ) == 0, "faulted shutdown re-entry" );
	errorCount++;
	longjmp( errorJump, 1 );
}

void VM_Debug( int level ) { (void)level; }
void *Hunk_Alloc( int size, ha_pref preference ) {
	(void)preference;
	Check( size > 0 && !codeAllocation, "code allocation" );
	codeAllocation = calloc( 1, size );
	Check( codeAllocation != NULL, "allocation failed" );
	return codeAllocation;
}

static int SystemCall( int *args ) {
	int nestedArgs[10] = {0};
	Check( args[0] == 0, "syscall number" );
	syscalls++;
	if ( corruptReturn ) {
		args[-1] = corruptReturn;
	}
	if ( recurseSyscall && syscalls == 1 ) {
		Check( VM_CallInterpreted( &vm, nestedArgs ) == 123, "recursive result" );
		Check( vm.currentlyInterpreting, "recursive interpretation state" );
	}
	return 123;
}

static void ResetCode( void ) {
	codeLength = instructionCount = syscalls = recurseSyscall = corruptReturn = 0;
	lastError[0] = '\0';
}

static void Emit( int op, int operand ) {
	int i;
	Check( codeLength + 5 <= (int)sizeof(code), "fixture code capacity" );
	code[codeLength++] = op;
	instructionCount++;
	if ( op == OP_ARG ) {
		code[codeLength++] = operand;
	} else if ( op == OP_ENTER || op == OP_LEAVE || op == OP_CONST ||
	            op == OP_LOCAL || op == OP_BLOCK_COPY ||
	            (op >= OP_EQ && op <= OP_GEF) ) {
		for ( i = 0; i < 4; i++ ) {
			code[codeLength++] = (byte)((unsigned int)operand >> (8 * i));
		}
	}
}

static void Execute( int entryStack, int floor, qboolean rejected, int result,
                     const char *errorText ) {
	vmHeader_t *header;
	int args[10] = {0};
	memset( &vm, 0, sizeof(vm) );
	header = calloc( 1, sizeof(*header) + codeLength );
	vm.dataBase = calloc( 1, IMAGE_SIZE );
	vm.instructionPointers = malloc( instructionCount * sizeof(int) );
	Check( header && vm.dataBase && vm.instructionPointers, "fixture allocation" );
	header->codeOffset = sizeof(*header);
	header->codeLength = vm.codeLength = codeLength;
	header->instructionCount = instructionCount;
	memcpy( (byte *)header + sizeof(*header), code, codeLength );
	vm.instructionPointersLength = instructionCount * sizeof(int);
	vm.dataMask = IMAGE_SIZE - 1;
	vm.stackBottom = floor;
	vm.programStack = entryStack;
	vm.systemCall = SystemCall;
	Check( VM_PrepareInterpreter( &vm, header ), "fixture preparation" );
	free( header );
	errorCount = 0;
	expectError = rejected;
	if ( setjmp( errorJump ) == 0 ) {
		int actual = VM_CallInterpreted( &vm, args );
		Check( !rejected && actual == result, "execution result" );
		Check( !vm.interpretFaulted && !vm.currentlyInterpreting &&
		       vm.programStack == entryStack, "normal return state" );
	}
	Check( errorCount == (int)rejected, "expected controlled rejection" );
	if ( rejected ) {
		Check( strstr( lastError, errorText ) != NULL, "wrong rejection reason" );
	}
	free( codeAllocation );
	codeAllocation = NULL;
	free( vm.dataBase );
	free( vm.instructionPointers );
}

static void Run( qboolean rejected, int result, const char *reason ) {
	Execute( IMAGE_SIZE, 0, rejected, result, reason );
}

static void TestValidExecution( void ) {
	ResetCode();
	Emit( OP_ENTER, 8 ); Emit( OP_CONST, 4 ); Emit( OP_CALL, 0 ); Emit( OP_LEAVE, 8 );
	Emit( OP_ENTER, 8 ); Emit( OP_CONST, 42 ); Emit( OP_LEAVE, 8 );
	Run( qfalse, 42, NULL );

	ResetCode();
	Emit( OP_ENTER, 0 ); Emit( OP_CONST, 4 ); Emit( OP_JUMP, 0 );
	Emit( OP_CONST, -99 ); Emit( OP_CONST, 7 ); Emit( OP_LEAVE, 0 );
	Run( qfalse, 7, NULL );

	ResetCode();
	Emit( OP_ENTER, 0 ); Emit( OP_CONST, 15 ); Emit( OP_BCOM, 0 ); Emit( OP_LEAVE, 0 );
	Run( qfalse, ~15, NULL );

	ResetCode();
	Emit( OP_ENTER, 0 ); Emit( OP_PUSH, 0 ); Emit( OP_LEAVE, 0 );
	Run( qfalse, 0, NULL );

	ResetCode();
	Emit( OP_ENTER, 64 ); Emit( OP_CONST, -1 ); Emit( OP_CALL, 0 ); Emit( OP_LEAVE, 64 );
	recurseSyscall = 1;
	Run( qfalse, 123, NULL );
	Check( syscalls == 2, "recursive syscall dispatch" );
}

static void TestOperandStacks( void ) {
	const int unary[] = {OP_LEAVE, OP_CALL, OP_POP, OP_JUMP, OP_LOAD1, OP_LOAD2,
		OP_LOAD4, OP_ARG, OP_SEX8, OP_SEX16, OP_NEGI, OP_BCOM, OP_NEGF, OP_CVIF, OP_CVFI};
	const int binary[] = {OP_STORE1, OP_STORE2, OP_STORE4, OP_BLOCK_COPY, OP_ADD,
		OP_SUB, OP_DIVI, OP_DIVU, OP_MODI, OP_MODU, OP_MULI, OP_MULU, OP_BAND,
		OP_BOR, OP_BXOR, OP_LSH, OP_RSHI, OP_RSHU, OP_ADDF, OP_SUBF, OP_DIVF, OP_MULF};
	const int pushes[] = {OP_CONST, OP_LOCAL, OP_PUSH};
	size_t i;
	int op, depth, n;
	for ( i = 0; i < sizeof(unary)/sizeof(unary[0]); i++ ) {
		ResetCode(); Emit( unary[i], 0 ); Run( qtrue, 0, "underflow" );
	}
	for ( depth = 0; depth < 2; depth++ ) {
		for ( i = 0; i < sizeof(binary)/sizeof(binary[0]); i++ ) {
			ResetCode();
			if ( depth ) Emit( OP_CONST, 1 );
			Emit( binary[i], 0 ); Run( qtrue, 0, "underflow" );
		}
		for ( op = OP_EQ; op <= OP_GEF; op++ ) {
			ResetCode();
			if ( depth ) Emit( OP_CONST, 1 );
			Emit( op, 0 ); Run( qtrue, 0, "underflow" );
		}
	}
	for ( i = 0; i < sizeof(pushes)/sizeof(pushes[0]); i++ ) {
		ResetCode();
		for ( n = 0; n < 255; n++ ) Emit( OP_CONST, 42 );
		Emit( pushes[i], 0 ); Run( qtrue, 0, "overflow" );
	}
	ResetCode();
	for ( n = 0; n < 255; n++ ) Emit( OP_CONST, 42 );
	for ( n = 0; n < 254; n++ ) Emit( OP_POP, 0 );
	Emit( OP_LEAVE, 0 ); Run( qfalse, 42, NULL );

	ResetCode(); Emit( OP_CONST, 1 ); Emit( OP_CONST, 2 ); Emit( OP_LEAVE, 0 );
	Run( qtrue, 0, "return operand stack mismatch" );
}

static void TestFrames( void ) {
	const int badEntries[] = {INT_MIN, -4, 0, 44, 49, IMAGE_SIZE + 4, INT_MAX};
	const int badFrames[] = {INT_MIN, -4, 1, IMAGE_SIZE, INT_MAX};
	size_t i;
	ResetCode(); Emit( OP_CONST, 42 ); Emit( OP_LEAVE, 0 );
	for ( i = 0; i < sizeof(badEntries)/sizeof(badEntries[0]); i++ ) {
		Execute( badEntries[i], 0, qtrue, 0, "entry stack" );
	}
	Execute( IMAGE_SIZE, IMAGE_SIZE - 44, qtrue, 0, "entry stack" );
	Execute( 48, -65536, qfalse, 42, NULL );
	for ( i = 0; i < sizeof(badFrames)/sizeof(badFrames[0]); i++ ) {
		ResetCode(); Emit( OP_ENTER, badFrames[i] ); Run( qtrue, 0, "ENTER frame" );
		ResetCode(); Emit( OP_CONST, 42 ); Emit( OP_LEAVE, badFrames[i] );
		Run( qtrue, 0, "LEAVE frame" );
	}
	ResetCode(); Emit( OP_ENTER, 16 );
	Execute( IMAGE_SIZE, IMAGE_SIZE - 52, qtrue, 0, "ENTER frame" );
}

static void TestTargets( void ) {
	const int badCalls[] = {3, INT_MAX};
	const int badJumps[] = {INT_MIN, -1, 3, INT_MAX};
	const int badReturns[] = {INT_MIN, -2, 1, 4096, INT_MAX};
	size_t i;
	for ( i = 0; i < sizeof(badCalls)/sizeof(badCalls[0]); i++ ) {
		ResetCode(); Emit( OP_ENTER, 8 ); Emit( OP_CONST, badCalls[i] ); Emit( OP_CALL, 0 );
		Run( qtrue, 0, "CALL target" );
	}
	for ( i = 0; i < sizeof(badJumps)/sizeof(badJumps[0]); i++ ) {
		ResetCode(); Emit( OP_ENTER, 0 ); Emit( OP_CONST, badJumps[i] ); Emit( OP_JUMP, 0 );
		Run( qtrue, 0, "JUMP target" );
	}
	for ( i = 0; i < sizeof(badReturns)/sizeof(badReturns[0]); i++ ) {
		ResetCode();
		Emit( OP_ENTER, 8 ); Emit( OP_LOCAL, 8 ); Emit( OP_CONST, badReturns[i] );
		Emit( OP_STORE4, 0 ); Emit( OP_CONST, 42 ); Emit( OP_LEAVE, 8 );
		Run( qtrue, 0, "return address" );
	}
	ResetCode();
	Emit( OP_ENTER, 8 ); Emit( OP_LOCAL, 0 ); Emit( OP_CONST, -1 );
	Emit( OP_STORE4, 0 ); Emit( OP_CONST, 42 ); Emit( OP_LEAVE, 0 );
	Run( qtrue, 0, "return stack mismatch" );

	ResetCode(); Emit( OP_IGNORE, 0 ); Run( qtrue, 0, "pc out of range" );
	ResetCode(); Emit( OP_IGNORE, 0 ); code[codeLength++] = OP_UNDEF;
	Run( qtrue, 0, "Bad VM instruction" );

	ResetCode();
	Emit( OP_ENTER, 64 ); Emit( OP_CONST, -1 ); Emit( OP_CALL, 0 ); Emit( OP_LEAVE, 64 );
	corruptReturn = 1; Run( qtrue, 0, "syscall return address" );
}

int main( void ) {
	TestValidExecution();
	TestOperandStacks();
	TestFrames();
	TestTargets();
	puts( "QVM runtime stack/control-flow regressions passed (issue #35)" );
	return 0;
}
