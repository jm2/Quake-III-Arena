/*
 * Issue #376: G_ReadSessionData (code/game/g_session.c) restores a
 * reconnecting client's session from the "sessionN" cvar with
 *
 *	sscanf( s, "%i %i %i %i %i %i %i", &sessionTeam, ... );
 *
 * and used whatever it got. SpectatorClientEndFrame (code/game/g_active.c)
 * then follows sess.spectatorClient with only a clientNum >= 0 check before
 * &level.clients[ clientNum ], so a spectatorClient of 64 or more read past
 * g_clients[MAX_CLIENTS]. A sessionTeam past TEAM_NUM_TEAMS indexes
 * level.teamScores in AddScore (team deathmatch), and a field that sscanf
 * never reached (garbage or too few fields) was left uninitialized. Only an
 * admin or rcon can set the session cvars.
 *
 * The fix gives every field the value G_InitSessionData gives a spectator
 * before the sscanf, and afterwards resets any value the game never writes to
 * that value. A spectatorClient in the follow1/follow2 modes or below
 * MAX_CLIENTS is kept, so a slot left past a lowered sv_maxclients still gets
 * the retail treatment (see #361). SpectatorClientEndFrame also treats a slot at or past
 * level.maxclients as the empty slot it always is (G_InitGame zeroes them),
 * instead of indexing level.clients with it.
 *
 * The test links the real g_session.c and g_active.c the way the monolithic
 * build does, stubbing the cvar traps with a store of the sessionN strings,
 * and checks:
 *  - out-of-range session strings (spectatorClient 64, 1000, INT_MAX, -3 and
 *    INT_MIN, also in hex and octal; every other field out of range; garbage;
 *    too few and too many fields) read back to the values listed here, and a
 *    connected spectator following whatever was read stays inside
 *    level.clients (ASan) and does what retail does;
 *  - valid session data written by the real G_WriteClientSessionData, and by
 *    G_InitSessionData as ClientConnect runs it, reads back unchanged;
 *  - SpectatorClientEndFrame for spectatorClient values from INT_MIN to
 *    INT_MAX with 4, 5, 8 and MAX_CLIENTS slots, by hand and against the
 *    retail rule.
 */
#include "../code/game/g_local.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* the level.time of the map the client connects to */
#define TEST_LEVEL_TIME	45000

/* the slot of the client whose session is read */
#define SPECTATOR_SLOT	3

/* not in g_local.h */
void G_WriteClientSessionData( gclient_t *client );
void SpectatorClientEndFrame( gentity_t *ent );

/* --- globals the files reference --- */
level_locals_t	level;
vmCvar_t		g_gametype;
vmCvar_t		g_teamAutoJoin;
vmCvar_t		g_maxGameClients;

/*
 * level.clients, on the heap: ASan's redzone after a block this size covers
 * the gclient_t fields a read one slot past the end touches, which gcc's
 * 32-byte redzone after a global array does not
 */
static gclient_t	*g_clients;
#define CLIENTS_SIZE	( MAX_CLIENTS * sizeof( gclient_t ) )

static char	currentCase[512];
static int	checkCount;

static void Fail( const char *message ) {
	fprintf( stderr, "session data regression failed: %s: %s\n", currentCase, message );
	exit( 1 );
}

static void Unexpected( const char *name ) {
	fprintf( stderr, "session data regression failed: %s: unexpected call to %s\n", currentCase, name );
	exit( 1 );
}

/* --- the sessionN cvars --- */
static char	sessionCvars[MAX_CLIENTS][MAX_STRING_CHARS];
static int	cvarSetCount;

static int SessionSlot( const char *var_name ) {
	char	name[16];
	int		i;

	for ( i = 0 ; i < MAX_CLIENTS ; i++ ) {
		Com_sprintf( name, sizeof( name ), "session%i", i );
		if ( !strcmp( var_name, name ) ) {
			return i;
		}
	}
	Unexpected( "a cvar trap for a cvar other than sessionN" );
	return 0;
}

void trap_Cvar_VariableStringBuffer( const char *var_name, char *buffer, int bufsize ) {
	Q_strncpyz( buffer, sessionCvars[SessionSlot( var_name )], bufsize );
}

void trap_Cvar_Set( const char *var_name, const char *value ) {
	Q_strncpyz( sessionCvars[SessionSlot( var_name )], value, sizeof( sessionCvars[0] ) );
	cvarSetCount++;
}

