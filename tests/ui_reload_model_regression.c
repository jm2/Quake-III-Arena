/* Issue #49: the Team Arena UI's menu reload and player model cvars.
 * UI_Load (the ui_load command) reactivated the focused menu by a name it
 * copied only when a menu had focus, so with no focused menu it read an
 * uninitialized buffer; it now reactivates a menu only when it captured a name.
 * The player and opponent model views copied the model, headmodel,
 * team_model, team_headmodel, ui_teamName and ui_opponentModel cvars into
 * 64- and 256-byte buffers with strcpy; a cvar holds up to 255 characters.
 * They are now copied with Q_strncpyz, as ioquake3 does: a longer name is cut
 * to the buffer, names no model on disk, and the view draws nothing, as for
 * any unknown model. Both were fixed by the port review (204fe36d); these
 * cases keep them fixed. */
#include "../code/ui/ui_main.c"

#include <stdio.h>

static void Check( int ok, const char *what );
#include "ui_menu_source_fixture.h"

/** Fail with a description of the first mismatch. */
static void Check( int ok, const char *what ) {
	if ( !ok ) {
		fprintf( stderr, "UI reload and model cvar regression failed: %s\n", what );
		exit( 1 );
	}
}

/* --- engine stand-ins --- */

#define SERVED_CVARS 8
static const char *cvarName[SERVED_CVARS];
static const char *cvarValue[SERVED_CVARS];

/** Serve value as the cvar name (empty when it is not served). */
static void ServeCvar( int index, const char *name, const char *value ) {
	Check( index >= 0 && index < SERVED_CVARS, "cvar slot" );
	cvarName[index] = name;
	cvarValue[index] = value;
}
static const char *CvarValue( const char *name ) {
	int i;
	for ( i = 0; i < SERVED_CVARS; i++ ) {
		if ( cvarName[i] && !strcmp( cvarName[i], name ) ) {
			return cvarValue[i];
		}
	}
	return "";
}
/** The engine copies at most bufsize - 1 characters of the cvar. */
void trap_Cvar_VariableStringBuffer( const char *var_name, char *buffer, int bufsize ) {
	Q_strncpyz( buffer, CvarValue( var_name ), bufsize );
}
float trap_Cvar_VariableValue( const char *var_name ) {
	return atof( CvarValue( var_name ) );
}
void trap_Cvar_Register( vmCvar_t *cvar, const char *var_name, const char *value, int flags ) {
	(void)var_name; (void)value; (void)flags;
	memset( cvar, 0, sizeof( *cvar ) );
}

#define RECORDED_NAMES 64
static char recordedModels[RECORDED_NAMES][MAX_QPATH * 2];
static char recordedSkins[RECORDED_NAMES][MAX_QPATH * 2];
static int recordedModelCount, recordedSkinCount;

/** Every model name the view asks for, as the renderer's lookup reads it. */
qhandle_t trap_R_RegisterModel( const char *name ) {
	Check( strlen( name ) < MAX_QPATH, "model name fits MAX_QPATH" );
	if ( recordedModelCount < RECORDED_NAMES ) {
		Q_strncpyz( recordedModels[recordedModelCount++], name, sizeof( recordedModels[0] ) );
	}
	return 1;
}
/** Every skin name the view asks for. */
qhandle_t trap_R_RegisterSkin( const char *name ) {
	Check( strlen( name ) < MAX_QPATH * 2, "skin name fits its buffer" );
	if ( recordedSkinCount < RECORDED_NAMES ) {
		Q_strncpyz( recordedSkins[recordedSkinCount++], name, sizeof( recordedSkins[0] ) );
	}
	return 1;
}
/** No file exists: no head skin and no animation.cfg, so the view draws nothing. */
int trap_FS_FOpenFile( const char *qpath, fileHandle_t *f, fsMode_t mode ) {
	(void)qpath; (void)mode;
	if ( f ) {
		*f = 0;
	}
	return -1;
}
void trap_FS_FCloseFile( fileHandle_t f ) { (void)f; }
void trap_FS_Read( void *buffer, int len, fileHandle_t f ) { (void)buffer; (void)len; (void)f; Check( 0, "no file read" ); }
int trap_FS_GetFileList( const char *path, const char *extension, char *listbuf, int bufsize ) {
	(void)path; (void)extension; (void)listbuf; (void)bufsize;
	return 0;
}
int trap_Milliseconds( void ) { return 0; }
void trap_Print( const char *string ) { (void)string; }
void trap_Error( const char *string ) {
	fprintf( stderr, "Unexpected trap_Error: %s\n", string );
	exit( 1 );
}
void QDECL Com_Printf( const char *msg, ... ) { (void)msg; }
void QDECL Com_Error( int level, const char *error, ... ) {
	(void)level;
	fprintf( stderr, "Unexpected Com_Error: %s\n", error );
	exit( 1 );
}
/** ui_atoms.c: the cvar as the engine returns it. */
char *UI_Cvar_VariableString( const char *var_name ) {
	static char buffer[MAX_STRING_CHARS];
	trap_Cvar_VariableStringBuffer( var_name, buffer, sizeof( buffer ) );
	return buffer;
}
/** ui_gameinfo.c: no arenas. */
void UI_LoadArenas( void ) {
	uiInfo.mapCount = 0;
}
qhandle_t trap_R_RegisterShaderNoMip( const char *name ) { (void)name; return 1; }
sfxHandle_t trap_S_RegisterSound( const char *sample, qboolean compressed ) { (void)sample; (void)compressed; return 1; }
void trap_R_RegisterFont( const char *fontName, int pointSize, fontInfo_t *font ) {
	(void)fontName; (void)pointSize; (void)font;
}
/* Nothing is drawn: no model loads its animation.cfg, so UI_DrawPlayer returns
 * before the scene calls below. */
