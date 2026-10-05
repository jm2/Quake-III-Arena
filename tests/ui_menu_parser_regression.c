/* Issue #11: the Team Arena UI reads its .menu files through the UI_PC_*
   traps, and retail 1.32c routes every one of them to botlib's precompiler
   (l_precomp.c), the same one the cgame uses for its hud.  This port had
   routed them to a small parser in cl_ui.c instead, which had no #include or
   #define, ignored global defines, reported no source locations and typed
   most tokens wrongly, so a retail menu (#include "ui/menudef.h", then
   MENU_TRUE, ITEM_TYPE_BUTTON, ...) did not load.
   This fixture drives the real cl_ui.c dispatcher (CL_UISystemCalls) as a
   QVM would, over the real l_precomp.c and l_script.c, against a corpus of
   retail-syntax menus (tests/ui_menu_corpus) and the repository's own
   ui/menudef.h:
   - every token, its type, subtype and values, and the file and line it came
     from, read through the traps, equal what botlib reads directly;
   - a token-by-token check of retail syntax (includes, object and global
     defines, conditionals, signed, decimal and hex numbers, strings and
     punctuation);
   - the real Team Arena UI (ui_main.c's UI_LoadMenus and ui_shared.c's
     Menu_New, see ui_menu_parser_module.c) loads every menu in the corpus with
     the values the syntax spells, and reports no parse error. */
#define Q3_CLIENT_SYSCALL_REAL_FS	/* the corpus serves the engine's and botlib's reads */
#include "../code/client/cl_ui.c"
#include "client_syscall_stubs.h"
#include <stdarg.h>

#define MAX_FIXTURE_FILES	16
#define MAX_SOURCE_HANDLES	64		/* l_precomp.c's MAX_SOURCEFILES */
#define NAME_OFFSET			16		/* where a trap's file name lives in the image */
#define TOKEN_OFFSET		1024	/* where a trap's pc_token_t lives in the image */
#define FILE_OFFSET			2560	/* where a trap's MAX_QPATH file name output lives */
#define LINE_OFFSET			2688	/* where a trap's line output lives */

/* the real botlib preprocessor (l_precomp.c, l_script.c built with BOTLIB) */
extern int numtokens;
int PC_AddGlobalDefine( char *string );
void PC_RemoveAllGlobalDefines( void );
int PC_LoadSourceHandle( const char *filename, int owner );
int PC_FreeSourceHandle( int handle );
int PC_ReadTokenHandle( int handle, pc_token_t *pc_token );
int PC_SourceFileAndLine( int handle, char *filename, int *line );
void PC_FreeSourceHandles( int owner );
botlib_import_t botimport;

/* ui_menu_parser_module.c: the real Team Arena UI, built as the UI module */
int Fixture_UILoadMenus( void );

static const char *phase = "setup";
static int failures, botlibMessages, liveBlocks;
static const char *roots[2];
static struct {
	char *text;
	int length;
} openFiles[MAX_FIXTURE_FILES];
static botlib_export_t api;

static void Expect( int ok, const char *message ) {
	if ( !ok ) {
		fprintf( stderr, "FAIL [%s]: %s\n", phase, message );
		failures++;
	}
}

void Sys_UnloadDll( void *dllHandle ) { Unexpected( __func__ ); }

