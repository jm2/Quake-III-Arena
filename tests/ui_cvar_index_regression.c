/* Issue #379: a server can set any cvar through systeminfo, so the Team Arena
 * UI must bound cg_selectedPlayer before it indexes the teammate lists, and
 * color1 before it indexes gamecodetoui. Issue #390: the orders script formats
 * the menu's string with a client number for Everyone too, and skips the local
 * player by client number. Issue #401: the orders and voiceOrders scripts only
 * format a menu string with at most one conversion, a plain %i or %d. */
#include "../code/ui/ui_local.h"
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* The orders strings come from the menus, so -Wformat cannot check them against
 * ui_main.c's arguments. Every va and Com_sprintf in ui_main.c goes through a
 * checker instead, which gets the type of the call's first argument from the
 * call site, so it never reads an argument as a type it was not passed as. */
typedef enum { FORMAT_NO_ARG, FORMAT_INT, FORMAT_STRING, FORMAT_OTHER } formatArg_t;
struct formatNoArg_s;
#define FORMAT_ARG( x ) _Generic( ( x ), int: FORMAT_INT, char *: FORMAT_STRING, \
	const char *: FORMAT_STRING, struct formatNoArg_s *: FORMAT_NO_ARG, default: FORMAT_OTHER )
#define FORMAT_FIRST_ARG( fmt, x, ... ) FORMAT_ARG( x )
#define va( ... ) CheckedVa( FORMAT_FIRST_ARG( __VA_ARGS__, (struct formatNoArg_s *)0, 0 ), __VA_ARGS__ )
#define Com_sprintf( dest, size, ... ) CheckedSprintf( dest, size, \
	FORMAT_FIRST_ARG( __VA_ARGS__, (struct formatNoArg_s *)0, 0 ), __VA_ARGS__ )

static char *CheckedVa( formatArg_t arg, const char *fmt, ... ) __attribute__(( format( printf, 2, 3 ) ));
static void CheckedSprintf( char *dest, int size, formatArg_t arg, const char *fmt, ... )
	__attribute__(( format( printf, 4, 5 ) ));
static void Check( int ok, const char *what );

static const char *checkedFormat;	/* the orders string under test */
static int formattedClients[MAX_CLIENTS];	/* its argument in each call */
static int formattedCount;

/** A call that formats the orders string under test: it must have at most one
 * conversion, a plain %i or %d with no flags, width, precision or length, and
 * pass an int, which is recorded. A string that is anything else, such as %s,
 * %n, %40000d or two conversions, must never reach a formatter (issue #401).
 * Other calls pass through. */
static void CheckFormat( formatArg_t arg, const char *fmt, va_list ap ) {
	const char *c;
	int conversions = 0;
	int bad = 0;

	if ( !checkedFormat || strcmp( fmt, checkedFormat ) ) {
		return;
	}
	for ( c = strchr( fmt, '%' ); c; c = strchr( c + 1, '%' ) ) {
		c++;
		if ( *c == 'i' || *c == 'd' ) {
			conversions++;
		} else if ( *c != '%' ) {
			bad = 1;	// anything else, or a trailing %
			break;
		}
	}
	Check( !bad && conversions <= 1, "only an orders string with at most one plain %i or %d is formatted" );
	Check( arg == FORMAT_INT, "the orders string's %i or %d is passed an int" );
	Check( formattedCount < MAX_CLIENTS, "at most MAX_CLIENTS orders" );
	formattedClients[formattedCount++] = va_arg( ap, int );
}

static char *CheckedVa( formatArg_t arg, const char *fmt, ... ) {
	static char string[2][32000];	// as va
	static int index;
	char *buf = string[index++ & 1];
	va_list ap;

	va_start( ap, fmt );
	CheckFormat( arg, fmt, ap );
	va_end( ap );
	va_start( ap, fmt );
	vsnprintf( buf, sizeof( string[0] ), fmt, ap );
	va_end( ap );
	return buf;
}

static void CheckedSprintf( char *dest, int size, formatArg_t arg, const char *fmt, ... ) {
	va_list ap;

	va_start( ap, fmt );
	CheckFormat( arg, fmt, ap );
	va_end( ap );
	va_start( ap, fmt );
	vsnprintf( dest, size, fmt, ap );
	va_end( ap );
}

#include "../code/ui/ui_main.c"
#include "ui_cvar_syscalls.h"

#define ARRAY_LEN( a ) ( (int)( sizeof( a ) / sizeof( ( a )[0] ) ) )
#define LOCAL_NAME "Local"
#define LOCAL_USERINFO_NAME "^1" LOCAL_NAME	/* the name cvar and the server keep colors */
#define TEAMMATES 3	/* clients 1, LOCAL_CLIENT and 4 are blue, 0 and 3 red */
#define EVERYONE TEAMMATES
#define NO_SELECTION -1
#define UNTOUCHED "untouched"
#define WHITE 6	/* the UI's effects color for game color 7 */

