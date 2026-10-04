/* Issue #48: a module's preprocessor handles must not outlive its VM.  The
   cgame (CG_PC_LOAD_SOURCE) and game (BOTLIB_PC_LOAD_SOURCE) open handles in
   botlib's one sourceFiles[] table, and the UI opens them in cl_ui.c's script
   table.  A module that errors out mid-parse never frees its handle, and
   nothing freed it when the VM shut down, so the handle kept its slot and,
   since the live token budget (MAX_LIVE_TOKENS), its tokens for the rest of
   the session: about 15 leaked Team Arena hud/menu files made every later bot
   or menu parse fail, and 63 small ones filled the table.
   Each fixture drives the real trap dispatcher and the real shutdown or
   restart of one module (CL_ShutdownCGame, CL_ShutdownUI, and
   SV_ShutdownGameProgs and SV_RestartGameProgs) through many simulated VM
   lifetimes that leak a handle, against the real l_precomp.c and l_script.c.
   A handle another module holds open meanwhile must survive every one of
   them, and the retail flow (load, read, free, shut down) must not change. */
#if defined(Q3_PC_FIXTURE_CGAME)
#include "../code/client/cl_cgame.c"
#elif defined(Q3_PC_FIXTURE_UI)
#define Q3_CLIENT_SYSCALL_REAL_FS	/* the fixture's files serve the UI parser */
#include "../code/client/cl_ui.c"
#elif defined(Q3_PC_FIXTURE_GAME)
#include "../code/server/sv_game.c"
#include "../code/qcommon/vm_local.h"
#else
#error define Q3_PC_FIXTURE_CGAME, Q3_PC_FIXTURE_UI or Q3_PC_FIXTURE_GAME
#endif
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_FIXTURE_FILES	8
#define MAX_SOURCE_HANDLES	64	/* l_precomp.c's MAX_SOURCEFILES */
#define LEAK_CYCLES			20	/* 300 tokens each: 6000 > MAX_LIVE_TOKENS */
#define SLOT_CYCLES			80	/* more than the 63 botlib or 15 UI slots */
#define NAME_OFFSET			16	/* where the trap's file name lives in the image */
#define TOKEN_OFFSET		1024	/* where the trap's pc_token_t lives in the image */

/* the real botlib preprocessor (l_precomp.c, l_script.c built with BOTLIB) */
extern int numtokens;
int PC_LoadSourceHandle( const char *filename, int owner );
int PC_FreeSourceHandle( int handle );
int PC_ReadTokenHandle( int handle, pc_token_t *pc_token );
int PC_SourceFileAndLine( int handle, char *filename, int *line );
void PC_FreeSourceHandles( int owner );
botlib_import_t botimport;

static const char *phase = "setup";
static int failures, botlibErrors, botlibWarnings, liveBlocks, openFiles;
static char *fileNames[MAX_FIXTURE_FILES], *fileTexts[MAX_FIXTURE_FILES];
static int numFiles, openText[MAX_SOURCE_HANDLES];
static char hudText[16384];
static botlib_export_t api;

static void Expect( int ok, const char *message ) {
	if ( !ok ) {
		fprintf( stderr, "FAIL [%s]: %s (numtokens=%d)\n", phase, message, numtokens );
		failures++;
	}
}


#if defined(Q3_PC_FIXTURE_GAME)
/* --- the engine services the game's shutdown, restart and traps reach ---- */
#define IMAGE_SIZE 8192
server_t sv;
serverStatic_t svs;
static cvar_t maxclients;
cvar_t *sv_maxclients = &maxclients;
static vm_t vm;
vm_t *gvm;
vm_t *currentVM;
static int restarts;

