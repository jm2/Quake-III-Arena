/*
 * Issue #361: the "follownext" and "followprev" client commands, and the
 * attack button of a spectator (SpectatorThink), run Cmd_FollowCycle_f in
 * code/game/g_cmds.c, which walks the client slots with
 *
 *	do { clientnum += dir; wrap into 0..level.maxclients-1; ... }
 *	while ( clientnum != original );
 *
 * and leaves early only when it finds a connected non-spectator to follow.
 * "team follow1" and "team follow2" make sess.spectatorClient -1 and -2,
 * which clientnum can never take again, so a follow1/follow2 spectator that
 * cycles while nobody is playing spins forever inside the server frame: any
 * client can hang an empty or spectator-only server. StopFollowing (a bare
 * "follow", or the intermission) keeps the -1 or -2 for a free spectator,
 * and a spectatorClient restored from session data after sv_maxclients was
 * lowered (a slot at or past the new level.maxclients) never comes back
 * around either.
 *
 * ioquake3 (02f3664b) switches a follow1/follow2 spectator between the two
 * dedicated follow modes instead of entering the loop. This port does that
 * for spectators in follow mode, and bounds the loop to one pass over the
 * slots so every other start keeps its retail 1.32c choice and returns.
 *
 * The test links the real g_cmds.c and sends the commands through the real
 * ClientCommand() entry, the way the engine hands client commands to the
 * statically linked game module, stubbing only the engine and game symbols
 * the file references. Every command runs under alarm(), so a hang fails in
 * seconds instead of stalling CI. It covers:
 *  - follow1/follow2 with nobody to follow (only the sender, spectators,
 *    connecting and disconnected slots, one slot, MAX_CLIENTS slots): the
 *    command returns and switches follow1 <-> follow2, touching nothing else;
 *  - follow1/follow2 with players: the same switch;
 *  - a free spectator left with -1 or -2, and "team follow1", "follow",
 *    "follownext" end to end;
 *  - normal cycling forward and back across mixed spectators, players,
 *    connecting and disconnected slots in free for all, team and tournament
 *    games, hand-checked, and against the retail rule for every layout of a
 *    five-slot server, every start and both directions;
 *  - a single player to follow, including that player sending follownext
 *    (the retail SetTeam path);
 *  - a stale spectatorClient at or past level.maxclients.
 */
#include "../code/game/g_local.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* seconds for one command, which takes microseconds even under qemu */
#define COMMAND_TIMEOUT	10

/* --- globals the file references --- */
level_locals_t	level;
gentity_t		g_entities[MAX_GENTITIES];
vmCvar_t		g_gametype;
vmCvar_t		g_dedicated;
vmCvar_t		g_cheats;
vmCvar_t		g_maxGameClients;
vmCvar_t		g_allowVote;
vmCvar_t		g_teamForceBalance;

static gclient_t	g_clients[MAX_CLIENTS];

static char	currentCase[256];

static void Fail( const char *message ) {
	fprintf( stderr, "follow cycle regression failed: %s: %s\n", currentCase, message );
	exit( 1 );
}

static void Timeout( int sig ) {
	static const char	prefix[] = "follow cycle regression failed: ";
	static const char	suffix[] = ": command did not return (server hang)\n";
	ssize_t				written;

	(void)sig;
	written = write( 2, prefix, sizeof( prefix ) - 1 );
	written = write( 2, currentCase, strlen( currentCase ) );
	written = write( 2, suffix, sizeof( suffix ) - 1 );
	(void)written;
	_exit( 1 );
}

/* --- the command being executed --- */
static int			testArgc;
static const char	*testArgv[2];

int trap_Argc( void ) {
	return testArgc;
}

void trap_Argv( int n, char *buffer, int bufferLength ) {
	if ( bufferLength <= 0 ) {
		Fail( "trap_Argv called with no buffer" );
	}
	if ( n < 0 || n >= testArgc ) {
		buffer[0] = 0;
		return;
	}
	Q_strncpyz( buffer, testArgv[n], bufferLength );
}

/* --- what the retail SetTeam path does to a player --- */
static int	sentCount;
static int	playerDieCount;
static int	userinfoChangedClient;
static int	beginClient;

void trap_SendServerCommand( int clientNum, const char *text ) {
	(void)clientNum;
	(void)text;
	sentCount++;
}

