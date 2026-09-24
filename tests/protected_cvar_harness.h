/* Issue #39: engine start-up, drop catching and module trap entry for the
 * protected cvar fixtures. They run the real cvar.c and cmd.c behind the real
 * syscall dispatchers, as a QVM and as a native (Mac OS 9 static) module. */
#ifndef Q3_PROTECTED_CVAR_HARNESS_H
#define Q3_PROTECTED_CVAR_HARNESS_H
#include "../code/qcommon/vm_local.h"
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#define IMAGE_SIZE 16384
#define INSTALL_PATH "Macintosh HD:Quake3"	/* the working folder mac_main.c returns */
#define CD_PATH "Macintosh HD:Q3 CD"		/* the player's +set fs_cdpath */
#define BASE_GAME "mybase"			/* the player's +set fs_basegame */
#define HOSTILE "Macintosh HD:System Folder"
#define COUNT(array) ((int)(sizeof(array) / sizeof((array)[0])))

/* The filesystem paths a module or server must not move (issue #39). */
static const char *protectedCvars[] = { "fs_basepath", "fs_homepath", "fs_cdpath", "fs_basegame" };
static const char *protectedValues[] = { INSTALL_PATH, INSTALL_PATH, CD_PATH, BASE_GAME };

cvar_t *com_cl_running;
cvar_t *com_sv_running;
static vm_t vm;
static const char *module = "engine";
static int native, expectDrop, drops, imageUsed;
static jmp_buf dropJump;
static char printed[16384];
int QDECL VM_DllSyscall( int arg, ... );	/* vm.c: the syscall pointer native modules receive */

/** Fail with the module, pass and contract that broke. */
static void Check( int ok, const char *message ) {
	if ( !ok ) {
		fprintf( stderr, "Protected cvar regression failed (%s%s): %s\n", module,
			native ? ", native" : "", message );
		exit( 1 );
	}
}
/** Fail when a trap reaches an engine service these fixtures never exercise. */
static void Unexpected( const char *name ) {
	fprintf( stderr, "Protected cvar regression reached %s\n", name );
	exit( 1 );
}
/** Take only an expected ERR_DROP, whose message must fit the engine's
 * MAXPRINTMSG com_errorMessage (Com_Error formats it unbounded). */
void QDECL Com_Error( int level, const char *format, ... ) {
	char message[MAXPRINTMSG];
	va_list ap;
	int length;

	va_start( ap, format );
	length = vsnprintf( message, sizeof( message ), format, ap );
	va_end( ap );
	if ( !expectDrop || level != ERR_DROP ) {
		fprintf( stderr, "Protected cvar regression failed (%s%s): unexpected engine error %d: %s\n",
			module, native ? ", native" : "", level, message );
		exit( 1 );
	}
	Check( length >= 0 && length < MAXPRINTMSG, "drop message fits com_errorMessage" );
	drops++;
	longjmp( dropJump, 1 );
}
/** Keep the console output, for the warnings the server path prints. */
void QDECL Com_Printf( const char *format, ... ) {
	size_t used = strlen( printed );
	va_list ap;

	va_start( ap, format );
	vsnprintf( printed + used, sizeof( printed ) - used, format, ap );
	va_end( ap );
}
void QDECL Com_DPrintf( const char *format, ... ) { (void)format; }
void Com_Memcpy( void *dest, const void *src, const size_t count ) { memcpy( dest, src, count ); }
void Com_Memset( void *dest, const int val, const size_t count ) { memset( dest, val, count ); }
/** Z_Malloc'd copies in the engine; cvar.c and cmd.c free them with Z_Free. */
char *CopyString( const char *in ) {
	char *out = malloc( strlen( in ) + 1 );

	Check( out != NULL, "CopyString allocation" );
	strcpy( out, in );
	return out;
}
void Z_Free( void *ptr ) { free( ptr ); }
void *S_Malloc( int size ) {
	void *out = malloc( size );

	Check( out != NULL, "S_Malloc allocation" );
	return out;
}
int Com_Filter( char *filter, char *name, int casesensitive ) { Unexpected( __func__ ); return 0; }
int FS_ReadFile( const char *qpath, void **buffer ) { Unexpected( __func__ ); return -1; }
void FS_FreeFile( void *buffer ) { Unexpected( __func__ ); }

