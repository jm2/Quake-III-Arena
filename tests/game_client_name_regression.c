/*
 * Issue #362: SanitizeString (code/game/g_cmds.c) lowercases a name and drops
 * control characters so ClientNumberFromString can match "follow <name>"
 * against the connected players. It drops an ESC (27) together with the byte
 * after it, the old color code, by stepping two bytes. When the ESC was the
 * last character that step went over the terminator, and the function kept
 * reading and copying whatever came next.
 *
 * Both of ClientNumberFromString's strings can end in ESC. The client's
 * argument can carry one inside quotes, and ClientCleanName keeps control
 * characters in a netname. ClientCleanName also writes only the new name and
 * its terminator, so after a rename the bytes of the older, longer name are
 * still in pers.netname behind it. On master a player renamed to "Trail<ESC>"
 * was matched as "trail" plus those stale bytes and could not be followed by
 * name, and a 35-character netname ending in ESC was read into the next
 * clientPersistant_t field.
 *
 * The fix stops at the terminator when the ESC is the last character. This
 * test links the real g_cmds.c, the way the monolithic static build links the
 * game module, and stubs only the engine and game symbols the file references,
 * as tests/game_command_order_regression.c does. It calls
 *   - SanitizeString on exact-size heap strings ending in ESC and '^' (ASan
 *     aborts on master), and on buffers with bytes left after the terminator
 *     (a plain compare fails on master, without a sanitizer too);
 *   - ClientNumberFromString with exact-size heap arguments against colored,
 *     escaped, renamed and full-length netnames;
 *   - "follow <name>" through the real ClientCommand and Cmd_Follow_f.
 * Every other name must match exactly as in retail 1.32c: case-insensitive,
 * ESC plus the next byte and other control characters dropped, and '^' color
 * codes kept as part of the name (ioquake3's Q_CleanStr strips those instead,
 * which would change which players a name matches).
 */
#include "../code/game/g_local.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* g_cmds.c defines these without a prototype in g_local.h */
void SanitizeString( char *in, char *out );
int ClientNumberFromString( gentity_t *to, char *s );

#define TEST_MAXCLIENTS	8

enum {
	SLOT_SENDER,		/* the spectator who sends the commands */
	SLOT_COLORED,		/* "^1Bob^7" */
	SLOT_ESCAPED,		/* ESC color code in front of "EscName" */
	SLOT_GONE,			/* disconnected */
	SLOT_RENAMED,		/* "Trail<ESC>" over an older, longer name */
	SLOT_PLAIN,			/* "Plain" */
	SLOT_FULL,			/* 34 characters and an ESC: the longest netname */
	SLOT_CONNECTING		/* not in game yet */
};

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

static int	checks;

static void Fail( const char *message ) {
	fprintf( stderr, "client name regression failed: %s\n", message );
	exit( 1 );
}

/* --- the command being executed --- */
static int		testArgc;
static const char	*testArgv[2];

int trap_Argc( void ) {
	return testArgc;
}

/* Cmd_ArgvBuffer copies with Q_strncpyz, as here. */
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

/* --- server commands the code produced --- */
#define MAX_SENT		4
#define MAX_SENT_CHARS	( MAX_STRING_CHARS * 2 )

static int	sentCount;
static int	sentClient[MAX_SENT];
static char	sentText[MAX_SENT][MAX_SENT_CHARS];

void trap_SendServerCommand( int clientNum, const char *text ) {
	if ( sentCount >= MAX_SENT ) {
		Fail( "too many server commands for one lookup" );
	}
	if ( strlen( text ) >= MAX_SENT_CHARS ) {
		Fail( "server command too long for the test buffer" );
	}
	sentClient[sentCount] = clientNum;
	strcpy( sentText[sentCount], text );
	sentCount++;
}

