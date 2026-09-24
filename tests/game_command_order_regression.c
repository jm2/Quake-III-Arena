/*
 * Issue #359: the "gc <player> <order>" client command (Cmd_GameCommand_f in
 * code/game/g_cmds.c) checked "order > sizeof(gc_orders)/sizeof(char *)",
 * which accepts 7 for the 7-entry gc_orders[] table. Any connected client
 * could send "gc 0 7" and make the server read the pointer after the table
 * and hand it to G_Say as the chat text (the game module is native code
 * here, so that is a real out-of-bounds read followed by a string read
 * through whatever pointer sits there).
 *
 * The player number was only checked against MAX_CLIENTS. Slots from
 * level.maxclients up to MAX_CLIENTS have no gclient_t (G_InitGame only
 * wires the first level.maxclients), and in team game types the SAY_TELL
 * path of G_Say reads target->client->sess.sessionTeam before G_SayTo
 * checks it, so "gc 63 0" on a team server dereferenced NULL.
 *
 * This test links the real g_cmds.c and drives the command through the real
 * ClientCommand() entry, the way the engine hands client commands to the
 * statically linked game module, stubbing only the engine and game symbols
 * the file references. It sends player numbers -1..MAX_CLIENTS (and
 * INT_MIN/INT_MAX) with orders -1, 0..6, 7, 8, INT_MIN and INT_MAX in a free
 * for all and a team game. Invalid values must be refused with no server
 * command and no out-of-bounds read (ASan/UBSan abort on master at "gc 0 7");
 * every valid value must produce the stock SAY_TELL chat text unchanged.
 */
#include "../code/game/g_local.h"
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_MAXCLIENTS	8
#define SENDER			0
#define FREE_SLOT		3

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

static void Fail( const char *message ) {
	fprintf( stderr, "gc command regression failed: %s\n", message );
	exit( 1 );
}

/* --- the command being executed --- */
static int		testArgc;
static const char	*testArgv[3];

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

/* --- server commands the command produced --- */
#define MAX_SENT	4

static int	sentCount;
static int	sentClient[MAX_SENT];
static char	sentText[MAX_SENT][MAX_STRING_CHARS];

void trap_SendServerCommand( int clientNum, const char *text ) {
	if ( sentCount >= MAX_SENT ) {
		Fail( "too many server commands for one gc" );
	}
	sentClient[sentCount] = clientNum;
	Q_strncpyz( sentText[sentCount], text, sizeof( sentText[0] ) );
	sentCount++;
}

/* Team_GetLocationMsg (g_team.c) needs map locations; report a fixed one so
 * the team-game SAY_TELL location branch is exercised. */
qboolean Team_GetLocationMsg( gentity_t *ent, char *loc, int loclen ) {
	(void)ent;
	Q_strncpyz( loc, "Red Base", loclen );
	return qtrue;
}

void QDECL G_LogPrintf( const char *fmt, ... ) {
	(void)fmt;
}

/* --- symbols only other commands reach --- */
static void Unexpected( const char *name ) {
	fprintf( stderr, "gc command regression failed: unexpected call to %s\n", name );
	exit( 1 );
}

void QDECL Com_Error( int errLevel, const char *error, ... ) { (void)errLevel; (void)error; Unexpected( "Com_Error" ); }
void QDECL Com_Printf( const char *msg, ... ) { (void)msg; Unexpected( "Com_Printf" ); }
void QDECL G_Error( const char *fmt, ... ) { (void)fmt; Unexpected( "G_Error" ); }
void QDECL G_Printf( const char *fmt, ... ) { (void)fmt; Unexpected( "G_Printf" ); }
void BeginIntermission( void ) { Unexpected( "BeginIntermission" ); }
gitem_t *BG_FindItem( const char *pickupName ) { (void)pickupName; Unexpected( "BG_FindItem" ); return NULL; }
void CheckTeamLeader( int team ) { (void)team; Unexpected( "CheckTeamLeader" ); }
void ClientBegin( int clientNum ) { (void)clientNum; Unexpected( "ClientBegin" ); }
void ClientUserinfoChanged( int clientNum ) { (void)clientNum; Unexpected( "ClientUserinfoChanged" ); }
void CopyToBodyQue( gentity_t *ent ) { (void)ent; Unexpected( "CopyToBodyQue" ); }
void FinishSpawningItem( gentity_t *ent ) { (void)ent; Unexpected( "FinishSpawningItem" ); }
void G_FreeEntity( gentity_t *e ) { (void)e; Unexpected( "G_FreeEntity" ); }
gentity_t *G_Spawn( void ) { Unexpected( "G_Spawn" ); return NULL; }
void G_SpawnItem( gentity_t *ent, gitem_t *item ) { (void)ent; (void)item; Unexpected( "G_SpawnItem" ); }
qboolean OnSameTeam( gentity_t *ent1, gentity_t *ent2 ) { (void)ent1; (void)ent2; Unexpected( "OnSameTeam" ); return qfalse; }
team_t PickTeam( int ignoreClientNum ) { (void)ignoreClientNum; Unexpected( "PickTeam" ); return TEAM_FREE; }
void player_die( gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod ) {
	(void)self; (void)inflictor; (void)attacker; (void)damage; (void)mod; Unexpected( "player_die" );
}
void SetLeader( int team, int client ) { (void)team; (void)client; Unexpected( "SetLeader" ); }
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

