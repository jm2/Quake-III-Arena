/* Issue #438: no snapshot may be longer than a retail 1.32c client takes.
 * Drives the real SV_SendClientSnapshot (sv_snapshot.c), SV_WriteDownloadToClient
 * (sv_client.c), SV_Netchan_Transmit (sv_net_chan.c) and Netchan_Transmit
 * (net_chan.c) with the real message and Huffman code, for a client
 * downloading at rate 25000 with snaps 2 (7 blocks a snapshot) or snaps 1
 * (8), for a queue of long reliable commands, and for many entities, each
 * swept across a retail client's limit a byte or two at a time.  Every packet
 * goes through retail's Netchan_Process, as the #345 tests model it: the message
 * is reassembled and copied behind the 4 byte sequence number into a heap
 * buffer of exactly MAX_MSGLEN, so it must be MAX_MSGLEN - 4 bytes at most
 * with its svc_EOF.  Retail's CL_Netchan_Decode then decodes it, and each
 * message is compared bit for bit with the one master writes (a copy of
 * master's snapshot and download writers below): a snapshot that fits is
 * master's; one that doesn't holds back only the download blocks that
 * don't fit, or is cleared as an overflow when its reliable commands and
 * entities alone don't.  Full snapshots are also read by retail's parser,
 * and the downloads must complete with the file served, byte for byte. */
#include "../code/server/server.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Retail 1.32c's Netchan_Process (dbe4ddb net_chan.c) checks a reassembled message only against
   MAX_MSGLEN, then copies it behind the 4 byte sequence number into Com_EventLoop's bufData[MAX_MSGLEN]. */
#define RETAIL_MSGLEN	( MAX_MSGLEN - 4 )
#define RETAIL_FRAGMENT_SIZE	( 1400 - 100 )	/* net_chan.c FRAGMENT_SIZE */
#define RETAIL_FRAGMENT_BIT	( 1U << 31 )
#define SERVER_ID 4380
#define DOWNLOAD_PAK "baseq3/mapdl"
#define FILE_BYTES ( 10 * MAX_DOWNLOAD_BLKSIZE + 700 )	/* 11 data blocks and the empty last one */
#define COMMANDS 16			/* long reliable commands that fill a snapshot short of the limit */
#define FAR_ENTITIES 680	/* entities far from their baselines that do */

serverStatic_t svs;
server_t sv;
vm_t *gvm;
static cvar_t zero = { .string = "" }, maxclients = { .string = "8", .integer = 8 };
static cvar_t running = { .string = "1", .integer = 1 }, reconnect = { .string = "3", .integer = 3 };
static cvar_t allowDownload = { .string = "1", .integer = 1 };
cvar_t *sv_maxclients = &maxclients, *sv_reconnectlimit = &reconnect, *sv_floodProtect = &zero;
cvar_t *com_sv_running = &running, *cl_shownet = &zero;
cvar_t *sv_maxRate = &zero, *sv_pure = &zero, *sv_padPackets = &zero, *sv_lanForceRate = &zero;
cvar_t *sv_allowDownload = &allowDownload, *sv_minPing = &zero, *sv_maxPing = &zero, *sv_privateClients = &zero;
cvar_t *sv_privatePassword = &zero, *com_dedicated = &zero, *com_cl_running = &zero;
qboolean com_errorEntered;
extern cvar_t *showpackets, *showdrop;

static sharedEntity_t entities[MAX_GENTITIES];
static playerState_t players[MAX_CLIENTS];
static byte pvs[MAX_MAP_AREA_BYTES], file[FILE_BYTES];
static char words[20 * 1024];	/* configstring text */
static char printed[1 << 16];
static size_t printedLength;
static qboolean fileOpen;
static int filePos;
static const char *scenario = "setup";

