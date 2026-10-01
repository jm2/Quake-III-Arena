/* Issue #440: the base q3_ui's UI_LoadArenas and UI_LoadBots built each listed
 * name's path with strcpy and strcat in a 128-byte stack filename, and a pk3
 * entry name is up to 255 bytes. Names of 120 and 250 bytes overflowed it. A
 * name that does not fit after "scripts/" is now reported and skipped; every
 * name that fits still loads, in list order, after arenas.txt or bots.txt, and
 * the arenas are numbered as before. */
#include "../code/q3_ui/ui_gameinfo.c"
#include "arena_bot_fs_fixture.h"

/** Count the names reported as too long. */
void trap_Print( const char *string ) {
	FixturePrint( string );
}

int main( void ) {
	static const char *const numbers[] = { "0", "1", "2", "3" };
	int i;

	FixtureServe();
	UI_InitMemory();

	FixtureBeginLoad();
	UI_LoadArenas();
	FixtureCheckOpened( "scripts/arenas.txt", "first.arena", fixtureArena119, "last.arena", "UI_LoadArenas opens" );
	FixtureCheckInfos( ui_arenaInfos, ui_numArenas, "map", fixtureArenaMaps, FIXTURE_COUNT( fixtureArenaMaps ), "UI_LoadArenas arenas" );
	for ( i = 0; i < ui_numArenas; i++ ) {
		Check( !strcmp( Info_ValueForKey( ui_arenaInfos[i], "num" ), numbers[i] ), "UI_LoadArenas arena numbers" );
	}
	Check( !strcmp( Info_ValueForKey( UI_GetArenaInfoByMap( "fits" ), "longname" ), "Fits" ), "119-byte name's arena" );

	FixtureBeginLoad();
	UI_LoadBots();
	FixtureCheckOpened( "scripts/bots.txt", "first.bot", fixtureBot119, "last.bot", "UI_LoadBots opens" );
	FixtureCheckInfos( ui_botInfos, ui_numBots, "name", fixtureBotNames, FIXTURE_COUNT( fixtureBotNames ), "UI_LoadBots bots" );
	Check( !strcmp( Info_ValueForKey( UI_GetBotInfoByName( "Fits" ), "model" ), "visor" ), "119-byte name's bot" );
	Check( !outOfMemory, "the infos fit the pool" );

	puts( "q3_ui arena and bot file name regressions passed (issue #440)" );
	return 0;
}
