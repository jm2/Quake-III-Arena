/* Issue #368: the base q3_ui bot lists (Remove Bots and Team Orders menus) stay
 * within CS_PLAYERS + MAX_CLIENTS whatever sv_maxclients a server's serverinfo says. */
#include "../code/q3_ui/ui_removebots.c"
#include "../code/q3_ui/ui_teamorders.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SENTINEL 0x5a5a5a5a
#define LOCAL_CLIENT 3
/* stop a list loop after this many reads outside CS_PLAYERS; unbounded, it would
 * read up to sv_maxclients strings, 2^31 for INT_MAX */
#define MAX_OUTSIDE_READS ( 2 * MAX_CONFIGSTRINGS )

static char configStrings[MAX_CONFIGSTRINGS][MAX_INFO_STRING];
static int outsidePlayerReads;
static qboolean full;

/** Fail with a description of the first mismatch. */
static void Check( int ok, const char *what ) {
	if ( !ok ) {
		fprintf( stderr, "q3_ui server maxclients regression failed: %s\n", what );
		exit( 1 );
	}
}

/** The server's client number for this client. */
void trap_GetClientState( uiClientState_t *state ) {
	memset( state, 0, sizeof( *state ) );
	state->connState = CA_ACTIVE;
	state->clientNum = LOCAL_CLIENT;
}

/** cl_ui.c GetConfigString: an index outside the table leaves buff untouched, an
 * unset string reads as empty. Reads other than serverinfo must stay in CS_PLAYERS. */
int trap_GetConfigString( int index, char *buff, int buffsize ) {
	if ( index != CS_SERVERINFO && ( index < CS_PLAYERS || index >= CS_PLAYERS + MAX_CLIENTS ) ) {
		Check( ++outsidePlayerReads <= MAX_OUTSIDE_READS, "configstring reads stop" );
	}
	if ( index < 0 || index >= MAX_CONFIGSTRINGS ) {
		return qfalse;
	}
	if ( !configStrings[index][0] ) {
		if ( buffsize ) {
			buff[0] = 0;
		}
		return qfalse;
	}
	Q_strncpyz( buff, configStrings[index], buffsize );
	return qtrue;
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

/** Slot n holds a client on a real server, every slot on a full one; a hostile
 * server fills every string after CS_PLAYERS + MAX_CLIENTS - 1 with bot info too. */
static int Filled( int n ) {
	return full || n >= MAX_CLIENTS || n % 4 != 1;
}

/** Two of three clients are bots, or all but the local client on a full server;
 * the local client is a human. */
static int IsBot( int n ) {
	if ( full || n >= MAX_CLIENTS ) {
		return n != LOCAL_CLIENT;
	}
	return Filled( n ) && n % 3 != 0;
}

/** A sparse or full server that also fills every string after its client slots,
 * and whose serverinfo says sv_maxclients is maxClients. */
static void Serve( const char *maxClients ) {
	int n;

	memset( configStrings, 0, sizeof( configStrings ) );
	Com_sprintf( configStrings[CS_SERVERINFO], MAX_INFO_STRING,
		"\\sv_maxclients\\%s\\g_gametype\\%d\\mapname\\q3dm17", maxClients, GT_CTF );
	for ( n = 0; CS_PLAYERS + n < MAX_CONFIGSTRINGS; n++ ) {
		if ( IsBot( n ) ) {
			Com_sprintf( configStrings[CS_PLAYERS + n], MAX_INFO_STRING,
				"\\n\\^%dB%03d\\t\\%d\\model\\sarge\\skill\\%d", n % 8, n, TEAM_BLUE, n % 5 + 1 );
		} else if ( Filled( n ) ) {
			Com_sprintf( configStrings[CS_PLAYERS + n], MAX_INFO_STRING,
				"\\n\\^%dH%03d\\t\\%d\\model\\sarge", n % 8, n, TEAM_BLUE );
		}
	}
	Check( Filled( LOCAL_CLIENT ) && !IsBot( LOCAL_CLIENT ), "local client is a human" );
	outsidePlayerReads = 0;
}

/** The Remove Bots list holds the client numbers of the bots in the first slots
 * slots, in slot order, and nothing after them changes. */
static void TestRemoveBots( const char *maxClients, int slots ) {
	int n, bots = 0;

	Serve( maxClients );
	memset( &removeBotsMenuInfo, 0x5a, sizeof( removeBotsMenuInfo ) );
	UI_RemoveBotsMenu_GetBots();

	for ( n = 0; n < slots; n++ ) {
		if ( IsBot( n ) ) {
			Check( bots < removeBotsMenuInfo.numBots && removeBotsMenuInfo.botClientNums[bots] == n,
				"bot client number in slot order" );
			bots++;
		}
	}
	Check( removeBotsMenuInfo.numBots == bots, "bot count" );
	for ( n = bots; n < MAX_BOTS; n++ ) {
		Check( removeBotsMenuInfo.botClientNums[n] == SENTINEL, "bot client numbers after the list" );
	}
	Check( outsidePlayerReads == 0, "Remove Bots reads stay in CS_PLAYERS + MAX_CLIENTS" );
}

/** The Team Orders list keeps "Everyone" first and at most 9 entries. This source
 * resets playerTeam on every slot (bk001204), so it never matches a bot's team and
 * lists no bot; that stays as it is. */
static void TestTeamOrders( const char *maxClients ) {
	int n;

	Serve( maxClients );
	memset( &teamOrdersMenuInfo, 0, sizeof( teamOrdersMenuInfo ) );
	UI_TeamOrdersMenu_BuildBotList();

	Check( teamOrdersMenuInfo.gametype == GT_CTF, "game type" );
	Check( teamOrdersMenuInfo.numBots == 1 && !strcmp( teamOrdersMenuInfo.botNames[0], "Everyone" ), "bot list" );
	for ( n = 0; n < 9; n++ ) {
		Check( teamOrdersMenuInfo.bots[n] == teamOrdersMenuInfo.botNames[n], "bot name pointers" );
	}
	Check( outsidePlayerReads == 0, "Team Orders reads stay in CS_PLAYERS + MAX_CLIENTS" );
}

/** Run one menu, server layout and sv_maxclients value per process, so a sanitizer
 * report names the case. */
int main( int argc, char **argv ) {
	int slots;

	if ( argc != 4 || ( strcmp( argv[1], "removebots" ) && strcmp( argv[1], "teamorders" ) )
		|| ( strcmp( argv[2], "sparse" ) && strcmp( argv[2], "full" ) ) ) {
		fprintf( stderr, "usage: %s removebots|teamorders sparse|full <sv_maxclients>\n", argv[0] );
		return 2;
	}
	full = !strcmp( argv[2], "full" );
	slots = atoi( argv[3] );
	if ( slots < 0 ) {
		slots = 0;
	} else if ( slots > MAX_CLIENTS ) {
		slots = MAX_CLIENTS;
	}
	if ( !strcmp( argv[1], "removebots" ) ) {
		TestRemoveBots( argv[3], slots );
	} else {
		TestTeamOrders( argv[3] );
	}
	printf( "q3_ui %s list stays within CS_PLAYERS + MAX_CLIENTS on a %s server with sv_maxclients %s (issue #368)\n",
		argv[1], argv[2], argv[3] );
	return 0;
}