/** Stop on the first message a retail client could not take, or that differs from master's. */
static void Check( int ok, const char *message ) {
	if ( !ok ) { fprintf( stderr, "Server snapshot budget regression failed (%s): %s\n", scenario, message ); exit( 1 ); }
}
void QDECL Com_Error( int code, const char *fmt, ... ) {
	char text[1024]; va_list ap;
	va_start( ap, fmt ); vsnprintf( text, sizeof(text), fmt, ap ); va_end( ap );
	fprintf( stderr, "Com_Error(%d): %s\n", code, text ); Check( 0, "engine error" );
}
/** Keep the console, for the overflow warnings. */
void QDECL Com_Printf( const char *fmt, ... ) {
	va_list ap; int n;
	if ( printedLength > sizeof(printed) / 2 ) { printedLength = 0; printed[0] = 0; }
	va_start( ap, fmt ); n = vsnprintf( printed + printedLength, sizeof(printed) - printedLength, fmt, ap ); va_end( ap );
	Check( n >= 0 && (size_t)n < sizeof(printed) - printedLength, "console capture capacity" );
	printedLength += n;
}
void QDECL Com_DPrintf( const char *fmt, ... ) { (void)fmt; }
void Com_Memset( void *dest, const int val, const size_t count ) { memset( dest, val, count ); }
void Com_Memcpy( void *dest, const void *src, const size_t count ) { memcpy( dest, src, count ); }
void *Z_Malloc( int size ) { void *p = calloc( 1, size ); Check( p != NULL, "allocation" ); return p; }
void Z_Free( void *ptr ) { free( ptr ); }
char *CopyString( const char *in ) { return strcpy( Z_Malloc( strlen( in ) + 1 ), in ); }
int Com_HashKey( char *string, int maxlen ) {
	int hash = 0, i;
	for ( i = 0; i < maxlen && string[i]; i++ ) hash += string[i] * ( 119 + i );
	return hash ^ ( hash >> 10 ) ^ ( hash >> 20 );
}
int VM_CallArgs( vm_t *vm, int callNum, const int *args, int argCount ) { (void)vm; (void)callNum; (void)args; (void)argCount; return 0; }
char *VM_CheckedExplicitString( vm_t *vm, int value, qboolean nullable ) { (void)vm; (void)nullable; Check( !value, "denied" ); return NULL; }
void QDECL SV_SendServerCommand( client_t *cl, const char *fmt, ... ) { (void)cl; (void)fmt; }
sharedEntity_t *SV_GentityNum( int num ) { return &entities[num]; }
playerState_t *SV_GameClientNum( int num ) { return &players[num]; }
svEntity_t *SV_SvEntityForGentity( sharedEntity_t *gEnt ) { return &sv.svEntities[gEnt - entities]; }
qboolean Sys_IsLANAddress( netadr_t adr ) { (void)adr; return qfalse; }
int CM_PointLeafnum( const vec3_t p ) { (void)p; return 0; }
int CM_LeafCluster( int leafnum ) { (void)leafnum; return 0; }
int CM_LeafArea( int leafnum ) { (void)leafnum; return 0; }
int CM_WriteAreaBits( byte *buffer, int area ) { (void)area; buffer[0] = 1; return 1; }
byte *CM_ClusterPVS( int cluster ) { (void)cluster; return pvs; }
qboolean CM_AreasConnected( int a, int b ) { (void)a; (void)b; return qfalse; }	/* only SVF_BROADCAST entities are sent */
void SV_BotFreeClient( int clientNum ) { (void)clientNum; }
void SV_Heartbeat_f( void ) {}
void SV_SetUserinfo( int index, const char *val ) { (void)index; (void)val; }
void Cvar_Set( const char *name, const char *value ) { (void)name; (void)value; }
qboolean FS_idPak( char *pak, char *base ) { (void)pak; (void)base; return qfalse; }
int FS_FileIsInPAK( const char *name, int *sum ) { (void)name; (void)sum; return -1; }
/** The one pk3 the level references, served from file[]. */
int FS_SV_FOpenFileRead( const char *name, fileHandle_t *fp ) {
	*fp = 0;
	if ( strcmp( name, DOWNLOAD_PAK ".pk3" ) ) return 0;
	Check( !fileOpen, "one download at a time" );
	fileOpen = qtrue; filePos = 0; *fp = 1;
	return FILE_BYTES;
}
void FS_FCloseFile( fileHandle_t f ) { Check( f == 1 && fileOpen, "closed the download" ); fileOpen = qfalse; }
int FS_Read( void *buffer, int len, fileHandle_t f ) {
	Check( f == 1 && fileOpen, "read the download" );
	if ( len > FILE_BYTES - filePos ) len = FILE_BYTES - filePos;
	memcpy( buffer, file + filePos, len ); filePos += len;
	return len;
}
const char *FS_ReferencedPakNames( void ) { return DOWNLOAD_PAK; }
const char *FS_LoadedPakPureChecksums( void ) { return ""; }
qboolean FS_FilenameCompare( const char *a, const char *b ) { return strcmp( a, b ) != 0; }

/* ---- the retail client: its netchan, CL_Netchan_Decode and CL_ParseServerMessage ---- */

static struct {
	byte		fragmentBuffer[MAX_MSGLEN];	/* retail netchan_t */
	int			fragmentSequence, fragmentLength;
	byte		*message;	/* the last message, reassembled into a MAX_MSGLEN buffer */
	int			length, sequence, messages, largest;
	char		commands[MAX_RELIABLE_COMMANDS][MAX_STRING_CHARS];	/* its reliable commands */
	int			commandSequence, commandsSent;
	int			serverId, serverCommandSequence;
	int			downloadBlock, downloadCount;	/* downloadBlock < 0: not downloading */
	byte		download[FILE_BYTES];
} client;

