/* Issue #319: drive a module console-command trap into the real command buffer (cmd.c). */
#ifndef Q3_CONSOLE_COMMAND_TRAP_HARNESS_H
#define Q3_CONSOLE_COMMAND_TRAP_HARNESS_H
#include "../code/qcommon/vm_local.h"
#include <limits.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#define IMAGE_SIZE 4096
#define COUNT(array) ((int)(sizeof(array) / sizeof((array)[0])))
cvar_t *com_cl_running;
cvar_t *com_sv_running;
static vm_t vm;
static byte before[IMAGE_SIZE];
static int expectDrop;
static jmp_buf dropJump;
static char executed[MAX_STRING_CHARS];
int QDECL VM_DllSyscall( int arg, ... );	/* vm.c: the syscall pointer native modules receive */

/** Fail when the trap, the command buffer, or the module fault state differs. */
static void Check( int ok, const char *message ) {
	if ( !ok ) { fprintf( stderr, "Console command trap regression failed: %s\n", message ); exit( 1 ); }
}
/** Fail when the trap reaches an engine service this fixture never exercises. */
static void Unexpected( const char *name ) {
	fprintf( stderr, "Console command trap regression reached %s\n", name ); exit( 1 );
}
/** Accept only the module drop; the retail ERR_FATAL for a bad exec_when fails here. */
void QDECL Com_Error( int level, const char *format, ... ) {
	char message[MAX_STRING_CHARS];
	va_list ap;
	va_start( ap, format );
	vsnprintf( message, sizeof(message), format, ap );
	va_end( ap );
	if ( !expectDrop || level != ERR_DROP ) {
		fprintf( stderr, "Console command trap regression failed: unexpected engine error %d: %s\n",
		         level, message );
		exit( 1 );
	}
	Check( vm.entryPoint || ( vm.interpretFaulted && !vm.currentlyInterpreting ), "QVM fault state" );
	Check( !memcmp( before, vm.dataBase, IMAGE_SIZE ), "rejection changed VM data" );
	longjmp( dropJump, 1 );
}
void QDECL Com_Printf( const char *fmt, ... ) { (void)fmt; }
void QDECL Com_DPrintf( const char *fmt, ... ) { (void)fmt; }
void Com_Memcpy( void *dest, const void *src, const size_t count ) { memcpy( dest, src, count ); }
void Com_Memset( void *dest, const int val, const size_t count ) { memset( dest, val, count ); }
/** No commands are registered, so every line the real buffer runs lands here, in order. */
qboolean Cvar_Command( void ) {
	Q_strcat( executed, sizeof(executed), Cmd_Argv( 0 ) );
	Q_strcat( executed, sizeof(executed), ";" );
	return qtrue;
}
void CL_ForwardCommandToServer( const char *string ) { Unexpected( __func__ ); }
qboolean CL_GameCommand( void ) { Unexpected( __func__ ); return qfalse; }
int VM_CallCompiled( vm_t *target, int *args ) { Unexpected( __func__ ); return 0; }
int VM_CallInterpreted( vm_t *target, int *args ) { Unexpected( __func__ ); return 0; }

