/* Issue #11: the Team Arena UI side of ui_menu_parser_regression.c.  It is the
   real ui_main.c (UI_LoadMenus, Load_Menu, UI_ParseMenu, Asset_Parse) over the
   real ui_shared.c (Menu_New and the menu and item keyword parsers), built as
   the UI module (MISSIONPACK).  Its trap_PC_* calls go through the engine's
   real UI dispatcher in the other translation unit, as a QVM's would.  The
   runner renames the module's Com_Printf and Com_Error, so the parse errors
   PC_SourceError reports are counted here. */
#include "../code/ui/ui_main.c"
#include <stdarg.h>

itemDef_t *Menu_FindItemByName( menuDef_t *menu, const char *p );	/* ui_shared.c */
int Fixture_PCAddGlobalDefine( const char *define );
int Fixture_PCLoadSource( const char *name );
int Fixture_PCFreeSource( int handle );
int Fixture_PCReadToken( int handle, pc_token_t *token );
int Fixture_PCSourceFileAndLine( int handle, char *filename, int *line );

static int uiFailures, uiMessages;

static void UIExpect( int ok, const char *message ) {
	if ( !ok ) {
		fprintf( stderr, "FAIL [Team Arena UI]: %s\n", message );
		uiFailures++;
	}
}

/* --- the module's traps --------------------------------------------------- */
int trap_PC_AddGlobalDefine( char *define ) { return Fixture_PCAddGlobalDefine( define ); }
int trap_PC_LoadSource( const char *filename ) { return Fixture_PCLoadSource( filename ); }
int trap_PC_FreeSource( int handle ) { return Fixture_PCFreeSource( handle ); }
int trap_PC_ReadToken( int handle, pc_token_t *pc_token ) { return Fixture_PCReadToken( handle, pc_token ); }
int trap_PC_SourceFileAndLine( int handle, char *filename, int *line ) {
	return Fixture_PCSourceFileAndLine( handle, filename, line );
}
int trap_Milliseconds( void ) { return 0; }
qhandle_t trap_R_RegisterShaderNoMip( const char *name ) { return name[0] ? 1 : 0; }
qhandle_t trap_R_RegisterModel( const char *name ) { return name[0] ? 1 : 0; }
sfxHandle_t trap_S_RegisterSound( const char *sample, qboolean compressed ) { (void)compressed; return sample[0] ? 1 : 0; }
void trap_R_RegisterFont( const char *fontName, int pointSize, fontInfo_t *font ) {
	memset( font, 0, sizeof( *font ) );
	Q_strncpyz( font->name, fontName, sizeof( font->name ) );
	font->glyphScale = 48.0f / pointSize;
}
void trap_Print( const char *string ) { uiMessages++; fprintf( stderr, "UI: %s", string ); }
void trap_Error( const char *string ) { fprintf( stderr, "UI error: %s", string ); exit( 1 ); }

/* PC_SourceError and PC_SourceWarning report through these */
void QDECL UI_Com_Printf( const char *msg, ... ) {
	char text[1024];
	va_list args;

	va_start( args, msg );
	Q_vsnprintf( text, sizeof( text ), msg, args );
	va_end( args );
	if ( strstr( text, "ERROR" ) || strstr( text, "WARNING" ) ) {
		uiMessages++;
		fprintf( stderr, "UI: %s", text );
	}
}
void QDECL UI_Com_Error( int level, const char *error, ... ) {
	(void)level;
	fprintf( stderr, "UI Com_Error: %s\n", error );
	exit( 1 );
}

/* --- what the corpus spells ---------------------------------------------- */
static itemDef_t *Item( menuDef_t *menu, const char *name ) {
	itemDef_t *item = menu ? Menu_FindItemByName( menu, name ) : NULL;

	UIExpect( item != NULL, va( "item %s is loaded", name ) );
	return item;
}
static int Rect( const rectDef_t *r, float x, float y, float w, float h ) {
	return r->x == x && r->y == y && r->w == w && r->h == h;
}
static int Color( const vec4_t c, float r, float g, float b, float a ) {
	return c[0] == r && c[1] == g && c[2] == b && c[3] == a;
}