/** Retail's Netchan_Process: the sequence, then the message, in a buffer of exactly MAX_MSGLEN, so ASan sees an overrun. */
static void Deliver( int sequence, const byte *data, int length ) {
	Check( !client.message, "each message is read before the next" );
	Check( length <= MAX_MSGLEN, "retail Netchan_Process takes the message" );
	Check( 4 + length <= MAX_MSGLEN, "message fits a retail client's buffer behind its sequence number" );
	client.message = malloc( MAX_MSGLEN );
	Check( client.message != NULL, "allocation" );
	*(int *)client.message = LittleLong( sequence );
	memcpy( client.message + 4, data, length );
	client.length = 4 + length; client.sequence = sequence; client.messages++;
	if ( length > client.largest ) client.largest = length;
}
/** Every packet the server sends: reassemble fragments as retail does. */
void Sys_SendPacket( int length, const void *data, netadr_t to ) {
	const byte *p = data; int sequence, start, size; short s;
	(void)to;
	Check( length >= 4, "packet header" );
	memcpy( &sequence, p, 4 ); sequence = LittleLong( sequence );
	if ( sequence == -1 ) return;	/* out of band */
	if ( !( sequence & RETAIL_FRAGMENT_BIT ) ) { Deliver( sequence, p + 4, length - 4 ); return; }
	sequence &= ~RETAIL_FRAGMENT_BIT;
	Check( length >= 8, "fragment header" );
	memcpy( &s, p + 4, 2 ); start = LittleShort( s );
	memcpy( &s, p + 6, 2 ); size = LittleShort( s );
	if ( sequence != client.fragmentSequence ) { client.fragmentSequence = sequence; client.fragmentLength = 0; }
	Check( start == client.fragmentLength, "fragments in order" );
	Check( size >= 0 && 8 + size <= length && client.fragmentLength + size <= (int)sizeof( client.fragmentBuffer ),
	       "retail takes the fragment" );
	memcpy( client.fragmentBuffer + client.fragmentLength, p + 8, size );
	client.fragmentLength += size;
	if ( size == RETAIL_FRAGMENT_SIZE ) return;
	Deliver( sequence, client.fragmentBuffer, client.fragmentLength );
	client.fragmentLength = 0;
}
/** Retail's CL_Netchan_Decode. */
static void Decode( msg_t *msg, int challenge ) {
	int i, index = 0, ack; byte key; const byte *string;
	ack = MSG_ReadLong( msg );
	msg->readcount = 4; msg->bit = 32;
	string = (const byte *)client.commands[ack & ( MAX_RELIABLE_COMMANDS - 1 )];
	key = challenge ^ LittleLong( *(unsigned *)msg->data );
	for ( i = msg->readcount + CL_DECODE_START; i < msg->cursize; i++ ) {
		if ( !string[index] ) index = 0;
		key ^= ( string[index] > 127 || string[index] == '%' ? '.' : string[index] ) << ( i & 1 );
		index++;
		msg->data[i] ^= key;
	}
}
/** A reliable command, as CL_AddReliableCommand queues it. */
static void ClientCommand( const char *text ) {
	client.commandSequence++;
	Q_strncpyz( client.commands[client.commandSequence & ( MAX_RELIABLE_COMMANDS - 1 )], text, MAX_STRING_CHARS );
}
/** Retail's CL_ParseServerMessage over a message of full snapshots; returns the download blocks in it. */
static int Parse( msg_t *msg ) {
	static byte data[MAX_MSGLEN];
	playerState_t ps; entityState_t state; int cmd, seq, block, size, num, blocks = 0;
	MSG_Bitstream( msg );
	MSG_ReadLong( msg );	/* the client commands the server has */
	for ( ;; ) {
		Check( msg->readcount <= msg->cursize, "read past end of server message" );
		cmd = MSG_ReadByte( msg );
		if ( cmd == svc_EOF ) break;
		switch ( cmd ) {
		case svc_serverCommand:
			seq = MSG_ReadLong( msg ); MSG_ReadString( msg );
			if ( seq > client.serverCommandSequence ) client.serverCommandSequence = seq;
			break;
		case svc_snapshot:
			MSG_ReadLong( msg );
			Check( MSG_ReadByte( msg ) == 0, "the parser reads full snapshots only" );
			MSG_ReadByte( msg );
			size = MSG_ReadByte( msg );
			Check( size >= 0 && size <= MAX_MAP_AREA_BYTES, "areabits" );
			MSG_ReadData( msg, data, size );
			MSG_ReadDeltaPlayerstate( msg, NULL, &ps );
			while ( ( num = MSG_ReadBits( msg, GENTITYNUM_BITS ) ) != MAX_GENTITIES - 1 ) {
				Check( msg->readcount <= msg->cursize, "CL_ParsePacketEntities: end of message" );
				MSG_ReadDeltaEntity( msg, &sv.svEntities[num].baseline, &state, num );
			}
			break;
		case svc_download:	/* CL_ParseDownload */
			block = MSG_ReadShort( msg );
			if ( !block ) Check( MSG_ReadLong( msg ) == FILE_BYTES, "block zero carries the file size" );
			size = MSG_ReadShort( msg );
			Check( size >= 0 && size <= MAX_DOWNLOAD_BLKSIZE, "download block size" );
			MSG_ReadData( msg, data, size );
			blocks++;
			if ( block != client.downloadBlock ) break;	/* a resent block: ignored, not acknowledged */
			Check( client.downloadCount + size <= FILE_BYTES, "download no longer than the file" );
			memcpy( client.download + client.downloadCount, data, size );
			client.downloadCount += size;
			ClientCommand( va( "nextdl %d", client.downloadBlock ) );
			client.downloadBlock = size ? client.downloadBlock + 1 : -1;	/* an empty block ends it */
			break;
		default:
			Check( 0, "CL_ParseServerMessage: Illegible server message" );
		}
	}
	Check( msg->readcount <= msg->cursize, "svc_EOF inside the message" );
	return blocks;
}
/** The client's next packet: it acknowledges the message and the server commands, and carries its new commands. */
static void Reply( client_t *cl, qboolean move ) {
	static byte data[MAX_MSGLEN];
	msg_t msg; usercmd_t nullcmd, cmd; int key;
	MSG_Init( &msg, data, sizeof(data) ); MSG_Bitstream( &msg );
	MSG_WriteLong( &msg, client.serverId ); MSG_WriteLong( &msg, client.sequence ); MSG_WriteLong( &msg, client.serverCommandSequence );
	for ( ; client.commandsSent < client.commandSequence; client.commandsSent++ ) {
		MSG_WriteByte( &msg, clc_clientCommand ); MSG_WriteLong( &msg, client.commandsSent + 1 );
		MSG_WriteString( &msg, client.commands[( client.commandsSent + 1 ) & ( MAX_RELIABLE_COMMANDS - 1 )] );
	}
	if ( move ) {
		memset( &nullcmd, 0, sizeof(nullcmd) ); memset( &cmd, 0, sizeof(cmd) ); cmd.serverTime = svs.time;
		key = sv.checksumFeed ^ client.sequence ^
			Com_HashKey( cl->reliableCommands[client.serverCommandSequence & ( MAX_RELIABLE_COMMANDS - 1 )], 32 );
		MSG_WriteByte( &msg, clc_move ); MSG_WriteByte( &msg, 1 ); MSG_WriteDeltaUsercmdKey( &msg, key, &nullcmd, &cmd );
	}
	MSG_WriteByte( &msg, clc_EOF );
	Check( !msg.overflowed, "client packet fits" );
	MSG_BeginReading( &msg ); SV_ExecuteClientMessage( cl, &msg );
}

