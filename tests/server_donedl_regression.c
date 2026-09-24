/* Issue #271: repeated donedl commands must not queue unbounded gamestate copies.
   Issue #314: reconnecting into a slot must release its download.
   Issue #326: slots that fill the server-wide queue budget must not get an honest client dropped.
   Issue #337: shutting the server down must release downloads in progress. */
#include "../code/server/server.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ZONE_BYTES ( 16 << 20 )	/* DEF_COMZONEMEGS: Z_Malloc failure is ERR_FATAL */
#define QUEUE_CAP 4
#define QUEUE_BUDGET ( 2 << 20 )
#define FLOOD 1500
#define SERVER_ID 4242
#define DOWNLOAD_PAK "baseq3/mapdl"
#define DOWNLOAD_BYTES ( 64 << 10 )	/* more than the block window, so the download stays in progress */
#define DOWNLOADERS 4

serverStatic_t svs;
server_t sv;
vm_t *gvm;
static cvar_t zero = { .string = "" }, maxclients = { .string = "64", .integer = MAX_CLIENTS };
static cvar_t running = { .string = "1", .integer = 1 }, reconnect = { .string = "3", .integer = 3 };
static cvar_t flood = { .string = "1", .integer = 1 }, allowDownload = { .string = "0" };
cvar_t *sv_maxclients = &maxclients, *sv_reconnectlimit = &reconnect, *sv_floodProtect = &flood;
cvar_t *com_sv_running = &running;
cvar_t *sv_maxRate = &zero, *sv_pure = &zero, *sv_padPackets = &zero, *sv_lanForceRate = &zero;
cvar_t *sv_allowDownload = &allowDownload, *sv_minPing = &zero, *sv_maxPing = &zero, *sv_privateClients = &zero;
cvar_t *sv_privatePassword = &zero, *com_dedicated = &zero, *com_cl_running = &zero;
qboolean com_errorEntered;
extern cvar_t *showpackets, *showdrop;

static sharedEntity_t entities[MAX_CLIENTS];
static playerState_t players[MAX_CLIENTS];
static byte pvs[MAX_MAP_AREA_BYTES];
static int zoneBytes, zonePeak, queueAllocs, begins, disconnects;
static char dropReason[MAX_STRING_CHARS];
static const char *referencedPaks = "";
static qboolean fileOpen[MAX_FILE_HANDLES];
static int filePos[MAX_FILE_HANDLES], openFiles;

