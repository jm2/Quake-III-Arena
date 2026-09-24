/* Issue #273: clients must not remove or forge the server-maintained "ip" userinfo key.
 * Issue #348: a client the game drops while connecting is refused once per challenge, without a misleading reason. */
#include "../code/server/sv_client.c"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#define CHALLENGE 1234
#define BANNED_HANDLE 1
#define UPSTREAM_REJECT "print\nUserinfo string length exceeded.  Try removing setu cvars from your config.\n"
#define BANNED_REPLY "print\nYou are banned from this server.\n"

/* The real baseq3 packet filter and addip command from code/game/g_svcmds.c. */
qboolean G_FilterPacket( char *from );
void Svcmd_AddIP_f( void );
vmCvar_t g_filterBan;

serverStatic_t svs;
vm_t *gvm;
static cvar_t maxclients, reconnectlimit, privatePassword, privateClients, minPing, maxPing, lanForceRate, dedicated;
cvar_t *sv_maxclients = &maxclients, *sv_reconnectlimit = &reconnectlimit, *sv_privatePassword = &privatePassword;
cvar_t *sv_privateClients = &privateClients, *sv_minPing = &minPing, *sv_maxPing = &maxPing;
cvar_t *sv_lanForceRate = &lanForceRate, *com_dedicated = &dedicated;
static client_t clients[4];
static sharedEntity_t entities[4];
static const char *argument, *addipArgument;
static char reply[MAX_MSGLEN], gameIP[MAX_INFO_STRING], dropCommand[MAX_STRING_CHARS];
static int connects, userinfoChanges, disconnects, gameFillsUserinfo, gameDropsClient;

