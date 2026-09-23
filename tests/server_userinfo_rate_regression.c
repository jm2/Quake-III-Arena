/* Issue #272: one client's userinfo burst must not overflow every other client's reliable commands. */
#include "../code/server/sv_client.c"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#define CLIENTS		8
#define FLOODER		0
#define NORMAL		1
#define PRIMED		(CLIENTS - 1)	/* a client that withholds usercmds stays CS_PRIMED */
#define FLOOD		40				/* userinfo changes queued in one frame */
#define FRAME_MSEC	50
#define QUEUE		128				/* a hacked client need not respect MAX_RELIABLE_COMMANDS */
#define BURST		5				/* Quake3e: SVC_RateLimit( &cl->info_rate, 5, 1000 ) */
#define PERIOD		1000

cvar_t *cl_shownet, *com_dedicated, *com_cl_running;
static cvar_t maxclients, dedicated, clRunning, floodProtect, pure, lanForceRate, shownet;
static char gameNames[CLIENTS][MAX_NAME_LENGTH];
static char queued[CLIENTS][QUEUE][256];
static int queuedSequence[CLIENTS], moveTime[CLIENTS], thinks[CLIENTS], gameCommands[CLIENTS], worstWindow;

/** Fail with the violated property. */
static void Check( int ok, const char *message ) {
	if ( !ok ) { fprintf( stderr, "Server userinfo rate regression failed: %s\n", message ); exit( 1 ); }
}
/** No path in this test may raise an engine error. */
void QDECL Com_Error( int level, const char *format, ... ) { (void)level; (void)format; Check( 0, "Com_Error" ); }
/** Console output, including the overflow dump, is not checked. */
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
qboolean NET_CompareAdr( netadr_t a, netadr_t b ) { return a.type == b.type && a.port == b.port; }
const char *NET_AdrToString( netadr_t a ) { (void)a; return "192.0.2.1:27960"; }
/** Only reached once every client has been dropped, the failure CheckOthers reports. */
void SV_Heartbeat_f( void ) { }
/** The paths below are linked but must not run here. */
int FS_FileIsInPAK( const char *filename, int *pChecksum ) { (void)filename; (void)pChecksum; Check( 0, "FS_FileIsInPAK" ); return -1; }
const char *FS_LoadedPakPureChecksums( void ) { Check( 0, "FS_LoadedPakPureChecksums" ); return ""; }
void SV_SendClientSnapshot( client_t *client ) { (void)client; Check( 0, "SV_SendClientSnapshot" ); }
void FS_FCloseFile( fileHandle_t f ) { (void)f; Check( 0, "FS_FCloseFile" ); }
void SV_BotFreeClient( int clientNum ) { (void)clientNum; Check( 0, "SV_BotFreeClient" ); }
void SV_UpdateServerCommandsToClient( client_t *client, msg_t *msg ) { (void)client; (void)msg; Check( 0, "gamestate resent" ); }
void SV_SendMessageToClient( msg_t *msg, client_t *client ) { (void)msg; (void)client; Check( 0, "gamestate resent" ); }
sharedEntity_t *SV_GentityNum( int num ) { (void)num; Check( 0, "client entered the world" ); return NULL; }

