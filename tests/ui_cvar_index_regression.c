/* Issue #379: a server can set any cvar through systeminfo, so the Team Arena
 * UI must bound cg_selectedPlayer before it indexes the teammate lists, and
 * color1 before it indexes gamecodetoui. */
#include "../code/ui/ui_main.c"
#include "ui_cvar_syscalls.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define LOCAL_NAME "Local"
#define TEAMMATES 3	/* clients 1, LOCAL_CLIENT and 4 are blue, 0 and 3 red */
#define EVERYONE TEAMMATES
#define NO_SELECTION -1
#define UNTOUCHED "untouched"
#define WHITE 6	/* the UI's effects color for game color 7 */

static const int teammateClients[TEAMMATES] = { 1, LOCAL_CLIENT, 4 };
static const char *teammateNames[TEAMMATES] = { "P1", LOCAL_NAME, "P4" };
static const int uiColors[7] = { 4, 2, 3, 0, 5, 1, 6 };	/* game colors 1-7 in the UI's order */
static const char *mode, *value;

/** Fail with the case under test and the contract that broke. */
static void Check( int ok, const char *what ) {
	if ( !ok ) {
		fprintf( stderr, "UI cvar index regression failed (%s %s): %s\n", mode, value, what );
		exit( 1 );
	}
}

/** Start the UI as the client does, without _UI_Init's start-up log on stdout. */
static void Init( void ) {
	int saved, null;

	fflush( stdout );
	saved = dup( STDOUT_FILENO );
	null = open( "/dev/null", O_WRONLY );
	Check( saved >= 0 && null >= 0 && dup2( null, STDOUT_FILENO ) >= 0, "stdout redirected" );
	close( null );
	_UI_Init( qfalse );
	fflush( stdout );
	Check( dup2( saved, STDOUT_FILENO ) >= 0, "stdout restored" );
	close( saved );
}

/** The server's player strings: the local client leads the blue team. */
static void Serve( void ) {
	int n;

	Q_strncpyz( configStrings[CS_SERVERINFO], "\\sv_maxclients\\8\\g_gametype\\4", MAX_INFO_STRING );
	for ( n = 0; n < 5; n++ ) {
		Com_sprintf( configStrings[CS_PLAYERS + n], MAX_INFO_STRING, "\\n\\%s\\t\\%d\\tl\\%d",
			n == LOCAL_CLIENT ? LOCAL_NAME : va( "P%d", n ), n % 3 ? TEAM_BLUE : TEAM_RED,
			n == LOCAL_CLIENT );
	}
}

/** Let the server select the value, then let the UI pick it up as a frame does. */
static void Select( void ) {
	SystemInfo_Set( "cg_selectedPlayer", value );
	Cvar_Set( "cg_selectedPlayerName", UNTOUCHED );
	UI_UpdateCvars();
	Check( !strcmp( ui_selectedPlayer.string, value ), "systeminfo reached ui_selectedPlayer" );
}

/** Run one menu script, collecting the commands it appends. */
static void RunScript( const char *script ) {
	char buffer[MAX_STRING_CHARS];
	char *args = buffer;

	Q_strncpyz( buffer, script, sizeof( buffer ) );
	executed[0] = 0;
	UI_RunMenuScript( &args );
}

/** The selected player cvars after a key on the selected player item. */
static void HandleKey( int key, int expected ) {
	char name[MAX_CVAR_VALUE_STRING];

	Select();
	UI_SelectedPlayer_HandleKey( 0, NULL, key );
	Cvar_VariableStringBuffer( "cg_selectedPlayerName", name, sizeof( name ) );
	Check( Cvar_VariableValue( "cg_selectedPlayer" ) == expected, "selected player after a key" );
	Check( !strcmp( name, expected == EVERYONE ? "Everyone" : teammateNames[expected] ),
		"selected player name after a key" );
}

/** Every consumer of cg_selectedPlayer, with the teammate it selects (a teammate
 * index, EVERYONE, or NO_SELECTION for values that name nobody). */
