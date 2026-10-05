/* Issue #35: fuzz the interpreted QVM sandbox under ASan/UBSan.
 *
 * Random and mutated QVM images go through the real VM_Create (interpreted),
 * VM_Call and VM_Restart. Every host memory error or undefined behavior
 * stops the run; a hang outside VM execution aborts. QVM-level outcomes (a
 * controlled ERR_DROP, or an infinite QVM loop cut off by the per-call timer)
 * are counted. The main loop runs on a 1 MB thread, the Mac's stack size.
 *
 * usage: vm_sandbox_fuzz <mode> <firstSeed> <count> [qvm files...]
 *   header           small images with hostile header fields
 *   program          structured bytecode with plausible frames and operands
 *   program-reenter  ...whose syscalls re-enter the VM without a bound
 *   program-free     ...whose syscalls may free the VM and clear the hunk,
 *                    as an EXEC_NOW "map" or "vid_restart" does
 *   mutate           corrupted copies of the given QVMs
 *   retail           the given retail cgame/qagame/ui QVMs' init entry
 *                    points and a few frames, with VM_FUZZ_PAKDIR naming
 *                    the extracted pk3; every module must run cleanly
 * VM_FUZZ_VERBOSE=1 prints a digest line per case (result, syscall trace
 * hash, error) for differential comparison between interpreter builds.
 */
#include "../code/qcommon/vm_local.h"
#include <limits.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <math.h>
#include <pthread.h>

#define MAX_ALLOCS 64
#define HUNK_LIMIT (24 * 1024 * 1024)

static cvar_t developer;
cvar_t *com_developer = &developer;
static void *allocs[MAX_ALLOCS];
static int allocCount, hunkUsed;
static byte *file;
static int fileLength;
static sigjmp_buf errorJump;
static volatile int phase;	// 0 idle, 1 load, 2 call
static char lastError[256];
static unsigned int traceHash, syscallCount;
static unsigned int rng;
static int nestDepth, syscallMode, deepNesting, freeDuringCall;
static vm_t *liveVM;
static int verbose;
static int retailModule = -1;
static int RetailSyscall( int *args );

static unsigned int Rand( void ) {
	rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
	return rng;
}

static void Mix( unsigned int v ) {
	traceHash = (traceHash ^ v) * 16777619u;
}

static void Arm( int ms );
void QDECL Com_Error( int level, const char *format, ... ) {
	va_list ap;
	va_start( ap, format );
	vsnprintf( lastError, sizeof(lastError), format, ap );
	va_end( ap );
	if ( level != ERR_DROP ) {
		fprintf( stderr, "FINDING: non-drop error %d: %s\n", level, lastError );
		abort();
	}
	// Model Com_Error's module shutdown before the longjmp: a faulted QVM
	// must not run again, a healthy one may run its shutdown entry.
	if ( liveVM && liveVM->name[0] && nestDepth == 0 ) {
		vm_t *vm = liveVM;
		liveVM = NULL;
		phase = 2;
		Arm( 200 );
		VM_Call( vm, 1, 0 );
	}
	siglongjmp( errorJump, 1 );
}
void QDECL Com_Printf( const char *format, ... ) { (void)format; }
void Com_Memset( void *dest, const int value, const size_t count ) { memset( dest, value, count ); }
void Com_Memcpy( void *dest, const void *src, const size_t count ) { memcpy( dest, src, count ); }
float Cvar_VariableValue( const char *name ) { (void)name; return 0; }
cvar_t *Cvar_Get( const char *a, const char *b, int c ) { (void)a; (void)b; (void)c; return &developer; }
void Cmd_AddCommand( const char *a, xcommand_t b ) { (void)a; (void)b; }
void *Z_Malloc( int size ) { return calloc( 1, size ); }
void Z_Free( void *p ) { free( p ); }
int Hunk_MemoryRemaining( void ) { return HUNK_LIMIT - hunkUsed; }
void *Hunk_Alloc( int size, ha_pref preference ) {
	void *p;
	(void)preference;
	if ( size <= 0 || size > HUNK_LIMIT - hunkUsed || allocCount == MAX_ALLOCS ) {
		Com_Error( ERR_DROP, "Hunk_Alloc failed on %i", size );
	}
	p = calloc( 1, size );
	if ( !p ) { abort(); }
	allocs[allocCount++] = p;
	hunkUsed += size;
	return p;
}

