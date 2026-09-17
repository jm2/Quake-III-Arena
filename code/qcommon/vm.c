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
// vm.c -- virtual machine

/*


intermix code and data
symbol table

a dll has one imported function: VM_SystemCall
and one exported function: Perform


*/

#include "vm_local.h"

#include <limits.h>


vm_t	*currentVM = NULL; // bk001212
vm_t	*lastVM    = NULL; // bk001212
int		vm_debugLevel;

#define	MAX_VM		3
vm_t	vmTable[MAX_VM];


void VM_VmInfo_f( void );
void VM_VmProfile_f( void );


// converts a VM pointer to a C pointer and
// checks to make sure that the range is acceptable
void	*VM_VM2C( vmptr_t p, int length ) {
	return (void *)p;
}

void VM_Debug( int level ) {
	vm_debugLevel = level;
}

/*
==============
VM_Init
==============
*/
void VM_Init( void ) {
	Cvar_Get( "vm_cgame", "2", CVAR_ARCHIVE );	// !@# SHIP WITH SET TO 2
	Cvar_Get( "vm_game", "2", CVAR_ARCHIVE );	// !@# SHIP WITH SET TO 2
	Cvar_Get( "vm_ui", "2", CVAR_ARCHIVE );		// !@# SHIP WITH SET TO 2

	Cmd_AddCommand ("vmprofile", VM_VmProfile_f );
	Cmd_AddCommand ("vminfo", VM_VmInfo_f );

	Com_Memset( vmTable, 0, sizeof( vmTable ) );
}


/*
===============
VM_ValueToSymbol

Assumes a program counter value
===============
*/
const char *VM_ValueToSymbol( vm_t *vm, int value ) {
	vmSymbol_t	*sym;
	static char		text[MAX_TOKEN_CHARS];

	sym = vm->symbols;
	if ( !sym ) {
		return "NO SYMBOLS";
	}

	// find the symbol
	while ( sym->next && sym->next->symValue <= value ) {
		sym = sym->next;
	}

	if ( value == sym->symValue ) {
		return sym->symName;
	}

	Com_sprintf( text, sizeof( text ), "%s+%i", sym->symName, value - sym->symValue );

	return text;
}

/*
===============
VM_ValueToFunctionSymbol

For profiling, find the symbol behind this value
===============
*/
vmSymbol_t *VM_ValueToFunctionSymbol( vm_t *vm, int value ) {
	vmSymbol_t	*sym;
	static vmSymbol_t	nullSym;

	sym = vm->symbols;
	if ( !sym ) {
		return &nullSym;
	}

	while ( sym->next && sym->next->symValue <= value ) {
		sym = sym->next;
	}

	return sym;
}


/*
===============
VM_SymbolToValue
===============
*/
int VM_SymbolToValue( vm_t *vm, const char *symbol ) {
	vmSymbol_t	*sym;

	for ( sym = vm->symbols ; sym ; sym = sym->next ) {
		if ( !strcmp( symbol, sym->symName ) ) {
			return sym->symValue;
		}
	}
	return 0;
}


/*
=====================
VM_SymbolForCompiledPointer
=====================
*/
const char *VM_SymbolForCompiledPointer( vm_t *vm, void *code ) {
	int			i;

	if ( code < (void *)vm->codeBase ) {
		return "Before code block";
	}
	if ( code >= (void *)(vm->codeBase + vm->codeLength) ) {
		return "After code block";
	}

	// find which original instruction it is after
	for ( i = 0 ; i < vm->codeLength ; i++ ) {
		if ( (void *)vm->instructionPointers[i] > code ) {
			break;
		}
	}
	i--;

	// now look up the bytecode instruction pointer
	return VM_ValueToSymbol( vm, i );
}



/*
===============
ParseHex
===============
*/
int	ParseHex( const char *text ) {
	int		value;
	int		c;

	value = 0;
	while ( ( c = *text++ ) != 0 ) {
		if ( c >= '0' && c <= '9' ) {
			value = value * 16 + c - '0';
			continue;
		}
		if ( c >= 'a' && c <= 'f' ) {
			value = value * 16 + 10 + c - 'a';
			continue;
		}
		if ( c >= 'A' && c <= 'F' ) {
			value = value * 16 + 10 + c - 'A';
			continue;
		}
	}

	return value;
}

