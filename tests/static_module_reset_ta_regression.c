/*
 * Issue #459: retail 1.32c built code/ui/ui_shared.c into both Team Arena
 * QVMs, the cgame (with -DCGAME) and the ui, so each module had its own
 * menus, string pool, memory pool and display context. The static Team Arena
 * build linked one copy for both, so CG_Init's String_Init, Init_Display and
 * Menu_Reset (loading the HUD) wiped the ui's menus and string pool and
 * pointed the ui's menu code at the cgame's display context, and a ui load
 * did the same to the HUD. The cgame copy also used the ui's traps (#234).
 *
 * Now the cgame compiles its own ui_shared.c, its globals renamed by
 * code/ui/ui_shared_cgame.h, and each copy lies in its own module's reset
 * bracket (#457). The runner builds the Team Arena cgame and ui from their
 * real sources the way CMakeLists.txt does, links them with each module's
 * data and bss bracketed, and this test drives them through the real vmMain
 * entry points against a stub engine that serves a few menu files. It checks:
 *  - the ui's menus, strings and display context survive CG_Init's HUD load;
 *  - the HUD menus survive a ui load (the reverse order);
 *  - each module has its own copy of ui_shared.c's code and data, inside its
 *    own module's brackets, and each copy reads menu scripts with its own
 *    module's traps;
 *  - the reset of one module (Sys_LoadDll) clears that module's menus only.
 *
 * The runner compiles this file once per part: Q3_TEST_CGAME and Q3_TEST_UI
 * hold the stub traps of each module, and the default part holds the stub
 * engine and the checks.
 */

/* --- stub engine shared by every part --- */
void		Q3T_Fail( const char *fmt, ... );
void		Q3T_DefaultTrap( const char *name );
int			Q3T_CvarIndex( const char *name, const char *defaultValue );
const char	*Q3T_CvarString( int index );
int			Q3T_CvarModified( int index );
void		Q3T_CvarSet( const char *name, const char *value );
const char	*Q3T_CvarGet( const char *name );
const char	*Q3T_ConfigstringGet( int index );
int			Q3T_Handle( const char *name );
int			Q3T_Milliseconds( void );
const char	*Q3T_File( const char *name );
int			Q3T_PC_LoadSource( const char *name, int module );
int			Q3T_PC_FreeSource( int handle );
int			Q3T_PC_ReadToken( int handle, void *pc_token, int module );

/* --- what the module parts offer the checks --- */
void		Q3CGame_Init( void );
int			Q3CGame_MenuCount( void );
const char	*Q3CGame_MenuName( const char *name );
const char	*Q3CGame_StringAlloc( const char *s );
int			Q3CGame_OwnContext( void );
void		*Q3CGame_Menus( void );
void		*Q3CGame_MenuNew( void );
void		Q3UI_Init( void );
int			Q3UI_MenuCount( void );
const char	*Q3UI_MenuName( const char *name );
const char	*Q3UI_ItemText( const char *menu, const char *item );
const char	*Q3UI_StringAlloc( const char *s );
int			Q3UI_OwnContext( void );
void		*Q3UI_Menus( void );
void		*Q3UI_MenuNew( void );

enum { MODULE_CGAME = 1, MODULE_UI };

#if defined( Q3_TEST_CGAME )
/* ======================================================================
 * cgame: stub traps for the Team Arena cgame, with the cgame's macros
 * ====================================================================== */
#include "../code/cgame/cg_local.h"
#include "../code/ui/ui_shared.h"
#include <stdlib.h>
#include <string.h>

int vmMain( int command, int arg0, int arg1, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7, int arg8, int arg9, int arg10, int arg11 );
extern menuDef_t Menus[];
extern displayContextDef_t cgDC;

static const char	*openFile;
static int			fileOffset;

