/*
 * Issue #463: CalculateRanks (code/game/g_main.c) cleared
 * level.numteamVotingClients[i] for every i below TEAM_NUM_TEAMS (4), as
 * retail 1.32c does, but g_local.h declares the array int[2]. Each call wrote
 * two ints past it, onto the next fields of level_locals_t, level.spawning
 * and level.numSpawnVars, and zeroed them. ioquake3 bounds the loop to the
 * array.
 *
 * Retail never showed a difference. G_SpawnEntitiesFromString, inside
 * G_InitGame, is the only code that sets those two fields, and it ends with
 * spawning cleared and numSpawnVars 0 (its last G_ParseSpawnVars zeroes the
 * count before it finds the end of the entity string). CalculateRanks runs
 * from ClientConnect, ClientBegin, ClientDisconnect, AddScore,
 * UpdateTournamentInfo and the CTF and Team Arena flag and obelisk
 * callbacks, and none of them runs during G_SpawnEntitiesFromString, so the
 * old code always wrote zero over zero. It still wrote past the array, and
 * UBSan's bounds check stops on it.
 *
 * This test links the real g_main.c, built with UBSan's bounds check and no
 * recovery, and runs CalculateRanks with level.spawning and
 * level.numSpawnVars holding sentinels and numteamVotingClients holding
 * stale counts. In team games (Team Deathmatch and CTF) it has human
 * players on both teams, a bot on each, a client still connecting, a
 * spectator and a free slot. Each call must leave the sentinels untouched
 * and count the connected human players of each team, and recount after a
 * player leaves and another finishes connecting. A team vote in progress on
 * each team then goes through the real CheckTeamVote, which must pass the
 * red vote and fail the blue one on those counts. A free-for-all game must
 * count no team voters. On master the first call stops on UBSan's "index 2
 * out of bounds for type 'int [2]'"; built without the sanitizer, it fails
 * the sentinel check.
 */
#include "../code/game/g_local.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_MAXCLIENTS		10
#define TEST_SPAWN_VARS		7		/* sentinel for level.numSpawnVars */
#define TEST_STALE_COUNT	9		/* stale numteamVotingClients before each call */

/* g_main.c does not export these through g_local.h. */
extern gclient_t	g_clients[MAX_CLIENTS];
void CheckTeamVote( int team );

static const char	*testCase = "setup";

static void Fail( const char *message ) {
	fprintf( stderr, "team voting bounds regression failed: %s: %s\n", testCase, message );
	exit( 1 );
}

/* --- engine traps CalculateRanks and CheckTeamVote reach --- */
#define MAX_RECORDED	8

static int	configCount;
static int	configNum[MAX_RECORDED];
static char	configText[MAX_RECORDED][MAX_STRING_CHARS];

void trap_SetConfigstring( int num, const char *string ) {
	if ( configCount == MAX_RECORDED ) {
		Fail( "too many configstrings" );
	}
	configNum[configCount] = num;
	Q_strncpyz( configText[configCount], string, sizeof( configText[0] ) );
	configCount++;
}

static int	serverCommandCount;
static char	serverCommand[MAX_RECORDED][MAX_STRING_CHARS];

void trap_SendServerCommand( int clientNum, const char *text ) {
	if ( clientNum != -1 ) {
		Fail( "unexpected server command to one client" );
	}
	if ( serverCommandCount == MAX_RECORDED ) {
		Fail( "too many server commands" );
	}
	Q_strncpyz( serverCommand[serverCommandCount], text, sizeof( serverCommand[0] ) );
	serverCommandCount++;
}

static int	consoleCommandCount;
static char	consoleCommand[MAX_STRING_CHARS];

void trap_SendConsoleCommand( int exec_when, const char *text ) {
	if ( exec_when != EXEC_APPEND ) {
		Fail( "unexpected console command execution time" );
	}
	consoleCommandCount++;
	Q_strncpyz( consoleCommand, text, sizeof( consoleCommand ) );
}

void trap_Printf( const char *text ) {
	fprintf( stderr, "%s", text );
	Fail( "unexpected print" );
}
void trap_Error( const char *text ) {
	fprintf( stderr, "%s\n", text );
	Fail( "unexpected error" );
}

/* --- what g_main.c references on paths this test must not reach --- */
static void Unexpected( const char *name ) {
	fprintf( stderr, "team voting bounds regression failed: %s: unexpected call to %s\n", testCase, name );
	exit( 1 );
}

