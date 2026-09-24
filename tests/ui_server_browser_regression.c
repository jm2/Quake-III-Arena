/* Issue #40: the Team Arena server browser is linked natively, so values that
 * servers and masters send must stay inside its tables:
 * - netnames[nettype]: a server's infoResponse picks the nettype
 *   (cl_main.c CL_SetServerInfo copies it);
 * - displayServers[]: a master can list MAX_GLOBAL_SERVERS servers, twice
 *   MAX_DISPLAY_SERVERS;
 * - pings[]: a server's status reply can list more than MAX_CLIENTS players. */
#include "../code/ui/ui_main.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LISTED_SERVER 7

static char serverInfo[MAX_STRING_CHARS];
static char statusText[MAX_SERVERSTATUS_TEXT];
static int visible[MAX_GLOBAL_SERVERS];

/** Fail with a description of the first mismatch. */
static void Check( int ok, const char *what ) {
	if ( !ok ) {
		fprintf( stderr, "UI server browser regression failed: %s\n", what );
		exit( 1 );
	}
}

/** cl_ui.c LAN_GetServerInfo: every server answers with serverInfo. */
void trap_LAN_GetServerInfo( int source, int n, char *buf, int buflen ) {
	Check( n >= 0 && n < MAX_GLOBAL_SERVERS, "listed server read" );
	(void)source;
	Q_strncpyz( buf, serverInfo, buflen );
}

/** A master listed MAX_GLOBAL_SERVERS servers, and each one answered. */
int trap_LAN_GetServerCount( int source ) {
	Check( source == AS_GLOBAL, "global server count" );
	return MAX_GLOBAL_SERVERS;
}
int trap_LAN_ServerIsVisible( int source, int n ) {
	Check( source == AS_GLOBAL && n >= 0 && n < MAX_GLOBAL_SERVERS, "visible server" );
	return visible[n];
}
void trap_LAN_MarkServerVisible( int source, int n, qboolean isVisible ) {
	Check( source == AS_GLOBAL && n >= -1 && n < MAX_GLOBAL_SERVERS, "marked server" );
	if ( n == -1 ) {
		for ( n = 0; n < MAX_GLOBAL_SERVERS; n++ ) {
			visible[n] = isVisible;
		}
		return;
	}
	visible[n] = isVisible;
}
int trap_LAN_GetServerPing( int source, int n ) {
	(void)source; (void)n;
	return 50;
}
/** cl_ui.c LAN_CompareServers: newest server first, as a sort key could order them. */
int trap_LAN_CompareServers( int source, int sortKey, int sortDir, int s1, int s2 ) {
	(void)source; (void)sortKey; (void)sortDir;
	Check( s1 >= 0 && s1 < MAX_GLOBAL_SERVERS && s2 >= 0 && s2 < MAX_GLOBAL_SERVERS, "compared servers" );
	return s1 > s2 ? -1 : s1 < s2 ? 1 : 0;
}
void trap_Cvar_VariableStringBuffer( const char *var_name, char *buffer, int bufsize ) {
	Check( !strcmp( var_name, "cl_motdString" ), "only the motd is read" );
	Q_strncpyz( buffer, "", bufsize );
}
/** The list box selection is not under test. */
void Menu_SetFeederSelection( menuDef_t *menu, int feeder, int index, const char *name ) {
	(void)menu; (void)feeder; (void)index; (void)name;
}

/** cl_main.c CL_ServerStatus: the reply text for the queried address. */
int trap_LAN_ServerStatus( const char *serverAddress, char *serverStatus, int maxLen ) {
	Check( serverAddress && !strcmp( serverAddress, "192.0.2.1:27960" ) && serverStatus, "status request" );
	Q_strncpyz( serverStatus, statusText, maxLen );
	return 1;
}

/** Fail on engine errors. */
void QDECL Com_Error( int level, const char *error, ... ) {
	(void)level;
	fprintf( stderr, "Unexpected Com_Error: %s\n", error );
	exit( 1 );
}

/** Nothing is printed on this path. */
void QDECL Com_Printf( const char *msg, ... ) {
	(void)msg;
}

/** The host column for a local server whose infoResponse says nettype. */
static const char *HostColumn( const char *nettype ) {
	qhandle_t handle;

	Com_sprintf( serverInfo, sizeof( serverInfo ),
		"\\hostname\\LAN Arena\\mapname\\q3dm17\\clients\\2\\sv_maxclients\\8\\ping\\25\\gametype\\0\\nettype\\%s",
		nettype );
	// the feeder rereads the server when the column changes
	UI_FeederItemText( FEEDER_SERVERS, 0, SORT_MAP, &handle );
	return UI_FeederItemText( FEEDER_SERVERS, 0, SORT_HOST, &handle );
}