/** Fail with the violated "ip" key property. */
static void Check( int ok, const char *message ) {
	if ( !ok ) { fprintf( stderr, "Server userinfo ip regression failed: %s\n", message ); exit( 1 ); }
}
void QDECL Com_Error( int level, const char *format, ... ) { (void)level; (void)format; Check( 0, "unexpected Com_Error" ); }
void QDECL Com_Printf( const char *format, ... ) { (void)format; }
void QDECL Com_DPrintf( const char *format, ... ) { (void)format; }
void QDECL G_Printf( const char *format, ... ) { (void)format; }
/** Cmd_Argv(1) is the connect packet's or the userinfo command's info string. */
char *Cmd_Argv( int arg ) { return (char *)( arg == 1 ? argument : "" ); }
int trap_Argc( void ) { return 2; }
void trap_Argv( int n, char *buffer, int bufferLength ) { Q_strncpyz( buffer, n == 1 ? addipArgument : "addip", bufferLength ); }
void trap_Cvar_Set( const char *name, const char *value ) { (void)name; (void)value; }
/** Mirror code/qcommon/net_chan.c for the address types used here. */
qboolean NET_IsLocalAddress( netadr_t adr ) { return adr.type == NA_LOOPBACK; }
qboolean NET_CompareBaseAdr( netadr_t a, netadr_t b ) { return a.type == b.type && ( a.type == NA_LOOPBACK || !memcmp( a.ip, b.ip, 4 ) ); }
qboolean NET_CompareAdr( netadr_t a, netadr_t b ) { return NET_CompareBaseAdr( a, b ) && a.port == b.port; }
const char *NET_AdrToString( netadr_t a ) {
	static char s[64];
	if ( a.type == NA_LOOPBACK ) return "loopback";
	Com_sprintf( s, sizeof( s ), "%i.%i.%i.%i:%hu", a.ip[0], a.ip[1], a.ip[2], a.ip[3], BigShort( a.port ) );
	return s;
}
qboolean Sys_IsLANAddress( netadr_t adr ) { (void)adr; return qfalse; }
/* SV_GetChallenge: not single player, and no authorize server (NA_BAD), so it answers once AUTHORIZE_TIMEOUT has passed. */
float Cvar_VariableValue( const char *name ) { (void)name; return 0; }
void Netchan_Setup( netsrc_t sock, netchan_t *chan, netadr_t adr, int qport ) {
	memset( chan, 0, sizeof( *chan ) ); chan->sock = sock; chan->remoteAddress = adr; chan->qport = qport; chan->outgoingSequence = 1;
}
/* With PR #312, SV_DropClient and reconnects free the client's netchan queue; nothing is queued here. */
void SV_Netchan_FreeQueue( client_t *client ) { (void)client; }
/** Capture the connectionless reply text. */
void QDECL NET_OutOfBandPrint( netsrc_t sock, netadr_t adr, const char *format, ... ) {
	va_list argptr;
	(void)sock; (void)adr;
	va_start( argptr, format ); Q_vsnprintf( reply, sizeof( reply ), format, argptr ); va_end( argptr );
}
/** Record the reliable disconnect command SV_DropClient queues. */
void QDECL SV_SendServerCommand( client_t *cl, const char *format, ... ) {
	va_list argptr;
	if ( !cl ) return;
	va_start( argptr, format ); Q_vsnprintf( dropCommand, sizeof( dropCommand ), format, argptr ); va_end( argptr );
}
void SV_SetUserinfo( int index, const char *val ) { Q_strncpyz( svs.clients[index].userinfo, val, sizeof( svs.clients[index].userinfo ) ); }
sharedEntity_t *SV_GentityNum( int num ) { return &entities[num]; }
void SV_Heartbeat_f( void ) {}
void SV_BotFreeClient( int clientNum ) { (void)clientNum; Check( 0, "bot freed" ); }
void FS_FCloseFile( fileHandle_t f ) { (void)f; }
void Z_Free( void *ptr ) { (void)ptr; }
void Com_Memset( void *dest, const int val, const size_t count ) { memset( dest, val, count ); }
/* The sanitizer's global registration keeps the ucmds table, so its other handlers must link. */
server_t sv;
cvar_t *sv_pure;
static void Unreachable( void ) { Check( 0, "unrelated client command handler reached" ); }
int Cmd_Argc( void ) { Unreachable(); return 0; }
void Cmd_TokenizeString( const char *text ) { (void)text; Unreachable(); }
int FS_FileIsInPAK( const char *filename, int *pChecksum ) { (void)filename; (void)pChecksum; Unreachable(); return -1; }
cvar_t *sv_strictAuth;
qboolean NET_StringToAdr( const char *s, netadr_t *a ) { (void)s; (void)a; Unreachable(); return qfalse; }
cvar_t *Cvar_Get( const char *name, const char *value, int flags ) { (void)name; (void)value; (void)flags; Unreachable(); return NULL; }
const char *FS_LoadedPakPureChecksums( void ) { Unreachable(); return ""; }
void MSG_Init( msg_t *buf, byte *data, int length ) { (void)buf; (void)data; (void)length; Unreachable(); }
void MSG_WriteByte( msg_t *sb, int c ) { (void)sb; (void)c; Unreachable(); }
void MSG_WriteShort( msg_t *sb, int c ) { (void)sb; (void)c; Unreachable(); }
void MSG_WriteLong( msg_t *sb, int c ) { (void)sb; (void)c; Unreachable(); }
void MSG_WriteBigString( msg_t *sb, const char *s ) { (void)sb; (void)s; Unreachable(); }
void MSG_WriteDeltaEntity( msg_t *msg, struct entityState_s *from, struct entityState_s *to, qboolean force ) {
	(void)msg; (void)from; (void)to; (void)force; Unreachable();
}
void SV_UpdateServerCommandsToClient( client_t *client, msg_t *msg ) { (void)client; (void)msg; Unreachable(); }
void SV_SendMessageToClient( msg_t *msg, client_t *client ) { (void)msg; (void)client; Unreachable(); }
void SV_SendClientSnapshot( client_t *client ) { (void)client; Unreachable(); }
/** The game side: ClientConnect's ban check (g_client.c) on the stored userinfo, as trap_GetUserinfo returns it. */
int VM_CallArgs( vm_t *vm, int callNum, const int *args, int argCount ) {
	char *ip;
	(void)vm; (void)argCount;
	if ( callNum == GAME_CLIENT_CONNECT ) {
		connects++;
		ip = Info_ValueForKey( svs.clients[args[0]].userinfo, "ip" );
		Q_strncpyz( gameIP, ip, sizeof( gameIP ) );
		if ( G_FilterPacket( ip ) ) return BANNED_HANDLE;
		if ( gameDropsClient ) {	/* a mod's trap_DropClient (SV_GameDropClient) inside ClientConnect */
			SV_DropClient( &svs.clients[args[0]], "Cheater detected" );
			return 0;
		}
		if ( gameFillsUserinfo ) {	/* a mod's trap_SetUserinfo leaving no room for "ip" */
			memset( svs.clients[args[0]].userinfo, 'u', MAX_INFO_STRING - 1 );
			memcpy( svs.clients[args[0]].userinfo, "\\name\\", 6 );
			svs.clients[args[0]].userinfo[MAX_INFO_STRING - 1] = 0;
		}
	} else if ( callNum == GAME_CLIENT_USERINFO_CHANGED ) {
		userinfoChanges++;
	} else if ( callNum == GAME_CLIENT_DISCONNECT ) {
		disconnects++;
	}
	return 0;
}
char *VM_CheckedExplicitString( vm_t *vm, int value, qboolean nullable ) {
	(void)vm; Check( nullable, "nullable connect result" );
	return value == BANNED_HANDLE ? "You are banned from this server." : NULL;
}
static void AddIP( const char *mask ) { addipArgument = mask; Svcmd_AddIP_f(); }
static netadr_t Address( int a, int b, int c, int d ) {
	netadr_t adr;
	memset( &adr, 0, sizeof( adr ) );
	adr.type = NA_IP; adr.ip[0] = a; adr.ip[1] = b; adr.ip[2] = c; adr.ip[3] = d; adr.port = BigShort( 27960 );
	return adr;
}
/** A connect userinfo with an optional leading pair, padded to exactly `length` bytes when nonzero. */
static const char *Userinfo( const char *prefix, size_t length ) {
	static char info[MAX_INFO_STRING];
	size_t used;
	Com_sprintf( info, sizeof( info ), "%s\\protocol\\%i\\challenge\\%i\\qport\\7\\name\\Player\\rate\\25000",
	             prefix, PROTOCOL_VERSION, CHALLENGE );
	if ( length ) {
		Q_strcat( info, sizeof( info ), "\\pad\\" ); used = strlen( info );
		Check( used < length && length < sizeof( info ), "padding length" );
		memset( info + used, 'p', length - used ); info[length] = 0;
	}
	return info;
}
/** Deliver a "connect" packet with a valid challenge for a fresh server. */
static void Connect( netadr_t from, const char *userinfo ) {
	memset( clients, 0, sizeof( clients ) ); memset( &svs.challenges, 0, sizeof( svs.challenges ) );
	svs.challenges[0].adr = from; svs.challenges[0].challenge = CHALLENGE; svs.challenges[0].pingTime = svs.time;
	argument = userinfo; reply[0] = 0; gameIP[0] = 0; dropCommand[0] = 0; connects = userinfoChanges = disconnects = 0;
	SV_DirectConnect( from );
}
/** Deliver another "connect" packet to the running server, as the client resends it every 3 s with the same challenge. */
static void Send( netadr_t from, const char *userinfo ) { argument = userinfo; reply[0] = 0; SV_DirectConnect( from ); }
/** Deliver a "getchallenge" packet, as a client's /connect or /reconnect does. */
static void GetChallenge( netadr_t from ) { reply[0] = 0; SV_GetChallenge( from ); }
/** Run a server frame `msec` later; free zombies like SV_CheckTimeouts in sv_main.c (sv_zombietime 2). */
static void Frame( int msec ) {
	int i;
	svs.time += msec;
	for ( i = 0; i < 4; i++ ) {
		if ( clients[i].state == CS_ZOMBIE && clients[i].lastPacketTime < svs.time - 2000 ) clients[i].state = CS_FREE;
	}
}
/** Deliver a "userinfo" client command to client 0. */
static void Update( const char *userinfo ) { argument = userinfo; SV_UpdateUserinfo_f( &clients[0] ); }
static const char *StoredIP( void ) { return Info_ValueForKey( clients[0].userinfo, "ip" ); }

