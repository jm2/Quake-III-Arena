/* Issue #37: the server side of tests/protocol71_regression.c.  The real sv_client.c, with
 * SV_ExecuteClientMessage traded for a capture of the message SV_PacketEvent (the real sv_main.c) hands
 * it after the real sv_net_chan.c and net_chan.c took it; the game and the rest of the server are stubs. */
#define SV_ExecuteClientMessage SV_ExecuteClientMessage_Unused
#include "../code/server/sv_client.c"
#undef SV_ExecuteClientMessage
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "protocol71_fixture.h"

#define CLIENTS 4

static client_t clients[CLIENTS];
static sharedEntity_t entities[CLIENTS];
static playerState_t players[CLIENTS];
static cvar_t maxclients = { .integer = CLIENTS }, zero = { .string = "" }, reconnect = { .integer = 3 };
static byte executed[MAX_MSGLEN];
static int executedLength = -1, executedClient, executedReadcount;

/** The server's side of a failed check. */
static void Fail( const char *message ) { fprintf( stderr, "Protocol 71 regression failed (server): %s\n", message ); exit( 1 ); }
static void Unreachable( void ) { Fail( "unrelated server code reached" ); }

void SV_ExecuteClientMessage( client_t *cl, msg_t *msg );
/** Keep what the netchan made of the client's packet, from where SV_ExecuteClientMessage reads it. */
void SV_ExecuteClientMessage( client_t *cl, msg_t *msg ) {
	if ( msg->readcount > msg->cursize || msg->cursize > (int)sizeof( executed ) ) Fail( "executed message bounds" );
	executedClient = cl - svs.clients;
	executedReadcount = msg->readcount;
	executedLength = msg->cursize - msg->readcount;
	memcpy( executed, msg->data + msg->readcount, executedLength );
}

/* the game, the rest of the server and its file system */
int VM_CallArgs( vm_t *vm, int callNum, const int *args, int argCount ) {
	(void)vm; (void)args; (void)argCount;
	if ( callNum != GAME_CLIENT_CONNECT && callNum != GAME_CLIENT_USERINFO_CHANGED && callNum != GAME_CLIENT_DISCONNECT ) {
		Fail( "unexpected game call" );
	}
	return 0;
}
char *VM_CheckedExplicitString( vm_t *vm, int value, qboolean nullable ) { (void)vm; (void)value; (void)nullable; return NULL; }
sharedEntity_t *SV_GentityNum( int num ) { return &entities[num]; }
playerState_t *SV_GameClientNum( int num ) { return &players[num]; }
void SV_SetUserinfo( int index, const char *val ) { Q_strncpyz( svs.clients[index].userinfo, val, sizeof( svs.clients[index].userinfo ) ); }
void SV_Heartbeat_f( void ) {}
void SV_BotFreeClient( int clientNum ) { (void)clientNum; Unreachable(); }
qboolean SV_GameCommand( void ) { Unreachable(); return qfalse; }
int FS_FileIsInPAK( const char *filename, int *pChecksum ) { (void)filename; (void)pChecksum; Unreachable(); return -1; }
const char *FS_LoadedPakPureChecksums( void ) { Unreachable(); return ""; }
void SV_UpdateServerCommandsToClient( client_t *client, msg_t *msg ) { (void)client; (void)msg; Unreachable(); }
void SV_SendMessageToClient( msg_t *msg, client_t *client ) { (void)msg; (void)client; Unreachable(); }
void SV_SendClientSnapshot( client_t *client ) { (void)client; Unreachable(); }

void Server_Init( void ) {
	memset( clients, 0, sizeof( clients ) );
	memset( &svs, 0, sizeof( svs ) );
	svs.clients = clients; svs.time = 100000; svs.authorizeAddress.type = NA_BAD;
	sv_maxclients = &maxclients; sv_reconnectlimit = &reconnect; sv_privatePassword = &zero; sv_privateClients = &zero;
	sv_minPing = &zero; sv_maxPing = &zero; sv_lanForceRate = &zero; sv_strictAuth = &zero; sv_pure = &zero;
	sv_floodProtect = &zero; sv_hostname = &zero; sv_mapname = &zero; sv_rconPassword = &zero; sv_padPackets = &zero;
	executedLength = -1;
}
qboolean Server_Connected( int n ) { return clients[n].state == CS_CONNECTED; }
qboolean Server_Free( int n ) { return clients[n].state == CS_FREE; }
qboolean Server_ClientCompat( int n ) { return clients[n].netchan.compat; }
int Server_ClientChallenge( int n ) { return clients[n].netchan.challenge; }
netadr_t Server_ClientAddress( int n ) { return clients[n].netchan.remoteAddress; }
int Server_ClientLastPacketTime( int n ) { return clients[n].lastPacketTime; }
int Server_OutgoingSequence( int n ) { return clients[n].netchan.outgoingSequence; }
void Server_SetCommands( int n, const char *lastClientCommand, int acknowledged, const char *serverCommand ) {
	Q_strncpyz( clients[n].lastClientCommandString, lastClientCommand, sizeof( clients[n].lastClientCommandString ) );
	Q_strncpyz( clients[n].reliableCommands[acknowledged & ( MAX_RELIABLE_COMMANDS - 1 )], serverCommand,
	            sizeof( clients[n].reliableCommands[0] ) );
}
void Server_Transmit( int n, msg_t *msg ) { SV_Netchan_Transmit( &clients[n], msg ); }
qboolean Server_NextFragment( int n ) {
	if ( !clients[n].netchan.unsentFragments ) return qfalse;
	SV_Netchan_TransmitNextFragment( &clients[n] );
	return qtrue;
}
int Server_TakeExecuted( int *n, int *readcount, byte *data ) {
	int length = executedLength;
	if ( length >= 0 ) { *n = executedClient; *readcount = executedReadcount; memcpy( data, executed, length ); }
	executedLength = -1;
	return length;
}
qboolean Server_Challenge( netadr_t from, int *challenge, qboolean *refused ) {
	int i;
	for ( i = 0; i < MAX_CHALLENGES; i++ ) {
		if ( NET_CompareAdr( from, svs.challenges[i].adr ) ) {
			*challenge = svs.challenges[i].challenge; *refused = svs.challenges[i].wasrefused;
			return qtrue;
		}
	}
	return qfalse;
}
void Server_Refuse( netadr_t from ) {
	int i;
	for ( i = 0; i < MAX_CHALLENGES; i++ ) {
		if ( NET_CompareAdr( from, svs.challenges[i].adr ) ) svs.challenges[i].wasrefused = qtrue;
	}
}
void Server_SetTime( int msec ) { svs.time = msec; }