/*
===============
VM_LoadSymbols
===============
*/
void VM_LoadSymbols( vm_t *vm ) {
	int		len;
	char	*mapfile, *text_p, *token;
	char	name[MAX_QPATH];
	char	symbols[MAX_QPATH];
	vmSymbol_t	**prev, *sym;
	int		count;
	int		value;
	int		chars;
	int		segment;
	int		numInstructions;

	// don't load symbols if not developer
	if ( !com_developer->integer ) {
		return;
	}

	COM_StripExtension( vm->name, name, sizeof(name) );
	Com_sprintf( symbols, sizeof( symbols ), "vm/%s.map", name );
	len = FS_ReadFile( symbols, (void **)&mapfile );
	if ( !mapfile ) {
		Com_Printf( "Couldn't load symbol file: %s\n", symbols );
		return;
	}

	numInstructions = vm->instructionPointersLength >> 2;

	// parse the symbols
	text_p = mapfile;
	prev = &vm->symbols;
	count = 0;

	while ( 1 ) {
		token = COM_Parse( &text_p );
		if ( !token[0] ) {
			break;
		}
		segment = ParseHex( token );
		if ( segment ) {
			COM_Parse( &text_p );
			COM_Parse( &text_p );
			continue;		// only load code segment values
		}

		token = COM_Parse( &text_p );
		if ( !token[0] ) {
			Com_Printf( "WARNING: incomplete line at end of file\n" );
			break;
		}
		value = ParseHex( token );

		token = COM_Parse( &text_p );
		if ( !token[0] ) {
			Com_Printf( "WARNING: incomplete line at end of file\n" );
			break;
		}
		chars = strlen( token );
		sym = Hunk_Alloc( sizeof( *sym ) + chars, h_high );
		*prev = sym;
		prev = &sym->next;
		sym->next = NULL;

		// convert value from an instruction number to a code offset
		if ( value >= 0 && value < numInstructions ) {
			value = vm->instructionPointers[value];
		}

		sym->symValue = value;
		Q_strncpyz( sym->symName, token, chars + 1 );

		count++;
	}

	vm->numSymbols = count;
	Com_Printf( "%i symbols parsed from %s\n", count, symbols );
	FS_FreeFile( mapfile );
}

/*
============
VM_DllSyscall

Dlls will call this directly

 rcg010206 The horror; the horror.

  The syscall mechanism relies on stack manipulation to get it's args.
   This is likely due to C's inability to pass "..." parameters to
   a function in one clean chunk. On PowerPC Linux, these parameters
   are not necessarily passed on the stack, so while (&arg[0] == arg)
   is true, (&arg[1] == 2nd function parameter) is not necessarily
   accurate, as arg's value might have been stored to the stack or
   other piece of scratch memory to give it a valid address, but the
   next parameter might still be sitting in a register.

  Quake's syscall system also assumes that the stack grows downward,
   and that any needed types can be squeezed, safely, into a signed int.

  This hack below copies all needed values for an argument to a
   array in memory, so that Quake can get the correct values. This can
   also be used on systems where the stack grows upwards, as the
   presumably standard and safe stdargs.h macros are used.

  As for having enough space in a signed int for your datatypes, well,
   it might be better to wait for DOOM 3 before you start porting.  :)

  The original code, while probably still inherently dangerous, seems
   to work well enough for the platforms it already works on. Rather
   than add the performance hit for those platforms, the original code
   is still in use there.

  For speed, we just grab 15 arguments, and don't worry about exactly
   how many the syscall actually needs; the extra is thrown away.
 
============
*/
int QDECL VM_DllSyscall( int arg, ... ) {
  // rcg010206 - see commentary above
  int args[16];
  int i;
  va_list ap;
  
  args[0] = arg;
  
  va_start(ap, arg);
  for (i = 1; i < sizeof (args) / sizeof (args[i]); i++)
    args[i] = va_arg(ap, int);
  va_end(ap);
  
  return currentVM->systemCall( args );
}

