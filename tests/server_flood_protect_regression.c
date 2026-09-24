/* Issue #320: sv_floodProtect must throttle the remote clients of a listen server as it does a
   dedicated server's, but never the listen server's own loopback client or a bot. */
#include "../code/server/sv_client.c"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#define REMOTE		0				/* a network client */
#define LOCAL		1				/* the listen server's own client; a network client on a dedicated server */
#define PRIMED		2				/* still entering the game: exempt, as in retail, while it downloads */
#define BOT			3
#define CLIENTS		4
#define BURST		6				/* game commands in one packet, e.g. a bound key on repeat */
#define QUEUE		64
#define WINDOW		1000			/* sv_floodProtect: one game command per second */

serverStatic_t svs;
server_t sv;
vm_t *gvm;
cvar_t *sv_maxclients, *sv_floodProtect, *sv_pure, *sv_lanForceRate;
cvar_t *com_dedicated, *com_cl_running;
static cvar_t maxclients, floodProtect, pure, lanForceRate, dedicated, clRunning;
static char queued[CLIENTS][QUEUE][MAX_STRING_CHARS];
static char lastSay[CLIENTS][MAX_STRING_CHARS];
static int queuedSequence[CLIENTS], moveTime[CLIENTS], thinks[CLIENTS], says[CLIENTS], userinfoChanges[CLIENTS];
static const char *config;

/** Fail with the violated property and the server configuration. */
static void Check( int ok, const char *message ) {
	if ( !ok ) { fprintf( stderr, "Server flood protect regression failed (%s): %s\n", config, message ); exit( 1 ); }
}
/** No path in this test may raise an engine error. */
void QDECL Com_Error( int level, const char *format, ... ) { (void)level; (void)format; Check( 0, "Com_Error" ); }
void QDECL Com_Printf( const char *format, ... ) { (void)format; }
void QDECL Com_DPrintf( const char *format, ... ) { (void)format; }
/** Same hash as qcommon/common.c, so the usercmd key matches the server's. */
int Com_HashKey( char *string, int maxlen ) {
	int hash = 0, i;
	for ( i = 0; i < maxlen && string[i] != '\0'; i++ ) hash += string[i] * ( 119 + i );
	return hash ^ ( hash >> 10 ) ^ ( hash >> 20 );
}
void Com_Memset( void *dest, const int val, const size_t count ) { memset( dest, val, count ); }
void Com_Memcpy( void *dest, const void *src, const size_t count ) { memcpy( dest, src, count ); }
char *CopyString( const char *in ) { char *out = malloc( strlen( in ) + 1 ); Check( out != NULL, "allocation" ); return strcpy( out, in ); }
void Z_Free( void *ptr ) { free( ptr ); }
qboolean Sys_IsLANAddress( netadr_t adr ) { (void)adr; return qfalse; }
qboolean NET_IsLocalAddress( netadr_t adr ) { return adr.type == NA_LOOPBACK; }
const char *NET_AdrToString( netadr_t a ) { (void)a; return "192.0.2.1:27960"; }
/** Same as sv_init.c. */
void SV_SetUserinfo( int index, const char *val ) {
	Q_strncpyz( svs.clients[index].userinfo, val, sizeof( svs.clients[index].userinfo ) );
	Q_strncpyz( svs.clients[index].name, Info_ValueForKey( val, "name" ), sizeof( svs.clients[index].name ) );
}
/** The game's replies to the commands are not checked. */
void QDECL SV_SendServerCommand( client_t *cl, const char *fmt, ... ) { (void)cl; (void)fmt; }
/** The paths below are linked but must not run here. */
int FS_FileIsInPAK( const char *filename, int *pChecksum ) { (void)filename; (void)pChecksum; Check( 0, "FS_FileIsInPAK" ); return -1; }
const char *FS_LoadedPakPureChecksums( void ) { Check( 0, "FS_LoadedPakPureChecksums" ); return ""; }
void FS_FCloseFile( fileHandle_t f ) { (void)f; Check( 0, "FS_FCloseFile" ); }
void SV_Netchan_FreeQueue( client_t *client ) { (void)client; Check( 0, "SV_Netchan_FreeQueue" ); }
qboolean NET_CompareAdr( netadr_t a, netadr_t b ) { (void)a; (void)b; Check( 0, "NET_CompareAdr" ); return qfalse; }
void SV_Heartbeat_f( void ) { Check( 0, "SV_Heartbeat_f" ); }
void SV_BotFreeClient( int clientNum ) { (void)clientNum; Check( 0, "SV_BotFreeClient" ); }
void SV_SendClientSnapshot( client_t *client ) { (void)client; Check( 0, "SV_SendClientSnapshot" ); }
void SV_UpdateServerCommandsToClient( client_t *client, msg_t *msg ) { (void)client; (void)msg; Check( 0, "gamestate resent" ); }
void SV_SendMessageToClient( msg_t *msg, client_t *client ) { (void)msg; (void)client; Check( 0, "gamestate resent" ); }
sharedEntity_t *SV_GentityNum( int num ) { (void)num; Check( 0, "client entered the world" ); return NULL; }