/** Mark the module as native, as the Mac OS 9 static modules are. */
static int QDECL NativeEntry( int command, ... ) { (void)command; return 0; }
/** True if native text pointers into page fit the int syscall ABI. */
static int Below4GiB( const byte *page ) {
	return (unsigned long)page == (unsigned int)(unsigned long)page;
}
/** Map the image page below 4 GiB, where native text pointers fit the int syscall ABI. */
static byte *LowPage( void ) {
	byte *page;
#ifdef MAP_32BIT
	page = mmap( NULL, IMAGE_SIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_32BIT, -1, 0 );
	Check( page != MAP_FAILED && Below4GiB( page ), "low native page" );
#else
	/* only a hint: a 64-bit host without MAP_32BIT may place the page above 4 GiB,
	   where only the QVM pass can use it */
	page = mmap( (void *)0x10000000, IMAGE_SIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0 );
	Check( page != MAP_FAILED, "image page" );
#endif
	return page;
}
/** Enter the real dispatcher the way each module kind does: QVM args directly, native via VM_DllSyscall. */
static int Send( int trap, int execWhen, int textOffset ) {
	int args[3];
	vm.interpretFaulted = qfalse;
	vm.currentlyInterpreting = !vm.entryPoint;
	memcpy( before, vm.dataBase, IMAGE_SIZE );
	if ( vm.entryPoint ) {
		return VM_DllSyscall( trap, execWhen, (int)(unsigned long)(vm.dataBase + textOffset),
		                      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 );
	}
	args[0] = trap; args[1] = execWhen; args[2] = textOffset;
	return vm.systemCall( args );
}
/** A retail exec_when reaches Cbuf_ExecuteText and leaves the module running. */
static void Accept( int trap, int execWhen, int textOffset ) {
	Check( Send( trap, execWhen, textOffset ) == 0, "valid trap result" );
	Check( !vm.interpretFaulted, "valid exec_when faulted the module" );
}
/** An out-of-range exec_when drops the module before its text is run or queued. */
static void Reject( int trap, int execWhen, int textOffset ) {
	char ran[sizeof(executed)];
	Q_strncpyz( ran, executed, sizeof(ran) );
	expectDrop = 1;
	if ( setjmp( dropJump ) == 0 ) {
		Send( trap, execWhen, textOffset );
		Check( 0, "out-of-range exec_when accepted" );
	}
	expectDrop = 0;
	Check( !strcmp( ran, executed ), "rejected command ran" );
}
/** Check retail EXEC_NOW/INSERT/APPEND and out-of-range values for one module, as a QVM and natively. */
static void RunConsoleCommandTrap( int (*dispatch)( int * ), int trap, const char *module ) {
	static const int bad[] = {-1, EXEC_APPEND + 1, INT_MAX, INT_MIN};
	char *text;
	int native, i;
	vm.dataBase = LowPage();
	vm.dataMask = IMAGE_SIZE - 1;
	vm.systemCall = dispatch;
	currentVM = &vm;
	text = (char *)vm.dataBase;
	strcpy( text + 16, "alpha\n" );
	strcpy( text + 32, "bravo\n" );
	strcpy( text + 48, "charlie" );
	strcpy( text + 64, "pending\n" );
	strcpy( text + 80, "rejected\n" );
	/* text + 96 stays "": EXEC_NOW with no text runs the buffer. */
	Cbuf_Init();
	for ( native = 0; native < 2; native++ ) {
		if ( native && !Below4GiB( vm.dataBase ) ) {
			printf( "%s console-command trap: SKIPPED the native pass, no page below 4 GiB on this host\n", module );
			break;
		}
		vm.entryPoint = native ? NativeEntry : NULL;
		executed[0] = 0;
		Accept( trap, EXEC_APPEND, 32 );
		Check( !executed[0], "EXEC_APPEND ran before the buffer" );
		Accept( trap, EXEC_INSERT, 16 );
		Check( !executed[0], "EXEC_INSERT ran before the buffer" );
		Accept( trap, EXEC_NOW, 48 );
		Check( !strcmp( executed, "charlie;" ), "EXEC_NOW did not run at once" );
		Accept( trap, EXEC_NOW, 96 );
		Check( !strcmp( executed, "charlie;alpha;bravo;" ), "buffer order" );
		executed[0] = 0;
		Accept( trap, EXEC_APPEND, 64 );
		for ( i = 0; i < COUNT(bad); i++ ) {
			Reject( trap, bad[i], 80 );
		}
		Accept( trap, EXEC_NOW, 96 );
		Check( !strcmp( executed, "pending;" ), "rejected text reached the buffer" );
	}
	munmap( vm.dataBase, IMAGE_SIZE );
	printf( "%s console-command trap regressions passed (issue #319)\n", module );
}
#endif