void trap_Print( const char *fmt ) {}
void trap_Error( const char *fmt ) { Q3T_Fail( "cgame trap_Error: %s", fmt ); }
int trap_Milliseconds( void ) { return Q3T_Milliseconds(); }
void trap_Cvar_Register( vmCvar_t *vmCvar, const char *varName, const char *defaultValue, int flags ) {
	int i = Q3T_CvarIndex( varName, defaultValue );
	if ( vmCvar ) {
		vmCvar->handle = i;
		trap_Cvar_Update( vmCvar );
	}
}
void trap_Cvar_Update( vmCvar_t *vmCvar ) {
	vmCvar->modificationCount = Q3T_CvarModified( vmCvar->handle );
	Q_strncpyz( vmCvar->string, Q3T_CvarString( vmCvar->handle ), sizeof( vmCvar->string ) );
	vmCvar->value = atof( vmCvar->string );
	vmCvar->integer = atoi( vmCvar->string );
}
void trap_Cvar_Set( const char *name, const char *value ) { Q3T_CvarSet( name, value ); }
void trap_Cvar_VariableStringBuffer( const char *name, char *buffer, int size ) { Q_strncpyz( buffer, Q3T_CvarGet( name ), size ); }
int trap_FS_FOpenFile( const char *qpath, fileHandle_t *f, fsMode_t mode ) {
	const char *text = mode == FS_READ ? Q3T_File( qpath ) : NULL;
	if ( f ) {
		*f = text ? 1 : 0;
	}
	if ( text && f ) {
		openFile = text;
		fileOffset = 0;
	}
	return text ? (int)strlen( text ) : -1;
}
void trap_FS_Read( void *buffer, int len, fileHandle_t f ) { memcpy( buffer, openFile + fileOffset, len ); fileOffset += len; }
void trap_FS_FCloseFile( fileHandle_t f ) {}
sfxHandle_t trap_S_RegisterSound( const char *sample, qboolean compressed ) { return Q3T_Handle( sample ); }
qhandle_t trap_R_RegisterModel( const char *name ) { return Q3T_Handle( name ); }
qhandle_t trap_R_RegisterSkin( const char *name ) { return Q3T_Handle( name ); }
qhandle_t trap_R_RegisterShader( const char *name ) { return Q3T_Handle( name ); }
qhandle_t trap_R_RegisterShaderNoMip( const char *name ) { return Q3T_Handle( name ); }
void trap_GetGlconfig( glconfig_t *glconfig ) {
	memset( glconfig, 0, sizeof( *glconfig ) );
	glconfig->vidWidth = 640;
	glconfig->vidHeight = 480;
	glconfig->colorBits = 32;
}
void trap_GetGameState( gameState_t *gs ) {
	int i, len;
	memset( gs, 0, sizeof( *gs ) );
	gs->dataCount = 1;
	for ( i = 0; i < MAX_CONFIGSTRINGS; i++ ) {
		const char *s = Q3T_ConfigstringGet( i );
		if ( !s[0] ) {
			continue;
		}
		len = strlen( s );
		gs->stringOffsets[i] = gs->dataCount;
		memcpy( gs->stringData + gs->dataCount, s, len + 1 );
		gs->dataCount += len + 1;
	}
}
int trap_MemoryRemaining( void ) { return 8 << 20; }
/* the cgame's own script traps: botlib handles, read in the cgame's name */
int trap_PC_LoadSource( const char *filename ) { return Q3T_PC_LoadSource( filename, MODULE_CGAME ); }
int trap_PC_FreeSource( int handle ) { return Q3T_PC_FreeSource( handle ); }
int trap_PC_ReadToken( int handle, pc_token_t *pc_token ) { return Q3T_PC_ReadToken( handle, pc_token, MODULE_CGAME ); }
int trap_PC_SourceFileAndLine( int handle, char *filename, int *line ) { filename[0] = 0; *line = 0; return 0; }

void Q3CGame_Init( void ) { vmMain( CG_INIT, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 ); }
int Q3CGame_MenuCount( void ) { return Menu_Count(); }
const char *Q3CGame_MenuName( const char *name ) {
	menuDef_t *menu = Menus_FindByName( name );
	return menu ? menu->window.name : NULL;
}
const char *Q3CGame_StringAlloc( const char *s ) { return String_Alloc( s ); }
int Q3CGame_OwnContext( void ) { return Display_GetContext() == &cgDC; }
void *Q3CGame_Menus( void ) { return Menus; }
void *Q3CGame_MenuNew( void ) { return (void *)Menu_New; }

