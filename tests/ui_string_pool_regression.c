/* Issues #49 and #467: the Team Arena UI's string pool.
 * #467: String_Alloc appended a new string after the second-to-last string of
 * its hash chain, orphaning the old tail, so that string was copied into the
 * pool again on its next use instead of being found.
 * #49: once the pool is full, String_Alloc returns NULL, and the mod, movie,
 * demo and character lists stored that NULL as an entry, which the menus then
 * read. The lists now keep only the entries whose strings were stored. */
#include "../code/ui/ui_main.c"
#include "ui_string_pool_fixture.h"

#define PROTOCOL 68

static char fixture[MAX_MENUFILE];
static int fixtureLength;
static const char *listed[8];
static int listedCount;
static const char *listedPath;

/** Serve teaminfo text to UI_ParseTeamInfo and start an empty character list. */
static void ServeTeamInfo( const char *text ) {
	Check( strlen( text ) < sizeof( fixture ), "fixture fits MAX_MENUFILE" );
	strcpy( fixture, text );
	fixtureLength = strlen( text );
	uiInfo.characterCount = 0;
}

/** Serve names, each NUL-terminated, as the FS_GetFileList result for path. */
static void ServeList( const char *path, int count, const char *const *names ) {
	int i;
	Check( count <= (int)( sizeof( listed ) / sizeof( listed[0] ) ), "fixture list fits" );
	listedPath = path;
	listedCount = count;
	for ( i = 0; i < count; i++ ) {
		listed[i] = names[i];
	}
}

/** FS_GetFileList: the served names, packed as the engine packs them. For
 * "$modlist" each served name is a directory and its description. */
int trap_FS_GetFileList( const char *path, const char *extension, char *listbuf, int bufsize ) {
	int i, used, len, count;
	Check( !strcmp( path, listedPath ), "listed directory" );
	(void)extension;
	used = 0;
	count = !strcmp( path, "$modlist" ) ? listedCount / 2 : listedCount;
	for ( i = 0; i < listedCount; i++ ) {
		len = strlen( listed[i] ) + 1;
		Check( used + len < bufsize, "listed names fit" );
		memcpy( listbuf + used, listed[i], len );
		used += len;
	}
	return count;
}
/** The demo extension is dm_<protocol>. */
float trap_Cvar_VariableValue( const char *var_name ) {
	Check( !strcmp( var_name, "protocol" ), "read cvar" );
	return PROTOCOL;
}
/** Serve the fixture as the team info file. */
int trap_FS_FOpenFile( const char *qpath, fileHandle_t *f, fsMode_t mode ) {
	(void)qpath; (void)mode;
	*f = 1;
	return fixtureLength;
}
/** Copy the served file into GetMenuBuffer's buffer. */
void trap_FS_Read( void *buffer, int len, fileHandle_t f ) {
	(void)f;
	Check( len == fixtureLength, "GetMenuBuffer reads the whole file" );
	memcpy( buffer, fixture, len );
}
/** Nothing to close for the served file. */
void trap_FS_FCloseFile( fileHandle_t f ) { (void)f; }
/** The renderer's shader lookup reads the whole name. */
qhandle_t trap_R_RegisterShaderNoMip( const char *name ) {
	ReadString( name );
	return 1;
}
/** Fail if GetMenuBuffer rejects the served file. */
void trap_Print( const char *string ) {
	fprintf( stderr, "%s", string );
	Check( 0, "menu file served" );
}
/** The parsers' "Loaded ..." reports are not checked. */
void QDECL Com_Printf( const char *msg, ... ) { (void)msg; }
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
/** The server browser rows of UI_FeederItemText never run here. */
void trap_LAN_GetServerInfo( int source, int n, char *buf, int buflen ) {
	(void)source; (void)n; (void)buf; (void)buflen;
	Check( 0, "no server browser rows" );
}
/** Fail on engine errors. */
void QDECL Com_Error( int level, const char *error, ... ) {
	(void)level;
	fprintf( stderr, "Unexpected Com_Error: %s\n", error );
	exit( 1 );
}

/** Read every listed mod, movie and demo through the menus' list feeder, and
 * every character's icon and model as the head list and its selection do. */
