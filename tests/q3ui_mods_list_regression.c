/* Found auditing issue #440: the base q3_ui Mods menu stored every listed mod
 * directory in its MAX_MODS (64) entry lists, baseq3 first, and only cut the
 * count to MAX_MODS after the loop. With 64 or more mod directories it wrote
 * past descriptionList into fs_gameList, which made baseq3's entry name another
 * mod, and past the end of s_mods. The menu now stops at MAX_MODS entries and
 * shows the same first 64 as before. */
#include "../code/q3_ui/ui_mods.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** Fail with a description of the first mismatch. */
static void Check( int ok, const char *what ) {
	if ( !ok ) {
		fprintf( stderr, "q3_ui mods list regression failed: %s\n", what );
		exit( 1 );
	}
}

static int servedMods;
static char parsedReport[64];

/** FS_GetModList: "mod<i>\0Mod number <i>\0" for each served mod directory. */
int trap_FS_GetFileList( const char *path, const char *extension, char *listbuf, int bufsize ) {
	char entry[64];
	int i, len, used;

	Check( !strcmp( path, "$modlist" ) && !strcmp( extension, "" ), "listed mods" );
	used = 0;
	for ( i = 0; i < servedMods; i++ ) {
		Com_sprintf( entry, sizeof( entry ), "mod%d", i );
		len = strlen( entry ) + 1;
		Com_sprintf( entry + len, sizeof( entry ) - len, "Mod number %d", i );
		len += strlen( entry + len ) + 1;
		Check( used + len < bufsize, "listed mods fit" );
		memcpy( listbuf + used, entry, len );
		used += len;
	}
	return servedMods;
}
/** Remember the parsed count report. */
void trap_Print( const char *string ) {
	Q_strncpyz( parsedReport, string, sizeof( parsedReport ) );
}
/** Fail on engine errors. */
void QDECL Com_Error( int level, const char *error, ... ) {
	(void)level;
	fprintf( stderr, "Unexpected Com_Error: %s\n", error );
	exit( 1 );
}
/** Nothing is printed through the engine on this path. */
void QDECL Com_Printf( const char *msg, ... ) {
	fprintf( stderr, "Unexpected Com_Printf: %s", msg );
	exit( 1 );
}

/** List count mod directories; the menu must show baseq3 and the first shown - 1. */
static void CheckMods( int count, int shown ) {
	char name[16], description[48], report[32];
	int i;

	servedMods = count;
	memset( &s_mods, 0, sizeof( s_mods ) );
	UI_Mods_LoadMods();
	Check( s_mods.list.numitems == shown, "shown mod count" );
	Check( !strcmp( s_mods.list.itemnames[0], "Quake III Arena" ) && !strcmp( s_mods.fs_gameList[0], "" ), "baseq3 entry" );
	for ( i = 1; i < shown; i++ ) {
		Com_sprintf( name, sizeof( name ), "mod%d", i - 1 );
		Com_sprintf( description, sizeof( description ), "Mod number %d", i - 1 );
		Check( !strcmp( s_mods.fs_gameList[i], name ), "mod directory" );
		Check( !strcmp( s_mods.descriptionList[i], description ), "mod description" );
		Check( s_mods.list.itemnames[i] == s_mods.descriptionList[i], "mod list item" );
	}
	Com_sprintf( report, sizeof( report ), "%i mods parsed\n", shown );
	Check( !strcmp( parsedReport, report ), "parsed count report" );
}

int main( void ) {
	CheckMods( 0, 1 );
	CheckMods( 3, 4 );
	CheckMods( MAX_MODS - 1, MAX_MODS );
	CheckMods( MAX_MODS, MAX_MODS );
	CheckMods( 100, MAX_MODS );
	puts( "q3_ui mods list regressions passed (issue #440 audit)" );
	return 0;
}
