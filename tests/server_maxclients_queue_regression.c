/* Issue #313: a sv_maxclients change must leave every netchan queue tail inside the new client array. */
#include "../code/server/server.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ZONE_BYTES ( 16 << 20 )	/* DEF_COMZONEMEGS: Z_Malloc failure is ERR_FATAL */
#define QUEUE_CAP 4
#define OLD_MAX 8
#define FORGED_ACK 0x7fffff00

serverStatic_t svs;
server_t sv;
vm_t *gvm;
static cvar_t zero = { .string = "" }, maxclients = { .string = "8", .integer = OLD_MAX };
static cvar_t running = { .string = "1", .integer = 1 }, reconnect = { .string = "3", .integer = 3 };
static cvar_t flood = { .string = "1", .integer = 1 };
cvar_t *sv_maxclients = &maxclients, *sv_reconnectlimit = &reconnect, *sv_floodProtect = &flood;
cvar_t *com_sv_running = &running;
cvar_t *sv_maxRate = &zero, *sv_pure = &zero, *sv_padPackets = &zero, *sv_lanForceRate = &zero;
cvar_t *sv_allowDownload = &zero, *sv_minPing = &zero, *sv_maxPing = &zero, *sv_privateClients = &zero;
cvar_t *sv_privatePassword = &zero, *com_dedicated = &zero, *com_cl_running = &zero;
qboolean com_errorEntered;
extern cvar_t *showpackets, *showdrop;
void SV_Startup( void );	/* sv_init.c, no header prototype */

static sharedEntity_t entities[MAX_CLIENTS];
static playerState_t players[MAX_CLIENTS];
static byte pvs[MAX_MAP_AREA_BYTES];
static int zoneBytes, queueLive, queueAllocs, latched, serverId = 4242;
static char dropReason[MAX_STRING_CHARS];

/** Stop on the first queue that was lost, leaked or written outside the client array. */
static void Check( int ok, const char *message ) {
	if ( !ok ) { fprintf( stderr, "Server maxclients queue regression failed: %s\n", message ); exit( 1 ); }
}
/** A fatal error here is the server process exiting. */
void QDECL Com_Error( int code, const char *fmt, ... ) {
	char text[1024]; va_list ap;
	va_start( ap, fmt ); vsnprintf( text, sizeof(text), fmt, ap ); va_end( ap );
	fprintf( stderr, "Server maxclients queue regression failed: Com_Error(%d) %s\n", code, text ); exit( 1 );
}
void QDECL Com_Printf( const char *fmt, ... ) { (void)fmt; }
void QDECL Com_DPrintf( const char *fmt, ... ) { (void)fmt; }
void Com_Memset( void *dest, const int val, const size_t count ) { memset( dest, val, count ); }
void Com_Memcpy( void *dest, const void *src, const size_t count ) { memcpy( dest, src, count ); }
/** Account zone use with the same fatal failure as Z_TagMalloc; client arrays are larger than queue buffers. */
void *Z_Malloc( int size ) {
	int *block;
	if ( zoneBytes + size > ZONE_BYTES ) Com_Error( ERR_FATAL, "Z_Malloc: failed on allocation of %i bytes", size );
	block = calloc( 1, size + 16 ); Check( block != NULL, "host allocation" );
	block[0] = size; zoneBytes += size;
	if ( size == sizeof(netchan_buffer_t) ) { queueLive++; queueAllocs++; }
	return (byte *)block + 16;
}
void Z_Free( void *ptr ) {
	int *block = (int *)( (byte *)ptr - 16 );
	zoneBytes -= block[0];
	if ( block[0] == sizeof(netchan_buffer_t) ) queueLive--;
	free( block );
}
void *Hunk_AllocateTempMemory( int size ) { void *p = malloc( size ); Check( p != NULL, "temp memory" ); return p; }
void Hunk_FreeTempMemory( void *buf ) { free( buf ); }
/** SV_BoundMaxClients re-registers sv_maxclients, which applies the value latched for the next map. */
cvar_t *Cvar_Get( const char *name, const char *value, int flags ) {
	(void)value; (void)flags;
	Check( !strcmp( name, "sv_maxclients" ), "only sv_maxclients is registered" );
	if ( latched ) { maxclients.integer = latched; latched = 0; }
	return &maxclients;
}
/** Cvar_Set forces the value, which SV_BoundMaxClients relies on to raise it. */
void Cvar_Set( const char *name, const char *value ) {
	if ( !strcmp( name, "sv_maxclients" ) ) maxclients.integer = atoi( value );
}
int Com_HashKey( char *string, int maxlen ) {
	int hash = 0, i;
	for ( i = 0; i < maxlen && string[i]; i++ ) hash += string[i] * ( 119 + i );
	return hash ^ ( hash >> 10 ) ^ ( hash >> 20 );
}
/** Accept every connection. */
int VM_CallArgs( vm_t *vm, int callNum, const int *args, int argCount ) { (void)vm; (void)callNum; (void)args; (void)argCount; return 0; }
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
qboolean FS_idPak( char *pak, char *base ) { (void)pak; (void)base; return qfalse; }
int FS_FileIsInPAK( const char *name, int *sum ) { (void)name; (void)sum; return -1; }
void FS_FCloseFile( fileHandle_t f ) { (void)f; }
int FS_SV_FOpenFileRead( const char *name, fileHandle_t *fp ) { (void)name; *fp = 0; return 0; }
int FS_Read( void *buffer, int len, fileHandle_t f ) { (void)buffer; (void)len; (void)f; return 0; }
const char *FS_ReferencedPakNames( void ) { return ""; }
const char *FS_LoadedPakPureChecksums( void ) { return ""; }
qboolean FS_FilenameCompare( const char *a, const char *b ) { return strcmp( a, b ) != 0; }