/*
=================
VM_ValidateQVMHeader

Validate the complete on-disk image before either load path allocates, clears,
or copies VM memory. Offsets are checked by subtraction before forming sums.
The largest data image must still fit a positive, power-of-two signed int.
=================
*/
static qboolean VM_ValidateQVMHeader( vmHeader_t *header, int length,
                                    int *dataLength ) {
	int i, initializedLength, imageLength, allocationLength;
	const int maxDataLength = INT_MAX / 2 + 1;

	if ( length < (int)sizeof( *header ) || length > INT_MAX - 31 ) {
		return qfalse;
	}

	for ( i = 0; i < (int)(sizeof( *header ) / sizeof( int )); i++ ) {
		((int *)header)[i] = LittleLong( ((int *)header)[i] );
	}

	if ( header->vmMagic != VM_MAGIC || header->instructionCount <= 0 ||
	     header->codeOffset < (int)sizeof( *header ) ||
	     header->codeOffset > length || header->codeLength <= 0 ||
	     header->codeLength > length - header->codeOffset ||
	     header->instructionCount > header->codeLength ||
	     // Interpreter instructions expand to ints; Hunk_Alloc rounds to 32.
	     header->codeLength > (INT_MAX - 31) / (int)sizeof( int ) ||
	     header->dataOffset < header->codeOffset + header->codeLength ||
	     header->dataOffset > length || header->dataLength < 0 ||
	     (header->dataLength & 3) || header->litLength < 0 ||
	     header->bssLength < 0 ) {
		return qfalse;
	}

	if ( header->dataLength > length - header->dataOffset ||
	     header->litLength > length - header->dataOffset - header->dataLength ) {
		return qfalse;
	}
	initializedLength = header->dataLength + header->litLength;
	if ( initializedLength > maxDataLength ||
	     header->bssLength > maxDataLength - initializedLength ) {
		return qfalse;
	}
	imageLength = initializedLength + header->bssLength;
	if ( imageLength == 0 ) {
		return qfalse;
	}

	for ( allocationLength = 1; allocationLength < imageLength;
	      allocationLength *= 2 ) {
	}
	*dataLength = allocationLength;
	return qtrue;
}

/*
=================
VM_Restart

Reload the data, but leave everything else in place
This allows a server to do a map_restart without changing memory allocation
=================
*/
vm_t *VM_Restart( vm_t *vm ) {
	vmHeader_t	*header;
	int			length;
	int			dataLength;
	int			i;
	char		filename[MAX_QPATH];

	// DLL's can't be restarted in place
	if ( vm->dllHandle ) {
		char	name[MAX_QPATH];
	    int			(*systemCall)( int *parms );
		
		systemCall = vm->systemCall;	
		Q_strncpyz( name, vm->name, sizeof( name ) );

		VM_Free( vm );

		vm = VM_Create( name, systemCall, VMI_NATIVE );
		return vm;
	}

	// load the image
	Com_Printf( "VM_Restart()\n" );
	Com_sprintf( filename, sizeof(filename), "vm/%s.qvm", vm->name );
	Com_Printf( "Loading vm file %s.\n", filename );
	length = FS_ReadFile( filename, (void **)&header );
	if ( !header ) {
		Com_Error( ERR_DROP, "VM_Restart failed.\n" );
		return NULL;
	}

	if ( !VM_ValidateQVMHeader( header, length, &dataLength ) ) {
		FS_FreeFile( header );
		// ERR_DROP calls the module's shutdown entry point before VM_Free.
		// Keep the live VM intact for the caller's normal cleanup.
		Com_Error( ERR_DROP, "%s has an invalid QVM header", filename );
		return NULL;
	}

	// Restart keeps the existing allocation and stack. A replacement file
	// must not resize it, even when all of its own file ranges are valid.
	if ( dataLength != vm->dataMask + 1 ) {
		FS_FreeFile( header );
		// ERR_DROP calls the module's shutdown entry point before VM_Free.
		// Keep the live VM intact for the caller's normal cleanup.
		Com_Error( ERR_DROP, "%s changed QVM data size on restart", filename );
		return NULL;
	}

	// Retained code and instruction tables belong to the original image.
	// Compare normalized headers plus every code/data/padding byte before
	// replacing data; a same-sized replacement can still be incompatible.
	if ( length != vm->qvmImageLength || !vm->qvmImage ||
	     memcmp( header, vm->qvmImage, length ) ) {
		FS_FreeFile( header );
		// Keep the old VM intact for ERR_DROP's normal shutdown callback.
		Com_Error( ERR_DROP, "%s changed QVM image on restart", filename );
		return NULL;
	}

	// clear the data
	Com_Memset( vm->dataBase, 0, dataLength );

	// copy the intialized data
	Com_Memcpy( vm->dataBase, (byte *)header + header->dataOffset, header->dataLength + header->litLength );

	// byte swap the longs
	for ( i = 0 ; i < header->dataLength ; i += 4 ) {
		*(int *)(vm->dataBase + i) = LittleLong( *(int *)(vm->dataBase + i ) );
	}

	// free the original file
	FS_FreeFile( header );

	return vm;
}