/** netnames[] labels local servers; anything past its three names is unknown. */
static void TestNetType( void ) {
	static const char *hostile[] = { "3", "4", "-1", "1000000", "-1000000", "2147483647", "-2147483648", NULL };
	int i;

	memset( &uiInfo, 0, sizeof( uiInfo ) );
	ui_netSource.integer = AS_LOCAL;
	uiInfo.serverStatus.numDisplayServers = 1;
	uiInfo.serverStatus.displayServers[0] = LISTED_SERVER;

	// the engine's own values keep their labels
	Check( !strcmp( HostColumn( "0" ), "LAN Arena [???]" ), "nettype 0" );
	Check( !strcmp( HostColumn( "1" ), "LAN Arena [UDP]" ), "nettype 1" );
	Check( !strcmp( HostColumn( "2" ), "LAN Arena [IPX]" ), "nettype 2" );
	Check( !strcmp( HostColumn( "" ), "LAN Arena [???]" ), "no nettype" );
	// the NULL terminator and everything past netnames read as unknown
	for ( i = 0; hostile[i]; i++ ) {
		Check( !strcmp( HostColumn( hostile[i] ), "LAN Arena [???]" ), hostile[i] );
	}
}

/** Every server a master lists answers: the display list keeps the first
 * MAX_DISPLAY_SERVERS in sort order and the fields after it keep theirs. */
static void TestDisplayList( void ) {
	int i;

	memset( &uiInfo, 0, sizeof( uiInfo ) );
	memset( uiInfo.serverStatus.displayServers, 0x5a, sizeof( uiInfo.serverStatus.displayServers ) );
	ui_netSource.integer = AS_GLOBAL;
	ui_browserShowEmpty.integer = ui_browserShowFull.integer = 1;
	ui_joinGameType.integer = 0;
	uiInfo.joinGameTypes[0].gtEnum = -1;	// All
	ui_serverFilterType.integer = 0;
	Q_strncpyz( serverInfo, "\\hostname\\Arena\\mapname\\q3dm17\\clients\\1\\sv_maxclients\\8\\gametype\\0",
		sizeof( serverInfo ) );

	UI_BuildServerDisplayList( qtrue );

	Check( uiInfo.serverStatus.numDisplayServers == MAX_DISPLAY_SERVERS, "display list fills up" );
	Check( uiInfo.serverStatus.numPlayersOnServers == MAX_GLOBAL_SERVERS, "every server was read" );
	Check( !strcmp( uiInfo.serverStatus.motd, "Welcome to Team Arena!" ), "motd after the list" );
	for ( i = 0; i < MAX_DISPLAY_SERVERS; i++ ) {
		// the first MAX_DISPLAY_SERVERS listed, sorted newest first
		Check( uiInfo.serverStatus.displayServers[i] == MAX_DISPLAY_SERVERS - 1 - i, "display list order" );
	}
	for ( i = 0; i < MAX_GLOBAL_SERVERS; i++ ) {
		Check( !visible[i], "every answered server is done" );
	}
}

/** Player numbers of a status reply stay in pings[] and number the listed rows. */
static void TestStatusPlayers( int players ) {
	serverStatusInfo_t *info;
	int i, row, playerRows;

	Q_strncpyz( statusText, "\\sv_hostname\\Arena\\", sizeof( statusText ) );
	for ( i = 0; i < players; i++ ) {
		Q_strcat( statusText, sizeof( statusText ), "\\0 0 x" );
	}
	Check( strlen( statusText ) < sizeof( statusText ) - 1, "status text fits" );

	// sized to the struct, so a read past pings and numLines is an ASan error
	info = malloc( sizeof( *info ) );
	Check( info != NULL, "allocation" );
	Check( UI_GetServerStatusInfo( "192.0.2.1:27960", info ), "status parsed" );
	Check( info->numLines >= 4 && info->numLines <= MAX_SERVERSTATUS_LINES, "status line count" );
	Check( !strcmp( info->lines[2][0], "" ) && !strcmp( info->lines[3][0], "num" ), "player header" );
	playerRows = info->numLines - 4;
	for ( row = 0; row < playerRows; row++ ) {
		const char *number = info->lines[4 + row][0];
		Check( number >= info->pings && number < info->pings + sizeof( info->pings ), "player number in pings" );
		Check( atoi( number ) == row && !strcmp( info->lines[4 + row][3], "x" ), "player row" );
	}
	Check( playerRows == ( players < MAX_CLIENTS ? players : MAX_CLIENTS ), "player rows" );
	free( info );
}

int main( void ) {
	TestNetType();
	TestDisplayList();
	TestStatusPlayers( 8 );
	TestStatusPlayers( MAX_CLIENTS );
	TestStatusPlayers( 150 );	// past MAX_SERVERSTATUS_LINES rows
	TestStatusPlayers( MAX_CLIENTS + 1 );
	puts( "Team Arena server browser stays inside its tables (issue #40)" );
	return 0;
}