#elif defined( Q3_TEST_UI )
/* ======================================================================
 * ui: stub traps for the Team Arena ui, with the ui's macros
 * ====================================================================== */
#include "../code/ui/ui_local.h"
#include <stdlib.h>
#include <string.h>

int vmMain( int command, int arg0, int arg1, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7, int arg8, int arg9, int arg10, int arg11 );
extern menuDef_t Menus[];
itemDef_t *Menu_FindItemByName( menuDef_t *menu, const char *p );

void trap_Print( const char *s ) {}
void trap_Error( const char *s ) { Q3T_Fail( "ui trap_Error: %s", s ); }
int trap_Milliseconds( void ) { return Q3T_Milliseconds(); }
void trap_Cvar_Register( vmCvar_t *vmCvar, const char *varName, const char *defaultValue, int flags ) {
	int i = Q3T_CvarIndex( varName, defaultValue );
	if ( vmCvar ) {
		vmCvar->handle = i;
		trap_Cvar_Update( vmCvar );
	}
}
void trap_Cvar_Update( vmCvar_t *vmCvar ) {
	vmCvar->modificationCount = Q3T_CvarModified( vmCvar->handle );
	Q_strncpyz( vmCvar->string, Q3T_CvarString( vmCvar->handle ), sizeof( vmCvar->string ) );
	vmCvar->value = atof( vmCvar->string );
	vmCvar->integer = atoi( vmCvar->string );
}
void trap_Cvar_Set( const char *name, const char *value ) { Q3T_CvarSet( name, value ); }
float trap_Cvar_VariableValue( const char *name ) { return atof( Q3T_CvarGet( name ) ); }
void trap_Cvar_VariableStringBuffer( const char *name, char *buffer, int size ) { Q_strncpyz( buffer, Q3T_CvarGet( name ), size ); }
void trap_Cvar_SetValue( const char *name, float value ) { Q3T_CvarSet( name, va( "%g", value ) ); }
void trap_Cvar_Create( const char *name, const char *value, int flags ) { Q3T_CvarIndex( name, value ); }
int trap_FS_FOpenFile( const char *qpath, fileHandle_t *f, fsMode_t mode ) { if ( f ) *f = 0; return -1; }
int trap_FS_GetFileList( const char *path, const char *extension, char *listbuf, int bufsize ) { if ( bufsize ) listbuf[0] = 0; return 0; }
qhandle_t trap_R_RegisterModel( const char *name ) { return Q3T_Handle( name ); }
qhandle_t trap_R_RegisterSkin( const char *name ) { return Q3T_Handle( name ); }
qhandle_t trap_R_RegisterShaderNoMip( const char *name ) { return Q3T_Handle( name ); }
sfxHandle_t trap_S_RegisterSound( const char *sample, qboolean compressed ) { return Q3T_Handle( sample ); }
void trap_GetGlconfig( glconfig_t *glconfig ) {
	memset( glconfig, 0, sizeof( *glconfig ) );
	glconfig->vidWidth = 640;
	glconfig->vidHeight = 480;
	glconfig->colorBits = 32;
}
void trap_GetClientState( uiClientState_t *state ) { memset( state, 0, sizeof( *state ) ); state->connState = CA_DISCONNECTED; }
int trap_GetConfigString( int index, char *buff, int buffsize ) { Q_strncpyz( buff, Q3T_ConfigstringGet( index ), buffsize ); return 1; }
int trap_MemoryRemaining( void ) { return 8 << 20; }
/* the ui's own script traps, read in the ui's name */
int trap_PC_LoadSource( const char *filename ) { return Q3T_PC_LoadSource( filename, MODULE_UI ); }
int trap_PC_FreeSource( int handle ) { return Q3T_PC_FreeSource( handle ); }
int trap_PC_ReadToken( int handle, pc_token_t *pc_token ) { return Q3T_PC_ReadToken( handle, pc_token, MODULE_UI ); }
int trap_PC_SourceFileAndLine( int handle, char *filename, int *line ) { filename[0] = 0; *line = 0; return 0; }