int FS_ReadFile( const char *name, void **buffer ) {
	(void)name;
	*buffer = malloc( fileLength ? fileLength : 1 );
	memcpy( *buffer, file, fileLength );
	return fileLength;
}
void FS_FreeFile( void *buffer ) { free( buffer ); }
void * QDECL Sys_LoadDll( const char *name, char *path, int (QDECL **entry)(int, ...),
                         int (QDECL *syscall)(int, ...) ) {
	(void)name; (void)path; (void)entry; (void)syscall; return NULL;
}
void Sys_UnloadDll( void *h ) { (void)h; }
int VM_CallCompiled( vm_t *vm, int *args ) { (void)vm; (void)args; abort(); }
void VM_Compile( vm_t *vm, vmHeader_t *h ) { (void)vm; (void)h; abort(); }


/* Just enough engine for the retail init paths: files from the extracted
 * pak, a game state, a GL config and an entity string. */
static FILE *handles[16];
static int entityToken;
static int RetailSyscall( int *args ) {
	static const int fopen_[3] = { 10, 10, 13 }, fread_[3] = { 11, 11, 14 },
		fclose_[3] = { 13, 13, 16 }, glconfig[3] = { 49, -1, 43 };
	static const char *entities[] = { "{", "classname", "worldspawn", "}", "{",
		"classname", "info_player_deathmatch", "origin", "0 0 0", "}" };
	int n = args[0], m = retailModule, i;
	if ( n == fopen_[m] ) {
		char path[512];
		const char *dir = getenv( "VM_FUZZ_PAKDIR" );
		int *handle = VM_CheckedArgPtr( args[2], 4, 4, qtrue ), h, length;
		FILE *f;
		snprintf( path, sizeof(path), "%s/%s", dir ? dir : ".", VM_CheckedArgString( args[1], qfalse ) );
		if ( handle ) *handle = 0;
		if ( args[3] != 0 || !handle || !(f = fopen( path, "rb" )) ) return -1;
		for ( h = 1; h < 16 && handles[h]; h++ ) {}
		if ( h == 16 ) { fclose( f ); return -1; }
		handles[h] = f;
		*handle = h;
		fseek( f, 0, SEEK_END ); length = ftell( f ); fseek( f, 0, SEEK_SET );
		return length;
	}
	if ( n == fread_[m] ) {
		byte *b = VM_CheckedArgPtr( args[1], args[2], 1, qfalse );
		if ( args[3] > 0 && args[3] < 16 && handles[args[3]] ) fread( b, 1, args[2], handles[args[3]] );
		return 0;
	}
	if ( n == fclose_[m] ) {
		if ( args[1] > 0 && args[1] < 16 && handles[args[1]] ) { fclose( handles[args[1]] ); handles[args[1]] = NULL; }
		return 0;
	}
	if ( n == glconfig[m] ) {
		int *g = VM_CheckedArgPtr( args[1], 11332, 4, qfalse );
		memset( g, 0, 11332 );
		/* vidWidth follows three 1024-byte strings, the 8192-byte
		 * extension string and ten ints. */
		g[11304 / 4] = 640;
		g[11308 / 4] = 480;
		((float *)g)[11312 / 4] = 640.0f / 480.0f;
		return 0;
	}
	if ( m == 0 && n == 50 ) {
		/* gameState_t: CS_SERVERINFO and CS_GAME_VERSION. */
		byte *g = VM_CheckedArgPtr( args[1], 20100, 4, qfalse );
		static const char info[] = "\\mapname\\q3dm1\\g_gametype\\0\\sv_maxclients\\8";
		memset( g, 0, 20100 );
		memcpy( g + 4096 + 1, info, sizeof(info) );
		*(int *)g = 1;
		memcpy( g + 4096 + 1 + sizeof(info), "baseq3-1", 9 );
		for ( i = 1; i < 64; i++ ) {
			((int *)g)[i] = 1 + sizeof(info);	// the demo's CS_GAME_VERSION index differs
		}
		((int *)g)[20099 / 4] = 1 + sizeof(info) + 9;
		return 0;
	}
	if ( m == 1 && n == 37 ) {
		char *b = VM_CheckedStringBuffer( args[1], args[2], qfalse );
		if ( entityToken >= (int)(sizeof(entities) / sizeof(entities[0])) ) return 0;
		Q_strncpyz( b, entities[entityToken++], args[2] );
		return 1;
	}
	return 0;
}