void player_die( gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod ) {
	(void)self; (void)inflictor; (void)attacker; (void)damage; (void)mod;
	playerDieCount++;
}

void ClientUserinfoChanged( int clientNum ) {
	userinfoChangedClient = clientNum;
}

void ClientBegin( int clientNum ) {
	beginClient = clientNum;
}

/* --- symbols only other commands reach --- */
static void Unexpected( const char *name ) {
	fprintf( stderr, "follow cycle regression failed: %s: unexpected call to %s\n", currentCase, name );
	exit( 1 );
}

void QDECL Com_Error( int errLevel, const char *error, ... ) { (void)errLevel; (void)error; Unexpected( "Com_Error" ); }
void QDECL Com_Printf( const char *msg, ... ) { (void)msg; Unexpected( "Com_Printf" ); }
void QDECL G_Error( const char *fmt, ... ) { (void)fmt; Unexpected( "G_Error" ); }
void QDECL G_Printf( const char *fmt, ... ) { (void)fmt; Unexpected( "G_Printf" ); }
void QDECL G_LogPrintf( const char *fmt, ... ) { (void)fmt; Unexpected( "G_LogPrintf" ); }
void BeginIntermission( void ) { Unexpected( "BeginIntermission" ); }
gitem_t *BG_FindItem( const char *pickupName ) { (void)pickupName; Unexpected( "BG_FindItem" ); return NULL; }
void CheckTeamLeader( int team ) { (void)team; Unexpected( "CheckTeamLeader" ); }
void CopyToBodyQue( gentity_t *ent ) { (void)ent; Unexpected( "CopyToBodyQue" ); }
void FinishSpawningItem( gentity_t *ent ) { (void)ent; Unexpected( "FinishSpawningItem" ); }
void G_FreeEntity( gentity_t *e ) { (void)e; Unexpected( "G_FreeEntity" ); }
gentity_t *G_Spawn( void ) { Unexpected( "G_Spawn" ); return NULL; }
void G_SpawnItem( gentity_t *ent, gitem_t *item ) { (void)ent; (void)item; Unexpected( "G_SpawnItem" ); }
qboolean OnSameTeam( gentity_t *ent1, gentity_t *ent2 ) { (void)ent1; (void)ent2; Unexpected( "OnSameTeam" ); return qfalse; }
team_t PickTeam( int ignoreClientNum ) { (void)ignoreClientNum; Unexpected( "PickTeam" ); return TEAM_FREE; }
void SetLeader( int team, int client ) { (void)team; (void)client; Unexpected( "SetLeader" ); }
qboolean Team_GetLocationMsg( gentity_t *ent, char *loc, int loclen ) {
	(void)ent; (void)loc; (void)loclen; Unexpected( "Team_GetLocationMsg" ); return qfalse;
}
team_t TeamCount( int ignoreClientNum, int team ) { (void)ignoreClientNum; (void)team; Unexpected( "TeamCount" ); return 0; }
int TeamLeader( int team ) { (void)team; Unexpected( "TeamLeader" ); return -1; }
void TeleportPlayer( gentity_t *player, vec3_t origin, vec3_t angles ) {
	(void)player; (void)origin; (void)angles; Unexpected( "TeleportPlayer" );
}
void Touch_Item( gentity_t *ent, gentity_t *other, trace_t *trace ) { (void)ent; (void)other; (void)trace; Unexpected( "Touch_Item" ); }
void trap_Cvar_VariableStringBuffer( const char *var_name, char *buffer, int bufsize ) {
	(void)var_name; (void)buffer; (void)bufsize; Unexpected( "trap_Cvar_VariableStringBuffer" );
}
void trap_GetUserinfo( int num, char *buffer, int bufferSize ) { (void)num; (void)buffer; (void)bufferSize; Unexpected( "trap_GetUserinfo" ); }
void trap_SendConsoleCommand( int exec_when, const char *text ) { (void)exec_when; (void)text; Unexpected( "trap_SendConsoleCommand" ); }
void trap_SetConfigstring( int num, const char *string ) { (void)num; (void)string; Unexpected( "trap_SetConfigstring" ); }
void trap_SetUserinfo( int num, const char *buffer ) { (void)num; (void)buffer; Unexpected( "trap_SetUserinfo" ); }
char *vtos( const vec3_t v ) { (void)v; Unexpected( "vtos" ); return NULL; }

/* --- test harness --- */