void Q3UI_Init( void ) { vmMain( UI_INIT, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 ); }
int Q3UI_MenuCount( void ) { return Menu_Count(); }
const char *Q3UI_MenuName( const char *name ) {
	menuDef_t *menu = Menus_FindByName( name );
	return menu ? menu->window.name : NULL;
}
const char *Q3UI_ItemText( const char *menu, const char *item ) {
	menuDef_t *m = Menus_FindByName( menu );
	itemDef_t *it = m ? Menu_FindItemByName( m, item ) : NULL;
	return it ? it->text : NULL;
}
const char *Q3UI_StringAlloc( const char *s ) { return String_Alloc( s ); }
int Q3UI_OwnContext( void ) { return Display_GetContext() == &uiInfo.uiDC; }
void *Q3UI_Menus( void ) { return Menus; }
void *Q3UI_MenuNew( void ) { return (void *)Menu_New; }

#else
/* ======================================================================
 * the stub engine and the checks
 * ====================================================================== */
#include "../code/game/q_shared.h"
#include "../code/game/bg_public.h"
#include "../code/qcommon/vm_static.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* the runner's linker script brackets each module as cmake/static_modules.py does */
extern unsigned char q3static_cgame_data_start[], q3static_cgame_data_end[];
extern unsigned char q3static_cgame_bss_start[], q3static_cgame_bss_end[];
extern unsigned char q3static_ui_data_start[], q3static_ui_data_end[];
extern unsigned char q3static_ui_bss_start[], q3static_ui_bss_end[];

static vmStaticModule_t modules[] = {
	{ "cgame", q3static_cgame_data_start, q3static_cgame_data_end,
		q3static_cgame_bss_start, q3static_cgame_bss_end },
	{ "ui", q3static_ui_data_start, q3static_ui_data_end,
		q3static_ui_bss_start, q3static_ui_bss_end },
};
#define MODULES		( (int)( sizeof( modules ) / sizeof( modules[0] ) ) )
enum { CGAME, UI };

static char		currentCase[256];
static int		checks;

void Q3T_Fail( const char *fmt, ... ) {
	va_list ap;
	fprintf( stderr, "Team Arena static module regression failed: %s: ", currentCase );
	va_start( ap, fmt );
	vfprintf( stderr, fmt, ap );
	va_end( ap );
	fputc( '\n', stderr );
	exit( 1 );
}

static void Check( int condition, const char *what ) {
	checks++;
	if ( !condition ) {
		Q3T_Fail( "%s", what );
	}
}

void Q3T_DefaultTrap( const char *name ) {}

/* --- engine state: cvars, handles, clock, files --- */
#define MAX_TEST_CVARS		1024
#define MAX_TEST_HANDLES	4096

static char		cvarNames[MAX_TEST_CVARS][64], cvarValues[MAX_TEST_CVARS][256];
static int		cvarModified[MAX_TEST_CVARS], numCvars = 1;
static char		handleNames[MAX_TEST_HANDLES][64];
static int		numHandles = 1, milliseconds;