/* Syscalls hash their arguments. Random programs' syscalls use the checked
 * accessors the dispatchers use, so their bounds see attacker-chosen values,
 * and re-enter or free the VM as immediate console commands can. */
static int SystemCall( int *args ) {
	int i, n = args[0];
	char *s;
	syscallCount++;
	for ( i = 0; i < 16; i++ ) {
		Mix( (unsigned int)args[i] );
	}
	if ( syscallCount > 200000 ) {
		Com_Error( ERR_DROP, "fuzz syscall budget" );
	}
	if ( syscallMode == 2 ) {
		/* Retail QVMs: the shared memory/math traps (100-106 in all three
		 * modules) through the engine's checked helpers; others return 0. */
		float f;
		switch ( n ) {
		case 100: VM_MemoryFill( args[1], args[2], args[3] ); return args[1];
		case 101: VM_MemoryCopy( args[1], args[2], args[3] ); return args[1];
		case 102: return VM_StringCopy( args[1], args[2], args[3] );
		case 103: f = sin( ((float *)args)[1] ); return *(int *)&f;
		case 104: f = cos( ((float *)args)[1] ); return *(int *)&f;
		case 105: f = atan2( ((float *)args)[1], ((float *)args)[2] ); return *(int *)&f;
		case 106: f = sqrt( ((float *)args)[1] ); return *(int *)&f;
		default:
			return RetailSyscall( args );
		case 0: case 1:
			VM_CheckedArgString( args[1], qfalse );
			if ( n == (retailModule == 2 ? 0 : 1) ) {
				Com_Error( ERR_DROP, "QVM error" );
			}
			return 0;
		}
	}
	if ( !syscallMode ) {
		return 0;
	}
	switch ( deepNesting ? 5 : freeDuringCall ? 7 : n & 7 ) {
	case 1:
		s = VM_CheckedArgString( args[1], qtrue );
		if ( s ) { Mix( strlen( s ) ); }
		return 0;
	case 2: {
		byte *p = VM_CheckedArgPtr( args[1], args[2] & 0xffff, 4, qtrue );
		if ( p ) { memset( p, 0x5a, args[2] & 0xffff ); }
		return 0;
	}
	case 3:
		VM_MemoryCopy( args[1], args[2], args[3] & 0xffff );
		return 0;
	case 4:
		return VM_StringCopy( args[1], args[2], args[3] & 0xffff );
	case 5:
		if ( nestDepth < (deepNesting ? 100000 : 3) ) {
			int r;
			nestDepth++;
			r = VM_Call( currentVM, args[1] & 15, args[2], args[3] );
			nestDepth--;
			return r;
		}
		return 0;
	case 6:
		VM_MemoryFill( args[1], args[2], args[3] & 0xffff );
		return 0;
	case 7:
		if ( freeDuringCall ) {
			/* An immediate "map"/"vid_restart": VM_Free, then Hunk_Clear. */
			int i;
			VM_Free( currentVM );
			for ( i = 0; i < allocCount; i++ ) {
				free( allocs[i] );
			}
			allocCount = hunkUsed = 0;
			liveVM = NULL;
		}
		return 0;
	default:
		return (int)Rand();
	}
}

static void OnAlarm( int sig ) {
	(void)sig;
	if ( phase != 2 ) {
		static const char msg[] = "FINDING: host hang outside VM execution\n";
		write( 2, msg, sizeof(msg) - 1 );
		abort();
	}
	snprintf( lastError, sizeof(lastError), "timeout" );
	siglongjmp( errorJump, 2 );
}

static void Arm( int ms ) {
	struct itimerval t;
	memset( &t, 0, sizeof(t) );
	t.it_value.tv_sec = ms / 1000;
	t.it_value.tv_usec = (ms % 1000) * 1000;
	setitimer( ITIMER_REAL, &t, NULL );
}

static void ResetAll( void ) {
	int i;
	Arm( 0 );
	VM_Clear();
	for ( i = 0; i < 16; i++ ) { if ( handles[i] ) { fclose( handles[i] ); handles[i] = NULL; } }
	entityToken = 0;
	for ( i = 0; i < allocCount; i++ ) {
		free( allocs[i] );
	}
	allocCount = hunkUsed = 0;
	liveVM = NULL;
	nestDepth = 0;
}