static void Unexpected( const char *name ) {
	fprintf( stderr, "FAIL [%s]: reached %s\n", phase, name );
	exit( 1 );
}
void QDECL Com_Error( int level, const char *format, ... ) { (void)level; (void)format; Unexpected( __func__ ); }
void QDECL Com_Printf( const char *format, ... ) { (void)format; }
void Com_Memcpy( void *dest, const void *src, const size_t count ) { memcpy( dest, src, count ); }
void Com_Memset( void *dest, const int val, const size_t count ) { memset( dest, val, count ); }
int Com_Milliseconds( void ) { return 0; }
char *CM_EntityString( void ) { return ""; }
/* a QVM shutting down after an error: the call does not reach the module */
int VM_CallArgs( vm_t *target, int callnum, const int *parameters, int count ) {
	(void)parameters; (void)count;
	Expect( target == &vm && (callnum == GAME_SHUTDOWN || callnum == GAME_INIT), "game entry point" );
	return 0;
}
void VM_Free( vm_t *target ) { Expect( target == &vm, "the game VM is freed" ); currentVM = NULL; }
/* a QVM restarts in place with its data reloaded */
vm_t *VM_Restart( vm_t *target ) { restarts++; return target; }
void VM_Error( const char *message ) { (void)message; Unexpected( __func__ ); }
void VM_ErrorForVM( vm_t *target, const char *message ) { (void)target; (void)message; Unexpected( __func__ ); }
void *VM_CheckedArgArray( int value, int count, int elementSize ) { Unexpected( __func__ ); return NULL; }
void *VM_CheckedArgPtr( int value, int length, int alignment, qboolean nullable ) {
	(void)alignment;
	if ( !value && nullable ) return NULL;
	Expect( value > 0 && value + length <= IMAGE_SIZE, "trap pointer inside the image" );
	return vm.dataBase + value;
}
char *VM_CheckedArgString( int value, qboolean nullable ) { return VM_CheckedArgPtr( value, 1, 1, nullable ); }
void *VM_CheckedStringBuffer( int value, int length, qboolean nullable ) { return VM_CheckedArgPtr( value, length, 1, nullable ); }
int SV_BotLibSetup( void ) { Unexpected( __func__ ); return 0; }
int SV_BotLibShutdown( void ) { Unexpected( __func__ ); return 0; }
int SV_BotGetSnapshotEntity( int client, int sequence ) { Unexpected( __func__ ); return 0; }
int SV_BotGetConsoleMessage( int client, char *buf, int size ) { Unexpected( __func__ ); return 0; }
void SV_ClientThink( client_t *cl, usercmd_t *cmd ) { Unexpected( __func__ ); }

#define MODULE_OWNER	PC_OWNER_GAME
#define TRAP_LOAD		BOTLIB_PC_LOAD_SOURCE
#define TRAP_FREE		BOTLIB_PC_FREE_SOURCE
#define TRAP_READ		BOTLIB_PC_READ_TOKEN
#define MODULE_SLOTS	(MAX_SOURCE_HANDLES - 1)
static int Trap( int *args ) { return SV_BotLibNavigationCalls( args ); }
static void StartModule( void ) {
	if ( !vm.dataBase ) vm.dataBase = calloc( 1, IMAGE_SIZE );
	vm.dataMask = IMAGE_SIZE - 1;
	vm.interpretFaulted = qfalse;
	gvm = currentVM = &vm;
}
/* alternate map changes and map_restarts */
static void StopModule( int cycle ) {
	if ( cycle & 1 ) {
		SV_RestartGameProgs();
		Expect( gvm == &vm, "the restarted game keeps its VM" );
	} else {
		SV_ShutdownGameProgs();
		Expect( gvm == NULL, "the game VM is gone" );
	}
}
#else
/* --- the client fixtures drive the real dispatcher over vm.c ------------- */
#include "client_syscall_stubs.h"
void Sys_UnloadDll( void *dllHandle ) { Unexpected( __func__ ); }
static byte *image;

static void StartClientVM( void ) {
	if ( !image ) image = calloc( 1, IMAGE_SIZE );
	Check( image != NULL, "allocation" );
	memset( &vm, 0, sizeof( vm ) );
	vm.dataBase = image;
	vm.dataMask = IMAGE_SIZE - 1;
	vm.currentlyInterpreting = qtrue;
	currentVM = &vm;
}
/* an ERR_DROP during the parse: the faulted QVM is shut down without
   reaching its own shutdown code, so an open handle is never freed (the
   retail flow has freed its handle before it gets here) */
