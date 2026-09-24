/* Issue #379: a server can set any cvar through systeminfo, so the Team Arena
 * UI must bound cg_selectedPlayer before it indexes the teammate lists, and
 * color1 before it indexes gamecodetoui. Issue #390: the orders script formats
 * the menu's string with a client number for Everyone too, and skips the local
 * player by client number. */
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

/** A call that formats the orders string under test: its one conversion must be
 * %i or %d and be passed an int, which is recorded. Other calls pass through. */
static void CheckFormat( formatArg_t arg, const char *fmt, va_list ap ) {
	const char *c;
	int conversions = 0;
	char conversion = 0;

	if ( !checkedFormat || strcmp( fmt, checkedFormat ) ) {
		return;
	}
	for ( c = strchr( fmt, '%' ); c; c = strchr( c + 1, '%' ) ) {
		c += 1 + strspn( c + 1, "-+ #0123456789." );
		if ( !*c ) {
			break;
		}
		if ( *c != '%' ) {
			conversion = *c;
			conversions++;
		}
	}
	Check( conversions == 1 && ( conversion == 'i' || conversion == 'd' ), "the orders string has one %i or %d" );
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
static const char *teammateNames[TEAMMATES] = { "P1", LOCAL_NAME, LOCAL_NAME };
static const int otherTeammateClients[] = { 1, 4 };	/* the teammates but LOCAL_CLIENT */
/* a vtell like the fixture's, and the voiceOrders string of Team Arena's ingame_orders.menu */
static const char *ordersStrings[] = { "vtell %i attack", "cmd vtell %d offense; +button7; wait; -button7" };
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

/** The server's player strings: the local client leads the blue team, and its
 * teammate 4 has the same name in another color. */
static void Serve( void ) {
	int n;

	Q_strncpyz( configStrings[CS_SERVERINFO], "\\sv_maxclients\\8\\g_gametype\\4", MAX_INFO_STRING );
	for ( n = 0; n < 5; n++ ) {
		Com_sprintf( configStrings[CS_PLAYERS + n], MAX_INFO_STRING, "\\n\\%s\\t\\%d\\tl\\%d",
			n == LOCAL_CLIENT ? LOCAL_USERINFO_NAME : n == 4 ? "^4" LOCAL_NAME : va( "P%d", n ),
			n % 3 ? TEAM_BLUE : TEAM_RED, n == LOCAL_CLIENT );
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
	int i;

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

	// orders go to the selected teammate, else to every teammate but the local
	// client, which only its client number identifies (its name has a color code
	// that the team list cleans off, and teammate 4 has the same name in another
	// color); voiceOrders only go to a selected teammate
	for ( i = 0; i < ARRAY_LEN( ordersStrings ); i++ ) {
		if ( selected >= 0 && selected < TEAMMATES ) {
			RunOrders( "orders", ordersStrings[i], &teammateClients[selected], 1 );
			RunOrders( "voiceOrders", ordersStrings[i], &teammateClients[selected], 1 );
		} else {
			RunOrders( "orders", ordersStrings[i], otherTeammateClients, ARRAY_LEN( otherTeammateClients ) );
			RunOrders( "voiceOrders", ordersStrings[i], NULL, 0 );
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
 * the effects item: a value outside the seven colors shows white. The cgame
 * reads color1 with atoi (CG_ColorFromString), so the value followed by "e1" or
 * "x3" shows its color too: "1e1" is drawn blue and "0x3" white, where atof
 * would read white and cyan. */
static void TestEffectsColor( int color ) {
	static const char *suffixes[] = { "e1", "x3" };
	rectDef_t rect = { 0, 20, 128, 8 };
	int expected = color >= 1 && color <= 7 ? uiColors[color - 1] : WHITE;
	int i;

	SystemInfo_Set( "color1", value );
	Init();
	Check( uiInfo.effectsColor == expected, "effects color from color1" );
	drawnShader = 0;
	UI_DrawEffects( &rect, 1, colorWhite );
	Check( drawnShader == uiInfo.uiDC.Assets.fxPic[expected], "effects color drawn" );
	UI_Effects_HandleKey( 0, NULL, K_MOUSE1 );
	Check( Cvar_VariableValue( "color1" ) == uitogamecode[( expected + 1 ) % 7], "next effects color" );
	for ( i = 0; i < ARRAY_LEN( suffixes ); i++ ) {
		SystemInfo_Set( "color1", va( "%s%s", value, suffixes[i] ) );
		Init();
		Check( uiInfo.effectsColor == expected, va( "effects color from color1 %s%s, as the cgame reads it", value, suffixes[i] ) );
	}
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
	Cvar_Set( "name", LOCAL_USERINFO_NAME );
	Cvar_Set( "color1", "4" );
	if ( !strcmp( mode, "color1" ) ) {
		TestEffectsColor( number >= 1 && number <= 7 ? (int)number : 0 );
		printf( "Team Arena UI bounds color1 %s from systeminfo (issue #379)\n", value );
	} else {
		Init();
		Serve();
		TestSelection( number >= 0 && number <= EVERYONE ? (int)number : NO_SELECTION );
		printf( "Team Arena UI bounds cg_selectedPlayer %s from systeminfo and orders client numbers (issues #379, #390)\n", value );
	}
	return 0;
}