/** baseq3 ClientUserinfoChanged: a "renamed" print once in the game, then the CS_PLAYERS configstring. */
static void GameUserinfoChanged( int n ) {
	client_t *cl = &svs.clients[n];
	char name[MAX_NAME_LENGTH], model[MAX_QPATH];
	Q_strncpyz( name, Info_ValueForKey( cl->userinfo, "name" ), sizeof( name ) );
	Q_strncpyz( model, Info_ValueForKey( cl->userinfo, "model" ), sizeof( model ) );
	if ( cl->state == CS_ACTIVE && strcmp( gameNames[n], name ) ) {
		SV_SendServerCommand( NULL, "print \"%s" S_COLOR_WHITE " renamed to %s\n\"", gameNames[n], name );
	}
	Q_strncpyz( gameNames[n], name, sizeof( gameNames[n] ) );
	SV_SetConfigstring( CS_PLAYERS + n, va( "n\\%s\\t\\0\\model\\%s\\hmodel\\%s\\c1\\4\\c2\\5\\hc\\100\\w\\0\\l\\0\\tt\\0\\tl\\0",
		name, model, model ) );
}
/** The game module entry points this path reaches. */
int VM_CallArgs( vm_t *vm, int callNum, const int *args, int argCount ) {
	int n = args[0];
	(void)vm;
	Check( argCount == 1 && n >= 0 && n < CLIENTS, "game call client" );
	if ( callNum == GAME_CLIENT_USERINFO_CHANGED ) GameUserinfoChanged( n );
	else if ( callNum == GAME_CLIENT_THINK ) thinks[n]++;
	else if ( callNum == GAME_CLIENT_COMMAND ) gameCommands[n]++;
	else Check( callNum == GAME_CLIENT_DISCONNECT, "unexpected game call" );
	return 0;
}

