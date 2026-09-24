/*
 * Issue #363: Cmd_Vote_f and Cmd_TeamVote_f (code/game/g_cmds.c) decided a
 * ballot with "msg[0] == 'y' || msg[1] == 'Y' || msg[1] == '1'", as retail
 * 1.32c does. The last two tests look at the second character, so "vote Y",
 * "vote Yes" and "vote 1" (and the same for teamvote) counted as no, while
 * "vote nY" and "vote 01" counted as yes. ioquake3 tests the first character
 * only: "tolower( msg[0] ) == 'y' || msg[0] == '1'".
 *
 * The stock clients send "vote yes"/"vote no" (default.cfg binds F1/F2) and
 * "teamvote yes"/"teamvote no" (Team Arena's in-game vote menu); both give
 * the same result under either test, and this regression checks them first.
 *
 * The test links the real g_cmds.c and drives both commands through the real
 * ClientCommand() entry, the way the engine hands client commands to the
 * statically linked game module, with a vote (and a team vote for each team)
 * in progress as Cmd_CallVote_f/Cmd_CallTeamVote_f leave it. Each ballot must
 * tell the voter it was cast, mark the voter, and move exactly one counter
 * and its configstring: yes for y, Y, yes, Yes, YES and 1; no for n, N, no,
 * 0, an empty argument and no argument. It then sweeps every first byte
 * 1..255, alone and followed by 'Y' or '1': only 'y', 'Y' and '1' first
 * count as yes. On master "Y", "Yes", "YES", "1" and "10" count as no and
 * "nY", "n1", "0Y", "01" and "x1" as yes.
 */
#include "../code/game/g_local.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_MAXCLIENTS	4
#define VOTE_CALLED		1000

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
	fprintf( stderr, "vote regression failed: %s\n", message );
	exit( 1 );
}

/* --- the command being executed --- */
static int		testArgc;
static const char	*testArgv[2];

int trap_Argc( void ) {
	return testArgc;
}

/* Cmd_ArgvBuffer: an argument past the last one reads as "" */
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

/* --- what the command produced --- */
#define MAX_SENT	4

static int	sentCount;
static int	sentClient[MAX_SENT];
static char	sentText[MAX_SENT][MAX_STRING_CHARS];

void trap_SendServerCommand( int clientNum, const char *text ) {
	if ( sentCount >= MAX_SENT ) {
		Fail( "too many server commands for one ballot" );
	}
	sentClient[sentCount] = clientNum;
	Q_strncpyz( sentText[sentCount], text, sizeof( sentText[0] ) );
	sentCount++;
}

static int	configCount;
static int	configNum[MAX_SENT];
static char	configText[MAX_SENT][MAX_STRING_CHARS];

void trap_SetConfigstring( int num, const char *string ) {
	if ( configCount >= MAX_SENT ) {
		Fail( "too many configstrings for one ballot" );
	}
	configNum[configCount] = num;
	Q_strncpyz( configText[configCount], string, sizeof( configText[0] ) );
	configCount++;
}

/* --- symbols only other commands reach --- */
static void Unexpected( const char *name ) {
	fprintf( stderr, "vote regression failed: unexpected call to %s\n", name );
	exit( 1 );
}

void QDECL Com_Error( int errLevel, const char *error, ... ) { (void)errLevel; (void)error; Unexpected( "Com_Error" ); }
void QDECL Com_Printf( const char *msg, ... ) { (void)msg; Unexpected( "Com_Printf" ); }
void QDECL G_Error( const char *fmt, ... ) { (void)fmt; Unexpected( "G_Error" ); }
void QDECL G_LogPrintf( const char *fmt, ... ) { (void)fmt; Unexpected( "G_LogPrintf" ); }
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
qboolean Team_GetLocationMsg( gentity_t *ent, char *loc, int loclen ) {
	(void)ent; (void)loc; (void)loclen; Unexpected( "Team_GetLocationMsg" ); return qfalse;
}
void TeleportPlayer( gentity_t *player, vec3_t origin, vec3_t angles ) {
	(void)player; (void)origin; (void)angles; Unexpected( "TeleportPlayer" );
}
void Touch_Item( gentity_t *ent, gentity_t *other, trace_t *trace ) { (void)ent; (void)other; (void)trace; Unexpected( "Touch_Item" ); }
void trap_Cvar_VariableStringBuffer( const char *var_name, char *buffer, int bufsize ) {
	(void)var_name; (void)buffer; (void)bufsize; Unexpected( "trap_Cvar_VariableStringBuffer" );
}
void trap_GetUserinfo( int num, char *buffer, int bufferSize ) { (void)num; (void)buffer; (void)bufferSize; Unexpected( "trap_GetUserinfo" ); }
void trap_SendConsoleCommand( int exec_when, const char *text ) { (void)exec_when; (void)text; Unexpected( "trap_SendConsoleCommand" ); }
void trap_SetUserinfo( int num, const char *buffer ) { (void)num; (void)buffer; Unexpected( "trap_SetUserinfo" ); }
char *vtos( const vec3_t v ) { (void)v; Unexpected( "vtos" ); return NULL; }

