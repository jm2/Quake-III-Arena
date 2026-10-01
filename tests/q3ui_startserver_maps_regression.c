/* Found reviewing issue #440's audit: the base q3_ui Create and Skirmish menus
 * (ui_startserver.c) store map names and game type bits in MAX_SERVERMAPS (64)
 * entry lists, but StartServer_Cache stored every arena and
 * StartServer_GametypeEvent every arena of the chosen game type. q3_ui keeps
 * up to MAX_ARENAS (1024) arenas, and one .arena file in any pk3 can hold
 * hundreds, so the lists ran past the end of s_startserver. The menus now keep
 * the first MAX_SERVERMAPS arenas, the same entries as before, and a build
 * script still precaches every arena's levelshot. */
#include "../code/q3_ui/ui_startserver.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** Fail with a description of the first mismatch. */
static void Check( int ok, const char *what ) {
	if ( !ok ) {
		fprintf( stderr, "q3_ui start server map list regression failed: %s\n", what );
		exit( 1 );
	}
}

#define ARENA_TYPES 5
static const char *const arenaTypes[ARENA_TYPES] = { "ffa", "team ffa", "tourney", "ctf", "single" };
static char arenaInfos[MAX_ARENAS][MAX_INFO_STRING];
static int arenaCount;
static int buildScript;
static int precached[MAX_ARENAS];

/** Arena i: its map name (every seventh one longer than MAX_NAMELENGTH) and
 * one of the five type strings. */
static void ArenaMap( int i, char *map, int size ) {
	Com_sprintf( map, size, i % 7 ? "arena%d" : "arena%d_with_a_long_name", i );
}

/** Arena i's map name as the menu lists it: cut to MAX_NAMELENGTH, upper case. */
static void ArenaListName( int i, char *name ) {
	char map[64];

	ArenaMap( i, map, sizeof( map ) );
	Q_strncpyz( name, map, MAX_NAMELENGTH );
	Q_strupr( name );
}

/** The arenas q3_ui has loaded, numbered in order. */
static void ServeArenas( int count ) {
	char map[64];
	int i;

	Check( count <= MAX_ARENAS, "arena count" );
	for ( i = 0; i < count; i++ ) {
		ArenaMap( i, map, sizeof( map ) );
		arenaInfos[i][0] = '\0';
		Info_SetValueForKey( arenaInfos[i], "map", map );
		Info_SetValueForKey( arenaInfos[i], "type", arenaTypes[i % ARENA_TYPES] );
		Info_SetValueForKey( arenaInfos[i], "num", va( "%d", i ) );
	}
	arenaCount = count;
}

int UI_GetNumArenas( void ) {
	return arenaCount;
}
const char *UI_GetArenaInfoByNumber( int num ) {
	Check( num >= 0 && num < arenaCount, "arena number" );
	return arenaInfos[num];
}
/** com_buildscript: whether every levelshot is precached. */
float trap_Cvar_VariableValue( const char *var_name ) {
	Check( !strcmp( var_name, "com_buildscript" ), "read cvar" );
	return buildScript;
}
/** Count each arena's precached levelshot. */
qhandle_t trap_R_RegisterShaderNoMip( const char *name ) {
	char map[64];
	int i;

	if ( !strncmp( name, "levelshots/", 11 ) ) {
		for ( i = 0; i < arenaCount; i++ ) {
			ArenaListName( i, map );
			if ( !strcmp( name + 11, map ) ) {
				precached[i]++;
				break;
			}
		}
		Check( i < arenaCount, "precached levelshot names an arena" );
	}
	return 1;
}
/** The menu is built but not drawn. */
void Menu_AddItem( menuframework_s *menu, void *item ) {
	(void)menu; (void)item;
}
void UI_PushMenu( menuframework_s *menu ) {
	(void)menu;
}
vec4_t color_white = { 1.00f, 1.00f, 1.00f, 1.00f };
vec4_t color_orange = { 1.00f, 0.43f, 0.00f, 1.00f };
vec4_t color_red = { 1.00f, 0.00f, 0.00f, 1.00f };
vec4_t text_color_normal = { 1.00f, 0.43f, 0.00f, 1.00f };
vec4_t listbar_color = { 1.00f, 0.43f, 0.00f, 0.30f };
vec4_t text_color_disabled = { 0.50f, 0.50f, 0.50f, 1.00f };
vec4_t text_color_highlight = { 1.00f, 1.00f, 0.00f, 1.00f };
const char *punkbuster_items[] = { "Disabled", "Enabled", NULL };
uiStatic_t uis;
/** The menus' drawing, events and the server options and bot menus they open
 * are kept but never run here. */