/* The retail 1.32c gc_orders[] text, in order. */
static const char *stockOrders[] = {
	"hold your position",
	"hold this position",
	"come here",
	"cover me",
	"guard location",
	"search and destroy",
	"report"
};
#define NUM_STOCK_ORDERS	( (int)( sizeof( stockOrders ) / sizeof( stockOrders[0] ) ) )

static const char *slotNames[TEST_MAXCLIENTS] = {
	"Sender", "Mate", "Enemy", NULL, "Bot", "Four", "Five", "Six"
};

/*
 * A server the way G_InitGame and ClientBegin leave it: only the first
 * level.maxclients entities have a gclient_t, slot FREE_SLOT is a
 * disconnected client, and the rest are connected players. In a team game
 * even slots are red and odd slots blue, so the sender (slot 0) shares a team
 * with some targets and not with others.
 */
static void SetupServer( int gametype ) {
	int i;

	memset( &level, 0, sizeof( level ) );
	memset( g_entities, 0, sizeof( g_entities ) );
	memset( g_clients, 0, sizeof( g_clients ) );

	g_gametype.integer = gametype;
	g_dedicated.integer = 0;
	level.maxclients = TEST_MAXCLIENTS;
	level.clients = g_clients;
	level.num_entities = MAX_CLIENTS;

	for ( i = 0 ; i < TEST_MAXCLIENTS ; i++ ) {
		g_entities[i].s.number = i;
		g_entities[i].client = &g_clients[i];
		g_clients[i].ps.clientNum = i;
		if ( i == FREE_SLOT ) {
			g_clients[i].pers.connected = CON_DISCONNECTED;
			continue;
		}
		g_entities[i].inuse = qtrue;
		g_clients[i].pers.connected = CON_CONNECTED;
		Q_strncpyz( g_clients[i].pers.netname, slotNames[i], sizeof( g_clients[i].pers.netname ) );
		if ( gametype >= GT_TEAM ) {
			g_clients[i].sess.sessionTeam = ( i & 1 ) ? TEAM_BLUE : TEAM_RED;
		} else {
			g_clients[i].sess.sessionTeam = TEAM_FREE;
		}
	}
	g_entities[4].r.svFlags |= SVF_BOT;
}

static void RunGc( const char *player, const char *order ) {
	testArgc = 3;
	testArgv[0] = "gc";
	testArgv[1] = player;
	testArgv[2] = order;
	sentCount = 0;
	ClientCommand( SENDER );
}

/* The stock SAY_TELL text G_Say sends for an order from the sender. */
static void StockTell( char *out, int outSize, int gametype, int target, int order ) {
	qboolean located;

	located = gametype >= GT_TEAM &&
		g_clients[target].sess.sessionTeam == g_clients[SENDER].sess.sessionTeam;
	if ( located ) {
		Com_sprintf( out, outSize, "chat \"" "\x19" "[%s^7" "\x19" "] (Red Base)" "\x19" ": ^6%s\"",
			slotNames[SENDER], stockOrders[order] );
	} else {
		Com_sprintf( out, outSize, "chat \"" "\x19" "[%s^7" "\x19" "]" "\x19" ": ^6%s\"",
			slotNames[SENDER], stockOrders[order] );
	}
}

static int	validRuns;
static int	refusedRuns;