/** The game module entry points this path reaches; baseq3 ClientCommand reads the command with trap_Argv. */
int VM_CallArgs( vm_t *vm, int callNum, const int *args, int argCount ) {
	int n = args[0];
	(void)vm;
	Check( argCount == 1 && n >= 0 && n < CLIENTS, "game call client" );
	if ( callNum == GAME_CLIENT_THINK ) thinks[n]++;
	else if ( callNum == GAME_CLIENT_USERINFO_CHANGED ) userinfoChanges[n]++;
	else if ( callNum == GAME_CLIENT_COMMAND ) {
		Check( !strcmp( Cmd_Argv( 0 ), "say" ), "unexpected game command" );
		Q_strncpyz( lastSay[n], Cmd_Argv( 1 ), sizeof( lastSay[n] ) );
		says[n]++;
	}
	else Check( 0, "unexpected game call" );
	return 0;
}

/** Add a reliable client command the way CL_AddReliableCommand does. */
static void Queue( int n, const char *command ) {
	queuedSequence[n]++;
	Check( queuedSequence[n] - svs.clients[n].lastClientCommand < QUEUE, "test queue overflow" );
	Q_strncpyz( queued[n][queuedSequence[n] & ( QUEUE - 1 )], command, sizeof( queued[n][0] ) );
}
/** Build one CL_WritePacket-style packet (every unacknowledged command, then a usercmd unless the
    client is still primed) and hand it to SV_ExecuteClientMessage the way SV_PacketEvent does, over
    the network or, for the listen server's own client, over the loopback netchan. */
static void SendPacket( int n ) {
	client_t *cl = &svs.clients[n];
	byte data[MAX_MSGLEN];
	msg_t msg;
	usercmd_t nullcmd, cmd;
	int i, messageAcknowledge = cl->messageAcknowledge + 1;
	MSG_Init( &msg, data, sizeof( data ) );
	MSG_Bitstream( &msg );
	MSG_WriteLong( &msg, sv.serverId );
	MSG_WriteLong( &msg, messageAcknowledge );
	MSG_WriteLong( &msg, cl->reliableSequence );
	for ( i = cl->lastClientCommand + 1; i <= queuedSequence[n]; i++ ) {
		MSG_WriteByte( &msg, clc_clientCommand );
		MSG_WriteLong( &msg, i );
		MSG_WriteString( &msg, queued[n][i & ( QUEUE - 1 )] );
	}
	if ( cl->state == CS_ACTIVE ) {
		memset( &nullcmd, 0, sizeof( nullcmd ) );
		cmd = nullcmd; cmd.serverTime = ++moveTime[n];
		MSG_WriteByte( &msg, clc_moveNoDelta );
		MSG_WriteByte( &msg, 1 );
		MSG_WriteDeltaUsercmdKey( &msg, sv.checksumFeed ^ messageAcknowledge ^
			Com_HashKey( cl->reliableCommands[cl->reliableSequence & ( MAX_RELIABLE_COMMANDS - 1 )], 32 ), &nullcmd, &cmd );
	}
	MSG_WriteByte( &msg, clc_EOF );
	Check( !msg.overflowed, "packet overflow" );
	MSG_BeginReading( &msg );
	cl->lastPacketTime = svs.time;
	SV_ExecuteClientMessage( cl, &msg );
	/* A command ignored by sv_floodProtect is still acknowledged, so the client is not dropped
	   with "Lost reliable commands", and the rest of the packet (its usercmd) still runs. */
	Check( cl->state != CS_ZOMBIE, "client dropped" );
	Check( cl->lastClientCommand == queuedSequence[n], "client commands left unacknowledged" );
	Check( cl->state != CS_ACTIVE || thinks[n] == moveTime[n], "usercmd behind the commands did not run" );
}
/** A fresh server: dedicated (no loopback client) or listen (LOCAL on the loopback netchan). */
static void Reset( int listen, int protect ) {
	int i;
	client_t *cl;
	memset( svs.clients, 0, CLIENTS * sizeof( client_t ) );
	memset( queuedSequence, 0, sizeof( queuedSequence ) ); memset( moveTime, 0, sizeof( moveTime ) );
	memset( thinks, 0, sizeof( thinks ) ); memset( says, 0, sizeof( says ) );
	memset( userinfoChanges, 0, sizeof( userinfoChanges ) ); memset( lastSay, 0, sizeof( lastSay ) );
	svs.time = 100000; sv.state = SS_GAME; sv.serverId = 4242; sv.checksumFeed = 0x5eed;
	dedicated.integer = !listen; clRunning.integer = listen; floodProtect.integer = protect;
	for ( i = 0, cl = svs.clients; i < CLIENTS; i++, cl++ ) {
		cl->netchan.remoteAddress.type = i == BOT ? NA_BOT : listen && i == LOCAL ? NA_LOOPBACK : NA_IP;
		Com_sprintf( cl->userinfo, sizeof( cl->userinfo ), "\\name\\player%i", i );
		SV_UserinfoChanged( cl );
		cl->state = i == PRIMED ? CS_PRIMED : CS_ACTIVE;
		cl->lastPacketTime = svs.time;
	}
}
/** `n` sends BURST says in one packet, then more after a pause; return how many reached the game. */
static int Burst( int n ) {
	int i, before = says[n];
	for ( i = 0; i < BURST; i++ ) Queue( n, va( "say burst%i", i ) );
	SendPacket( n );
	svs.time += WINDOW / 2;	/* a second packet inside the window */
	Queue( n, "say late" );
	SendPacket( n );
	return says[n] - before;
}
/** A client that is throttled gets its first say through, and one more a full window after its last command,
    ignored or not. */