static const int teammateClients[TEAMMATES] = { 1, LOCAL_CLIENT, 4 };
static const char *teammateNames[TEAMMATES] = { "P1", LOCAL_NAME, "P4" };
static const int otherTeammateClients[] = { 1, 4 };	/* the teammates but LOCAL_CLIENT */
/* orders strings with the command each sends client 1: a vtell like the
 * fixture's, one with %% literals, two that retail sends as they are (with %%
 * as %), and the seven voiceOrders strings of Team Arena's ingame_orders.menu */
static const struct {
	const char *orders, *client1;
} ordersStrings[] = {
	{ "vtell %i attack", "vtell 1 attack" },
	{ "say_team %%%i %%i 100%%", "say_team %1 %i 100%" },
	{ "cmd vsay_team offense", "cmd vsay_team offense" },
	{ "cmd vtell %%d offense", "cmd vtell %d offense" },
	{ "cmd vtell %d offense; +button7; wait; -button7", "cmd vtell 1 offense; +button7; wait; -button7" },
	{ "cmd vtell %d defend; +button8; wait; -button8", "cmd vtell 1 defend; +button8; wait; -button8" },
	{ "cmd vtell %d patrol; +button9; wait; -button9", "cmd vtell 1 patrol; +button9; wait; -button9" },
	{ "cmd vtell %d followme; +button10; wait; -button10", "cmd vtell 1 followme; +button10; wait; -button10" },
	{ "cmd vtell %d camp", "cmd vtell 1 camp" },
	{ "cmd vtell %d followflagcarrier", "cmd vtell 1 followflagcarrier" },
	{ "cmd vtell %d returnflag", "cmd vtell 1 returnflag" },
};
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
			n == LOCAL_CLIENT ? LOCAL_USERINFO_NAME : va( "P%d", n ), n % 3 ? TEAM_BLUE : TEAM_RED,
			n == LOCAL_CLIENT );
	}
}

/** Let the server select a value, then let the UI pick it up as a frame does. */
static void Select( const char *selected ) {
	SystemInfo_Set( "cg_selectedPlayer", selected );
	Cvar_Set( "cg_selectedPlayerName", UNTOUCHED );
	UI_UpdateCvars();
	Check( !strcmp( ui_selectedPlayer.string, selected ), "systeminfo reached ui_selectedPlayer" );
}

/** Run one menu script, collecting the commands it appends. */
static void RunScript( const char *script ) {
	char buffer[MAX_STRING_CHARS];
	char *args = buffer;

	Q_strncpyz( buffer, script, sizeof( buffer ) );
	executed[0] = 0;
	UI_RunMenuScript( &args );
}

/** Run the orders or voiceOrders script with a menu's string, checking every call
 * that formats it. It must order these clients in turn, each with the string
 * formatted with their client number, as ioquake3 does. */
static void RunOrders( const char *script, const char *orders, const int *clients, int count ) {
	char expected[MAX_STRING_CHARS] = "";
	char command[MAX_STRING_CHARS];
	int i;

	checkedFormat = orders;
	formattedCount = 0;
	RunScript( va( "%s \"%s\"", script, orders ) );
	checkedFormat = NULL;
	Check( formattedCount == count, va( "%s formats one command per ordered client", script ) );
	for ( i = 0; i < count; i++ ) {
		Check( formattedClients[i] == clients[i], va( "%s formats the ordered client numbers", script ) );
		Com_sprintf( command, sizeof( command ), orders, clients[i] );
		Q_strcat( expected, sizeof( expected ), va( "%s\n", command ) );
	}
	Check( !strcmp( executed, expected ), va( "%s commands", script ) );
}

/** A menu string with more than one conversion, or one that is not a plain %i
 * or %d (issue #401): orders, for a teammate and for Everyone, and voiceOrders,
 * for a teammate, refuse it, format and send nothing, and warn developers.
 * voiceOrdersTeam never formats its string: it sends it as it is, for Everyone
 * only. */
