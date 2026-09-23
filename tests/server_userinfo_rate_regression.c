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
#define REPEAT		2				/* changes per 50 msec frame from a UI control on key repeat */
#define HELD		100				/* changes in one held-key burst */
#define FRAME_MSEC	50				/* sv_fps 20 */
#define QUEUE		128				/* a hacked client need not respect MAX_RELIABLE_COMMANDS */
#define BURST		5				/* Quake3e: SVC_RateLimit( &cl->info_rate, 5, 1000 ) */
#define PERIOD		1000

cvar_t *cl_shownet, *com_dedicated, *com_cl_running, *com_sv_running, *com_speeds, *cl_paused, *sv_paused;
int cvar_modifiedFlags, time_game;
qboolean com_errorEntered;
static cvar_t maxclients, dedicated, clRunning, floodProtect, pure, lanForceRate, shownet;
static cvar_t svRunning, speeds, paused, fps, timeout, zombietime, killserver;
static char gameNames[CLIENTS][MAX_NAME_LENGTH];
static char queued[CLIENTS][QUEUE][256];
static int queuedSequence[CLIENTS], moveTime[CLIENTS], thinks[CLIENTS], gameCommands[CLIENTS], applied[CLIENTS], worstWindow;

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
/** SV_Frame work outside this test: bots, snapshots, pings, the master heartbeat. */
void SV_BotFrame( int time ) { (void)time; }
void SV_SendClientMessages( void ) { }
void SV_Heartbeat_f( void ) { }
int Sys_Milliseconds( void ) { return 0; }
/** The paths below are linked but must not run here. */
int FS_FileIsInPAK( const char *filename, int *pChecksum ) { (void)filename; (void)pChecksum; Check( 0, "FS_FileIsInPAK" ); return -1; }
const char *FS_LoadedPakPureChecksums( void ) { Check( 0, "FS_LoadedPakPureChecksums" ); return ""; }
void FS_FCloseFile( fileHandle_t f ) { (void)f; Check( 0, "FS_FCloseFile" ); }
void SV_BotFreeClient( int clientNum ) { (void)clientNum; Check( 0, "SV_BotFreeClient" ); }
void SV_SendClientSnapshot( client_t *client ) { (void)client; Check( 0, "SV_SendClientSnapshot" ); }
void SV_UpdateServerCommandsToClient( client_t *client, msg_t *msg ) { (void)client; (void)msg; Check( 0, "gamestate resent" ); }
void SV_SendMessageToClient( msg_t *msg, client_t *client ) { (void)msg; (void)client; Check( 0, "gamestate resent" ); }
sharedEntity_t *SV_GentityNum( int num ) { (void)num; Check( 0, "client entered the world" ); return NULL; }
playerState_t *SV_GameClientNum( int num ) { (void)num; Check( 0, "SV_GameClientNum" ); return NULL; }
void Cvar_Set( const char *name, const char *value ) { (void)name; (void)value; Check( 0, "Cvar_Set" ); }
char *Cvar_InfoString( int bit ) { (void)bit; Check( 0, "Cvar_InfoString" ); return ""; }
char *Cvar_InfoString_Big( int bit ) { (void)bit; Check( 0, "Cvar_InfoString_Big" ); return ""; }
void NET_Sleep( int msec ) { (void)msec; Check( 0, "NET_Sleep" ); }
qboolean NET_StringToAdr( const char *s, netadr_t *a ) { (void)s; (void)a; Check( 0, "NET_StringToAdr" ); return qfalse; }
void QDECL NET_OutOfBandPrint( netsrc_t sock, netadr_t adr, const char *format, ... ) { (void)sock; (void)adr; (void)format; Check( 0, "NET_OutOfBandPrint" ); }
void SV_ShutdownGameProgs( void ) { Check( 0, "SV_ShutdownGameProgs" ); }
void SV_RemoveOperatorCommands( void ) { Check( 0, "SV_RemoveOperatorCommands" ); }
void CL_Disconnect( qboolean showMainMenu ) { (void)showMainMenu; Check( 0, "CL_Disconnect" ); }

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
	applied[n]++;
}
/** The game module entry points this path reaches. */
int VM_CallArgs( vm_t *vm, int callNum, const int *args, int argCount ) {
	int n = args[0];
	(void)vm;
	if ( callNum == GAME_RUN_FRAME ) { Check( argCount == 1 && n == svs.time, "game frame time" ); return 0; }
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
/** Build one CL_WritePacket-style packet (every unacknowledged command, then a usercmd) and hand it
    to SV_ExecuteClientMessage the way SV_PacketEvent does. */
static void SendPacket( int n, int move ) {
	client_t *cl = &svs.clients[n];
	byte data[MAX_MSGLEN];
	msg_t msg;
	usercmd_t nullcmd, cmd;
	int i, messageAcknowledge = cl->messageAcknowledge + 1;
	if ( cl->state == CS_ZOMBIE || cl->state == CS_FREE ) return;	/* SV_PacketEvent only runs the netchan for these */
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
	cl->lastPacketTime = svs.time;
	SV_ExecuteClientMessage( cl, &msg );
	/* Every command is acknowledged at once, and the netchan key string stays in step with it. */
	Check( cl->state == CS_ZOMBIE || cl->lastClientCommand == queuedSequence[n], "client commands left unacknowledged" );
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
/** One frame: `sender`'s packet, everyone else's acknowledging packet, then the real SV_Frame. */
static void Frame( int sender, int move ) {
	int i;
	SendPacket( sender, move );
	CheckOthers( sender );
	for ( i = 0; i < CLIENTS; i++ ) if ( i != sender ) SendPacket( i, i != PRIMED );
	SV_Frame( FRAME_MSEC );
	CheckOthers( sender );
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
/** Fresh server with CLIENTS connected players; `listen` is a listen server with sv_floodProtect off. */
static void Reset( int listen ) {
	int i;
	client_t *cl;
	for ( i = 0; i < MAX_CONFIGSTRINGS; i++ ) { Z_Free( sv.configstrings[i] ); sv.configstrings[i] = CopyString( "" ); }
	memset( svs.clients, 0, CLIENTS * sizeof( client_t ) );
	memset( queuedSequence, 0, sizeof( queuedSequence ) ); memset( moveTime, 0, sizeof( moveTime ) );
	memset( thinks, 0, sizeof( thinks ) ); memset( gameCommands, 0, sizeof( gameCommands ) );
	memset( gameNames, 0, sizeof( gameNames ) );
	svs.time = 100000; sv.timeResidual = 0; sv.state = SS_GAME; sv.serverId = 4242; sv.checksumFeed = 0x5eed;
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
		cl->lastPacketTime = svs.time;
	}
	memset( applied, 0, sizeof( applied ) );
}
/** Ordinary name, model and team changes still apply at once, usercmds included. */
static void NormalChanges( int listen ) {
	client_t *cl = &svs.clients[NORMAL];
	int i;
	Reset( listen );
	Queue( NORMAL, "userinfo \"\\name\\Visor\\model\\sarge\"" );
	SendPacket( NORMAL, 1 );
	Check( applied[NORMAL] == 1 && thinks[NORMAL] == 1, "single name change delayed" );
	CheckPropagated( NORMAL, "Visor" );
	Check( strstr( svs.clients[FLOODER].reliableCommands[( svs.clients[FLOODER].reliableSequence - 1 ) & ( MAX_RELIABLE_COMMANDS - 1 )],
		"renamed to Visor" ) != NULL, "rename print not sent" );
	Queue( NORMAL, "userinfo \"\\name\\Visor\\model\\visor/blue\"" );
	SendPacket( NORMAL, 1 );
	Check( applied[NORMAL] == 2 && thinks[NORMAL] == 2, "model change delayed" );
	Check( strstr( sv.configstrings[CS_PLAYERS + NORMAL], "\\model\\visor/blue\\" ) != NULL, "model change not applied" );
	CheckPropagated( NORMAL, "Visor" );
	svs.time += 1000;	/* sv_floodProtect allows one game command per second */
	Queue( NORMAL, "team blue" );
	SendPacket( NORMAL, 1 );
	Check( gameCommands[NORMAL] == 1 && thinks[NORMAL] == 3, "team command not passed to the game" );
	/* A settings menu that changes a few userinfo cvars on successive frames is not delayed. */
	svs.time += 10 * PERIOD;
	for ( i = 0; i < BURST; i++ ) Queue( NORMAL, va( "userinfo \"\\name\\Menu%i\\model\\sarge\"", i ) );
	SendPacket( NORMAL, 1 );
	Check( applied[NORMAL] == 2 + BURST && thinks[NORMAL] == 4, "a short burst of changes was delayed" );
	CheckPropagated( NORMAL, va( "Menu%i", BURST - 1 ) );
	for ( i = 0; i < 2 * PERIOD / FRAME_MSEC; i++ ) Frame( NORMAL, 1 );
	Check( applied[NORMAL] == 2 + BURST, "a change was applied twice" );
}
/** A burst sent in one packet: all acknowledged, the first BURST applied, the newest of the rest a second later. */
static void Flood( int sender, int count, int listen ) {
	client_t *cl = &svs.clients[sender];
	char final[MAX_NAME_LENGTH];
	int i, start, frames, move = sender != PRIMED;
	Reset( listen );
	for ( i = 0; i < count; i++ ) Queue( sender, va( "userinfo \"\\name\\flood%i\\model\\sarge\"", i ) );
	Com_sprintf( final, sizeof( final ), "flood%i", count - 1 );
	start = svs.time;
	Frame( sender, move );
	Check( applied[sender] == BURST, "burst not limited" );
	Check( thinks[sender] == move, "usercmd behind the burst did not run" );
	for ( frames = 1; applied[sender] == BURST && frames < 4 * PERIOD / FRAME_MSEC; frames++ ) Frame( sender, move );
	Check( applied[sender] == BURST + 1, "held-back userinfo never applied, or not only the newest" );
	Check( svs.time - start == PERIOD, "held-back userinfo not applied as soon as the rate limit allowed" );
	CheckPropagated( sender, final );
	for ( i = 0; i < 2 * PERIOD / FRAME_MSEC; i++, frames++ ) Frame( sender, move );
	Check( applied[sender] == BURST + 1, "a held-back change was applied twice" );
	Check( thinks[sender] == ( move ? frames : 0 ), "usercmds stalled" );
	Check( worstWindow <= 2 * BURST, "other clients' reliable windows grew past one burst" );
	Check( cl->state == ( sender == PRIMED ? CS_PRIMED : CS_ACTIVE ), "flooder dropped" );
}
/** map_restart and SV_SpawnServer advance svs.time without SV_Frame, so a newer change can be
    applied directly while an older one is still held back; the older one must not follow it. */
static void Superseded( int listen ) {
	int i;
	Reset( listen );
	for ( i = 0; i <= BURST; i++ ) Queue( NORMAL, va( "userinfo \"\\name\\old%i\\model\\sarge\"", i ) );
	SendPacket( NORMAL, 1 );
	Check( applied[NORMAL] == BURST, "burst not limited" );
	svs.time += 3 * 100 + PERIOD;	/* SV_MapRestart_f settle frames */
	Queue( NORMAL, "userinfo \"\\name\\new\\model\\sarge\"" );
	SendPacket( NORMAL, 1 );
	Check( applied[NORMAL] == BURST + 1, "newer change not applied at once" );
	for ( i = 0; i < 4 * PERIOD / FRAME_MSEC; i++ ) Frame( NORMAL, 1 );
	Check( applied[NORMAL] == BURST + 1, "an older held-back change was applied over a newer one" );
	CheckPropagated( NORMAL, "new" );
}
/** A UI control on key repeat: HELD changes, REPEAT per frame, then the same again ending in a disconnect. */
static void HeldKey( int listen ) {
	client_t *cl = &svs.clients[NORMAL];
	char final[MAX_NAME_LENGTH];
	int i, j, start, frames, last;
	Reset( listen );
	start = svs.time;
	for ( i = frames = 0; i < HELD; frames++ ) {
		for ( j = 0; j < REPEAT; j++, i++ ) Queue( NORMAL, va( "userinfo \"\\name\\held%i\\model\\sarge\"", i ) );
		/* A 1.32c client overwrites its netchan key at 64 unacknowledged commands and
		   fails with "Client command overflow" at 65; master acknowledges every command. */
		Check( queuedSequence[NORMAL] - cl->lastClientCommand < MAX_RELIABLE_COMMANDS, "client's own reliable window overflowed" );
		Frame( NORMAL, 1 );
		Check( thinks[NORMAL] == frames + 1, "usercmds stalled during the burst" );
		Check( applied[NORMAL] <= BURST + ( svs.time - start ) / PERIOD, "more than one change per second after the burst" );
	}
	/* The final value arrives within one period of the last change. */
	last = svs.time;
	Com_sprintf( final, sizeof( final ), "held%i", HELD - 1 );
	while ( strcmp( gameNames[NORMAL], final ) && svs.time - last <= PERIOD ) Frame( NORMAL, 1 );
	Check( svs.time - last <= PERIOD, "final userinfo not applied within one second" );
	CheckPropagated( NORMAL, final );
	Check( worstWindow <= 2 * BURST, "other clients' reliable windows grew past one burst" );
	/* Hold the key again, then disconnect: CL_Disconnect queues "disconnect" behind
	   everything unacknowledged, sends three packets and goes silent. */
	for ( i = 0; i < HELD; ) {
		for ( j = 0; j < REPEAT; j++, i++ ) Queue( NORMAL, va( "userinfo \"\\name\\again%i\\model\\sarge\"", i ) );
		if ( i < HELD ) Frame( NORMAL, 1 );
	}
	Queue( NORMAL, "disconnect" );
	SendPacket( NORMAL, 1 ); SendPacket( NORMAL, 1 ); SendPacket( NORMAL, 1 );
	Check( cl->state == CS_ZOMBIE, "disconnect behind a userinfo burst left a ghost player" );
	last = applied[NORMAL];
	for ( i = 0; i < 4 * PERIOD / FRAME_MSEC; i++ ) Frame( NORMAL, 0 );
	Check( applied[NORMAL] == last, "held-back userinfo applied after the client left" );
	Check( cl->state == CS_FREE, "zombie slot not freed" );
}
/** Dedicated and listen-server configurations; sv_floodProtect must not matter. */
int main( void ) {
	int listen;
	sv_maxclients = &maxclients; com_dedicated = &dedicated; com_cl_running = &clRunning;
	sv_floodProtect = &floodProtect; sv_pure = &pure; sv_lanForceRate = &lanForceRate; cl_shownet = &shownet;
	com_sv_running = &svRunning; com_speeds = &speeds; cl_paused = &paused; sv_fps = &fps;
	sv_timeout = &timeout; sv_zombietime = &zombietime; sv_killserver = &killserver;
	maxclients.integer = CLIENTS; lanForceRate.integer = 1; svRunning.integer = 1;
	fps.integer = 1000 / FRAME_MSEC; timeout.integer = 200; zombietime.integer = 2;
	svs.clients = calloc( CLIENTS, sizeof( client_t ) ); Check( svs.clients != NULL, "allocation" );
	for ( listen = 0; listen < 2; listen++ ) {
		NormalChanges( listen );
		Flood( FLOODER, FLOOD, listen );
		/* More changes than MAX_RELIABLE_COMMANDS from a client still in CS_PRIMED. */
		Flood( PRIMED, MAX_RELIABLE_COMMANDS + 8, listen );
		Superseded( listen );
		HeldKey( listen );
	}
	puts( "Server userinfo rate regressions passed (issue #272)" );
	return 0;
}
