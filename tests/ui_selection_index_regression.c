/* Issue #419: the Team Arena UI indexes its tables with local selections that
 * can name no entry:
 * - the addBot script's botIndex is stepped through the bot table of the
 *   gametype the menu showed (and two entries past it), but the server's
 *   g_gametype picks the table the script indexes;
 * - an empty server list selects row -1, and a click below the last row selects
 *   row numDisplayServers, one past a full list of MAX_DISPLAY_SERVERS; the
 *   FEEDER_SERVERS selection and the ServerStatus, addFavorite and
 *   deleteFavorite scripts read displayServers at the selected row;
 * - an empty find-player list selects row -1 too, and the FEEDER_FINDPLAYER
 *   selection and FoundPlayerServerStatus script read foundPlayerServerAddresses
 *   at the selected row.
 * A row that names no server leaves an empty status address, and no status is
 * requested for it, which the engine would try to resolve.
 * The fixture includes the real ui_main.c and links the rest of the UI behind
 * the real syscall layer; the fake engine below answers as cl_ui.c does. */
#include "../code/ui/ui_main.c"
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void dllEntry( int (QDECL *syscallptr)( int arg, ... ) );	/* ui_syscalls.c */

#define LISTED_SERVERS MAX_GLOBAL_SERVERS	/* the servers the engine lists */
#define NO_SERVER -1	/* a server number the engine lists nothing under */
#define NOT_ASKED -2
#define STALE 0x5a5a5a5a	/* displayServers past the list: a stale row */
#define PLAYING 5	/* the previous selection's cinematic */
#define CINEMATIC 9	/* the handle of a cinematic the selection plays */
#define PREVIEW 7	/* the handle of a levelshot the engine has */
#define BOTS 3
#define NOTHING "(nothing)"	/* no status request, or no favorite added or removed */
#define UNTOUCHED "(untouched)"	/* the status address before a find-player selection */
#define FOUND_STALE "203.0.113.99:27960"	/* a find-player slot past the list */

static const char botsText[] =
	"{ name Grunt funname Grunt }\n{ name Major funname Major }\n{ name Visor funname Visor }\n";
static const char *botNames[BOTS] = { "Grunt", "Major", "Visor" };
static char headNames[MAX_HEADS][16];
static const char *testCase;
static float gametype;
static char executed[MAX_STRING_CHARS];	/* EXEC_APPEND text */
static char printed[MAX_STRING_CHARS];	/* UI_PRINT text */
static int askedServer;	/* the last server number the engine was asked about */
static char registeredShader[MAX_QPATH];
static char playedCinematic[MAX_QPATH];
static int stoppedCinematic;
static char queriedStatus[MAX_ADDRESSLENGTH];	/* the last status request's address */
static int statusResets;
static char addedFavorite[MAX_STRING_CHARS];
static char removedFavorite[MAX_ADDRESSLENGTH];

/** Fail with the case under test and the first mismatch. */
static void Check( int ok, const char *what ) {
	if ( !ok ) {
		fprintf( stderr, "UI selection index regression failed (%s): %s\n", testCase, what );
		exit( 1 );
	}
}

/** The fake engine's server n. */
static const char *ServerAddress( int n ) {
	return va( "192.0.%d.%d:27960", n / 256, n % 256 );
}

/** cl_ui.c LAN_GetServerInfo and LAN_GetServerAddressString: every source
 * lists LISTED_SERVERS servers, and any other number gets an empty string. */
static qboolean Listed( int source, int n ) {
	Check( source == ui_netSource.integer, "the engine is asked about the browsed source" );
	Check( n == NO_SERVER || ( n >= 0 && n < LISTED_SERVERS ), "the engine is asked about a listed server, or none" );
	askedServer = n;
	return n >= 0 && n < LISTED_SERVERS;
}