void UI_AdjustFrom640( float *x, float *y, float *w, float *h ) { (void)x; (void)y; (void)w; (void)h; Check( 0, "nothing drawn" ); }
void trap_S_StartLocalSound( sfxHandle_t sfx, int channelNum ) { (void)sfx; (void)channelNum; Check( 0, "nothing drawn" ); }
void trap_R_ClearScene( void ) { Check( 0, "nothing drawn" ); }
void trap_R_AddRefEntityToScene( const refEntity_t *re ) { (void)re; Check( 0, "nothing drawn" ); }
void trap_R_AddLightToScene( const vec3_t org, float intensity, float r, float g, float b ) {
	(void)org; (void)intensity; (void)r; (void)g; (void)b;
	Check( 0, "nothing drawn" );
}
void trap_R_RenderScene( const refdef_t *fd ) { (void)fd; Check( 0, "nothing drawn" ); }
int trap_CM_LerpTag( orientation_t *tag, clipHandle_t mod, int startFrame, int endFrame, float frac, const char *tagName ) {
	(void)tag; (void)mod; (void)startFrame; (void)endFrame; (void)frac; (void)tagName;
	Check( 0, "nothing drawn" );
	return 0;
}
static void QDECL TestPrint( const char *msg, ... ) { (void)msg; }
static void TestStopCinematic( int handle ) { (void)handle; }

/* --- menu reload --- */

/** Load menus "x" and "second" from ui/menus.txt into reset pools. */
static void LoadTwoMenus( void ) {
	MenuSource_File( 0, "ui/menus.txt", "{ loadMenu { \"ui/two.menu\" } }" );
	MenuSource_File( 1, "ui/two.menu",
		"{ menuDef { name \"x\" rect 0 0 640 480 } menuDef { name \"second\" rect 0 0 640 480 } }" );
	ServeCvar( 0, "ui_menuFiles", "ui/menus.txt" );
	String_Init();
	UI_LoadMenus( "ui/menus.txt", qtrue );
	Check( Menu_Count() == 2, "two menus loaded" );
}

/** The menu that has focus after a reload, or NULL. */
static const char *FocusedName( void ) {
	menuDef_t *menu = Menu_GetFocused();
	return menu ? menu->window.name : NULL;
}

/** ui_load with a focused menu reloads the set and gives that menu focus again. */
static void TestReloadFocused( void ) {
	LoadTwoMenus();
	Check( Menus_ActivateByName( "second" ) != NULL, "second menu opened" );
	Check( FocusedName() && !strcmp( FocusedName(), "second" ), "second menu focused" );
	UI_Load();
	Check( Menu_Count() == 2, "menus reloaded" );
	Check( FocusedName() && !strcmp( FocusedName(), "second" ), "focused menu reactivated after reload" );
}

/** Leave the stack below the caller holding "x" at every even offset, the
 * name an uninitialized name buffer of UI_Load would then hold. */
static void __attribute__((noinline)) DirtyStack( void ) {
	volatile char stale[8192];
	int i;
	for ( i = 0; i < (int)sizeof( stale ); i++ ) {
		stale[i] = ( i & 1 ) ? 0 : 'x';
	}
}

/** ui_load with no focused menu reloads the set and opens nothing, whatever
 * its name buffer held before. */