/* --- what G_InitSessionData and SpectatorClientEndFrame call --- */
static team_t	pickedTeam;
static int		beginClient;
static int		beginCount;

team_t PickTeam( int ignoreClientNum ) {
	if ( ignoreClientNum != -1 ) {
		Fail( "PickTeam did not get -1" );
	}
	return pickedTeam;
}

void BroadcastTeamChange( gclient_t *client, int oldTeam ) {
	(void)client;
	(void)oldTeam;
}

void ClientBegin( int clientNum ) {
	beginClient = clientNum;
	beginCount++;
}

void QDECL Com_Error( int errLevel, const char *error, ... ) { (void)errLevel; (void)error; Unexpected( "Com_Error" ); }
void QDECL Com_Printf( const char *msg, ... ) { (void)msg; Unexpected( "Com_Printf" ); }
void QDECL G_Printf( const char *fmt, ... ) { (void)fmt; Unexpected( "G_Printf" ); }

/* --- the session fields --- */
typedef struct {
	int		sessionTeam;
	int		spectatorTime;
	int		spectatorState;
	int		spectatorClient;
	int		wins;
	int		losses;
	int		teamLeader;
} fields_t;

static void GetFields( const gclient_t *client, fields_t *f ) {
	f->sessionTeam = client->sess.sessionTeam;
	f->spectatorTime = client->sess.spectatorTime;
	f->spectatorState = client->sess.spectatorState;
	f->spectatorClient = client->sess.spectatorClient;
	f->wins = client->sess.wins;
	f->losses = client->sess.losses;
	f->teamLeader = client->sess.teamLeader;
}

static void SetFields( gclient_t *client, const fields_t *f ) {
	client->sess.sessionTeam = (team_t)f->sessionTeam;
	client->sess.spectatorTime = f->spectatorTime;
	client->sess.spectatorState = (spectatorState_t)f->spectatorState;
	client->sess.spectatorClient = f->spectatorClient;
	client->sess.wins = f->wins;
	client->sess.losses = f->losses;
	client->sess.teamLeader = (qboolean)f->teamLeader;
}

static void ExpectFields( const fields_t *got, const fields_t *want ) {
	char	message[512];

	if ( memcmp( got, want, sizeof( *got ) ) ) {
		Com_sprintf( message, sizeof( message ),
			"read \"%i %i %i %i %i %i %i\"; want \"%i %i %i %i %i %i %i\"",
			got->sessionTeam, got->spectatorTime, got->spectatorState, got->spectatorClient,
			got->wins, got->losses, got->teamLeader,
			want->sessionTeam, want->spectatorTime, want->spectatorState, want->spectatorClient,
			want->wins, want->losses, want->teamLeader );
		Fail( message );
	}
}

/*
 * ClientConnect for a client whose session is carried over: clear the
 * client, mark it connecting and read the session. G_ReadSessionData may
 * change only client->sess, and never the cvar.
 */
static gclient_t *ReadSession( int slot, const char *text ) {
	static gclient_t	before;
	gclient_t			*client;
	int					sets;

	client = &g_clients[slot];
	memset( client, 0, sizeof( *client ) );
	client->pers.connected = CON_CONNECTING;
	Q_strncpyz( sessionCvars[slot], text, sizeof( sessionCvars[slot] ) );
	memcpy( &before, client, sizeof( before ) );
	sets = cvarSetCount;

	G_ReadSessionData( client );

	if ( cvarSetCount != sets || strcmp( sessionCvars[slot], text ) ) {
		Fail( "G_ReadSessionData changed a cvar" );
	}
	before.sess = client->sess;
	if ( memcmp( &before, client, sizeof( before ) ) ) {
		Fail( "G_ReadSessionData changed more than client->sess" );
	}
	checkCount++;
	return client;
}

/* --- the server a spectator follows in --- */

/* a connected player whose playerState_t is easy to recognise */
static void SetPlayer( int slot ) {
	gclient_t	*cl;

	cl = &g_clients[slot];
	cl->pers.connected = CON_CONNECTED;
	cl->sess.sessionTeam = TEAM_FREE;
	cl->sess.spectatorState = SPECTATOR_NOT;
	cl->ps.clientNum = slot;
	cl->ps.commandTime = 1000 + slot;
	cl->ps.origin[0] = 10.0f * slot;
	cl->ps.eFlags = EF_FIRING | EF_VOTED;
	cl->ps.pm_flags = PMF_SCOREBOARD;
}

