/* Issue #35: exercise the real VM_Create/VM_Restart file-loading paths.
 * Interpreter execution is deliberately stubbed; its sandbox is a separate
 * acceptance criterion. File buffers are exact-sized to expose overreads.
 */
#include "../code/qcommon/vm_local.h"
#include "../code/game/g_public.h"
#include <limits.h>
#include <setjmp.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FIXTURE_SIZE 47
#define MAX_ALLOCS 8

static byte fixture[FIXTURE_SIZE];
static int fixtureLength;
static void *allocations[MAX_ALLOCS];
static int allocationCount, fileReads, fileFrees, preparations;
static int expectError, errorLevel;
static vm_t *shutdownVM;
static vm_t savedVM;
static int shutdownCalls;
static jmp_buf errorJump;
static cvar_t developer;
cvar_t *com_developer = &developer;
extern vm_t vmTable[];

static void Check( int condition, const char *message ) {
	if ( !condition ) {
		fprintf( stderr, "QVM loading regression failed: %s\n", message );
		exit( 1 );
	}
}

void QDECL Com_Error( int level, const char *format, ... ) {
	(void)format;
	Check( expectError, "unexpected engine error" );
	errorLevel = level;
	if ( shutdownVM ) {
		// Model Com_Error -> SV_ShutdownGameProgs before the error longjmp.
		Check( !memcmp( shutdownVM, &savedVM, sizeof(savedVM) ),
		       "restart cleared the live VM before error cleanup" );
		Check( fileReads == fileFrees, "file buffer retained during shutdown" );
		VM_Call( shutdownVM, GAME_SHUTDOWN, qfalse );
		VM_Free( shutdownVM );
		shutdownVM = NULL;
	}
	longjmp( errorJump, 1 );
}

void QDECL Com_Printf( const char *format, ... ) { (void)format; }
void Com_Memset( void *dest, const int value, const size_t count ) {
	memset( dest, value, count );
}
void Com_Memcpy( void *dest, const void *src, const size_t count ) {
	memcpy( dest, src, count );
}
float Cvar_VariableValue( const char *name ) { (void)name; return 0; }
int Hunk_MemoryRemaining( void ) { return 8 * 1024 * 1024; }

void *Hunk_Alloc( int size, ha_pref preference ) {
	void *result;
	(void)preference;
	Check( size > 0 && size <= 1024 * 1024, "unbounded allocation" );
	Check( allocationCount < MAX_ALLOCS, "too many allocations" );
	result = calloc( 1, size );
	Check( result != NULL, "host allocation failed" );
	allocations[allocationCount++] = result;
	return result;
}

int FS_ReadFile( const char *name, void **buffer ) {
	(void)name;
	fileReads++;
	*buffer = malloc( fixtureLength > 0 ? fixtureLength : 1 );
	Check( *buffer != NULL, "file allocation failed" );
	memcpy( *buffer, fixture, fixtureLength );
	return fixtureLength;
}

void FS_FreeFile( void *buffer ) {
	fileFrees++;
	free( buffer );
}

void VM_PrepareInterpreter( vm_t *vm, vmHeader_t *header ) {
	(void)vm;
	Check( header->instructionCount > 0, "invalid header reached preparation" );
	preparations++;
}

int VM_CallInterpreted( vm_t *vm, int *args ) {
	Check( vm == shutdownVM && args[0] == GAME_SHUTDOWN,
	       "unexpected shutdown dispatch" );
	Check( vm->dataBase && vm->dataBase[0] == 0x5a && vm->systemCall,
	       "shutdown callback received invalid VM state" );
	shutdownCalls++;
	return 0;
}

int VM_CallCompiled( vm_t *vm, int *args ) {
	(void)vm; (void)args;
	Check( 0, "unexpected compiled VM call" );
	return 0;
}

void VM_Compile( vm_t *vm, vmHeader_t *header ) {
	(void)vm;
	(void)header;
	Check( 0, "unexpected compiled VM" );
}

void * QDECL Sys_LoadDll( const char *name, char *path,
                         int (QDECL **entry)(int, ...),
                         int (QDECL *syscall)(int, ...) ) {
	(void)name; (void)path; (void)entry; (void)syscall;
	return NULL;
}
void Sys_UnloadDll( void *handle ) { (void)handle; }
static int SystemCall( int *args ) { (void)args; return 0; }

static void Reset( void ) {
	int i;
	VM_Clear();
	for ( i = 0; i < allocationCount; i++ ) {
		free( allocations[i] );
	}
	allocationCount = fileReads = fileFrees = preparations = 0;
	expectError = shutdownCalls = 0;
	shutdownVM = NULL;
}

static void PutWord( size_t offset, int value ) {
	unsigned int word = (unsigned int)value;
	int i;
	for ( i = 0; i < 4; i++ ) {
		fixture[offset + i] = (byte)(word >> (8 * i));
	}
}

#define SET_FIELD(field, value) PutWord( offsetof(vmHeader_t, field), value )

static void ValidFixture( void ) {
	memset( fixture, 0, sizeof(fixture) );
	fixtureLength = sizeof(fixture);
	SET_FIELD( vmMagic, VM_MAGIC );
	SET_FIELD( instructionCount, 8 );
	SET_FIELD( codeOffset, 32 );
	SET_FIELD( codeLength, 8 );
	SET_FIELD( dataOffset, 40 );
	SET_FIELD( dataLength, 4 );
	SET_FIELD( litLength, 3 );
	SET_FIELD( bssLength, 65536 );
	memset( fixture + 32, OP_IGNORE, 8 );
	PutWord( 40, 0x12345678 );
	memcpy( fixture + 44, "abc", 3 );
}