/*
================
VM_Create

If image ends in .qvm it will be interpreted, otherwise
it will attempt to load as a system dll
================
*/

#define	STACK_SIZE	0x20000

vm_t *VM_Create( const char *module, int (*systemCalls)(int *), 
				vmInterpret_t interpret ) {
	vm_t		*vm;
	vmHeader_t	*header;
	int			length;
	int			dataLength;
	int			i, remaining;
	char		filename[MAX_QPATH];

	if ( !module || !module[0] || !systemCalls ) {
		Com_Error( ERR_FATAL, "VM_Create: bad parms" );
	}

	remaining = Hunk_MemoryRemaining();

	// see if we already have the VM
	for ( i = 0 ; i < MAX_VM ; i++ ) {
		if (!Q_stricmp(vmTable[i].name, module)) {
			vm = &vmTable[i];
			return vm;
		}
	}

	// find a free vm
	for ( i = 0 ; i < MAX_VM ; i++ ) {
		if ( !vmTable[i].name[0] ) {
			break;
		}
	}

	if ( i == MAX_VM ) {
		Com_Error( ERR_FATAL, "VM_Create: no free vm_t" );
	}

	vm = &vmTable[i];

	Q_strncpyz( vm->name, module, sizeof( vm->name ) );
	vm->systemCall = systemCalls;

	// never allow dll loading with a demo
	if ( interpret == VMI_NATIVE ) {
		if ( Cvar_VariableValue( "fs_restrict" ) ) {
			interpret = VMI_COMPILED;
		}
	}

	if ( interpret == VMI_NATIVE ) {
		// try to load as a system dll
		Com_Printf( "Loading dll file %s.\n", vm->name );
		vm->dllHandle = Sys_LoadDll( module, vm->fqpath , &vm->entryPoint, VM_DllSyscall );
		if ( vm->dllHandle ) {
			return vm;
		}

		Com_Printf( "Failed to load dll, looking for qvm.\n" );
		interpret = VMI_COMPILED;
	}

	// load the image
	Com_sprintf( filename, sizeof(filename), "vm/%s.qvm", vm->name );
	Com_Printf( "Loading vm file %s.\n", filename );
	length = FS_ReadFile( filename, (void **)&header );
	if ( !header ) {
		Com_Printf( "Failed.\n" );
		VM_Free( vm );
		return NULL;
	}

	if ( !VM_ValidateQVMHeader( header, length, &dataLength ) ) {
		FS_FreeFile( header );
		VM_Free( vm );
		Com_Error( ERR_DROP, "%s has an invalid QVM header", filename );
		return NULL;
	}

	// allocate zero filled space for initialized and uninitialized data
	vm->dataBase = Hunk_Alloc( dataLength, h_high );
	vm->dataMask = dataLength - 1;

	// copy the intialized data
	Com_Memcpy( vm->dataBase, (byte *)header + header->dataOffset, header->dataLength + header->litLength );

	// byte swap the longs
	for ( i = 0 ; i < header->dataLength ; i += 4 ) {
		*(int *)(vm->dataBase + i) = LittleLong( *(int *)(vm->dataBase + i ) );
	}

	// allocate space for the jump targets, which will be filled in by the compile/prep functions
	vm->instructionPointersLength = header->instructionCount * 4;
	vm->instructionPointers = Hunk_Alloc( vm->instructionPointersLength, h_high );

	// copy or compile the instructions
	vm->codeLength = header->codeLength;

#ifdef __MACOS__
	// VM_Compile / VM_CallCompiled are empty stubs on Mac OS 9; a
	// "compiled" VM would silently return 0 from every vmMain call
	// (this path is reachable via fs_restrict / pure servers / mods
	// shipping only QVMs). Always use the interpreter instead.
	if ( interpret >= VMI_COMPILED ) {
		Com_Printf( "Forcing interpreted VM for %s (no compiler on Mac OS 9).\n", vm->name );
		interpret = VMI_BYTECODE;
	}
#endif
	if ( interpret >= VMI_COMPILED ) {
		vm->compiled = qtrue;
		VM_Compile( vm, header );
	} else {
		vm->compiled = qfalse;
		if ( !VM_PrepareInterpreter( vm, header ) ) {
			FS_FreeFile( header );
			VM_Free( vm );
			Com_Error( ERR_DROP, "%s has invalid QVM bytecode", filename );
			return NULL;
		}
	}

	// A map_restart reuses executable state. Keep the original image so
	// code, layout, initial data, and padding changes are rejected exactly.
	vm->qvmImage = Hunk_Alloc( length, h_high );
	vm->qvmImageLength = length;
	Com_Memcpy( vm->qvmImage, header, length );

	// free the original file
	FS_FreeFile( header );

	// load the map file
	VM_LoadSymbols( vm );

	// the stack is implicitly at the end of the image
	vm->programStack = vm->dataMask + 1;
	vm->stackBottom = vm->programStack - STACK_SIZE;

	Com_Printf("%s loaded in %d bytes on the hunk\n", module, remaining - Hunk_MemoryRemaining());

	return vm;
}

