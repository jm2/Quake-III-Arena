/* Issue #257: getstatus/getinfo must not echo an unbounded challenge. */
#include "../code/server/sv_main.c"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#define RETAIL_SERVERINFO "\\sv_hostname\\Mac OS 9 test server\\sv_maxclients\\64" \
	"\\g_gametype\\0\\protocol\\68\\mapname\\q3dm17\\sv_keywords\\ffa rail" \
	"\\version\\Q3 1.32c ppc-mac\\timelimit\\15\\fraglimit\\50\\sv_floodProtect\\1" \
	"\\sv_maxRate\\25000\\dmflags\\0\\g_needpass\\0\\capturelimit\\8\\sv_privateClients\\0"
#define PEER_CHALLENGE_LIMIT 128	/* ioquake3 and Quake3e ignore longer challenges */

static cvar_t maxclients, privateClients, hostname, mapname, gametype, pure, minPing, maxPing;
static client_t clients[MAX_CLIENTS];
static playerState_t players[MAX_CLIENTS];
static const char *command, *challenge;
static char reply[MAX_MSGLEN];
static int replies, prints;

/** Fail with the violated connectionless-reply property. */
static void Check( int ok, const char *message ) {
	if ( !ok ) { fprintf( stderr, "Server connectionless regression failed: %s\n", message ); exit( 1 ); }
}
/** A server-side ERR_DROP here is the shutdown the issue reports. */
void QDECL Com_Error( int level, const char *format, ... ) {
	(void)level; (void)format; Check( 0, "connectionless query raised Com_Error" );
}
/** Count console output; a spoofed query must not print on every packet. */
void QDECL Com_Printf( const char *format, ... ) { (void)format; prints++; }
/** Developer-only diagnostics are allowed for dropped queries. */
void QDECL Com_DPrintf( const char *format, ... ) { (void)format; }
/** Return the tokenized "<command> <challenge>" packet. */
char *Cmd_Argv( int arg ) { return (char *)( arg == 0 ? command : arg == 1 ? challenge : "" ); }
/** FFA multiplayer, unrestricted, not a single-player session. */
float Cvar_VariableValue( const char *name ) { (void)name; return 0; }
char *Cvar_VariableString( const char *name ) { (void)name; return ""; }
char *Cvar_InfoString( int bit ) { static char info[MAX_INFO_STRING]; (void)bit; strcpy( info, RETAIL_SERVERINFO ); return info; }
playerState_t *SV_GameClientNum( int num ) { return &players[num]; }
const char *NET_AdrToString( netadr_t a ) { (void)a; return "192.0.2.1:27960"; }
/** Capture the datagram exactly as NET_OutOfBandPrint formats it. */
void QDECL NET_OutOfBandPrint( netsrc_t sock, netadr_t adr, const char *format, ... ) {
	va_list argptr;
	(void)sock; (void)adr;
	memset( reply, 0xff, 4 );
	va_start( argptr, format ); Q_vsnprintf( reply + 4, sizeof( reply ) - 4, format, argptr ); va_end( argptr );
	reply[sizeof( reply ) - 1] = 0;
	replies++;
}
/** Run one query and report whether a reply datagram was produced. */
static int Query( const char *cmd, const char *arg ) {
	netadr_t from;
	memset( &from, 0, sizeof( from ) ); from.type = NA_IP;
	command = cmd; challenge = arg; replies = 0; prints = 0; reply[4] = 0;
	if ( !strcmp( cmd, "getstatus" ) ) SVC_Status( from ); else SVC_Info( from );
	Check( replies <= 1, "more than one reply datagram" );
	return replies;
}
/** Return the echoed challenge from the first infostring line of the reply. */
static const char *Echo( const char *header ) {
	static char info[MAX_INFO_STRING];
	const char *body = reply + 4;
	Check( !strncmp( body, header, strlen( header ) ), "reply header" );
	body += strlen( header );
	Check( strcspn( body, "\n" ) < sizeof( info ), "infostring bounded" );
	Q_strncpyz( info, body, (int)strcspn( body, "\n" ) + 1 );
	return Info_ValueForKey( info, "challenge" );
}
/** Accept 128-byte challenges, reject longer ones silently, keep full-server replies bounded. */
int main( void ) {
	char longest[MAX_STRING_CHARS], name[MAX_NAME_LENGTH];
	int i, lines;
	const char *p;
	sv_maxclients = &maxclients; sv_privateClients = &privateClients; sv_hostname = &hostname;
	sv_mapname = &mapname; sv_gametype = &gametype; sv_pure = &pure; sv_minPing = &minPing; sv_maxPing = &maxPing;
	maxclients.integer = MAX_CLIENTS; hostname.string = "Mac OS 9 test server"; mapname.string = "q3dm17";
	pure.integer = 1; svs.clients = clients;
	memset( name, 'N', sizeof( name ) - 1 ); name[sizeof( name ) - 1] = 0;
	for ( i = 0; i < MAX_CLIENTS; i++ ) {
		clients[i].state = CS_ACTIVE; clients[i].ping = 999; Q_strncpyz( clients[i].name, name, sizeof( clients[i].name ) );
		players[i].persistant[PERS_SCORE] = -999;
	}
	/* The longest line MSG_ReadStringLine hands to "getstatus <challenge>". */
	memset( longest, 'c', sizeof( longest ) ); longest[sizeof( longest ) - 1 - strlen( "getstatus " )] = 0;
	Check( strlen( longest ) > 1000, "maximum-length challenge" );
	Check( Query( "getstatus", longest ) == 0 && prints == 0, "maximum-length getstatus challenge was answered" );
	Check( Query( "getinfo", longest ) == 0 && prints == 0, "maximum-length getinfo challenge was answered" );
	longest[PEER_CHALLENGE_LIMIT + 1] = 0;
	Check( Query( "getstatus", longest ) == 0, "129-byte getstatus challenge was answered" );
	Check( Query( "getinfo", longest ) == 0, "129-byte getinfo challenge was answered" );
	longest[PEER_CHALLENGE_LIMIT] = 0;
	Check( Query( "getstatus", longest ) == 1 && prints == 0, "128-byte getstatus challenge was ignored" );
	Check( !strcmp( Echo( "statusResponse\n" ), longest ), "128-byte getstatus challenge not echoed intact" );
	for ( lines = 0, p = strchr( reply + 4, '\n' ) + 1; ( p = strchr( p, '\n' ) ) != NULL; p++ ) lines++;
	Check( lines == MAX_CLIENTS + 1, "full-server status lists every player" );
	/* A full server's status legitimately exceeds the old 1400-byte Mac send limit. */
	Check( strlen( reply ) > 1400 && strlen( reply ) < MAX_MSGLEN, "full-server status size" );
	Check( Query( "getinfo", longest ) == 1 && !strcmp( Echo( "infoResponse\n" ), longest ),
	       "128-byte getinfo challenge not echoed intact" );
	/* Retail 1.32c clients send "getinfo xxx" and a bare "getstatus". */
	Check( Query( "getinfo", "xxx" ) == 1 && !strcmp( Echo( "infoResponse\n" ), "xxx" ), "retail getinfo" );
	Check( Query( "getstatus", "" ) == 1 && !*Echo( "statusResponse\n" ), "retail getstatus" );
	puts( "Server connectionless challenge regressions passed (issue #257)" );
	return 0;
}