#define FOLLOW1		-1		/* sess.spectatorClient for "team follow1" */
#define FOLLOW2		-2		/* and for "team follow2" */
#define NO_SLOT		-100	/* RetailChoice: the command keeps the spectator as it was */

/* what occupies a client slot */
enum {
	EMPTY,		/* disconnected */
	CONNECTING,	/* CON_CONNECTING, not yet in the game */
	PLAYER,		/* connected, playing */
	SPECTATOR	/* connected spectator */
};

static int	commandCount;

/*
 * A server the way G_InitGame, ClientConnect and ClientBegin leave it: only
 * the first maxclients entities have a gclient_t. Disconnected slots keep a
 * stale sessionTeam (ClientDisconnect sets TEAM_FREE; a red one is used here
 * so only the connected check can skip it), and connecting ones are TEAM_FREE
 * as G_InitSessionData leaves a free for all client. In team game types even
 * player slots are red and odd ones blue.
 */
static void SetupServer( int gametype, int maxclients, const int *layout ) {
	gclient_t	*cl;
	int			i;

	memset( &level, 0, sizeof( level ) );
	memset( g_entities, 0, sizeof( g_entities ) );
	memset( g_clients, 0, sizeof( g_clients ) );

	g_gametype.integer = gametype;
	level.maxclients = maxclients;
	level.clients = g_clients;
	level.num_entities = MAX_CLIENTS;

	for ( i = 0 ; i < maxclients ; i++ ) {
		cl = &g_clients[i];
		g_entities[i].s.number = i;
		g_entities[i].client = cl;
		cl->ps.clientNum = i;
		Com_sprintf( cl->pers.netname, sizeof( cl->pers.netname ), "Client%d", i );
		switch ( layout[i] ) {
		case EMPTY:
			cl->pers.connected = CON_DISCONNECTED;
			cl->sess.sessionTeam = TEAM_RED;
			break;
		case CONNECTING:
			g_entities[i].inuse = qtrue;
			cl->pers.connected = CON_CONNECTING;
			cl->sess.sessionTeam = TEAM_FREE;
			break;
		case PLAYER:
			g_entities[i].inuse = qtrue;
			g_entities[i].health = 100;
			cl->pers.connected = CON_CONNECTED;
			cl->ps.stats[STAT_HEALTH] = 100;
			if ( gametype >= GT_TEAM ) {
				cl->sess.sessionTeam = ( i & 1 ) ? TEAM_BLUE : TEAM_RED;
			} else {
				cl->sess.sessionTeam = TEAM_FREE;
			}
			cl->sess.spectatorState = SPECTATOR_NOT;
			break;
		case SPECTATOR:
			g_entities[i].inuse = qtrue;
			g_entities[i].health = 125;
			cl->pers.connected = CON_CONNECTED;
			cl->ps.stats[STAT_HEALTH] = 125;
			cl->sess.sessionTeam = TEAM_SPECTATOR;
			cl->sess.spectatorState = SPECTATOR_FREE;
			break;
		default:
			Fail( "bad layout" );
		}
	}
}

static void SetCase( const char *what, int sender, const char *command, int startClient, int startState ) {
	Com_sprintf( currentCase, sizeof( currentCase ),
		"%s: slot %d (spectatorState %d, spectatorClient %d) sends %s",
		what, sender, startState, startClient, command );
}

/* "command" alone, or "command argument" */
static void RunCommandArg( int sender, const char *command, const char *argument ) {
	testArgc = argument ? 2 : 1;
	testArgv[0] = command;
	testArgv[1] = argument;
	sentCount = 0;
	playerDieCount = 0;
	userinfoChangedClient = -1;
	beginClient = -1;
	alarm( COMMAND_TIMEOUT );
	ClientCommand( sender );
	alarm( 0 );
	commandCount++;
}

static void RunCommand( int sender, const char *command ) {
	RunCommandArg( sender, command, NULL );
}

/*
 * A spectator sends the command: it may only change its own
 * sess.spectatorClient and sess.spectatorState; every other byte of every
 * client and entity stays as it was and nothing is sent.
 */