/*
 * maxclients (at least 4) slots as the game leaves them: slot 0 and, when
 * there are more than 4 slots, slot 4 playing (and the last one at
 * MAX_CLIENTS slots), slot 1 a spectator, slot 2 connecting, the rest empty;
 * slots from maxclients on stay zeroed, as G_InitGame leaves them. The
 * spectator in SPECTATOR_SLOT carries EF_TEAMVOTED, which a follow keeps, and
 * PMF_SCOREBOARD, which only the scoreboard state keeps.
 */
static void SetupServer( int maxclients, int follow1, int follow2 ) {
	gclient_t	*cl;

	memset( &level, 0, sizeof( level ) );
	memset( g_clients, 0, CLIENTS_SIZE );
	level.maxclients = maxclients;
	level.clients = g_clients;
	level.time = TEST_LEVEL_TIME;
	level.follow1 = follow1;
	level.follow2 = follow2;

	SetPlayer( 0 );
	if ( maxclients > 4 ) {
		SetPlayer( 4 );
	}
	if ( maxclients == MAX_CLIENTS ) {
		SetPlayer( MAX_CLIENTS - 1 );
	}
	g_clients[1].pers.connected = CON_CONNECTED;
	g_clients[1].sess.sessionTeam = TEAM_SPECTATOR;
	g_clients[1].sess.spectatorState = SPECTATOR_FREE;
	g_clients[2].pers.connected = CON_CONNECTING;
	g_clients[2].sess.sessionTeam = TEAM_FREE;

	cl = &g_clients[SPECTATOR_SLOT];
	cl->pers.connected = CON_CONNECTED;
	cl->sess.sessionTeam = TEAM_SPECTATOR;
	cl->sess.spectatorState = SPECTATOR_FREE;
	cl->ps.clientNum = SPECTATOR_SLOT;
	cl->ps.commandTime = 777;
	cl->ps.eFlags = EF_TEAMVOTED;
	cl->ps.pm_flags = PMF_SCOREBOARD;
}

/*
 * What retail 1.32c's SpectatorClientEndFrame does to the spectator, reading
 * a slot at or past level.maxclients as the zeroed, disconnected slot that
 * G_InitGame leaves there. Returns the client ClientBegin gets, or -1.
 */
static int RetailEndFrame( gclient_t *want, int slot ) {
	gclient_t	*cl;
	int			clientNum;
	int			flags;
	int			begin;

	begin = -1;
	if ( want->sess.spectatorState == SPECTATOR_FOLLOW ) {
		clientNum = want->sess.spectatorClient;
		if ( clientNum == -1 ) {
			clientNum = level.follow1;
		} else if ( clientNum == -2 ) {
			clientNum = level.follow2;
		}
		if ( clientNum >= 0 ) {
			if ( clientNum < level.maxclients ) {
				cl = &g_clients[clientNum];
				if ( cl->pers.connected == CON_CONNECTED && cl->sess.sessionTeam != TEAM_SPECTATOR ) {
					flags = ( cl->ps.eFlags & ~( EF_VOTED | EF_TEAMVOTED ) )
						| ( want->ps.eFlags & ( EF_VOTED | EF_TEAMVOTED ) );
					want->ps = cl->ps;
					want->ps.pm_flags |= PMF_FOLLOW;
					want->ps.eFlags = flags;
					return -1;
				}
			}
			if ( want->sess.spectatorClient >= 0 ) {
				want->sess.spectatorState = SPECTATOR_FREE;
				begin = slot;
			}
		}
	}
	if ( want->sess.spectatorState == SPECTATOR_SCOREBOARD ) {
		want->ps.pm_flags |= PMF_SCOREBOARD;
	} else {
		want->ps.pm_flags &= ~PMF_SCOREBOARD;
	}
	return begin;
}

/*
 * Run SpectatorClientEndFrame for the spectator in slot: it must do what
 * RetailEndFrame says and change no other client.
 */