/* --- the engine's file system over the corpus, then the repository ------- */
/* sv_bot.c gives botlib FS_FOpenFileByMode, FS_Read and FS_FCloseFile too */
int FS_FOpenFileByMode( const char *qpath, fileHandle_t *f, fsMode_t mode ) {
	char path[1024];
	FILE *file = NULL;
	long length;
	int i, handle;

	*f = 0;
	Expect( mode == FS_READ, "files are only read" );
	if ( strstr( qpath, ".." ) ) return -1;
	for ( i = 0; i < 2 && !file; i++ ) {
		snprintf( path, sizeof( path ), "%s/%s", roots[i], qpath );
		file = fopen( path, "rb" );
	}
	if ( !file ) return -1;
	for ( handle = 1; handle < MAX_FIXTURE_FILES && openFiles[handle].text; handle++ );
	Expect( handle < MAX_FIXTURE_FILES, "fixture file table" );
	if ( handle >= MAX_FIXTURE_FILES || fseek( file, 0, SEEK_END ) || ( length = ftell( file ) ) < 0 ) {
		fclose( file );
		return -1;
	}
	rewind( file );
	openFiles[handle].text = malloc( length + 1 );
	Check( openFiles[handle].text != NULL, "allocation" );
	Check( fread( openFiles[handle].text, 1, length, file ) == (size_t)length, "fixture read" );
	fclose( file );
	openFiles[handle].length = (int)length;
	*f = handle;
	return (int)length;
}
int FS_Read( void *buffer, int len, fileHandle_t f ) {
	Check( f > 0 && f < MAX_FIXTURE_FILES && openFiles[f].text && len <= openFiles[f].length, "file read" );
	memcpy( buffer, openFiles[f].text, len );
	return len;
}
void FS_FCloseFile( fileHandle_t f ) {
	Check( f > 0 && f < MAX_FIXTURE_FILES && openFiles[f].text, "file close" );
	free( openFiles[f].text );
	openFiles[f].text = NULL;
}
void *Z_Malloc( int size ) { void *p = calloc( 1, size ); if ( p ) liveBlocks++; return p; }
void Z_Free( void *ptr ) { if ( ptr ) { liveBlocks--; free( ptr ); } }
int FS_GetFileList( const char *path, const char *extension, char *listbuf, int bufsize ) { Unexpected( __func__ ); return 0; }
int FS_Read2( void *buffer, int len, fileHandle_t f ) { Unexpected( __func__ ); return 0; }
int FS_Seek( fileHandle_t f, long offset, int origin ) { Unexpected( __func__ ); return 0; }
int FS_SV_FOpenFileRead( const char *filename, fileHandle_t *fp ) { Unexpected( __func__ ); return 0; }
fileHandle_t FS_SV_FOpenFileWrite( const char *filename ) { Unexpected( __func__ ); return 0; }
int FS_Write( const void *buffer, int len, fileHandle_t f ) { Unexpected( __func__ ); return 0; }