/** The UI's syscalls on the paths under test. */
static int QDECL FakeSyscall( int command, ... ) {
	va_list ap;
	int result = 0;

	va_start( ap, command );
	switch ( command ) {
	case UI_ERROR:
		Check( 0, va_arg( ap, const char * ) );
		break;
	case UI_PRINT:
		Q_strcat( printed, sizeof( printed ), va_arg( ap, const char * ) );
		break;
	case UI_CVAR_REGISTER: {	/* g_botsFile */
		vmCvar_t *vmCvar = va_arg( ap, vmCvar_t * );
		const char *name = va_arg( ap, const char * );
		Check( !strcmp( name, "g_botsFile" ), "only g_botsFile is registered" );
		memset( vmCvar, 0, sizeof( *vmCvar ) );
		Q_strncpyz( vmCvar->string, va_arg( ap, const char * ), sizeof( vmCvar->string ) );
		break;
	}
	case UI_CVAR_SET:	/* JoinServer's camera cvars */
		break;
	case UI_CVAR_VARIABLEVALUE: {
		const char *name = va_arg( ap, const char * );
		float value = 0;
		if ( !strcmp( name, "g_gametype" ) ) {
			value = gametype;
		}
		memcpy( &result, &value, sizeof( result ) );	// FloatAsInt
		break;
	}
	case UI_CMD_EXECUTETEXT:
		Check( va_arg( ap, int ) == EXEC_APPEND, "commands are appended" );
		Q_strcat( executed, sizeof( executed ), va_arg( ap, const char * ) );
		break;
	case UI_FS_FOPENFILE: {	/* scripts/bots.txt only */
		const char *name = va_arg( ap, const char * );
		fileHandle_t *f = va_arg( ap, fileHandle_t * );
		*f = !strcmp( name, "scripts/bots.txt" );
		result = *f ? (int)strlen( botsText ) : -1;
		break;
	}
	case UI_FS_READ: {
		char *buffer = va_arg( ap, char * );
		int len = va_arg( ap, int );
		Check( va_arg( ap, fileHandle_t ) == 1 && len == (int)strlen( botsText ), "bots.txt is read whole" );
		memcpy( buffer, botsText, len );
		break;
	}
	case UI_FS_FCLOSEFILE:
	case UI_FS_GETFILELIST:	/* no .bot files */
		break;
	case UI_R_REGISTERSHADERNOMIP: {
		const char *name = va_arg( ap, const char * );
		Q_strncpyz( registeredShader, name, sizeof( registeredShader ) );
		// the renderer answers 0 for an image it does not have
		result = strcmp( name, "levelshots/" ) ? PREVIEW : 0;
		break;
	}
	case UI_CIN_PLAYCINEMATIC:
		Q_strncpyz( playedCinematic, va_arg( ap, const char * ), sizeof( playedCinematic ) );
		result = CINEMATIC;
		break;
	case UI_CIN_STOPCINEMATIC:
		stoppedCinematic = va_arg( ap, int );
		break;
	case UI_LAN_GETSERVERINFO:
	case UI_LAN_GETSERVERADDRESSSTRING: {
		int source = va_arg( ap, int );
		int n = va_arg( ap, int );
		char *buf = va_arg( ap, char * );
		int buflen = va_arg( ap, int );
		if ( !Listed( source, n ) ) {
			buf[0] = '\0';
		} else if ( command == UI_LAN_GETSERVERADDRESSSTRING ) {
			Q_strncpyz( buf, ServerAddress( n ), buflen );
		} else {
			Q_strncpyz( buf, va( "\\hostname\\Server %d\\mapname\\map%d\\addr\\%s", n, n, ServerAddress( n ) ), buflen );
		}
		break;
	}
	case UI_LAN_SERVERSTATUS: {	/* never answered */
		const char *address = va_arg( ap, const char * );
		if ( address ) {
			Q_strncpyz( queriedStatus, address, sizeof( queriedStatus ) );
		} else {
			statusResets++;
		}
		break;
	}
	case UI_LAN_ADDSERVER: {
		const char *name;
		Check( va_arg( ap, int ) == AS_FAVORITES, "favorites are added" );
		name = va_arg( ap, const char * );
		Com_sprintf( addedFavorite, sizeof( addedFavorite ), "%s %s", name, va_arg( ap, const char * ) );
		result = 1;
		break;
	}
	case UI_LAN_REMOVESERVER:
		Check( va_arg( ap, int ) == AS_FAVORITES, "favorites are removed" );
		Q_strncpyz( removedFavorite, va_arg( ap, const char * ), sizeof( removedFavorite ) );
		break;
	default:
		Check( 0, va( "unexpected UI syscall %d", command ) );
	}
	va_end( ap );
	return result;
}

