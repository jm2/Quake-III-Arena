/* Issue #382: a click or key in the base q3_ui Team Orders menu selects only a row
 * that the shown list has. UI_CursorInRect includes the bottom edge of the list, and
 * page up/down move the cursor of the zero-height list off it, so a row one past the
 * end read botNames[9] from a full bot list, or a NULL or out-of-bounds order format.
 * The fixture drives the real menu (ui_teamorders.c included here, ui_qmenu.c and
 * ui_atoms.c linked) through UI_MouseEvent and the menu's key handler. */
#include "../code/q3_ui/ui_teamorders.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOCAL_CLIENT 3
#define MAX_PLAYERS 16
#define BOT_ROWS 9
/* the list is 200 pixels wide around the middle of the screen */
#define LIST_X 320

static char serverInfo[MAX_INFO_STRING];
static int commands;
static char lastCommand[MAX_STRING_CHARS];
static sfxHandle_t sounds;

/** Fail with a description of the first mismatch. */
static void Check( int ok, const char *what ) {
	if ( !ok ) {
		fprintf( stderr, "q3_ui Team Orders keys regression failed: %s\n", what );
		exit( 1 );
	}
}

void trap_Error( const char *string ) {
	fprintf( stderr, "Unexpected trap_Error: %s\n", string );
	exit( 1 );
}

void trap_Print( const char *string ) {
	(void)string;
}

void trap_GetClientState( uiClientState_t *state ) {
	memset( state, 0, sizeof( *state ) );
	state->connState = CA_ACTIVE;
	state->clientNum = LOCAL_CLIENT;
}

/** A full blue team: the local client is a human and every other slot is a bot. */
int trap_GetConfigString( int index, char *buff, int buffsize ) {
	int n = index - CS_PLAYERS;

	if ( index == CS_SERVERINFO ) {
		Q_strncpyz( buff, serverInfo, buffsize );
	} else if ( n == LOCAL_CLIENT ) {
		Com_sprintf( buff, buffsize, "\\n\\Player\\t\\%d\\model\\sarge", TEAM_BLUE );
	} else if ( n >= 0 && n < MAX_PLAYERS ) {
		Com_sprintf( buff, buffsize, "\\n\\B%03d\\t\\%d\\model\\sarge\\skill\\3", n, TEAM_BLUE );
	} else {
		Check( 0, "config string index" );
	}
	return qtrue;
}

/** The menu sends its order with this; keep the last one. */
void trap_Cmd_ExecuteText( int exec_when, const char *text ) {
	Check( exec_when == EXEC_APPEND, "orders are appended" );
	Q_strncpyz( lastCommand, text, sizeof( lastCommand ) );
	commands++;
}

qhandle_t trap_R_RegisterShaderNoMip( const char *name ) {
	(void)name;
	return 1;
}

sfxHandle_t trap_S_RegisterSound( const char *sample, qboolean compressed ) {
	(void)sample;
	(void)compressed;
	return ++sounds;
}

void trap_S_StartLocalSound( sfxHandle_t sfx, int channelNum ) {
	(void)sfx;
	(void)channelNum;
}

void trap_Key_SetCatcher( int catcher ) {
	(void)catcher;
}

int trap_Key_GetCatcher( void ) {
	return KEYCATCH_UI;
}

void trap_Key_ClearStates( void ) {
}

void trap_Cvar_Set( const char *var_name, const char *value ) {
	(void)var_name;
	(void)value;
}

/** The list draws itself, but nothing draws here. */
void trap_R_SetColor( const float *rgba ) {
	(void)rgba;
	Check( 0, "nothing draws" );
}

void trap_R_DrawStretchPic( float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader ) {
	(void)x; (void)y; (void)w; (void)h; (void)s1; (void)t1; (void)s2; (void)t2; (void)hShader;
	Check( 0, "nothing draws" );
}

/** The menu has no text field. */
void MenuField_Init( menufield_s *m ) {
	(void)m;
	Check( 0, "no text field" );
}

sfxHandle_t MenuField_Key( menufield_s *m, int *key ) {
	(void)m;
	(void)key;
	Check( 0, "no text field" );
	return 0;
}

/** The name on bot row row: "Everyone", then the bots in slot order without the
 * local client. */
static const char *RowName( int row ) {
	static char name[16];

	if ( row == 0 ) {
		return "Everyone";
	}
	Com_sprintf( name, sizeof( name ), "B%03d", row <= LOCAL_CLIENT ? row - 1 : row );
	return name;
}

/** The row of a list of rows rows that pixel row y shows, or -1 off its rows. */
static int Row( int y, int rows ) {
	int top = teamOrdersMenuInfo.list.generic.top;

	if ( y < top || y >= top + rows * PROP_HEIGHT ) {
		return -1;
	}
	return ( y - top ) / PROP_HEIGHT;
}

