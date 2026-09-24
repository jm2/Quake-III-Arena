/* Issue #377: the base q3_ui Load Config menu strips ".cfg" only from names at
 * least as long as it. The engine lists every name that ends in "cfg", so a file
 * or pk3 entry named just cfg is listed as "cfg", one byte shorter than ".cfg".
 * Comparing that name against ".cfg" read the byte before the list (the end of
 * the Go button in s_configs) and wrote a NUL there when that byte was '.'.
 * The Demos menu (ui_demo2.c, linked too) compares ".dm3" against names that end
 * in "dm_<protocol>", which are never shorter than it; it must list as before. */
#include "../code/q3_ui/ui_loadconfig.c"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *servedDirectory;
static const char *servedExtension;
static const char *const *servedNames;
static int servedCount;
static int protocol;
static menulist_s *shownList;
static menuframework_s *pushedMenu;

/** Fail with a description of the first mismatch. */
static void Check( int ok, const char *what ) {
	if ( !ok ) {
		fprintf( stderr, "q3_ui file list regression failed: %s\n", what );
		exit( 1 );
	}
}

/** The last byte of s_configs.go, which is the byte right before s_configs.names. */
static char *ByteBeforeConfigNames( void ) {
	return (char *)&s_configs.go + sizeof( s_configs.go ) - 1;
}

/** FS_GetFileList: the served names, each NUL-terminated, packed from the start of
 * listbuf. The Load Config menu has set up its Go button by now; a '.' in the Go
 * button's last byte stands for whatever byte comes before the list. */
int trap_FS_GetFileList( const char *path, const char *extension, char *listbuf, int bufsize ) {
	int i, len, used;

	Check( !strcmp( path, servedDirectory ), "listed directory" );
	Check( !strcmp( extension, servedExtension ), "listed extension" );
	if ( listbuf == s_configs.names ) {
		*ByteBeforeConfigNames() = '.';
	}
	used = 0;
	for ( i = 0; i < servedCount; i++ ) {
		len = strlen( servedNames[i] ) + 1;
		Check( used + len < bufsize, "served list fits" );
		memcpy( listbuf + used, servedNames[i], len );
		used += len;
	}
	return servedCount;
}
/** Only the Demos menu reads a cvar: the protocol its extension is built from. */
float trap_Cvar_VariableValue( const char *var_name ) {
	Check( !strcmp( var_name, "protocol" ), "protocol cvar" );
	return protocol;
}
/** Remember the menu's scroll list. */
void Menu_AddItem( menuframework_s *menu, void *item ) {
	(void)menu;
	if ( ( (menucommon_s *)item )->type == MTYPE_SCROLLLIST ) {
		shownList = item;
	}
}
/** Remember the shown menu. */
void UI_PushMenu( menuframework_s *menu ) {
	pushedMenu = menu;
}
/** Give every menu picture a handle. */
qhandle_t trap_R_RegisterShaderNoMip( const char *name ) {
	(void)name;
	return 1;
}
vec4_t color_white = { 1.00f, 1.00f, 1.00f, 1.00f };
/** The menus' event and key handlers are kept but never run here. */
void UI_PopMenu( void ) {
	Check( 0, "no menu events" );
}
/** The menus' event and key handlers are kept but never run here. */
void UI_ForceMenuOff( void ) {
	Check( 0, "no menu events" );
}
/** The menus' event and key handlers are kept but never run here. */
void trap_Cmd_ExecuteText( int exec_when, const char *text ) {
	(void)exec_when; (void)text;
	Check( 0, "no menu events" );
}
/** The menus' event and key handlers are kept but never run here. */
sfxHandle_t ScrollList_Key( menulist_s *l, int key ) {
	(void)l; (void)key;
	Check( 0, "no menu events" );
	return 0;
}
/** The menus' event and key handlers are kept but never run here. */
void *Menu_ItemAtCursor( menuframework_s *m ) {
	(void)m;
	Check( 0, "no menu events" );
	return NULL;
}
/** The menus' event and key handlers are kept but never run here. */
sfxHandle_t Menu_DefaultKey( menuframework_s *m, int key ) {
	(void)m; (void)key;
	Check( 0, "no menu events" );
	return 0;
}
/** Fail on engine errors. */
void QDECL Com_Error( int level, const char *error, ... ) {
	(void)level;
	fprintf( stderr, "Unexpected Com_Error: %s\n", error );
	exit( 1 );
}
/** Nothing is printed on this path. */
void QDECL Com_Printf( const char *msg, ... ) {
	(void)msg;
}

