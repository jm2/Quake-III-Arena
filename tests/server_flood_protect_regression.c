/* Issue #320: sv_floodProtect must throttle the remote clients of a listen server as it does a
   dedicated server's, but never the listen server's own loopback client or a bot.
   Issue #341: it must throttle a network client that has not entered the world as well, while the
   commands of a retail client's connect sequence, downloads included, still all run.
   Issue #378: the game must never take a command from a client below CS_PRIMED, which has not loaded the
   map (retail's team would spawn it with ClientBegin), while that client's server commands still run.  A
   bot's exit chat, which it sends as a CS_ZOMBIE while the game disconnects it, still reaches the game. */
#include "../code/server/sv_client.c"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#define REMOTE		0				/* a network client */
#define LOCAL		1				/* the listen server's own client; a network client on a dedicated server */
#define PRIMED		2				/* has its gamestate but withholds usercmds, so it never enters the world */
#define BOT			3
#define CONNECTED	4				/* sends the current serverId instead of asking for a gamestate */
#define JOINING		5				/* a retail client going through the connect sequence */
#define RELOADING	6				/* a retail client whose download spans a map change */
#define CLIENTS		7
#define BURST		6				/* game commands in one packet, e.g. a bound key on repeat */
#define QUEUE		64
#define WINDOW		1000			/* sv_floodProtect: one game command per second */
#define BLOCKS		30				/* blocks of the pak JOINING downloads */
#define FRAME		50				/* msec between two packets of a connecting client */

serverStatic_t svs;
server_t sv;
vm_t *gvm;
cvar_t *sv_maxclients, *sv_floodProtect, *sv_pure, *sv_lanForceRate;
cvar_t *com_dedicated, *com_cl_running;
static cvar_t maxclients, floodProtect, pure, lanForceRate, dedicated, clRunning;
static char queued[CLIENTS][QUEUE][MAX_STRING_CHARS];
static char lastSay[CLIENTS][MAX_STRING_CHARS];
static char lastGameCommand[CLIENTS][MAX_STRING_CHARS];	/* another spelling of a server command (see OddSpellings) */
static int gameCommands[CLIENTS];
static int queuedSequence[CLIENTS], moveTime[CLIENTS], thinks[CLIENTS], says[CLIENTS], userinfoChanges[CLIENTS];
static int moving[CLIENTS], begins[CLIENTS], teams[CLIENTS], serverIds[CLIENTS];
static int gamestatesDue, gamestateFor;	/* gamestates the connect sequence asks for with donedl, and its client */
static int dropping = -1, exitChatState;	/* the bot SV_DropClient is dropping, and its state when it chatted */
static sharedEntity_t entities[CLIENTS];
static char emptyConfigstring[1];
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
void SV_Netchan_FreeQueue( client_t *client ) { Check( dropping >= 0 && client == &svs.clients[dropping], "SV_Netchan_FreeQueue" ); }
qboolean NET_CompareAdr( netadr_t a, netadr_t b ) { (void)a; (void)b; Check( 0, "NET_CompareAdr" ); return qfalse; }
void SV_Heartbeat_f( void ) { Check( 0, "SV_Heartbeat_f" ); }
/** Same as sv_bot.c; only the bot being dropped may be freed. */
void SV_BotFreeClient( int clientNum ) {
	client_t *cl;
	Check( clientNum == dropping, "SV_BotFreeClient" );
	cl = &svs.clients[clientNum];
	cl->state = CS_FREE;
	cl->name[0] = 0;
	if ( cl->gentity ) cl->gentity->r.svFlags &= ~SVF_BOT;
}
void SV_SendClientSnapshot( client_t *client ) { (void)client; Check( 0, "SV_SendClientSnapshot" ); }
/** SV_SendClientGameState's output; only the donedl of a connect sequence may ask for a gamestate. */
void SV_UpdateServerCommandsToClient( client_t *client, msg_t *msg ) { (void)msg; Check( gamestatesDue > 0 && client == &svs.clients[gamestateFor], "gamestate resent" ); }
void SV_SendMessageToClient( msg_t *msg, client_t *client ) { (void)msg; Check( gamestatesDue-- > 0 && client == &svs.clients[gamestateFor], "gamestate resent" ); }
/** Only JOINING and RELOADING, the clients that end their connect sequence with a usercmd, may enter the world. */
sharedEntity_t *SV_GentityNum( int num ) { Check( num == JOINING || num == RELOADING, "client entered the world" ); return &entities[num]; }

