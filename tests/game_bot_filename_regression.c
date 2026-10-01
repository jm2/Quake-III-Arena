/* Issue #440: the game's G_LoadArenas and G_LoadBots built each listed name's
 * path with strcpy and strcat in a 128-byte stack filename, and a pk3 entry name
 * is up to 255 bytes, so any pk3 a server runs with could overflow it. A name
 * that does not fit after "scripts/" is now reported and skipped; every name
 * that fits still loads, in list order, after arenas.txt or bots.txt. */
#include "../code/game/g_bot.c"
#include "arena_bot_fs_fixture.h"

vmCvar_t g_debugAlloc;

/** Count the names reported as too long. */
void trap_Printf( const char *fmt ) {
	FixturePrint( fmt );
}
/** Bots are enabled, so G_LoadBots loads them. */
int trap_Cvar_VariableIntegerValue( const char *var_name ) {
	Check( !strcmp( var_name, "bot_enable" ), "read cvar" );
	return 1;
}
/** G_Alloc's pool holds the infos. */
void QDECL G_Error( const char *fmt, ... ) {
	fprintf( stderr, "Unexpected G_Error: %s\n", fmt );
	exit( 1 );
}
/** Only g_debugAlloc prints through G_Printf, and it is off. */
void QDECL G_Printf( const char *fmt, ... ) {
	fprintf( stderr, "Unexpected G_Printf: %s", fmt );
	exit( 1 );
}

int main( void ) {
	static const char *const numbers[] = { "0", "1", "2", "3" };
	int i;

	FixtureServe();

	FixtureBeginLoad();
	G_LoadArenas();
	FixtureCheckOpened( "scripts/arenas.txt", "first.arena", fixtureArena119, "last.arena", "G_LoadArenas opens" );
	FixtureCheckInfos( g_arenaInfos, g_numArenas, "map", fixtureArenaMaps, FIXTURE_COUNT( fixtureArenaMaps ), "G_LoadArenas arenas" );
	for ( i = 0; i < g_numArenas; i++ ) {
		Check( !strcmp( Info_ValueForKey( g_arenaInfos[i], "num" ), numbers[i] ), "G_LoadArenas arena numbers" );
	}
	Check( !strcmp( Info_ValueForKey( G_GetArenaInfoByMap( "fits" ), "longname" ), "Fits" ), "119-byte name's arena" );

	FixtureBeginLoad();
	G_LoadBots();
	FixtureCheckOpened( "scripts/bots.txt", "first.bot", fixtureBot119, "last.bot", "G_LoadBots opens" );
	FixtureCheckInfos( g_botInfos, g_numBots, "name", fixtureBotNames, FIXTURE_COUNT( fixtureBotNames ), "G_LoadBots bots" );
	Check( !strcmp( Info_ValueForKey( G_GetBotInfoByName( "Fits" ), "model" ), "visor" ), "119-byte name's bot" );

	puts( "game arena and bot file name regressions passed (issue #440)" );
	return 0;
}
