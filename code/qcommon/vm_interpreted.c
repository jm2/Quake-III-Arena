/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
#include "vm_local.h"
#include <limits.h>

#ifdef DEBUG_VM // bk001204
static char	*opnames[256] = {
	"OP_UNDEF", 

	"OP_IGNORE", 

	"OP_BREAK",

	"OP_ENTER",
	"OP_LEAVE",
	"OP_CALL",
	"OP_PUSH",
	"OP_POP",

	"OP_CONST",

	"OP_LOCAL",

	"OP_JUMP",

	//-------------------

	"OP_EQ",
	"OP_NE",

	"OP_LTI",
	"OP_LEI",
	"OP_GTI",
	"OP_GEI",

	"OP_LTU",
	"OP_LEU",
	"OP_GTU",
	"OP_GEU",

	"OP_EQF",
	"OP_NEF",

	"OP_LTF",
	"OP_LEF",
	"OP_GTF",
	"OP_GEF",

	//-------------------

	"OP_LOAD1",
	"OP_LOAD2",
	"OP_LOAD4",
	"OP_STORE1",
	"OP_STORE2",
	"OP_STORE4",
	"OP_ARG",

	"OP_BLOCK_COPY",

	//-------------------

	"OP_SEX8",
	"OP_SEX16",

	"OP_NEGI",
	"OP_ADD",
	"OP_SUB",
	"OP_DIVI",
	"OP_DIVU",
	"OP_MODI",
	"OP_MODU",
	"OP_MULI",
	"OP_MULU",

	"OP_BAND",
	"OP_BOR",
	"OP_BXOR",
	"OP_BCOM",

	"OP_LSH",
	"OP_RSHI",
	"OP_RSHU",

	"OP_NEGF",
	"OP_ADDF",
	"OP_SUBF",
	"OP_DIVF",
	"OP_MULF",

	"OP_CVIF",
	"OP_CVFI"
};
#endif

/* QVM immediates are little-endian and need not be word aligned. */
static int VM_ReadCodeWord( const byte *code ) {
	return (int)((unsigned int)code[0] | ((unsigned int)code[1] << 8) |
	             ((unsigned int)code[2] << 16) | ((unsigned int)code[3] << 24));
}

/* Keep the sparse, byte-offset code representation used by the interpreter. */
static int VM_OperandSize( int op ) {
	if ( op == OP_ARG ) {
		return 1;
	}
	if ( op == OP_ENTER || op == OP_LEAVE || op == OP_CONST ||
	     op == OP_LOCAL || op == OP_BLOCK_COPY ||
	     (op >= OP_EQ && op <= OP_GEF) ) {
		return 4;
	}
	return 0;
}

char *VM_Indent( vm_t *vm ) {
	static char	*string = "                                        ";
	if ( vm->callLevel > 20 ) {
		return string;
	}
	return string + 2 * ( 20 - vm->callLevel );
}

void VM_StackTrace( vm_t *vm, int programCounter, int programStack ) {
	int		count;

	count = 0;
	do {
		Com_Printf( "%s\n", VM_ValueToSymbol( vm, programCounter ) );
		programStack =  *(int *)&vm->dataBase[programStack+4];
		programCounter = *(int *)&vm->dataBase[programStack];
	} while ( programCounter != -1 && ++count < 32 );

}