static void ExpectSpectator( const char *what, int sender, const char *command,
		int startClient, int startState, int wantClient, int wantState ) {
	static gclient_t	beforeClients[MAX_CLIENTS];
	static gentity_t	beforeEntities[MAX_CLIENTS];
	gclient_t			*cl;
	char				message[256];
	int					slots;

	SetCase( what, sender, command, startClient, startState );
	cl = &g_clients[sender];
	if ( cl->pers.connected != CON_CONNECTED || cl->sess.sessionTeam != TEAM_SPECTATOR ) {
		Fail( "the sender is not a connected spectator" );
	}
	cl->sess.spectatorClient = startClient;
	cl->sess.spectatorState = startState;

	/* the configured slots and one past them */
	slots = level.maxclients < MAX_CLIENTS ? level.maxclients + 1 : MAX_CLIENTS;
	memcpy( beforeClients, g_clients, slots * sizeof( g_clients[0] ) );
	memcpy( beforeEntities, g_entities, slots * sizeof( g_entities[0] ) );
	beforeClients[sender].sess.spectatorClient = wantClient;
	beforeClients[sender].sess.spectatorState = wantState;

	RunCommand( sender, command );

	if ( cl->sess.spectatorClient != wantClient || (int)cl->sess.spectatorState != wantState ) {
		Com_sprintf( message, sizeof( message ),
			"got spectatorState %d, spectatorClient %d; want spectatorState %d, spectatorClient %d",
			(int)cl->sess.spectatorState, cl->sess.spectatorClient, wantState, wantClient );
		Fail( message );
	}
	if ( memcmp( beforeClients, g_clients, slots * sizeof( g_clients[0] ) ) ) {
		Fail( "a client changed beyond the sender's spectatorClient and spectatorState" );
	}
	if ( memcmp( beforeEntities, g_entities, slots * sizeof( g_entities[0] ) ) ) {
		Fail( "an entity changed" );
	}
	if ( sentCount != 0 ) {
		Fail( "a server command was sent" );
	}
}

/* follownext or followprev from a spectator that keeps its state */
static void ExpectUnchanged( const char *what, int sender, const char *command, int startClient, int startState ) {
	ExpectSpectator( what, sender, command, startClient, startState, startClient, startState );
}

/* follownext or followprev from a spectator that ends up following wantClient */
static void ExpectFollow( const char *what, int sender, const char *command, int startClient, int startState, int wantClient ) {
	ExpectSpectator( what, sender, command, startClient, startState, wantClient, SPECTATOR_FOLLOW );
}

/* follownext and followprev from follow1 and follow2: switch to the other one */
static void ExpectDedicatedSwitch( const char *what, int sender ) {
	ExpectSpectator( what, sender, "follownext", FOLLOW1, SPECTATOR_FOLLOW, FOLLOW2, SPECTATOR_FOLLOW );
	ExpectSpectator( what, sender, "followprev", FOLLOW1, SPECTATOR_FOLLOW, FOLLOW2, SPECTATOR_FOLLOW );
	ExpectSpectator( what, sender, "follownext", FOLLOW2, SPECTATOR_FOLLOW, FOLLOW1, SPECTATOR_FOLLOW );
	ExpectSpectator( what, sender, "followprev", FOLLOW2, SPECTATOR_FOLLOW, FOLLOW1, SPECTATOR_FOLLOW );
}

/*
 * The retail 1.32c choice: the first step wraps as the retail loop does (to
 * slot 0 past the last slot, to the last slot below 0), then the walk goes
 * round the slots once and takes the first connected non-spectator, which
 * for a start inside 0..level.maxclients-1 is the start slot itself last.
 * NO_SLOT when there is none; the command then leaves the spectator as it
 * was (where the retail loop spins for a start it cannot come back to).
 */
static int RetailChoice( int start, int dir ) {
	gclient_t	*cl;
	int			step, slot;

	slot = start + dir;
	if ( slot >= level.maxclients ) {
		slot = 0;
	}
	if ( slot < 0 ) {
		slot = level.maxclients - 1;
	}
	for ( step = 0 ; step < level.maxclients ; step++ ) {
		cl = &g_clients[slot];
		if ( cl->pers.connected == CON_CONNECTED && cl->sess.sessionTeam != TEAM_SPECTATOR ) {
			return slot;
		}
		slot = ( slot + dir + level.maxclients ) % level.maxclients;
	}
	return NO_SLOT;
}