static int outcomes[4];
/* Load the current file and drive it. Returns the outcome class. */
static int RunCase( unsigned int seed, int calls, int timeoutMs, const int *firstCall ) {
	static vm_t *vm;	// static: survives siglongjmp
	static int stage, result;
	int c, outcome;
	traceHash = 2166136261u;
	syscallCount = 0;
	lastError[0] = 0;
	stage = 0;
	result = 0;
	vm = NULL;
	if ( (outcome = sigsetjmp( errorJump, 1 )) == 0 ) {
		phase = 1;
		Arm( 5000 );
		vm = VM_Create( "fuzz", SystemCall, VMI_BYTECODE );
		Arm( 0 );
		stage = 1;
		if ( !vm ) {
			snprintf( lastError, sizeof(lastError), "no vm" );
			outcome = 1;
		} else {
			liveVM = vm;
			for ( c = 0; c < calls; c++ ) {
				int a[MAX_VMMAIN_ARGS - 1], j, forced = -1;
				for ( j = 0; j < MAX_VMMAIN_ARGS - 1; j++ ) {
					a[j] = (Rand() & 3) ? (int)(Rand() & 0xff) : (int)Rand();
				}
				if ( c == 0 && firstCall ) {
					memcpy( a, firstCall + 1, sizeof(a) );
				}
				if ( c > 0 && retailModule >= 0 ) {
					/* cgame CG_CROSSHAIR_PLAYER and CG_LAST_ATTACKER (a
					 * frame needs snapshots), game GAME_RUN_FRAME, ui
					 * UI_SET_ACTIVE_MENU(main) then UI_REFRESH. */
					static const int frameCalls[3][2] = { { 4, 5 }, { 7, 7 }, { 6, 4 } };
					memset( a, 0, sizeof(a) );
					a[0] = retailModule == 2 && c == 1 ? 1 : 1000 + 50 * c;
					forced = frameCalls[retailModule][c == 1 ? 0 : 1];
				}
				phase = 2;
				Arm( timeoutMs );
				result = VM_CallArgs( vm, c == 0 && firstCall ? firstCall[0] : forced >= 0 ? forced : (int)(Rand() % 12),
				                      a, MAX_VMMAIN_ARGS - 1 );
				Arm( 0 );
				phase = 0;
				Mix( (unsigned int)result );
				stage = 2 + c;
				if ( c == 0 && (Rand() & 3) == 0 && retailModule < 0 ) {
					phase = 1;
					Arm( 5000 );
					vm = VM_Restart( vm );
					Arm( 0 );
					phase = 0;
					if ( !vm ) {
						fprintf( stderr, "FINDING: restart of an unchanged image failed\n" );
						abort();
					}
				}
			}
			outcome = 0;
		}
	} else {
		Arm( 0 );
	}
	phase = 0;
	outcomes[outcome < 4 ? outcome : 3]++;
	if ( verbose ) {
		printf( "%u stage=%d outcome=%d result=%d syscalls=%u trace=%08x err=%s\n",
		        seed, stage, outcome, result, syscallCount, traceHash, lastError );
	}
	ResetAll();
	return outcome;
}

static void Put( byte *p, int v ) {
	p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24;
}

static int Interesting( int length ) {
	static const int values[] = { 0, 1, 2, 3, 4, -1, -4, 31, 32, 33, 36, 64, 255, 256,
		0x7fff, 0x8000, 0xffff, 0x10000, 0x20000, 0x40000000, 0x3fffffff,
		INT_MAX, INT_MAX - 3, INT_MIN, INT_MIN + 4 };
	switch ( Rand() % 4 ) {
	case 0: return values[Rand() % (sizeof(values) / sizeof(values[0]))];
	case 1: return length + (int)(Rand() % 9) - 4;
	case 2: return (int)(Rand() % (length + 8));
	default: return (int)Rand();
	}
}