/*
==============
VM_Free
==============
*/
void VM_Free( vm_t *vm ) {

	if ( vm->dllHandle ) {
		Sys_UnloadDll( vm->dllHandle );
		Com_Memset( vm, 0, sizeof( *vm ) );
	}
#if 0	// now automatically freed by hunk
	if ( vm->codeBase ) {
		Z_Free( vm->codeBase );
	}
	if ( vm->dataBase ) {
		Z_Free( vm->dataBase );
	}
	if ( vm->instructionPointers ) {
		Z_Free( vm->instructionPointers );
	}
#endif
	Com_Memset( vm, 0, sizeof( *vm ) );

	currentVM = NULL;
	lastVM = NULL;
}

void VM_Clear(void) {
	int i;
	for (i=0;i<MAX_VM; i++) {
		if ( vmTable[i].dllHandle ) {
			Sys_UnloadDll( vmTable[i].dllHandle );
		}
		Com_Memset( &vmTable[i], 0, sizeof( vm_t ) );
	}
	currentVM = NULL;
	lastVM = NULL;
}

void *VM_ArgPtr( int intValue ) {
	if ( !intValue ) {
		return NULL;
	}
	// bk001220 - currentVM is missing on reconnect
	if ( currentVM==NULL )
	  return NULL;

	if ( currentVM->entryPoint ) {
		return (void *)(currentVM->dataBase + intValue);
	}
	else {
		return (void *)(currentVM->dataBase + (intValue & currentVM->dataMask));
	}
}

/** Mark the active QVM as faulted before ERR_DROP can invoke module shutdown. */
void VM_Error( const char *message ) {
	if ( currentVM && !currentVM->entryPoint ) {
		currentVM->interpretFaulted = qtrue;
		currentVM->currentlyInterpreting = qfalse;
	}
	Com_Error( ERR_DROP, "%s", message );
}

/* Alignment is required when native code consumes VM structs or floats. */
/** Validate the complete QVM range and alignment, retaining masked addresses and optional NULL. */
void *VM_CheckedArgPtr( int value, int length, int alignment, qboolean nullable ) {
	int offset;
	if ( !currentVM || length < 0 || (alignment != 1 && alignment != 4) ||
	     (!value && !nullable) ) {
		VM_Error( "VM syscall buffer out of range" );
		return NULL;
	}
	if ( !value ) {
		return NULL;
	}
	if ( currentVM->entryPoint ) {
		return (void *)(unsigned long)(unsigned int)value;
	}
	offset = value & currentVM->dataMask;
	if ( (offset & (alignment - 1)) || length > currentVM->dataMask + 1 - offset ) {
		VM_Error( "VM syscall buffer out of range or unaligned" );
		return NULL;
	}
	return currentVM->dataBase + offset;
}

/** Require a terminator inside the QVM image before native string code reads it. */
char *VM_CheckedArgString( int value, qboolean nullable ) {
	char *input = VM_CheckedArgPtr( value, 1, 1, nullable );
	if ( input && !currentVM->entryPoint &&
	     !memchr( input, 0, currentVM->dataMask + 1 - (value & currentVM->dataMask) ) ) {
		VM_Error( "VM syscall string is not terminated" );
		return NULL;
	}
	return input;
}

/** Reject negative or overflowing element counts before checking an aligned array range. */
void *VM_CheckedArgArray( int value, int count, int elementSize ) {
	if ( count < 0 || elementSize <= 0 || count > INT_MAX / elementSize ) {
		VM_Error( "VM syscall array size out of range" );
		return NULL;
	}
	return VM_CheckedArgPtr( value, count * elementSize, 4, count == 0 );
}