/* FS_Startup's cvars, from files.c itself (the runner extracts the block). */
static cvar_t *fs_debug, *fs_copyfiles, *fs_cdpath, *fs_basepath, *fs_basegame, *fs_homepath;
static cvar_t *fs_gamedirvar, *fs_restrict;
char *Sys_DefaultCDPath( void ) { return ""; }	/* mac_main.c */
char *Sys_DefaultInstallPath( void ) { return INSTALL_PATH; }
char *Sys_DefaultHomePath( void ) { return INSTALL_PATH; }
static void FS_StartupCvars( void ) {
	const char *homePath;
#include Q3_FS_STARTUP_CVARS
}
/* The player's command line, as Com_ParseCommandLine splits it, and
 * Com_StartupVariable from common.c itself (the runner extracts it). */
static char *commandLine[] = {
	"set fs_cdpath \"" CD_PATH "\"", "set fs_basegame " BASE_GAME, "set fs_copyfiles 0",
	"set rconPassword secret", "set activeAction \"demo intro\""
};
int com_numConsoleLines;
char *com_consoleLines[COUNT( commandLine )];
#include Q3_COM_STARTUP_VARIABLE
/** The start-up Com_Init runs before any module or server: the cvar and
 * command subsystems with their engine commands, the command line, the
 * filesystem cvars, q3config.cfg, the command line again, then the cvars
 * SV_Init and CL_Init register (empty defaults over the player's values). */
static void StartEngine( void ) {
	int i;

	for ( i = 0; i < COUNT( commandLine ); i++ ) {
		com_consoleLines[i] = commandLine[i];
	}
	com_numConsoleLines = COUNT( commandLine );
	Cvar_Init();
	Cmd_Init();
	Com_StartupVariable( NULL );
	FS_StartupCvars();
	Cmd_ExecuteString( "seta sv_master2 master.example.com" );
	Com_StartupVariable( NULL );
	Cvar_Get( "sv_master2", "", CVAR_ARCHIVE );	/* SV_Init */
	Cvar_Get( "rconPassword", "", CVAR_TEMP );
	Cvar_Get( "activeAction", "", CVAR_TEMP );	/* CL_Init */
	Cvar_Get( "cl_allowDownload", "0", CVAR_ARCHIVE );
}

/** The cvar, or NULL: Cvar_Get would create a missing one. */
static cvar_t *FindCvar( const char *name ) {
	extern cvar_t *cvar_vars;	/* cvar.c */
	cvar_t *var;

	for ( var = cvar_vars; var; var = var->next ) {
		if ( !Q_stricmp( var->name, name ) ) {
			return var;
		}
	}
	return NULL;
}
static const char *Value( const char *name ) {
	cvar_t *var = FindCvar( name );

	return var ? var->string : NULL;
}
/** The protected paths still hold the values the engine gave them. */
static void CheckPaths( const char *what ) {
	int i;

	for ( i = 0; i < COUNT( protectedCvars ); i++ ) {
		Check( Value( protectedCvars[i] ) && !strcmp( Value( protectedCvars[i] ), protectedValues[i] ), what );
	}
}

/** Mark the module as native, as the Mac OS 9 static modules are. */
static int QDECL NativeEntry( int command, ... ) { (void)command; return 0; }
/** True if native pointers into page fit the int syscall ABI. */
static int Below4GiB( const byte *page ) {
	return (unsigned long)page == (unsigned int)(unsigned long)page;
}
/** Map the image page below 4 GiB, where native pointers fit the int syscall ABI. */
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
/** Load a module image: a QVM (pass 0) or a native module (pass 1). Returns
 * qfalse, after saying so, when this host has no page for the native pass. */