/* ---- master's writers, to hold every message to ---- */

/** What the server knew before a snapshot: all master's writers depend on. */
typedef struct {
	int				sequence, lastClientCommand, reliableAcknowledge, reliableSequence, deltaMessage;
	int				xmit, clientBlock, sendTime;
	qboolean		rateDelayed, downloading, opening;
	clientState_t	state;
} before_t;

static entityState_t *Ent( clientSnapshot_t *frame, int i ) {
	return &svs.snapshotEntities[( frame->first_entity + i ) % svs.numSnapshotEntities];
}
/** Master's SV_EmitPacketEntities. */
static void EmitEntities( clientSnapshot_t *from, clientSnapshot_t *to, msg_t *msg ) {
	entityState_t *oldent = NULL, *newent = NULL;
	int oldindex = 0, newindex = 0, oldnum, newnum, fromCount = from ? from->num_entities : 0;
	while ( newindex < to->num_entities || oldindex < fromCount ) {
		newnum = newindex < to->num_entities ? ( newent = Ent( to, newindex ) )->number : 9999;
		oldnum = oldindex < fromCount ? ( oldent = Ent( from, oldindex ) )->number : 9999;
		if ( newnum == oldnum ) {
			MSG_WriteDeltaEntity( msg, oldent, newent, qfalse ); oldindex++; newindex++;
		} else if ( newnum < oldnum ) {
			MSG_WriteDeltaEntity( msg, &sv.svEntities[newnum].baseline, newent, qtrue ); newindex++;
		} else {
			MSG_WriteDeltaEntity( msg, oldent, NULL, qtrue ); oldindex++;
		}
	}
	MSG_WriteBits( msg, MAX_GENTITIES - 1, GENTITYNUM_BITS );
}
/** A download block as master's SV_WriteDownloadToClient writes it, from the file. */
static void WriteBlock( msg_t *msg, int block ) {
	int size = FILE_BYTES - block * MAX_DOWNLOAD_BLKSIZE;
	size = size < 0 ? 0 : size > MAX_DOWNLOAD_BLKSIZE ? MAX_DOWNLOAD_BLKSIZE : size;
	MSG_WriteByte( msg, svc_download ); MSG_WriteShort( msg, block );
	if ( !block ) MSG_WriteLong( msg, FILE_BYTES );
	MSG_WriteShort( msg, size );
	if ( size ) MSG_WriteData( msg, file + block * MAX_DOWNLOAD_BLKSIZE, size );
}
/** Master's SV_SendClientSnapshot for the snapshot just sent, up to its svc_EOF, in a buffer with room past
    MAX_MSGLEN, but with only the first maxBlocks of its download blocks; returns how many master sends. */