int main( void ) {
	netadr_t banned = Address( 192, 0, 2, 1 ), player = Address( 198, 51, 100, 23 ), other = Address( 203, 0, 113, 9 ), local;
	netadr_t newcomer = Address( 198, 51, 100, 42 );
	char real[64], update[MAX_INFO_STRING], info[MAX_INFO_STRING];
	size_t limit;
	int i;

	maxclients.integer = 4; reconnectlimit.integer = 3; privatePassword.string = ""; dedicated.integer = 1;
	svs.clients = clients; svs.time = 100000; g_filterBan.integer = 1; svs.authorizeAddress.type = NA_BAD;
	memset( &local, 0, sizeof( local ) ); local.type = NA_LOOPBACK;

	/* Bots have no "ip" key; the runner pattern-fills uninitialised locals (0xFE GCC, 0xAA Clang). */
	AddIP( "254.254.254.254" ); AddIP( "170.170.170.170" );
	Check( !G_FilterPacket( "" ), "G_FilterPacket(\"\") read uninitialised octets" );
	AddIP( "192.0.2.1" );
	Check( G_FilterPacket( "192.0.2.1:27960" ) && !G_FilterPacket( "198.51.100.23:27960" ), "addip filter" );

	/* Connect with a forged "IP" and no room for the real pair: rejected before the game sees it. */
	Q_strncpyz( real, NET_AdrToString( banned ), sizeof( real ) );
	limit = MAX_INFO_STRING - 4 - strlen( real );
	Connect( banned, Userinfo( "\\IP\\198.51.100.7:27960", limit ) );
	Check( !strcmp( reply, UPSTREAM_REJECT ), "near-limit connect not rejected with the upstream message" );
	Check( !connects && clients[0].state == CS_FREE, "near-limit connect reached the game" );
	/* One byte shorter fits: the game sees the real address first and applies the ban. */
	Connect( banned, Userinfo( "\\IP\\198.51.100.7:27960", limit - 1 ) );
	Check( connects == 1 && !strcmp( gameIP, real ) && !strcmp( reply, BANNED_REPLY ), "banned address accepted at connect" );
	Connect( local, Userinfo( "", MAX_INFO_STRING - 4 - strlen( "localhost" ) ) );
	Check( !strcmp( reply, UPSTREAM_REJECT ) && !connects, "near-limit local connect not rejected" );

	/* A connected client cannot replace its "ip" through a later userinfo update. */
	Q_strncpyz( real, NET_AdrToString( player ), sizeof( real ) );
	Connect( player, Userinfo( "", 0 ) );
	Check( !strcmp( reply, "connectResponse" ) && clients[0].state == CS_CONNECTED && !strcmp( gameIP, real ), "unbanned connect" );
	Update( "\\name\\Player\\ip\\localhost" );
	Check( !strcmp( StoredIP(), real ) && userinfoChanges == 1, "client-supplied ip survived an update" );
	Update( "\\IP\\192.0.2.99:27960\\name\\Player" );
	Check( !strcmp( StoredIP(), real ), "client-supplied IP (other case) survived an update" );
	Update( "\\ip\\192.0.2.98:27960\\ip\\192.0.2.99:27960\\name\\Player" );
	Check( !strcmp( StoredIP(), real ), "duplicate client-supplied ip survived an update" );
	/* A stale ip whose replacement still fits exactly (1023 bytes) keeps the client. */
	Com_sprintf( update, sizeof( update ), "\\ip\\localhost\\name\\Player\\pad\\" );
	limit = MAX_INFO_STRING - 1 + strlen( "\\ip\\localhost" ) - 4 - strlen( real );
	memset( update + strlen( update ), 'p', limit - strlen( update ) ); update[limit] = 0;
	Update( update );
	Check( clients[0].state == CS_CONNECTED && !strcmp( StoredIP(), real ) && strlen( clients[0].userinfo ) == MAX_INFO_STRING - 1,
	       "near-limit update with a replaceable ip dropped or not replaced" );
	/* Map change: sv_init.c re-runs ClientConnect(firstTime=qfalse) on the stored userinfo after an addip. */
	AddIP( "198.51.100.23" );
	Check( VM_CheckedExplicitString( gvm, VM_Call( gvm, GAME_CLIENT_CONNECT, 0, qfalse, qfalse ), qtrue ) != NULL
	       && !strcmp( gameIP, real ), "addip ban not applied at map change" );
	/* No room for the real address: drop the client instead of keeping a forged or missing key. */
	userinfoChanges = 0;
	Update( Userinfo( "\\IP\\203.0.113.5:27960", MAX_INFO_STRING - 1 ) );
	Check( clients[0].state == CS_ZOMBIE && disconnects == 1 && !userinfoChanges, "near-limit forged update not dropped" );
	Check( !strcmp( dropCommand, "disconnect \"userinfo string length exceeded\"" ), "drop reason" );
	/* A forged "IP" longer than the real address: ioquake3's strlen arithmetic sees room, but "ip" cannot be added. */
	Q_strncpyz( real, NET_AdrToString( other ), sizeof( real ) );
	Connect( other, Userinfo( "", 0 ) );
	Check( clients[0].state == CS_CONNECTED && !strcmp( StoredIP(), real ), "second unbanned connect" );
	Update( Userinfo( "\\IP\\255.255.255.255:65535", MAX_INFO_STRING - 1 ) );
	Check( clients[0].state == CS_ZOMBIE && disconnects == 1 && !userinfoChanges, "near-limit forged IP longer than the address not dropped" );

	/* Local clients are always "localhost". */
	Connect( local, Userinfo( "", 0 ) );
	Check( clients[0].state == CS_CONNECTED && !strcmp( StoredIP(), "localhost" ), "local connect" );
	Update( "\\ip\\192.0.2.99:27960\\name\\Player" );
	Check( !strcmp( StoredIP(), "localhost" ), "local client ip replaced by client value" );

	/* A game that leaves no room for "ip" in ClientConnect must not leave a connected client without one. */
	gameFillsUserinfo = 1;
	Connect( other, Userinfo( "", 0 ) );
	Check( clients[0].state == CS_ZOMBIE && disconnects == 1, "connected without an ip key" );
	Check( !strcmp( reply, "print\nUserinfo string length exceeded.\n" ), "client dropped at connect not told why" );
	/* Its resent connects must not rerun ClientConnect and the drop (two broadcasts) every 3 s, even once the zombie is
	 * freed; they stay silent so the client keeps showing the reason. */
	for ( i = 0; i < 3; i++ ) {
		Frame( 3000 ); Send( other, Userinfo( "", 0 ) );
		Check( !reply[0] && connects == 1 && disconnects == 1 && clients[0].state == CS_FREE, "overflow refusal retried by a resent connect" );
	}
	/* A new challenge request is a new attempt: refused again while the game still overflows, then let in. */
	GetChallenge( other );
	Check( !strcmp( reply, "challengeResponse 1234" ), "no challenge after a refusal" );
	Send( other, Userinfo( "", 0 ) );
	Check( !strcmp( reply, "print\nUserinfo string length exceeded.\n" ) && connects == 2 && disconnects == 2, "new challenge not tried again" );
	gameFillsUserinfo = 0;
	Frame( 50 ); Send( other, Userinfo( "", 0 ) );
	Check( !reply[0] && connects == 2, "second overflow refusal retried by a resent connect" );
	GetChallenge( other ); Send( other, Userinfo( "", 0 ) );
	Check( !strcmp( reply, "connectResponse" ) && connects == 3 && clients[0].state == CS_CONNECTED && !strcmp( StoredIP(), real ),
	       "fitting connect after a new challenge" );

	/* A game that drops the client in ClientConnect already told everyone why: the client is not sent the overflow
	 * reason or a connectResponse, and its slot and challenge are released like any other drop. */
	Q_strncpyz( real, NET_AdrToString( newcomer ), sizeof( real ) );
	gameDropsClient = 1;
	Connect( newcomer, Userinfo( "", 0 ) );
	Check( connects == 1 && disconnects == 1 && !strcmp( dropCommand, "disconnect \"Cheater detected\"" ), "game drop at connect" );
	Check( !reply[0], "game drop at connect misreported to the client" );
	Check( clients[0].state == CS_ZOMBIE && !clients[0].lastPacketTime && !clients[0].userinfo[0] && !svs.challenges[0].connected,
	       "game drop at connect left the slot or its challenge in use" );
	Frame( 50 );
	Check( clients[0].state == CS_FREE, "slot dropped at connect not freed on the next frame" );
	Frame( 3000 ); Send( newcomer, Userinfo( "", 0 ) );
	Check( !reply[0] && connects == 1 && disconnects == 1 && clients[0].state == CS_FREE, "game drop retried by a resent connect" );
	gameDropsClient = 0;
	GetChallenge( newcomer ); Send( newcomer, Userinfo( "", 0 ) );
	Check( !strcmp( reply, "connectResponse" ) && connects == 2 && clients[0].state == CS_CONNECTED && !strcmp( StoredIP(), real ),
	       "freed slot not reused after a new challenge" );
	/* No challenge to record the refusal on for the local client; the drop itself is still handled. */
	gameDropsClient = 1;
	Connect( local, Userinfo( "", 0 ) );
	Check( !reply[0] && clients[0].state == CS_ZOMBIE && disconnects == 1, "local game drop at connect" );
	gameDropsClient = 0;

	/* The normal connect path is unchanged, and sv_reconnectlimit (3 s) still holds back a connected client's resends. */
	Connect( newcomer, Userinfo( "", 0 ) );
	Check( !strcmp( reply, "connectResponse" ) && clients[0].state == CS_CONNECTED && connects == 1 && !disconnects
	       && clients[0].lastConnectTime == svs.time && clients[0].lastPacketTime == svs.time && svs.challenges[0].connected,
	       "normal connect" );
	Frame( 2999 ); Send( newcomer, Userinfo( "", 0 ) );
	Check( !reply[0] && connects == 1 && clients[0].state == CS_CONNECTED, "reconnect inside sv_reconnectlimit" );
	Frame( 1 ); Send( newcomer, Userinfo( "", 0 ) );
	Check( !strcmp( reply, "connectResponse" ) && connects == 2 && clients[0].state == CS_CONNECTED
	       && clients[0].lastConnectTime == svs.time, "reconnect after sv_reconnectlimit" );

	/* A game rejection is not a drop: every resend gets the game's own reason again, since a client that fixed
	 * its userinfo (a password) is let in on a resend, as in retail. */
	Connect( banned, Userinfo( "", 0 ) );
	Frame( 3000 ); Send( banned, Userinfo( "", 0 ) );
	Check( !strcmp( reply, BANNED_REPLY ) && connects == 2 && !disconnects, "game rejection not repeated on a resend" );
	/* The engine's own refusals a retail client sees are unchanged. */
	Com_sprintf( info, sizeof( info ), "\\protocol\\67\\challenge\\%i\\qport\\7", CHALLENGE );
	Connect( newcomer, info );
	Check( !strcmp( reply, "print\nServer uses protocol version 68.\n" ), "protocol refusal" );
	Com_sprintf( info, sizeof( info ), "\\protocol\\%i\\challenge\\%i\\qport\\7", PROTOCOL_VERSION, CHALLENGE + 1 );
	Connect( newcomer, info );
	Check( !strcmp( reply, "print\nNo or bad challenge for address.\n" ), "challenge refusal" );
	Connect( newcomer, Userinfo( "", 0 ) );
	for ( i = 1; i < 4; i++ ) { clients[i].state = CS_CONNECTED; clients[i].netchan.remoteAddress = Address( 10, 0, 0, i ); }
	svs.challenges[1].adr = other; svs.challenges[1].challenge = CHALLENGE;
	Send( other, Userinfo( "", 0 ) );
	Check( !strcmp( reply, "print\nServer is full.\n" ), "full server refusal" );

	puts( "Server userinfo ip regressions passed (issues #273, #348)" );
	return 0;
}