/** Check output capacity, reserving a terminator slot unless NULL is an allowed query. */
void *VM_CheckedStringBuffer( int value, int length, qboolean nullable ) {
	// A NULL output is supported by query/reset APIs. Non-NULL string outputs
	// require a terminator slot; zero lengths must not reach Q_strncpyz.
	if ( length < 0 || (value && length == 0) ) {
		VM_Error( "VM syscall string buffer is empty" );
		return NULL;
	}
	return VM_CheckedArgPtr( value, length, 1, nullable );
}

static void *VM_TrapBuffer( int value, int length ) {
	return VM_CheckedArgPtr( value, length, 1, length == 0 );
}

void VM_MemoryFill( int dest, int value, int length ) {
	void *buffer = VM_TrapBuffer( dest, length );
	if ( length ) {
		Com_Memset( buffer, value, length );
	}
}

void VM_MemoryCopy( int dest, int source, int length ) {
	void *output = VM_TrapBuffer( dest, length );
	void *input = VM_TrapBuffer( source, length );
	if ( length ) {
		memmove( output, input, length );
	}
}

int VM_StringCopy( int dest, int source, int length ) {
	char *output = VM_TrapBuffer( dest, length );
	char *input;
	int copied = 0;
	if ( length == 0 ) {
		return dest;
	}
	input = VM_TrapBuffer( source, 1 );
	if ( currentVM->entryPoint ) {
		strncpy( output, input, length );
		return dest;
	}
	// strncpy may stop reading early at NUL, so permit a short source when
	// it terminates in range. Reject an unterminated source before writing.
	while ( copied < length ) {
		if ( copied >= currentVM->dataMask + 1 - (source & currentVM->dataMask) ) {
			VM_TrapBuffer( source, copied + 1 );
			return dest;
		}
		if ( input[copied++] == '\0' ) {
			break;
		}
	}
	memmove( output, input, copied );
	if ( copied < length ) {
		Com_Memset( output + copied, 0, length - copied );
	}
	return dest;
}

void *VM_ExplicitArgPtr( vm_t *vm, int intValue ) {
	if ( !intValue ) {
		return NULL;
	}

	// bk010124 - currentVM is missing on reconnect here as well?
	if ( currentVM==NULL )
	  return NULL;

	//
	if ( vm->entryPoint ) {
		return (void *)(vm->dataBase + intValue);
	}
	else {
		return (void *)(vm->dataBase + (intValue & vm->dataMask));
	}
}


/*
==============
VM_Call


Upon a system call, the stack will look like:

sp+32	parm1
sp+28	parm0
sp+24	return value
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

/* Shared marshalling for interpreted and compiled QVM entry. */
int VM_SetupCallFrame( vm_t *vm, const int *args ) {
	int stack = vm->programStack;
	int floor = vm->stackBottom > 0 ? vm->stackBottom : 0;
	int arg;
	if ( !args || (stack & 3) || stack < floor ||
	     stack - floor < VM_ENTRY_FRAME_SIZE || stack > vm->dataMask + 1 ) {
		vm->interpretFaulted = qtrue;
		vm->currentlyInterpreting = qfalse;
		Com_Error( ERR_DROP, "VM entry stack out of range" );
		return 0;
	}
	stack -= VM_ENTRY_FRAME_SIZE;
	for ( arg = 0; arg < MAX_VMMAIN_ARGS; arg++ ) {
		*(int *)(vm->dataBase + stack + 8 + arg * 4) = args[arg];
	}
	*(int *)(vm->dataBase + stack + 4) = 0;
	*(int *)(vm->dataBase + stack) = -1;
	return stack;
}

