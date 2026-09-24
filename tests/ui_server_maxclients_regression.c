/* Issue #368: the Team Arena player lists built by UI_BuildPlayerList stay within
 * MAX_CLIENTS and CS_PLAYERS whatever sv_maxclients a server's serverinfo says. */
#include "../code/ui/ui_main.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SENTINEL 0x5a5a5a5a
#define LOCAL_CLIENT 3
#define UNTOUCHED "untouched"
/* stop a list loop after this many reads outside CS_PLAYERS; unbounded, it would
 * read up to sv_maxclients strings, 2^31 for INT_MAX */
#define MAX_OUTSIDE_READS ( 2 * MAX_CONFIGSTRINGS )

static char configStrings[MAX_CONFIGSTRINGS][MAX_INFO_STRING];
static char selectedPlayer[MAX_CVAR_VALUE_STRING];
static char selectedPlayerName[MAX_CVAR_VALUE_STRING];
static int outsidePlayerReads;
static qboolean full;

/** Fail with a description of the first mismatch. */
static void Check( int ok, const char *what ) {
	if ( !ok ) {
		fprintf( stderr, "UI server maxclients regression failed: %s\n", what );
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

/** Keep the two cvars UI_BuildPlayerList publishes. */
void trap_Cvar_Set( const char *var_name, const char *value ) {
	if ( !Q_stricmp( var_name, "cg_selectedPlayer" ) ) {
		Q_strncpyz( selectedPlayer, value, sizeof( selectedPlayer ) );
	} else if ( !Q_stricmp( var_name, "cg_selectedPlayerName" ) ) {
		Q_strncpyz( selectedPlayerName, value, sizeof( selectedPlayerName ) );
	} else {
		Check( 0, "only the selected player cvars are set" );
	}
}

/** Read back cg_selectedPlayer. */
float trap_Cvar_VariableValue( const char *var_name ) {
	Check( !Q_stricmp( var_name, "cg_selectedPlayer" ), "only cg_selectedPlayer is read" );
	return atof( selectedPlayer );
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

/** Slot n holds a player on a real server, every slot on a full one; a hostile
 * server fills every string after CS_PLAYERS + MAX_CLIENTS - 1 with player info too. */
static int Filled( int n ) {
	return full || n >= MAX_CLIENTS || n % 4 != 1;
}

/** Players alternate between the red and blue team, or are all blue on a full
 * server; LOCAL_CLIENT is blue. */
static int Team( int n ) {
	return ( full || n % 2 ) ? TEAM_BLUE : TEAM_RED;
}

/** A sparse or full server that also fills every string after its player slots,
 * and whose serverinfo says sv_maxclients is maxClients. */
static void Serve( const char *maxClients ) {
	int n;

	memset( configStrings, 0, sizeof( configStrings ) );
	Com_sprintf( configStrings[CS_SERVERINFO], MAX_INFO_STRING,
		"\\sv_maxclients\\%s\\g_gametype\\%d\\mapname\\mp_test", maxClients, GT_CTF );
	for ( n = 0; CS_PLAYERS + n < MAX_CONFIGSTRINGS; n++ ) {
		if ( Filled( n ) ) {
			Com_sprintf( configStrings[CS_PLAYERS + n], MAX_INFO_STRING,
				"\\n\\^%dP%03d\\t\\%d\\model\\sarge\\hmodel\\sarge\\tl\\0\\skill\\%d",
				n % 8, n, Team( n ), n % 3 );
		}
	}
	Check( Filled( LOCAL_CLIENT ) && Team( LOCAL_CLIENT ) == TEAM_BLUE, "local client is a blue player" );
}

/** Mark every list row and the field after the lists, as a stale UI would have them. */
static void Reset( void ) {
	memset( &uiInfo, 0, sizeof( uiInfo ) );
	memset( uiInfo.playerNames, 'x', sizeof( uiInfo.playerNames ) );
	memset( uiInfo.teamNames, 'y', sizeof( uiInfo.teamNames ) );
	memset( uiInfo.teamClientNums, 0x5a, sizeof( uiInfo.teamClientNums ) );
	uiInfo.mapCount = SENTINEL;
	strcpy( selectedPlayer, UNTOUCHED );
	strcpy( selectedPlayerName, UNTOUCHED );
	outsidePlayerReads = 0;
}

/** Whether a list row still holds the fill byte Reset gave it. */
static int RowUntouched( const char *row, char fill ) {
	int i;
	for ( i = 0; i < MAX_NAME_LENGTH; i++ ) {
		if ( row[i] != fill ) {
			return 0;
		}
	}
	return 1;
}

/** Build the lists for a server that says sv_maxclients is maxClients; they must
 * hold the players of the first slots slots, in slot order. */
static void TestServer( const char *maxClients, int slots ) {
	int n, players = 0, teammates = 0, localIndex = 0, mates[MAX_CLIENTS];

	Serve( maxClients );
	Reset();
	UI_BuildPlayerList();

	for ( n = 0; n < slots; n++ ) {
		if ( !Filled( n ) ) {
			continue;
		}
		Check( !strcmp( uiInfo.playerNames[players], va( "P%03d", n ) ), "player name in slot order" );
		players++;
		if ( Team( n ) == Team( LOCAL_CLIENT ) ) {
			Check( !strcmp( uiInfo.teamNames[teammates], va( "P%03d", n ) ), "teammate name in slot order" );
			Check( uiInfo.teamClientNums[teammates] == n, "teammate client number" );
			mates[teammates] = n;
			if ( n == LOCAL_CLIENT ) {
				localIndex = teammates;
			}
			teammates++;
		}
	}
	Check( uiInfo.playerNumber == LOCAL_CLIENT && !uiInfo.teamLeader, "local client state" );
	Check( uiInfo.playerCount == players, "player count" );
	Check( uiInfo.myTeamCount == teammates, "teammate count" );
	for ( n = players; n < MAX_CLIENTS; n++ ) {
		Check( RowUntouched( uiInfo.playerNames[n], 'x' ), "player rows after the list" );
	}
	for ( n = teammates; n < MAX_CLIENTS; n++ ) {
		Check( RowUntouched( uiInfo.teamNames[n], 'y' ), "teammate rows after the list" );
		Check( uiInfo.teamClientNums[n] == SENTINEL, "teammate client numbers after the list" );
	}
	Check( uiInfo.mapCount == SENTINEL, "memory after teamClientNums" );
	Check( outsidePlayerReads == 0, "configstring reads stay in CS_PLAYERS + MAX_CLIENTS" );

	/* not a team leader: the local client, or the first teammate when it is past
	 * sv_maxclients, becomes the selected player */
	Check( atoi( selectedPlayer ) == localIndex, "cg_selectedPlayer" );
	if ( teammates ) {
		Check( !strcmp( selectedPlayerName, va( "P%03d", mates[localIndex] ) ), "cg_selectedPlayerName" );
	} else {
		Check( !strcmp( selectedPlayerName, UNTOUCHED ), "cg_selectedPlayerName without teammates" );
	}
}

/** Run one server layout and sv_maxclients value per process, so a sanitizer report
 * names the case. */
int main( int argc, char **argv ) {
	int slots;

	if ( argc != 3 || ( strcmp( argv[1], "sparse" ) && strcmp( argv[1], "full" ) ) ) {
		fprintf( stderr, "usage: %s sparse|full <sv_maxclients>\n", argv[0] );
		return 2;
	}
	full = !strcmp( argv[1], "full" );
	slots = atoi( argv[2] );
	if ( slots < 0 ) {
		slots = 0;
	} else if ( slots > MAX_CLIENTS ) {
		slots = MAX_CLIENTS;
	}
	TestServer( argv[2], slots );
	printf( "Team Arena player lists stay within MAX_CLIENTS on a %s server with sv_maxclients %s (issue #368)\n",
		argv[1], argv[2] );
	return 0;
}