/* --- test harness --- */

/* One kind of ballot: the global vote, or one team's team vote. */
typedef struct {
	const char	*command;	/* the client command */
	const char	*castText;	/* what the voter is told */
	int			voter;		/* client slot that votes */
	int			flag;		/* EF_VOTED or EF_TEAMVOTED */
	int			yesCs;
	int			noCs;
	int			*time;		/* level fields the call set up */
	int			*yes;
	int			*no;
} ballot_t;

/*
 * A server with every slot connected and in game. In a team game even slots
 * are red and odd slots blue; otherwise everyone is TEAM_FREE.
 */
static void SetupServer( int gametype ) {
	int i;

	memset( &level, 0, sizeof( level ) );
	memset( g_entities, 0, sizeof( g_entities ) );
	memset( g_clients, 0, sizeof( g_clients ) );

	g_gametype.integer = gametype;
	level.maxclients = TEST_MAXCLIENTS;
	level.clients = g_clients;
	level.num_entities = MAX_CLIENTS;
	level.time = VOTE_CALLED;

	for ( i = 0 ; i < TEST_MAXCLIENTS ; i++ ) {
		g_entities[i].s.number = i;
		g_entities[i].inuse = qtrue;
		g_entities[i].client = &g_clients[i];
		g_clients[i].ps.clientNum = i;
		g_clients[i].pers.connected = CON_CONNECTED;
		if ( gametype >= GT_TEAM ) {
			g_clients[i].sess.sessionTeam = ( i & 1 ) ? TEAM_BLUE : TEAM_RED;
		} else {
			g_clients[i].sess.sessionTeam = TEAM_FREE;
		}
	}
}

static int	yesBallots;
static int	noBallots;
static int	failures;

static void Describe( char *out, int outSize, const char *arg ) {
	char	*o;
	int		left;

	if ( !arg ) {
		Q_strncpyz( out, "(no argument)", outSize );
		return;
	}
	o = out;
	left = outSize;
	*o++ = '"';
	left--;
	for ( ; *arg && left > 6 ; arg++ ) {
		unsigned char c = (unsigned char)*arg;
		if ( c >= 32 && c < 127 && c != '"' && c != '\\' ) {
			*o++ = (char)c;
			left--;
		} else {
			Com_sprintf( o, left, "\\x%02X", c );
			o += 4;
			left -= 4;
		}
	}
	*o++ = '"';
	*o = 0;
}

/*
 * Cast one ballot ("<command> <arg>", or just "<command>" for a NULL arg) with
 * the call just made: the caller's own yes already counted and nobody else
 * voted. Reports every mismatch; the caller stops after the batch.
 */
static void CastBallot( const ballot_t *b, const char *arg, qboolean wantYes, qboolean report ) {
	char		shown[64];
	char		sent[64];
	char		want[16];
	char		message[MAX_STRING_CHARS * 2];
	gclient_t	*voter;
	qboolean	ok;

	voter = &g_clients[b->voter];
	*b->time = VOTE_CALLED;
	*b->yes = 1;
	*b->no = 0;
	voter->ps.eFlags &= ~b->flag;

	testArgv[0] = b->command;
	testArgv[1] = arg;
	testArgc = arg ? 2 : 1;
	sentCount = 0;
	configCount = 0;
	ClientCommand( b->voter );

	Com_sprintf( want, sizeof( want ), "%d", wantYes ? 2 : 1 );
	ok = sentCount == 1 && sentClient[0] == b->voter && !strcmp( sentText[0], b->castText ) &&
		( voter->ps.eFlags & b->flag ) &&
		*b->yes == ( wantYes ? 2 : 1 ) && *b->no == ( wantYes ? 0 : 1 ) &&
		configCount == 1 && configNum[0] == ( wantYes ? b->yesCs : b->noCs ) &&
		!strcmp( configText[0], want );

	if ( ok ) {
		if ( wantYes ) {
			yesBallots++;
		} else {
			noBallots++;
		}
		return;
	}

	failures++;
	if ( !report ) {
		return;
	}
	Describe( shown, sizeof( shown ), arg );
	Describe( sent, sizeof( sent ), sentCount ? sentText[0] : "" );
	Com_sprintf( message, sizeof( message ),
		"vote regression failed: \"%s\" from client %d with %s: want %s, got yes %d no %d, "
		"%d configstring(s) (first %d \"%s\"), %d command(s) (first to %d: %s), flag %s\n",
		b->command, b->voter, shown, wantYes ? "yes" : "no", *b->yes, *b->no,
		configCount, configCount ? configNum[0] : -1, configCount ? configText[0] : "",
		sentCount, sentCount ? sentClient[0] : -1, sent,
		( voter->ps.eFlags & b->flag ) ? "set" : "clear" );
	fputs( message, stderr );
}