/** Add a reliable client command the way CL_AddReliableCommand does. */
static void Queue( int n, const char *command ) {
	queuedSequence[n]++;
	Check( queuedSequence[n] - svs.clients[n].lastClientCommand < QUEUE, "test queue overflow" );
	Q_strncpyz( queued[n][queuedSequence[n] & ( QUEUE - 1 )], command, sizeof( queued[n][0] ) );
}
/** Build and execute one CL_WritePacket-style packet: every unacknowledged command, then a usercmd. */
static void SendPacket( int n, int move ) {
	client_t *cl = &svs.clients[n];
	byte data[MAX_MSGLEN];
	msg_t msg;
	usercmd_t nullcmd, cmd;
	int i, messageAcknowledge = cl->messageAcknowledge + 1;
	MSG_Init( &msg, data, sizeof( data ) );
	MSG_Bitstream( &msg );
	MSG_WriteLong( &msg, sv.serverId );
	MSG_WriteLong( &msg, messageAcknowledge );
	MSG_WriteLong( &msg, cl->reliableSequence );	/* every server command so far has arrived */
	for ( i = cl->lastClientCommand + 1; i <= queuedSequence[n]; i++ ) {
		MSG_WriteByte( &msg, clc_clientCommand );
		MSG_WriteLong( &msg, i );
		MSG_WriteString( &msg, queued[n][i & ( QUEUE - 1 )] );
	}
	if ( move ) {
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
	SV_ExecuteClientMessage( cl, &msg );
	/* The acknowledged command and the netchan key string must stay in step,
	   or the client can no longer decode the server's packets. */
	Check( cl->lastClientCommand <= queuedSequence[n], "server acknowledged an unsent command" );
	Check( !strcmp( cl->lastClientCommandString, cl->lastClientCommand ?
		queued[n][cl->lastClientCommand & ( QUEUE - 1 )] : "" ), "netchan key string out of step" );
}
/** Require every client other than `sender` to be connected with room in its reliable window. */
static void CheckOthers( int sender ) {
	int i, window;
	for ( i = 0; i < CLIENTS; i++ ) {
		if ( i == sender ) continue;
		Check( svs.clients[i].state == ( i == PRIMED ? CS_PRIMED : CS_ACTIVE ), "another client was dropped (Server command overflow)" );
		window = svs.clients[i].reliableSequence - svs.clients[i].reliableAcknowledge;
		if ( window > worstWindow ) worstWindow = window;
		Check( window < MAX_RELIABLE_COMMANDS, "another client's reliable window is full" );
	}
}
/** Run one server frame: `sender`'s packet, then everyone else sends one acknowledging what arrived. */
static void Frame( int sender, int move ) {
	int i;
	svs.time += FRAME_MSEC;
	SendPacket( sender, move );
	CheckOthers( sender );
	for ( i = 0; i < CLIENTS; i++ ) if ( i != sender ) SendPacket( i, i != PRIMED );
}
/** Require `n`'s current CS_PLAYERS configstring, and every other client's newest server command, to carry `name`. */
static void CheckPropagated( int n, const char *name ) {
	char prefix[64];
	int i;
	client_t *cl;
	Check( !strcmp( svs.clients[n].name, name ) && !strcmp( gameNames[n], name ), "userinfo name not applied" );
	Com_sprintf( prefix, sizeof( prefix ), "n\\%s\\", name );
	Check( !strncmp( sv.configstrings[CS_PLAYERS + n], prefix, strlen( prefix ) ), "configstring not updated" );
	Com_sprintf( prefix, sizeof( prefix ), "cs %i \"n\\%s\\", CS_PLAYERS + n, name );
	for ( i = 0, cl = svs.clients; i < CLIENTS; i++, cl++ ) {
		if ( i == n ) continue;
		Check( !strncmp( cl->reliableCommands[cl->reliableSequence & ( MAX_RELIABLE_COMMANDS - 1 )], prefix, strlen( prefix ) ),
			"configstring update did not reach another client" );
	}
}
/** Fresh server with CLIENTS connected players; `listen` also turns sv_floodProtect off. */
static void Reset( int listen ) {
	int i;
	client_t *cl;
	for ( i = 0; i < MAX_CONFIGSTRINGS; i++ ) { Z_Free( sv.configstrings[i] ); sv.configstrings[i] = CopyString( "" ); }
	memset( svs.clients, 0, CLIENTS * sizeof( client_t ) );
	memset( queuedSequence, 0, sizeof( queuedSequence ) ); memset( moveTime, 0, sizeof( moveTime ) );
	memset( thinks, 0, sizeof( thinks ) ); memset( gameCommands, 0, sizeof( gameCommands ) );
	memset( gameNames, 0, sizeof( gameNames ) );
	svs.time = 100000; sv.state = SS_GAME; sv.serverId = 4242; sv.checksumFeed = 0x5eed;
	clRunning.integer = listen; dedicated.integer = !listen; floodProtect.integer = !listen; worstWindow = 0;
	for ( i = 0, cl = svs.clients; i < CLIENTS; i++, cl++ ) {	/* ClientConnect, before anyone is primed */
		cl->netchan.remoteAddress.type = listen && i == NORMAL ? NA_LOOPBACK : NA_IP;
		cl->netchan.remoteAddress.port = i + 1;
		Com_sprintf( cl->userinfo, sizeof( cl->userinfo ), "\\name\\player%i\\model\\sarge", i );
		SV_UserinfoChanged( cl );
		GameUserinfoChanged( i );
	}
	for ( i = 0, cl = svs.clients; i < CLIENTS; i++, cl++ ) {
		Check( cl->reliableSequence == 0, "server command sent during setup" );
		cl->state = i == PRIMED ? CS_PRIMED : CS_ACTIVE;
	}
}
/** Ordinary name, model and team changes still apply at once, usercmds included. */
static void NormalChanges( int listen ) {
	client_t *cl = &svs.clients[NORMAL];
	int i;
	Reset( listen );
	Queue( NORMAL, "userinfo \"\\name\\Visor\\model\\sarge\"" );
	SendPacket( NORMAL, 1 );
	Check( cl->lastClientCommand == 1 && thinks[NORMAL] == 1, "single name change delayed" );
	CheckPropagated( NORMAL, "Visor" );
	Check( strstr( svs.clients[FLOODER].reliableCommands[( svs.clients[FLOODER].reliableSequence - 1 ) & ( MAX_RELIABLE_COMMANDS - 1 )],
		"renamed to Visor" ) != NULL, "rename print not sent" );
	Queue( NORMAL, "userinfo \"\\name\\Visor\\model\\visor/blue\"" );
	SendPacket( NORMAL, 1 );
	Check( cl->lastClientCommand == 2 && thinks[NORMAL] == 2, "model change delayed" );
	Check( strstr( sv.configstrings[CS_PLAYERS + NORMAL], "\\model\\visor/blue\\" ) != NULL, "model change not applied" );
	CheckPropagated( NORMAL, "Visor" );
	svs.time += 1000;	/* sv_floodProtect allows one game command per second */
	Queue( NORMAL, "team blue" );
	SendPacket( NORMAL, 1 );
	Check( cl->lastClientCommand == 3 && gameCommands[NORMAL] == 1 && thinks[NORMAL] == 3, "team command not passed to the game" );
	/* A settings menu that changes a few userinfo cvars on successive frames is not delayed. */
	svs.time += 10 * PERIOD;
	for ( i = 0; i < BURST; i++ ) Queue( NORMAL, va( "userinfo \"\\name\\Menu%i\\model\\sarge\"", i ) );
	SendPacket( NORMAL, 1 );
	Check( cl->lastClientCommand == 3 + BURST && thinks[NORMAL] == 4, "a short burst of changes was delayed" );
	CheckPropagated( NORMAL, va( "Menu%i", BURST - 1 ) );
	CheckOthers( NORMAL );
}
/** One client's burst leaves everyone connected, and its final value still propagates. */
static void Flood( int sender, int count, int listen ) {
	client_t *cl = &svs.clients[sender];
	char final[MAX_NAME_LENGTH];
	int i, frames, move = sender != PRIMED;
	Reset( listen );
	for ( i = 0; i < count; i++ ) Queue( sender, va( "userinfo \"\\name\\flood%i\\model\\sarge\"", i ) );
	Com_sprintf( final, sizeof( final ), "flood%i", count - 1 );
	Frame( sender, move );
	/* Only the burst was applied; the rest stays unacknowledged, so the client resends it. */
	Check( cl->lastClientCommand == BURST, "more than the burst acknowledged; the client must resend the rest" );
	Check( thinks[sender] == 0, "flooder's usercmd ran after a stalled command" );
	for ( frames = 0; cl->lastClientCommand < count && frames < 2 * count * PERIOD / FRAME_MSEC; frames++ ) {
		Frame( sender, move );
	}
	Check( cl->lastClientCommand == count, "final userinfo never applied" );
	Check( frames * FRAME_MSEC == ( count - BURST ) * PERIOD, "not one change per second after the burst" );
	Check( worstWindow <= 2 * BURST, "other clients' reliable windows grew past one burst" );
	CheckPropagated( sender, final );
	if ( move ) {
		Check( thinks[sender] == 1, "flooder's usercmds were not held back" );
		Frame( sender, move );
		Check( thinks[sender] == 2, "flooder's usercmds did not resume" );
	}
	Check( cl->state == ( sender == PRIMED ? CS_PRIMED : CS_ACTIVE ), "flooder dropped" );
}
/** Dedicated and listen-server configurations; sv_floodProtect must not matter. */
int main( void ) {
	int listen;
	sv_maxclients = &maxclients; com_dedicated = &dedicated; com_cl_running = &clRunning;
	sv_floodProtect = &floodProtect; sv_pure = &pure; sv_lanForceRate = &lanForceRate; cl_shownet = &shownet;
	maxclients.integer = CLIENTS; lanForceRate.integer = 1;
	svs.clients = calloc( CLIENTS, sizeof( client_t ) ); Check( svs.clients != NULL, "allocation" );
	for ( listen = 0; listen < 2; listen++ ) {
		NormalChanges( listen );
		Flood( FLOODER, FLOOD, listen );
		/* More changes than MAX_RELIABLE_COMMANDS from a client still in CS_PRIMED. */
		Flood( PRIMED, MAX_RELIABLE_COMMANDS + 8, listen );
	}
	puts( "Server userinfo rate regressions passed (issue #272)" );
	return 0;
}
