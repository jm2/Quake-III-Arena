/* Issue #35: validate bytecode preparation without executing untrusted code. */
#include "../code/qcommon/vm_local.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void *codeAllocation;
static int allocationCount;

static void Check( int condition, const char *message ) {
	if ( !condition ) {
		fprintf( stderr, "QVM bytecode regression failed: %s\n", message );
		exit( 1 );
	}
}

void *Hunk_Alloc( int size, ha_pref preference ) {
	(void)preference;
	Check( size > 0 && size <= 4096 && !codeAllocation, "code allocation" );
	codeAllocation = calloc( 1, size );
	Check( codeAllocation != NULL, "allocation failed" );
	allocationCount++;
	return codeAllocation;
}

static void PutWord( byte *code, int value ) {
	unsigned int word = (unsigned int)value;
	int i;
	for ( i = 0; i < 4; i++ ) {
		code[i] = (byte)(word >> (8 * i));
	}
}

static void Prepare( const byte *code, int length, int count,
                     int offsetPadding, qboolean valid ) {
	vm_t vm;
	vmHeader_t *header;
	int *pointers;
	int i, offset = sizeof(*header) + offsetPadding;

	memset( &vm, 0, sizeof(vm) );
	header = calloc( 1, offset + length );
	pointers = malloc( count * sizeof(*pointers) );
	Check( header && pointers, "fixture allocation" );
	for ( i = 0; i < count; i++ ) {
		pointers[i] = -12345;
	}
	header->codeOffset = offset;
	header->codeLength = length;
	header->instructionCount = count;
	memcpy( (byte *)header + offset, code, length );
	vm.codeLength = length;
	vm.instructionPointers = pointers;
	vm.instructionPointersLength = count * sizeof(*pointers);
	allocationCount = 0;

	Check( VM_PrepareInterpreter( &vm, header ) == valid, "preparation result" );
	if ( valid ) {
		Check( allocationCount == 1 && vm.codeBase, "valid code not prepared" );
	} else {
		Check( allocationCount == 0 && !vm.codeBase, "invalid code allocated" );
		for ( i = 0; i < count; i++ ) {
			Check( pointers[i] == -12345, "invalid code partially wrote table" );
		}
	}
	free( codeAllocation );
	codeAllocation = NULL;
	free( pointers );
	free( header );
}

static void TestTruncatedOperands( void ) {
	const int wordOps[] = {
		OP_ENTER, OP_LEAVE, OP_CONST, OP_LOCAL, OP_BLOCK_COPY,
		OP_EQ, OP_NE, OP_LTI, OP_LEI, OP_GTI, OP_GEI,
		OP_LTU, OP_LEU, OP_GTU, OP_GEU,
		OP_EQF, OP_NEF, OP_LTF, OP_LEF, OP_GTF, OP_GEF
	};
	byte code[5] = {0};
	size_t i;
	int length, padding;

	for ( padding = 0; padding < 4; padding++ ) {
		for ( i = 0; i < sizeof(wordOps)/sizeof(wordOps[0]); i++ ) {
			code[0] = wordOps[i];
			for ( length = 0; length < 5; length++ ) {
				Prepare( code, length, 1, padding, qfalse );
			}
			Prepare( code, 5, 1, padding, qtrue );
		}
		code[0] = OP_ARG;
		Prepare( code, 1, 1, padding, qfalse );
		Prepare( code, 2, 1, padding, qtrue );
	}
}

static void TestOpcodesAndCounts( void ) {
	byte code[5] = {0};
	int op;
	for ( op = 0; op <= 255; op++ ) {
		code[0] = op;
		Prepare( code, sizeof(code), 1, 0,
		         op >= OP_IGNORE && op <= OP_CVFI ? qtrue : qfalse );
	}
	code[0] = OP_IGNORE;
	code[1] = OP_BREAK;
	Prepare( code, 2, 2, 0, qtrue );
	Prepare( code, 2, 3, 0, qfalse );
	code[0] = OP_CONST;
	PutWord( code + 1, 0 );
	Prepare( code, 5, 2, 0, qfalse );
}

static void TestBranches( void ) {
	const int invalidTargets[] = { -1, INT_MIN, 1, INT_MAX };
	byte code[5];
	int op;
	size_t i;
	for ( op = OP_EQ; op <= OP_GEF; op++ ) {
		code[0] = op;
		for ( i = 0; i < sizeof(invalidTargets)/sizeof(invalidTargets[0]); i++ ) {
			PutWord( code + 1, invalidTargets[i] );
			Prepare( code, 5, 1, 0, qfalse );
		}
		PutWord( code + 1, 0 );
		Prepare( code, 5, 1, 0, qtrue );
	}
}

static void TestValidTranslation( void ) {
	vm_t vm;
	vmHeader_t *header;
	byte *code;
	int pointers[6], i;
	const int offsets[] = {0, 5, 10, 12, 13, 18};
	int *decoded;

	memset( &vm, 0, sizeof(vm) );
	header = calloc( 1, sizeof(*header) + 1 + 24 );
	Check( header != NULL, "translation fixture" );
	header->codeOffset = sizeof(*header) + 1;
	header->codeLength = vm.codeLength = 24;
	header->instructionCount = 6;
	code = (byte *)header + header->codeOffset;
	code[0] = OP_CONST; PutWord( code + 1, 0x12345678 );
	code[5] = OP_EQ; PutWord( code + 6, 5 );
	code[10] = OP_ARG; code[11] = 252;
	code[12] = OP_IGNORE;
	code[13] = OP_CONST; PutWord( code + 14, -1 );
	code[18] = OP_LEAVE; PutWord( code + 19, 0 );
	// Byte 23 is q3asm's alignment padding, not a declared instruction.
	vm.instructionPointers = pointers;
	vm.instructionPointersLength = sizeof(pointers);
	Check( VM_PrepareInterpreter( &vm, header ), "valid translation" );
	decoded = (int *)vm.codeBase;
	for ( i = 0; i < 6; i++ ) {
		Check( pointers[i] == offsets[i], "instruction offsets" );
	}
	Check( decoded[1] == 0x12345678 && decoded[14] == -1,
	       "unaligned little-endian signed immediates" );
	Check( decoded[6] == 18, "forward branch translation" );
	Check( decoded[11] == 252, "unsigned one-byte argument" );
	free( codeAllocation );
	codeAllocation = NULL;
	free( header );
}

int main( void ) {
	TestTruncatedOperands();
	TestOpcodesAndCounts();
	TestBranches();
	TestValidTranslation();
	puts( "QVM bytecode preparation regressions passed (issue #35)" );
	return 0;
}