static void FaultClientVM( void ) {
	vm.interpretFaulted = qtrue;
	vm.currentlyInterpreting = qfalse;
}
#if defined(Q3_PC_FIXTURE_CGAME)
vm_t *cgvm;
int Key_GetCatcher( void ) { Unexpected( __func__ ); return 0; }
void Key_SetCatcher( int catcher ) { Unexpected( __func__ ); }
#define MODULE_OWNER	PC_OWNER_CGAME
#define TRAP_LOAD		CG_PC_LOAD_SOURCE
#define TRAP_FREE		CG_PC_FREE_SOURCE
#define TRAP_READ		CG_PC_READ_TOKEN
#define MODULE_SLOTS	(MAX_SOURCE_HANDLES - 1)
static int Trap( int *args ) { return CL_CgameSystemCalls( args ); }
static void StartModule( void ) { StartClientVM(); cgvm = &vm; }
static void StopModule( int cycle ) {
	(void)cycle;
	FaultClientVM();
	CL_ShutdownCGame();
	Expect( cgvm == NULL, "the cgame VM is gone" );
}
#else
/* the UI reads its menus through cl_ui.c's own script parser */
#define MODULE_OWNER	PC_OWNER_UI
/* the UI parser does not preprocess, so its hud is a plain menu file */
#define HUD_FILE		"ui/small.menu"
#define HUD_TOKENS		"menu", "1", "end"
#define TRAP_LOAD		UI_PC_LOAD_SOURCE
#define TRAP_FREE		UI_PC_FREE_SOURCE
#define TRAP_READ		UI_PC_READ_TOKEN
#define MODULE_SLOTS	(MAX_SCRIPT_HANDLES - 1)
static int Trap( int *args ) { return CL_UISystemCalls( args ); }
static void StartModule( void ) { StartClientVM(); uivm = &vm; }
static void StopModule( int cycle ) {
	(void)cycle;
	FaultClientVM();
	CL_ShutdownUI();
	Expect( uivm == NULL, "the UI VM is gone" );
}
#endif
#endif

#ifndef HUD_FILE
/* a Team Arena hud read through botlib, holding 300 live tokens */
#define HUD_FILE		"ui/hud.txt"
#define HUD_TOKENS		"hud", "299", "end"
#endif

/* --- files and memory for botlib and the UI parser ----------------------- */
static void AddFile( const char *name, const char *contents ) {
	Expect( numFiles < MAX_FIXTURE_FILES, "fixture file table" );
	fileNames[numFiles] = strdup( name );
	fileTexts[numFiles] = strdup( contents );
	numFiles++;
}
static int OpenFile( const char *path, fileHandle_t *file ) {
	int i, handle;

	*file = 0;
	for ( i = 0; i < numFiles; i++ ) {
		if ( strcmp( fileNames[i], path ) ) continue;
		for ( handle = 1; handle < MAX_SOURCE_HANDLES && openText[handle]; handle++ );
		if ( handle >= MAX_SOURCE_HANDLES ) return -1;
		openText[handle] = i + 1;
		openFiles++;
		*file = handle;
		return (int)strlen( fileTexts[i] );
	}
	return -1;
}
static int ReadFile( void *out, int length, fileHandle_t file ) {
	memcpy( out, fileTexts[openText[file] - 1], length );
	return length;
}
static void CloseFile( fileHandle_t file ) { openText[file] = 0; openFiles--; }
static int BotFOpen( const char *path, fileHandle_t *file, fsMode_t mode ) { (void)mode; return OpenFile( path, file ); }
static void QDECL BotPrint( int type, char *format, ... ) {
	if ( type == PRT_ERROR || type == PRT_FATAL ) botlibErrors++;
	else if ( type == PRT_WARNING ) botlibWarnings++;
	(void)format;
}

void *GetMemory( unsigned long size ) {
	void *p = malloc( size ? size : 1 );
	if ( p ) liveBlocks++;
	return p;
}
void *GetClearedMemory( unsigned long size ) { void *p = GetMemory( size ); if ( p ) memset( p, 0, size ); return p; }
void FreeMemory( void *p ) { if ( p ) { liveBlocks--; free( p ); } }
void *GetHunkMemory( unsigned long size ) { return GetMemory( size ); }
void *GetClearedHunkMemory( unsigned long size ) { return GetClearedMemory( size ); }
void QDECL Log_Write( char *format, ... ) { (void)format; }