/* --- botlib's imports ------------------------------------------------------ */
static void QDECL BotPrint( int type, char *format, ... ) {
	va_list args;

	botlibMessages++;
	va_start( args, format );
	fprintf( stderr, "botlib (%d): ", type );
	vfprintf( stderr, format, args );
	va_end( args );
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

/* --- the UI's traps, marshalled through an interpreted VM's image -------- */
static int Trap( int callnum, int arg1, int arg2, int arg3 ) {
	int args[4];

	args[0] = callnum; args[1] = arg1; args[2] = arg2; args[3] = arg3;
	Check( !vm.interpretFaulted, "the UI VM is live" );
	return CL_UISystemCalls( args );
}
int Fixture_PCAddGlobalDefine( const char *define ) {
	Q_strncpyz( (char *)vm.dataBase + NAME_OFFSET, define, TOKEN_OFFSET - NAME_OFFSET );
	return Trap( UI_PC_ADD_GLOBAL_DEFINE, NAME_OFFSET, 0, 0 );
}
int Fixture_PCLoadSource( const char *name ) {
	Q_strncpyz( (char *)vm.dataBase + NAME_OFFSET, name, TOKEN_OFFSET - NAME_OFFSET );
	return Trap( UI_PC_LOAD_SOURCE, NAME_OFFSET, 0, 0 );
}
int Fixture_PCFreeSource( int handle ) {
	return Trap( UI_PC_FREE_SOURCE, handle, 0, 0 );
}
int Fixture_PCReadToken( int handle, pc_token_t *token ) {
	int result;

	memset( vm.dataBase + TOKEN_OFFSET, 0, sizeof( *token ) );
	result = Trap( UI_PC_READ_TOKEN, handle, TOKEN_OFFSET, 0 );
	memcpy( token, vm.dataBase + TOKEN_OFFSET, sizeof( *token ) );
	return result;
}
int Fixture_PCSourceFileAndLine( int handle, char *filename, int *line ) {
	int result;

	memset( vm.dataBase + FILE_OFFSET, 0, MAX_QPATH );
	memset( vm.dataBase + LINE_OFFSET, 0, sizeof( int ) );
	result = Trap( UI_PC_SOURCE_FILE_AND_LINE, handle, FILE_OFFSET, LINE_OFFSET );
	memcpy( filename, vm.dataBase + FILE_OFFSET, MAX_QPATH );
	memcpy( line, vm.dataBase + LINE_OFFSET, sizeof( int ) );
	return result;
}

/* --- every token through the traps equals botlib's own reading ----------- */
static void CompareWithBotlib( const char *name ) {
	pc_token_t viaTrap, direct;
	char trapFile[MAX_QPATH], directFile[MAX_QPATH];
	int trapHandle, directHandle, trapLine, directLine, trapRead, directRead, count;

	phase = name;
	trapHandle = Fixture_PCLoadSource( name );
	directHandle = PC_LoadSourceHandle( name, PC_OWNER_CGAME );
	Expect( trapHandle > 0 && directHandle > 0, "the file loads both ways" );
	if ( trapHandle <= 0 || directHandle <= 0 ) return;
	for ( count = 0; ; count++ ) {
		trapRead = Fixture_PCReadToken( trapHandle, &viaTrap );
		directRead = PC_ReadTokenHandle( directHandle, &direct );
		Expect( trapRead == directRead, "the token stream ends in the same place" );
		if ( !trapRead || !directRead ) break;
		if ( strcmp( viaTrap.string, direct.string ) || viaTrap.type != direct.type ||
			viaTrap.subtype != direct.subtype || viaTrap.intvalue != direct.intvalue ||
			memcmp( &viaTrap.floatvalue, &direct.floatvalue, sizeof( float ) ) ) {
			fprintf( stderr, "token %d: trap '%s' type %d/%d (%d, %g), botlib '%s' type %d/%d (%d, %g)\n",
				count, viaTrap.string, viaTrap.type, viaTrap.subtype, viaTrap.intvalue, viaTrap.floatvalue,
				direct.string, direct.type, direct.subtype, direct.intvalue, direct.floatvalue );
			Expect( 0, "the trap's token equals botlib's" );
			break;
		}
		trapLine = directLine = -1;
		Expect( Fixture_PCSourceFileAndLine( trapHandle, trapFile, &trapLine ) &&
			PC_SourceFileAndLine( directHandle, directFile, &directLine ) &&
			!strcmp( trapFile, directFile ) && trapLine == directLine && trapLine > 0,
			"the trap reports botlib's file and line" );
	}
	Expect( count > 10, "the file has tokens" );
	Expect( Fixture_PCFreeSource( trapHandle ) && PC_FreeSourceHandle( directHandle ), "both handles free" );
}

/* --- retail syntax, token by token --------------------------------------- */
typedef struct {
	const char *string;		/* NULL: a number, checked by value */
	int type;
	float value;
	int line;				/* nonzero: the line the trap must report */
} expectedToken_t;

static void CheckTokens( void ) {
	static const expectedToken_t expected[] = {
		{ "pack", TT_NAME, 0, 4 }, { NULL, TT_NUMBER, 1, 0 },	/* #ifdef of a global define */
		{ "type", TT_NAME, 0, 8 }, { NULL, TT_NUMBER, 6, 0 },	/* menudef.h's ITEM_TYPE_LISTBOX */
		{ "feeder", TT_NAME, 0, 0 }, { NULL, TT_NUMBER, 10, 0 },	/* FEEDER_DEMOS, 0x0a */
		{ "rect", TT_NAME, 0, 9 }, { "-", TT_PUNCTUATION, 0, 0 }, { NULL, TT_NUMBER, 12, 0 },
		{ "+", TT_PUNCTUATION, 0, 0 }, { NULL, TT_NUMBER, 3, 0 }, { NULL, TT_NUMBER, 0.5f, 0 },
		{ NULL, TT_NUMBER, 640, 0 }, { NULL, TT_NUMBER, 0.25f, 0 },	/* WIDE, then .25 */
		{ "text", TT_NAME, 0, 10 }, { "ab", TT_STRING, 0, 0 },	/* adjacent strings merge */
		{ "action", TT_NAME, 0, 11 }, { "{", TT_PUNCTUATION, 0, 0 }, { "play", TT_NAME, 0, 0 },
		{ "sound/x.wav", TT_STRING, 0, 0 }, { ";", TT_PUNCTUATION, 0, 0 }, { "open", TT_NAME, 0, 0 },
		{ "main", TT_NAME, 0, 0 }, { "}", TT_PUNCTUATION, 0, 11 }
	};
	pc_token_t token;
	char file[MAX_QPATH];
	int i, handle, line;

	phase = "ui/tokens.menu, token by token";
	handle = Fixture_PCLoadSource( "ui/tokens.menu" );
	Expect( handle > 0, "the file loads" );
	if ( handle <= 0 ) return;
	for ( i = 0; i < (int)( sizeof( expected ) / sizeof( expected[0] ) ); i++ ) {
		if ( !Fixture_PCReadToken( handle, &token ) ) {
			Expect( 0, "a token is missing" );
			break;
		}
		if ( token.type != expected[i].type ||
			( expected[i].string && strcmp( token.string, expected[i].string ) ) ||
			( !expected[i].string && ( token.floatvalue != expected[i].value ||
				token.intvalue != (int)expected[i].value ) ) ) {
			fprintf( stderr, "token %d: '%s' type %d (%d, %g)\n", i, token.string, token.type,
				token.intvalue, token.floatvalue );
			Expect( 0, "the token is the retail one" );
		}
		if ( expected[i].line ) {
			line = -1;
			Expect( Fixture_PCSourceFileAndLine( handle, file, &line ) &&
				!strcmp( file, "ui/tokens.menu" ) && line == expected[i].line,
				"the token's file and line" );
		}
	}
	Expect( !Fixture_PCReadToken( handle, &token ), "the file ends there" );
	Expect( Fixture_PCFreeSource( handle ), "the handle frees" );
}

int main( int argc, char **argv ) {
	static const char *corpus[] = {
		"ui/menus.txt", "ui/main.menu", "ui/setup.menu", "ui/player.menu", "ui/tokens.menu"
	};
	char file[MAX_QPATH];
	byte *image;
	int i, line;

	if ( argc != 3 ) {
		fprintf( stderr, "usage: %s <corpus directory> <repository root>\n", argv[0] );
		return 2;
	}
	roots[0] = argv[1];
	roots[1] = argv[2];
	botimport.Print = BotPrint;
	botimport.FS_FOpenFile = FS_FOpenFileByMode;
	botimport.FS_Read = FS_Read;
	botimport.FS_FCloseFile = FS_FCloseFile;
	api.PC_AddGlobalDefine = PC_AddGlobalDefine;
	api.PC_LoadSourceHandle = PC_LoadSourceHandle;
	api.PC_FreeSourceHandle = PC_FreeSourceHandle;
	api.PC_ReadTokenHandle = PC_ReadTokenHandle;
	api.PC_SourceFileAndLine = PC_SourceFileAndLine;
	api.PC_FreeSourceHandles = PC_FreeSourceHandles;
	botlib_export = &api;
	SetupVM();
	vm.currentlyInterpreting = qtrue;
	uivm = &vm;

	/* a global define a module adds is seen by every file it loads after */
	phase = "global define";
	Expect( Fixture_PCAddGlobalDefine( "MISSIONPACK" ), "the UI adds a global define" );

	for ( i = 0; i < (int)( sizeof( corpus ) / sizeof( corpus[0] ) ); i++ ) {
		CompareWithBotlib( corpus[i] );
	}
	CheckTokens();

	phase = "Team Arena UI menu load";
	Expect( Fixture_UILoadMenus() == 0, "the Team Arena UI loads the corpus menus" );

	/* the UI loaded its menus and freed every handle; shutting it down as a
	   faulted QVM (no call back into it) must leave nothing for it to free */
	phase = "shutdown";
	image = vm.dataBase;
	vm.interpretFaulted = qtrue;
	vm.currentlyInterpreting = qfalse;
	CL_ShutdownUI();
	Expect( uivm == NULL, "the UI VM is gone" );
	free( image );
	for ( i = 1; i < MAX_SOURCE_HANDLES; i++ ) {
		Expect( !PC_SourceFileAndLine( i, file, &line ), "no preprocessor handle is left open" );
	}
	PC_RemoveAllGlobalDefines();
	Expect( numtokens == 0, "every live token is released" );
	Expect( liveBlocks == 0, "every source, define and script buffer is released" );
	for ( i = 1; i < MAX_FIXTURE_FILES; i++ ) {
		Expect( !openFiles[i].text, "every file is closed" );
	}
	Expect( botlibMessages == 0, "botlib reports nothing" );

	if ( failures ) {
		fprintf( stderr, "%d Team Arena menu parser check(s) failed\n", failures );
		return 1;
	}
	puts( "Team Arena menus read through the UI traps parse as botlib parses them (issue #11)" );
	return 0;
}