int Q3T_CvarIndex( const char *name, const char *defaultValue ) {
	int i;
	for ( i = 1; i < numCvars; i++ ) {
		if ( !strcasecmp( cvarNames[i], name ) ) {
			return i;
		}
	}
	if ( numCvars == MAX_TEST_CVARS ) {
		Q3T_Fail( "too many cvars" );
	}
	snprintf( cvarNames[numCvars], sizeof( cvarNames[0] ), "%s", name );
	snprintf( cvarValues[numCvars], sizeof( cvarValues[0] ), "%s", defaultValue ? defaultValue : "" );
	cvarModified[numCvars] = 1;
	return numCvars++;
}
const char *Q3T_CvarString( int index ) { return index > 0 && index < numCvars ? cvarValues[index] : ""; }
int Q3T_CvarModified( int index ) { return index > 0 && index < numCvars ? cvarModified[index] : 0; }
void Q3T_CvarSet( const char *name, const char *value ) {
	int i = Q3T_CvarIndex( name, value );
	if ( strcmp( cvarValues[i], value ) ) {
		snprintf( cvarValues[i], sizeof( cvarValues[0] ), "%s", value );
		cvarModified[i]++;
	}
}
const char *Q3T_CvarGet( const char *name ) {
	int i;
	for ( i = 1; i < numCvars; i++ ) {
		if ( !strcasecmp( cvarNames[i], name ) ) {
			return cvarValues[i];
		}
	}
	return "";
}
/* The gamestate of a Team Arena server running testmap. */
const char *Q3T_ConfigstringGet( int index ) {
	switch ( index ) {
	case CS_SERVERINFO: return "\\mapname\\testmap\\g_gametype\\0\\sv_hostname\\ta\\sv_maxclients\\8";
	case CS_SYSTEMINFO: return "\\sv_serverid\\1234";
	case CS_GAME_VERSION: return GAME_VERSION;
	case CS_LEVEL_START_TIME: return "1000";
	default: return "";
	}
}
/* The renderer and sound system hand out one handle per name. */
int Q3T_Handle( const char *name ) {
	int i;
	for ( i = 1; i < numHandles; i++ ) {
		if ( !strcasecmp( handleNames[i], name ) ) {
			return i;
		}
	}
	if ( numHandles == MAX_TEST_HANDLES ) {
		Q3T_Fail( "too many handles" );
	}
	snprintf( handleNames[numHandles], sizeof( handleNames[0] ), "%s", name );
	return numHandles++;
}
int Q3T_Milliseconds( void ) { return ++milliseconds; }

/* The menu files, as in pak0.pk3 but small: the ui's main and in-game
   menus, and the cgame's HUD. */
static const char *files[][2] = {
	{ "ui/menus.txt", "{\nloadMenu { \"ui/main.menu\" }\n}\n" },
	{ "ui/ingame.txt", "{\nloadMenu { \"ui/ingame.menu\" }\n}\n" },
	{ "ui/main.menu",
	  "{\nmenuDef {\nname \"main\"\nfullScreen 1\nrect 0 0 640 480\nvisible 1\n"
	  "itemDef {\nname \"title\"\ntext \"Team Arena main menu\"\nrect 16 16 200 32\nvisible 1\n}\n}\n}\n" },
	{ "ui/ingame.menu",
	  "{\nmenuDef {\nname \"ingame\"\nrect 0 0 640 48\nvisible 0\n"
	  "itemDef {\nname \"leave\"\ntext \"Leave the arena\"\nrect 16 8 96 24\nvisible 1\n}\n}\n}\n" },
	{ "ui/hud.txt", "{\nloadMenu { \"ui/hud.menu\" }\n}\n" },
	{ "ui/hud.menu",
	  "{\nmenuDef {\nname \"hudScore\"\nrect 0 440 640 40\nvisible 1\n"
	  "itemDef {\nname \"score\"\ntext \"HUD score text that is not a ui string\"\nrect 8 8 64 24\nvisible 1\n}\n}\n"
	  "menuDef {\nname \"hudTeam\"\nrect 0 0 160 120\nvisible 1\n}\n}\n" },
};
#define NUM_FILES	( (int)( sizeof( files ) / sizeof( files[0] ) ) )

const char *Q3T_File( const char *name ) {
	int i;
	for ( i = 0; i < NUM_FILES; i++ ) {
		if ( !strcasecmp( files[i][0], name ) ) {
			return files[i][1];
		}
	}
	return NULL;
}

/* The script handles botlib's PC_* hand out, shared by every module, and
   which module read each source. */
#define MAX_SOURCES	16
static struct {
	const char	*text, *p;
	int			module;		// the module that loaded it
	int			readBy;		// the modules that read it (bit mask)
	char		name[64];
} sources[MAX_SOURCES];
static int		uiSourceReadByCGame, cgameSourceReadByUI;