static void ExpectEndFrame( int slot ) {
	static gclient_t	before[MAX_CLIENTS];
	static gclient_t	want;
	gentity_t			ent;
	int					wantBegin;
	char				message[512];

	memcpy( before, g_clients, CLIENTS_SIZE );
	memcpy( &want, &g_clients[slot], sizeof( want ) );
	wantBegin = RetailEndFrame( &want, slot );

	memset( &ent, 0, sizeof( ent ) );
	ent.client = &g_clients[slot];
	beginClient = -1;
	beginCount = 0;

	SpectatorClientEndFrame( &ent );

	if ( wantBegin >= 0 ? ( beginCount != 1 || beginClient != wantBegin ) : beginCount != 0 ) {
		Com_sprintf( message, sizeof( message ), "ClientBegin ran %d times (last for %d); want %s",
			beginCount, beginClient, wantBegin >= 0 ? "once for the spectator" : "none" );
		Fail( message );
	}
	if ( memcmp( &want, &g_clients[slot], sizeof( want ) ) ) {
		Com_sprintf( message, sizeof( message ),
			"got spectatorState %d, ps.clientNum %d, pm_flags 0x%x, eFlags 0x%x; want %d, %d, 0x%x, 0x%x",
			(int)g_clients[slot].sess.spectatorState, g_clients[slot].ps.clientNum,
			(unsigned)g_clients[slot].ps.pm_flags, (unsigned)g_clients[slot].ps.eFlags,
			(int)want.sess.spectatorState, want.ps.clientNum,
			(unsigned)want.ps.pm_flags, (unsigned)want.ps.eFlags );
		Fail( message );
	}
	memcpy( &before[slot], &want, sizeof( want ) );
	if ( memcmp( before, g_clients, CLIENTS_SIZE ) ) {
		Fail( "another client changed" );
	}
	checkCount++;
}

/* --- out-of-range session strings --- */

typedef struct {
	const char	*text;
	fields_t	want;
} readCase_t;

#define T	TEAM_SPECTATOR
#define N	SPECTATOR_NOT
#define F	SPECTATOR_FREE
#define W	SPECTATOR_FOLLOW
#define S	SPECTATOR_SCOREBOARD