/*
====================
VM_PrepareInterpreter
====================
*/
qboolean VM_PrepareInterpreter( vm_t *vm, vmHeader_t *header ) {
	int op, pc, instruction, operandSize, value;
	byte *code = (byte *)header + header->codeOffset;
	int *codeBase;

	// Validate the entire instruction stream before writing either table.
	// VM_Create has already validated the header and allocation arithmetic.
	pc = 0;
	for ( instruction = 0; instruction < header->instructionCount; instruction++ ) {
		if ( pc >= header->codeLength ) {
			return qfalse;
		}
		op = code[pc++];
		if ( op < OP_IGNORE || op > OP_CVFI ) {
			return qfalse;
		}
		operandSize = VM_OperandSize( op );
		if ( operandSize > header->codeLength - pc ) {
			return qfalse;
		}
		if ( op >= OP_EQ && op <= OP_GEF ) {
			value = VM_ReadCodeWord( code + pc );
			if ( value < 0 || value >= header->instructionCount ) {
				return qfalse;
			}
		}
		pc += operandSize;
	}

	// q3asm may pad the code section after the declared instructions.
	// Preserve that layout while ensuring every decoded operand is in range.
	vm->codeBase = Hunk_Alloc( vm->codeLength * sizeof(int), h_high );
	codeBase = (int *)vm->codeBase;
	pc = 0;
	for ( instruction = 0; instruction < header->instructionCount; instruction++ ) {
		vm->instructionPointers[instruction] = pc;
		op = code[pc];
		codeBase[pc++] = op;
		operandSize = VM_OperandSize( op );
		if ( operandSize == 4 ) {
			codeBase[pc] = VM_ReadCodeWord( code + pc );
		} else if ( operandSize == 1 ) {
			codeBase[pc] = code[pc];
		}
		pc += operandSize;
	}

	// Translate branches only after every instruction's offset is available.
	for ( instruction = 0; instruction < header->instructionCount; instruction++ ) {
		pc = vm->instructionPointers[instruction];
		op = codeBase[pc++];
		if ( op >= OP_EQ && op <= OP_GEF ) {
			codeBase[pc] = vm->instructionPointers[codeBase[pc]];
		}
	}
	return qtrue;
}

/* Address masking is part of the legacy VM ABI, but a complete access must
 * fit after masking. Subtraction avoids wrapping the end of the range. */
static qboolean VM_DataRange( vm_t *vm, unsigned int offset, unsigned int length ) {
	unsigned int size = (unsigned int)vm->dataMask + 1;
	return offset <= size && length <= size - offset;
}

/* Return addresses live in writable VM data. Require an instruction boundary,
 * not merely an offset into the expanded opcode/operand buffer. */
static qboolean VM_ValidReturnAddress( vm_t *vm, int pc ) {
	int low = 0, high = vm->instructionPointersLength / sizeof(int);
	while ( low < high ) {
		int middle = low + (high - low) / 2;
		int offset = vm->instructionPointers[middle];
		if ( pc == offset ) {
			return qtrue;
		}
		if ( pc < offset ) {
			high = middle;
		} else {
			low = middle + 1;
		}
	}
	return qfalse;
}

/* Operands required before executing each opcode. Positive stack growth is
 * limited separately for CONST, LOCAL, and PUSH; CALL replaces its target. */
static int VM_RequiredOperands( int op ) {
	switch ( op ) {
	case OP_IGNORE: case OP_BREAK: case OP_ENTER:
	case OP_CONST: case OP_LOCAL: case OP_PUSH:
		return 0;
	case OP_LEAVE: case OP_CALL: case OP_POP: case OP_JUMP:
	case OP_LOAD1: case OP_LOAD2: case OP_LOAD4: case OP_ARG:
	case OP_SEX8: case OP_SEX16: case OP_NEGI: case OP_BCOM:
	case OP_NEGF: case OP_CVIF: case OP_CVFI:
		return 1;
	default:
		return 2;
	}
}

/*
==============
VM_Call


Upon a system call, the stack will look like:

sp+32	parm1
sp+28	parm0
sp+24	return stack
sp+20	return address
sp+16	local1
sp+14	local0
sp+12	arg1
sp+8	arg0
sp+4	return stack
sp		return address

An interpreted function will immediately execute
an OP_ENTER instruction, which will subtract space for
locals from sp
==============
*/
#define	MAX_STACK	256
#define	STACK_MASK	(MAX_STACK-1)
//#define	DEBUG_VM

#define	DEBUGSTR va("%s%i", VM_Indent(vm), opStack-stack )

int	VM_CallInterpreted( vm_t *vm, int *args ) {
	int		stack[MAX_STACK + 1];
	int		*opStack;
	int		programCounter;
	int		programStack;
	int		stackOnEntry;
	byte	*image;
	int		*codeImage;
	int		v1;
	int		dataMask;
	int		stackFloor;
	const int entryFrame = VM_ENTRY_FRAME_SIZE;
	qboolean wasInterpreting;
#ifdef DEBUG_VM
	vmSymbol_t	*profileSymbol;
#endif

	// ERR_DROP invokes module shutdown before longjmp. Never execute a VM
	// that has already faulted, including when a syscall re-enters it.
	if ( vm->interpretFaulted ) {
		return 0;
	}
	wasInterpreting = vm->currentlyInterpreting;
	vm->currentlyInterpreting = qtrue;
#define VM_INTERPRETER_ERROR(message) do { \
	vm->interpretFaulted = qtrue; \
	vm->currentlyInterpreting = qfalse; \
	Com_Error( ERR_DROP, "%s", message ); \
	return 0; \
} while (0)

	// we might be called recursively, so this might not be the very top
	programStack = stackOnEntry = vm->programStack;