static void ExpectRefused( int gametype, const char *player, const char *order ) {
	char message[256];

	RunGc( player, order );
	if ( sentCount != 0 ) {
		Com_sprintf( message, sizeof( message ),
			"gametype %d: \"gc %s %s\" was not refused (%d command(s), first to %d: %s)",
			gametype, player, order, sentCount, sentClient[0], sentText[0] );
		Fail( message );
	}
	refusedRuns++;
}

static void ExpectStock( int gametype, int target, int order ) {
	char	player[16];
	char	orderText[16];
	char	expectTarget[MAX_STRING_CHARS];
	char	expectSelf[MAX_STRING_CHARS];
	char	message[MAX_STRING_CHARS * 2];

	Com_sprintf( player, sizeof( player ), "%d", target );
	Com_sprintf( orderText, sizeof( orderText ), "%d", order );
	RunGc( player, orderText );

	StockTell( expectTarget, sizeof( expectTarget ), gametype, target, order );
	StockTell( expectSelf, sizeof( expectSelf ), gametype, SENDER, order );

	/* retail: one tell to the target, then one echo to the sender (also
	 * when the target is the sender, which then gets it twice) */
	if ( sentCount != 2 ||
		sentClient[0] != target || strcmp( sentText[0], expectTarget ) ||
		sentClient[1] != SENDER || strcmp( sentText[1], expectSelf ) ) {
		Com_sprintf( message, sizeof( message ),
			"gametype %d: \"gc %d %d\" sent %d command(s); want \"%s\" to %d and \"%s\" to %d, got \"%s\" to %d and \"%s\" to %d",
			gametype, target, order, sentCount, expectTarget, target, expectSelf, SENDER,
			sentCount > 0 ? sentText[0] : "", sentCount > 0 ? sentClient[0] : -1,
			sentCount > 1 ? sentText[1] : "", sentCount > 1 ? sentClient[1] : -1 );
		Fail( message );
	}
	validRuns++;
}

static qboolean ValidTarget( int player ) {
	return player >= 0 && player < TEST_MAXCLIENTS && player != FREE_SLOT;
}

static void Test_GameType( int gametype ) {
	static const int	badOrders[] = { -1, NUM_STOCK_ORDERS, NUM_STOCK_ORDERS + 1, INT_MAX, INT_MIN };
	char	player[16];
	char	order[16];
	int		p, o, i;

	SetupServer( gametype );

	/* stock chat for every valid target and order, before any bad value */
	for ( p = 0 ; p < TEST_MAXCLIENTS ; p++ ) {
		if ( !ValidTarget( p ) ) {
			continue;
		}
		for ( o = 0 ; o < NUM_STOCK_ORDERS ; o++ ) {
			ExpectStock( gametype, p, o );
		}
	}

	/* the reported read: order 7 (and other bad orders) to a valid player */
	for ( p = 0 ; p < TEST_MAXCLIENTS ; p++ ) {
		if ( !ValidTarget( p ) ) {
			continue;
		}
		for ( i = 0 ; i < (int)( sizeof( badOrders ) / sizeof( badOrders[0] ) ) ; i++ ) {
			Com_sprintf( player, sizeof( player ), "%d", p );
			Com_sprintf( order, sizeof( order ), "%d", badOrders[i] );
			ExpectRefused( gametype, player, order );
		}
	}

	/* every other player number -1..MAX_CLIENTS with every order */
	for ( p = -1 ; p <= MAX_CLIENTS ; p++ ) {
		if ( ValidTarget( p ) ) {
			continue;
		}
		Com_sprintf( player, sizeof( player ), "%d", p );
		for ( o = -1 ; o <= NUM_STOCK_ORDERS ; o++ ) {
			Com_sprintf( order, sizeof( order ), "%d", o );
			ExpectRefused( gametype, player, order );
		}
		Com_sprintf( order, sizeof( order ), "%d", INT_MAX );
		ExpectRefused( gametype, player, order );
	}
	Com_sprintf( player, sizeof( player ), "%d", INT_MAX );
	ExpectRefused( gametype, player, "0" );
	Com_sprintf( player, sizeof( player ), "%d", INT_MIN );
	ExpectRefused( gametype, player, "0" );
}

int main( void ) {
	Test_GameType( GT_FFA );
	Test_GameType( GT_TEAM );
	Test_GameType( GT_CTF );
	printf( "gc command regression passed (%d stock tells, %d refused commands).\n",
		validRuns, refusedRuns );
	return 0;
}