static void Throttled( int n ) {
	Check( Burst( n ) == 1 && !strcmp( lastSay[n], "burst0" ), "game commands not throttled" );
	svs.time += WINDOW / 2;	/* a full window after the first say, but inside the ignored one's */
	Queue( n, "say still" );
	SendPacket( n );
	Check( says[n] == 1, "an ignored game command did not restart the window" );
	svs.time += WINDOW;
	Queue( n, "say again" );
	SendPacket( n );
	Check( says[n] == 2 && !strcmp( lastSay[n], "again" ), "game command a full window later was ignored" );
}
/** A client that is not throttled gets every say through, in order. */
static void Unthrottled( int n ) {
	Check( Burst( n ) == BURST + 1 && !strcmp( lastSay[n], "late" ), "game commands throttled" );
}
/** Inside the window a userinfo change is still applied: it is a server command, which #317's
    userinfo rate limit governs, not sv_floodProtect.  Only the game command after it is ignored. */
static void UserinfoInWindow( int n, int throttled ) {
	int before = says[n];
	Queue( n, "say first" );
	Queue( n, "userinfo \"\\name\\renamed\"" );
	Queue( n, "say second" );
	SendPacket( n );
	Check( userinfoChanges[n] == 1 && !strcmp( svs.clients[n].name, "renamed" ), "userinfo inside the flood window not applied" );
	Check( says[n] - before == ( throttled ? 1 : 2 ), throttled ? "say after userinfo not throttled" : "say after userinfo throttled" );
}
/** Bots run their commands through SV_ExecuteClientCommand directly (sv_bot.c BotClientCommand). */
static void BotUnthrottled( void ) {
	int i;
	for ( i = 0; i < BURST; i++ ) SV_ExecuteClientCommand( &svs.clients[BOT], va( "say bot%i", i ), qtrue );
	Check( says[BOT] == BURST, "bot commands throttled" );
}
/** One server configuration: which of REMOTE and LOCAL sv_floodProtect must throttle. */
static void Run( const char *name, int listen, int protect, int remoteThrottled, int localThrottled ) {
	config = name;
	Reset( listen, protect );
	if ( remoteThrottled ) Throttled( REMOTE ); else Unthrottled( REMOTE );
	if ( localThrottled ) Throttled( LOCAL ); else Unthrottled( LOCAL );
	Unthrottled( PRIMED );
	BotUnthrottled();
	svs.time += 10 * WINDOW;
	UserinfoInWindow( REMOTE, remoteThrottled );
	UserinfoInWindow( LOCAL, localThrottled );
}
int main( void ) {
	sv_maxclients = &maxclients; sv_floodProtect = &floodProtect; sv_pure = &pure;
	sv_lanForceRate = &lanForceRate; com_dedicated = &dedicated; com_cl_running = &clRunning;
	maxclients.integer = CLIENTS; lanForceRate.integer = 1;
	svs.clients = calloc( CLIENTS, sizeof( client_t ) ); Check( svs.clients != NULL, "allocation" );
	/* Dedicated servers are unchanged: every network client is throttled. */
	Run( "dedicated server", 0, 1, 1, 1 );
	/* A listen server throttles its remote clients too, but never its own local client. */
	Run( "listen server", 1, 1, 1, 0 );
	/* sv_floodProtect 0 still turns it off for everyone. */
	Run( "dedicated server, sv_floodProtect 0", 0, 0, 0, 0 );
	Run( "listen server, sv_floodProtect 0", 1, 0, 0, 0 );
	puts( "Server flood protect regressions passed (issue #320)" );
	return 0;
}