static void ExpectRetail( const char *what, int sender, int startClient, int startState ) {
	static const char	*commands[2] = { "follownext", "followprev" };
	static const int	dirs[2] = { 1, -1 };
	int					i, want;

	for ( i = 0 ; i < 2 ; i++ ) {
		want = RetailChoice( startClient, dirs[i] );
		if ( want == NO_SLOT ) {
			ExpectUnchanged( what, sender, commands[i], startClient, startState );
		} else {
			ExpectFollow( what, sender, commands[i], startClient, startState, want );
		}
	}
}

/* issue #361: follow1/follow2 with nobody to follow */
static void Test_NobodyToFollow( void ) {
	static const int	alone[MAX_CLIENTS] = { SPECTATOR };
	static const int	spectators[8] = {
		SPECTATOR, SPECTATOR, CONNECTING, EMPTY, SPECTATOR, EMPTY, CONNECTING, SPECTATOR
	};
	int					gametypes[3] = { GT_FFA, GT_TOURNAMENT, GT_CTF };
	int					g, start;

	for ( g = 0 ; g < 3 ; g++ ) {
		SetupServer( gametypes[g], 8, alone );
		ExpectDedicatedSwitch( "only the sender on the server", 0 );
		for ( start = 0 ; start < 8 ; start++ ) {
			ExpectUnchanged( "only the sender on the server", 0, "follownext", start, SPECTATOR_FREE );
			ExpectUnchanged( "only the sender on the server", 0, "followprev", start, SPECTATOR_FREE );
		}

		SetupServer( gametypes[g], 8, spectators );
		ExpectDedicatedSwitch( "spectators, connecting and disconnected slots", 1 );
		ExpectDedicatedSwitch( "spectators, connecting and disconnected slots", 7 );
		for ( start = 0 ; start < 8 ; start++ ) {
			ExpectUnchanged( "spectators, connecting and disconnected slots", 4, "follownext", start, SPECTATOR_FREE );
			ExpectUnchanged( "spectators, connecting and disconnected slots", 4, "followprev", start, SPECTATOR_FOLLOW );
		}
	}

	SetupServer( GT_FFA, 1, alone );
	ExpectDedicatedSwitch( "a one-slot server", 0 );
	ExpectUnchanged( "a one-slot server", 0, "follownext", 0, SPECTATOR_FREE );
	ExpectUnchanged( "a one-slot server", 0, "followprev", 0, SPECTATOR_FREE );

	SetupServer( GT_FFA, MAX_CLIENTS, alone );
	ExpectDedicatedSwitch( "a MAX_CLIENTS server with only the sender", 0 );
	ExpectUnchanged( "a MAX_CLIENTS server with only the sender", 0, "follownext", MAX_CLIENTS - 1, SPECTATOR_FREE );
	ExpectUnchanged( "a MAX_CLIENTS server with only the sender", 0, "followprev", 0, SPECTATOR_FREE );
}

/* a free spectator that StopFollowing left with follow1's -1 or follow2's -2 */
static void Test_FreeAfterDedicated( void ) {
	static const int	alone[8] = { SPECTATOR };
	static const int	mixed[8] = {
		PLAYER, SPECTATOR, EMPTY, PLAYER, CONNECTING, SPECTATOR, PLAYER, EMPTY
	};
	static const int	edges[8] = {
		SPECTATOR, PLAYER, EMPTY, EMPTY, CONNECTING, SPECTATOR, EMPTY, PLAYER
	};

	/* nobody to follow: the command returns and keeps it */
	SetupServer( GT_FFA, 8, alone );
	ExpectUnchanged( "free after follow1, nobody to follow", 0, "follownext", FOLLOW1, SPECTATOR_FREE );
	ExpectUnchanged( "free after follow1, nobody to follow", 0, "followprev", FOLLOW1, SPECTATOR_FREE );
	ExpectUnchanged( "free after follow2, nobody to follow", 0, "follownext", FOLLOW2, SPECTATOR_FREE );
	ExpectUnchanged( "free after follow2, nobody to follow", 0, "followprev", FOLLOW2, SPECTATOR_FREE );

	/* players: the retail choice, which steps from -1 up to slot 0 and
	 * from anywhere else below 0 to the last slot first */
	SetupServer( GT_FFA, 8, mixed );
	ExpectFollow( "free after follow1", 1, "follownext", FOLLOW1, SPECTATOR_FREE, 0 );
	ExpectFollow( "free after follow1", 1, "followprev", FOLLOW1, SPECTATOR_FREE, 6 );
	ExpectFollow( "free after follow2", 1, "follownext", FOLLOW2, SPECTATOR_FREE, 0 );
	ExpectFollow( "free after follow2", 1, "followprev", FOLLOW2, SPECTATOR_FREE, 6 );

	SetupServer( GT_CTF, 8, edges );
	ExpectFollow( "free after follow1, players in the first and last slots", 0, "follownext", FOLLOW1, SPECTATOR_FREE, 1 );
	ExpectFollow( "free after follow1, players in the first and last slots", 0, "followprev", FOLLOW1, SPECTATOR_FREE, 7 );
	ExpectFollow( "free after follow2, players in the first and last slots", 5, "follownext", FOLLOW2, SPECTATOR_FREE, 7 );
	ExpectFollow( "free after follow2, players in the first and last slots", 5, "followprev", FOLLOW2, SPECTATOR_FREE, 7 );
}