static int Master( client_t *cl, const before_t *b, int maxBlocks, msg_t *msg ) {
	static byte buffers[2][4 * MAX_MSGLEN];
	static int next;
	clientSnapshot_t *frame = &cl->frames[b->sequence & PACKET_MASK], *oldframe = NULL;
	int i, lastframe = 0, blocks = 0, n, xmit, clientBlock, sendTime;
	MSG_Init( msg, buffers[next++ & 1], sizeof(buffers[0]) );
	MSG_WriteLong( msg, b->lastClientCommand );
	for ( i = b->reliableAcknowledge + 1; i <= b->reliableSequence; i++ ) {
		MSG_WriteByte( msg, svc_serverCommand ); MSG_WriteLong( msg, i );
		MSG_WriteString( msg, cl->reliableCommands[i & ( MAX_RELIABLE_COMMANDS - 1 )] );
	}
	if ( b->deltaMessage > 0 && b->state == CS_ACTIVE && b->sequence - b->deltaMessage < PACKET_BACKUP - 3 ) {
		oldframe = &cl->frames[b->deltaMessage & PACKET_MASK]; lastframe = b->sequence - b->deltaMessage;
		if ( oldframe->first_entity <= svs.nextSnapshotEntities - svs.numSnapshotEntities ) { oldframe = NULL; lastframe = 0; }
	}
	MSG_WriteByte( msg, svc_snapshot ); MSG_WriteLong( msg, svs.time ); MSG_WriteByte( msg, lastframe );
	MSG_WriteByte( msg, svs.snapFlagServerBit | ( b->rateDelayed ? SNAPFLAG_RATE_DELAYED : 0 ) |
	               ( b->state != CS_ACTIVE ? SNAPFLAG_NOT_ACTIVE : 0 ) );
	MSG_WriteByte( msg, frame->areabytes ); MSG_WriteData( msg, frame->areabits, frame->areabytes );
	MSG_WriteDeltaPlayerstate( msg, oldframe ? &oldframe->ps : NULL, &frame->ps );
	EmitEntities( oldframe, frame, msg );
	if ( b->downloading ) {
		n = ( cl->rate * cl->snapshotMsec / 1000 + MAX_DOWNLOAD_BLKSIZE ) / MAX_DOWNLOAD_BLKSIZE;
		xmit = b->opening ? 0 : b->xmit; clientBlock = b->opening ? 0 : b->clientBlock; sendTime = b->sendTime;
		while ( n-- && clientBlock != cl->downloadCurrentBlock ) {
			if ( xmit == cl->downloadCurrentBlock ) {
				if ( svs.time - sendTime <= 1000 ) break;
				xmit = clientBlock;
			}
			if ( blocks < maxBlocks ) WriteBlock( msg, xmit );
			blocks++; xmit++; sendTime = svs.time;
		}
	}
	Check( !msg->overflowed, "master's message measured whole" );
	return blocks;
}
/** Ends msg with the netchan's svc_EOF; returns its size then. */
static int Ended( msg_t *msg ) { MSG_WriteByte( msg, svc_EOF ); return msg->cursize; }
/** The message bits the client got are those of msg. */
static qboolean Same( const msg_t *msg, const byte *data, int length ) {
	int full = msg->bit >> 3, rest = msg->bit & 7;
	return length == msg->cursize && !memcmp( data, msg->data, full ) &&
		( !rest || !( ( data[full] ^ msg->data[full] ) & ( ( 1 << rest ) - 1 ) ) );
}

/* ---- snapshots ---- */

static struct { int snapshots, normal, capped, cleared, retailOverrun, masterOverflow, largestMaster; } seen;