/* Mode header: tiny files with hostile header fields. */
static void GenHeader( void ) {
	static byte buffer[512];
	int i, codeLength, dataLength, litLength, field;
	fileLength = Rand() % 4 ? 32 + (int)(Rand() % (sizeof(buffer) - 32)) : (int)(Rand() % 40);
	for ( i = 0; i < fileLength; i++ ) {
		buffer[i] = Rand() % 3 ? (byte)(OP_IGNORE + Rand() % OP_CVFI) : (byte)Rand();
	}
	if ( fileLength >= 32 ) {
		// A consistent layout, then hostile values in a few fields.
		codeLength = Rand() % (fileLength - 32 + 1);
		dataLength = ((fileLength - 32 - codeLength) / 4) * 4;
		dataLength = dataLength ? (int)(Rand() % (dataLength / 4 + 1)) * 4 : 0;
		litLength = fileLength - 32 - codeLength - dataLength;
		Put( buffer, VM_MAGIC );
		Put( buffer + 4, 1 + (int)(Rand() % (codeLength / 2 + 1)) );
		Put( buffer + 8, 32 );
		Put( buffer + 12, codeLength );
		Put( buffer + 16, 32 + codeLength );
		Put( buffer + 20, dataLength );
		Put( buffer + 24, litLength );
		Put( buffer + 28, (int)(Rand() % 64) * 4 );
		for ( field = Rand() % 4; field > 0; field-- ) {
			Put( buffer + 4 * (Rand() % 8), Interesting( fileLength ) );
		}
	}
	file = buffer;
}

/* Mode program: structured bytecode with plausible frames and operands. */
static int OperandSize( int op ) {
	if ( op == OP_ARG ) return 1;
	if ( op == OP_ENTER || op == OP_LEAVE || op == OP_CONST || op == OP_LOCAL ||
	     op == OP_BLOCK_COPY || (op >= OP_EQ && op <= OP_GEF) ) return 4;
	return 0;
}

static int Needs( int op ) {
	switch ( op ) {
	case OP_IGNORE: case OP_BREAK: case OP_ENTER: case OP_CONST: case OP_LOCAL: case OP_PUSH:
		return 0;
	case OP_LEAVE: case OP_CALL: case OP_POP: case OP_JUMP: case OP_LOAD1: case OP_LOAD2:
	case OP_LOAD4: case OP_ARG: case OP_SEX8: case OP_SEX16: case OP_NEGI: case OP_BCOM:
	case OP_NEGF: case OP_CVIF: case OP_CVFI:
		return 1;
	default:
		return 2;
	}
}

static int Effect( int op ) {
	switch ( op ) {
	case OP_CONST: case OP_LOCAL: case OP_PUSH: return 1;
	case OP_IGNORE: case OP_BREAK: case OP_ENTER: case OP_LEAVE: case OP_CALL:
	case OP_LOAD1: case OP_LOAD2: case OP_LOAD4: case OP_SEX8: case OP_SEX16:
	case OP_NEGI: case OP_BCOM: case OP_NEGF: case OP_CVIF: case OP_CVFI: return 0;
	case OP_POP: case OP_JUMP: case OP_ARG: return -1;
	default: return Needs( op ) == 2 ? (op >= OP_EQ && op <= OP_GEF) || op == OP_BLOCK_COPY ||
	                                  (op >= OP_STORE1 && op <= OP_STORE4) ? -2 : -1 : 0;
	}
}