/** Run one menu script. */
static void RunScript( const char *script ) {
	char buffer[MAX_STRING_CHARS];
	char *args = buffer;

	Q_strncpyz( buffer, script, sizeof( buffer ) );
	UI_RunMenuScript( &args );
}

/** Nothing recorded yet. */
static void Reset( void ) {
	executed[0] = printed[0] = registeredShader[0] = playedCinematic[0] = '\0';
	Q_strncpyz( queriedStatus, NOTHING, sizeof( queriedStatus ) );
	Q_strncpyz( addedFavorite, NOTHING, sizeof( addedFavorite ) );
	Q_strncpyz( removedFavorite, NOTHING, sizeof( removedFavorite ) );
	askedServer = NOT_ASKED;
	stoppedCinematic = -1;
	statusResets = 0;
}

/** A server list of rows rows browsing source; each row shows a listed server,
 * and the entries past the list are stale. The previous selection's cinematic
 * plays. */
static void List( int rows, int source ) {
	int i;

	Check( rows >= 0 && rows <= MAX_DISPLAY_SERVERS, "a server list" );
	memset( &uiInfo, 0, sizeof( uiInfo ) );
	ui_netSource.integer = source;
	for ( i = 0; i < MAX_DISPLAY_SERVERS; i++ ) {
		uiInfo.serverStatus.displayServers[i] = i < rows ? LISTED_SERVERS - 1 - i : STALE;
	}
	uiInfo.serverStatus.numDisplayServers = rows;
	uiInfo.serverStatus.currentServerCinematic = PLAYING;
	Reset();
}

/** A find-player list of rows rows: the servers found, then the row that
 * counts them, whose address was never written, and stale slots past the list.
 * The browser's selection is one past its list of 3, which retail still lets
 * the find-player status through for. */
static void FindPlayerList( int rows ) {
	int i;

	Check( rows >= 0 && rows <= MAX_FOUNDPLAYER_SERVERS, "a find-player list" );
	List( 3, AS_GLOBAL );
	uiInfo.serverStatus.currentServer = 3;
	for ( i = 0; i < MAX_FOUNDPLAYER_SERVERS; i++ ) {
		Q_strncpyz( uiInfo.foundPlayerServerAddresses[i], i < rows - 1 ? va( "198.51.100.%d:27960", i ) :
			i == rows - 1 ? "" : FOUND_STALE, sizeof( uiInfo.foundPlayerServerAddresses[i] ) );
	}
	uiInfo.numFoundPlayerServers = rows;
	Q_strncpyz( uiInfo.serverStatusAddress, UNTOUCHED, sizeof( uiInfo.serverStatusAddress ) );
}

/** Selecting a found server shows its status, even while the browser's
 * selection is one past its list; the row that counts them, and a row past
 * the list, show nothing. FoundPlayerServerStatus shows the status of any row
 * in the list, and none for a row past it or for an empty address.
 * FoundPlayerJoinServer joins any row in the list, as it did. */