static void ExpectState( gclient_t *cl, int wantState, int wantClient ) {
	char	message[256];

	if ( cl->pers.connected != CON_CONNECTED || cl->sess.sessionTeam != TEAM_SPECTATOR ||
		(int)cl->sess.spectatorState != wantState || cl->sess.spectatorClient != wantClient ) {
		Com_sprintf( message, sizeof( message ),
			"got team %d, spectatorState %d, spectatorClient %d; want a spectator with spectatorState %d, spectatorClient %d",
			(int)cl->sess.sessionTeam, (int)cl->sess.spectatorState, cl->sess.spectatorClient, wantState, wantClient );
		Fail( message );
	}
}

static void SequenceStep( int sender, const char *command, const char *argument, int wantState, int wantClient ) {
	gclient_t	*cl;
	char		text[64];

	cl = &g_clients[sender];
	if ( argument ) {
		Com_sprintf( text, sizeof( text ), "%s %s", command, argument );
	} else {
		Q_strncpyz( text, command, sizeof( text ) );
	}
	SetCase( "team follow1/follow2 sequence", sender, text, cl->sess.spectatorClient, cl->sess.spectatorState );
	RunCommandArg( sender, command, argument );
	ExpectState( cl, wantState, wantClient );
}

/* the commands a client sends, end to end */
static void Test_TeamFollowSequence( void ) {
	static const int	alone[8] = { SPECTATOR, EMPTY, SPECTATOR };
	static const int	mixed[8] = {
		PLAYER, SPECTATOR, EMPTY, PLAYER, CONNECTING, SPECTATOR, PLAYER, EMPTY
	};

	/* the reported hang: "team follow1" on a server nobody plays on */
	SetupServer( GT_FFA, 8, alone );
	SequenceStep( 2, "team", "follow1", SPECTATOR_FOLLOW, FOLLOW1 );
	SequenceStep( 2, "follownext", NULL, SPECTATOR_FOLLOW, FOLLOW2 );
	SequenceStep( 2, "followprev", NULL, SPECTATOR_FOLLOW, FOLLOW1 );
	SequenceStep( 2, "follow", NULL, SPECTATOR_FREE, FOLLOW1 );
	SequenceStep( 2, "follownext", NULL, SPECTATOR_FREE, FOLLOW1 );
	SequenceStep( 2, "followprev", NULL, SPECTATOR_FREE, FOLLOW1 );

	SetupServer( GT_FFA, 8, mixed );
	SequenceStep( 1, "team", "follow2", SPECTATOR_FOLLOW, FOLLOW2 );
	SequenceStep( 1, "follownext", NULL, SPECTATOR_FOLLOW, FOLLOW1 );
	SequenceStep( 1, "follow", NULL, SPECTATOR_FREE, FOLLOW1 );
	SequenceStep( 1, "follownext", NULL, SPECTATOR_FOLLOW, 0 );
	SequenceStep( 1, "follownext", NULL, SPECTATOR_FOLLOW, 3 );
	SequenceStep( 1, "followprev", NULL, SPECTATOR_FOLLOW, 0 );
	SequenceStep( 1, "followprev", NULL, SPECTATOR_FOLLOW, 6 );
}