/** Send the rest of a fragment train, as SV_SendClientMessages does. */
static void Drain( client_t *cl ) {
	int guard = 0;
	while ( cl->netchan.unsentFragments ) { SV_Netchan_TransmitNextFragment( cl ); Check( ++guard < 100, "drain" ); }
}
/** One snapshot, the retail client's checks of it and its reply; returns the size of master's message. */
static int Snapshot( client_t *cl, qboolean move ) {
	before_t b; msg_t sent, master; int blocks, masterBlocks, size;
	qboolean full = cl->state != CS_ACTIVE || cl->deltaMessage <= 0;
	size_t mark = printedLength;
	b.sequence = cl->netchan.outgoingSequence; b.lastClientCommand = cl->lastClientCommand;
	b.reliableAcknowledge = cl->reliableAcknowledge; b.reliableSequence = cl->reliableSequence;
	b.deltaMessage = cl->deltaMessage; b.rateDelayed = cl->rateDelayed; b.state = cl->state;
	b.downloading = cl->downloadName[0] != 0; b.opening = !cl->download;
	b.xmit = cl->downloadXmitBlock; b.clientBlock = cl->downloadClientBlock; b.sendTime = cl->downloadSendTime;

	SV_SendClientSnapshot( cl ); Drain( cl );
	Check( client.message && !cl->netchan_start_queue, "one message on the wire" );
	MSG_Init( &sent, client.message, MAX_MSGLEN );
	sent.cursize = client.length; sent.readcount = 4; sent.bit = 32;
	Decode( &sent, cl->challenge );
	seen.snapshots++;

	masterBlocks = Master( cl, &b, 1 << 30, &master );
	size = Ended( &master );
	if ( size > seen.largestMaster ) seen.largestMaster = size;
	if ( size > MAX_MSGLEN ) seen.masterOverflow++;
	else if ( size > RETAIL_MSGLEN ) seen.retailOverrun++;

	if ( client.length == 5 && strstr( printed + mark, "WARNING: msg overflowed for " ) ) {
		/* the overflow path: MSG_Clear, then the netchan's svc_EOF */
		Master( cl, &b, 0, &master );
		Check( Ended( &master ) > RETAIL_MSGLEN, "a snapshot is cleared only when its commands and entities alone are too long" );
		seen.cleared++;
	} else {
		if ( full ) {
			blocks = Parse( &sent );
		} else {	/* master's bit for bit, so it carries the commands */
			Check( !b.downloading, "the delta snapshots are not parsed, so carry no download" );
			blocks = 0; client.serverCommandSequence = b.reliableSequence;
		}
		Check( blocks <= masterBlocks, "no more download blocks than master" );
		Master( cl, &b, blocks, &master ); Ended( &master );
		Check( Same( &master, client.message + 4, client.length - 4 ), "the message is master's, bit for bit" );
		if ( blocks < masterBlocks ) {
			Master( cl, &b, blocks + 1, &master );
			Check( Ended( &master ) > RETAIL_MSGLEN, "a download block is held back only when it doesn't fit" );
			seen.capped++;
		} else {
			Check( size <= RETAIL_MSGLEN, "master's message is sent only when it fits" );
			seen.normal++;
		}
	}
	free( client.message ); client.message = NULL;
	Reply( cl, move );
	return size;
}

/* ---- scenarios ---- */

/** Add a reliable command the way SV_AddServerCommand does. */
static void ServerCommand( client_t *cl, const char *text ) {
	cl->reliableSequence++;
	Q_strncpyz( cl->reliableCommands[cl->reliableSequence & ( MAX_RELIABLE_COMMANDS - 1 )], text, MAX_STRING_CHARS );
}
/** A command of the given length in a char whose code is a byte or less, so each char adds a byte at most. */
static const char *Padding( int length ) {
	static char command[MAX_STRING_CHARS];
	Check( length < MAX_STRING_CHARS - 16, "padding length" );
	memcpy( command, "print \"", 7 ); memset( command + 7, 'e', length );
	command[7 + length] = '"'; command[8 + length] = 0;
	return command;
}
/** A client through SV_DirectConnect, with its gamestate. */
static client_t *Connect( int snaps ) {
	netadr_t from; char text[MAX_STRING_CHARS]; client_t *cl;
	svs.time += 5000;	/* past sv_reconnectlimit */
	memset( &from, 0, sizeof(from) ); from.type = NA_IP; from.ip[0] = 10; from.ip[3] = 38; from.port = 27960;
	svs.challenges[0].adr = from; svs.challenges[0].challenge = 438;
	Com_sprintf( text, sizeof(text), "connect \"\\protocol\\%i\\challenge\\438\\qport\\438\\name\\retail\\rate\\25000\\snaps\\%i\"",
	             PROTOCOL_VERSION, snaps );
	Cmd_TokenizeString( text ); SV_DirectConnect( from );
	cl = &svs.clients[0];
	Check( cl->state == CS_CONNECTED && cl->rate == 25000 && cl->snapshotMsec == 1000 / snaps, "connected at the rate" );
	memset( &client, 0, sizeof(client) ); client.downloadBlock = -1;
	Reply( cl, qfalse );	/* the first packet, of no serverId yet, brings the gamestate */
	Drain( cl );
	Check( cl->state == CS_PRIMED && client.messages == 1, "gamestate sent" );
	free( client.message ); client.message = NULL;
	client.serverId = sv.serverId; client.serverCommandSequence = cl->reliableSequence;
	return cl;
}
/** Download the pk3, with `padding` chars of reliable command in the first snapshot; returns master's
    size of that snapshot. The download must end with the file, byte for byte. */