static void TestSelection( int selected ) {
	int self = selected == 1;
	char name[MAX_CVAR_VALUE_STRING];

	// the team leader keeps the server's value; the name follows a teammate, the
	// first one when the value names nobody
	Select();
	UI_BuildPlayerList();
	Check( uiInfo.teamLeader && uiInfo.myTeamCount == TEAMMATES, "blue team list" );
	Check( !memcmp( uiInfo.teamClientNums, teammateClients, sizeof( teammateClients ) ), "teammate clients" );
	Cvar_VariableStringBuffer( "cg_selectedPlayerName", name, sizeof( name ) );
	Check( !strcmp( name, selected == EVERYONE ? UNTOUCHED : teammateNames[selected < 0 ? 0 : selected] ),
		"cg_selectedPlayerName from the player list" );

	// leader-only items hide when the leader selected themselves
	Check( UI_OwnerDrawVisible( UI_SHOW_LEADER ) == !self, "UI_SHOW_LEADER" );
	Check( UI_OwnerDrawVisible( UI_SHOW_NOTLEADER ) == self, "UI_SHOW_NOTLEADER" );

	// orders go to the selected teammate, else to every other teammate by name
	if ( selected >= 0 && selected < TEAMMATES ) {
		RunScript( "orders \"vtell %i attack\"" );
		Check( !strcmp( executed, va( "vtell %i attack\n", teammateClients[selected] ) ), "orders to a teammate" );
	} else {
		RunScript( "orders \"say_team attack\"" );
		Check( !strcmp( executed, "say_team attack\nsay_team attack\n" ), "orders to the team" );
	}
	RunScript( "voiceOrders \"vtell %i hello\"" );
	Check( !strcmp( executed, selected >= 0 && selected < TEAMMATES ?
		va( "vtell %i hello\n", teammateClients[selected] ) : "" ), "voiceOrders" );
	RunScript( "voiceOrdersTeam \"vsay_team hello\"" );
	Check( !strcmp( executed, selected == EVERYONE ? "vsay_team hello\n" : "" ), "voiceOrdersTeam" );

	// the selected player item steps through the teammates and Everyone; from
	// nobody, next is the first teammate and previous is Everyone
	HandleKey( K_MOUSE1, selected < 0 || selected == EVERYONE ? 0 : selected + 1 );
	HandleKey( K_MOUSE2, selected <= 0 ? EVERYONE : selected - 1 );
}

/** color1 (game colors 1-7) as the UI starts with it, then drawn and stepped by
 * the effects item: a value outside the seven colors shows white. */
static void TestEffectsColor( int color ) {
	rectDef_t rect = { 0, 20, 128, 8 };
	int expected = color >= 1 && color <= 7 ? uiColors[color - 1] : WHITE;

	SystemInfo_Set( "color1", value );
	Init();
	Check( uiInfo.effectsColor == expected, "effects color from color1" );
	drawnShader = 0;
	UI_DrawEffects( &rect, 1, colorWhite );
	Check( drawnShader == uiInfo.uiDC.Assets.fxPic[expected], "effects color drawn" );
	UI_Effects_HandleKey( 0, NULL, K_MOUSE1 );
	Check( Cvar_VariableValue( "color1" ) == uitogamecode[( expected + 1 ) % 7], "next effects color" );
}

/** One cvar and value per process: cg_selectedPlayer as a teammate index (or
 * Everyone), or color1 as a game color; anything else names nothing. */
int main( int argc, char **argv ) {
	char *end;
	long number;

	if ( argc != 3 || ( strcmp( argv[1], "selected" ) && strcmp( argv[1], "color1" ) ) ) {
		fprintf( stderr, "usage: %s selected|color1 <value>\n", argv[0] );
		return 2;
	}
	mode = argv[1];
	value = argv[2];
	number = strtol( value, &end, 10 );
	Check( *value && !*end, "a decimal value" );

	dllEntry( FakeSyscall );
	// the userinfo cvars CL_Init creates
	Cvar_Set( "name", LOCAL_NAME );
	Cvar_Set( "color1", "4" );
	if ( !strcmp( mode, "color1" ) ) {
		TestEffectsColor( number >= 1 && number <= 7 ? (int)number : 0 );
		printf( "Team Arena UI bounds color1 %s from systeminfo (issue #379)\n", value );
	} else {
		Init();
		Serve();
		TestSelection( number >= 0 && number <= EVERYONE ? (int)number : NO_SELECTION );
		printf( "Team Arena UI bounds cg_selectedPlayer %s from systeminfo (issue #379)\n", value );
	}
	return 0;
}