static void GenProgram( void ) {
	static byte buffer[64 * 1024];
	int count, i, pc = 32, data, lit, bss, image, frame, depth = 0, corrupt;
	static int ops[400], values[400];
	int target = 1 + Rand() % 150;
	frame = (Rand() % 8) * 4 + 8 + 16;
	data = (Rand() % 64) * 4;
	lit = Rand() % 64;
	bss = Rand() % 4 ? 0x20000 : (int)(Rand() % 0x400);
	image = data + lit + bss;
	corrupt = Rand() % 8 == 0;
	count = 0;
	ops[count] = OP_ENTER; values[count++] = frame;
	while ( count < target ) {
		int op = OP_IGNORE + Rand() % OP_CVFI, v;
		if ( op == OP_ENTER || op == OP_LEAVE ) {
			op = Rand() % 4 ? OP_CONST : op;
		}
		while ( depth < Needs( op ) && count < target ) {
			ops[count] = Rand() % 4 ? OP_CONST : OP_LOCAL;
			switch ( Rand() % 6 ) {
			case 0: v = (int)(Rand() % (target + 1)); break;
			case 1: v = (int)(Rand() % 32) * 4; break;
			case 2: v = -1 - (int)(Rand() % 8); break;
			case 3: v = image - (int)(Rand() % 16); break;
			case 4: v = Interesting( image ); break;
			default: v = (int)(Rand() % 256); break;
			}
			values[count++] = v;
			depth++;
		}
		if ( count >= target ) {
			break;
		}
		v = (int)(Rand() % 256);
		if ( op == OP_ENTER || op == OP_LEAVE ) v = Rand() % 8 ? frame : Interesting( image );
		if ( op >= OP_EQ && op <= OP_GEF ) v = Rand() % target;
		if ( op == OP_ARG ) v = Rand() % 4 ? 8 + (int)(Rand() % 4) * 4 : (int)(Rand() % 256);
		if ( op == OP_BLOCK_COPY ) v = Rand() % 2 ? (int)(Rand() % 16) * 4 : Interesting( image );
		if ( op == OP_CONST || op == OP_LOCAL ) v = Rand() % 2 ? (int)(Rand() % 64) * 4 : Interesting( image );
		ops[count] = op; values[count++] = v;
		depth += Effect( op );
		if ( depth < 0 ) depth = 0;
	}
	if ( count < 3 ) {
		count = 3;
		ops[1] = OP_CONST; values[1] = 7;
	}
	if ( Rand() % 4 ) {
		ops[count - 1] = OP_LEAVE; values[count - 1] = frame;
		if ( depth == 0 ) { ops[count - 2] = OP_PUSH; }
	}
	if ( (deepNesting || freeDuringCall) && count > 4 ) {
		// Reach a syscall early: the modes test what syscalls do.
		ops[1] = OP_CONST; values[1] = -1;
		ops[2] = OP_CALL;
		ops[3] = OP_POP;
	}
	if ( corrupt ) {
		i = Rand() % count;
		if ( Rand() % 2 ) ops[i] = Rand() % 256; else values[i] = Interesting( count );
	}
	for ( i = 0; i < count; i++ ) {
		int n = OperandSize( ops[i] );
		buffer[pc++] = ops[i];
		if ( n == 4 ) { Put( buffer + pc, values[i] ); }
		else if ( n == 1 ) { buffer[pc] = values[i]; }
		pc += n;
	}
	if ( corrupt && Rand() % 4 == 0 ) {
		pc -= Rand() % 4;	// truncate the code segment
	}
	Put( buffer, VM_MAGIC );
	Put( buffer + 4, count );
	Put( buffer + 8, 32 );
	Put( buffer + 12, pc - 32 );
	Put( buffer + 16, pc );
	Put( buffer + 20, data );
	Put( buffer + 24, lit );
	Put( buffer + 28, bss );
	for ( i = 0; i < data + lit; i++ ) {
		buffer[pc + i] = Rand() % 2 ? 0 : (byte)Rand();
	}
	fileLength = pc + data + lit;
	file = buffer;
}

static byte *qvms[8];
static int qvmLengths[8], qvmCount;
static byte *mutated;

/* Mode mutate: corrupt a real QVM's code (and rarely its header). */
static void GenMutate( void ) {
	int which = Rand() % qvmCount, k, n;
	int codeOffset, codeLength;
	fileLength = qvmLengths[which];
	memcpy( mutated, qvms[which], fileLength );
	codeOffset = mutated[8] | (mutated[9] << 8) | (mutated[10] << 16) | (mutated[11] << 24);
	codeLength = mutated[12] | (mutated[13] << 8) | (mutated[14] << 16) | (mutated[15] << 24);
	n = 1 + Rand() % 8;
	for ( k = 0; k < n; k++ ) {
		int at = codeOffset + (int)(Rand() % codeLength);
		switch ( Rand() % 8 ) {
		case 0: mutated[at] = (byte)Rand(); break;
		case 1: mutated[at] ^= 1 << (Rand() % 8); break;
		case 2: if ( at + 4 <= fileLength ) Put( mutated + at, Interesting( codeLength ) ); break;
		case 3: mutated[at] = OP_IGNORE + Rand() % OP_CVFI; break;
		case 4: if ( Rand() % 4 == 0 ) Put( mutated + 4 * (1 + Rand() % 7), Interesting( fileLength ) ); break;
		default: mutated[at] = (byte)(mutated[at] + (int)(Rand() % 5) - 2); break;
		}
	}
	file = mutated;
}