static vm_t *CreateValid( void ) {
	vm_t *vm;
	ValidFixture();
	vm = VM_Create( "test", SystemCall, VMI_BYTECODE );
	Check( vm != NULL, "valid image rejected" );
	Check( vm->dataMask == 131071, "data allocation rounding" );
	Check( *(int *)vm->dataBase == 0x12345678, "initialized word" );
	Check( !memcmp( vm->dataBase + 4, "abc", 3 ), "literal bytes" );
	Check( vm->dataBase[7] == 0 && vm->dataBase[vm->dataMask] == 0,
	       "zero-filled data image" );
	Check( fileReads == fileFrees && preparations == 1, "valid load ownership" );
	return vm;
}

static void RejectFixture( int restart ) {
	byte invalid[FIXTURE_SIZE];
	int invalidLength = fixtureLength;
	vm_t *vm = NULL;
	byte *oldData = NULL;
	int oldSize = 0, oldAllocations, i;

	memcpy( invalid, fixture, sizeof(invalid) );
	Reset();
	if ( restart ) {
		vm = CreateValid();
		oldData = vm->dataBase;
		oldSize = vm->dataMask + 1;
		memset( oldData, 0x5a, oldSize );
		shutdownVM = vm;
		memcpy( &savedVM, vm, sizeof(savedVM) );
	}
	memcpy( fixture, invalid, sizeof(fixture) );
	fixtureLength = invalidLength;
	oldAllocations = allocationCount;
	expectError = 1;
	if ( setjmp( errorJump ) == 0 ) {
		if ( restart ) {
			VM_Restart( vm );
		} else {
			VM_Create( "test", SystemCall, VMI_BYTECODE );
		}
		Check( 0, "malformed image accepted" );
	}
	expectError = 0;
	Check( errorLevel == ERR_DROP, "malformed image must use recoverable error" );
	Check( fileReads == fileFrees, "file buffer leaked on rejection" );
	Check( allocationCount == oldAllocations, "allocated before validation" );
	Check( vmTable[0].name[0] == '\0', "failed VM registration retained" );
	Check( shutdownCalls == restart, "restart must permit normal shutdown" );
	for ( i = 0; i < oldSize; i++ ) {
		Check( oldData[i] == 0x5a, "restart mutated data before validation" );
	}
}

static void TestTruncations( void ) {
	int length, restart;
	for ( restart = 0; restart <= 1; restart++ ) {
		for ( length = 0; length < FIXTURE_SIZE; length++ ) {
			ValidFixture();
			fixtureLength = length;
			RejectFixture( restart );
		}
	}
}

static void TestHeaderFields( void ) {
	struct { size_t offset; int value; } cases[] = {
#define BAD(field, value) { offsetof(vmHeader_t, field), value }
		BAD(vmMagic, 0),
		BAD(instructionCount, -1), BAD(instructionCount, 0),
		BAD(instructionCount, 9), BAD(instructionCount, INT_MAX),
		BAD(codeOffset, -1), BAD(codeOffset, 0), BAD(codeOffset, 31),
		BAD(codeOffset, FIXTURE_SIZE), BAD(codeOffset, INT_MAX),
		BAD(codeLength, -1), BAD(codeLength, 0), BAD(codeLength, INT_MAX),
		BAD(dataOffset, -1), BAD(dataOffset, 32), BAD(dataOffset, 39),
		BAD(dataOffset, FIXTURE_SIZE + 1), BAD(dataOffset, INT_MAX),
		BAD(dataLength, -1), BAD(dataLength, 1), BAD(dataLength, 3),
		BAD(dataLength, 8), BAD(dataLength, INT_MAX - 3),
		BAD(litLength, -1), BAD(litLength, 4), BAD(litLength, INT_MAX),
		BAD(bssLength, -1), BAD(bssLength, INT_MAX),
		BAD(bssLength, (INT_MAX / 2 + 1) - 6)
#undef BAD
	};
	int restart;
	size_t i;
	for ( restart = 0; restart <= 1; restart++ ) {
		for ( i = 0; i < sizeof(cases)/sizeof(cases[0]); i++ ) {
			ValidFixture();
			PutWord( cases[i].offset, cases[i].value );
			RejectFixture( restart );
		}
		ValidFixture();
		SET_FIELD( dataLength, 0 );
		SET_FIELD( litLength, 0 );
		SET_FIELD( bssLength, 0 );
		RejectFixture( restart );
	}
}

static void TestRestart( void ) {
	vm_t *vm;
	byte *data;
	int before;

	Reset();
	vm = CreateValid();
	data = vm->dataBase;
	before = allocationCount;
	memset( data, 0x5a, vm->dataMask + 1 );
	ValidFixture();
	PutWord( 40, 0x76543210 );
	Check( VM_Restart( vm ) == vm && vm->dataBase == data,
	       "restart must reuse allocation" );
	Check( allocationCount == before && preparations == 1,
	       "restart must not reallocate or prepare code" );
	Check( *(int *)data == 0x76543210 && data[7] == 0 &&
	       data[vm->dataMask] == 0, "restart initializes and clears data" );
	Check( fileReads == fileFrees, "restart file ownership" );

	ValidFixture();
	SET_FIELD( bssLength, 131072 );
	RejectFixture( 1 );
	ValidFixture();
	SET_FIELD( bssLength, 32768 );
	RejectFixture( 1 );
}

int main( void ) {
	Reset();
	CreateValid();
	TestTruncations();
	TestHeaderFields();
	TestRestart();
	Reset();
	puts( "QVM loading regressions passed (issue #35)" );
	return 0;
}