/** Count the messages waiting behind a slot's fragment train. */
static int Queued( int slot ) {
	netchan_buffer_t *b; int n = 0;
	for ( b = svs.clients[slot].netchan_start_queue; b; b = b->next ) n++;
	return n;
}
/** The tail must be the last next pointer of the list, inside this slot or a queued buffer. */
static void CheckTail( int slot ) {
	client_t *cl = &svs.clients[slot]; netchan_buffer_t **tail = &cl->netchan_start_queue;
	while ( *tail ) tail = &(*tail)->next;
	Check( cl->netchan_end_queue == tail, "queue tail follows the client into the new array" );
}
/** Send the rest of the fragment train and every message queued behind it; return how many went out. */
static int Drain( int slot ) {
	client_t *cl = &svs.clients[slot]; int guard = 0, before = cl->netchan.outgoingSequence;
	while ( cl->netchan.unsentFragments ) { SV_Netchan_TransmitNextFragment( cl ); Check( ++guard < 1000, "drain" ); }
	Check( !Queued( slot ), "every queued message was sent" );
	CheckTail( slot );
	return cl->netchan.outgoingSequence - before;
}
/** Build and execute one client packet: header, reliable commands, optional usercmd. */
static void Packet( client_t *cl, int packetServerId, int ack, const char *command, int count, qboolean move ) {
	static byte data[MAX_MSGLEN];
	msg_t msg; usercmd_t nullcmd, cmd; int i, key;
	MSG_Init( &msg, data, sizeof(data) ); MSG_Bitstream( &msg );
	MSG_WriteLong( &msg, packetServerId ); MSG_WriteLong( &msg, ack ); MSG_WriteLong( &msg, cl->reliableSequence );
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
/** Connect a client through SV_DirectConnect and answer its first packet with the fragmented gamestate. */
static int Connect( int host ) {
	netadr_t from; char text[MAX_STRING_CHARS]; int i;
	memset( &from, 0, sizeof(from) ); from.type = NA_IP; from.ip[0] = 10; from.ip[3] = host; from.port = 27960;
	svs.challenges[host].adr = from; svs.challenges[host].challenge = 100 + host;
	Com_sprintf( text, sizeof(text), "connect \"\\protocol\\%i\\challenge\\%i\\qport\\%i\\name\\p%i\\rate\\25000\\snaps\\20\"",
	             PROTOCOL_VERSION, 100 + host, host, host );
	Cmd_TokenizeString( text ); SV_DirectConnect( from );
	for ( i = 0; i < sv_maxclients->integer; i++ ) {
		if ( svs.clients[i].state == CS_CONNECTED && NET_CompareAdr( from, svs.clients[i].netchan.remoteAddress ) ) break;
	}
	Check( i < sv_maxclients->integer, "connect" );
	Packet( &svs.clients[i], 0, 0, NULL, 0, qfalse );
	Check( svs.clients[i].state == CS_PRIMED && svs.clients[i].netchan.unsentFragments && !Queued( i ),
	       "initial gamestate is fragmented" );
	return i;
}
/** A client in the world whose last message was a snapshot, with an empty queue and nothing in flight. */
static int Active( int host ) {
	int slot = Connect( host ), gamestate; client_t *cl = &svs.clients[slot];
	Drain( slot ); gamestate = cl->netchan.outgoingSequence - 1;
	Packet( cl, sv.serverId, gamestate, NULL, 0, qtrue );
	Check( cl->state == CS_ACTIVE, "client entered the world" );
	SV_SendClientSnapshot( cl );
	Check( !cl->netchan.unsentFragments && !Queued( slot ), "snapshot went out whole" );
	return slot;
}
/** A client acknowledging everything asks for gamestate copies (#271); behind a fragment train they queue. */
static void Donedl( int slot, int count ) {
	Packet( &svs.clients[slot], sv.serverId, FORGED_ACK, "donedl", count, qfalse );
}
/** SV_SpawnServer with a latched sv_maxclients: resize the client array, then restart the level. */
static void MapChange( int latchedMax ) {
	int i;
	latched = latchedMax; maxclients.modified = qtrue;
	SV_ChangeMaxClients();
	Check( svs.clientCapacity == sv_maxclients->integer && !maxclients.modified, "client array follows sv_maxclients" );
	free( svs.snapshotEntities ); svs.snapshotEntities = calloc( svs.numSnapshotEntities, sizeof(entityState_t) );
	svs.nextSnapshotEntities = 0;
	sv.serverId = sv.restartedServerId = ++serverId;
	for ( i = 0; i < sv_maxclients->integer; i++ ) {
		if ( svs.clients[i].state >= CS_CONNECTED ) {
			svs.clients[i].state = CS_CONNECTED;	// humans get the new gamestate with their next packet
		}
	}
}
/** The client's first packet after the map change acknowledges its last snapshot and gets the new gamestate. */
static void Rejoin( int slot ) {
	client_t *cl = &svs.clients[slot];
	Packet( cl, sv.serverId - 1, cl->netchan.outgoingSequence - 1, NULL, 0, qfalse );
	Check( cl->state == CS_PRIMED && cl->netchan.unsentFragments && !Queued( slot ), "map change resends the gamestate" );
}
/** Stop the server; every queue and the client array must go back to the zone exactly once. */
static void Shutdown( void ) {
	SV_Shutdown( "test" );
	Check( !queueLive && !zoneBytes, "shutdown released every queued message and the client array" );
}
/** Start a server of OLD_MAX slots on a map whose gamestate needs several fragments, like any real level. */
static void Setup( void ) {
	int i, j; unsigned seed = 1;
	static char set[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789/_";
	showpackets = showdrop = &zero;
	SV_Startup();
	Check( svs.clientCapacity == OLD_MAX, "startup allocates sv_maxclients slots" );
	svs.snapshotEntities = calloc( svs.numSnapshotEntities, sizeof(entityState_t) ); svs.time = 10000;
	sv.state = SS_GAME; sv.serverId = sv.restartedServerId = serverId; sv.checksumFeed = 0x1234567;
	for ( i = 0; i < MAX_CONFIGSTRINGS; i++ ) {
		int len = i < 8 ? 1500 : 0;
		sv.configstrings[i] = Z_Malloc( len + 1 );
		for ( j = 0; j < len; j++ ) { seed = seed * 1103515245 + 12345; sv.configstrings[i][j] = set[(seed >> 16) & 63]; }
	}
}

/** Empty queues with a fragment train in flight across the resize, then gamestate copies queued behind it. */
static void EmptyQueue( void ) {
	int a = Active( 1 ), b = Active( 2 ), c = Active( 3 );
	Donedl( b, 1 );		// b is mid-train with nothing queued
	Check( svs.clients[b].netchan.unsentFragments && !Queued( b ), "gamestate in flight" );
	MapChange( 4 );
	Check( sv_maxclients->integer == 4 && !queueLive, "shrunk to the latched value" );
	Donedl( b, 2 );
	Check( Queued( b ) == 2 && queueLive == 2, "copies queued behind the train that crossed the resize" );
	Rejoin( a ); Donedl( a, 2 );
	Check( Queued( a ) == 2 && queueLive == 4, "copies queued behind the new gamestate" );
	Check( Drain( a ) == 3 && Drain( b ) == 3 && !queueLive, "every queued message was sent once" );
	Rejoin( c ); Drain( c );
	Shutdown();
	puts( "empty queues are re-pointed into the new client array" );
}
/** Honest map change: the resent gamestate is in flight when the server quits, so the final messages queue. */
static void FinalMessage( void ) {
	int a = Active( 1 ), b = Active( 2 );
	MapChange( 16 );
	Check( sv_maxclients->integer == 16 && svs.clientCapacity == 16, "grown to the latched value" );
	Rejoin( a ); Rejoin( b );
	queueAllocs = 0;
	Shutdown();
	Check( queueAllocs == 4, "both final messages queued for each client" );
	puts( "final messages after a resize queue inside the new client array" );
}
/** Messages already queued survive the copy, keep their order and are freed once, by a send or a drop. */
static void NonEmptyQueue( void ) {
	int a = Active( 1 ), b = Active( 2 );
	Donedl( a, 3 );				// one gamestate in flight, two queued
	Donedl( b, QUEUE_CAP + 1 );	// one in flight, the queue at its cap
	Check( Queued( a ) == 2 && Queued( b ) == QUEUE_CAP && queueLive == 6, "queues filled before the resize" );
	MapChange( 4 );
	Check( Queued( a ) == 2 && Queued( b ) == QUEUE_CAP && queueLive == 6, "no queued message lost or freed by the resize" );
	Donedl( a, 2 );
	Check( Queued( a ) == QUEUE_CAP && queueLive == 8, "new messages append after the copied ones" );
	Donedl( b, 1 );
	Check( svs.clients[b].state == CS_ZOMBIE && !strcmp( dropReason, "\"Netchan queue overflow\"" ), "cap still enforced" );
	Check( !Queued( b ) && queueLive == QUEUE_CAP, "dropping the client frees its copied queue once" );
	Check( Drain( a ) == 1 + QUEUE_CAP && !queueLive, "every copied and appended message was sent once" );
	Rejoin( a ); Donedl( a, 1 );
	Check( Queued( a ) == 1, "the tail is reset into the new array once the copied queue empties" );
	Shutdown();
	puts( "queued messages survive a resize and are released once" );
}
/** A shrink discards free and zombie slots past the highest connected client, with anything they still queue. */
static void ShrinkDiscards( void ) {
	int a = Active( 1 ), b = Active( 2 ), c = Active( 3 ), d = Active( 4 ), e = Active( 5 );
	byte data[64]; msg_t msg;
	Donedl( a, 3 );
	Check( Queued( a ) == 2, "kept client queue" );
	SV_DropClient( &svs.clients[c], "left" );
	Donedl( d, QUEUE_CAP + 2 );
	Check( svs.clients[d].state == CS_ZOMBIE && !Queued( d ), "overflow dropped a client and freed its queue" );
	// nothing queues for a zombie today (SV_DropClient empties the queue first),
	// but a discarded slot must not leak whatever it still holds
	Donedl( e, 1 ); SV_DropClient( &svs.clients[e], "left" );
	MSG_Init( &msg, data, sizeof(data) ); MSG_WriteLong( &msg, 0 );
	SV_Netchan_Transmit( &svs.clients[e], &msg );
	Check( svs.clients[e].state == CS_ZOMBIE && Queued( e ) == 1 && queueLive == 3, "zombie slot holds a queued message" );
	MapChange( 1 );
	Check( sv_maxclients->integer == b + 1, "never shrunk below the highest connected client" );
	Check( Queued( a ) == 2 && queueLive == 2, "discarded slots released their queue exactly once" );
	Rejoin( b ); Donedl( b, 2 );
	Check( Queued( b ) == 2, "copies queued for the kept idle client" );
	Donedl( a, 2 );
	Check( Queued( a ) == QUEUE_CAP, "copies appended for the kept busy client" );
	Check( Drain( a ) == 1 + QUEUE_CAP && Drain( b ) == 3 && !queueLive, "kept clients sent every message once" );
	Shutdown();
	puts( "a shrink releases the queues of discarded slots" );
}
/** A grow keeps the old clients and their queues and lets new clients use the added slots. */
static void Grow( void ) {
	int slot, a = 0, b = 0, c = 0, fresh[4], i;
	for ( i = 1; i <= OLD_MAX; i++ ) {
		slot = Active( i );
		if ( i == 1 ) a = slot; else if ( i == 2 ) b = slot; else if ( i == 3 ) c = slot;
	}
	Donedl( b, 3 ); Donedl( c, 1 );
	Check( Queued( b ) == 2 && !Queued( c ) && svs.clients[c].netchan.unsentFragments, "queues before the grow" );
	MapChange( OLD_MAX + 4 );
	Check( sv_maxclients->integer == OLD_MAX + 4 && Queued( b ) == 2 && queueLive == 2, "grown with queues intact" );
	for ( i = 0; i < 4; i++ ) {
		fresh[i] = Connect( OLD_MAX + 1 + i );
		Check( fresh[i] == OLD_MAX + i, "new clients take the added slots" );
		Donedl( fresh[i], 2 );
		Check( Queued( fresh[i] ) == 2, "new slot queues" );
	}
	Rejoin( a ); Donedl( a, 2 ); Donedl( b, 2 ); Donedl( c, 2 );
	Check( Queued( a ) == 2 && Queued( b ) == QUEUE_CAP && Queued( c ) == 2, "old slots queue after the grow" );
	Check( Drain( a ) == 3 && Drain( b ) == 1 + QUEUE_CAP && Drain( c ) == 3, "old slots sent every message once" );
	for ( i = 0; i < 4; i++ ) Check( Drain( fresh[i] ) == 3, "new slots sent every message once" );
	Check( !queueLive, "nothing left queued" );
	Shutdown();
	puts( "a grow keeps every queue inside the new client array" );
}
int main( int argc, char **argv ) {
	int test = argc > 1 ? atoi( argv[1] ) : -1;
	Setup();
	switch ( test ) {
	case 0: EmptyQueue(); break;
	case 1: FinalMessage(); break;
	case 2: NonEmptyQueue(); break;
	case 3: ShrinkDiscards(); break;
	case 4: Grow(); break;
	default: Check( 0, "usage: server-maxclients-queue-tests <0-4>" );
	}
	return 0;
}