#ifdef DEBUG_VM
	profileSymbol = VM_ValueToFunctionSymbol( vm, 0 );
	// uncomment this for debugging breakpoints
	vm->breakFunction = 0;
#endif
	// set up the stack frame 

	image = vm->dataBase;
	codeImage = (int *)vm->codeBase;
	dataMask = vm->dataMask;
	
	// Reserve two initialized slots below the first operand so cached
	// reads of opStack[0] and opStack[-1] are safe even at depth zero.
	stack[0] = stack[1] = 0;
	opStack = stack + 1;
	programCounter = 0;

	stackFloor = vm->stackBottom > 0 ? vm->stackBottom : 0;
	programStack = VM_SetupCallFrame( vm, args );

	vm->callLevel = 0;
	
	VM_Debug(0);

//	vm_debugLevel=2;
	// main interpreter loop, will exit when a LEAVE instruction
	// grabs the -1 program counter

#define r2 codeImage[programCounter]

	while ( 1 ) {
		int		opcode,	r0, r1;
//		unsigned int	r2;

nextInstruction:
		r0 = ((int *)opStack)[0];
		r1 = ((int *)opStack)[-1];
nextInstruction2:
		if ( (unsigned int)programCounter >= (unsigned int)vm->codeLength ) {
			VM_INTERPRETER_ERROR( "VM pc out of range" );
		}
		opcode = codeImage[programCounter++];
		if ( opcode < OP_IGNORE || opcode > OP_CVFI ) {
			VM_INTERPRETER_ERROR( "Bad VM instruction" );
		}
		if ( opStack - (stack + 1) < VM_RequiredOperands( opcode ) ) {
			VM_INTERPRETER_ERROR( "VM operand stack underflow" );
		}
		if ( (opcode == OP_CONST || opcode == OP_LOCAL || opcode == OP_PUSH) &&
		     opStack == stack + MAX_STACK ) {
			VM_INTERPRETER_ERROR( "VM operand stack overflow" );
		}
#ifdef DEBUG_VM
		if ( vm_debugLevel > 1 ) {
			Com_Printf( "%s %s\n", DEBUGSTR, opnames[opcode] );
		}
		profileSymbol->profileCount++;
#endif

		switch ( opcode ) {
		case OP_IGNORE:
			goto nextInstruction2;
		case OP_BREAK:
			vm->breakCount++;
			goto nextInstruction2;
		case OP_CONST:
			opStack++;
			r1 = r0;
			r0 = *opStack = r2;
			
			programCounter += 4;
			goto nextInstruction2;
		case OP_LOCAL:
			opStack++;
			r1 = r0;
			r0 = *opStack = (int)((unsigned int)r2 + (unsigned int)programStack);

			programCounter += 4;
			goto nextInstruction2;

		case OP_LOAD4:
			v1 = r0 & dataMask;
			if ( !VM_DataRange( vm, v1, 4 ) ) {
				VM_INTERPRETER_ERROR( "VM LOAD4 out of range" );
			}
			// Native byte order matches the initialized data image. memcpy
			// also permits unaligned accesses without a PowerPC alignment trap.
			memcpy( opStack, image + v1, 4 );
			r0 = *opStack;
			goto nextInstruction2;
		case OP_LOAD2:
			{
				unsigned short value;
				v1 = r0 & dataMask;
				if ( !VM_DataRange( vm, v1, 2 ) ) {
					VM_INTERPRETER_ERROR( "VM LOAD2 out of range" );
				}
				memcpy( &value, image + v1, 2 );
				r0 = *opStack = value;
			}
			goto nextInstruction2;
		case OP_LOAD1:
			r0 = *opStack = image[r0 & dataMask];
			goto nextInstruction2;

		case OP_STORE4:
			// Preserve legacy store-address alignment as well as masking.
			v1 = r1 & (dataMask & ~3);
			if ( !VM_DataRange( vm, v1, 4 ) ) {
				VM_INTERPRETER_ERROR( "VM STORE4 out of range" );
			}
			*(int *)&image[v1] = r0;
			opStack -= 2;
			goto nextInstruction;
		case OP_STORE2:
			v1 = r1 & (dataMask & ~1);
			if ( !VM_DataRange( vm, v1, 2 ) ) {
				VM_INTERPRETER_ERROR( "VM STORE2 out of range" );
			}
			*(short *)&image[v1] = r0;
			opStack -= 2;
			goto nextInstruction;
		case OP_STORE1:
			image[ r1&dataMask ] = r0;
			opStack -= 2;
			goto nextInstruction;

		case OP_ARG:
			// Unlike general data addresses, stack arguments must not wrap.
			v1 = codeImage[programCounter];
			if ( (v1 & 3) || !VM_DataRange( vm, programStack + v1, 4 ) ) {
				VM_INTERPRETER_ERROR( "VM ARG out of range or unaligned" );
			}
			*(int *)&image[programStack + v1] = r0;
			opStack--;
			programCounter++;
			goto nextInstruction;

		case OP_BLOCK_COPY:
			{
				int count = r2;
				int source = r0 & dataMask, dest = r1 & dataMask;
				if ( count < 0 || !VM_DataRange( vm, source, count ) ||
				     !VM_DataRange( vm, dest, count ) ) {
					VM_INTERPRETER_ERROR( "VM BLOCK_COPY out of range" );
				}
				if ( (source | dest | count) & 3 ) {
					VM_INTERPRETER_ERROR( "VM BLOCK_COPY not dword aligned" );
				}
				memmove( image + dest, image + source, count );
				programCounter += 4;
				opStack -= 2;
			}
			goto nextInstruction;

		case OP_CALL:
			// save current program counter
			*(int *)&image[ programStack ] = programCounter;
			
			// jump to the location on the stack
			programCounter = r0;
			opStack--;
			if ( programCounter < 0 ) {
				// system call
				int		r;
				int		temp;
				int syscallArgs[16] = {0};
				int available;
#ifdef DEBUG_VM
				int		stomped;

				if ( vm_debugLevel ) {
					Com_Printf( "%s---> systemcall(%i)\n", DEBUGSTR, -1 - programCounter );
				}
#endif
				// save the stack to allow recursive VM entry
				temp = vm->callLevel;
				vm->programStack = programStack - 4;
#ifdef DEBUG_VM
				stomped = *(int *)&image[ programStack + 4 ];
#endif
				*(int *)&image[ programStack + 4 ] = -1 - programCounter;

//VM_LogSyscalls( (int *)&image[ programStack + 4 ] );
				// Dispatchers consume up to 16 integer arguments. Snapshot the
				// available words and zero missing ones so an undersized frame
				// cannot expose native memory beyond the VM data allocation.
				available = (dataMask + 1 - (programStack + 4)) / sizeof(int);
				if ( available > 16 ) {
					available = 16;
				}
				memcpy( syscallArgs, image + programStack + 4, available * sizeof(int) );
				r = vm->systemCall( syscallArgs );

#ifdef DEBUG_VM
				// this is just our stack frame pointer, only needed
				// for debugging
				*(int *)&image[ programStack + 4 ] = stomped;
#endif

				// save return value
				opStack++;
				*opStack = r;
				programCounter = *(int *)&image[ programStack ];
				vm->callLevel = temp;
#ifdef DEBUG_VM
				if ( vm_debugLevel ) {
					Com_Printf( "%s<--- %s\n", DEBUGSTR, VM_ValueToSymbol( vm, programCounter ) );
				}
#endif
				if ( !VM_ValidReturnAddress( vm, programCounter ) ) {
					VM_INTERPRETER_ERROR( "VM syscall return address out of range" );
				}
			} else {
				if ( programCounter >= vm->instructionPointersLength / (int)sizeof(int) ) {
					VM_INTERPRETER_ERROR( "VM CALL target out of range" );
				}
				programCounter = vm->instructionPointers[programCounter];
			}
			goto nextInstruction;

		// push and pop are only needed for discarded or bad function return values
		case OP_PUSH:
			*++opStack = 0;
			goto nextInstruction;
		case OP_POP:
			opStack--;
			goto nextInstruction;

		case OP_ENTER:
#ifdef DEBUG_VM
			profileSymbol = VM_ValueToFunctionSymbol( vm, programCounter );
#endif
			// get size of stack frame
			v1 = r2;

			if ( v1 < 0 || (v1 & 3) || v1 > programStack - stackFloor ) {
				VM_INTERPRETER_ERROR( "VM ENTER frame out of range" );
			}
			programCounter += 4;
			programStack -= v1;
#ifdef DEBUG_VM
			// save old stack frame for debugging traces
			*(int *)&image[programStack+4] = programStack + v1;
			if ( vm_debugLevel ) {
				Com_Printf( "%s---> %s\n", DEBUGSTR, VM_ValueToSymbol( vm, programCounter - 5 ) );
				if ( vm->breakFunction && programCounter - 5 == vm->breakFunction ) {
					// this is to allow setting breakpoints here in the debugger
					vm->breakCount++;
//					vm_debugLevel = 2;
//					VM_StackTrace( vm, programCounter, programStack );
				}
				vm->callLevel++;
			}
#endif
			goto nextInstruction;
		case OP_LEAVE:
			// remove our stack frame
			v1 = r2;

			if ( v1 < 0 || (v1 & 3) || v1 > stackOnEntry - entryFrame - programStack ) {
				VM_INTERPRETER_ERROR( "VM LEAVE frame out of range" );
			}
			programStack += v1;

			// grab the saved program counter
			programCounter = *(int *)&image[ programStack ];
#ifdef DEBUG_VM
			profileSymbol = VM_ValueToFunctionSymbol( vm, programCounter );
			if ( vm_debugLevel ) {
				vm->callLevel--;
				Com_Printf( "%s<--- %s\n", DEBUGSTR, VM_ValueToSymbol( vm, programCounter ) );
			}
#endif
			// check for leaving the VM
			if ( programCounter == -1 ) {
				if ( programStack != stackOnEntry - entryFrame ) {
					VM_INTERPRETER_ERROR( "VM return stack mismatch" );
				}
				goto done;
			}
			if ( !VM_ValidReturnAddress( vm, programCounter ) ) {
				VM_INTERPRETER_ERROR( "VM return address out of range" );
			}
			goto nextInstruction;

		/*
		===================================================================
		BRANCHES
		===================================================================
		*/

		case OP_JUMP:
			if ( (unsigned int)r0 >= vm->instructionPointersLength / sizeof(int) ) {
				VM_INTERPRETER_ERROR( "VM JUMP target out of range" );
			}
			programCounter = vm->instructionPointers[r0];
			opStack--;
			goto nextInstruction;

		case OP_EQ:
			opStack -= 2;
			if ( r1 == r0 ) {
				programCounter = r2;	//vm->instructionPointers[r2];
				goto nextInstruction;
			} else {
				programCounter += 4;
				goto nextInstruction;
			}

		case OP_NE:
			opStack -= 2;
			if ( r1 != r0 ) {
				programCounter = r2;	//vm->instructionPointers[r2];
				goto nextInstruction;
			} else {
				programCounter += 4;
				goto nextInstruction;
			}

		case OP_LTI:
			opStack -= 2;
			if ( r1 < r0 ) {
				programCounter = r2;	//vm->instructionPointers[r2];
				goto nextInstruction;
			} else {
				programCounter += 4;
				goto nextInstruction;
			}

		case OP_LEI:
			opStack -= 2;
			if ( r1 <= r0 ) {
				programCounter = r2;	//vm->instructionPointers[r2];
				goto nextInstruction;
			} else {
				programCounter += 4;
				goto nextInstruction;
			}

		case OP_GTI:
			opStack -= 2;
			if ( r1 > r0 ) {
				programCounter = r2;	//vm->instructionPointers[r2];
				goto nextInstruction;
			} else {
				programCounter += 4;
				goto nextInstruction;
			}

		case OP_GEI:
			opStack -= 2;
			if ( r1 >= r0 ) {
				programCounter = r2;	//vm->instructionPointers[r2];
				goto nextInstruction;
			} else {
				programCounter += 4;
				goto nextInstruction;
			}

		case OP_LTU:
			opStack -= 2;
			if ( ((unsigned)r1) < ((unsigned)r0) ) {
				programCounter = r2;	//vm->instructionPointers[r2];
				goto nextInstruction;
			} else {
				programCounter += 4;
				goto nextInstruction;
			}

		case OP_LEU:
			opStack -= 2;
			if ( ((unsigned)r1) <= ((unsigned)r0) ) {
				programCounter = r2;	//vm->instructionPointers[r2];
				goto nextInstruction;
			} else {
				programCounter += 4;
				goto nextInstruction;
			}

		case OP_GTU:
			opStack -= 2;
			if ( ((unsigned)r1) > ((unsigned)r0) ) {
				programCounter = r2;	//vm->instructionPointers[r2];
				goto nextInstruction;
			} else {
				programCounter += 4;
				goto nextInstruction;
			}

		case OP_GEU:
			opStack -= 2;
			if ( ((unsigned)r1) >= ((unsigned)r0) ) {
				programCounter = r2;	//vm->instructionPointers[r2];
				goto nextInstruction;
			} else {
				programCounter += 4;
				goto nextInstruction;
			}

		case OP_EQF:
			if ( ((float *)opStack)[-1] == *(float *)opStack ) {
				programCounter = r2;	//vm->instructionPointers[r2];
				opStack -= 2;
				goto nextInstruction;
			} else {
				programCounter += 4;
				opStack -= 2;
				goto nextInstruction;
			}

		case OP_NEF:
			if ( ((float *)opStack)[-1] != *(float *)opStack ) {
				programCounter = r2;	//vm->instructionPointers[r2];
				opStack -= 2;
				goto nextInstruction;
			} else {
				programCounter += 4;
				opStack -= 2;
				goto nextInstruction;
			}

		case OP_LTF:
			if ( ((float *)opStack)[-1] < *(float *)opStack ) {
				programCounter = r2;	//vm->instructionPointers[r2];
				opStack -= 2;
				goto nextInstruction;
			} else {
				programCounter += 4;
				opStack -= 2;
				goto nextInstruction;
			}

		case OP_LEF:
			if ( ((float *)opStack)[-1] <= *(float *)opStack ) {
				programCounter = r2;	//vm->instructionPointers[r2];
				opStack -= 2;
				goto nextInstruction;
			} else {
				programCounter += 4;
				opStack -= 2;
				goto nextInstruction;
			}

		case OP_GTF:
			if ( ((float *)opStack)[-1] > *(float *)opStack ) {
				programCounter = r2;	//vm->instructionPointers[r2];
				opStack -= 2;
				goto nextInstruction;
			} else {
				programCounter += 4;
				opStack -= 2;
				goto nextInstruction;
			}

		case OP_GEF:
			if ( ((float *)opStack)[-1] >= *(float *)opStack ) {
				programCounter = r2;	//vm->instructionPointers[r2];
				opStack -= 2;
				goto nextInstruction;
			} else {
				programCounter += 4;
				opStack -= 2;
				goto nextInstruction;
			}


		//===================================================================

		// Arithmetic edge cases never fault: retail QVMs depend on native
		// results (cg_scoreboard.c evaluates 1 << score->client for every
		// client, including 32-63). Reproduce what retail PowerPC clients
		// computed without host undefined behavior: vm_ppc.c/vm_ppc_new.c
		// emit divw/divwu, mullw+subf for remainders, slw/sraw/srw and
		// fctiwz. divw leaves x / 0 and INT_MIN / -1 undefined (x86 idiv
		// faults), so define them as 0 and the wrapped quotient INT_MIN.

		case OP_NEGI:
			*opStack = (int)(0u - (unsigned int)r0);
			goto nextInstruction;
		case OP_ADD:
			opStack[-1] = (int)((unsigned int)r1 + (unsigned int)r0);
			opStack--;
			goto nextInstruction;
		case OP_SUB:
			opStack[-1] = (int)((unsigned int)r1 - (unsigned int)r0);
			opStack--;
			goto nextInstruction;
		case OP_DIVI:
			// x / 0 is 0; INT_MIN / -1 wraps to INT_MIN.
			if ( r0 == 0 ) {
				opStack[-1] = 0;
			} else if ( r0 == -1 ) {
				opStack[-1] = (int)(0u - (unsigned int)r1);
			} else {
				opStack[-1] = r1 / r0;
			}
			opStack--;
			goto nextInstruction;
		case OP_DIVU:
			opStack[-1] = r0 == 0 ? 0 : ((unsigned)r1) / ((unsigned)r0);
			opStack--;
			goto nextInstruction;
		case OP_MODI:
			// Remainders are x - (x / y) * y, so x % 0 keeps x whatever
			// divw/divwu returned, as on PowerPC; INT_MIN % -1 is 0.
			if ( r0 == -1 ) {
				opStack[-1] = 0;
			} else if ( r0 != 0 ) {
				opStack[-1] = r1 % r0;
			}
			opStack--;
			goto nextInstruction;
		case OP_MODU:
			if ( r0 != 0 ) {
				opStack[-1] = ((unsigned)r1) % (unsigned)r0;
			}
			opStack--;
			goto nextInstruction;
		case OP_MULI:
			opStack[-1] = (int)((unsigned int)r1 * (unsigned int)r0);
			opStack--;
			goto nextInstruction;
		case OP_MULU:
			opStack[-1] = ((unsigned)r1) * ((unsigned)r0);
			opStack--;
			goto nextInstruction;

		case OP_BAND:
			opStack[-1] = ((unsigned)r1) & ((unsigned)r0);
			opStack--;
			goto nextInstruction;
		case OP_BOR:
			opStack[-1] = ((unsigned)r1) | ((unsigned)r0);
			opStack--;
			goto nextInstruction;
		case OP_BXOR:
			opStack[-1] = ((unsigned)r1) ^ ((unsigned)r0);
			opStack--;
			goto nextInstruction;
		case OP_BCOM:
			*opStack = ~ ((unsigned)r0);
			goto nextInstruction;

		// slw/srw/sraw use the low six count bits: counts 32-63 (and -1)
		// shift every bit out, so 1 << 32 is 0 and sraw fills with the sign,
		// while 64 shifts by 0. x86 would mask to five bits (1 << 32 == 1).
		case OP_LSH:
			opStack[-1] = (r0 & 32) ? 0 : (int)((unsigned int)r1 << (r0 & 31));
			opStack--;
			goto nextInstruction;
		case OP_RSHI:
			r0 = (r0 & 32) ? 31 : (r0 & 31);	// sign fill equals a 31-bit shift
			// Spell out sign extension without implementation-defined shifts.
			opStack[-1] = (int)((unsigned int)r1 >> r0);
			if ( r1 < 0 && r0 != 0 ) {
				opStack[-1] |= (int)(~0u << (32 - r0));
			}
			opStack--;
			goto nextInstruction;
		case OP_RSHU:
			opStack[-1] = (r0 & 32) ? 0 : ((unsigned)r1) >> (r0 & 31);
			opStack--;
			goto nextInstruction;

		case OP_NEGF:
			*(float *)opStack =  -*(float *)opStack;
			goto nextInstruction;
		case OP_ADDF:
			*(float *)(opStack-1) = *(float *)(opStack-1) + *(float *)opStack;
			opStack--;
			goto nextInstruction;
		case OP_SUBF:
			*(float *)(opStack-1) = *(float *)(opStack-1) - *(float *)opStack;
			opStack--;
			goto nextInstruction;
		case OP_DIVF:
			*(float *)(opStack-1) = *(float *)(opStack-1) / *(float *)opStack;
			opStack--;
			goto nextInstruction;
		case OP_MULF:
			*(float *)(opStack-1) = *(float *)(opStack-1) * *(float *)opStack;
			opStack--;
			goto nextInstruction;

		case OP_CVIF:
			*(float *)opStack =  (float)*opStack;
			goto nextInstruction;
		case OP_CVFI:
			{
				float value = *(float *)opStack;
				// Truncate like fctiwz: saturate out-of-range values and give
				// INT_MIN for NaN (x86 conversions also give INT_MIN for NaN).
				// INT_MAX rounds up when converted to float, so compare with
				// the exact exclusive bound before the C cast.
				if ( value >= 2147483648.0f ) {
					*opStack = INT_MAX;
				} else if ( value >= -2147483648.0f ) {
					*opStack = (int)value;
				} else {
					*opStack = INT_MIN;	// below range or NaN
				}
			}
			goto nextInstruction;
		case OP_SEX8:
			*opStack = (signed char)*opStack;
			goto nextInstruction;
		case OP_SEX16:
			*opStack = (short)*opStack;
			goto nextInstruction;
		}
	}

done:
	if ( opStack != &stack[2] ) {
		VM_INTERPRETER_ERROR( "VM return operand stack mismatch" );
	}
	vm->currentlyInterpreting = wasInterpreting;
	vm->programStack = stackOnEntry;
#undef VM_INTERPRETER_ERROR

	// return the result
	return *opStack;
}