static qboolean LoadModule( const char *name, int (*dispatch)( int * ), int pass ) {
	module = name;
	native = pass;
	if ( !vm.dataBase ) {
		vm.dataBase = LowPage();
		vm.dataMask = IMAGE_SIZE - 1;
	}
	if ( native && !Below4GiB( vm.dataBase ) ) {
		/* CI output hides passing runners, so a CI host must run the native pass */
		Check( !getenv( "CI" ), "low native page (CI does not skip the native pass)" );
		printf( "%s: SKIPPED the native pass, no page below 4 GiB on this host\n", name );
		return qfalse;
	}
	memset( vm.dataBase, 0, IMAGE_SIZE );
	imageUsed = 16;	/* offset 0 is NULL */
	vm.systemCall = dispatch;
	vm.entryPoint = native ? NativeEntry : NULL;
	vm.interpretFaulted = qfalse;
	vm.currentlyInterpreting = !native;
	currentVM = &vm;
	return qtrue;
}
/** Room for size bytes in the module image, as the module's own data. */
static int Alloc( int size ) {
	int offset = imageUsed;

	imageUsed += ( size + 3 ) & ~3;
	Check( imageUsed <= IMAGE_SIZE, "image space" );
	return offset;
}
/** A string in the module image, as the trap argument the module passes:
 * an image offset for a QVM, a pointer for a native module. */
static int Arg( int offset ) {
	return native ? (int)(unsigned long)( vm.dataBase + offset ) : offset;
}
static int Str( const char *text ) {
	int offset = Alloc( (int)strlen( text ) + 1 );

	strcpy( (char *)vm.dataBase + offset, text );
	return Arg( offset );
}
static void *Image( int arg ) {
	return native ? (void *)(unsigned long)(unsigned int)arg : vm.dataBase + arg;
}
/** Enter the real dispatcher the way each module kind does: a QVM's args
 * directly, a native module's through VM_DllSyscall. */
static int Trap( int trap, int a1, int a2, int a3, int a4 ) {
	int args[5];

	vm.interpretFaulted = qfalse;
	vm.currentlyInterpreting = !native;
	if ( native ) {
		return VM_DllSyscall( trap, a1, a2, a3, a4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 );
	}
	args[0] = trap; args[1] = a1; args[2] = a2; args[3] = a3; args[4] = a4;
	return vm.systemCall( args );
}
/** A trap the engine must refuse by dropping the module before it changes anything. */
static void Refuse( int trap, int a1, int a2, int a3, int a4, const char *what ) {
	int before = drops;

	expectDrop = 1;
	if ( setjmp( dropJump ) == 0 ) {
		Trap( trap, a1, a2, a3, a4 );
	}
	expectDrop = 0;
	Check( drops == before + 1, what );
}
/** A module registering its own cvar with the flags only the engine gives
 * (who created it, protection, the not-found marker), each alone and all
 * together, new and again: Cvar_Register drops them, and the module still
 * sets the cvar through setTrap. */
static void CheckEngineOnlyFlags( void (*registerCvar)( const char *name, int flags ), int setTrap ) {
	static const unsigned int asked[] = {
		CVAR_USER_CREATED, CVAR_SERVER_CREATED, CVAR_PROTECTED, CVAR_NONEXISTENT,
		CVAR_USER_CREATED | CVAR_SERVER_CREATED | CVAR_PROTECTED | CVAR_NONEXISTENT
	};
	char name[MAX_CVAR_VALUE_STRING];
	cvar_t *var;
	int i, again;

	for ( i = 0; i < COUNT( asked ); i++ ) {
		Com_sprintf( name, sizeof( name ), "%s_flags%i%s", module, i, native ? "_native" : "" );
		for ( again = 0; again < 2; again++ ) {
			registerCvar( name, CVAR_ARCHIVE | (int)asked[i] );
			var = FindCvar( name );
			Check( var && ( var->flags & CVAR_ARCHIVE ) && !( (unsigned int)var->flags & asked[i] ),
				"a module gave its cvar a flag only the engine gives" );
		}
		Trap( setTrap, Str( name ), Str( "2" ), 0, 0 );
		Check( !strcmp( var->string, "2" ), "a module sets its cvar after asking for engine-only flags" );
	}
}
#endif