static const readCase_t readCases[] = {
	/* spectatorClient past g_clients[MAX_CLIENTS], the case of the issue first */
	{ "3 100 2 64 0 0 0",						{ T, 100, W, 0, 0, 0, qfalse } },
	{ "3 100 2 65 0 0 0",						{ T, 100, W, 0, 0, 0, qfalse } },
	{ "3 100 2 1000 0 0 0",						{ T, 100, W, 0, 0, 0, qfalse } },
	{ "3 100 2 2147483647 0 0 0",				{ T, 100, W, 0, 0, 0, qfalse } },
	{ "3 100 2 0x40 0 0 0",						{ T, 100, W, 0, 0, 0, qfalse } },
	{ "3 100 2 0100 0 0 0",						{ T, 100, W, 0, 0, 0, qfalse } },
	/* below team follow2 */
	{ "3 100 2 -3 0 0 0",						{ T, 100, W, 0, 0, 0, qfalse } },
	{ "3 100 2 -64 0 0 0",						{ T, 100, W, 0, 0, 0, qfalse } },
	{ "3 100 2 -1000 0 0 0",					{ T, 100, W, 0, 0, 0, qfalse } },
	{ "3 100 2 -2147483648 0 0 0",				{ T, 100, W, 0, 0, 0, qfalse } },
	/* the edges that stay: follow2, follow1, slot 0 and the last slot */
	{ "3 100 2 -2 0 0 0",						{ T, 100, W, -2, 0, 0, qfalse } },
	{ "3 100 2 -1 0 0 0",						{ T, 100, W, -1, 0, 0, qfalse } },
	{ "3 100 2 0 0 0 0",						{ T, 100, W, 0, 0, 0, qfalse } },
	{ "3 100 2 63 0 0 0",						{ T, 100, W, 63, 0, 0, qfalse } },
	{ "3 100 2 0x3f 0 0 0",						{ T, 100, W, 63, 0, 0, qfalse } },
	/* sessionTeam past the teams */
	{ "4 100 0 5 1 2 0",						{ T, 100, N, 5, 1, 2, qfalse } },
	{ "64 100 0 5 1 2 0",						{ T, 100, N, 5, 1, 2, qfalse } },
	{ "1000 100 0 5 1 2 0",						{ T, 100, N, 5, 1, 2, qfalse } },
	{ "2147483647 100 0 5 1 2 0",				{ T, 100, N, 5, 1, 2, qfalse } },
	{ "-1 100 0 5 1 2 0",						{ T, 100, N, 5, 1, 2, qfalse } },
	{ "-3 100 0 5 1 2 0",						{ T, 100, N, 5, 1, 2, qfalse } },
	{ "-2147483648 100 0 5 1 2 0",				{ T, 100, N, 5, 1, 2, qfalse } },
	/* spectatorState past the states */
	{ "3 100 4 5 1 2 0",						{ T, 100, F, 5, 1, 2, qfalse } },
	{ "3 100 64 5 1 2 0",						{ T, 100, F, 5, 1, 2, qfalse } },
	{ "3 100 2147483647 5 1 2 0",				{ T, 100, F, 5, 1, 2, qfalse } },
	{ "3 100 -1 5 1 2 0",						{ T, 100, F, 5, 1, 2, qfalse } },
	{ "3 100 -3 5 1 2 0",						{ T, 100, F, 5, 1, 2, qfalse } },
	{ "3 100 -2147483648 5 1 2 0",				{ T, 100, F, 5, 1, 2, qfalse } },
	/* negative wins and losses; any count up to INT_MAX stays */
	{ "1 100 0 5 -1 -3 1",						{ TEAM_RED, 100, N, 5, 0, 0, qtrue } },
	{ "1 100 0 5 -2147483648 -1000 1",			{ TEAM_RED, 100, N, 5, 0, 0, qtrue } },
	{ "1 100 0 5 64 1000 1",					{ TEAM_RED, 100, N, 5, 64, 1000, qtrue } },
	{ "1 100 0 5 2147483647 2147483647 1",		{ TEAM_RED, 100, N, 5, INT_MAX, INT_MAX, qtrue } },
	/* teamLeader other than qfalse or qtrue */
	{ "2 100 0 5 1 2 2",						{ TEAM_BLUE, 100, N, 5, 1, 2, qfalse } },
	{ "2 100 0 5 1 2 64",						{ TEAM_BLUE, 100, N, 5, 1, 2, qfalse } },
	{ "2 100 0 5 1 2 2147483647",				{ TEAM_BLUE, 100, N, 5, 1, 2, qfalse } },
	{ "2 100 0 5 1 2 -1",						{ TEAM_BLUE, 100, N, 5, 1, 2, qfalse } },
	{ "2 100 0 5 1 2 -3",						{ TEAM_BLUE, 100, N, 5, 1, 2, qfalse } },
	/* spectatorTime is a level.time, which wraps: any value stays */
	{ "3 -2147483648 1 5 1 2 0",				{ T, INT_MIN, F, 5, 1, 2, qfalse } },
	{ "3 2147483647 1 5 1 2 0",					{ T, INT_MAX, F, 5, 1, 2, qfalse } },
	/* everything out of range at once */
	{ "1000 100 -3 64 -3 -1000 1000",			{ T, 100, F, 0, 0, 0, qfalse } },
	{ "-3 -3 -3 -3 -3 -3 -3",					{ T, -3, F, 0, 0, 0, qfalse } },
	{ "2147483647 2147483647 2147483647 2147483647 2147483647 2147483647 2147483647",
												{ T, INT_MAX, F, 0, INT_MAX, INT_MAX, qfalse } },
	{ "-2147483648 -2147483648 -2147483648 -2147483648 -2147483648 -2147483648 -2147483648",
												{ T, INT_MIN, F, 0, 0, 0, qfalse } },
	/* garbage: a field sscanf does not reach gets what G_InitSessionData gives a spectator */
	{ "",										{ T, TEST_LEVEL_TIME, F, 0, 0, 0, qfalse } },
	{ "   ",									{ T, TEST_LEVEL_TIME, F, 0, 0, 0, qfalse } },
	{ "garbage",								{ T, TEST_LEVEL_TIME, F, 0, 0, 0, qfalse } },
	{ "x 100 2 64 7 8 1",						{ T, TEST_LEVEL_TIME, F, 0, 0, 0, qfalse } },
	{ "3 100 2 x 7 8 1",						{ T, 100, W, 0, 0, 0, qfalse } },
	{ "1 100 0 5 7 8 yes",						{ TEAM_RED, 100, N, 5, 7, 8, qfalse } },
	{ "3,100,2,64,0,0,0",						{ T, TEST_LEVEL_TIME, F, 0, 0, 0, qfalse } },
	/* too few fields */
	{ "3 100 2 64",								{ T, 100, W, 0, 0, 0, qfalse } },
	{ "3 100 2 1000",							{ T, 100, W, 0, 0, 0, qfalse } },
	{ "3 100 2 -3",								{ T, 100, W, 0, 0, 0, qfalse } },
	{ "1 100 0 5 7 8",							{ TEAM_RED, 100, N, 5, 7, 8, qfalse } },
	{ "1 100 0 5 7",							{ TEAM_RED, 100, N, 5, 7, 0, qfalse } },
	{ "1 100 0 5",								{ TEAM_RED, 100, N, 5, 0, 0, qfalse } },
	{ "3 100 2",								{ T, 100, W, 0, 0, 0, qfalse } },
	{ "3 100",									{ T, 100, F, 0, 0, 0, qfalse } },
	{ "0",										{ TEAM_FREE, TEST_LEVEL_TIME, F, 0, 0, 0, qfalse } },
	/* too many fields: the rest is ignored, as before */
	{ "1 100 0 5 7 8 1 64",						{ TEAM_RED, 100, N, 5, 7, 8, qtrue } },
	{ "3 100 3 -2 7 8 0 garbage",				{ T, 100, S, -2, 7, 8, qfalse } },
};