/** Stop on the first behaviour that differs from the bounded policy. */
static void Check( int ok, const char *message ) {
	if ( !ok ) { fprintf( stderr, "Server donedl regression failed: %s\n", message ); exit( 1 ); }
}
/** A fatal error here is the server process exiting. */
void QDECL Com_Error( int code, const char *fmt, ... ) {
	char text[1024]; va_list ap;
	va_start( ap, fmt ); vsnprintf( text, sizeof(text), fmt, ap ); va_end( ap );
	fprintf( stderr, "Server donedl regression failed: Com_Error(%d) %s\n", code, text ); exit( 1 );
}
void QDECL Com_Printf( const char *fmt, ... ) { (void)fmt; }
void QDECL Com_DPrintf( const char *fmt, ... ) { (void)fmt; }
void Com_Memset( void *dest, const int val, const size_t count ) { memset( dest, val, count ); }
void Com_Memcpy( void *dest, const void *src, const size_t count ) { memcpy( dest, src, count ); }
/** Account zone use with the same fatal failure as Z_TagMalloc. */
void *Z_Malloc( int size ) {
	int *block;
	if ( zoneBytes + size > ZONE_BYTES ) Com_Error( ERR_FATAL, "Z_Malloc: failed on allocation of %i bytes", size );
	block = calloc( 1, size + 16 ); Check( block != NULL, "host allocation" );
	block[0] = size; zoneBytes += size; if ( zoneBytes > zonePeak ) zonePeak = zoneBytes;
	if ( size == sizeof(netchan_buffer_t) ) queueAllocs++;
	return (byte *)block + 16;
}
void Z_Free( void *ptr ) { int *block = (int *)( (byte *)ptr - 16 ); zoneBytes -= block[0]; free( block ); }
int Com_HashKey( char *string, int maxlen ) {
	int hash = 0, i;
	for ( i = 0; i < maxlen && string[i]; i++ ) hash += string[i] * ( 119 + i );
	return hash ^ ( hash >> 10 ) ^ ( hash >> 20 );
}
/** Record game callbacks and accept every connection. */
int VM_CallArgs( vm_t *vm, int callNum, const int *args, int argCount ) {
	(void)vm; (void)args; (void)argCount;
	if ( callNum == GAME_CLIENT_BEGIN ) begins++;
	if ( callNum == GAME_CLIENT_DISCONNECT ) disconnects++;
	return 0;
}
char *VM_CheckedExplicitString( vm_t *vm, int value, qboolean nullable ) { (void)vm; (void)nullable; Check( !value, "denied" ); return NULL; }
/** Keep the reason sent with the disconnect command. */
void QDECL SV_SendServerCommand( client_t *cl, const char *fmt, ... ) {
	char text[MAX_STRING_CHARS]; va_list ap;
	va_start( ap, fmt ); vsnprintf( text, sizeof(text), fmt, ap ); va_end( ap );
	if ( cl && !strncmp( text, "disconnect ", 11 ) ) Q_strncpyz( dropReason, text + 11, sizeof(dropReason) );
}
sharedEntity_t *SV_GentityNum( int num ) { return &entities[num]; }
playerState_t *SV_GameClientNum( int num ) { return &players[num]; }
svEntity_t *SV_SvEntityForGentity( sharedEntity_t *gEnt ) { return &sv.svEntities[gEnt - entities]; }
void Sys_SendPacket( int length, const void *data, netadr_t to ) { (void)length; (void)data; (void)to; }
qboolean Sys_IsLANAddress( netadr_t adr ) { (void)adr; return qtrue; }
int CM_PointLeafnum( const vec3_t p ) { (void)p; return 0; }
int CM_LeafCluster( int leafnum ) { (void)leafnum; return 0; }
int CM_LeafArea( int leafnum ) { (void)leafnum; return 0; }
int CM_WriteAreaBits( byte *buffer, int area ) { (void)buffer; (void)area; return 0; }
byte *CM_ClusterPVS( int cluster ) { (void)cluster; return pvs; }
qboolean CM_AreasConnected( int a, int b ) { (void)a; (void)b; return qfalse; }
void SV_BotFreeClient( int clientNum ) { (void)clientNum; }
void SV_Heartbeat_f( void ) {}
void SV_RemoveOperatorCommands( void ) {}
void SV_MasterShutdown( void ) {}
void SV_ShutdownGameProgs( void ) {}
void CL_Disconnect( qboolean showMainMenu ) { (void)showMainMenu; }
void Cvar_Set( const char *name, const char *value ) { (void)name; (void)value; }
qboolean FS_idPak( char *pak, char *base ) { (void)pak; (void)base; return qfalse; }
int FS_FileIsInPAK( const char *name, int *sum ) { (void)name; (void)sum; return -1; }
/** The FS_HandleForFile table: handle 0 is never used and a full table is ERR_DROP. */
int FS_SV_FOpenFileRead( const char *name, fileHandle_t *fp ) {
	int f;
	*fp = 0;
	if ( strcmp( name, DOWNLOAD_PAK ".pk3" ) ) return 0;
	for ( f = 1; f < MAX_FILE_HANDLES && fileOpen[f]; f++ ) {}
	if ( f == MAX_FILE_HANDLES ) Com_Error( ERR_DROP, "FS_HandleForFile: none free" );
	fileOpen[f] = qtrue; filePos[f] = 0; openFiles++; *fp = f;
	return DOWNLOAD_BYTES;
}
void FS_FCloseFile( fileHandle_t f ) {
	Check( f > 0 && f < MAX_FILE_HANDLES && fileOpen[f], "closed a file that is not open" );
	fileOpen[f] = qfalse; openFiles--;
}
int FS_Read( void *buffer, int len, fileHandle_t f ) {
	Check( f > 0 && f < MAX_FILE_HANDLES && fileOpen[f], "read a file that is not open" );
	if ( len > DOWNLOAD_BYTES - filePos[f] ) len = DOWNLOAD_BYTES - filePos[f];
	memset( buffer, 'd', len ); filePos[f] += len;
	return len;
}
const char *FS_ReferencedPakNames( void ) { return referencedPaks; }
const char *FS_LoadedPakPureChecksums( void ) { return ""; }
qboolean FS_FilenameCompare( const char *a, const char *b ) { return strcmp( a, b ) != 0; }