int Q3T_PC_LoadSource( const char *name, int module ) {
	int i;
	const char *text = Q3T_File( name );
	if ( !text ) {
		return 0;
	}
	for ( i = 1; i < MAX_SOURCES; i++ ) {
		if ( !sources[i].text ) {
			sources[i].text = sources[i].p = text;
			sources[i].module = module;
			sources[i].readBy = 0;
			snprintf( sources[i].name, sizeof( sources[i].name ), "%s", name );
			return i;
		}
	}
	Q3T_Fail( "too many script sources" );
	return 0;
}
int Q3T_PC_FreeSource( int handle ) {
	if ( handle <= 0 || handle >= MAX_SOURCES || !sources[handle].text ) {
		Q3T_Fail( "PC_FreeSource of a bad handle %d", handle );
	}
	sources[handle].text = NULL;
	return 1;
}
/* What botlib's tokenizer returns for these files: strings without their
   quotes, numbers with their values, names and punctuation. */
int Q3T_PC_ReadToken( int handle, void *token, int module ) {
	pc_token_t *t = token;
	const char *p;
	int n = 0;

	if ( handle <= 0 || handle >= MAX_SOURCES || !sources[handle].text ) {
		Q3T_Fail( "PC_ReadToken of a bad handle %d", handle );
	}
	if ( module != sources[handle].module ) {
		if ( module == MODULE_CGAME ) {
			uiSourceReadByCGame++;
		} else {
			cgameSourceReadByUI++;
		}
	}
	memset( t, 0, sizeof( *t ) );
	p = sources[handle].p;
	while ( *p && *p <= ' ' ) {
		p++;
	}
	if ( !*p ) {
		sources[handle].p = p;
		return 0;
	}
	if ( *p == '"' ) {
		p++;
		while ( *p && *p != '"' && n < MAX_TOKENLENGTH - 1 ) {
			t->string[n++] = *p++;
		}
		if ( *p == '"' ) {
			p++;
		}
		t->type = TT_STRING;
	} else if ( *p == '{' || *p == '}' ) {
		t->string[n++] = *p++;
		t->type = TT_PUNCTUATION;
	} else {
		while ( *p > ' ' && *p != '{' && *p != '}' && *p != '"' && n < MAX_TOKENLENGTH - 1 ) {
			t->string[n++] = *p++;
		}
		if ( t->string[0] >= '0' && t->string[0] <= '9' ) {
			t->type = TT_NUMBER;
			t->intvalue = atoi( t->string );
			t->floatvalue = atof( t->string );
		} else {
			t->type = TT_NAME;
		}
	}
	t->string[n] = 0;
	sources[handle].p = p;
	return 1;
}

/* engine functions the hard-linked modules call directly */
void QDECL Com_Error( int level, const char *fmt, ... ) {
	char text[1024];
	va_list ap;
	va_start( ap, fmt );
	vsnprintf( text, sizeof( text ), fmt, ap );
	va_end( ap );
	Q3T_Fail( "Com_Error( %d, %s )", level, text );
}
void QDECL Com_Printf( const char *fmt, ... ) {}
void Sys_SnapVector( float *v ) {
	int i;
	for ( i = 0; i < 3; i++ ) {
		v[i] = (float)(int)( v[i] < 0 ? v[i] - 0.5f : v[i] + 0.5f );
	}
}

/* --- loading modules the way Sys_LoadDll and Sys_UnloadDll do --- */
static void Load( int m ) {
	const char *error;
	VM_UnloadStaticModule( &modules[m] );
	error = VM_LoadStaticModule( &modules[m] );
	if ( error ) {
		Q3T_Fail( "VM_LoadStaticModule( %s ): %s", modules[m].name, error );
	}
}

static int InBracket( const void *p, int m ) {
	const unsigned char *c = p;
	return ( c >= modules[m].dataStart && c < modules[m].dataEnd ) ||
		( c >= modules[m].bssStart && c < modules[m].bssEnd );
}

static void Case( const char *name ) {
	snprintf( currentCase, sizeof( currentCase ), "%s", name );
}