static void CheckMain( void ) {
	menuDef_t *menu = Menus_FindByName( "main" );
	itemDef_t *item;

	UIExpect( menu != NULL, "the main menu is loaded" );
	if ( !menu ) return;
	UIExpect( menu->fullScreen == MENU_TRUE && ( menu->window.flags & WINDOW_VISIBLE ), "MENU_TRUE" );
	UIExpect( Rect( &menu->window.rect, 0, 0, 640, 480 ), "the main menu's rect" );
	UIExpect( Color( menu->focusColor, 1, .75f, 0, 1 ), "the main menu's focus color" );
	UIExpect( menu->itemCount == 3, "the main menu's items" );
	UIExpect( menu->onOpen && strstr( menu->onOpen, "\"fadeout\" \"fadebox\" ; " ), "the main menu's onOpen script" );
	if ( ( item = Item( menu, "mappreview" ) ) != NULL ) {
		UIExpect( item->window.ownerDraw == UI_STARTMAPCINEMATIC &&
			item->window.style == WINDOW_STYLE_FILLED && ( item->window.flags & WINDOW_DECORATION ),
			"the cinematic owner draw" );
	}
	if ( ( item = Item( menu, "fight" ) ) != NULL ) {
		UIExpect( item->type == ITEM_TYPE_BUTTON && item->textStyle == ITEM_TEXTSTYLE_SHADOWEDMORE &&
			item->textalignment == ITEM_ALIGN_CENTER, "the fight button's menudef.h values" );
		UIExpect( Rect( &item->window.rectClient, 410, 320, 128, 26 ) && item->textalignx == 64 &&
			item->textaligny == 20 && item->textscale == .416f, "the fight button's numbers" );
		UIExpect( item->text && !strcmp( item->text, "FIGHT" ), "the fight button's text" );
		UIExpect( item->action && strstr( item->action, "\"open\" \"fight_menu\" " ), "the fight button's action" );
	}
	UIExpect( uiInfo.uiDC.Assets.fontRegistered && uiInfo.uiDC.Assets.fadeCycle == 1 &&
		uiInfo.uiDC.Assets.fadeAmount == 0.1f && uiInfo.uiDC.Assets.shadowColor[3] == 0.25f &&
		uiInfo.uiDC.Assets.cursorStr && !strcmp( uiInfo.uiDC.Assets.cursorStr, "ui/assets/3_cursor3" ),
		"assetGlobalDef" );
}

