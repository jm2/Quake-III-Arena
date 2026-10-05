/* Issue #35: a QVM's syscalls must not take the interpreter outside its
 * sandbox by re-entering or freeing the running VM.
 *
 * Since #13 (PR #490) a mod's or a server's QVM runs in the interpreter, and
 * its syscalls can run console commands immediately: G_SEND_CONSOLE_COMMAND
 * and UI_CMD_EXECUTETEXT with EXEC_NOW reach Cmd_ExecuteString, which passes
 * unknown commands to the game, cgame and UI modules.
 *
 * nesting  A QVM whose console command executes itself re-enters the VM
 *          until the VM program stack runs out, about 1700 levels and over
 *          2 MB of native stack on x86-64; the Mac build's application stack
 *          is 1 MB (mac_resources.r). The test runs on a 1 MB thread, where
 *          the unbounded recursion overflows the native stack.
 * freed    An immediate "map" or "vid_restart" frees the running VM and
 *          clears the hunk holding its code and data (SV_SpawnServer,
 *          CL_Vid_Restart_f). The interpreter then kept executing from freed
 *          memory. ioquake3 refuses VM_Free on a running VM; here the
 *          interpreter notices on the syscall's return and drops.
 *
 * The QVMs go through the real VM_Create, VM_Call and VM_Free; hunk blocks
 * are separate heap allocations, so ASan reports any use after the
 * simulated Hunk_Clear.
 */
#include "../code/qcommon/vm_local.h"
#include <pthread.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ALLOCS 16
#define NESTING_LIMIT 32	// MAX_VM_NESTING in vm_interpreted.c

static cvar_t developer;
cvar_t *com_developer = &developer;
static byte qvm[256];
static int qvmLength;
static void *allocs[MAX_ALLOCS];
static int allocCount;
static jmp_buf errorJump;
static int expectError;
static char lastError[128];
static vm_t *testVM;
static int syscalls, depth, maxDepth, reenterLimit, freeOnSyscall, recreate;
static vm_t *replacement;

static void Check( int ok, const char *message ) {
	if ( !ok ) {
		fprintf( stderr, "QVM sandbox regression failed: %s (%s)\n", message, lastError );
		exit( 1 );
	}
}

void QDECL Com_Error( int level, const char *format, ... ) {
	va_list ap;
	va_start( ap, format );
	vsnprintf( lastError, sizeof(lastError), format, ap );
	va_end( ap );
	Check( expectError && level == ERR_DROP, "unexpected engine error" );
	longjmp( errorJump, 1 );
}
void QDECL Com_Printf( const char *format, ... ) { (void)format; }
void Com_Memset( void *dest, const int value, const size_t count ) { memset( dest, value, count ); }
void Com_Memcpy( void *dest, const void *src, const size_t count ) { memcpy( dest, src, count ); }
float Cvar_VariableValue( const char *name ) { (void)name; return 0; }
int Hunk_MemoryRemaining( void ) { return 0; }
void *Z_Malloc( int size ) { return calloc( 1, size ); }
void Z_Free( void *pointer ) { free( pointer ); }

void *Hunk_Alloc( int size, ha_pref preference ) {
	(void)preference;
	Check( size > 0 && allocCount < MAX_ALLOCS, "hunk allocation" );
	allocs[allocCount] = calloc( 1, size );
	Check( allocs[allocCount] != NULL, "host allocation" );
	return allocs[allocCount++];
}

/* Hunk_Clear: the freed VM's code, data and instruction table go. */
static void HunkClear( void ) {
	while ( allocCount ) {
		free( allocs[--allocCount] );
	}
}

int FS_ReadFile( const char *name, void **buffer ) {
	(void)name;
	*buffer = malloc( qvmLength );
	Check( *buffer != NULL, "file allocation" );
	memcpy( *buffer, qvm, qvmLength );
	return qvmLength;
}
void FS_FreeFile( void *buffer ) { free( buffer ); }
void * QDECL Sys_LoadDll( const char *name, char *path, int (QDECL **entry)(int, ...),
                         int (QDECL *syscall)(int, ...) ) {
	(void)name; (void)path; (void)entry; (void)syscall;
	return NULL;
}
void Sys_UnloadDll( void *handle ) { (void)handle; }
int VM_CallCompiled( vm_t *vm, int *args ) { (void)vm; (void)args; Check( 0, "compiled call" ); return 0; }
void VM_Compile( vm_t *vm, vmHeader_t *header ) { (void)vm; (void)header; Check( 0, "compile" ); }

static int SystemCall( int *args ) {
	int result = 0;
	Check( args[0] == 0, "syscall number" );
	syscalls++;
	if ( freeOnSyscall && syscalls == 1 ) {
		// The command frees the running module and the hunk, then loads
		// and runs a new one in the same vm_t slot.
		VM_Free( testVM );
		HunkClear();
		if ( recreate ) {
			replacement = VM_Create( "replacement", SystemCall, VMI_BYTECODE );
			Check( replacement == testVM, "replacement reuses the slot" );
			Check( VM_Call( replacement, 7 ) == 0, "replacement runs" );
		}
		return 0;
	}
	if ( depth < reenterLimit ) {
		// EXEC_NOW of a command the module itself handles: re-entry.
		depth++;
		if ( depth > maxDepth ) {
			maxDepth = depth;
		}
		result = VM_Call( testVM, 2 );
		depth--;
	}
	return result;
}