#undef T
#undef N
#undef F
#undef W
#undef S

/*
 * Read each string, then (the worst case) follow whatever was read as a
 * connected spectator in follow mode: that must stay inside level.clients
 * and do what retail does. The read fields are checked afterwards, so that
 * on a tree without the fix ASan names the out-of-bounds read first.
 */
static void CheckOutOfRange( void ) {
	const readCase_t	*c;
	gclient_t			*client;
	fields_t			got;
	int					i;

	for ( i = 0 ; i < (int)( sizeof( readCases ) / sizeof( readCases[0] ) ) ; i++ ) {
		c = &readCases[i];
		Com_sprintf( currentCase, sizeof( currentCase ), "session%d \"%s\"", SPECTATOR_SLOT, c->text );

		SetupServer( 8, 0, 4 );
		client = ReadSession( SPECTATOR_SLOT, c->text );
		GetFields( client, &got );

		client->pers.connected = CON_CONNECTED;
		client->sess.sessionTeam = TEAM_SPECTATOR;
		client->sess.spectatorState = SPECTATOR_FOLLOW;
		client->ps.clientNum = SPECTATOR_SLOT;
		ExpectEndFrame( SPECTATOR_SLOT );

		ExpectFields( &got, &c->want );
	}
}

/* --- valid session data round-trips --- */

/*
 * Every team, spectator state, spectatorClient (follow2, follow1 and every
 * slot) and teamLeader, with counts and times from the edges, written by the
 * real G_WriteClientSessionData into every sessionN cvar in turn.
 */
static void CheckWriteRoundTrip( void ) {
	static const int	winsValues[] = { 0, 1, 1000, INT_MAX };
	static const int	lossesValues[] = { 0, 3, INT_MAX };
	static const int	timeValues[] = { 0, TEST_LEVEL_TIME, INT_MAX, -1, INT_MIN };
	char				text[MAX_STRING_CHARS];
	gclient_t			*client;
	fields_t			want;
	fields_t			got;
	int					team, state, follow, leader, i, slot;

	SetupServer( MAX_CLIENTS, 0, 4 );
	i = 0;
	for ( team = TEAM_FREE ; team < TEAM_NUM_TEAMS ; team++ ) {
		for ( state = SPECTATOR_NOT ; state <= SPECTATOR_SCOREBOARD ; state++ ) {
			for ( follow = FOLLOW_ACTIVE2 ; follow < MAX_CLIENTS ; follow++ ) {
				for ( leader = qfalse ; leader <= qtrue ; leader++, i++ ) {
					want.sessionTeam = team;
					want.spectatorTime = timeValues[i % 5];
					want.spectatorState = state;
					want.spectatorClient = follow;
					want.wins = winsValues[( i / 5 ) % 4];
					want.losses = lossesValues[( i / 20 ) % 3];
					want.teamLeader = leader;
					slot = i % MAX_CLIENTS;

					client = &g_clients[slot];
					memset( client, 0, sizeof( *client ) );
					SetFields( client, &want );
					G_WriteClientSessionData( client );
					Q_strncpyz( text, sessionCvars[slot], sizeof( text ) );
					Com_sprintf( currentCase, sizeof( currentCase ), "round trip session%d \"%s\"", slot, text );

					client = ReadSession( slot, text );
					GetFields( client, &got );
					ExpectFields( &got, &want );
				}
			}
		}
	}
}