static int Download( client_t *cl, int padding ) {
	int guard = 0, size;
	ClientCommand( "download " DOWNLOAD_PAK ".pk3" );
	client.downloadBlock = 0; client.downloadCount = 0;
	Reply( cl, qfalse );
	if ( padding >= 0 ) ServerCommand( cl, Padding( padding ) );
	size = Snapshot( cl, qfalse );
	Check( client.downloadBlock > 0, "the first snapshot carries blocks" );
	while ( client.downloadBlock >= 0 ) {
		svs.time += cl->snapshotMsec;
		Snapshot( cl, qfalse );
		Check( ++guard < 50, "download completes" );
	}
	Check( client.downloadCount == FILE_BYTES && !memcmp( client.download, file, FILE_BYTES ), "the file arrives whole" );
	Check( !cl->downloadName[0] && !fileOpen, "the server finished the download" );
	return size;
}
static void Report( const char *what ) {
	printf( "%s: %d snapshots, %d as master's, %d with blocks held back, %d cleared; "
	        "master overruns retail %d times, overflows %d, largest message %d, largest sent %d\n",
	        what, seen.snapshots, seen.normal, seen.capped, seen.cleared,
	        seen.retailOverrun, seen.masterOverflow, seen.largestMaster, client.largest );
}
/** Rate 25000 and snaps 2 put 7 blocks in a snapshot; sweep a reliable command in the first one across the limit. */
static void DownloadSweep( void ) {
	client_t *cl = Connect( 2 ); int padding, size;
	scenario = "download at snaps 2";
	for ( padding = 0; ; padding++ ) {
		size = Download( cl, padding );
		Check( padding || size <= RETAIL_MSGLEN, "the sweep starts below the limit" );
		if ( size > MAX_MSGLEN ) break;
	}
	Check( seen.capped && seen.retailOverrun && !seen.cleared, "blocks held back where master overruns retail" );
	Check( client.largest >= RETAIL_MSGLEN - 1, "held back only what doesn't fit" );
	Report( scenario );
}
/** Snaps 1 would put 8 blocks in a snapshot, more than a message holds: master clears it and loses them. */
static void DownloadEight( void ) {
	client_t *cl = Connect( 1 );
	scenario = "download at snaps 1";
	Download( cl, -1 );
	Check( seen.capped && seen.masterOverflow && !seen.cleared, "blocks held back where master overflows" );
	Report( scenario );
}
/** An active client that acknowledged everything, with full snapshots. */
static client_t *Active( void ) {
	client_t *cl = Connect( 20 );
	Reply( cl, qtrue );
	Check( cl->state == CS_ACTIVE, "active" );
	return cl;
}
/** Long reliable commands (a burst of configstrings) and one more, a char longer each time; the rest of
    each snapshot stays the same, so the sweep takes every size. */