/* issue #361 (the same loop): a spectatorClient from before sv_maxclients was lowered */
static void Test_StaleSlot( void ) {
	static const int	alone[8] = { SPECTATOR };
	static const int	mixed[8] = {
		PLAYER, SPECTATOR, EMPTY, PLAYER, CONNECTING, SPECTATOR, PLAYER, EMPTY
	};
	static const int	stale[] = { 8, 9, 10, MAX_CLIENTS - 1 };
	int					i;

	SetupServer( GT_FFA, 8, alone );
	for ( i = 0 ; i < (int)( sizeof( stale ) / sizeof( stale[0] ) ) ; i++ ) {
		ExpectUnchanged( "a stale slot, nobody to follow", 0, "follownext", stale[i], SPECTATOR_FREE );
		ExpectUnchanged( "a stale slot, nobody to follow", 0, "followprev", stale[i], SPECTATOR_FREE );
		ExpectUnchanged( "a stale slot, nobody to follow", 0, "follownext", stale[i], SPECTATOR_FOLLOW );
		ExpectUnchanged( "a stale slot, nobody to follow", 0, "followprev", stale[i], SPECTATOR_FOLLOW );
	}

	/* with players the retail loop found one; the choice stays the same */
	SetupServer( GT_FFA, 8, mixed );
	ExpectFollow( "a stale slot with players", 1, "follownext", 8, SPECTATOR_FREE, 0 );
	ExpectFollow( "a stale slot with players", 1, "followprev", 8, SPECTATOR_FREE, 6 );
	ExpectFollow( "a stale slot with players", 1, "follownext", 10, SPECTATOR_FREE, 0 );
	ExpectFollow( "a stale slot with players", 1, "followprev", 10, SPECTATOR_FREE, 0 );
}

/* normal cycling, hand-checked across spectators, players and free slots */
static void Test_NormalCycling( void ) {
	/* players at 0, 3 and 6 */
	static const int	mixed[8] = {
		PLAYER, SPECTATOR, EMPTY, PLAYER, CONNECTING, SPECTATOR, PLAYER, EMPTY
	};
	int					gametypes[3] = { GT_FFA, GT_TOURNAMENT, GT_CTF };
	int					g;

	for ( g = 0 ; g < 3 ; g++ ) {
		SetupServer( gametypes[g], 8, mixed );

		ExpectFollow( "mixed slots", 1, "follownext", 0, SPECTATOR_FOLLOW, 3 );
		ExpectFollow( "mixed slots", 1, "follownext", 3, SPECTATOR_FOLLOW, 6 );
		ExpectFollow( "mixed slots", 1, "follownext", 6, SPECTATOR_FOLLOW, 0 );
		ExpectFollow( "mixed slots", 1, "followprev", 0, SPECTATOR_FOLLOW, 6 );
		ExpectFollow( "mixed slots", 1, "followprev", 6, SPECTATOR_FOLLOW, 3 );
		ExpectFollow( "mixed slots", 1, "followprev", 3, SPECTATOR_FOLLOW, 0 );

		/* free spectators start from the slot SetTeam left (0) or an old one */
		ExpectFollow( "mixed slots", 1, "follownext", 0, SPECTATOR_FREE, 3 );
		ExpectFollow( "mixed slots", 1, "followprev", 0, SPECTATOR_FREE, 6 );
		ExpectFollow( "mixed slots", 5, "follownext", 5, SPECTATOR_FREE, 6 );
		ExpectFollow( "mixed slots", 5, "followprev", 5, SPECTATOR_FREE, 3 );
		ExpectFollow( "mixed slots", 5, "follownext", 7, SPECTATOR_FREE, 0 );
		ExpectFollow( "mixed slots", 5, "followprev", 1, SPECTATOR_FREE, 0 );
		ExpectFollow( "mixed slots", 5, "followprev", 4, SPECTATOR_SCOREBOARD, 3 );

		/* dedicated follow modes with players switch too (as ioquake3) */
		ExpectDedicatedSwitch( "mixed slots", 1 );
		ExpectDedicatedSwitch( "mixed slots", 5 );
	}
}