/* The ui's menus, strings and display context, as UI_Init left them. */
static void CheckUIMenus( const char *mainName ) {
	const char *name = Q3UI_MenuName( "main" ), *text = Q3UI_ItemText( "main", "title" );
	Check( Q3UI_MenuCount() == 2, "the ui still has its two menus" );
	Check( name != NULL, "the ui still finds its main menu" );
	Check( Q3UI_MenuName( "ingame" ) != NULL, "the ui still finds its in-game menu (ESC)" );
	Check( !Q3UI_MenuName( "hudScore" ), "the ui's menus hold no HUD menu" );
	Check( name == mainName && !strcmp( name, "main" ), "the ui's menu name string is intact" );
	Check( text && !strcmp( text, "Team Arena main menu" ), "the ui's item text string is intact" );
	Check( Q3UI_StringAlloc( "main" ) == mainName, "the ui's string pool still finds its strings" );
	Check( Q3UI_OwnContext(), "the ui's menu code still draws through the ui's display context" );
}

/* The cgame's HUD menus, as CG_Init left them. */
static void CheckHUDMenus( const char *hudName ) {
	const char *name = Q3CGame_MenuName( "hudScore" );
	Check( Q3CGame_MenuCount() == 2, "the cgame still has its two HUD menus" );
	Check( name != NULL && Q3CGame_MenuName( "hudTeam" ) != NULL, "the cgame still finds its HUD menus" );
	Check( !Q3CGame_MenuName( "main" ) && !Q3CGame_MenuName( "ingame" ), "the cgame's menus hold no ui menu" );
	Check( name == hudName && !strcmp( name, "hudScore" ), "the cgame's menu name string is intact" );
	Check( Q3CGame_StringAlloc( "hudScore" ) == hudName, "the cgame's string pool still finds its strings" );
	Check( Q3CGame_OwnContext(), "the cgame's menu code still draws through the cgame's display context" );
}

int main( void ) {
	const char *error, *mainName, *hudName;

	Case( "startup" );
	error = VM_InitStaticModules( modules, MODULES );
	if ( error ) {
		Q3T_Fail( "VM_InitStaticModules: %s", error );
	}

	/* each module has its own ui_shared.c, inside its own brackets */
	Case( "the ui_shared.c copies" );
	Check( Q3CGame_Menus() != Q3UI_Menus(), "the cgame and the ui have their own Menus[]" );
	Check( Q3CGame_MenuNew() != Q3UI_MenuNew(), "the cgame and the ui have their own Menu_New" );
	Check( InBracket( Q3CGame_Menus(), CGAME ) && !InBracket( Q3CGame_Menus(), UI ),
		"the cgame's Menus[] is inside the cgame brackets" );
	Check( InBracket( Q3UI_Menus(), UI ) && !InBracket( Q3UI_Menus(), CGAME ),
		"the ui's Menus[] is inside the ui brackets" );

	/* the ui first (the main menu), then a map load (CG_Init loads the HUD) */
	Case( "the ui's menus across CG_Init" );
	Load( UI );
	Q3UI_Init();
	mainName = Q3UI_MenuName( "main" );
	CheckUIMenus( mainName );
	Load( CGAME );
	Q3CGame_Init();
	hudName = Q3CGame_MenuName( "hudScore" );
	CheckHUDMenus( hudName );
	CheckUIMenus( mainName );

	/* the reverse: a ui load (vid_restart, disconnect) during a game */
	Case( "the HUD menus across a ui load" );
	Load( UI );
	Q3UI_Init();
	mainName = Q3UI_MenuName( "main" );
	CheckUIMenus( mainName );
	CheckHUDMenus( hudName );

	Case( "the script traps" );
	Check( !uiSourceReadByCGame && !cgameSourceReadByUI,
		"each module's menu code reads only the scripts its own module opened" );

	/* Sys_LoadDll's reset of one module leaves the other's menus alone */
	Case( "the cgame reset" );
	Load( CGAME );
	Check( Q3CGame_MenuCount() == 0, "a cgame load starts with no HUD menus, as a fresh QVM did" );
	CheckUIMenus( mainName );
	Q3CGame_Init();
	hudName = Q3CGame_MenuName( "hudScore" );
	CheckHUDMenus( hudName );
	Case( "the ui reset" );
	Load( UI );
	Check( Q3UI_MenuCount() == 0, "a ui load starts with no menus, as a fresh QVM did" );
	CheckHUDMenus( hudName );

	printf( "Team Arena static module regression passed (%d checks)\n", checks );
	return 0;
}
#endif
