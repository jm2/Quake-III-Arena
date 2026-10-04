/* Issue #49: UI_LoadArenas stored String_Alloc's NULL as a map's load name,
 * long name or level shot path once the Team Arena UI's string pool was full,
 * and the map lists then read it. The map list now keeps only the arenas whose
 * strings were stored. */
#include "../code/ui/ui_gameinfo.c"
#include "ui_string_pool_fixture.h"

uiInfo_t uiInfo;

static const char *arenasText;

/** g_arenasFile is empty, so scripts/arenas.txt loads. */
void trap_Cvar_Register( vmCvar_t *vmCvar, const char *varName, const char *defaultValue, int flags ) {
	(void)defaultValue; (void)flags;
	Check( !strcmp( varName, "g_arenasFile" ), "registered cvar" );
	memset( vmCvar, 0, sizeof( *vmCvar ) );
}
/** No .arena files besides scripts/arenas.txt. */
int trap_FS_GetFileList( const char *path, const char *extension, char *listbuf, int bufsize ) {
	(void)path; (void)extension; (void)listbuf; (void)bufsize;
	return 0;
}
/** Serve the arenas text as scripts/arenas.txt. */
int trap_FS_FOpenFile( const char *qpath, fileHandle_t *f, fsMode_t mode ) {
	(void)mode;
	Check( !strcmp( qpath, "scripts/arenas.txt" ), "opened file" );
	*f = 1;
	return strlen( arenasText );
}
/** Copy the served arenas text. */
void trap_FS_Read( void *buffer, int len, fileHandle_t f ) {
	(void)f;
	memcpy( buffer, arenasText, len );
}
/** Nothing to close for the served file. */
void trap_FS_FCloseFile( fileHandle_t f ) { (void)f; }
/** Only the arena count is printed. */
void trap_Print( const char *string ) {
	Check( strstr( string, "arenas parsed" ) != NULL, "only the arena count is printed" );
}
/** The served arenas parse, so nothing else is reported. */
void QDECL Com_Printf( const char *msg, ... ) {
	fprintf( stderr, "Unexpected Com_Printf: %s", msg );
	exit( 1 );
}
/** Fail on engine errors. */
void QDECL Com_Error( int level, const char *error, ... ) {
	(void)level;
	fprintf( stderr, "Unexpected Com_Error: %s\n", error );
	exit( 1 );
}
/** Menu script parsing is kept by the linked ui_shared.c but never runs here. */
int trap_PC_ReadToken( int handle, pc_token_t *pc_token ) {
	(void)handle; (void)pc_token;
	Check( 0, "no menu script parsing" );
	return 0;
}
/** Menu script parsing is kept by the linked ui_shared.c but never runs here. */
int trap_PC_SourceFileAndLine( int handle, char *filename, int *line ) {
	(void)handle; (void)filename; (void)line;
	Check( 0, "no menu script parsing" );
	return 0;
}

/** Load the served arenas and read every listed map's names as the map
 * lists, their level shots and the server commands do. */
static void LoadArenas( const char *text ) {
	int i;

	arenasText = text;
	UI_LoadArenas();
	for ( i = 0; i < uiInfo.mapCount; i++ ) {
		ReadString( uiInfo.mapList[i].mapLoadName );
		ReadString( uiInfo.mapList[i].mapName );
		ReadString( uiInfo.mapList[i].imageName );
	}
}

/** Whether map n is the arena with this load name and long name. */
static int IsMap( int n, const char *mapLoadName, const char *mapName ) {
	return !strcmp( uiInfo.mapList[n].mapLoadName, mapLoadName ) && !strcmp( uiInfo.mapList[n].mapName, mapName )
		&& !strcmp( uiInfo.mapList[n].imageName, va( "levelshots/%s", mapLoadName ) );
}

int main( int argc, char **argv ) {
	static const char normal[] =
		"{ map \"mpteam1\" longname \"Team Arena 1\" type \"ctf\" }\n"
		"{ map \"mpteam2\" longname \"Team Arena 2\" type \"oneflag\" }\n";
	const char *test = argc > 1 ? argv[1] : "";

	UI_InitMemory();

	/* the arenas load in full while the pool has room, as in retail */
	LoadArenas( normal );
	Check( uiInfo.mapCount == 2 && IsMap( 0, "mpteam1", "Team Arena 1" ) && IsMap( 1, "mpteam2", "Team Arena 2" )
		&& uiInfo.mapList[1].typeBits == ( 1 << GT_1FCTF ), "arenas with room" );
	if ( !strcmp( test, "room" ) ) {
		return 0;
	}

	/* names pooled before the pool fills, without the level shot path */
	String_Alloc( "mpteam3" );
	String_Alloc( "Team Arena 3" );
	FillStringPool();

	if ( !strcmp( test, "names" ) ) {
		/* a new load name or long name ends the list */
		LoadArenas( "{ map \"mpteam2\" longname \"Team Arena 2\" }\n{ map \"mpnew\" longname \"New\" }\n" );
		Check( uiInfo.mapCount == 1 && IsMap( 0, "mpteam2", "Team Arena 2" ), "arena with a new load name" );
		LoadArenas( "{ map \"mpteam1\" longname \"Team Arena 1\" }\n{ map \"mpteam2\" longname \"New\" }\n" );
		Check( uiInfo.mapCount == 1 && IsMap( 0, "mpteam1", "Team Arena 1" ), "arena with a new long name" );
	} else {
		Check( !strcmp( test, "levelshot" ), "known case" );
		/* a new level shot path ends the list */
		LoadArenas( "{ map \"mpteam1\" longname \"Team Arena 1\" }\n{ map \"mpteam3\" longname \"Team Arena 3\" }\n" );
		Check( uiInfo.mapCount == 1 && IsMap( 0, "mpteam1", "Team Arena 1" ), "arena with a new level shot path" );
	}
	/* the normal arenas still load in full: their strings are all pooled */
	LoadArenas( normal );
	Check( uiInfo.mapCount == 2 && IsMap( 0, "mpteam1", "Team Arena 1" ) && IsMap( 1, "mpteam2", "Team Arena 2" ), "pooled arenas" );
	return 0;
}