static void TestRefused( void ) {
	static const char *selections[] = { "0", "3" };	/* client 1, and EVERYONE */
	int i;

	Cvar_Set( "developer", "1" );
	for ( i = 0; i < ARRAY_LEN( selections ); i++ ) {
		Select( selections[i] );
		UI_BuildPlayerList();
		Check( uiInfo.myTeamCount == TEAMMATES, "blue team list" );
		printed[0] = 0;
		RunOrders( "orders", value, NULL, 0 );
		Check( strstr( printed, "WARNING: orders refused" ) != NULL, "orders warns developers" );
		printed[0] = 0;
		RunOrders( "voiceOrders", value, NULL, 0 );
		Check( i ? !printed[0] : strstr( printed, "WARNING: voiceOrders refused" ) != NULL,
			"voiceOrders warns developers when it has a teammate to order" );
		checkedFormat = value;
		formattedCount = 0;
		RunScript( va( "voiceOrdersTeam \"%s\"", value ) );
		checkedFormat = NULL;
		Check( formattedCount == 0 && !strcmp( executed, i ? va( "%s\n", value ) : "" ), "voiceOrdersTeam" );
	}
}

/** The selected player cvars after a key on the selected player item. */
static void HandleKey( int key, int expected ) {
	char name[MAX_CVAR_VALUE_STRING];

	Select( value );
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
	char client1[MAX_STRING_CHARS];
	int i;

	// the team leader keeps the server's value; the name follows a teammate, the
	// first one when the value names nobody
	Select( value );
	UI_BuildPlayerList();
	Check( uiInfo.teamLeader && uiInfo.myTeamCount == TEAMMATES, "blue team list" );
	Check( !memcmp( uiInfo.teamClientNums, teammateClients, sizeof( teammateClients ) ), "teammate clients" );
	Cvar_VariableStringBuffer( "cg_selectedPlayerName", name, sizeof( name ) );
	Check( !strcmp( name, selected == EVERYONE ? UNTOUCHED : teammateNames[selected < 0 ? 0 : selected] ),
		"cg_selectedPlayerName from the player list" );

	// leader-only items hide when the leader selected themselves
	Check( UI_OwnerDrawVisible( UI_SHOW_LEADER ) == !self, "UI_SHOW_LEADER" );
	Check( UI_OwnerDrawVisible( UI_SHOW_NOTLEADER ) == self, "UI_SHOW_NOTLEADER" );

	// orders go to the selected teammate, else to every teammate but the local
	// client, which only its client number identifies (its name has a color code
	// that the team list cleans off); voiceOrders only go to a selected teammate.
	// Client 1 is first whenever it is ordered, and gets the command byte for byte.
	for ( i = 0; i < ARRAY_LEN( ordersStrings ); i++ ) {
		const char *orders = ordersStrings[i].orders;

		Com_sprintf( client1, sizeof( client1 ), "%s\n", ordersStrings[i].client1 );
		if ( selected >= 0 && selected < TEAMMATES ) {
			RunOrders( "orders", orders, &teammateClients[selected], 1 );
			Check( selected || !strcmp( executed, client1 ), "orders sends client 1 the command" );
			RunOrders( "voiceOrders", orders, &teammateClients[selected], 1 );
			Check( selected || !strcmp( executed, client1 ), "voiceOrders sends client 1 the command" );
		} else {
			RunOrders( "orders", orders, otherTeammateClients, ARRAY_LEN( otherTeammateClients ) );
			Check( !strncmp( executed, client1, strlen( client1 ) ), "orders sends client 1 the command" );
			RunOrders( "voiceOrders", orders, NULL, 0 );
		}
	}
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

	if ( argc != 3 || ( strcmp( argv[1], "selected" ) && strcmp( argv[1], "color1" ) && strcmp( argv[1], "refused" ) ) ) {
		fprintf( stderr, "usage: %s selected|color1 <value>, or refused <orders string>\n", argv[0] );
		return 2;
	}
	mode = argv[1];
	value = argv[2];
	number = strtol( value, &end, 10 );
	Check( !strcmp( mode, "refused" ) || ( *value && !*end ), "a decimal value" );

	dllEntry( FakeSyscall );
	// the userinfo cvars CL_Init creates
	Cvar_Set( "name", LOCAL_USERINFO_NAME );
	Cvar_Set( "color1", "4" );
	if ( !strcmp( mode, "color1" ) ) {
		TestEffectsColor( number >= 1 && number <= 7 ? (int)number : 0 );
		printf( "Team Arena UI bounds color1 %s from systeminfo (issue #379)\n", value );
	} else if ( !strcmp( mode, "refused" ) ) {
		Init();
		Serve();
		TestRefused();
		printf( "Team Arena UI refuses the orders string %s (issue #401)\n", value );
	} else {
		Init();
		Serve();
		TestSelection( number >= 0 && number <= EVERYONE ? (int)number : NO_SELECTION );
		printf( "Team Arena UI bounds cg_selectedPlayer %s from systeminfo and orders client numbers (issues #379, #390)\n", value );
	}
	return 0;
}