void BotInterbreedEndMatch( void ) { Unexpected( "BotInterbreedEndMatch" ); }
void ClientUserinfoChanged( int clientNum ) { (void)clientNum; Unexpected( "ClientUserinfoChanged" ); }
void DeathmatchScoreboardMessage( gentity_t *ent ) { (void)ent; Unexpected( "DeathmatchScoreboardMessage" ); }
gentity_t *G_Find( gentity_t *from, int fieldofs, const char *match ) {
	(void)from; (void)fieldofs; (void)match; Unexpected( "G_Find" ); return NULL;
}
gentity_t *G_PickTarget( char *targetname ) { (void)targetname; Unexpected( "G_PickTarget" ); return NULL; }
void G_WriteSessionData( void ) { Unexpected( "G_WriteSessionData" ); }
void respawn( gentity_t *ent ) { (void)ent; Unexpected( "respawn" ); }
gentity_t *SelectSpawnPoint( vec3_t avoidPoint, vec3_t origin, vec3_t angles ) {
	(void)avoidPoint; (void)origin; (void)angles; Unexpected( "SelectSpawnPoint" ); return NULL;
}
void SetTeam( gentity_t *ent, char *s ) { (void)ent; (void)s; Unexpected( "SetTeam" ); }
void SpawnModelsOnVictoryPads( void ) { Unexpected( "SpawnModelsOnVictoryPads" ); }	/* base only */
void StopFollowing( gentity_t *ent ) { (void)ent; Unexpected( "StopFollowing" ); }
void UpdateTournamentInfo( void ) { Unexpected( "UpdateTournamentInfo" ); }
void trap_Cvar_Set( const char *var_name, const char *value ) { (void)var_name; (void)value; Unexpected( "trap_Cvar_Set" ); }	/* Team Arena only */
void trap_FS_Write( const void *buffer, int len, fileHandle_t f ) { (void)buffer; (void)len; (void)f; Unexpected( "trap_FS_Write" ); }

/* --- helpers --- */

typedef struct {
	team_t			team;
	clientConnected_t	connected;
	qboolean		bot;
} testClient_t;

/*
 * Red: three players, a bot and a free slot. Blue: two players, a bot and a
 * client still connecting. One spectator. In a free-for-all game everyone
 * but the spectator plays TEAM_FREE.
 */
static const testClient_t	testClients[TEST_MAXCLIENTS] = {
	{ TEAM_RED,			CON_CONNECTED,		qfalse },
	{ TEAM_BLUE,		CON_CONNECTED,		qfalse },
	{ TEAM_RED,			CON_CONNECTED,		qfalse },
	{ TEAM_BLUE,		CON_CONNECTED,		qfalse },
	{ TEAM_RED,			CON_CONNECTED,		qfalse },
	{ TEAM_RED,			CON_CONNECTED,		qtrue },
	{ TEAM_BLUE,		CON_CONNECTED,		qtrue },
	{ TEAM_BLUE,		CON_CONNECTING,		qfalse },
	{ TEAM_SPECTATOR,	CON_CONNECTED,		qfalse },
	{ TEAM_RED,			CON_DISCONNECTED,	qfalse },
};

static void SetupServer( int gametype ) {
	int i;

	memset( &level, 0, sizeof( level ) );
	memset( g_entities, 0, sizeof( g_entities ) );
	memset( g_clients, 0, sizeof( g_clients ) );

	g_gametype.integer = gametype;
	g_maxclients.integer = TEST_MAXCLIENTS;
	level.maxclients = TEST_MAXCLIENTS;
	level.clients = g_clients;
	level.gentities = g_entities;
	level.num_entities = MAX_CLIENTS;
	level.time = 60000;
	level.startTime = 0;

	for ( i = 0 ; i < TEST_MAXCLIENTS ; i++ ) {
		g_entities[i].s.number = i;
		g_entities[i].client = &g_clients[i];
		g_clients[i].ps.clientNum = i;
		g_clients[i].ps.persistant[PERS_SCORE] = i;
		g_clients[i].pers.connected = testClients[i].connected;
		g_clients[i].sess.sessionTeam = testClients[i].team;
		if ( gametype < GT_TEAM && testClients[i].team != TEAM_SPECTATOR ) {
			g_clients[i].sess.sessionTeam = TEAM_FREE;
		}
		if ( testClients[i].bot ) {
			g_entities[i].r.svFlags |= SVF_BOT;
		}
		g_entities[i].inuse = testClients[i].connected != CON_DISCONNECTED;
	}
}

/* Runs the real CalculateRanks over sentinels and stale counts and checks them. */
static void RunCalculateRanks( int voting, int redVoting, int blueVoting ) {
	level.spawning = qtrue;
	level.numSpawnVars = TEST_SPAWN_VARS;
	level.numVotingClients = TEST_STALE_COUNT;
	level.numteamVotingClients[0] = TEST_STALE_COUNT;
	level.numteamVotingClients[1] = TEST_STALE_COUNT;
	configCount = 0;
	serverCommandCount = 0;

	CalculateRanks();

	if ( level.spawning != qtrue ) {
		Fail( "CalculateRanks changed level.spawning" );
	}
	if ( level.numSpawnVars != TEST_SPAWN_VARS ) {
		Fail( "CalculateRanks changed level.numSpawnVars" );
	}
	if ( level.numVotingClients != voting ) {
		Fail( "wrong level.numVotingClients" );
	}
	if ( level.numteamVotingClients[0] != redVoting ) {
		Fail( "wrong red level.numteamVotingClients" );
	}
	if ( level.numteamVotingClients[1] != blueVoting ) {
		Fail( "wrong blue level.numteamVotingClients" );
	}
	/* CS_SCORES1 and CS_SCORES2, and nothing else: no exit rule fired */
	if ( configCount != 2 || configNum[0] != CS_SCORES1 || configNum[1] != CS_SCORES2 ) {
		Fail( "CalculateRanks did not set just CS_SCORES1 and CS_SCORES2" );
	}
	if ( serverCommandCount != 0 || level.intermissionQueued || level.intermissiontime ) {
		Fail( "CalculateRanks ended the level" );
	}
}