#if defined(Q3_PC_FIXTURE_UI)
int FS_FOpenFileByMode( const char *qpath, fileHandle_t *f, fsMode_t mode ) { (void)mode; return OpenFile( qpath, f ); }
int FS_Read( void *buffer, int len, fileHandle_t f ) { return ReadFile( buffer, len, f ); }
void FS_FCloseFile( fileHandle_t f ) { CloseFile( f ); }
void *Z_Malloc( int size ) { void *p = calloc( 1, size ); if ( p ) liveBlocks++; return p; }
void Z_Free( void *ptr ) { if ( ptr ) { liveBlocks--; free( ptr ); } }
int FS_GetFileList( const char *path, const char *extension, char *listbuf, int bufsize ) { Unexpected( __func__ ); return 0; }
int FS_Read2( void *buffer, int len, fileHandle_t f ) { Unexpected( __func__ ); return 0; }
int FS_Seek( fileHandle_t f, long offset, int origin ) { Unexpected( __func__ ); return 0; }
int FS_SV_FOpenFileRead( const char *filename, fileHandle_t *fp ) { Unexpected( __func__ ); return 0; }
fileHandle_t FS_SV_FOpenFileWrite( const char *filename ) { Unexpected( __func__ ); return 0; }
int FS_Write( const void *buffer, int len, fileHandle_t f ) { Unexpected( __func__ ); return 0; }
#endif

/* --- the module's view: its traps ---------------------------------------- */
static int LoadTrap( const char *name ) {
	int args[4] = {TRAP_LOAD, NAME_OFFSET, 0, 0};
	strcpy( (char *)vm.dataBase + NAME_OFFSET, name );
	return Trap( args );
}
static const char *ReadTrap( int handle ) {
	int args[4] = {TRAP_READ, handle, TOKEN_OFFSET, 0};
	pc_token_t *token = (pc_token_t *)( vm.dataBase + TOKEN_OFFSET );
	if ( !Trap( args ) ) return "(no token)";
	return token->string;
}
static int FreeTrap( int handle ) {
	int args[4] = {TRAP_FREE, handle, 0, 0};
	return Trap( args );
}
/* a module that loads its hud, reads into it and then errors out */
static int LeakOneHandle( const char *name, const char *first, const char *second ) {
	int handle;

	StartModule();
	handle = LoadTrap( name );
	Expect( handle > 0, "the module's file loads" );
	if ( handle <= 0 ) return 0;
	Expect( !strcmp( ReadTrap( handle ), first ), "the module's first token" );
	Expect( !strcmp( ReadTrap( handle ), second ), "the module's macro expands" );
	return handle;
}

/* --- the other module's handle, opened directly in botlib ---------------- */
static int OpenBystander( int owner ) {
	pc_token_t token;
	int handle = PC_LoadSourceHandle( "botfiles/other.c", owner );

	Expect( handle > 0, "another module's handle opens" );
	Expect( PC_ReadTokenHandle( handle, &token ) && !strcmp( token.string, "other" ), "another module reads its file" );
	return handle;
}
static void CloseBystander( int handle ) {
	pc_token_t token;
	char name[MAX_QPATH];
	int line = -1;

	Expect( PC_SourceFileAndLine( handle, name, &line ) && !strcmp( name, "botfiles/other.c" ),
		"another module's handle survives this module's shutdowns" );
	Expect( PC_ReadTokenHandle( handle, &token ) && !strcmp( token.string, "7" ),
		"another module's handle still expands its macro" );
	Expect( PC_ReadTokenHandle( handle, &token ) && !strcmp( token.string, "tail" ),
		"another module's handle reads on to its end" );
	Expect( PC_FreeSourceHandle( handle ), "another module frees its own handle" );
}
static void ExpectNothingOpen( void ) {
	char name[MAX_QPATH];
	int i, line;

	for ( i = 1; i < MAX_SOURCE_HANDLES; i++ ) {
		Expect( !PC_SourceFileAndLine( i, name, &line ), "no preprocessor handle is left open" );
	}
	Expect( numtokens == 0, "every live token is released" );
	Expect( liveBlocks == 0, "every source and script buffer is released" );
	Expect( openFiles == 0, "every file is closed" );
}