static void ReadLists( void ) {
	qhandle_t handle;
	int i;
	for ( i = 0; i < uiInfo.modCount; i++ ) {
		ReadString( uiInfo.modList[i].modName );
		ReadString( UI_FeederItemText( FEEDER_MODS, i, 0, &handle ) );
	}
	for ( i = 0; i < uiInfo.movieCount; i++ ) {
		ReadString( UI_FeederItemText( FEEDER_CINEMATICS, i, 0, &handle ) );
	}
	for ( i = 0; i < uiInfo.demoCount; i++ ) {
		ReadString( UI_FeederItemText( FEEDER_DEMOS, i, 0, &handle ) );
	}
	for ( i = 0; i < uiInfo.characterCount; i++ ) {
		trap_R_RegisterShaderNoMip( uiInfo.characterList[i].imageName );
		ReadString( va( "%s", uiInfo.characterList[i].base ) );
		ReadString( uiInfo.characterList[i].base );
	}
}

/** #467: strings whose hashes collide are all found again, however long the
 * chain, and finding one takes no pool space. Case is ignored by the hash but
 * not by the comparison, so the spellings below share one chain. */
static void TestHashChain( void ) {
	static const char *const spellings[] = { "chain", "Chain", "CHAIN", "cHain", "chAin" };
	const char *first[5];
	int i, j;

	for ( i = 0; i < 5; i++ ) {
		first[i] = String_Alloc( spellings[i] );
		Check( first[i] && !strcmp( first[i], spellings[i] ), "colliding string stored" );
		for ( j = 0; j <= i; j++ ) {
			Check( String_Alloc( spellings[j] ) == first[j], "every string of the chain is found again" );
		}
	}
	/* the next new string lands right after the last spelling: nothing was copied twice */
	Check( String_Alloc( "after" ) == first[4] + strlen( spellings[4] ) + 1, "finding strings took no pool space" );
}

/** Normal lists load in full while the pool has room, as in retail. */
static void TestListsWithRoom( void ) {
	static const char *const mods[] = { "missionpack", "Quake III Team Arena", "basemod", "" };
	static const char *const movies[] = { "intro.roq", "idlogo.roq" };
	static const char *const demos[] = { "one.dm_68", "two.dm_68" };

	ServeList( "$modlist", 4, mods );
	UI_LoadMods();
	Check( uiInfo.modCount == 2 && !strcmp( uiInfo.modList[0].modName, "missionpack" )
		&& !strcmp( uiInfo.modList[0].modDescr, "Quake III Team Arena" )
		&& !strcmp( uiInfo.modList[1].modName, "basemod" ) && !*uiInfo.modList[1].modDescr, "mods with room" );
	ServeList( "video", 2, movies );
	UI_LoadMovies();
	Check( uiInfo.movieCount == 2 && !strcmp( uiInfo.movieList[0], "INTRO" )
		&& !strcmp( uiInfo.movieList[1], "IDLOGO" ), "movies with room" );
	ServeList( "demos", 2, demos );
	UI_LoadDemos();
	Check( uiInfo.demoCount == 2 && !strcmp( uiInfo.demoList[0], "ONE" )
		&& !strcmp( uiInfo.demoList[1], "TWO" ), "demos with room" );
	/* no female character yet, so "Janet" is not pooled before the pool fills */
	ServeTeamInfo( "characters {\n\t{ \"Kyonshi\" \"male\" }\n\t{ \"Brandon\" \"Brandon\" }\n}\n" );
	UI_ParseTeamInfo( "teaminfo.txt" );
	Check( uiInfo.characterCount == 2 && !strcmp( uiInfo.characterList[0].base, "James" )
		&& !strcmp( uiInfo.characterList[1].base, "Brandon" )
		&& !strcmp( uiInfo.characterList[0].imageName, "models/players/heads/Kyonshi/icon_default.tga" ), "characters with room" );
	ReadLists();
}

/** Load the lists with room, then pool the strings the character cases need
 * (a character whose name and sex are pooled but whose icon path is not, and one
 * whose name, sex and icon path are pooled but whose model "Janet" is not), and
 * fill the pool. */
static void FillPoolAfterLists( void ) {
	TestListsWithRoom();
	String_Alloc( "Iconless" );
	String_Alloc( "Modelless" );
	String_Alloc( "female" );
	String_Alloc( "Robot" );
	String_Alloc( "models/players/heads/Modelless/icon_default.tga" );
	FillStringPool();
}

