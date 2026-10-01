/* Found auditing issue #440: the base q3_ui single player menu strcpy'd
 * va( "levelshots/%s.tga", map ) into a MAX_QPATH levelPicNames entry. The map
 * name comes from an .arena file in any pk3 and is cut to 63 bytes, so a name of
 * 49 bytes or more ran into the next entry, and from the last entry into
 * levelNames. The renderer refuses a shader name of MAX_QPATH bytes or more, so
 * retail showed the unknown map picture for those maps; the menu now shows it
 * without building the name, and shows every other levelshot as before. */
#include "../code/q3_ui/ui_splevel.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** Fail with a description of the first mismatch. */
static void Check( int ok, const char *what ) {
	if ( !ok ) {
		fprintf( stderr, "q3_ui levelshot name regression failed: %s\n", what );
		exit( 1 );
	}
}

/** The levelshots in the served pk3s. */
static const char *const levelshots[] = { "levelshots/q3dm1.tga", "levelshots/q3dm17.tga" };

static char looked[256];	/* the last levelshot looked up */

/** RE_RegisterShaderNoMip: 0 for a name of MAX_QPATH bytes or more (as the
 * renderer refuses it) or a missing levelshot. */
qhandle_t trap_R_RegisterShaderNoMip( const char *name ) {
	int i;

	Q_strncpyz( looked, name, sizeof( looked ) );
	if ( strlen( name ) >= MAX_QPATH ) {
		return 0;
	}
	for ( i = 0; i < (int)( sizeof( levelshots ) / sizeof( levelshots[0] ) ); i++ ) {
		if ( !strcmp( name, levelshots[i] ) ) {
			return i + 1;
		}
	}
	return 0;
}
/** No level has a best score. */
void UI_GetBestScore( int level, int *score, int *skill ) {
	(void)level;
	*score = 0;
	*skill = 0;
}
/** Fail on engine errors. */
void QDECL Com_Error( int level, const char *error, ... ) {
	(void)level;
	fprintf( stderr, "Unexpected Com_Error: %s\n", error );
	exit( 1 );
}
/** Every formatted levelshot name fits, so Com_sprintf reports nothing. */
void QDECL Com_Printf( const char *msg, ... ) {
	fprintf( stderr, "Unexpected Com_Printf: %s", msg );
	exit( 1 );
}

/** Show the arena with map name map as level n; its picture must be picture and
 * its label label. */
static void ShowArena( int n, const char *map, const char *picture, const char *label ) {
	char info[MAX_INFO_STRING];

	info[0] = '\0';
	Info_SetValueForKey( info, "map", map );
	UI_SPLevelMenu_SetMenuArena( n, n, info );
	if ( strcmp( levelMenuInfo.levelPicNames[n], picture ) ) {
		fprintf( stderr, "level %d picture \"%s\", expected \"%s\"\n", n, levelMenuInfo.levelPicNames[n], picture );
		Check( 0, "level picture" );
	}
	Check( !strcmp( levelMenuInfo.levelNames[n], label ), "level name" );
}

/** A map name of length bytes of c. */
static const char *MapName( char *name, int length, char c ) {
	memset( name, c, length );
	name[length] = '\0';
	return name;
}

int main( void ) {
	char map48[49], map49[50], map63[64], expected48[MAX_QPATH];
	int n;

	/* Fill every picture and name so a write past one entry shows. */
	for ( n = 0; n < 4; n++ ) {
		memset( levelMenuInfo.levelPicNames[n], 'p', sizeof( levelMenuInfo.levelPicNames[n] ) - 1 );
		memset( levelMenuInfo.levelNames[n], 'n', sizeof( levelMenuInfo.levelNames[n] ) - 1 );
	}

	/* Levelshots that exist and a map without one, as retail shows them. */
	ShowArena( 0, "q3dm1", "levelshots/q3dm1.tga", "Q3DM1" );
	ShowArena( 1, "q3dm17", "levelshots/q3dm17.tga", "Q3DM17" );
	ShowArena( 2, "custom", ART_MAP_UNKNOWN, "CUSTOM" );

	/* "levelshots/<48 bytes>.tga" is 63 bytes and is looked up; a 49-byte name
	 * makes 64 bytes and a 63-byte name 78, which retail's renderer refused. */
	strcpy( expected48, "levelshots/" );
	strcat( expected48, MapName( map48, 48, 'a' ) );
	strcat( expected48, ".tga" );
	Check( strlen( expected48 ) == MAX_QPATH - 1, "48-byte map's levelshot length" );
	ShowArena( 2, map48, ART_MAP_UNKNOWN, "AAAAAAAAAAAAAAA" );
	Check( !strcmp( looked, expected48 ), "48-byte map's levelshot looked up" );
	ShowArena( 1, MapName( map49, 49, 'b' ), ART_MAP_UNKNOWN, "BBBBBBBBBBBBBBB" );
	ShowArena( 3, MapName( map63, 63, 'c' ), ART_MAP_UNKNOWN, "CCCCCCCCCCCCCCC" );

	/* The other levels' pictures and names are untouched. */
	Check( !strcmp( levelMenuInfo.levelPicNames[0], "levelshots/q3dm1.tga" ), "level 0 picture kept" );
	Check( !strcmp( levelMenuInfo.levelPicNames[2], ART_MAP_UNKNOWN ), "level 2 picture kept" );
	Check( !strcmp( levelMenuInfo.levelNames[0], "Q3DM1" ), "level 0 name kept" );
	Check( !strcmp( levelMenuInfo.levelNames[2], "AAAAAAAAAAAAAAA" ), "level 2 name kept" );

	puts( "q3_ui levelshot name regressions passed (issue #440 audit)" );
	return 0;
}