static const char *yesArgs[] = { "yes", "y", "Y", "Yes", "YES", "1", "yY", "Y1", "10" };
static const char *noArgs[] = { "no", "n", "N", "No", "NO", "0", "", "nY", "n1", "0Y", "01", "x1" };

static void Test_Ballot( const ballot_t *b ) {
	char	arg[3];
	int		i, c;
	int		sweepFailures;

	/* the stock client strings first: F1/F2 and the Team Arena vote menu */
	CastBallot( b, "yes", qtrue, qtrue );
	CastBallot( b, "no", qfalse, qtrue );

	for ( i = 0 ; i < (int)( sizeof( yesArgs ) / sizeof( yesArgs[0] ) ) ; i++ ) {
		CastBallot( b, yesArgs[i], qtrue, qtrue );
	}
	for ( i = 0 ; i < (int)( sizeof( noArgs ) / sizeof( noArgs[0] ) ) ; i++ ) {
		CastBallot( b, noArgs[i], qfalse, qtrue );
	}
	CastBallot( b, NULL, qfalse, qtrue );

	/* every first byte, alone and before the characters retail tested */
	sweepFailures = failures;
	for ( c = 1 ; c < 256 ; c++ ) {
		qboolean wantYes = c == 'y' || c == 'Y' || c == '1';

		arg[0] = (char)c;
		arg[1] = 0;
		CastBallot( b, arg, wantYes, qfalse );
		arg[1] = 'Y';
		arg[2] = 0;
		CastBallot( b, arg, wantYes, qfalse );
		arg[1] = '1';
		CastBallot( b, arg, wantYes, qfalse );
	}
	if ( failures != sweepFailures ) {
		fprintf( stderr, "vote regression failed: \"%s\" from client %d: %d of the %d first-byte sweep ballots miscounted\n",
			b->command, b->voter, failures - sweepFailures, 255 * 3 );
	}
}

static void Test_Vote( int gametype, int voter ) {
	ballot_t b;

	SetupServer( gametype );
	memset( &b, 0, sizeof( b ) );
	b.command = "vote";
	b.castText = "print \"Vote cast.\n\"";
	b.voter = voter;
	b.flag = EF_VOTED;
	b.yesCs = CS_VOTE_YES;
	b.noCs = CS_VOTE_NO;
	b.time = &level.voteTime;
	b.yes = &level.voteYes;
	b.no = &level.voteNo;
	Test_Ballot( &b );
}

static void Test_TeamVote( int voter ) {
	ballot_t	b;
	int			cs_offset;

	SetupServer( GT_TEAM );
	cs_offset = g_clients[voter].sess.sessionTeam == TEAM_RED ? 0 : 1;
	memset( &b, 0, sizeof( b ) );
	b.command = "teamvote";
	b.castText = "print \"Team vote cast.\n\"";
	b.voter = voter;
	b.flag = EF_TEAMVOTED;
	b.yesCs = CS_TEAMVOTE_YES + cs_offset;
	b.noCs = CS_TEAMVOTE_NO + cs_offset;
	b.time = &level.teamVoteTime[cs_offset];
	b.yes = &level.teamVoteYes[cs_offset];
	b.no = &level.teamVoteNo[cs_offset];
	Test_Ballot( &b );
}

int main( void ) {
	Test_Vote( GT_FFA, 0 );
	Test_Vote( GT_TEAM, 1 );
	Test_TeamVote( 0 );		/* red */
	Test_TeamVote( 1 );		/* blue */

	if ( failures ) {
		fprintf( stderr, "vote regression failed: %d ballot(s) miscounted.\n", failures );
		return 1;
	}
	printf( "vote regression passed (%d yes and %d no ballots).\n", yesBallots, noBallots );
	return 0;
}
