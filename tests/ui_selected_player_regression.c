/* Issue #379: a server can set cg_selectedPlayer to anything through systeminfo,
 * so the Team Arena UI must bound it before it indexes the teammate lists. */
#include "../code/ui/ui_main.c"
#include "selected_player_systeminfo.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOCAL_CLIENT 2
#define LOCAL_NAME "Local"
#define TEAMMATES 3	/* clients 1, 2 and 4 are blue, 0 and 3 red */
#define EVERYONE TEAMMATES
#define NO_SELECTION -1
#define UNTOUCHED "untouched"

void dllEntry( int (QDECL *syscallptr)( int arg, ... ) );	/* ui_syscalls.c */

static const int teammateClients[TEAMMATES] = { 1, LOCAL_CLIENT, 4 };
static const char *teammateNames[TEAMMATES] = { "P1", LOCAL_NAME, "P4" };
static char configStrings[MAX_CONFIGSTRINGS][MAX_INFO_STRING];
static char executed[MAX_STRING_CHARS];
static const char *selection;

/** Fail with the selection under test and the contract that broke. */
static void Check( int ok, const char *what ) {
	if ( !ok ) {
		fprintf( stderr, "UI selected player regression failed (cg_selectedPlayer %s): %s\n", selection, what );
		exit( 1 );
	}
}

/** cl_ui.c's syscalls on the tested paths; cvars go to the real cvar.c. */
static int QDECL FakeSyscall( int command, ... ) {
	va_list ap;
	int result = 0;

	va_start( ap, command );
	switch ( command ) {
	case UI_ERROR:
		Check( 0, va_arg( ap, const char * ) );
		break;
	case UI_PRINT:
		break;
	case UI_CVAR_REGISTER: {
		vmCvar_t *vmCvar = va_arg( ap, vmCvar_t * );
		const char *name = va_arg( ap, const char * );
		const char *value = va_arg( ap, const char * );
		Cvar_Register( vmCvar, name, value, va_arg( ap, int ) );
		break;
	}
	case UI_CVAR_UPDATE:
		Cvar_Update( va_arg( ap, vmCvar_t * ) );
		break;
	case UI_CVAR_SET: {
		const char *name = va_arg( ap, const char * );
		Cvar_Set( name, va_arg( ap, const char * ) );
		break;
	}
	case UI_CVAR_VARIABLEVALUE: {
		float value = Cvar_VariableValue( va_arg( ap, const char * ) );
		memcpy( &result, &value, sizeof( result ) );	// FloatAsInt
		break;
	}
	case UI_CVAR_VARIABLESTRINGBUFFER: {
		const char *name = va_arg( ap, const char * );
		char *buffer = va_arg( ap, char * );
		Cvar_VariableStringBuffer( name, buffer, va_arg( ap, int ) );
		break;
	}
	case UI_CMD_EXECUTETEXT:
		Check( va_arg( ap, int ) == EXEC_APPEND, "orders are appended" );
		Q_strcat( executed, sizeof( executed ), va_arg( ap, const char * ) );
		break;
	case UI_GETCLIENTSTATE: {
		uiClientState_t *state = va_arg( ap, uiClientState_t * );
		memset( state, 0, sizeof( *state ) );
		state->connState = CA_ACTIVE;
		state->clientNum = LOCAL_CLIENT;
		break;
	}
	case UI_GETCONFIGSTRING: {
		int index = va_arg( ap, int );
		char *buffer = va_arg( ap, char * );
		int size = va_arg( ap, int );
		Check( index >= 0 && index < MAX_CONFIGSTRINGS, "configstring index" );
		Q_strncpyz( buffer, configStrings[index], size );
		result = 1;
		break;
	}
	case UI_KEY_GETCATCHER:
		result = KEYCATCH_UI;
		break;
	case UI_KEY_SETCATCHER:
	case UI_KEY_CLEARSTATES:
		break;
	default:
		Check( 0, va( "unexpected UI syscall %d", command ) );
	}
	va_end( ap );
	return result;
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

/** Let the server select value, then let the UI pick it up as a frame does. */
static void Select( void ) {
	SystemInfo_Set( "cg_selectedPlayer", selection );
	Cvar_Set( "cg_selectedPlayerName", UNTOUCHED );
	UI_UpdateCvars();
	Check( !strcmp( ui_selectedPlayer.string, selection ), "systeminfo reached ui_selectedPlayer" );
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

/** One selection per process: its teammate index (or Everyone), or a value a
 * server set that names nobody. */
int main( int argc, char **argv ) {
	char *end;
	long value;
	int selected;

	if ( argc != 2 ) {
		fprintf( stderr, "usage: %s <cg_selectedPlayer>\n", argv[0] );
		return 2;
	}
	selection = argv[1];
	value = strtol( selection, &end, 10 );
	Check( *selection && !*end, "a decimal selection" );
	selected = value >= 0 && value <= EVERYONE ? (int)value : NO_SELECTION;

	dllEntry( FakeSyscall );
	UI_RegisterCvars();
	Cvar_Set( "name", LOCAL_NAME );
	Serve();
	TestSelection( selected );
	printf( "Team Arena UI bounds cg_selectedPlayer %s from systeminfo (issue #379)\n", selection );
	return 0;
}