/* normal cycling with a single player to follow */
static void Test_SinglePlayer( void ) {
	static const int	single[8] = {
		SPECTATOR, EMPTY, SPECTATOR, PLAYER, CONNECTING, EMPTY, SPECTATOR, EMPTY
	};
	gclient_t			*cl;
	int					start;

	SetupServer( GT_FFA, 8, single );
	ExpectFollow( "one player", 0, "follownext", 3, SPECTATOR_FOLLOW, 3 );
	ExpectFollow( "one player", 0, "followprev", 3, SPECTATOR_FOLLOW, 3 );
	for ( start = 0 ; start < 8 ; start++ ) {
		ExpectFollow( "one player", 0, "follownext", start, SPECTATOR_FREE, 3 );
		ExpectFollow( "one player", 6, "followprev", start, SPECTATOR_FREE, 3 );
	}
	ExpectDedicatedSwitch( "one player", 2 );

	/*
	 * The player itself sends follownext: SetTeam makes it a free spectator
	 * at slot 0 (a tournament loss first), and with nobody else playing
	 * the loop comes back to 0 and leaves it there.
	 */
	SetupServer( GT_TOURNAMENT, 8, single );
	cl = &g_clients[3];
	SetCase( "one player", 3, "follownext", cl->sess.spectatorClient, cl->sess.spectatorState );
	RunCommand( 3, "follownext" );
	if ( cl->sess.sessionTeam != TEAM_SPECTATOR || cl->sess.spectatorState != SPECTATOR_FREE ||
		cl->sess.spectatorClient != 0 || cl->sess.losses != 1 ) {
		Fail( "the player did not become a free spectator at slot 0 with one loss" );
	}
	if ( playerDieCount != 1 || userinfoChangedClient != 3 || beginClient != 3 || sentCount != 1 ) {
		Fail( "the retail SetTeam path did not run" );
	}
}

/* a player sends follownext or followprev with others playing */
static void Test_PlayerToSpectator( void ) {
	static const int	mixed[8] = {
		PLAYER, SPECTATOR, EMPTY, PLAYER, CONNECTING, SPECTATOR, PLAYER, EMPTY
	};
	static const char	*commands[2] = { "follownext", "followprev" };
	static const int	want[2] = { 3, 6 };
	gclient_t			*cl;
	int					i;

	for ( i = 0 ; i < 2 ; i++ ) {
		SetupServer( GT_TOURNAMENT, 8, mixed );
		cl = &g_clients[0];
		SetCase( "a player", 0, commands[i], cl->sess.spectatorClient, cl->sess.spectatorState );
		RunCommand( 0, commands[i] );
		if ( cl->sess.sessionTeam != TEAM_SPECTATOR || cl->sess.spectatorState != SPECTATOR_FOLLOW ||
			cl->sess.spectatorClient != want[i] || cl->sess.losses != 1 ) {
			Fail( "the player did not become a spectator following the next player" );
		}
		if ( playerDieCount != 1 || userinfoChangedClient != 0 || beginClient != 0 || sentCount != 1 ) {
			Fail( "the retail SetTeam path did not run" );
		}
	}
}

/*
 * Every layout of a five-slot server against the retail rule: each other
 * slot empty, connecting, playing or spectating; starts from -2 to two past
 * the last slot for a free spectator and from 0 for a following one.
 */
static void Test_AllLayouts( void ) {
	int		layout[5];
	int		sender, code, rest, i, start;
	int		layouts;

	layouts = 0;
	for ( sender = 0 ; sender < 5 ; sender++ ) {
		/* the four other slots in base 4 */
		for ( code = 0 ; code < 4 * 4 * 4 * 4 ; code++ ) {
			rest = code;
			for ( i = 0 ; i < 5 ; i++ ) {
				if ( i == sender ) {
					layout[i] = SPECTATOR;
				} else {
					layout[i] = rest % 4;
					rest /= 4;
				}
			}
			SetupServer( ( sender & 1 ) ? GT_TEAM : GT_FFA, 5, layout );
			for ( start = -2 ; start < 5 + 2 ; start++ ) {
				ExpectRetail( "five-slot layout", sender, start, SPECTATOR_FREE );
				if ( start >= 0 ) {
					ExpectRetail( "five-slot layout", sender, start, SPECTATOR_FOLLOW );
				}
			}
			ExpectDedicatedSwitch( "five-slot layout", sender );
			layouts++;
		}
	}
	if ( layouts != 5 * 256 ) {
		Fail( "layout count" );
	}
}

int main( void ) {
	signal( SIGALRM, Timeout );

	Test_NobodyToFollow();
	Test_FreeAfterDedicated();
	Test_TeamFollowSequence();
	Test_StaleSlot();
	Test_NormalCycling();
	Test_SinglePlayer();
	Test_PlayerToSpectator();
	Test_AllLayouts();

	printf( "follow cycle regression passed (%d commands).\n", commandCount );
	return 0;
}