/* #49: with the pool full, each list keeps the entries whose strings are
 * already in the pool and stops at the first one that needs new space. */

/** UI_LoadMods: a new directory name or a new description ends the list. */
static void TestModsWithFullPool( void ) {
	static const char *const mods[] = { "missionpack", "Quake III Team Arena", "newmod", "New Mod" };
	static const char *const modsNewDescription[] = { "basemod", "", "missionpack", "New Description" };

	ServeList( "$modlist", 4, mods );
	UI_LoadMods();
	ReadLists();
	Check( uiInfo.modCount == 1 && !strcmp( uiInfo.modList[0].modName, "missionpack" ), "mods with a full pool" );
	ServeList( "$modlist", 4, modsNewDescription );
	UI_LoadMods();
	ReadLists();
	Check( uiInfo.modCount == 1 && !strcmp( uiInfo.modList[0].modName, "basemod" ), "mod with a new description" );
}

/** UI_LoadMovies: a new movie name ends the list. */
static void TestMoviesWithFullPool( void ) {
	static const char *const movies[] = { "idlogo.roq", "newmovie.roq", "intro.roq" };

	ServeList( "video", 3, movies );
	UI_LoadMovies();
	ReadLists();
	Check( uiInfo.movieCount == 1 && !strcmp( uiInfo.movieList[0], "IDLOGO" ), "movies with a full pool" );
}

/** UI_LoadDemos: a new demo name ends the list. */
static void TestDemosWithFullPool( void ) {
	static const char *const demos[] = { "two.dm_68", "newdemo.dm_68", "one.dm_68" };

	ServeList( "demos", 3, demos );
	UI_LoadDemos();
	ReadLists();
	Check( uiInfo.demoCount == 1 && !strcmp( uiInfo.demoList[0], "TWO" ), "demos with a full pool" );
}

/** Character_Parse: a new icon path or model name fails the list, as a name it
 * cannot store does, and keeps the characters before it. */
static void TestCharactersWithFullPool( void ) {
	ServeTeamInfo( "characters {\n\t{ \"Kyonshi\" \"male\" }\n\t{ \"Iconless\" \"male\" }\n}\n" );
	UI_ParseTeamInfo( "teaminfo.txt" );
	ReadLists();
	Check( uiInfo.characterCount == 1 && !strcmp( uiInfo.characterList[0].name, "Kyonshi" ), "character without an icon path" );

	ServeTeamInfo( "characters {\n\t{ \"Kyonshi\" \"male\" }\n\t{ \"Modelless\" \"female\" }\n}\n" );
	UI_ParseTeamInfo( "teaminfo.txt" );
	ReadLists();
	Check( uiInfo.characterCount == 1 && !strcmp( uiInfo.characterList[0].name, "Kyonshi" ), "character without a model name" );

	/* a pooled custom model still loads */
	ServeTeamInfo( "characters {\n\t{ \"Kyonshi\" \"male\" }\n\t{ \"Modelless\" \"Robot\" }\n}\n" );
	UI_ParseTeamInfo( "teaminfo.txt" );
	ReadLists();
	Check( uiInfo.characterCount == 2 && !strcmp( uiInfo.characterList[1].base, "Robot" ), "pooled custom model" );
}

/** One case per process, named by the argument, so each fails on its own. */
int main( int argc, char **argv ) {
	const char *test = argc > 1 ? argv[1] : "";

	memset( &uiInfo, 0, sizeof( uiInfo ) );
	UI_InitMemory();
	if ( !strcmp( test, "chain" ) ) {
		TestHashChain();
		return 0;
	}
	if ( !strcmp( test, "room" ) ) {
		TestListsWithRoom();
		return 0;
	}
	FillPoolAfterLists();
	if ( !strcmp( test, "mods" ) ) {
		TestModsWithFullPool();
	} else if ( !strcmp( test, "movies" ) ) {
		TestMoviesWithFullPool();
	} else if ( !strcmp( test, "demos" ) ) {
		TestDemosWithFullPool();
	} else {
		Check( !strcmp( test, "characters" ), "known case" );
		TestCharactersWithFullPool();
	}
	return 0;
}