/*
 * ClientConnect for a first-time client: G_InitSessionData writes the cvar
 * and G_ReadSessionData reads it straight back, which must change nothing.
 */
static void CheckInitRoundTrip( void ) {
	static const int	gametypes[] = { GT_FFA, GT_TOURNAMENT, GT_SINGLE_PLAYER, GT_TEAM, GT_CTF };
	static const char	*userinfos[] = { "", "\\team\\s", "\\team\\spectator", "\\team\\red" };
	gclient_t			*client;
	clientSession_t		want;
	char				userinfo[MAX_INFO_STRING];
	int					g, autoJoin, picked, u, playing, maxGame;

	for ( g = 0 ; g < (int)( sizeof( gametypes ) / sizeof( gametypes[0] ) ) ; g++ ) {
		for ( autoJoin = 0 ; autoJoin <= 1 ; autoJoin++ ) {
			for ( picked = TEAM_RED ; picked <= TEAM_BLUE ; picked++ ) {
				for ( u = 0 ; u < (int)( sizeof( userinfos ) / sizeof( userinfos[0] ) ) ; u++ ) {
					for ( playing = 0 ; playing <= 3 ; playing++ ) {
						for ( maxGame = 0 ; maxGame <= 2 ; maxGame++ ) {
							Com_sprintf( currentCase, sizeof( currentCase ),
								"G_InitSessionData then G_ReadSessionData: gametype %d, g_teamAutoJoin %d, "
								"PickTeam %d, userinfo \"%s\", %d playing, g_maxGameClients %d",
								gametypes[g], autoJoin, picked, userinfos[u], playing, maxGame );
							SetupServer( 8, 0, 4 );
							g_gametype.integer = gametypes[g];
							g_teamAutoJoin.integer = autoJoin;
							g_maxGameClients.integer = maxGame;
							pickedTeam = (team_t)picked;
							level.numNonSpectatorClients = playing;

							client = &g_clients[SPECTATOR_SLOT];
							memset( client, 0, sizeof( *client ) );
							client->pers.connected = CON_CONNECTING;
							Q_strncpyz( userinfo, userinfos[u], sizeof( userinfo ) );
							G_InitSessionData( client, userinfo );
							want = client->sess;

							G_ReadSessionData( client );
							if ( memcmp( &want, &client->sess, sizeof( want ) ) ) {
								Fail( "the session read back differs from the one G_InitSessionData wrote" );
							}
							checkCount++;
						}
					}
				}
			}
		}
	}
}

/* --- SpectatorClientEndFrame alone --- */

/* set the spectator's state and target, run it and check it by hand */
static void ExpectHand( int maxclients, int spectatorClient, int wantFollow, qboolean wantDrop ) {
	gclient_t	*cl;
	gentity_t	ent;
	char		message[512];

	Com_sprintf( currentCase, sizeof( currentCase ),
		"SpectatorClientEndFrame by hand: %d slots, following spectatorClient %d", maxclients, spectatorClient );
	SetupServer( maxclients, 0, 4 );
	cl = &g_clients[SPECTATOR_SLOT];
	cl->sess.spectatorState = SPECTATOR_FOLLOW;
	cl->sess.spectatorClient = spectatorClient;

	memset( &ent, 0, sizeof( ent ) );
	ent.client = cl;
	beginClient = -1;
	beginCount = 0;
	SpectatorClientEndFrame( &ent );

	if ( wantFollow >= 0 ) {
		if ( cl->ps.clientNum != wantFollow || cl->ps.commandTime != 1000 + wantFollow
			|| !( cl->ps.pm_flags & PMF_FOLLOW ) || cl->ps.eFlags != ( EF_FIRING | EF_TEAMVOTED )
			|| cl->sess.spectatorState != SPECTATOR_FOLLOW || beginCount != 0 ) {
			Com_sprintf( message, sizeof( message ), "did not follow slot %d", wantFollow );
			Fail( message );
		}
	} else if ( wantDrop ) {
		if ( cl->sess.spectatorState != SPECTATOR_FREE || beginCount != 1 || beginClient != SPECTATOR_SLOT
			|| cl->ps.clientNum != SPECTATOR_SLOT || ( cl->ps.pm_flags & ( PMF_FOLLOW | PMF_SCOREBOARD ) ) ) {
			Fail( "was not dropped to a free spectator" );
		}
	} else {
		if ( cl->sess.spectatorState != SPECTATOR_FOLLOW || beginCount != 0
			|| cl->ps.clientNum != SPECTATOR_SLOT || cl->ps.commandTime != 777
			|| ( cl->ps.pm_flags & PMF_FOLLOW ) ) {
			Fail( "did not stay a camera with no target" );
		}
	}
	if ( cl->sess.spectatorClient != spectatorClient ) {
		Fail( "spectatorClient changed" );
	}
	checkCount++;
}