/** Serve names for directory and extension. */
static void Serve( const char *directory, const char *extension, const char *const *names, int count ) {
	servedDirectory = directory;
	servedExtension = extension;
	servedNames = names;
	servedCount = count;
	shownList = NULL;
	pushedMenu = NULL;
}

/** Check that the shown menu's list shows expected[0..count). */
static void CheckList( const char *const *expected, int count, const char *what ) {
	int i;

	Check( pushedMenu != NULL && shownList != NULL, what );
	Check( shownList->numitems == count, what );
	for ( i = 0; i < count; i++ ) {
		if ( strcmp( shownList->itemnames[i], expected[i] ) ) {
			fprintf( stderr, "%s %d: \"%s\", expected \"%s\"\n", what, i, shownList->itemnames[i], expected[i] );
			Check( 0, what );
		}
	}
}

/** List the served names as the cfg files through the real Load Config menu. The
 * byte before the list must keep its '.'. */
static void CheckConfigs( const char *const *names, const char *const *expected, int count ) {
	Serve( "", "cfg", names, count );
	UI_LoadConfigMenu();
	Check( pushedMenu == &s_configs.menu, "Load Config menu shown" );
	CheckList( expected, count, "config" );
	Check( *ByteBeforeConfigNames() == '.', "byte before the config list" );
}

/** List the served names as the dm_<protocol> demos through the real Demos menu. */
static void CheckDemos( int demoProtocol, const char *const *names, const char *const *expected, int count ) {
	char extension[32];

	Com_sprintf( extension, sizeof( extension ), "dm_%d", demoProtocol );
	protocol = demoProtocol;
	Serve( "demos", extension, names, count );
	UI_DemosMenu();
	CheckList( expected, count, "demo" );
}

#define COUNT( a ) ( (int)( sizeof( a ) / sizeof( ( a )[0] ) ) )

int main( void ) {
	/* The first name starts the list: "cfg" is shorter than ".cfg", ".cfg" as
	 * long, the others longer (default.cfg, z.cfg and !slug.cfg are in the
	 * retail pak0.pk3). "mycfg" lacks the dot and is listed whole, as before. */
	static const char *const configs[] = { "cfg", "q3config.cfg", "default.cfg", "z.cfg", "!slug.cfg", ".cfg", "mycfg", "Autoexec.CFG" };
	static const char *const configNames[] = { "CFG", "Q3CONFIG", "DEFAULT", "Z", "!SLUG", "", "MYCFG", "AUTOEXEC" };
	/* Normal names list as before when the short name is not first. */
	static const char *const laterConfigs[] = { "q3config.cfg", "cfg" };
	static const char *const laterConfigNames[] = { "Q3CONFIG", "CFG" };
	/* Retail compares ".dm3", so no dm_<protocol> name is stripped. "dm_68" and
	 * "dm_5" are the shortest names the engine lists for their protocol. */
	static const char *const demos[] = { "dm_68", "four.dm_68", ".dm_68", "Match.DM_68" };
	static const char *const demoNames[] = { "DM_68", "FOUR.DM_68", ".DM_68", "MATCH.DM_68" };
	static const char *const shortDemos[] = { "dm_5", "a.dm_5" };
	static const char *const shortDemoNames[] = { "DM_5", "A.DM_5" };

	Check( offsetof( configs_t, names ) == offsetof( configs_t, go ) + sizeof( s_configs.go ),
		"the config list follows the Go button" );
	CheckConfigs( configs, configNames, COUNT( configs ) );
	CheckConfigs( laterConfigs, laterConfigNames, COUNT( laterConfigs ) );
	CheckDemos( 68, demos, demoNames, COUNT( demos ) );
	CheckDemos( 5, shortDemos, shortDemoNames, COUNT( shortDemos ) );
	puts( "q3_ui config and demo list extension regressions passed (issue #377)" );
	return 0;
}