static void TestFindPlayer( const char *what, int rows, int row ) {
	char address[MAX_ADDRESSLENGTH];
	int listed = row >= 0 && row < rows;

	FindPlayerList( rows );
	Q_strncpyz( address, listed ? uiInfo.foundPlayerServerAddresses[row] : "", sizeof( address ) );
	if ( !strcmp( what, "select" ) ) {
		UI_FeederSelection( FEEDER_FINDPLAYER, row );
		Check( uiInfo.currentFoundPlayerServer == row, "the selected row" );
		if ( row >= 0 && row < rows - 1 ) {
			Check( !strcmp( uiInfo.serverStatusAddress, address ) && statusResets == 1, "the status list is rebuilt" );
			Check( !strcmp( queriedStatus, address ), "the found server's status is requested" );
		} else {
			Check( !strcmp( uiInfo.serverStatusAddress, UNTOUCHED ) && statusResets == 0, "the status list is kept" );
			Check( !strcmp( queriedStatus, NOTHING ), "no status is requested" );
		}
		return;
	}
	uiInfo.currentFoundPlayerServer = row;
	RunScript( what );
	if ( !strcmp( what, "FoundPlayerJoinServer" ) ) {
		Check( !strcmp( executed, listed ? va( "connect %s\n", address ) : "" ), "the found server is joined" );
		return;
	}
	Check( !strcmp( what, "FoundPlayerServerStatus" ), "a script under test" );
	Check( !strcmp( uiInfo.serverStatusAddress, address ) && statusResets == 1, "the status list is rebuilt" );
	Check( !strcmp( queriedStatus, address[0] ? address : NOTHING ), "the status of a found server only is requested" );
}

/** The server a row shows, or NO_SERVER. */
static int RowServer( int rows, int row ) {
	return row >= 0 && row < rows ? LISTED_SERVERS - 1 - row : NO_SERVER;
}

/** Selecting a row shows its server's levelshot and cinematic; a row that
 * shows no server shows no map, as retail's empty list did. */
static void TestSelect( int rows, int row ) {
	int server = RowServer( rows, row );

	List( rows, AS_GLOBAL );
	UI_FeederSelection( FEEDER_SERVERS, row );
	Check( uiInfo.serverStatus.currentServer == row, "the selected row" );
	Check( askedServer == server, "the engine is asked about the row's server" );
	Check( stoppedCinematic == PLAYING, "the previous cinematic stops" );
	if ( server == NO_SERVER ) {
		Check( !strcmp( registeredShader, "levelshots/" ) && uiInfo.serverStatus.currentServerPreview == 0, "no preview" );
		Check( !playedCinematic[0] && uiInfo.serverStatus.currentServerCinematic == -1, "no cinematic" );
	} else {
		Check( !strcmp( registeredShader, va( "levelshots/map%d", server ) ) &&
			uiInfo.serverStatus.currentServerPreview == PREVIEW, "the server's preview" );
		Check( !strcmp( playedCinematic, va( "map%d.roq", server ) ) &&
			uiInfo.serverStatus.currentServerCinematic == CINEMATIC, "the server's cinematic" );
	}
}

/** The scripts that act on the selected row's server do nothing for a row
 * that shows none. */
static void TestScript( const char *script, int rows, int row ) {
	int server = RowServer( rows, row );
	char address[MAX_ADDRESSLENGTH];

	Q_strncpyz( address, server == NO_SERVER ? "" : ServerAddress( server ), sizeof( address ) );
	List( rows, !strcmp( script, "deleteFavorite" ) ? AS_FAVORITES : AS_GLOBAL );
	uiInfo.serverStatus.currentServer = row;
	RunScript( script );
	if ( !strcmp( script, "JoinServer" ) ) {
		// JoinServer bounds the row itself
		Check( askedServer == ( server == NO_SERVER ? NOT_ASKED : server ), "the engine is asked about the row's server" );
		Check( !strcmp( executed, server == NO_SERVER ? "" : va( "connect %s\n", address ) ), "the server is joined" );
		return;
	}
	Check( askedServer == server, "the engine is asked about the row's server" );
	if ( !strcmp( script, "ServerStatus" ) ) {
		// the status list is cleared, then the server's status requested
		Check( !strcmp( uiInfo.serverStatusAddress, address ), "the status address" );
		Check( statusResets == 1 && uiInfo.serverStatusInfo.numLines == 0, "the status list is cleared" );
		Check( !strcmp( queriedStatus, server == NO_SERVER ? NOTHING : address ), "the server's status is requested" );
	} else if ( !strcmp( script, "addFavorite" ) ) {
		Check( !strcmp( addedFavorite, server == NO_SERVER ? NOTHING : va( "Server %d %s", server, address ) ),
			"the server is added to the favorites" );
	} else {
		Check( !strcmp( script, "deleteFavorite" ), "a script under test" );
		Check( !strcmp( removedFavorite, server == NO_SERVER ? NOTHING : address ), "the server is removed from the favorites" );
	}
}