/* A team vote in progress, as Cmd_CallTeamVote_f and Cmd_TeamVote_f leave it. */
static void StartTeamVote( int cs_offset, const char *command, int yes, int no ) {
	level.teamVoteTime[cs_offset] = level.time - 1000;
	Q_strncpyz( level.teamVoteString[cs_offset], command, sizeof( level.teamVoteString[0] ) );
	level.teamVoteYes[cs_offset] = yes;
	level.teamVoteNo[cs_offset] = no;
}

/* Runs the real CheckTeamVote for one team and checks that the vote ended as wanted. */
static void CheckVoteResult( int team, int cs_offset, qboolean wantPassed, const char *wantCommand ) {
	configCount = 0;
	serverCommandCount = 0;
	consoleCommandCount = 0;

	CheckTeamVote( team );

	if ( level.teamVoteTime[cs_offset] != 0 ) {
		Fail( "CheckTeamVote did not end the team vote" );
	}
	if ( configCount != 1 || configNum[0] != CS_TEAMVOTE_TIME + cs_offset || configText[0][0] ) {
		Fail( "CheckTeamVote did not clear the team's CS_TEAMVOTE_TIME" );
	}
	if ( serverCommandCount != 1 || strcmp( serverCommand[0],
		wantPassed ? "print \"Team vote passed.\n\"" : "print \"Team vote failed.\n\"" ) ) {
		Fail( wantPassed ? "the team vote did not pass" : "the team vote did not fail" );
	}
	if ( wantPassed ) {
		if ( consoleCommandCount != 1 || strcmp( consoleCommand, wantCommand ) ) {
			Fail( "the passed team vote did not run its command" );
		}
	} else if ( consoleCommandCount != 0 ) {
		Fail( "the failed team vote ran its command" );
	}
}

/* --- tests --- */

static void Test_TeamGame( int gametype, const char *name ) {
	testCase = name;
	SetupServer( gametype );

	/* red: clients 0, 2 and 4; blue: clients 1 and 3 */
	RunCalculateRanks( 5, 3, 2 );
	if ( level.numConnectedClients != 9 || level.numNonSpectatorClients != 8 || level.numPlayingClients != 7 ) {
		Fail( "wrong connected, non-spectator or playing count" );
	}

	/*
	 * Red: 2 of 3 voted yes, more than half, so it passes. Blue: 1 yes and
	 * 1 no of 2, not a majority for and at least half against, so it fails.
	 * With the counts zeroed both would pass; with stale counts both would
	 * still be waiting.
	 */
	StartTeamVote( 0, "g_testRedTeamVote 1", 2, 0 );
	StartTeamVote( 1, "g_testBlueTeamVote 1", 1, 1 );
	CheckVoteResult( TEAM_RED, 0, qtrue, "g_testRedTeamVote 1\n" );
	CheckVoteResult( TEAM_BLUE, 1, qfalse, NULL );

	/* a red player leaves and the connecting blue client enters the game */
	g_clients[4].pers.connected = CON_DISCONNECTED;
	g_entities[4].inuse = qfalse;
	g_clients[7].pers.connected = CON_CONNECTED;
	RunCalculateRanks( 5, 2, 3 );
	if ( level.numConnectedClients != 8 || level.numNonSpectatorClients != 7 || level.numPlayingClients != 7 ) {
		Fail( "wrong connected, non-spectator or playing count after the changes" );
	}

	/* red: 1 of 2 yes is not more than half; 1 of 2 no is at least half */
	StartTeamVote( 0, "g_testRedTeamVote 2", 1, 1 );
	CheckVoteResult( TEAM_RED, 0, qfalse, NULL );
	/* blue: 2 of 3 yes is more than half */
	StartTeamVote( 1, "g_testBlueTeamVote 2", 2, 0 );
	CheckVoteResult( TEAM_BLUE, 1, qtrue, "g_testBlueTeamVote 2\n" );
}

static void Test_FreeForAll( void ) {
	testCase = "free for all";
	SetupServer( GT_FFA );

	/* clients 0 to 4 vote; nobody is on a team */
	RunCalculateRanks( 5, 0, 0 );
	if ( level.numConnectedClients != 9 || level.numNonSpectatorClients != 8 || level.numPlayingClients != 7 ) {
		Fail( "wrong connected, non-spectator or playing count" );
	}
}

int main( void ) {
	Test_TeamGame( GT_TEAM, "team deathmatch" );
	Test_TeamGame( GT_CTF, "capture the flag" );
	Test_FreeForAll();
	printf( "team voting bounds regression passed.\n" );
	return 0;
}