static void Put( int offset, int value ) {
	int i;
	for ( i = 0; i < 4; i++ ) {
		qvm[offset + i] = (byte)((unsigned int)value >> (8 * i));
	}
}

/* vmMain: ENTER 16; CONST -1; CALL (syscall 0); CONST 0; LOAD4; ADD;
 * LEAVE 16: returns the syscall's result plus data word 0, which is 0. */
static void BuildQVM( void ) {
	int pc = 32, dataLength = 4, bssLength = 0x30000;
	qvm[pc++] = OP_ENTER; Put( pc, 16 ); pc += 4;
	qvm[pc++] = OP_CONST; Put( pc, -1 ); pc += 4;
	qvm[pc++] = OP_CALL;
	qvm[pc++] = OP_CONST; Put( pc, 0 ); pc += 4;
	qvm[pc++] = OP_LOAD4;
	qvm[pc++] = OP_ADD;
	qvm[pc++] = OP_LEAVE; Put( pc, 16 ); pc += 4;
	Put( 0, VM_MAGIC );
	Put( 4, 7 );
	Put( 8, 32 );
	Put( 12, pc - 32 );
	Put( 16, pc );
	Put( 20, dataLength );
	Put( 24, 0 );
	Put( 28, bssLength );
	Put( pc, 0 );
	qvmLength = pc + dataLength;
}

static void Reset( void ) {
	VM_Clear();
	HunkClear();
	syscalls = depth = maxDepth = reenterLimit = freeOnSyscall = recreate = 0;
	expectError = 0;
	lastError[0] = '\0';
	replacement = NULL;
	testVM = VM_Create( "sandbox", SystemCall, VMI_BYTECODE );
	Check( testVM && !testVM->dllHandle && !testVM->compiled, "interpreted QVM" );
}

static void TestNesting( void ) {
	int call;

	// Retail-style nesting below the limit returns normally, and the level
	// unwinds: repeated calls do not accumulate toward the limit.
	Reset();
	reenterLimit = NESTING_LIMIT - 1;
	for ( call = 0; call < 100; call++ ) {
		Check( VM_Call( testVM, 0 ) == 0, "nested result" );
		Check( maxDepth == NESTING_LIMIT - 1 && depth == 0, "nesting depth below the limit" );
		Check( !testVM->interpretFaulted && !testVM->currentlyInterpreting, "nesting unwound" );
	}

	// Unbounded self re-entry stops at the limit with a controlled drop,
	// long before the native stack is exhausted.
	Reset();
	reenterLimit = 1 << 30;
	expectError = 1;
	if ( setjmp( errorJump ) == 0 ) {
		VM_Call( testVM, 0 );
		Check( 0, "unbounded re-entry returned" );
	}
	Check( strstr( lastError, "nested too deeply" ) != NULL, "nesting rejection" );
	Check( maxDepth == NESTING_LIMIT, "rejected at the nesting limit" );
	Check( testVM->interpretFaulted, "nesting fault marks the VM" );
	depth = 0;
}

static void TestFreed( qboolean withReplacement ) {
	Reset();
	freeOnSyscall = 1;
	recreate = withReplacement;
	expectError = 1;
	if ( setjmp( errorJump ) == 0 ) {
		VM_Call( testVM, 0 );
		Check( 0, "freed VM kept running" );
	}
	Check( strstr( lastError, "freed by its own syscall" ) != NULL, "freed VM rejection" );
	if ( withReplacement ) {
		// The old activation must leave the new module's state alone.
		Check( replacement->name[0] && !replacement->interpretFaulted &&
		       !replacement->currentlyInterpreting &&
		       replacement->programStack == replacement->dataMask + 1, "replacement state" );
		expectError = 0;
		Check( VM_Call( replacement, 9 ) == 0, "replacement still runs" );
	}
}

static void *RunTests( void *unused ) {
	(void)unused;
	BuildQVM();
	VM_Init();
	TestNesting();
	TestFreed( qfalse );
	TestFreed( qtrue );
	Reset();
	Check( VM_Call( testVM, 0 ) == 0 && syscalls == 1, "plain call" );
	VM_Clear();
	HunkClear();
	return NULL;
}

void Cmd_AddCommand( const char *name, xcommand_t function ) { (void)name; (void)function; }
cvar_t *Cvar_Get( const char *name, const char *value, int flags ) {
	(void)name; (void)value; (void)flags;
	return &developer;
}

int main( void ) {
	pthread_t thread;
	pthread_attr_t attr;
	// The Mac application stack (mac_resources.r appStackSize).
	Check( pthread_attr_init( &attr ) == 0 &&
	       pthread_attr_setstacksize( &attr, 1024 * 1024 ) == 0 &&
	       pthread_create( &thread, &attr, RunTests, NULL ) == 0 &&
	       pthread_join( thread, NULL ) == 0, "test thread" );
	printf( "QVM sandbox regression passed\n" );
	return 0;
}