/** addBot adds the bot the selection names in the table of the server's
 * gametype: the team heads, or the bots of bots.txt. The bots' names are
 * looked up by UI_GetBotNameByNumber, which sends Sarge for a number it
 * does not have, as retail did. */
static void TestAddBot( int characters, int botIndex ) {
	int i;

	Check( characters >= 0 && characters <= MAX_HEADS, "a head count" );
	memset( &uiInfo, 0, sizeof( uiInfo ) );
	for ( i = 0; i < characters; i++ ) {
		Com_sprintf( headNames[i], sizeof( headNames[i] ), "Head%d", i );
		uiInfo.characterList[i].name = headNames[i];
	}
	uiInfo.characterCount = characters;
	UI_LoadBots();
	Check( UI_GetNumBots() == BOTS, "bots.txt is loaded" );
	uiInfo.botIndex = botIndex;
	uiInfo.skillIndex = 2;
	uiInfo.redBlue = 1;
	Reset();
	RunScript( "addBot" );
	if ( gametype >= GT_TEAM ) {
		Check( !strcmp( executed, botIndex >= 0 && botIndex < characters ? va( "addbot Head%d 3 Blue\n", botIndex ) : "" ),
			"the team head is added" );
	} else if ( botIndex >= 0 && botIndex < BOTS ) {
		Check( !strcmp( executed, va( "addbot %s 3 Blue\n", botNames[botIndex] ) ), "the bot is added" );
	} else {
		Check( !strcmp( executed, "addbot Sarge 3 Blue\n" ) && strstr( printed, "Invalid bot number" ),
			"Sarge is added for a bot number past bots.txt" );
	}
}

/** An int argument, INT_MIN to INT_MAX. */
static int Number( const char *arg ) {
	char *end;
	long number = strtol( arg, &end, 10 );

	Check( *arg && !*end && number >= INT_MIN && number <= INT_MAX, "a decimal int" );
	return (int)number;
}

/** One case per process: UBSan and ASan stop at the first read past a table. */
int main( int argc, char **argv ) {
	static char caseName[MAX_STRING_CHARS];
	int i;

	for ( i = 1; i < argc; i++ ) {
		Q_strcat( caseName, sizeof( caseName ), va( i > 1 ? " %s" : "%s", argv[i] ) );
	}
	testCase = caseName;
	dllEntry( FakeSyscall );
	if ( argc == 5 && !strcmp( argv[1], "addbot" ) ) {
		gametype = Number( argv[2] );
		TestAddBot( Number( argv[3] ), Number( argv[4] ) );
	} else if ( argc == 4 && !strcmp( argv[1], "select" ) ) {
		TestSelect( Number( argv[2] ), Number( argv[3] ) );
	} else if ( argc == 5 && !strcmp( argv[1], "script" ) ) {
		TestScript( argv[2], Number( argv[3] ), Number( argv[4] ) );
	} else if ( argc == 5 && !strcmp( argv[1], "findplayer" ) ) {
		TestFindPlayer( argv[2], Number( argv[3] ), Number( argv[4] ) );
	} else {
		fprintf( stderr, "usage: %s addbot <gametype> <heads> <botIndex> | select <rows> <row> | script <name> <rows> <row>"
			" | findplayer select|<script> <rows> <row>\n", argv[0] );
		return 2;
	}
	printf( "Team Arena UI %s stays inside its tables (issue #419)\n", testCase );
	return 0;
}