int VM_CallArgs( vm_t *vm, int callnum, const int *parameters, int count ) {
	vm_t *oldVM;
	int result, args[MAX_VMMAIN_ARGS] = {0};

	if ( !vm || count < 0 || count >= MAX_VMMAIN_ARGS || (count && !parameters) ) {
		Com_Error( ERR_FATAL, "VM_Call: invalid VM or argument count" );
		return 0;
	}
	if ( !vm->entryPoint && vm->interpretFaulted ) {
		return 0; // ERR_DROP module shutdown must not re-enter a faulted VM
	}
	args[0] = callnum;
	if ( count ) {
		Com_Memcpy( args + 1, parameters, count * sizeof(int) );
	}
	oldVM = currentVM;
	currentVM = vm;
	lastVM = vm;
	if ( vm_debugLevel ) {
		Com_Printf( "VM_Call( %i )\n", callnum );
	}

	if ( vm->entryPoint ) {
		// Retail vmMain uses a fixed command + twelve-parameter signature.
		// Its calling convention on PPC differs from a varargs function.
		typedef int (QDECL *vmMain_t)(int, int, int, int, int, int, int,
		                             int, int, int, int, int, int);
		vmMain_t entry = (vmMain_t)vm->entryPoint;
		result = entry( args[0], args[1], args[2], args[3], args[4],
		                args[5], args[6], args[7], args[8], args[9],
		                args[10], args[11], args[12] );
	} else if ( vm->compiled ) {
		result = VM_CallCompiled( vm, args );
	} else {
		result = VM_CallInterpreted( vm, args );
	}
	if ( oldVM ) {
		currentVM = oldVM;
	}
	return result;
}

//=================================================================

static int QDECL VM_ProfileSort( const void *a, const void *b ) {
	vmSymbol_t	*sa, *sb;

	sa = *(vmSymbol_t **)a;
	sb = *(vmSymbol_t **)b;

	if ( sa->profileCount < sb->profileCount ) {
		return -1;
	}
	if ( sa->profileCount > sb->profileCount ) {
		return 1;
	}
	return 0;
}

/*
==============
VM_VmProfile_f

==============
*/
void VM_VmProfile_f( void ) {
	vm_t		*vm;
	vmSymbol_t	**sorted, *sym;
	int			i;
	double		total;

	if ( !lastVM ) {
		return;
	}

	vm = lastVM;

	if ( !vm->numSymbols ) {
		return;
	}

	sorted = Z_Malloc( vm->numSymbols * sizeof( *sorted ) );
	sorted[0] = vm->symbols;
	total = sorted[0]->profileCount;
	for ( i = 1 ; i < vm->numSymbols ; i++ ) {
		sorted[i] = sorted[i-1]->next;
		total += sorted[i]->profileCount;
	}

	qsort( sorted, vm->numSymbols, sizeof( *sorted ), VM_ProfileSort );

	for ( i = 0 ; i < vm->numSymbols ; i++ ) {
		int		perc;

		sym = sorted[i];

		perc = 100 * (float) sym->profileCount / total;
		Com_Printf( "%2i%% %9i %s\n", perc, sym->profileCount, sym->symName );
		sym->profileCount = 0;
	}

	Com_Printf("    %9.0f total\n", total );

	Z_Free( sorted );
}

/*
==============
VM_VmInfo_f

==============
*/
void VM_VmInfo_f( void ) {
	vm_t	*vm;
	int		i;

	Com_Printf( "Registered virtual machines:\n" );
	for ( i = 0 ; i < MAX_VM ; i++ ) {
		vm = &vmTable[i];
		if ( !vm->name[0] ) {
			break;
		}
		Com_Printf( "%s : ", vm->name );
		if ( vm->dllHandle ) {
			Com_Printf( "native\n" );
			continue;
		}
		if ( vm->compiled ) {
			Com_Printf( "compiled on load\n" );
		} else {
			Com_Printf( "interpreted\n" );
		}
		Com_Printf( "    code length : %7i\n", vm->codeLength );
		Com_Printf( "    table length: %7i\n", vm->instructionPointersLength );
		Com_Printf( "    data length : %7i\n", vm->dataMask + 1 );
	}
}

/*
===============
VM_LogSyscalls

Insert calls to this while debugging the vm compiler
===============
*/
void VM_LogSyscalls( int *args ) {
	static	int		callnum;
	static	FILE	*f;

	if ( !f ) {
		f = fopen("syscalls.log", "w" );
	}
	callnum++;
	fprintf(f, "%i: %i (%i) = %i %i %i %i\n", callnum, args - (int *)currentVM->dataBase,
		args[0], args[1], args[2], args[3], args[4] );
}



#ifdef oDLL_ONLY // bk010215 - for DLL_ONLY dedicated servers/builds w/o VM
int	VM_CallCompiled( vm_t *vm, int *args ) {
  return(0); 
}

void VM_Compile( vm_t *vm, vmHeader_t *header ) {}
#endif // DLL_ONLY