static void CheckEndFrame( void ) {
	static const int	targets[] = {
		INT_MIN, INT_MIN + 1, -65536, -1000, -65, -64, -63, -3, -2, -1,
		0, 1, 2, 3, 4, 5, 7, 8, 9, 62, 63, 64, 65, 127, 128, 1000, 65536, INT_MAX - 1, INT_MAX
	};
	static const int	states[] = { SPECTATOR_NOT, SPECTATOR_FREE, SPECTATOR_FOLLOW, SPECTATOR_SCOREBOARD };
	static const int	slotCounts[] = { 4, 5, 8, MAX_CLIENTS };
	gclient_t			*cl;
	int					m, f, s, t, maxclients;

	/* by hand: past g_clients, past level.maxclients, and below follow2 */
	ExpectHand( 8, 0, 0, qfalse );
	ExpectHand( 8, 4, 4, qfalse );
	ExpectHand( 8, -1, 0, qfalse );
	ExpectHand( 8, -2, 4, qfalse );
	ExpectHand( 8, 1, -1, qtrue );
	ExpectHand( 8, 2, -1, qtrue );
	ExpectHand( 8, 7, -1, qtrue );
	ExpectHand( 8, 8, -1, qtrue );
	ExpectHand( 8, 63, -1, qtrue );
	ExpectHand( 8, 64, -1, qtrue );
	ExpectHand( 8, 1000, -1, qtrue );
	ExpectHand( 8, INT_MAX, -1, qtrue );
	ExpectHand( 8, -3, -1, qfalse );
	ExpectHand( 8, INT_MIN, -1, qfalse );
	ExpectHand( MAX_CLIENTS, MAX_CLIENTS - 1, MAX_CLIENTS - 1, qfalse );
	ExpectHand( MAX_CLIENTS, MAX_CLIENTS, -1, qtrue );
	ExpectHand( MAX_CLIENTS, 1000, -1, qtrue );
	ExpectHand( MAX_CLIENTS, INT_MAX, -1, qtrue );

	/* against the retail rule */
	for ( m = 0 ; m < (int)( sizeof( slotCounts ) / sizeof( slotCounts[0] ) ) ; m++ ) {
		maxclients = slotCounts[m];
		for ( f = 0 ; f < 3 ; f++ ) {
			for ( s = 0 ; s < (int)( sizeof( states ) / sizeof( states[0] ) ) ; s++ ) {
				for ( t = 0 ; t < (int)( sizeof( targets ) / sizeof( targets[0] ) ) ; t++ ) {
					Com_sprintf( currentCase, sizeof( currentCase ),
						"SpectatorClientEndFrame: %d slots, follow1/follow2 set %d, spectatorState %d, spectatorClient %d",
						maxclients, f, states[s], targets[t] );
					/* nobody playing; the first two players; follow1 only */
					if ( f == 0 ) {
						SetupServer( maxclients, -1, -1 );
					} else if ( f == 1 ) {
						SetupServer( maxclients, 0, maxclients > 4 ? 4 : -1 );
					} else {
						SetupServer( maxclients, 0, -1 );
					}
					cl = &g_clients[SPECTATOR_SLOT];
					cl->sess.spectatorState = (spectatorState_t)states[s];
					cl->sess.spectatorClient = targets[t];
					ExpectEndFrame( SPECTATOR_SLOT );
				}
			}
		}
	}
}

int main( void ) {
	g_clients = calloc( MAX_CLIENTS, sizeof( *g_clients ) );
	if ( !g_clients ) {
		Fail( "out of memory" );
	}

	CheckOutOfRange();
	CheckWriteRoundTrip();
	CheckInitRoundTrip();
	CheckEndFrame();

	free( g_clients );
	printf( "session data regression passed: %d checks\n", checkCount );
	return 0;
}