/** Count the gamestate copies and snapshots waiting behind the current fragment train. */
static int Queued( client_t *cl ) {
	netchan_buffer_t *b; int n = 0;
	for ( b = cl->netchan_start_queue; b; b = b->next ) n++;
	return n;
}
/** Send the rest of the fragment train and any queued message, as SV_SendClientMessages does. */
static void Drain( client_t *cl ) {
	int guard = 0;
	while ( cl->netchan.unsentFragments ) { SV_Netchan_TransmitNextFragment( cl ); Check( ++guard < 1000, "drain" ); }
}
/** Build and execute one client packet: header, reliable commands, optional usercmd. */
static void Packet( client_t *cl, int serverId, int ack, const char *command, int count, qboolean move ) {
	static byte data[MAX_MSGLEN];
	msg_t msg; usercmd_t nullcmd, cmd; int i, key;
	MSG_Init( &msg, data, sizeof(data) ); MSG_Bitstream( &msg );
	MSG_WriteLong( &msg, serverId ); MSG_WriteLong( &msg, ack ); MSG_WriteLong( &msg, cl->reliableSequence );
	for ( i = 0; i < count; i++ ) {
		MSG_WriteByte( &msg, clc_clientCommand ); MSG_WriteLong( &msg, cl->lastClientCommand + 1 + i );
		MSG_WriteString( &msg, command );
	}
	if ( move ) {
		memset( &nullcmd, 0, sizeof(nullcmd) ); memset( &cmd, 0, sizeof(cmd) ); cmd.serverTime = svs.time;
		key = sv.checksumFeed ^ ack ^ Com_HashKey( cl->reliableCommands[cl->reliableSequence & (MAX_RELIABLE_COMMANDS-1)], 32 );
		MSG_WriteByte( &msg, clc_move ); MSG_WriteByte( &msg, 1 ); MSG_WriteDeltaUsercmdKey( &msg, key, &nullcmd, &cmd );
	}
	MSG_WriteByte( &msg, clc_EOF );
	Check( !msg.overflowed, "client packet fits" );
	MSG_BeginReading( &msg ); SV_ExecuteClientMessage( cl, &msg );
}
/** Connect a client through SV_DirectConnect with a valid challenge. */
static client_t *Connect( int host, int qport ) {
	netadr_t from; char text[MAX_STRING_CHARS]; int i;
	memset( &from, 0, sizeof(from) ); from.type = NA_IP; from.ip[0] = 10; from.ip[3] = host; from.port = 27960;
	svs.challenges[host].adr = from; svs.challenges[host].challenge = 100 + host;
	Com_sprintf( text, sizeof(text), "connect \"\\protocol\\%i\\challenge\\%i\\qport\\%i\\name\\p%i\\rate\\25000\\snaps\\20\"",
	             PROTOCOL_VERSION, 100 + host, qport, host );
	Cmd_TokenizeString( text ); SV_DirectConnect( from );
	for ( i = 0; i < sv_maxclients->integer; i++ ) {
		if ( svs.clients[i].state == CS_CONNECTED && NET_CompareAdr( from, svs.clients[i].netchan.remoteAddress ) ) return &svs.clients[i];
	}
	Check( 0, "connect" ); return NULL;
}
/** Answer the connection's first packet with the initial gamestate, left mid-fragment. */
static void InitialGamestate( client_t *cl ) {
	Packet( cl, 0, 0, NULL, 0, qfalse );
	Check( cl->state == CS_PRIMED && cl->netchan.unsentFragments && !Queued( cl ), "initial gamestate is fragmented" );
}
/** Start a map whose gamestate needs several fragments, like any real level. */
static void Setup( void ) {
	int i, j; unsigned seed = 1;
	static char set[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789/_";
	showpackets = showdrop = &zero;
	svs.clients = Z_Malloc( MAX_CLIENTS * sizeof(client_t) ); svs.clientCapacity = MAX_CLIENTS; svs.time = 10000;
	svs.numSnapshotEntities = 64; svs.snapshotEntities = calloc( 64, sizeof(entityState_t) );
	sv.state = SS_GAME; sv.serverId = sv.restartedServerId = SERVER_ID; sv.checksumFeed = 0x1234567;
	for ( i = 0; i < MAX_CONFIGSTRINGS; i++ ) {
		int len = i < 8 ? 1500 : 0;
		sv.configstrings[i] = Z_Malloc( len + 1 );
		for ( j = 0; j < len; j++ ) { seed = seed * 1103515245 + 12345; sv.configstrings[i][j] = set[(seed >> 16) & 63]; }
	}
}
/** Retail download completion: one gamestate, the client enters the world; repeats stay single copies. */
static void LegitimateFlow( qboolean mapChange ) {
	client_t *cl = Connect( 1, 11 ); int before, gamestate;
	InitialGamestate( cl ); Drain( cl );
	before = cl->netchan.outgoingSequence;
	Packet( cl, SERVER_ID, before - 1, "download maps.pk3", 1, qfalse );
	Check( !strcmp( cl->downloadName, "maps.pk3" ), "download started" );
	if ( mapChange ) {
		// SV_SpawnServer: the downloading client keeps its old serverId
		sv.serverId = sv.restartedServerId = SERVER_ID + 1; cl->state = CS_CONNECTED;
	}
	Packet( cl, SERVER_ID, before - 1, "nextdl 0", 1, qfalse );
	Check( !cl->downloadName[0] && cl->state != CS_ZOMBIE, "empty block acknowledged, download complete" );
	Packet( cl, SERVER_ID, before - 1, "donedl", 1, qfalse );
	Check( cl->state == CS_PRIMED && cl->gamestateMessageNum == before && cl->netchan.unsentFragments && !Queued( cl ),
	       "donedl sends exactly one fragmented gamestate" );
	Drain( cl ); gamestate = cl->netchan.outgoingSequence - 1;
	Check( gamestate == before, "one message on the wire" );
	Packet( cl, sv.serverId, gamestate, NULL, 0, qtrue );
	Check( cl->state == CS_ACTIVE && begins == 1, "client enters the world after the new gamestate" );
	// as on master an in-game donedl reloads the gamestate, but once per acknowledged gamestate
	Packet( cl, sv.serverId, gamestate, "donedl", 3, qfalse );
	Check( cl->state == CS_PRIMED && cl->gamestateMessageNum == gamestate + 1 && !Queued( cl ) && !queueAllocs,
	       "repeated in-game donedl sends one gamestate" );
	Drain( cl ); Packet( cl, sv.serverId, gamestate + 1, NULL, 0, qtrue );
	Check( cl->state == CS_ACTIVE && begins == 2, "client enters the world again" );
	puts( mapChange ? "download across a map change sends one gamestate" : "download completion sends one gamestate" );
}
/** A packet full of donedl with an honest acknowledge yields one gamestate; the rest are ignored. */
static void RepeatedDonedl( void ) {
	client_t *cl = Connect( 1, 11 ); int before;
	InitialGamestate( cl ); Drain( cl );
	before = cl->netchan.outgoingSequence;
	Packet( cl, SERVER_ID, before - 1, "donedl", FLOOD, qfalse );
	Check( cl->state == CS_PRIMED && cl->lastClientCommand == FLOOD, "every donedl was executed" );
	Check( cl->gamestateMessageNum == before && !Queued( cl ) && !queueAllocs, "repeated donedl queued no copies" );
	Drain( cl ); Check( cl->netchan.outgoingSequence == before + 1, "exactly one gamestate sent" );
	Packet( cl, SERVER_ID, before - 1, "donedl", FLOOD, qfalse );
	Check( cl->netchan.outgoingSequence == before + 1 && !cl->netchan.unsentFragments && !queueAllocs,
	       "donedl before the gamestate is acknowledged is ignored" );
	puts( "repeated donedl is ignored until the gamestate is acknowledged" );
}
/** A forged acknowledge cannot grow the queue past its cap; the client is dropped and the zone recovers. */
static void ForgedAcknowledge( void ) {
	client_t *cl = Connect( 2, 22 ); int base;
	InitialGamestate( cl ); base = zoneBytes;
	Packet( cl, SERVER_ID, 0x7fffff00, "donedl", FLOOD, qfalse );
	Check( cl->state == CS_ZOMBIE && disconnects == 1 && !strcmp( dropReason, "\"Netchan queue overflow\"" ),
	       "queue overflow drops the client" );
	// the ten-fragment gamestate is still in flight, so copies queued until the fifth overflowed
	Check( queueAllocs == QUEUE_CAP && cl->lastClientCommand == QUEUE_CAP + 1, "rest of the packet was not run" );
	Check( !Queued( cl ) && zoneBytes == base, "dropping the client frees its queue" );
	Check( zonePeak - base <= QUEUE_CAP * (int)sizeof(netchan_buffer_t), "zone use stayed under the cap" );
	puts( "forged acknowledges hit the queue cap and drop the client" );
}
/** Reconnecting into a slot with queued messages must release them. */
static void Reconnect( void ) {
	client_t *cl = Connect( 2, 22 ); int base;
	InitialGamestate( cl ); base = zoneBytes;
	Packet( cl, SERVER_ID, 0x7fffff00, "donedl", QUEUE_CAP, qfalse );
	Check( cl->state == CS_PRIMED && Queued( cl ) == QUEUE_CAP, "queue filled to the cap without a drop" );
	svs.time += 5000;
	Check( Connect( 2, 22 ) == cl && !Queued( cl ) && zoneBytes == base, "reconnect frees the old queue" );
	puts( "reconnect releases queued messages" );
}
/** Reconnecting mid-download, more times than there are file handles, must close the file and free
    its block window every time; as in retail, the game gets no ClientDisconnect for a reconnect. */
static void ReconnectDuringDownload( void ) {
	client_t *cl = Connect( 3, 33 ); int cycle, base = zoneBytes;
	allowDownload.integer = 1; referencedPaks = DOWNLOAD_PAK;
	for ( cycle = 0; cycle < 2 * MAX_FILE_HANDLES; cycle++ ) {
		InitialGamestate( cl ); Drain( cl );
		Packet( cl, SERVER_ID, cl->netchan.outgoingSequence - 1, "download " DOWNLOAD_PAK ".pk3", 1, qfalse );
		SV_SendClientSnapshot( cl );
		Check( openFiles == 1 && cl->downloadXmitBlock > 0 && zoneBytes - base >= MAX_DOWNLOAD_WINDOW * MAX_DOWNLOAD_BLKSIZE,
		       "download in progress: file open, block window read, first block sent" );
		svs.time += 5000;
		Check( Connect( 3, 33 ) == cl && cl->state == CS_CONNECTED, "client reconnects into its slot" );
		Check( !openFiles, "reconnect closes the download file" );
		Check( zoneBytes == base, "reconnect frees the download blocks" );
	}
	Check( !disconnects && !begins, "reconnect does not call ClientDisconnect" );
	puts( "reconnect during a download releases its file handle and blocks" );
}
/** SV_MapRestart_f forces a downloading client active; its donedl must still bring the new gamestate. */
static void MapRestartDuringDownload( void ) {
	client_t *cl = Connect( 1, 11 ); int before, gamestate;
	InitialGamestate( cl ); Drain( cl );
	before = cl->netchan.outgoingSequence;
	Packet( cl, SERVER_ID, before - 1, "download maps.pk3", 1, qfalse );
	sv.serverId = SERVER_ID + 1;				// map_restart leaves restartedServerId unchanged
	SV_ClientEnterWorld( cl, &cl->lastUsercmd );	// as SV_MapRestart_f does to every client >= CS_CONNECTED
	Packet( cl, SERVER_ID, before - 1, "nextdl 0", 1, qfalse );
	Packet( cl, SERVER_ID, before - 1, "donedl", 1, qfalse );
	Check( cl->state == CS_PRIMED && cl->gamestateMessageNum == before && cl->netchan.unsentFragments,
	       "donedl after a map_restart during the download sends the gamestate" );
	Drain( cl ); gamestate = cl->netchan.outgoingSequence - 1;
	Packet( cl, sv.serverId, gamestate, NULL, 0, qtrue );
	Check( cl->state == CS_ACTIVE && begins == 2 && !queueAllocs, "client enters the restarted map" );
	puts( "download across a map_restart sends one gamestate" );
}
/** Every slot filled up to the per-client cap cannot queue more than the server-wide budget. */
static void ManyClients( void ) {
	client_t *cl; int i, base = zoneBytes, full = 0;
	for ( i = 1; i <= MAX_CLIENTS; i++ ) {
		cl = Connect( i, i ); InitialGamestate( cl );
		Packet( cl, SERVER_ID, 0x7fffff00, "donedl", QUEUE_CAP, qfalse );
		if ( cl->state == CS_ZOMBIE ) {
			Check( !strcmp( dropReason, "\"Server netchan queue full\"" ) && !Queued( cl ), "budget overflow drops the client" );
		} else {
			Check( Queued( cl ) == QUEUE_CAP, "client under the budget keeps its queue" ); full++;
		}
		Check( zoneBytes - base <= QUEUE_BUDGET, "queued memory stays within the server budget" );
	}
	Check( full == QUEUE_BUDGET / (int)sizeof(netchan_buffer_t) / QUEUE_CAP && disconnects == MAX_CLIENTS - full,
	       "only clients past the budget were dropped" );
	Check( zonePeak - base <= QUEUE_BUDGET, "peak queued memory within the budget" );
	puts( "all client slots together stay within the queue budget" );
}
/** Count the messages queued on every slot. */
static int QueuedTotal( void ) {
	int i, n = 0;
	for ( i = 0; i < sv_maxclients->integer; i++ ) n += Queued( &svs.clients[i] );
	return n;
}
/** Fill the server-wide budget with forged acknowledges from as few slots as the per-client cap allows. */
static void PinBudget( int host ) {
	client_t *cl; int n, first = host, budget = QUEUE_BUDGET / (int)sizeof(netchan_buffer_t);
	for ( ; QueuedTotal() < budget; host++ ) {
		n = budget - QueuedTotal() < QUEUE_CAP ? budget - QueuedTotal() : QUEUE_CAP;
		cl = Connect( host, host ); InitialGamestate( cl );
		Packet( cl, sv.serverId, 0x7fffff00, "donedl", n, qfalse );
		Check( cl->state == CS_PRIMED && Queued( cl ) == n, "attacker slot queues without a drop" );
	}
	Check( host - first == 32 && ( budget + 1 ) * (int)sizeof(netchan_buffer_t) > QUEUE_BUDGET,
	       "32 attacker slots fill the budget" );
}
/** Add a reliable command the way SV_AddServerCommand does. */
static void ServerCommand( client_t *cl, const char *text ) {
	cl->reliableSequence++;
	Q_strncpyz( cl->reliableCommands[cl->reliableSequence & (MAX_RELIABLE_COMMANDS-1)], text, MAX_STRING_CHARS );
}
/** A map change resends the gamestate to an active client whose snapshot is still a fragment train;
    32 slots holding the budget must not get that one queued gamestate refused. */
static void PinnedBudgetMapChange( void ) {
	client_t *cl = Connect( 1, 11 ), *spare; char text[MAX_STRING_CHARS]; int i, snapshot, allocs;
	InitialGamestate( cl ); Drain( cl );
	Packet( cl, SERVER_ID, cl->netchan.outgoingSequence - 1, NULL, 0, qtrue );
	SV_SendClientSnapshot( cl ); snapshot = cl->netchan.outgoingSequence - 1;
	Check( cl->state == CS_ACTIVE && !cl->netchan.unsentFragments, "active client gets whole snapshots" );
	PinBudget( 2 );
	// a burst of long reliable commands (configstring changes at intermission) makes the next snapshot a train
	for ( i = 0; i < 4; i++ ) {
		Com_sprintf( text, sizeof(text), "cs %i \"%.1000s\"", i, sv.configstrings[i] ); ServerCommand( cl, text );
	}
	SV_SendClientSnapshot( cl );
	Check( cl->netchan.unsentFragments && !Queued( cl ), "the snapshot is a fragment train" );
	sv.serverId = sv.restartedServerId = SERVER_ID + 1; cl->state = CS_CONNECTED;	// SV_SpawnServer
	Packet( cl, SERVER_ID, snapshot, NULL, 0, qtrue );
	Check( cl->state == CS_PRIMED && Queued( cl ) == 1, "map change queues the gamestate instead of dropping the client" );
	Drain( cl ); Packet( cl, sv.serverId, cl->netchan.outgoingSequence - 1, NULL, 0, qtrue );
	Check( cl->state == CS_ACTIVE && begins == 2 && !disconnects, "client enters the new map" );
	// only the first message is exempt: past it the budget still refuses a slot
	spare = Connect( 40, 40 ); InitialGamestate( spare ); allocs = queueAllocs;
	Packet( spare, sv.serverId, 0x7fffff00, "donedl", 2, qfalse );
	Check( spare->state == CS_ZOMBIE && !strcmp( dropReason, "\"Server netchan queue full\"" ) && queueAllocs == allocs + 1,
	       "a second queued message is still held to the budget" );
	Check( QueuedTotal() == QUEUE_BUDGET / (int)sizeof(netchan_buffer_t), "the budget is unchanged" );
	puts( "filled queue budget does not drop a client at a map change" );
}
/** The server retransmits a download block when the last acknowledges are held up past a second;
    32 slots holding the budget must not get the gamestate queued by donedl behind it refused. */
static void PinnedBudgetDonedl( void ) {
	client_t *cl = Connect( 1, 11 ); int block, blocks = DOWNLOAD_BYTES / MAX_DOWNLOAD_BLKSIZE, ack;
	allowDownload.integer = 1; referencedPaks = DOWNLOAD_PAK;
	InitialGamestate( cl ); Drain( cl );
	Packet( cl, SERVER_ID, cl->netchan.outgoingSequence - 1, "download " DOWNLOAD_PAK ".pk3", 1, qfalse );
	PinBudget( 2 );
	// one block per snapshot at this rate; all but the last data block are acknowledged in time
	for ( block = 0; block <= blocks; block++ ) {
		SV_SendClientSnapshot( cl );
		Check( cl->downloadXmitBlock == block + 1 && ( block == blocks || cl->netchan.unsentFragments ),
		       "each data block is a fragment train" );
		Drain( cl ); ack = cl->netchan.outgoingSequence - 1;
		if ( block < blocks - 1 ) Packet( cl, SERVER_ID, ack, va( "nextdl %i", block ), 1, qfalse );
	}
	svs.time += 1001;
	SV_SendClientSnapshot( cl );
	Check( cl->downloadXmitBlock == blocks && cl->netchan.unsentFragments && !Queued( cl ), "last data block is retransmitted" );
	Packet( cl, SERVER_ID, ack, va( "nextdl %i", blocks - 1 ), 1, qfalse );
	Packet( cl, SERVER_ID, ack, va( "nextdl %i", blocks ), 1, qfalse );
	Check( !cl->downloadName[0] && !openFiles, "end-of-file block completes the download" );
	Packet( cl, SERVER_ID, ack, "donedl", 1, qfalse );
	Check( cl->state == CS_PRIMED && Queued( cl ) == 1, "donedl queues the gamestate instead of dropping the client" );
	Drain( cl ); Packet( cl, SERVER_ID, cl->netchan.outgoingSequence - 1, NULL, 0, qtrue );
	Check( cl->state == CS_ACTIVE && begins == 1 && !disconnects, "client enters the world" );
	puts( "filled queue budget does not drop a client finishing a download" );
}
/** Each slot's first queued message is outside the budget, so the zone holds at most the budget plus one
    message on each slot that does not hold it: 32 slots pin the budget, the other 32 queue one each. */
static void BudgetPlusFirstMessages( void ) {
	client_t *cl = NULL; int host, allocs, base = zoneBytes, budget = QUEUE_BUDGET / (int)sizeof(netchan_buffer_t);
	PinBudget( 1 );
	for ( host = 33; host <= MAX_CLIENTS; host++ ) {
		cl = Connect( host, host ); InitialGamestate( cl );
		Packet( cl, sv.serverId, 0x7fffff00, "donedl", 1, qfalse );
		Check( cl->state == CS_PRIMED && Queued( cl ) == 1, "a slot's first queued message is outside the budget" );
	}
	Check( budget == 127 && QueuedTotal() == 159, "the budget and one message on each other slot: 159 in all" );
	Check( zonePeak - base <= QUEUE_BUDGET + 32 * (int)sizeof(netchan_buffer_t), "zone use within the budget plus 32 messages" );
	allocs = queueAllocs;
	Packet( cl, sv.serverId, 0x7fffff00, "donedl", 1, qfalse );
	Check( cl->state == CS_ZOMBIE && !strcmp( dropReason, "\"Server netchan queue full\"" ) && queueAllocs == allocs,
	       "the 160th message is refused" );
	Check( QueuedTotal() == 158, "dropping the slot frees its message" );
	puts( "the queue budget plus each slot's first message is the most the zone holds" );
}
/** Final messages queued behind a fragment train are released with the client array. */
static void Shutdown( void ) {
	client_t *cl = Connect( 1, 11 ); int gamestate;
	InitialGamestate( cl ); Drain( cl ); gamestate = cl->netchan.outgoingSequence - 1;
	Packet( cl, SERVER_ID, gamestate, NULL, 0, qtrue );
	Check( cl->state == CS_ACTIVE, "active" );
	SV_SendClientSnapshot( cl );
	sv.serverId = sv.restartedServerId = SERVER_ID + 1;
	Packet( cl, SERVER_ID, gamestate + 1, NULL, 0, qtrue );
	Check( cl->state == CS_PRIMED && cl->netchan.unsentFragments, "map change resends the gamestate" );
	SV_Shutdown( "test" );
	Check( queueAllocs == 2 && !zoneBytes, "shutdown frees the final messages" );
	puts( "shutdown releases queued final messages" );
}
/** Shutting down with several downloads in progress, over more server runs than there are file
    handles, must close every download file and free every block window with the client array.
    A downloader dropped first leaves a zombie slot, whose file and blocks must not be released twice. */
static void ShutdownDuringDownload( void ) {
	client_t *cl; entityState_t *snapshots; int cycle, i, base;
	allowDownload.integer = 1; referencedPaks = DOWNLOAD_PAK;
	for ( cycle = 0; cycle < MAX_FILE_HANDLES; cycle++ ) {
		if ( cycle ) Setup();	// the next server run
		base = zoneBytes;
		for ( i = 1; i <= DOWNLOADERS; i++ ) {
			cl = Connect( i, i ); InitialGamestate( cl ); Drain( cl );
			Packet( cl, SERVER_ID, cl->netchan.outgoingSequence - 1, "download " DOWNLOAD_PAK ".pk3", 1, qfalse );
			SV_SendClientSnapshot( cl );
			Check( openFiles == i && cl->downloadXmitBlock > 0, "download in progress: file open, first block sent" );
		}
		Check( zoneBytes - base >= DOWNLOADERS * MAX_DOWNLOAD_WINDOW * MAX_DOWNLOAD_BLKSIZE, "every block window read" );
		SV_DropClient( cl, "test" );
		Check( cl->state == CS_ZOMBIE && openFiles == DOWNLOADERS - 1, "dropping a downloader closes its file" );
		snapshots = svs.snapshotEntities;
		SV_Shutdown( "test" );
		free( snapshots );
		Check( !openFiles, "shutdown closes every download file" );
		Check( !zoneBytes, "shutdown frees every download block window" );
	}
	puts( "shutdown during downloads releases their file handles and blocks" );
}
int main( int argc, char **argv ) {
	int test = argc > 1 ? atoi( argv[1] ) : -1;
	Setup();
	switch ( test ) {
	case 0: LegitimateFlow( qfalse ); break;
	case 1: RepeatedDonedl(); break;
	case 2: ForgedAcknowledge(); break;
	case 3: Reconnect(); break;
	case 4: Shutdown(); break;
	case 5: LegitimateFlow( qtrue ); break;
	case 6: MapRestartDuringDownload(); break;
	case 7: ManyClients(); break;
	case 8: ReconnectDuringDownload(); break;
	case 9: ShutdownDuringDownload(); break;
	case 10: PinnedBudgetMapChange(); break;
	case 11: PinnedBudgetDonedl(); break;
	case 12: BudgetPlusFirstMessages(); break;
	default: Check( 0, "usage: server-donedl-tests <0-12>" );
	}
	return 0;
}