/** The middle pixel row of row row. */
static int RowY( int row ) {
	return teamOrdersMenuInfo.list.generic.top + row * PROP_HEIGHT + PROP_HEIGHT / 2;
}

/** Open the menu from the console command, with the cursor on the full bot list. */
static void OpenMenu( void ) {
	uis.menusp = 0;
	uis.activemenu = NULL;
	commands = 0;
	lastCommand[0] = 0;
	UI_TeamOrdersMenu_f();
	Check( uis.activemenu == &teamOrdersMenuInfo.menu, "the menu opens" );
	Check( Menu_ItemAtCursor( uis.activemenu ) == &teamOrdersMenuInfo.list, "the list has the cursor" );
	Check( teamOrdersMenuInfo.list.generic.id == ID_LIST_BOTS && teamOrdersMenuInfo.list.curvalue == 0,
		"the bot list comes first" );
}

/** Move the mouse to pixel (x, y), as the client does, and click. */
static sfxHandle_t Click( int x, int y ) {
	uis.cursorx = x;
	uis.cursory = y;
	UI_MouseEvent( 0, 0 );
	Check( uis.cursorx == x && uis.cursory == y, "mouse position" );
	return uis.activemenu->key( K_MOUSE1 );
}

/** Press a key, as UI_KeyEvent does. */
static sfxHandle_t Press( int key ) {
	return uis.activemenu->key( key );
}

/** The orders of the game type, and how many there are. */
static const char **Orders( int *rows, int *id ) {
	if ( teamOrdersMenuInfo.gametype == GT_CTF ) {
		*rows = NUM_CTF_ORDERS;
		*id = ID_LIST_CTF_ORDERS;
		return ctfMessages;
	}
	*rows = NUM_TEAM_ORDERS;
	*id = ID_LIST_TEAM_ORDERS;
	return teamMessages;
}

/** The menu sent order row of the orders to the bot on bot row bot and closed. */
static void CheckOrder( int row, int bot ) {
	char message[256];
	char expected[MAX_STRING_CHARS];
	const char **messages;
	int rows, id;

	messages = Orders( &rows, &id );
	Check( row >= 0 && row < rows, "order row" );
	Com_sprintf( message, sizeof( message ), messages[row], RowName( bot ) );
	Com_sprintf( expected, sizeof( expected ), "say_team \"%s\"\n", message );
	Check( commands == 1 && !strcmp( lastCommand, expected ), "the order sent" );
	Check( uis.activemenu == NULL, "the menu closes after an order" );
}

/** The menu shows the orders for the bot on bot row bot, with row row selected. */
static void CheckOrders( int bot, int row ) {
	int rows, id;

	Orders( &rows, &id );
	Check( uis.activemenu == &teamOrdersMenuInfo.menu, "the menu stays open" );
	Check( teamOrdersMenuInfo.list.generic.id == id && teamOrdersMenuInfo.list.numitems == rows, "the orders are shown" );
	Check( teamOrdersMenuInfo.selectedBot == bot, "the selected bot" );
	Check( teamOrdersMenuInfo.list.curvalue == row, "the selected order" );
	Check( commands == 0, "nothing sent before an order" );
}

/** Click every pixel row of the full bot list and around it. A bot row opens the
 * orders for that bot, and an order with the bot's name goes to it; the bottom edge
 * of the list, one past the last row, selects nothing. */
static void TestBotClicks( void ) {
	int y, row;
	sfxHandle_t sound;

	for ( y = 0; y <= SCREEN_HEIGHT; y++ ) {
		OpenMenu();
		Check( teamOrdersMenuInfo.numBots == BOT_ROWS && teamOrdersMenuInfo.list.numitems == BOT_ROWS,
			"a full bot list" );
		row = Row( y, BOT_ROWS );
		sound = Click( LIST_X, y );
		if ( teamOrdersMenuInfo.list.generic.id != ID_LIST_BOTS ) {
			/* order the chosen bot to defend the base (CTF) or to follow (team) */
			Click( LIST_X, RowY( 1 ) );
			Check( row >= 0, "a click off the bot rows opens no orders" );
			CheckOrder( 1, row );
		}
		if ( row >= 0 ) {
			Check( sound == menu_move_sound && commands == 1, "a bot row click opens its orders" );
		} else {
			Check( sound == menu_null_sound, "a click off the bot rows is silent" );
			Check( teamOrdersMenuInfo.list.curvalue == 0 && commands == 0, "a click off the bot rows does nothing" );
		}
	}
}

/** For every bot row, click every pixel row of the orders and around them. An order
 * row sends that order; the bottom edge of the list, one past the last row (a NULL
 * format), sends nothing. */