/** The game module entry points this path reaches; baseq3 ClientCommand reads the command with trap_Argv. */
int VM_CallArgs( vm_t *vm, int callNum, const int *args, int argCount ) {
	int n = args[0];
	(void)vm;
	Check( argCount == 1 && n >= 0 && n < CLIENTS, "game call client" );
	if ( callNum == GAME_CLIENT_THINK ) thinks[n]++;
	else if ( callNum == GAME_CLIENT_BEGIN ) begins[n]++;
	else if ( callNum == GAME_CLIENT_USERINFO_CHANGED ) userinfoChanges[n]++;
	else if ( callNum == GAME_CLIENT_DISCONNECT ) {
		/* retail ClientDisconnect -> BotAIShutdownClient -> BotChat_ExitGame: the bot says goodbye, and
		   EA_Command runs it through sv_bot.c BotClientCommand */
		Check( n == dropping, "client dropped" );
		exitChatState = svs.clients[n].state;
		SV_ExecuteClientCommand( &svs.clients[n], "say goodbye", qtrue );
	}
	else if ( callNum == GAME_CLIENT_COMMAND && !strcmp( Cmd_Argv( 0 ), "team" ) ) {
		/* retail g_cmds.c Cmd_Team_f: SetTeam spawns the client with ClientBegin */
		teams[n]++;
		begins[n]++;
	}
	else if ( callNum == GAME_CLIENT_COMMAND && ( !strcmp( Cmd_Argv( 0 ), "DOWNLOAD" ) || !strcmp( Cmd_Argv( 0 ), "userinfox" ) ) ) {
		/* not the exact name of a server command, so the game's (see OddSpellings) */
		Q_strncpyz( lastGameCommand[n], Cmd_Argv( 0 ), sizeof( lastGameCommand[n] ) );
		gameCommands[n]++;
	}
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
/** Build one CL_WritePacket-style packet (every unacknowledged command, then a usercmd if the
    client sends them) and hand it to SV_ExecuteClientMessage the way SV_PacketEvent does, over
    the network or, for the listen server's own client, over the loopback netchan. */
static void SendPacket( int n ) {
	client_t *cl = &svs.clients[n];
	byte data[MAX_MSGLEN];
	msg_t msg;
	usercmd_t nullcmd, cmd;
	int i, messageAcknowledge = cl->messageAcknowledge + 1;
	MSG_Init( &msg, data, sizeof( data ) );
	MSG_Bitstream( &msg );
	MSG_WriteLong( &msg, serverIds[n] );
	MSG_WriteLong( &msg, messageAcknowledge );
	MSG_WriteLong( &msg, cl->reliableSequence );
	for ( i = cl->lastClientCommand + 1; i <= queuedSequence[n]; i++ ) {
		MSG_WriteByte( &msg, clc_clientCommand );
		MSG_WriteLong( &msg, i );
		MSG_WriteString( &msg, queued[n][i & ( QUEUE - 1 )] );
	}
	if ( moving[n] ) {
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
	/* The usercmd that puts a primed client in the world does not run. */
	Check( cl->state != CS_ACTIVE || thinks[n] + begins[n] == moveTime[n], "usercmd behind the commands did not run" );
}
/** A fresh server: dedicated (no loopback client) or listen (LOCAL on the loopback netchan). */
static void Reset( int listen, int protect ) {
	int i;
	client_t *cl;
	memset( svs.clients, 0, CLIENTS * sizeof( client_t ) );
	memset( queuedSequence, 0, sizeof( queuedSequence ) ); memset( moveTime, 0, sizeof( moveTime ) );
	memset( thinks, 0, sizeof( thinks ) ); memset( says, 0, sizeof( says ) ); memset( begins, 0, sizeof( begins ) );
	memset( userinfoChanges, 0, sizeof( userinfoChanges ) ); memset( lastSay, 0, sizeof( lastSay ) );
	memset( teams, 0, sizeof( teams ) ); memset( gameCommands, 0, sizeof( gameCommands ) );
	memset( lastGameCommand, 0, sizeof( lastGameCommand ) );
	svs.time = 100000; sv.state = SS_GAME; sv.serverId = sv.restartedServerId = 4242; sv.checksumFeed = 0x5eed;
	dedicated.integer = !listen; clRunning.integer = listen; floodProtect.integer = protect;
	for ( i = 0, cl = svs.clients; i < CLIENTS; i++, cl++ ) {
		cl->netchan.remoteAddress.type = i == BOT ? NA_BOT : listen && i == LOCAL ? NA_LOOPBACK : NA_IP;
		Com_sprintf( cl->userinfo, sizeof( cl->userinfo ), "\\name\\player%i", i );
		SV_UserinfoChanged( cl );
		cl->state = i == PRIMED || i == JOINING || i == RELOADING ? CS_PRIMED : i == CONNECTED ? CS_CONNECTED : CS_ACTIVE;
		moving[i] = cl->state == CS_ACTIVE;
		serverIds[i] = sv.serverId;
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
/** A client below CS_PRIMED gets none of its game commands through, whatever the window or sv_floodProtect,
    so its team never spawns it; its userinfo is still applied. */
static void Refused( int n ) {
	int i;
	for ( i = 0; i < BURST; i++ ) Queue( n, va( "say burst%i", i ) );
	Queue( n, "team red" );
	SendPacket( n );
	svs.time += 10 * WINDOW;	/* long after any window */
	Queue( n, "team blue" );
	Queue( n, "userinfo \"\\name\\renamed\"" );
	Queue( n, "say later" );
	SendPacket( n );
	Check( !says[n] && !teams[n], "game command from a client without its gamestate reached the game" );
	Check( !begins[n] && svs.clients[n].state == CS_CONNECTED, "client without its gamestate spawned" );
	Check( userinfoChanges[n] == 1 && !strcmp( svs.clients[n].name, "renamed" ), "userinfo of a connected client not applied" );
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
/** SV_DropClient makes a bot CS_ZOMBIE before the game disconnects it, as retail does, and the game has
    it say goodbye then (kick, bot_minplayers).  Its exit chat reaches the game. */
static void BotExitChat( void ) {
	client_t *cl = &svs.clients[BOT];
	int before = says[BOT];
	entities[BOT].r.svFlags = SVF_BOT;
	cl->gentity = &entities[BOT];
	dropping = BOT;
	SV_DropClient( cl, "was kicked" );
	dropping = -1;
	Check( exitChatState == CS_ZOMBIE && cl->state == CS_FREE, "bot not dropped" );
	Check( says[BOT] == before + 1 && !strcmp( lastSay[BOT], "goodbye" ), "bot exit chat refused" );
}
/** An active client's server command starts the window, as in retail: a say right after a userinfo is
    ignored, in the same packet or half a window later.  Before the client is active only its game commands
    do (see Connect). */
static void ServerCommandStartsWindow( int n, int throttled ) {
	int before = says[n];
	svs.time += 10 * WINDOW;
	Queue( n, "userinfo \"\\name\\windowed\"" );
	Queue( n, "say after" );
	SendPacket( n );
	Check( !strcmp( svs.clients[n].name, "windowed" ), "userinfo not applied" );
	Check( says[n] - before == !throttled, throttled ? "server command of an active client did not start the window" : "say after userinfo throttled" );
	svs.time += 10 * WINDOW;
	Queue( n, "userinfo \"\\name\\later\"" );
	SendPacket( n );
	svs.time += WINDOW / 2;
	Queue( n, "say later" );
	SendPacket( n );
	Check( !strcmp( svs.clients[n].name, "later" ), "userinfo not applied" );
	Check( says[n] - before == 2 * !throttled, throttled ? "server command of an active client did not start the window for its next packet" : "say after userinfo throttled" );
}
/** SV_ExecuteClientCommand runs a command itself only by the exact name of a server command, after
    Cmd_TokenizeString has skipped any leading space; the window of a client that is not active yet
    follows it.  DOWNLOAD and userinfox go to the game and start the window, so the say after each is
    ignored; a userinfo after a space is the server's and does not, so the say after it gets through. */
static void OddSpellings( int n, int throttled ) {
	static const char *spellings[] = { "DOWNLOAD x", "userinfox" };
	int i, before = says[n];
	for ( i = 0; i < 2; i++ ) {
		svs.time += 10 * WINDOW;
		Queue( n, spellings[i] );
		Queue( n, "say after" );
		SendPacket( n );
		Check( gameCommands[n] == i + 1 && !strncmp( spellings[i], lastGameCommand[n], strlen( lastGameCommand[n] ) ),
			"another spelling of a server command did not reach the game" );
		Check( says[n] - before == ( i + 1 ) * !throttled,
			throttled ? "another spelling of a server command did not start the window" : "say after a game command throttled" );
	}
	svs.time += 10 * WINDOW;
	Queue( n, " userinfo \"\\name\\spaced\"" );
	Queue( n, "say spaced" );
	SendPacket( n );
	Check( !strcmp( svs.clients[n].name, "spaced" ) && gameCommands[n] == 2, "userinfo after a space not run by the server" );
	Check( says[n] - before == 2 * !throttled + 1 && !strcmp( lastSay[n], "spaced" ), "userinfo after a space started the window" );
}
/** A connected client's nextdl, and the say refused with it, start no window: the say it sends with the
    donedl that gets it the gamestate reaches the game. */
static void ConnectedStartsNoWindow( int n ) {
	client_t *cl = &svs.clients[n];
	int before = says[n];
	svs.time += 10 * WINDOW;
	Queue( n, "nextdl 0" );	/* no download in progress: nothing to acknowledge */
	Queue( n, "say downloading" );
	SendPacket( n );
	Check( says[n] == before && cl->state == CS_CONNECTED, "game command from a client without its gamestate reached the game" );
	gamestatesDue = 1; gamestateFor = n;
	svs.time += FRAME;
	Queue( n, "donedl" );
	Queue( n, "say loaded" );
	SendPacket( n );
	Check( !gamestatesDue && cl->state == CS_PRIMED, "donedl did not send the gamestate" );
	Check( says[n] == before + 1 && !strcmp( lastSay[n], "loaded" ), "nextdl or a refused say of a connected client started the window" );
}
/** A retail client's connect sequence, from CL_InitDownloads to its first usercmd: it downloads a pak,
    acknowledging every block with a nextdl, while its player says something twice, a full window apart;
    donedl asks for the gamestate again; after loading, cp and the userinfo its cgame registered; then
    usercmds.  All of it runs, and neither say is held back by the nextdl sent around it. */
static void Connect( int n ) {
	client_t *cl = &svs.clients[n];
	int block, typed = 2;
	Queue( n, "download test.pk3" );
	SendPacket( n );
	Check( !strcmp( cl->downloadName, "test.pk3" ), "download not started" );
	for ( block = 0; block <= BLOCKS; block++ ) {
		/* SV_WriteDownloadToClient sent this block; an empty one ends the file */
		cl->downloadBlockSize[block % MAX_DOWNLOAD_WINDOW] = block < BLOCKS ? MAX_DOWNLOAD_BLKSIZE : 0;
		svs.time += FRAME;
		Queue( n, va( "nextdl %i", block ) );
		if ( block == typed ) Queue( n, "say downloading" );
		if ( block == typed + WINDOW / FRAME ) Queue( n, "say almost" );
		SendPacket( n );
		Check( cl->state == CS_PRIMED, "download interrupted" );
	}
	Check( !*cl->downloadName && cl->downloadClientBlock == BLOCKS, "download not completed" );
	Check( says[n] == 2 && !strcmp( lastSay[n], "almost" ), "say typed while downloading ignored" );
	gamestatesDue = 1; gamestateFor = n;
	svs.time += FRAME;
	Queue( n, "donedl" );
	SendPacket( n );
	Check( !gamestatesDue && cl->state == CS_PRIMED, "donedl did not resend the gamestate" );
	svs.time += FRAME;
	Queue( n, va( "cp %i", sv.serverId ) );
	Queue( n, "userinfo \"\\name\\joined\"" );
	SendPacket( n );
	Check( userinfoChanges[n] == 1 && !strcmp( cl->name, "joined" ), "userinfo sent while connecting not applied" );
	moving[n] = 1;
	svs.time += FRAME;
	SendPacket( n );
	Check( cl->state == CS_ACTIVE && begins[n] == 1, "client did not enter the world" );
}
/** A retail client downloading a pak when the map changes: SV_SpawnServer connects it again (CS_CONNECTED),
    and it keeps downloading with its old serverId, which SV_ExecuteClientMessage lets through for a download
    (zerowing bug 536).  Its nextdl all run, but the say and team its player types meanwhile are refused, so
    the game never spawns a client that has not loaded the new map.  donedl gets it the new gamestate, and
    from then on its connect sequence runs as JOINING's does: a say sent with its cp is not held back by the
    team refused just before. */
static void Reload( int n ) {
	client_t *cl = &svs.clients[n];
	int block;
	Queue( n, "download test.pk3" );
	SendPacket( n );
	Check( !strcmp( cl->downloadName, "test.pk3" ), "download not started" );
	sv.serverId = sv.restartedServerId = sv.serverId + 1;
	cl->state = CS_CONNECTED;
	for ( block = 0; block <= BLOCKS; block++ ) {
		cl->downloadBlockSize[block % MAX_DOWNLOAD_WINDOW] = block < BLOCKS ? MAX_DOWNLOAD_BLKSIZE : 0;
		svs.time += FRAME;
		Queue( n, va( "nextdl %i", block ) );
		if ( block == 2 ) Queue( n, "say downloading" );
		if ( block == BLOCKS - 1 ) Queue( n, "team red" );
		SendPacket( n );
		Check( cl->state == CS_CONNECTED, "download interrupted" );
	}
	Check( !*cl->downloadName && cl->downloadClientBlock == BLOCKS, "download not completed" );
	Check( !says[n] && !teams[n], "game command from a client without its gamestate reached the game" );
	Check( !begins[n], "client without its gamestate spawned" );
	gamestatesDue = 1; gamestateFor = n;
	svs.time += FRAME;
	Queue( n, "donedl" );
	SendPacket( n );
	Check( !gamestatesDue && cl->state == CS_PRIMED, "donedl did not send the new gamestate" );
	serverIds[n] = sv.serverId;	/* CL_ParseGamestate */
	svs.time += FRAME;
	Queue( n, va( "cp %i", sv.serverId ) );
	Queue( n, "userinfo \"\\name\\reloaded\"" );
	Queue( n, "say loaded" );
	SendPacket( n );
	Check( userinfoChanges[n] == 1 && !strcmp( cl->name, "reloaded" ), "userinfo sent while connecting not applied" );
	Check( says[n] == 1 && !strcmp( lastSay[n], "loaded" ), "say held back by a refused game command" );
	moving[n] = 1;
	svs.time += FRAME;
	SendPacket( n );
	Check( cl->state == CS_ACTIVE && begins[n] == 1 && !teams[n], "client did not enter the world" );
}
/** One server configuration: which of REMOTE and LOCAL sv_floodProtect must throttle. */
static void Run( const char *name, int listen, int protect, int remoteThrottled, int localThrottled ) {
	config = name;
	Reset( listen, protect );
	if ( remoteThrottled ) Throttled( REMOTE ); else Unthrottled( REMOTE );
	if ( localThrottled ) Throttled( LOCAL ); else Unthrottled( LOCAL );
	/* A network client that withholds its usercmds (primed) is throttled too (#341).  One that never asks
	   for its gamestate (connected) has its game commands refused (#378). */
	if ( remoteThrottled ) Throttled( PRIMED ); else Unthrottled( PRIMED );
	Refused( CONNECTED );
	BotUnthrottled();
	BotExitChat();
	svs.time += 10 * WINDOW;
	UserinfoInWindow( REMOTE, remoteThrottled );
	UserinfoInWindow( LOCAL, localThrottled );
	UserinfoInWindow( PRIMED, remoteThrottled );
	ServerCommandStartsWindow( REMOTE, remoteThrottled );
	ServerCommandStartsWindow( LOCAL, localThrottled );
	OddSpellings( PRIMED, remoteThrottled );
	Check( svs.clients[PRIMED].state == CS_PRIMED && svs.clients[CONNECTED].state == CS_CONNECTED, "client state changed" );
	ConnectedStartsNoWindow( CONNECTED );
	Connect( JOINING );
	Reload( RELOADING );
	/* The listen server's own client is not throttled while it loads a map either. */
	Reset( listen, protect );
	svs.clients[LOCAL].state = CS_PRIMED; moving[LOCAL] = 0;
	if ( localThrottled ) Throttled( LOCAL ); else Unthrottled( LOCAL );
	/* Before it has its gamestate, its game commands are refused like any client's. */
	Reset( listen, protect );
	svs.clients[LOCAL].state = CS_CONNECTED; moving[LOCAL] = 0;
	Refused( LOCAL );
}
int main( void ) {
	int i;
	sv_maxclients = &maxclients; sv_floodProtect = &floodProtect; sv_pure = &pure;
	sv_lanForceRate = &lanForceRate; com_dedicated = &dedicated; com_cl_running = &clRunning;
	maxclients.integer = CLIENTS; lanForceRate.integer = 1;
	for ( i = 0; i < MAX_CONFIGSTRINGS; i++ ) sv.configstrings[i] = emptyConfigstring;
	svs.clients = calloc( CLIENTS, sizeof( client_t ) ); Check( svs.clients != NULL, "allocation" );
	/* A dedicated server throttles every network client, whether it has entered the world or not. */
	Run( "dedicated server", 0, 1, 1, 1 );
	/* A listen server throttles its remote clients too, but never its own local client. */
	Run( "listen server", 1, 1, 1, 0 );
	/* sv_floodProtect 0 still turns it off for everyone. */
	Run( "dedicated server, sv_floodProtect 0", 0, 0, 0, 0 );
	Run( "listen server, sv_floodProtect 0", 1, 0, 0, 0 );
	puts( "Server flood protect regressions passed (issues #320, #341, #378)" );
	return 0;
}