static void CheckSetup( void ) {
	menuDef_t *menu = Menus_FindByName( "setup_menu" );
	itemDef_t *item;
	editFieldDef_t *field;
	multiDef_t *multi;
	listBoxDef_t *list;

	UIExpect( menu != NULL, "the setup menu is loaded" );
	if ( !menu ) return;
	/* SETUP_X and SETUP_W from the included corpus.h */
	UIExpect( Rect( &menu->window.rect, 80, 40, 480, 400 ) && !menu->fullScreen &&
		!( menu->window.flags & WINDOW_VISIBLE ) && ( menu->window.flags & WINDOW_POPUP ) &&
		menu->window.border == WINDOW_BORDER_FULL, "the setup menu's macros" );
	UIExpect( menu->itemCount == 4, "the setup menu's items" );
	if ( ( item = Item( menu, "sensitivity" ) ) != NULL ) {	/* BUTTON(sensitivity, 60) */
		field = (editFieldDef_t *)item->typeData;
		UIExpect( Rect( &item->window.rectClient, 80, 60, 480, 20 ) && item->type == ITEM_TYPE_SLIDER,
			"a function-like macro's item" );
		UIExpect( field && field->defVal == 5 && field->minVal == -1 && field->maxVal == 30.5f,
			"cvarFloat's signed limits" );
		UIExpect( item->textalignx == -12 && item->textaligny == 17 && item->textalignment == ITEM_ALIGN_RIGHT,
			"a negative text offset" );
	}
	if ( ( item = Item( menu, "quality" ) ) != NULL ) {
		multi = (multiDef_t *)item->typeData;
		UIExpect( item->textalignx == 64, "a hex number" );
		UIExpect( multi && multi->count == 4 && !multi->strDef && multi->cvarValue[2] == -1 &&
			multi->cvarValue[3] == -2.5f && !strcmp( multi->cvarList[0], "High Quality" ), "cvarFloatList" );
	}
	if ( ( item = Item( menu, "team" ) ) != NULL ) {
		multi = (multiDef_t *)item->typeData;
		UIExpect( multi && multi->count == 2 && multi->strDef && !strcmp( multi->cvarList[1], "Stroggs" ) &&
			!strcmp( multi->cvarStr[1], "stroggs" ), "cvarStrList" );
	}
	if ( ( item = Item( menu, "demolist" ) ) != NULL ) {
		list = (listBoxDef_t *)item->typeData;
		UIExpect( item->special == FEEDER_DEMOS && list && list->elementWidth == 120 &&
			list->numColumns == 2 && list->columnInfo[1].pos == -42, "the list box" );
		UIExpect( Color( item->window.foreColor, 1, .75f, 0, 1 ), "#ifdef of the UI's global define" );
		UIExpect( item->window.border == WINDOW_BORDER_HORZ, "#if arithmetic over macros" );
		UIExpect( list && list->doubleClick && strstr( list->doubleClick, "LoadDemos" ), "the list's script" );
	}
}

static void CheckPlayer( void ) {
	menuDef_t *menu = Menus_FindByName( "player_menu" );
	itemDef_t *item;

	UIExpect( menu != NULL, "the player menu is loaded" );
	if ( !menu ) return;
	UIExpect( menu->itemCount == 2, "the player menu's items" );
	if ( ( item = Item( menu, "namefield" ) ) != NULL ) {
		UIExpect( item->text && !strcmp( item->text, "Player Name:" ), "adjacent strings merge" );
		UIExpect( item->cvar && !strcmp( item->cvar, "name" ), "#undef and redefinition" );
		UIExpect( item->type == ITEM_TYPE_EDITFIELD && item->typeData &&
			( (editFieldDef_t *)item->typeData )->maxChars == 32, "the edit field" );
		UIExpect( item->action && strstr( item->action, "\"set ui_note \"quoted\"\" ; " ), "an escaped quote in a script" );
	}
	if ( ( item = Item( menu, "headmodel" ) ) != NULL ) {
		UIExpect( item->window.ownerDraw == UI_PLAYERMODEL && item->window.ownerDrawFlags == UI_SHOW_NETANYTEAMGAME &&
			Rect( &item->window.rectClient, 360, 30, -150.5f, 200 ), "the owner draw's numbers" );
	}
}

/* --- load the corpus's ui/menus.txt the way _UI_Init loads ui/menus.txt --- */
int Fixture_UILoadMenus( void ) {
	uiInfo.uiDC.registerShaderNoMip = &trap_R_RegisterShaderNoMip;
	uiInfo.uiDC.registerModel = &trap_R_RegisterModel;
	uiInfo.uiDC.registerSound = &trap_S_RegisterSound;
	Init_Display( &uiInfo.uiDC );
	String_Init();

	UI_LoadMenus( "ui/menus.txt", qtrue );
	UIExpect( Menu_Count() == 3, "every corpus menu is loaded" );
	CheckMain();
	CheckSetup();
	CheckPlayer();
	UIExpect( uiMessages == 0, "the UI reports no parse error or warning" );
	return uiFailures;
}