static byte *ReadAll( const char *path, int *length ) {
	FILE *f = fopen( path, "rb" );
	byte *b;
	if ( !f ) { perror( path ); exit( 2 ); }
	fseek( f, 0, SEEK_END ); *length = ftell( f ); fseek( f, 0, SEEK_SET );
	b = malloc( *length );
	if ( fread( b, 1, *length, f ) != (size_t)*length ) { exit( 2 ); }
	fclose( f );
	return b;
}

static int FuzzMain( int argc, char **argv ) {
	unsigned int seed, first, count;
	const char *mode;
	int i, maxLength = 0;
	if ( argc < 4 ) {
		fprintf( stderr, "usage: %s mode first count [qvm...]\n", argv[0] );
		return 2;
	}
	mode = argv[1];
	first = strtoul( argv[2], NULL, 0 );
	count = strtoul( argv[3], NULL, 0 );
	verbose = getenv( "VM_FUZZ_VERBOSE" ) != NULL;
	for ( i = 4; i < argc && qvmCount < 8; i++ ) {
		qvms[qvmCount] = ReadAll( argv[i], &qvmLengths[qvmCount] );
		if ( qvmLengths[qvmCount] > maxLength ) maxLength = qvmLengths[qvmCount];
		qvmCount++;
	}
	mutated = malloc( maxLength + 1 );
	{
		sigset_t set;
		sigemptyset( &set ); sigaddset( &set, SIGALRM );
		pthread_sigmask( SIG_UNBLOCK, &set, NULL );
	}
	signal( SIGALRM, OnAlarm );
	VM_Init();
	for ( seed = first; seed < first + count; seed++ ) {
		rng = seed * 2654435761u + 1;
		syscallMode = 0;
		deepNesting = !strcmp( mode, "program-reenter" );
		freeDuringCall = !strcmp( mode, "program-free" );
		if ( !strcmp( mode, "header" ) ) {
			GenHeader();
			RunCase( seed, 2, 50, NULL );
		} else if ( !strncmp( mode, "program", 7 ) ) {
			GenProgram();
			syscallMode = deepNesting || freeDuringCall || Rand() % 2;
			RunCase( seed, 3, 20, NULL );
		} else if ( !strcmp( mode, "mutate" ) ) {
			GenMutate();
			syscallMode = Rand() % 4 == 0;
			RunCase( seed, 2, 300, NULL );
		} else if ( !strcmp( mode, "retail" ) ) {
			/* argv[4+seed%count]: run its init entry with retail-shaped args. */
			int init[MAX_VMMAIN_ARGS] = {0};
			int which = seed % qvmCount;
			fileLength = qvmLengths[which];
			memcpy( mutated, qvms[which], fileLength );
			file = mutated;
			init[0] = strstr( argv[4 + which], "ui" ) ? 1 : 0;	// UI_INIT is 1, CG/GAME_INIT 0
			init[1] = 100; init[2] = 1234;
			syscallMode = 2;
			retailModule = init[0] ? 2 : strstr( argv[4 + which], "cgame" ) ? 0 : 1;
			RunCase( seed, 4, 20000, init );
		} else {
			return 2;
		}
	}
	fprintf( stderr, "%s: %u cases, ok=%d error=%d timeout=%d\n", mode, count,
	         outcomes[0], outcomes[1], outcomes[2] );
	if ( !strcmp( mode, "retail" ) && outcomes[0] != (int)count ) {
		fprintf( stderr, "retail QVMs did not run cleanly\n" );
		return 1;
	}
	return 0;
}

static int mainArgc; static char **mainArgv; static int mainResult;
static void *FuzzThread( void *unused ) { (void)unused; mainResult = FuzzMain( mainArgc, mainArgv ); return NULL; }
int main( int argc, char **argv ) {
	pthread_t thread;
	pthread_attr_t attr;
	sigset_t set;
	mainArgc = argc; mainArgv = argv;
	pthread_attr_init( &attr );
	/* The Mac build's native stack is 1 MB (mac_resources.r appStackSize). */
	pthread_attr_setstacksize( &attr, 1024 * 1024 );
	sigemptyset( &set ); sigaddset( &set, SIGALRM );
	pthread_sigmask( SIG_BLOCK, &set, NULL );	/* deliver the timer to the fuzz thread */
	if ( pthread_create( &thread, &attr, FuzzThread, NULL ) ) return 2;
	pthread_join( thread, NULL );
	return mainResult;
}