int main( void ) {
	static const char *hudTokens[3] = {HUD_TOKENS};
	int bystanders[2], owners[3] = {PC_OWNER_GAME, PC_OWNER_CGAME, PC_OWNER_UI};
	int i, n, handle, used, console;

	botimport.Print = BotPrint;
	botimport.FS_FOpenFile = BotFOpen;
	botimport.FS_Read = ReadFile;
	botimport.FS_FCloseFile = CloseFile;
	api.PC_LoadSourceHandle = PC_LoadSourceHandle;
	api.PC_FreeSourceHandle = PC_FreeSourceHandle;
	api.PC_ReadTokenHandle = PC_ReadTokenHandle;
	api.PC_SourceFileAndLine = PC_SourceFileAndLine;
	api.PC_FreeSourceHandles = PC_FreeSourceHandles;
	botlib_export = &api;
	/* cl_ui.c logs every script load to stdout */
	fflush( stdout );
	console = dup( 1 );
	n = open( "/dev/null", O_WRONLY );
	if ( console < 0 || n < 0 || dup2( n, 1 ) < 0 ) { fprintf( stderr, "stdout\n" ); return 2; }
	close( n );

	/* a Team Arena hud: about menudef.h's 253 one-token definitions */
	used = 0;
	for ( i = 0; i < 300; i++ ) {
		used += snprintf( hudText + used, sizeof( hudText ) - used, "#define D%d %d\n", i, i );
	}
	snprintf( hudText + used, sizeof( hudText ) - used, "hud D299 end\n" );
	AddFile( "ui/hud.txt", hudText );
	AddFile( "ui/small.menu", "menu 1 end\n" );
	AddFile( "botfiles/other.c", "#define SEVEN 7\nother SEVEN tail\n" );

	phase = "retail load, read, free and shutdown";
	StartModule();
	handle = LoadTrap( HUD_FILE );
	Expect( handle > 0, "the hud loads" );
	Expect( !strcmp( ReadTrap( handle ), hudTokens[0] ) && !strcmp( ReadTrap( handle ), hudTokens[1] ) &&
		!strcmp( ReadTrap( handle ), hudTokens[2] ), "the hud reads in full" );
	Expect( FreeTrap( handle ), "the module frees its handle" );
	StopModule( 0 );
	Expect( !botlibErrors && !botlibWarnings, "the retail flow reports nothing" );
	ExpectNothingOpen();

	/* every other module holds a handle open the whole time */
	n = 0;
	for ( i = 0; i < 3; i++ ) {
		if ( owners[i] != MODULE_OWNER ) bystanders[n++] = OpenBystander( owners[i] );
	}

	phase = "hud leaked by each of 20 VM lifetimes";
	for ( i = 0; i < LEAK_CYCLES; i++ ) {
		LeakOneHandle( HUD_FILE, hudTokens[0], hudTokens[1] );
		StopModule( i );
	}

	phase = "small menu leaked by each of 80 VM lifetimes";
	for ( i = 0; i < SLOT_CYCLES; i++ ) {
		LeakOneHandle( "ui/small.menu", "menu", "1" );
		StopModule( i );
	}
	Expect( MODULE_SLOTS < SLOT_CYCLES, "the cycles outnumber the module's slots" );

	phase = "other modules' handles";
	CloseBystander( bystanders[0] );
	CloseBystander( bystanders[1] );
	ExpectNothingOpen();
#if defined(Q3_PC_FIXTURE_GAME)
	Expect( restarts == ( LEAK_CYCLES + SLOT_CYCLES ) / 2, "every odd cycle was a map_restart" );
#endif

	fflush( stdout );
	dup2( console, 1 );
	if ( failures ) {
		fprintf( stderr, "%d preprocessor handle lifecycle check(s) failed\n", failures );
		return 1;
	}
	puts( "Preprocessor handles a module leaves open are freed with its VM, and only its own (issue #48)" );
	return 0;
}