/* --- symbols only other commands reach --- */
static void Unexpected( const char *name ) {
	fprintf( stderr, "client name regression failed: unexpected call to %s\n", name );
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

/* Printable form of a test string for failure messages, cut after 200 bytes. */
static const char *Show( const char *s ) {
	static char	buffers[4][200 * 4 + 8];
	static int	which;
	char		*out;
	int			len;
	int			count;

	out = buffers[which];
	which = ( which + 1 ) % 4;
	len = 0;
	for ( count = 0 ; s[count] && count < 200 ; count++ ) {
		unsigned char c = (unsigned char)s[count];
		if ( c < 32 || c >= 127 || c == '\\' ) {
			len += sprintf( out + len, "\\x%02x", c );
		} else {
			out[len++] = (char)c;
		}
	}
	if ( s[count] ) {
		strcpy( out + len, "..." );
	} else {
		out[len] = 0;
	}
	return out;
}

/* An exact-size heap copy: one byte past the terminator is out of bounds. */
static char *HeapString( const char *s ) {
	size_t	size;
	char	*copy;

	size = strlen( s ) + 1;
	copy = malloc( size );
	if ( !copy ) {
		Fail( "out of memory" );
	}
	memcpy( copy, s, size );
	return copy;
}

/* A string of count copies of c followed by tail. */
static char *Repeat( char c, int count, const char *tail ) {
	size_t	tailSize;
	char	*s;

	tailSize = strlen( tail ) + 1;
	s = malloc( count + tailSize );
	if ( !s ) {
		Fail( "out of memory" );
	}
	memset( s, c, count );
	memcpy( s + count, tail, tailSize );
	return s;
}

/*
 * SanitizeString: in, the retail result. Every case with a trailing ESC read
 * past its terminator on master.
 */
typedef struct {
	const char	*in;
	const char	*want;
} sanitizeCase_t;

static const sanitizeCase_t sanitizeCases[] = {
	/* ordinary names, lowercased */
	{ "Plain", "plain" },
	{ "BoB", "bob" },
	{ "Sarge ", "sarge " },
	{ "", "" },
	/* '^' color codes are kept, a trailing '^' too */
	{ "^1Bob^7", "^1bob^7" },
	{ "^1B^2o^3B", "^1b^2o^3b" },
	{ "Bob^", "bob^" },
	{ "^", "^" },
	{ "^^", "^^" },
	/* control characters are dropped, DEL is kept */
	{ "B" "\x01" "o" "\x1f" "b", "bob" },
	{ "Bob" "\x7f", "bob" "\x7f" },
	/* a byte of 0x80 or more is negative with the target's signed char
	 * (the runner passes -fsigned-char), so "*in < 32" drops it too */
	{ "B" "\xe9" "ob", "bob" },
	/* ESC and the byte after it are dropped */
	{ "\x1b" "1Bob", "bob" },
	{ "B" "\x1b" "7oB", "bob" },
	{ "\x1b" "^Bob", "bob" },
	{ "\x1b" "\x1b" "Bob", "bob" },
	{ "Bob" "\x1b" "\x1b", "bob" },
	{ "^1Bob" "\x1b" "^7", "^1bob7" },
	/* an ESC as the last character */
	{ "\x1b", "" },
	{ "Bob" "\x1b", "bob" },
	{ "Bob" "\x1b" "\x1b" "\x1b", "bob" },
	{ "\x1b" "1Bob" "\x1b", "bob" },
	{ "Bob^" "\x1b", "bob^" },
	{ "^1Bob^7" "\x1b", "^1bob^7" },
	{ "B" "\x01" "ob" "\x1b", "bob" }
};
#define NUM_SANITIZE_CASES	( (int)( sizeof( sanitizeCases ) / sizeof( sanitizeCases[0] ) ) )

#define OUT_PATTERN	0x55

static void CheckSanitized( const sanitizeCase_t *tc, const char *how, const char *out, int outSize ) {
	char	message[MAX_SENT_CHARS];
	int		used;
	int		i;

	if ( strcmp( out, tc->want ) ) {
		snprintf( message, sizeof( message ), "SanitizeString(\"%s\") %s gave \"%s\"; want \"%s\"",
			Show( tc->in ), how, Show( out ), Show( tc->want ) );
		Fail( message );
	}
	/* nothing written after the terminator */
	used = (int)strlen( tc->want ) + 1;
	for ( i = used ; i < outSize ; i++ ) {
		if ( (unsigned char)out[i] != OUT_PATTERN ) {
			snprintf( message, sizeof( message ), "SanitizeString(\"%s\") %s wrote output byte %d past its terminator",
				Show( tc->in ), how, i );
			Fail( message );
		}
	}
	checks++;
}

static void Test_SanitizeString( void ) {
	static const char	stale[] = "Stale";
	const sanitizeCase_t	*tc;
	char	*in;
	char	*out;
	char	buffer[64];
	char	result[64];
	size_t	size;
	int		i;

	for ( i = 0 ; i < NUM_SANITIZE_CASES ; i++ ) {
		tc = &sanitizeCases[i];
		size = strlen( tc->in ) + 1;

		/* exact-size heap input and output (the result is never longer) */
		in = HeapString( tc->in );
		out = malloc( size );
		if ( !out ) {
			Fail( "out of memory" );
		}
		memset( out, OUT_PATTERN, size );
		SanitizeString( in, out );
		if ( memcmp( in, tc->in, size ) ) {
			Fail( "SanitizeString changed its input" );
		}
		CheckSanitized( tc, "on a heap string", out, (int)size );
		free( out );
		free( in );

		/* the same string followed by an older, longer one, as a netname is
		 * after ClientCleanName writes a shorter name over it */
		if ( size + sizeof( stale ) > sizeof( buffer ) ) {
			Fail( "sanitize case too long for the test buffer" );
		}
		memcpy( buffer, tc->in, size );
		memcpy( buffer + size, stale, sizeof( stale ) );
		memset( result, OUT_PATTERN, sizeof( result ) );
		SanitizeString( buffer, result );
		CheckSanitized( tc, "with bytes after its terminator", result, (int)sizeof( result ) );
	}
}

/* 34 characters: with a trailing ESC, the longest name a netname holds. */
#define FULL_NAME_CHARS	( MAX_NETNAME - 2 )

static void SetClient( int slot, clientConnected_t connected, team_t team, const char *previousName, const char *name ) {
	gclient_t	*cl;
	char		message[128];

	cl = &g_clients[slot];
	if ( strlen( name ) >= sizeof( cl->pers.netname ) ) {
		snprintf( message, sizeof( message ), "netname for slot %d too long", slot );
		Fail( message );
	}
	g_entities[slot].s.number = slot;
	g_entities[slot].client = cl;
	g_entities[slot].inuse = connected != CON_DISCONNECTED;
	cl->ps.clientNum = slot;
	cl->pers.connected = connected;
	cl->pers.maxHealth = 100;
	cl->sess.sessionTeam = team;
	cl->sess.spectatorState = SPECTATOR_NOT;
	/* ClientCleanName writes the new name and its terminator only */
	if ( previousName ) {
		Q_strncpyz( cl->pers.netname, previousName, sizeof( cl->pers.netname ) );
	}
	memcpy( cl->pers.netname, name, strlen( name ) + 1 );
}

static void SetupServer( void ) {
	char	*fullName;

	memset( &level, 0, sizeof( level ) );
	memset( g_entities, 0, sizeof( g_entities ) );
	memset( g_clients, 0, sizeof( g_clients ) );

	g_gametype.integer = GT_FFA;
	level.maxclients = TEST_MAXCLIENTS;
	level.clients = g_clients;
	level.num_entities = MAX_CLIENTS;

	fullName = Repeat( 'x', FULL_NAME_CHARS, "\x1b" );
	SetClient( SLOT_SENDER, CON_CONNECTED, TEAM_SPECTATOR, NULL, "Watcher" );
	SetClient( SLOT_COLORED, CON_CONNECTED, TEAM_FREE, NULL, "^1Bob^7" );
	SetClient( SLOT_ESCAPED, CON_CONNECTED, TEAM_FREE, NULL, "\x1b" "2EscName" );
	SetClient( SLOT_GONE, CON_DISCONNECTED, TEAM_FREE, NULL, "Ghost" );
	SetClient( SLOT_RENAMED, CON_CONNECTED, TEAM_FREE, "TrailingStale", "Trail" "\x1b" );
	SetClient( SLOT_PLAIN, CON_CONNECTED, TEAM_FREE, NULL, "Plain" );
	SetClient( SLOT_FULL, CON_CONNECTED, TEAM_FREE, NULL, fullName );
	SetClient( SLOT_CONNECTING, CON_CONNECTING, TEAM_FREE, NULL, "Sarge" );
	free( fullName );

	g_clients[SLOT_SENDER].sess.spectatorState = SPECTATOR_FREE;
	g_clients[SLOT_SENDER].sess.spectatorClient = SLOT_SENDER;
}

/* The message ClientNumberFromString sends when nothing matches. */
static void NotFoundText( char *out, int outSize, const char *arg ) {
	char	message[MAX_SENT_CHARS];
	int		idnum;

	if ( arg[0] >= '0' && arg[0] <= '9' ) {
		idnum = atoi( arg );
		if ( idnum < 0 || idnum >= TEST_MAXCLIENTS ) {
			snprintf( out, outSize, "print \"Bad client slot: %i\n\"", idnum );
		} else {
			snprintf( out, outSize, "print \"Client %i is not active\n\"", idnum );
		}
		return;
	}
	if ( strlen( arg ) + 64 > (size_t)outSize ) {
		snprintf( message, sizeof( message ), "argument \"%s\" too long for the test buffer", Show( arg ) );
		Fail( message );
	}
	snprintf( out, outSize, "print \"User %s is not on the server\n\"", arg );
}

static void CheckReply( const char *what, const char *arg, int want, int got ) {
	char	expect[MAX_SENT_CHARS];
	char	message[MAX_SENT_CHARS * 2];

	if ( got != want ) {
		snprintf( message, sizeof( message ), "%s \"%s\" found slot %d; want %d",
			what, Show( arg ), got, want );
		Fail( message );
	}
	if ( want >= 0 ) {
		if ( sentCount != 0 ) {
			snprintf( message, sizeof( message ), "%s \"%s\" found slot %d but also sent \"%s\"",
				what, Show( arg ), got, Show( sentText[0] ) );
			Fail( message );
		}
		return;
	}
	NotFoundText( expect, sizeof( expect ), arg );
	if ( sentCount != 1 || sentClient[0] != SLOT_SENDER || strcmp( sentText[0], expect ) ) {
		snprintf( message, sizeof( message ), "%s \"%s\" sent %d command(s), first to %d: \"%s\"; want \"%s\" to %d",
			what, Show( arg ), sentCount, sentCount ? sentClient[0] : -1,
			sentCount ? Show( sentText[0] ) : "", Show( expect ), SLOT_SENDER );
		Fail( message );
	}
}

typedef struct {
	const char	*arg;
	int			want;
} lookupCase_t;

static const lookupCase_t lookupCases[] = {
	/* ordinary names: whole name, any case */
	{ "Plain", SLOT_PLAIN },
	{ "PLAIN", SLOT_PLAIN },
	{ "plain", SLOT_PLAIN },
	{ "Pla", -1 },
	{ "Watcher", SLOT_SENDER },
	/* '^' colored names match with their color codes, as in retail */
	{ "^1Bob^7", SLOT_COLORED },
	{ "^1BOB^7", SLOT_COLORED },
	{ "Bob", -1 },
	{ "^1Bob", -1 },
	{ "Plain^", -1 },
	{ "^", -1 },
	/* control characters and ESC color codes in either string are dropped */
	{ "EscName", SLOT_ESCAPED },
	{ "\x1b" "5escname", SLOT_ESCAPED },
	{ "P" "\x01" "lain", SLOT_PLAIN },
	{ "Pl" "\xe9" "ain", SLOT_PLAIN },
	{ "\x1b" "1Plain", SLOT_PLAIN },
	/* a trailing ESC in the argument */
	{ "Plain" "\x1b", SLOT_PLAIN },
	{ "^1Bob^7" "\x1b", SLOT_COLORED },
	{ "\x1b" "2EscName" "\x1b", SLOT_ESCAPED },
	{ "Bob" "\x1b", -1 },
	{ "Plain^" "\x1b", -1 },
	{ "\x1b", -1 },
	/* a trailing ESC in the netname, after a rename from a longer name */
	{ "Trail", SLOT_RENAMED },
	{ "trail" "\x1b", SLOT_RENAMED },
	{ "TrailingStale", -1 },
	{ "trailgstale", -1 },
	/* clients not in the game */
	{ "Ghost", -1 },
	{ "Sarge", -1 },
	/* slot numbers (unchanged path) */
	{ "5", SLOT_PLAIN },
	{ "0", SLOT_SENDER },
	{ "3", -1 },
	{ "8", -1 }
};
#define NUM_LOOKUP_CASES	( (int)( sizeof( lookupCases ) / sizeof( lookupCases[0] ) ) )

static void ExpectLookup( const char *argText, int want ) {
	char	*arg;

	arg = HeapString( argText );
	sentCount = 0;
	CheckReply( "ClientNumberFromString", argText,
		want, ClientNumberFromString( &g_entities[SLOT_SENDER], arg ) );
	if ( strcmp( arg, argText ) ) {
		Fail( "ClientNumberFromString changed its argument" );
	}
	free( arg );
	checks++;
}

static void Test_ClientNumberFromString( void ) {
	char	*arg;
	int		i;

	SetupServer();
	for ( i = 0 ; i < NUM_LOOKUP_CASES ; i++ ) {
		ExpectLookup( lookupCases[i].arg, lookupCases[i].want );
	}

	/* the full-length netname: its ESC is the byte before the array's
	 * last byte, so master read on into pers.maxHealth */
	arg = Repeat( 'X', FULL_NAME_CHARS, "" );
	ExpectLookup( arg, SLOT_FULL );
	free( arg );
	arg = Repeat( 'x', FULL_NAME_CHARS, "\x1b" );
	ExpectLookup( arg, SLOT_FULL );
	free( arg );
	arg = Repeat( 'x', FULL_NAME_CHARS - 1, "" );
	ExpectLookup( arg, -1 );
	free( arg );
}

/* "follow <arg>" from the spectator through ClientCommand. */
static void ExpectFollow( const char *arg, int want ) {
	gclient_t	*sender;
	int			got;

	SetupServer();
	sender = &g_clients[SLOT_SENDER];

	testArgc = 2;
	testArgv[0] = "follow";
	testArgv[1] = arg;
	sentCount = 0;
	ClientCommand( SLOT_SENDER );

	if ( sender->sess.spectatorState == SPECTATOR_FOLLOW ) {
		got = sender->sess.spectatorClient;
	} else if ( sender->sess.spectatorState == SPECTATOR_FREE && sender->sess.spectatorClient == SLOT_SENDER ) {
		got = -1;
	} else {
		Fail( "follow left the spectator in an unexpected state" );
		return;
	}
	if ( sender->sess.sessionTeam != TEAM_SPECTATOR ) {
		Fail( "follow changed the spectator's team" );
	}
	CheckReply( "follow", arg, want, got );
	checks++;
}

static void Test_Follow( void ) {
	char	*arg;

	ExpectFollow( "Plain", SLOT_PLAIN );
	ExpectFollow( "^1bob^7", SLOT_COLORED );
	ExpectFollow( "bob", -1 );
	ExpectFollow( "EscName" "\x1b", SLOT_ESCAPED );
	ExpectFollow( "Trail", SLOT_RENAMED );
	ExpectFollow( "Trail" "\x1b", SLOT_RENAMED );

	arg = Repeat( 'x', FULL_NAME_CHARS, "" );
	ExpectFollow( arg, SLOT_FULL );
	free( arg );

	/*
	 * Arguments that fill Cmd_Follow_f's MAX_TOKEN_CHARS buffer and end in
	 * ESC: master read the byte after the buffer. (A network "follow" is
	 * shorter, as the whole command line is at most MAX_STRING_CHARS - 1
	 * bytes; this pins the buffer end.)
	 */
	arg = Repeat( 'a', MAX_TOKEN_CHARS - 2, "\x1b" );
	ExpectFollow( arg, -1 );
	free( arg );
	arg = Repeat( '\x01', MAX_TOKEN_CHARS - 7, "Plain" "\x1b" );
	ExpectFollow( arg, SLOT_PLAIN );
	free( arg );
}

int main( void ) {
	Test_SanitizeString();
	Test_ClientNumberFromString();
	Test_Follow();
	printf( "client name regression passed (%d checks).\n", checks );
	return 0;
}