static void TestOrderClicks( void ) {
	int rows, id, bots, bot, y, row;
	sfxHandle_t sound;

	OpenMenu();
	bots = teamOrdersMenuInfo.numBots;
	for ( bot = 0; bot < bots; bot++ ) {
		for ( y = 0; y <= SCREEN_HEIGHT; y++ ) {
			OpenMenu();
			Check( Click( LIST_X, RowY( bot ) ) == menu_move_sound, "a bot row click sound" );
			Orders( &rows, &id );
			CheckOrders( bot, bot < rows ? bot : 0 );
			row = Row( y, rows );
			sound = Click( LIST_X, y );
			if ( row >= 0 ) {
				Check( sound == menu_move_sound, "an order row click sound" );
				CheckOrder( row, bot );
			} else {
				Check( sound == menu_null_sound, "a click off the order rows is silent" );
				CheckOrders( bot, bot < rows ? bot : 0 );
			}
		}
	}
	Check( teamOrdersMenuInfo.numBots == BOT_ROWS, "a full bot list" );
}

/** Choose bot row bot with the arrow keys and enter. */
static void ChooseBot( int bot ) {
	int n;

	OpenMenu();
	for ( n = 0; n < bot; n++ ) {
		Check( Press( K_DOWNARROW ) == menu_move_sound, "down arrow" );
	}
	Check( teamOrdersMenuInfo.list.curvalue == bot, "the arrow keys select the bot" );
	Check( Press( K_ENTER ) == menu_move_sound, "enter chooses the bot" );
}

/** The keyboard: every bot row opens its orders on a row they have, the bot row if
 * there is one; page up/down past either end of a list and enter choose nothing. */
static void TestKeys( void ) {
	int rows, id, bot;

	for ( bot = 0; bot < BOT_ROWS; bot++ ) {
		ChooseBot( bot );
		Orders( &rows, &id );
		CheckOrders( bot, bot < rows ? bot : 0 );
		Check( Press( K_ENTER ) == menu_move_sound, "enter sends the order" );
		CheckOrder( bot < rows ? bot : 0, bot );

		/* orders: page down from the first row and page up from the last */
		ChooseBot( bot );
		Press( K_HOME );
		Press( K_PGDN );	/* a zero-height list's page down goes to row -1 */
		Press( K_ENTER );
		CheckOrders( bot, teamOrdersMenuInfo.list.curvalue );
		Press( K_END );
		Press( K_PGUP );	/* and page up to one past the last row */
		Press( K_ENTER );
		CheckOrders( bot, teamOrdersMenuInfo.list.curvalue );
		Press( K_END );
		Press( K_ENTER );
		CheckOrder( rows - 1, bot );
	}

	/* bots: page down from the first row and page up from the last */
	OpenMenu();
	Press( K_HOME );
	Press( K_PGDN );
	Press( K_ENTER );
	Check( teamOrdersMenuInfo.list.generic.id == ID_LIST_BOTS && commands == 0, "enter off the bot rows" );
	Press( K_END );
	Press( K_PGUP );
	Press( K_ENTER );
	Check( teamOrdersMenuInfo.list.generic.id == ID_LIST_BOTS && commands == 0, "enter off the bot rows" );
	Press( K_END );
	Press( K_ENTER );
	CheckOrders( BOT_ROWS - 1, 0 );
	Click( LIST_X, RowY( 1 ) );
	CheckOrder( 1, BOT_ROWS - 1 );
}

/** Run one input and game type per process, so a sanitizer report names the case. */
int main( int argc, char **argv ) {
	int gametype;

	if ( argc != 3 || ( strcmp( argv[1], "bots" ) && strcmp( argv[1], "orders" ) && strcmp( argv[1], "keys" ) )
		|| ( strcmp( argv[2], "ctf" ) && strcmp( argv[2], "team" ) ) ) {
		fprintf( stderr, "usage: %s bots|orders|keys ctf|team\n", argv[0] );
		return 2;
	}
	gametype = strcmp( argv[2], "ctf" ) ? GT_TEAM : GT_CTF;
	Com_sprintf( serverInfo, sizeof( serverInfo ), "\\sv_maxclients\\%d\\g_gametype\\%d", MAX_PLAYERS, gametype );
	Menu_Cache();
	Check( menu_null_sound != menu_move_sound, "menu sounds" );

	if ( !strcmp( argv[1], "bots" ) ) {
		TestBotClicks();
	} else if ( !strcmp( argv[1], "orders" ) ) {
		TestOrderClicks();
	} else {
		TestKeys();
	}
	printf( "q3_ui Team Orders %s input in %s selects only rows its lists have (issue #382)\n", argv[1], argv[2] );
	return 0;
}