static void CommandSweep( void ) {
	client_t *cl = Active(); int base = cl->reliableSequence, padding, size, i;
	scenario = "reliable commands";
	for ( padding = 0; ; padding++ ) {
		cl->reliableAcknowledge = cl->reliableSequence = client.serverCommandSequence = base;
		for ( i = 0; i < COMMANDS; i++ ) ServerCommand( cl, va( "cs %i \"%.960s\"", 700 + i, words + 960 * i ) );
		ServerCommand( cl, Padding( padding ) );
		cl->deltaMessage = -1;
		size = Snapshot( cl, qfalse );
		Check( padding || size <= RETAIL_MSGLEN, "the sweep starts below the limit" );
		if ( size > MAX_MSGLEN ) break;
	}
	Check( seen.retailOverrun && seen.cleared && !seen.capped, "cleared where master overruns retail" );
	Check( client.largest == RETAIL_MSGLEN, "a message of the limit exactly is sent" );
	Report( scenario );
}
/** Many entities: some far from their baselines, then more at them, one more each time. */
static void EntitySweep( void ) {
	client_t *cl = Active(); int far = FAR_ENTITIES, near, size, i;
	scenario = "entities";
	for ( i = 1; i < MAX_GENTITIES - 1; i++ ) {
		entities[i].r.linked = qtrue; entities[i].r.svFlags = SVF_BROADCAST;
		entities[i].s.number = i; entities[i].s.eType = ET_GENERAL;
		entities[i].s.pos.trBase[0] = i * 1.37f; entities[i].s.pos.trBase[1] = i * -2.11f; entities[i].s.pos.trBase[2] = 0.5f;
		entities[i].s.apos.trBase[1] = i * 0.73f; entities[i].s.modelindex = i & 255;
		memset( &sv.svEntities[i].baseline, 0, sizeof(entityState_t) );
		if ( i > far ) sv.svEntities[i].baseline = entities[i].s;
	}
	for ( near = 0; ; near++ ) {
		Check( far + near < MAX_GENTITIES - 1, "entities enough to cross the limit" );
		sv.num_entities = far + near + 1;
		cl->deltaMessage = -1;
		size = Snapshot( cl, qfalse );
		Check( near || size <= RETAIL_MSGLEN, "the sweep starts below the limit" );
		if ( size > MAX_MSGLEN ) break;
	}
	Check( seen.retailOverrun && seen.cleared && !seen.capped, "cleared where master overruns retail" );
	Check( client.largest >= RETAIL_MSGLEN - 2, "cleared only what doesn't fit" );
	Report( scenario );
}
/** Normal play: delta snapshots of a few moving entities with the odd command, and a download at snaps 20. */
static void Normal( void ) {
	client_t *cl = Active(); int frame, i;
	scenario = "normal play";
	sv.num_entities = 40;
	for ( i = 1; i < sv.num_entities; i++ ) {
		entities[i].r.linked = qtrue; entities[i].r.svFlags = i % 5 ? SVF_BROADCAST : 0;
		entities[i].s.number = i; entities[i].s.eType = ET_GENERAL + i % 3; entities[i].s.modelindex = i;
	}
	for ( frame = 0; frame < 60; frame++ ) {
		for ( i = 1; i < sv.num_entities; i++ ) {
			entities[i].s.pos.trBase[0] = (float)( ( i * 7 + frame * ( i % 4 ) ) % 300 );
			entities[i].r.linked = ( i + frame / 10 ) % 7 != 0;
		}
		players[0].origin[0] = frame * 3.5f; players[0].commandTime = svs.time;
		if ( frame % 9 == 4 ) ServerCommand( cl, va( "cs %i \"frame %i\"", 300 + frame % 16, frame ) );
		svs.time += 50;
		Snapshot( cl, qtrue );
	}
	Check( seen.normal == seen.snapshots && !seen.capped && !seen.cleared, "every snapshot is master's" );
	cl = Connect( 20 );
	Download( cl, 0 );
	Check( seen.normal == seen.snapshots && !seen.capped && !seen.cleared, "every download snapshot is master's" );
	Report( scenario );
}

int main( int argc, char **argv ) {
	int test = argc > 1 ? atoi( argv[1] ) : -1, i; unsigned seed = 438;
	showpackets = showdrop = &zero;
	svs.clients = Z_Malloc( 8 * sizeof(client_t) ); svs.clientCapacity = 8; svs.time = 100000;
	svs.numSnapshotEntities = PACKET_BACKUP * MAX_GENTITIES; svs.snapshotEntities = Z_Malloc( svs.numSnapshotEntities * sizeof(entityState_t) );
	sv.state = SS_GAME; sv.serverId = sv.restartedServerId = SERVER_ID; sv.checksumFeed = 0x4380438;
	for ( i = 0; i < MAX_CONFIGSTRINGS; i++ ) sv.configstrings[i] = CopyString( i < 4 ? "configstring" : "" );
	/* a pk3 is deflated: its bytes are as good as random */
	for ( i = 0; i < FILE_BYTES; i++ ) { seed = seed * 1103515245u + 12345u; file[i] = seed >> 23; }
	for ( i = 0; i < (int)sizeof(words) - 1; i++ ) { seed = seed * 1103515245u + 12345u; words[i] = "abcdefghijklmnopqrstuvwxyz/_0123456789"[( seed >> 16 ) % 38]; }
	{	/* the padding char's Huffman code is a byte at most, so the command sweep takes every size */
		msg_t msg; byte data[16];
		MSG_Init( &msg, data, sizeof(data) ); MSG_WriteByte( &msg, 'e' );
		Check( msg.bit <= 8, "padding char code length" );
	}
	switch ( test ) {
	case 0: Normal(); break;
	case 1: DownloadSweep(); break;
	case 2: DownloadEight(); break;
	case 3: CommandSweep(); break;
	case 4: EntitySweep(); break;
	default: Check( 0, "usage: server-snapshot-budget-tests <0-4>" );
	}
	return 0;
}