static void TestReloadUnfocused( void ) {
	LoadTwoMenus();
	Menus_CloseAll();
	Check( FocusedName() == NULL, "no menu focused" );
	DirtyStack();
	UI_Load();
	Check( Menu_Count() == 2, "menus reloaded" );
	Check( FocusedName() == NULL, "no menu reactivated after reload" );
	Check( !( Menus_FindByName( "x" )->window.flags & WINDOW_VISIBLE )
		&& !( Menus_FindByName( "second" )->window.flags & WINDOW_VISIBLE ), "every reloaded menu closed" );
}

/* --- player model cvars --- */

/** length copies of c. */
static const char *Repeat( char c, int length ) {
	static char text[6][MAX_CVAR_VALUE_STRING];
	static int next;
	char *s = text[next++ % 6];

	Check( length < MAX_CVAR_VALUE_STRING, "cvar value fits" );
	memset( s, c, length );
	s[length] = 0;
	return s;
}

/** The first model and skin names the view asks for are those of model, head
 * and team cut to the view's buffers (64, 64 and 256 bytes). */
static void CheckRegistered( const char *model, const char *head, const char *team ) {
	char name[MAX_QPATH * 2];
	char cut[MAX_QPATH];
	char headCut[MAX_QPATH];
	char teamCut[256];

	Q_strncpyz( cut, model, sizeof( cut ) );
	Q_strncpyz( headCut, head, sizeof( headCut ) );
	Q_strncpyz( teamCut, team, sizeof( teamCut ) );
	Check( recordedModelCount >= 3 && recordedSkinCount >= 2, "model and skins asked for" );
	Com_sprintf( name, MAX_QPATH, "models/players/%s/lower.md3", cut );
	Check( !strcmp( recordedModels[0], name ), "legs model named by the cut model cvar" );
	Com_sprintf( name, MAX_QPATH, "models/players/%s/head.md3", headCut );
	Check( !strcmp( recordedModels[2], name ), "head model named by the cut headmodel cvar" );
	if ( *teamCut ) {
		Com_sprintf( name, sizeof( name ), "models/players/%s/%s/lower_default.skin", cut, teamCut );
	} else {
		Com_sprintf( name, sizeof( name ), "models/players/%s/lower_default.skin", cut );
	}
	Check( !strcmp( recordedSkins[0], name ), "legs skin named by the cut model and team cvars" );
}

/** Draw the Q3 model view, the Team Arena model view and the opponent with
 * every model, head and team cvar length characters long. */
static void TestModelCvars( int length ) {
	rectDef_t rect = { 0, 0, 100, 100 };
	const char *model = Repeat( 'm', length );
	const char *head = Repeat( 'h', length );
	const char *teamModel = Repeat( 'M', length );
	const char *teamHead = Repeat( 'H', length );
	const char *team = Repeat( 't', length );
	const char *opponent = Repeat( 'o', length );

	ServeCvar( 0, "model", model );
	ServeCvar( 1, "headmodel", head );
	ServeCvar( 2, "team_model", teamModel );
	ServeCvar( 3, "team_headmodel", teamHead );
	ServeCvar( 4, "ui_teamName", team );
	ServeCvar( 5, "ui_opponentModel", opponent );

	ServeCvar( 6, "ui_Q3Model", "1" );
	recordedModelCount = recordedSkinCount = 0;
	UI_DrawPlayerModel( &rect );
	CheckRegistered( model, head, "" );

	ServeCvar( 6, "ui_Q3Model", "0" );
	recordedModelCount = recordedSkinCount = 0;
	UI_DrawPlayerModel( &rect );
	CheckRegistered( teamModel, teamHead, team );

	recordedModelCount = recordedSkinCount = 0;
	updateOpponentModel = qtrue;
	UI_DrawOpponent( &rect );
	CheckRegistered( opponent, opponent, "" );
}

/** One case per process, named by the argument, so each fails on its own. */
int main( int argc, char **argv ) {
	const char *test = argc > 1 ? argv[1] : "";

	memset( &uiInfo, 0, sizeof( uiInfo ) );
	uiInfo.uiDC.Print = TestPrint;
	uiInfo.uiDC.stopCinematic = TestStopCinematic;
	Init_Display( &uiInfo.uiDC );
	if ( !strcmp( test, "reload-focused" ) ) {
		TestReloadFocused();
	} else if ( !strcmp( test, "reload-unfocused" ) ) {
		TestReloadUnfocused();
	} else {
		Check( !strncmp( test, "cvar", 4 ), "known case" );
		TestModelCvars( atoi( test + 4 ) );
	}
	return 0;
}
