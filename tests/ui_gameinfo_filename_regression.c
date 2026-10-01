/* Issue #440: the Team Arena UI's UI_LoadArenas and UI_LoadBots built each listed
 * name's path with strcpy and strcat in a 128-byte stack filename, and a pk3
 * entry name is up to 255 bytes. Names of 120 and 250 bytes overflowed it. A
 * name that does not fit after "scripts/" is now reported and skipped; every
 * name that fits still loads, in list order, after arenas.txt or bots.txt. */
#include "../code/ui/ui_gameinfo.c"
#include "arena_bot_fs_fixture.h"

uiInfo_t uiInfo;

/** Count the names reported as too long. */
void trap_Print( const char *string ) {
	FixturePrint( string );
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

int main( void ) {
	static const char *const mapNames[] = { "Arena Gate", "First", "Fits", "Last" };
	int i;

	FixtureServe();
	UI_InitMemory();

	FixtureBeginLoad();
	UI_LoadArenas();
	FixtureCheckOpened( "scripts/arenas.txt", "first.arena", fixtureArena119, "last.arena", "UI_LoadArenas opens" );
	FixtureCheckInfos( ui_arenaInfos, ui_numArenas, "map", fixtureArenaMaps, FIXTURE_COUNT( fixtureArenaMaps ), "UI_LoadArenas arenas" );
	Check( uiInfo.mapCount == FIXTURE_COUNT( fixtureArenaMaps ), "UI_LoadArenas map list" );
	for ( i = 0; i < uiInfo.mapCount; i++ ) {
		Check( !strcmp( uiInfo.mapList[i].mapLoadName, fixtureArenaMaps[i] ), "UI_LoadArenas map load names" );
		Check( !strcmp( uiInfo.mapList[i].mapName, mapNames[i] ), "UI_LoadArenas map names" );
	}
	Check( uiInfo.mapList[0].typeBits == ( ( 1 << GT_FFA ) | ( 1 << GT_TOURNAMENT ) ), "arenas.txt map types" );
	Check( uiInfo.mapList[2].typeBits == ( 1 << GT_CTF ), "119-byte name's map types" );

	FixtureBeginLoad();
	UI_LoadBots();
	FixtureCheckOpened( "scripts/bots.txt", "first.bot", fixtureBot119, "last.bot", "UI_LoadBots opens" );
	FixtureCheckInfos( ui_botInfos, ui_numBots, "name", fixtureBotNames, FIXTURE_COUNT( fixtureBotNames ), "UI_LoadBots bots" );
	Check( !strcmp( Info_ValueForKey( UI_GetBotInfoByName( "Fits" ), "model" ), "visor" ), "119-byte name's bot" );

	puts( "Team Arena UI arena and bot file name regressions passed (issue #440)" );
	return 0;
}