void UI_DrawHandlePic( float x, float y, float w, float h, qhandle_t hShader ) {
	(void)x; (void)y; (void)w; (void)h; (void)hShader;
	Check( 0, "no drawing" );
}
void UI_FillRect( float x, float y, float width, float height, const float *color ) {
	(void)x; (void)y; (void)width; (void)height; (void)color;
	Check( 0, "no drawing" );
}
void UI_DrawString( int x, int y, const char *str, int style, vec4_t color ) {
	(void)x; (void)y; (void)str; (void)style; (void)color;
	Check( 0, "no drawing" );
}
void UI_DrawChar( int x, int y, int ch, int style, vec4_t color ) {
	(void)x; (void)y; (void)ch; (void)style; (void)color;
	Check( 0, "no drawing" );
}
void Bitmap_Draw( menubitmap_s *b ) {
	(void)b;
	Check( 0, "no drawing" );
}
void UI_PopMenu( void ) {
	Check( 0, "no menu events" );
}
char *UI_Cvar_VariableString( const char *var_name ) {
	(void)var_name;
	Check( 0, "no menu events" );
	return "";
}
void trap_Cvar_VariableStringBuffer( const char *var_name, char *buffer, int bufsize ) {
	(void)var_name; (void)buffer; (void)bufsize;
	Check( 0, "no menu events" );
}
void trap_Cvar_Set( const char *var_name, const char *value ) {
	(void)var_name; (void)value;
	Check( 0, "no menu events" );
}
void trap_Cvar_SetValue( const char *var_name, float value ) {
	(void)var_name; (void)value;
	Check( 0, "no menu events" );
}
void trap_Cmd_ExecuteText( int exec_when, const char *text ) {
	(void)exec_when; (void)text;
	Check( 0, "no menu events" );
}
const char *UI_GetArenaInfoByMap( const char *map ) {
	(void)map;
	Check( 0, "no menu events" );
	return NULL;
}
char *UI_GetBotInfoByNumber( int num ) {
	(void)num;
	Check( 0, "no menu events" );
	return NULL;
}
char *UI_GetBotInfoByName( const char *name ) {
	(void)name;
	Check( 0, "no menu events" );
	return NULL;
}
int UI_GetNumBots( void ) {
	Check( 0, "no menu events" );
	return 0;
}
/** Fail on engine errors. */
void QDECL Com_Error( int level, const char *error, ... ) {
	(void)level;
	fprintf( stderr, "Unexpected Com_Error: %s\n", error );
	exit( 1 );
}
/** Nothing reports a malformed info or a cut name on this path. */
void QDECL Com_Printf( const char *msg, ... ) {
	fprintf( stderr, "Unexpected Com_Printf: %s", msg );
	exit( 1 );
}

/** The menu's list must hold the first 64 of the arenas whose type bits match
 * matchbits (all arenas for 0), in order, with their upper-case names. */
static void CheckList( int matchbits, const char *what ) {
	char map[64];
	int i, n, bits;

	n = 0;
	for ( i = 0; i < arenaCount && n < MAX_SERVERMAPS; i++ ) {
		bits = GametypeBits( (char *)arenaTypes[i % ARENA_TYPES] );
		if ( matchbits && !( bits & matchbits ) ) {
			continue;
		}
		ArenaListName( i, map );
		if ( strcmp( s_startserver.maplist[n], map ) || s_startserver.mapGamebits[n] != bits ) {
			fprintf( stderr, "%s, %d arenas: entry %d is \"%s\" %d, expected \"%s\" %d\n", what, arenaCount, n,
				s_startserver.maplist[n], s_startserver.mapGamebits[n], map, bits );
			Check( 0, what );
		}
		n++;
	}
	Check( s_startserver.nummaps == n, what );
	Check( s_startserver.maxpages == ( n + MAX_MAPSPERPAGE - 1 ) / MAX_MAPSPERPAGE, what );
}

/** Open the menu over count arenas, then choose each game type. */
static void CheckArenas( int count ) {
	int i, type, matchbits;

	ServeArenas( count );

	/* StartServer_Cache lists every arena; a build script precaches all. */
	buildScript = 1;
	memset( precached, 0, sizeof( precached ) );
	StartServer_Cache();
	CheckList( 0, "cached list" );
	for ( i = 0; i < count; i++ ) {
		Check( precached[i] == 1, "every arena's levelshot is precached once" );
	}
	buildScript = 0;

	UI_StartServerMenu( qtrue );
	CheckList( ( 1 << GT_FFA ) | ( 1 << GT_SINGLE_PLAYER ), "free for all list" );
	Check( !strcmp( s_startserver.mapname.string, s_startserver.maplist[0] ), "first map shown" );

	for ( type = 0; type < 4; type++ ) {
		s_startserver.gametype.curvalue = type;
		s_startserver.gametype.generic.callback( &s_startserver.gametype, QM_ACTIVATED );
		matchbits = 1 << gametype_remap[type];
		if ( gametype_remap[type] == GT_FFA ) {
			matchbits |= 1 << GT_SINGLE_PLAYER;
		}
		CheckList( matchbits, "game type list" );
	}
}

int main( void ) {
	/* Up to 64 arenas list as before; then the first 64. */
	CheckArenas( 30 );
	CheckArenas( MAX_SERVERMAPS );
	CheckArenas( MAX_SERVERMAPS + 1 );
	CheckArenas( 100 );
	CheckArenas( MAX_ARENAS );
	puts( "q3_ui start server map list regressions passed (issue #440 audit)" );
	return 0;
}
