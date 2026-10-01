/* Issue #37: ioquake3's protocol 71 binds connection setup and every sequenced packet to the negotiated
 * challenge, and protocol 68 stays retail 1.32c's, byte for byte.  The real client (cl_main.c, included
 * here, and cl_net_chan.c), the real server (sv_main.c, sv_client.c and sv_net_chan.c; see
 * protocol71_server.c) and the real netchan, message, Huffman and command code talk to each other and to
 * models of a retail 1.32c peer (dbe4ddb's netchan header and XOR encoding, below) and of ioquake3 and
 * Quake3e servers (their challengeResponse and connectResponse text, and packets ioquake3's own
 * net_chan.c wrote).  Usage: protocol71_regression <case>, for the cases in main(). */
#define SV_Frame Test_SV_Frame			/* CL_Connect_f kills a local server; there is none */
#define SV_Shutdown Test_SV_Shutdown
#include "../code/client/cl_main.c"
#undef SV_Frame
#undef SV_Shutdown
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "protocol71_fixture.h"

#define OOB				"\xff\xff\xff\xff"
#define PACKETLEN		1400		/* net_chan.c MAX_PACKETLEN */
#define FRAGMENTLEN		1300		/* net_chan.c FRAGMENT_SIZE */
#define FRAGMENTED		( 1U << 31 )	/* net_chan.c FRAGMENT_BIT */
#define QPORT			0x6d38
#define USERINFO		"\\name\\Visor\\rate\\25000\\snaps\\20"
#define WIRE			64

static const char *scenario = "setup";
static netadr_t server, client, attacker, serverOtherPort, clientOtherPort, proxy, wan;
static cvar_t protocol, quiet, one = { .string = "1", .integer = 1 }, qportCvar = { .integer = QPORT };
static cvar_t timeout = { .string = "200", .value = 200, .integer = 200 };
static int now;

/** Stop on the first broken property. */
static void Check( int ok, const char *message ) {
	if ( !ok ) { fprintf( stderr, "Protocol 71 regression failed (%s): %s\n", scenario, message ); exit( 1 ); }
}

/* ---- the rest of the engine ---- */

cvar_t *com_protocol = &protocol, *com_cl_running = &one, *com_sv_running = &quiet, *com_dedicated = &quiet;
cvar_t *cl_shownet = &quiet, *cl_paused = &quiet, *sv_paused = &quiet, *com_version = &quiet;
int cvar_modifiedFlags;
char cl_cdkey[34];
int cl_connectedToPureServer;
vm_t *uivm;
void QDECL Com_Error( int level, const char *format, ... ) { (void)level; (void)format; Check( 0, "engine error" ); }
void QDECL Com_Printf( const char *format, ... ) { (void)format; }
void QDECL Com_DPrintf( const char *format, ... ) { (void)format; }
void Com_Memcpy( void *out, const void *in, size_t size ) { memcpy( out, in, size ); }
void Com_Memset( void *out, int value, size_t size ) { memset( out, value, size ); }
void *Z_Malloc( int size ) { void *p = calloc( 1, size ); Check( p != NULL, "allocation" ); return p; }
void Z_Free( void *p ) { free( p ); }
char *CopyString( const char *in ) { return strcpy( Z_Malloc( strlen( in ) + 1 ), in ); }
int Com_HashKey( char *string, int maxlen ) {
	int hash = 0, i;
	for ( i = 0; i < maxlen && string[i]; i++ ) hash += string[i] * ( 119 + i );
	return hash ^ ( hash >> 10 ) ^ ( hash >> 20 );
}
int Sys_Milliseconds( void ) { return now; }
int Com_Milliseconds( void ) { return now; }
/** A clock since startup that has moved on by the next call. */
unsigned Sys_Entropy( void ) { static unsigned micros = 0x3c6ef372; return micros += 0x2545f491; }
/** Not single player, no fs_restrict; the client's qport. */
float Cvar_VariableValue( const char *name ) { return !strcmp( name, "net_qport" ) ? QPORT : 0; }
char *Cvar_VariableString( const char *name ) { (void)name; return ""; }
cvar_t *Cvar_Get( const char *name, const char *value, int flags ) {
	(void)value; (void)flags;
	return !strcmp( name, "net_qport" ) ? &qportCvar : &quiet;
}
void Cvar_Set( const char *name, const char *value ) { (void)name; (void)value; }
char *Cvar_InfoString( int bit ) {
	static char info[MAX_INFO_STRING];
	Check( bit == CVAR_USERINFO, "the userinfo" );
	return strcpy( info, USERINFO );
}
qboolean Cvar_Command( void ) { Check( 0, "rcon" ); return qfalse; }
qboolean CL_GameCommand( void ) { Check( 0, "client game command" ); return qfalse; }
qboolean UI_GameCommand( void ) { Check( 0, "UI command" ); return qfalse; }
void Com_BeginRedirect( char *buffer, int buffersize, void ( *flush )( char * ) ) { (void)buffer; (void)buffersize; (void)flush; Check( 0, "rcon" ); }
void Com_EndRedirect( void ) { Check( 0, "rcon" ); }
/** Every address but `wan` is on the LAN: challenges are answered at once, and no CD key is authorized. */
qboolean Sys_IsLANAddress( netadr_t adr ) { return !NET_CompareBaseAdr( adr, wan ); }
qboolean Sys_StringToAdr( const char *name, netadr_t *address ) {
	int a, b, c, d;
	memset( address, 0, sizeof( *address ) );
	if ( sscanf( name, "%d.%d.%d.%d", &a, &b, &c, &d ) != 4 ) return qfalse;
	address->type = NA_IP; address->ip[0] = a; address->ip[1] = b; address->ip[2] = c; address->ip[3] = d;
	return qtrue;
}
void Test_SV_Frame( int msec ) { (void)msec; }
void Test_SV_Shutdown( char *finalmsg ) { (void)finalmsg; Check( 0, "local server shut down" ); }
void Con_Close( void ) {}
void SCR_StopCinematic( void ) {}
void S_ClearSoundBuffer( void ) {}
void CL_WritePacket( void ) {}	/* the disconnect CL_Disconnect sends */
static byte demo[MAX_MSGLEN + 8];
static int demoLength;
/** The demo CL_WriteDemoMessage records. */
int FS_Write( const void *buffer, int length, fileHandle_t f ) {
	Check( f == 1 && length >= 0 && demoLength + length <= (int)sizeof( demo ), "demo file" );
	memcpy( demo + demoLength, buffer, length ); demoLength += length;
	return length;
}
void FS_FCloseFile( fileHandle_t f ) { (void)f; Check( 0, "file" ); }

/* ---- the wire between them ---- */

typedef struct { netadr_t to; int length; byte data[MAX_MSGLEN]; } packet_t;
static packet_t wire[WIRE];
static int wireCount, wireRead;

/** Every datagram either side sends. */
void Sys_SendPacket( int length, const void *data, netadr_t to ) {
	Check( wireCount < WIRE, "wire capacity" );
	Check( length > 0 && length <= (int)sizeof( wire[0].data ), "datagram length" );
	Check( length <= PACKETLEN || !memcmp( data, OOB, 4 ), "sequenced datagram within MAX_PACKETLEN" );
	wire[wireCount].to = to; wire[wireCount].length = length; memcpy( wire[wireCount].data, data, length );
	wireCount++;
}
/** The next datagram sent, which must be to `to`. */
static packet_t *Next( netadr_t to ) {
	Check( wireRead < wireCount, "a datagram was sent" );
	Check( NET_CompareAdr( wire[wireRead].to, to ), "datagram destination" );
	return &wire[wireRead++];
}
/** Nothing more was sent. */
static int Quiet( void ) { return wireRead == wireCount; }
static void Clear( void ) { wireRead = wireCount = 0; }
static const char *Text( const packet_t *p ) {
	static char text[MAX_MSGLEN];
	Check( p->length >= 4 && !memcmp( p->data, OOB, 4 ), "out of band datagram" );
	memcpy( text, p->data + 4, p->length - 4 ); text[p->length - 4] = 0;
	return text;
}
/** A connect datagram, Huffman decoded as SV_ConnectionlessPacket does. */
static const char *ConnectText( const packet_t *p ) {
	static byte data[MAX_MSGLEN];
	msg_t msg;
	Check( p->length > 12 && !memcmp( p->data, OOB "connect", 11 ), "connect datagram" );
	MSG_Init( &msg, data, sizeof( data ) ); memcpy( data, p->data, p->length ); msg.cursize = p->length;
	Huff_Decompress( &msg, 12 );
	Check( msg.cursize < (int)sizeof( data ), "connect text length" );
	data[msg.cursize] = 0;
	return (const char *)data + 4;
}

static byte bufData[MAX_MSGLEN_BUF];	/* Com_EventLoop's */
static void Deliver( qboolean toServer, netadr_t from, const byte *data, int length ) {
	msg_t msg;
	Check( length <= MAX_MSGLEN, "datagram fits Com_EventLoop's buffer" );
	MSG_Init( &msg, bufData, sizeof( bufData ) ); memcpy( bufData, data, length ); msg.cursize = length;
	now += 250;	/* well inside the rate limits */
	if ( toServer ) SV_PacketEvent( from, &msg ); else CL_PacketEvent( from, &msg );
}
static void ToClient( netadr_t from, const byte *data, int length ) { Deliver( qfalse, from, data, length ); }
static void ToServer( netadr_t from, const byte *data, int length ) { Deliver( qtrue, from, data, length ); }
static void OutOfBand( qboolean toServer, netadr_t from, const char *text ) {
	byte data[MAX_MSGLEN];
	memcpy( data, OOB, 4 ); memcpy( data + 4, text, strlen( text ) );
	Deliver( toServer, from, data, 4 + (int)strlen( text ) );
}
static void ClientGets( netadr_t from, const char *text ) { OutOfBand( qfalse, from, text ); }
static void ServerGets( netadr_t from, const char *text ) { OutOfBand( qtrue, from, text ); }
/** A connect datagram as a client's NET_OutOfBandData sends it, Huffman coded after the 12 byte header. */
static void ServerGetsConnect( netadr_t from, const char *userinfo ) {
	byte data[MAX_MSGLEN];
	msg_t msg;
	MSG_Init( &msg, data, sizeof( data ) );
	Com_sprintf( (char *)data, sizeof( data ), OOB "connect \"%s\"", userinfo );
	msg.cursize = strlen( (char *)data );
	Huff_Compress( &msg, 12 );
	ToServer( from, data, msg.cursize );
}
/** Pass the next datagram on. */
static void ClientToServer( void ) { packet_t *p = Next( server ); ToServer( client, p->data, p->length ); }
static void ServerToClient( void ) { packet_t *p = Next( client ); ToClient( server, p->data, p->length ); }

/* ---- what the client parses and the server executes ---- */

static byte parsed[MAX_MSGLEN], executed[MAX_MSGLEN];
static int parsedLength = -1, parsedReadcount;

void CL_ParseServerMessage( msg_t *msg ) {
	Check( msg->readcount <= msg->cursize && msg->cursize <= MAX_MSGLEN, "parsed message bounds" );
	parsedReadcount = msg->readcount; parsedLength = msg->cursize - msg->readcount;
	memcpy( parsed, msg->data + msg->readcount, parsedLength );
}
/** The client parsed exactly `data`, from `readcount` on. */
static int Parsed( const byte *data, int length, int readcount ) {
	int ok = parsedLength == length && parsedReadcount == readcount && !memcmp( parsed, data, length );
	parsedLength = -1;
	return ok;
}
static int NothingParsed( void ) { return parsedLength < 0; }
/** The server's client 0 executed exactly `data`, from `readcount` on. */
static int Executed( const byte *data, int length, int readcount ) {
	int n = -1, at = -1, got = Server_TakeExecuted( &n, &at, executed );
	return got == length && n == 0 && at == readcount && !memcmp( executed, data, length );
}
static int NothingExecuted( void ) { int n, at; return Server_TakeExecuted( &n, &at, executed ) < 0; }

/* ---- messages, as the engine writes them ---- */

typedef struct { msg_t msg; byte data[MAX_MSGLEN]; } message_t;

/** CL_WritePacket's start: serverId, messageAcknowledge and reliableAcknowledge, then a command. */
static void ClientMessage( message_t *m, int serverId, int messageAck, int reliableAck, const char *command ) {
	MSG_Init( &m->msg, m->data, sizeof( m->data ) ); MSG_Bitstream( &m->msg );
	MSG_WriteLong( &m->msg, serverId ); MSG_WriteLong( &m->msg, messageAck ); MSG_WriteLong( &m->msg, reliableAck );
	MSG_WriteByte( &m->msg, clc_clientCommand ); MSG_WriteLong( &m->msg, reliableAck + 1 ); MSG_WriteString( &m->msg, command );
}
/** A server message: the client commands it has, a server command and `pad` more bytes. */
static void ServerMessage( message_t *m, int reliableAck, const char *command, int pad ) {
	int i;
	MSG_Init( &m->msg, m->data, sizeof( m->data ) ); MSG_Bitstream( &m->msg );
	MSG_WriteLong( &m->msg, reliableAck );
	MSG_WriteByte( &m->msg, svc_serverCommand ); MSG_WriteLong( &m->msg, 2 ); MSG_WriteString( &m->msg, command );
	for ( i = 0; i < pad; i++ ) MSG_WriteByte( &m->msg, ( i * 37 + 11 ) & 255 );
	Check( !m->msg.overflowed, "message fits" );
}
/** The message with the svc_EOF or clc_EOF the netchan adds: what the other side must get. */
static int WithEOF( const message_t *m, int eof, byte *out ) {
	message_t copy = *m;
	copy.msg.data = copy.data;
	MSG_WriteByte( &copy.msg, eof );
	memcpy( out, copy.data, copy.msg.cursize );
	return copy.msg.cursize;
}

/* ---- a retail 1.32c peer: dbe4ddb's net_chan.c header and cl_net_chan.c / sv_net_chan.c XOR ---- */

static void PutLong( byte *p, unsigned v ) { p[0] = v & 255; p[1] = ( v >> 8 ) & 255; p[2] = ( v >> 16 ) & 255; p[3] = v >> 24; }
static void PutShort( byte *p, unsigned v ) { p[0] = v & 255; p[1] = ( v >> 8 ) & 255; }
static unsigned GetLong( const byte *p ) { return p[0] | p[1] << 8 | p[2] << 16 | (unsigned)p[3] << 24; }
/** The XOR of CL_Netchan_Encode, SV_Netchan_Decode, SV_Netchan_Encode and CL_Netchan_Decode alike. */
static void RetailXor( byte *data, int start, int end, byte key, const char *command ) {
	byte string[MAX_STRING_CHARS] = { 0 };	/* as the engine's command arrays, zero after the text */
	int i, index = 0;
	Q_strncpyz( (char *)string, command, sizeof( string ) );
	for ( i = start; i < end; i++ ) {
		if ( !string[index] ) index = 0;
		key ^= ( string[index] > 127 || string[index] == '%' ? '.' : string[index] ) << ( i & 1 );
		index++;
		data[i] ^= key;
	}
}
/** A retail client's datagram: CL_Netchan_Encode, keyed by the challenge, the serverId and the
 * messageAcknowledge, and by the server command it acknowledges; then Netchan_Transmit's header. */
static int RetailClientPacket( byte *out, int sequence, int challenge, int serverId, int messageAck,
	const char *serverCommand, const byte *data, int length ) {
	Check( length < FRAGMENTLEN, "one datagram" );
	PutLong( out, sequence ); PutShort( out + 4, QPORT ); memcpy( out + 6, data, length );
	RetailXor( out + 6, 12, length, (byte)( challenge ^ serverId ^ messageAck ), serverCommand );
	return 6 + length;
}
/** A retail server's message: SV_Netchan_Encode, keyed by the challenge, the sequence and the client's
 * last command; out holds it whole, still to be split into datagrams. */
static void RetailServerEncode( byte *out, int sequence, int challenge, const char *clientCommand, const byte *data, int length ) {
	memcpy( out, data, length );
	RetailXor( out, 4, length, (byte)( challenge ^ sequence ), clientCommand );
}
/** Retail Netchan_Transmit and Netchan_TransmitNextFragment's datagram `index` for a server message. */
static int RetailServerDatagram( byte *out, int sequence, const byte *encoded, int length, int index ) {
	int start = index * FRAGMENTLEN, size;
	if ( length < FRAGMENTLEN ) {
		Check( !index, "one datagram" );
		PutLong( out, sequence ); memcpy( out + 4, encoded, length );
		return 4 + length;
	}
	size = length - start < FRAGMENTLEN ? length - start : FRAGMENTLEN;
	Check( size >= 0, "fragment index" );
	PutLong( out, sequence | FRAGMENTED ); PutShort( out + 4, start ); PutShort( out + 6, size );
	memcpy( out + 8, encoded + start, size );
	return 8 + size;
}
static int Datagrams( int length ) { return length < FRAGMENTLEN ? 1 : length / FRAGMENTLEN + 1; }

/** The demo record of a message: its sequence, its length and the message, without the netchan header, so
 * that a demo is retail's dm_68 whatever the protocol. */
static int Demo( int sequence, const byte *data, int length ) {
	int ok = demoLength == 8 + length && GetLong( demo ) == (unsigned)sequence && GetLong( demo + 4 ) == (unsigned)length
		&& !memcmp( demo + 8, data, length );
	demoLength = 0;
	return ok;
}

/* ---- ioquake3's protocol 71 ---- */

/** NETCHAN_GENCHECKSUM, as ioquake3 computes it with a 32 bit wrapping multiply. */
static unsigned Checksum( int challenge, int sequence ) {
	return (unsigned)( (unsigned long long)(unsigned)challenge ^ ( (unsigned long long)(unsigned)sequence * (unsigned)challenge ) );
}
/** A protocol 71 datagram: sequence, the client's qport, the checksum, then the message (not XOR encoded). */
static int Packet71( byte *out, int sequence, int challenge, qboolean fromClient, const byte *data, int length ) {
	int at = 0;
	PutLong( out, sequence ); at = 4;
	if ( fromClient ) { PutShort( out + at, QPORT ); at += 2; }
	PutLong( out + at, Checksum( challenge, sequence ) ); at += 4;
	memcpy( out + at, data, length );
	return at + length;
}

/* ---- connection setup ---- */

/** The console's "connect <address>", through the real CL_Connect_f. */
static void ClientConnect( const char *address ) {
	Cmd_TokenizeString( va( "connect %s", address ) );
	CL_Connect_f();
	Check( cls.state == CA_CONNECTING && NET_CompareAdr( clc.serverAddress, server ), "connecting" );
	Check( !clc.compat, "a new connection starts out offering protocol 71" );
}
/** CL_CheckForResend, three seconds on. */
static void Resend( void ) { cls.realtime += RETRANSMIT_TIMEOUT; CL_CheckForResend(); }
/** The connect the client sends: its protocol, and the challenge it echoes. */
static const char *Connecting( int *version, int *challenge ) {
	static char info[MAX_INFO_STRING];
	const char *text;
	Resend();
	text = ConnectText( Next( clc.serverAddress ) );
	Check( text[8] == '"' && strlen( text ) < sizeof( info ) + 10, "connect format" );
	Q_strncpyz( info, text + 9, sizeof( info ) );
	Check( info[0] && info[strlen( info ) - 1] == '"', "quoted userinfo" );
	info[strlen( info ) - 1] = 0;
	*version = atoi( Info_ValueForKey( info, "protocol" ) );
	*challenge = atoi( Info_ValueForKey( info, "challenge" ) );
	Check( atoi( Info_ValueForKey( info, "qport" ) ) == QPORT, "connect qport" );
	return info;
}
/** Master's connect text: CL_CheckForResend's userinfo with protocol 68, the qport and the challenge. */
static const char *RetailConnect( int challenge ) {
	static char info[MAX_INFO_STRING];
	strcpy( info, USERINFO );
	Info_SetValueForKey( info, "protocol", "68" );
	Info_SetValueForKey( info, "qport", va( "%i", QPORT ) );
	Info_SetValueForKey( info, "challenge", va( "%i", challenge ) );
	return info;
}
/** A fresh client, server and wire. */
static void Start( const char *name ) {
	scenario = name;
	Clear();
	Server_Init();
	protocol.integer = PROTOCOL_CHECKSUM_VERSION;
	cls.state = CA_DISCONNECTED; cls.realtime = 1000;
	CL_Disconnect( qfalse );
	parsedLength = -1;
}
/** This client connects to this server, through the real handshake both ways; returns the protocol it chose. */
static int ConnectBoth( void ) {
	int version, challenge, serverChallenge;
	qboolean refused;
	ClientConnect( "192.0.2.10:27960" );
	Resend(); ClientToServer();
	Check( Server_Challenge( client, &serverChallenge, &refused ) && !refused, "challenge record" );
	Check( !strcmp( Text( &wire[wireRead] ), va( "challengeResponse %i %i %i", serverChallenge, clc.challenge,
		protocol.integer ) ), "challengeResponse echoes the client's challenge and names com_protocol" );
	ServerToClient();
	Check( cls.state == CA_CHALLENGING && clc.challenge == serverChallenge, "challenge taken" );
	Connecting( &version, &challenge );
	Check( challenge == serverChallenge, "connect carries the server's challenge" );
	ToServer( client, wire[wireRead - 1].data, wire[wireRead - 1].length );
	Check( Server_Connected( 0 ) && Server_ClientChallenge( 0 ) == serverChallenge, "server took the connect" );
	return version;
}

/* ---- the cases ---- */

/** ioquake3 main's net_chan.c (83a7762) wrote these for the same calls; this netchan must, byte for byte. */
static void IoquakePackets( void ) {
	static const byte client1[] = { 1, 0, 0, 0, 0x38, 0x6d, 0, 0, 0, 0, 1, 8, 15, 22, 29 };
	static const byte client2[] = { 2, 0, 0, 0, 0x38, 0x6d, 0x88, 0xfa, 0x5c, 0x36, 1, 8, 15, 22, 29 };
	static const byte server1[] = { 0xf0, 0xff, 0xff, 0x7f, 0x0e, 0xd0, 0x09, 0x2f, 1, 8, 15 };
	static const byte first[] = { 0x4d, 0, 0, 0x80, 0x1c, 0xe5, 0x98, 0x08, 0, 0, 0x14, 0x05 };
	static const byte last[] = { 0x4d, 0, 0, 0x80, 0x1c, 0xe5, 0x98, 0x08, 0x14, 0x05, 0xc8, 0 };
	static const byte retail[] = { 1, 0, 0, 0, 0x38, 0x6d, 1, 8, 15, 22, 29 };
	static byte payload[MAX_MSGLEN], *exact;
	netchan_t chan, receiver;
	msg_t msg;
	packet_t *p;
	int i, length;

	scenario = "ioquake3's packets";
	Clear();
	Netchan_Init( QPORT );
	for ( i = 0; i < (int)sizeof( payload ); i++ ) payload[i] = (byte)( i * 7 + 1 );
	Netchan_Setup( NS_CLIENT, &chan, server, QPORT, 0x12345678, qfalse );
	Netchan_Transmit( &chan, 5, payload ); p = Next( server );
	Check( p->length == sizeof( client1 ) && !memcmp( p->data, client1, sizeof( client1 ) ), "client sequence 1" );
	Netchan_Transmit( &chan, 5, payload ); p = Next( server );
	Check( p->length == sizeof( client2 ) && !memcmp( p->data, client2, sizeof( client2 ) ), "client sequence 2" );
	Netchan_Setup( NS_SERVER, &chan, client, 0, -1234567890, qfalse );
	chan.outgoingSequence = 0x7ffffff0;
	Netchan_Transmit( &chan, 3, payload ); p = Next( client );
	Check( p->length == sizeof( server1 ) && !memcmp( p->data, server1, sizeof( server1 ) ), "server sequence 0x7ffffff0" );
	Netchan_Setup( NS_SERVER, &chan, client, 0, (int)0x9e3779b9, qfalse );
	chan.outgoingSequence = 77;
	Netchan_Transmit( &chan, 1500, payload ); p = Next( client );
	Check( p->length == 1312 && !memcmp( p->data, first, 12 ) && !memcmp( p->data + 12, payload, 1300 ), "first fragment" );
	Netchan_TransmitNextFragment( &chan ); p = Next( client );
	Check( p->length == 212 && !memcmp( p->data, last, 12 ) && !memcmp( p->data + 12, payload + 1300, 200 ), "last fragment" );
	Check( !chan.unsentFragments && chan.outgoingSequence == 78, "fragments sent" );
	Netchan_Setup( NS_CLIENT, &chan, server, QPORT, 0x12345678, qtrue );
	Netchan_Transmit( &chan, 5, payload ); p = Next( server );
	Check( p->length == sizeof( retail ) && !memcmp( p->data, retail, sizeof( retail ) ), "protocol 68 header is retail's" );

	/* the server takes ioquake3's client packets, and nothing with one bit of the checksum off */
	Netchan_Setup( NS_SERVER, &receiver, client, QPORT, 0x12345678, qfalse );
	MSG_Init( &msg, bufData, sizeof( bufData ) );
	memcpy( bufData, client2, sizeof( client2 ) ); msg.cursize = sizeof( client2 ); bufData[7] ^= 0x10;
	Check( !Netchan_Process( &receiver, &msg ) && !receiver.incomingSequence, "checksum one bit off" );
	memcpy( bufData, client2, 8 ); msg.cursize = 8;
	Check( !Netchan_Process( &receiver, &msg ) && !receiver.incomingSequence, "header cut inside the checksum" );
	Netchan_Setup( NS_SERVER, &chan, client, QPORT, 0x55555555, qfalse );	/* sequence 2's checksum is -1, */
	PutLong( bufData + 6, Checksum( 0x55555555, 2 ) ); msg.cursize = 8;	/* what a cut MSG_ReadLong returns */
	Check( Checksum( 0x55555555, 2 ) == 0xffffffff && !Netchan_Process( &chan, &msg ) && !chan.incomingSequence,
	       "header cut inside a checksum of -1" );
	memcpy( bufData, client2, sizeof( client2 ) ); msg.cursize = sizeof( client2 );
	Check( Netchan_Process( &receiver, &msg ) && receiver.incomingSequence == 2 && msg.readcount == 10, "ioquake3's client packet" );
	Check( !Netchan_Process( &receiver, &msg ), "replayed packet" );
	/* a protocol 68 packet on a protocol 71 channel, and the other way round */
	Netchan_Setup( NS_SERVER, &receiver, client, QPORT, 0x12345678, qfalse );
	memcpy( bufData, retail, sizeof( retail ) ); msg.cursize = sizeof( retail );
	Check( !Netchan_Process( &receiver, &msg ), "protocol 68 packet on a protocol 71 channel" );
	Netchan_Setup( NS_SERVER, &receiver, client, QPORT, 0x12345678, qtrue );
	memcpy( bufData, retail, sizeof( retail ) ); msg.cursize = sizeof( retail );
	Check( Netchan_Process( &receiver, &msg ) && msg.readcount == 6, "retail packet on a protocol 68 channel" );

	/* The checksum is in each datagram, not in the message: one retail clients take whole (MAX_MSGLEN - 4 with its
	 * EOF, see server.h MAX_CLIENT_MSGLEN) still fits exactly behind the sequence in a MAX_MSGLEN buffer. */
	length = MAX_MSGLEN - 4;
	Netchan_Setup( NS_SERVER, &chan, client, 0, 0x2468ace, qfalse );
	Netchan_Setup( NS_CLIENT, &receiver, server, QPORT, 0x2468ace, qfalse );
	chan.outgoingSequence = 5;	/* sequence 1's checksum is 0, whatever the challenge */
	Netchan_Transmit( &chan, length, payload );
	exact = malloc( MAX_MSGLEN );
	Check( exact != NULL, "allocation" );
	for ( i = 0; ; i++ ) {
		p = Next( client );
		Check( p->length <= PACKETLEN, "datagram within MAX_PACKETLEN" );
		Check( GetLong( p->data + 4 ) == Checksum( 0x2468ace, 5 ), "each fragment's checksum" );
		MSG_Init( &msg, exact, MAX_MSGLEN ); memcpy( exact, p->data, p->length ); msg.cursize = p->length;
		if ( Netchan_Process( &receiver, &msg ) ) break;
		Check( chan.unsentFragments, "message completes" );
		Netchan_TransmitNextFragment( &chan );
	}
	Check( i == length / FRAGMENTLEN && msg.cursize == MAX_MSGLEN && msg.readcount == 4 && !memcmp( exact + 4, payload, length ),
	       "reassembled into exactly MAX_MSGLEN" );
	free( exact );
	Check( Quiet(), "no other datagrams" );
}

/** A retail 1.32c client: bare getchallenge, protocol 68, XOR encoding.  The server's every byte after the
 * challengeResponse is master's; that one has ioquake3's two more arguments, which retail clients ignore. */
static void RetailClient( void ) {
	static byte plain[MAX_MSGLEN], encoded[MAX_MSGLEN], out[MAX_MSGLEN], queuedPlain[MAX_MSGLEN];
	static message_t m, queued;
	packet_t *p;
	int challenge, sequence, length, i, n, queuedLength = 0;
	qboolean refused;

	Start( "retail client" );
	ServerGets( client, "getchallenge" );
	Check( Server_Challenge( client, &challenge, &refused ), "challenge record" );
	Check( !strcmp( Text( Next( client ) ), va( "challengeResponse %i 0 71", challenge ) ),
	       "ioquake3's challengeResponse, whose first argument is all a retail client reads" );
	ServerGetsConnect( client, RetailConnect( challenge ) );
	Check( !strcmp( Text( Next( client ) ), "connectResponse" ), "bare connectResponse, as master's" );
	Check( Server_Connected( 0 ) && Server_ClientCompat( 0 ), "protocol 68 client" );
	Server_SetCommands( 0, "say hello", 1, "print \"welcome\"" );

	/* the client's packets, XOR encoded with retail's key */
	for ( sequence = 1; sequence <= 2; sequence++ ) {
		ClientMessage( &m, 4380, sequence, 1, "say hello" );
		length = WithEOF( &m, clc_EOF, plain );
		n = RetailClientPacket( out, sequence, challenge, 4380, sequence, "print \"welcome\"", plain, length );
		ToServer( client, out, n );
		Check( Executed( plain, length, 6 ), "server decodes retail's packet to the message" );
	}
	/* The server's messages: one datagram, then a fragmented one with another queued behind it (id's bug 462,
	 * see SV_Netchan_Transmit), which is encoded when it goes out.  Retail's header and XOR, byte for byte. */
	for ( i = 0; i < 2; i++ ) {
		ServerMessage( &m, 2, "print \"news\"", i ? 3000 : 20 );
		length = WithEOF( &m, svc_EOF, plain );
		sequence = Server_OutgoingSequence( 0 );
		Server_Transmit( 0, &m.msg );
		if ( i ) {
			ServerMessage( &queued, 2, "print \"more\"", 20 );
			queuedLength = WithEOF( &queued, svc_EOF, queuedPlain );
			Server_Transmit( 0, &queued.msg );
			while ( Server_NextFragment( 0 ) ) {}
		}
		RetailServerEncode( encoded, sequence, challenge, "say hello", plain, length );
		for ( n = 0; n < Datagrams( length ); n++ ) {
			p = Next( client );
			Check( p->length == RetailServerDatagram( out, sequence, encoded, length, n ) && !memcmp( p->data, out, p->length ),
			       "server datagram is retail's" );
		}
	}
	RetailServerEncode( encoded, sequence + 1, challenge, "say hello", queuedPlain, queuedLength );
	p = Next( client );
	Check( p->length == RetailServerDatagram( out, sequence + 1, encoded, queuedLength, 0 ) && !memcmp( p->data, out, p->length ),
	       "queued server datagram is retail's" );
	Check( Quiet(), "no other datagrams" );

	/* NAT: only a packet the netchan takes moves the client's port, and a stranger gets no reply */
	ClientMessage( &m, 4380, 3, 1, "say hello" );
	length = WithEOF( &m, clc_EOF, plain );
	n = RetailClientPacket( out, 2, challenge, 4380, 3, "print \"welcome\"", plain, length );
	ToServer( clientOtherPort, out, n );
	Check( NothingExecuted() && NET_CompareAdr( Server_ClientAddress( 0 ), client ), "replayed sequence moved the port" );
	n = RetailClientPacket( out, 3, challenge, 4380, 3, "print \"welcome\"", plain, length );
	ToServer( clientOtherPort, out, n );
	Check( Executed( plain, length, 6 ) && NET_CompareAdr( Server_ClientAddress( 0 ), clientOtherPort ), "translated port" );
	ToServer( attacker, out, n );
	Check( NothingExecuted() && Quiet(), "a sequenced packet from a stranger is answered (with a disconnect)" );

	/* master's refusals */
	ServerGetsConnect( attacker, va( "\\protocol\\70\\qport\\7\\challenge\\%i", challenge ) );
	Check( !strcmp( Text( Next( attacker ) ), "print\nServer uses protocol version 68.\n" ), "protocol refusal" );
	Check( Quiet(), "one reply" );
}

/** This client and a retail 1.32c server: no echo, no protocol, so protocol 68, and every byte after the
 * getchallenge is master's. */
static void RetailServer( void ) {
	static byte plain[MAX_MSGLEN], encoded[MAX_MSGLEN], out[MAX_MSGLEN];
	message_t m;
	int version, challenge, length, i, n, sequence;

	Start( "retail server" );
	ClientConnect( "192.0.2.10:27960" );
	Resend();
	Check( !strcmp( Text( Next( server ) ), va( "getchallenge %i Quake3Arena", clc.challenge ) ),
	       "getchallenge with the client's challenge and ioquake3's game name, which retail ignores" );
	ClientGets( server, "challengeResponse 4321" );
	Check( cls.state == CA_CHALLENGING && clc.compat && clc.challenge == 4321, "retail challengeResponse taken" );
	Check( !strcmp( Connecting( &version, &challenge ), RetailConnect( 4321 ) ), "connect is master's" );
	ClientGets( server, "connectResponse" );
	Check( cls.state == CA_CONNECTED && clc.netchan.compat && clc.netchan.challenge == 4321, "connected with protocol 68" );

	/* the client's packets: retail's header and XOR, byte for byte */
	Q_strncpyz( clc.serverCommands[1], "print \"welcome\"", sizeof( clc.serverCommands[1] ) );
	for ( sequence = 1; sequence <= 2; sequence++ ) {
		ClientMessage( &m, 4380, sequence, 1, "say hello" );
		length = WithEOF( &m, clc_EOF, plain );
		CL_Netchan_Transmit( &clc.netchan, &m.msg );
		n = RetailClientPacket( out, sequence, 4321, 4380, sequence, "print \"welcome\"", plain, length );
		Check( Next( server )->length == n && !memcmp( wire[wireRead - 1].data, out, n ), "client datagram is retail's" );
	}
	/* the retail server's messages, one datagram and fragmented, decode to what it wrote */
	Q_strncpyz( clc.reliableCommands[2], "say hello", sizeof( clc.reliableCommands[2] ) );
	for ( i = 0, sequence = 1; i < 2; i++, sequence++ ) {
		ServerMessage( &m, 2, "print \"news\"", i ? 3000 : 20 );
		length = WithEOF( &m, svc_EOF, plain );
		RetailServerEncode( encoded, sequence, 4321, "say hello", plain, length );
		for ( n = 0; n < Datagrams( length ); n++ ) {
			ToClient( server, out, RetailServerDatagram( out, sequence, encoded, length, n ) );
		}
		Check( Parsed( plain, length, 4 ), "client decodes retail's message" );
	}
	clc.demorecording = qtrue; clc.demowaiting = qfalse; clc.demofile = 1;
	RetailServerEncode( encoded, sequence, 4321, "say hello", plain, length );
	for ( n = 0; n < Datagrams( length ); n++ ) {
		ToClient( server, out, RetailServerDatagram( out, sequence, encoded, length, n ) );
	}
	Check( Parsed( plain, length, 4 ) && Demo( sequence, plain, length ), "demo message" );
	clc.demorecording = qfalse; clc.demofile = 0;
	/* retail servers answer a sequenced packet they do not know with a disconnect; retail clients never took it */
	cls.realtime += 5000;
	ClientGets( server, "disconnect" );
	Check( cls.state == CA_CONNECTED, "out of band disconnect taken" );
	Check( Quiet(), "no other datagrams" );
}

/** This client and this server: protocol 71, the checksum in every datagram and no XOR. */
static void Protocol71( void ) {
	static byte plain[MAX_MSGLEN], out[MAX_MSGLEN], queuedPlain[MAX_MSGLEN];
	static message_t m, queued;
	packet_t *p;
	netadr_t from;
	msg_t msg;
	int challenge, length, i, n, sequence, queuedLength = 0;
	qboolean refused;

	Start( "protocol 71" );
	Check( ConnectBoth() == PROTOCOL_CHECKSUM_VERSION && !Server_ClientCompat( 0 ), "connected with protocol 71" );
	challenge = Server_ClientChallenge( 0 );
	Check( !strcmp( Text( &wire[wireRead] ), va( "connectResponse %i", challenge ) ), "connectResponse names the challenge" );
	ServerToClient();
	Check( cls.state == CA_CONNECTED && !clc.netchan.compat && clc.netchan.challenge == challenge, "client took it" );

	/* the client's packets carry the checksum, and the message as it is */
	for ( sequence = 1; sequence <= 2; sequence++ ) {
		ClientMessage( &m, 4380, sequence, 0, "say hello" );
		length = WithEOF( &m, clc_EOF, plain );
		CL_Netchan_Transmit( &clc.netchan, &m.msg );
		p = Next( server );
		Check( p->length == Packet71( out, sequence, challenge, qtrue, plain, length ) && !memcmp( p->data, out, p->length ),
		       "client datagram" );
		ToServer( client, p->data, p->length );
		Check( Executed( plain, length, 10 ), "server reads the message after the checksum" );
	}
	/* the server's: one datagram, then a fragmented message the client reassembles, and one queued behind it */
	for ( i = 0; i < 2; i++ ) {
		ServerMessage( &m, 2, "print \"news\"", i ? 3000 : 20 );
		length = WithEOF( &m, svc_EOF, plain );
		sequence = Server_OutgoingSequence( 0 );
		Server_Transmit( 0, &m.msg );
		if ( i ) {
			ServerMessage( &queued, 2, "print \"more\"", 20 );
			queuedLength = WithEOF( &queued, svc_EOF, queuedPlain );
			Server_Transmit( 0, &queued.msg );
			while ( Server_NextFragment( 0 ) ) {}
		}
		for ( n = 0; n < Datagrams( length ); n++ ) {
			p = Next( client );
			Check( GetLong( p->data ) == ( length < FRAGMENTLEN ? (unsigned)sequence : sequence | FRAGMENTED )
			       && GetLong( p->data + 4 ) == Checksum( challenge, sequence ), "server datagram's checksum" );
			if ( length < FRAGMENTLEN ) {
				Check( p->length == 8 + length && !memcmp( p->data + 8, plain, length ), "message as it is" );
			} else {
				Check( (int)( GetLong( p->data + 8 ) & 0xffff ) == n * FRAGMENTLEN && !memcmp( p->data + 12, plain + n * FRAGMENTLEN,
				       p->length - 12 ), "fragment as it is" );
			}
			ToClient( server, p->data, p->length );
		}
		Check( Parsed( plain, length, length < FRAGMENTLEN ? 8 : 4 ), "client parses the message" );
	}
	p = Next( client );
	Check( p->length == Packet71( out, sequence + 1, challenge, qfalse, queuedPlain, queuedLength ) && !memcmp( p->data, out, p->length ),
	       "queued message as it is" );
	ToClient( server, p->data, p->length );
	Check( Parsed( queuedPlain, queuedLength, 8 ), "client parses the queued message" );
	Check( Quiet(), "no other datagrams" );
	clc.demorecording = qtrue; clc.demowaiting = qfalse; clc.demofile = 1;
	ToClient( server, out, Packet71( out, sequence + 2, challenge, qfalse, queuedPlain, queuedLength ) );
	Check( Parsed( queuedPlain, queuedLength, 8 ) && Demo( sequence + 2, queuedPlain, queuedLength ), "demo message" );
	clc.demorecording = qfalse; clc.demofile = 0;

	/* a challenge from an address outside the LAN, once the authorize server has timed out, also echoes */
	ServerGets( wan, "getchallenge 77 Quake3Arena" );
	Check( Quiet(), "authorizing" );
	Server_SetTime( 100000 + 5000 + 1 );	/* server.h AUTHORIZE_TIMEOUT */
	ServerGets( wan, "getchallenge 78 Quake3Arena" );
	Check( Server_Challenge( wan, &n, &refused ) && !strcmp( Text( Next( wan ) ), va( "challengeResponse %i 78 71", n ) ),
	       "authorize timeout answer" );
	/* a challenge request with the client's own challenge is a new attempt for #417 too */
	Server_Refuse( wan );
	ServerGets( wan, "getchallenge 79 Quake3Arena" );
	Check( Server_Challenge( wan, &n, &refused ) && !refused && !strcmp( Text( Next( wan ) ), va( "challengeResponse %i 79 71", n ) ),
	       "refusal kept" );
	ServerGets( wan, "getchallenge" );	/* spoofed from the client's address */
	Check( !strcmp( Text( Next( wan ) ), va( "challengeResponse %i 79 71", n ) ), "bare answer once the client sent its challenge" );

	/* a listen server's own client connects over the loopback, with protocol 71 and challenge 0 */
	Start( "protocol 71 loopback" );
	Q_strncpyz( cls.servername, "localhost", sizeof( cls.servername ) );
	cls.state = CA_CHALLENGING;	/* as CL_MapLoading sets it */
	Check( NET_StringToAdr( cls.servername, &clc.serverAddress ) && clc.serverAddress.type == NA_LOOPBACK, "loopback" );
	Resend();
	for ( n = 0; n < 2; n++ ) {
		MSG_Init( &msg, bufData, sizeof( bufData ) );
		while ( NET_GetLoopPacket( n ? NS_CLIENT : NS_SERVER, &from, &msg ) ) {
			if ( n ) CL_PacketEvent( from, &msg ); else SV_PacketEvent( from, &msg );
			MSG_Init( &msg, bufData, sizeof( bufData ) );
		}
	}
	Check( Server_Connected( 0 ) && !Server_ClientCompat( 0 ) && cls.state == CA_CONNECTED
	       && !clc.netchan.compat && !clc.netchan.challenge, "local client connected with protocol 71" );
	ServerMessage( &m, 0, "print \"local\"", 2000 );
	length = WithEOF( &m, svc_EOF, plain );
	Server_Transmit( 0, &m.msg );
	while ( Server_NextFragment( 0 ) ) {}
	MSG_Init( &msg, bufData, sizeof( bufData ) );
	while ( NET_GetLoopPacket( NS_CLIENT, &from, &msg ) ) { CL_PacketEvent( from, &msg ); MSG_Init( &msg, bufData, sizeof( bufData ) ); }
	Check( Parsed( plain, length, 4 ), "local message" );
	Check( Quiet(), "nothing on the network" );
}

/** This client and ioquake3 or Quake3e servers, whatever protocol they name. */
static void OtherServers( void ) {
	static const byte ioquake3[] = { 0xf0, 0xff, 0xff, 0x7f, 0x0e, 0xd0, 0x09, 0x2f, 1, 8, 15 };
	static byte plain[MAX_MSGLEN], out[MAX_MSGLEN];
	message_t m;
	int version, challenge, length;

	Start( "ioquake3 server" );
	ClientConnect( "192.0.2.10:27960" );
	Resend(); Next( server );
	ClientGets( server, va( "challengeResponse %d %d %d", -1234567890, clc.challenge, 71 ) );	/* sv_client.c */
	Check( cls.state == CA_CHALLENGING && !clc.compat && clc.challenge == -1234567890, "protocol 71 offered" );
	Connecting( &version, &challenge );
	Check( version == 71 && challenge == -1234567890, "connects with protocol 71" );
	ClientGets( server, va( "connectResponse %d", -1234567890 ) );
	Check( cls.state == CA_CONNECTED && !clc.netchan.compat, "connected with protocol 71" );
	ToClient( server, ioquake3, sizeof( ioquake3 ) );	/* its net_chan.c's datagram, sequence 0x7ffffff0 */
	Check( Parsed( ioquake3 + 8, 3, 8 ), "ioquake3's datagram parsed" );
	ClientMessage( &m, 4380, 0x7ffffff0, 0, "say hello" );
	length = WithEOF( &m, clc_EOF, plain );
	CL_Netchan_Transmit( &clc.netchan, &m.msg );
	Check( Next( server )->length == Packet71( out, 1, -1234567890, qtrue, plain, length )
	       && !memcmp( wire[wireRead - 1].data, out, wire[wireRead - 1].length ), "client datagram as ioquake3 reads it" );

	/* one that names another protocol, as an ioquake3 70 or a standalone game: protocol 68 */
	Start( "ioquake3 server, protocol 70" );
	ClientConnect( "192.0.2.10:27960" );
	Resend(); Next( server );
	ClientGets( server, va( "challengeResponse %d %d %d", 99, clc.challenge, 70 ) );
	Check( cls.state == CA_CHALLENGING && clc.compat, "legacy protocol" );
	Connecting( &version, &challenge );
	Check( version == 68 && challenge == 99, "connects with protocol 68" );
	ClientGets( server, "connectResponse 98" );
	Check( cls.state == CA_CHALLENGING, "connectResponse for another challenge" );
	ClientGets( server, "connectResponse 99" );	/* ioquake3 and Quake3e servers send it in protocol 68 too */
	Check( cls.state == CA_CONNECTED && clc.netchan.compat, "connected with protocol 68" );

	/* Quake3e: the same challengeResponse, and its connectResponse may add the protocol */
	Start( "Quake3e server" );
	ClientConnect( "192.0.2.10:27960" );
	Resend(); Next( server );
	ClientGets( server, va( "challengeResponse %i %i %i", 4242, clc.challenge, 71 ) );
	Connecting( &version, &challenge );
	Check( version == 71 && challenge == 4242, "connects with protocol 71" );
	ClientGets( server, "connectResponse 4242 71" );
	Check( cls.state == CA_CONNECTED && !clc.netchan.compat && clc.netchan.challenge == 4242, "connected with protocol 71" );
	Check( Quiet(), "no other datagrams" );
}

/** Spoofed connection setup and the client's timeouts. */
static void SpoofedSetup( void ) {
	int nonce, version, challenge, connectedAt, i;

	Start( "spoofed challengeResponse" );
	ClientConnect( "192.0.2.10:27960" );
	Resend(); Next( server );
	nonce = clc.challenge;
	ClientGets( attacker, "challengeResponse 666" );
	Check( cls.state == CA_CONNECTING && NET_CompareAdr( clc.serverAddress, server ), "taken from another address" );
	ClientGets( serverOtherPort, "challengeResponse 666" );
	Check( cls.state == CA_CONNECTING && NET_CompareAdr( clc.serverAddress, server ), "taken from another port" );
	ClientGets( attacker, va( "challengeResponse 666 %i 71", nonce + 1 ) );
	ClientGets( server, va( "challengeResponse 666 %i 71", nonce + 1 ) );
	ClientGets( server, "challengeResponse 666 0 71" );
	ClientGets( server, "challengeResponse 666 x 71" );
	ClientGets( server, va( "challengeResponse 666 %i", nonce + 1 ) );	/* ioquake3 would take this one */
	Check( cls.state == CA_CONNECTING && clc.challenge == nonce, "taken with another echo" );
	/* a spoof that would leave ioquake3 3.x stuck in protocol 68 does not downgrade the real one */
	ClientGets( attacker, "challengeResponse 666" );
	ClientGets( server, va( "challengeResponse 7 %i 71", nonce ) );
	Check( cls.state == CA_CHALLENGING && !clc.compat && clc.challenge == 7, "real challengeResponse" );
	ClientGets( server, va( "challengeResponse 8 %i 71", nonce ) );
	Check( clc.challenge == 7, "second challengeResponse" );
	/* connectResponse: from the address that answered, naming its challenge */
	Connecting( &version, &challenge );
	ClientGets( serverOtherPort, "connectResponse 7" );
	ClientGets( attacker, "connectResponse 7" );
	ClientGets( server, "connectResponse" );
	ClientGets( server, "connectResponse 8" );
	Check( cls.state == CA_CHALLENGING, "connectResponse taken" );

	/* A connect that takes long, on a client up for long: the timeout runs from the connectResponse, and from nothing
	 * an attacker sends. */
	cls.realtime = 10 * 1000 * 1000;
	clc.lastPacketTime = 0;
	ClientGets( server, "connectResponse 7" );
	Check( cls.state == CA_CONNECTED && clc.lastPacketTime == cls.realtime, "connected" );
	for ( i = 0; i < 10; i++ ) { cls.realtime += 100; CL_CheckTimeout(); }
	Check( cls.state == CA_CONNECTED, "slow connect timed out" );
	connectedAt = clc.lastPacketTime;
	ClientGets( attacker, "print\nhello\n" );
	ClientGets( server, "statusResponse\n\\sv_hostname\\x\n" );
	ClientGets( server, "disconnect" );
	ToClient( server, (const byte *)"\x02\x00\x00\x00\x00\x00\x00\x00\x2a", 9 );
	Check( clc.lastPacketTime == connectedAt && NothingParsed(), "spoofed packets kept the connection open" );
	cls.realtime = connectedAt + timeout.integer * 1000 + 1;
	for ( i = 0; i < 5; i++ ) {	/* CL_CheckTimeout gives up the sixth time */
		ClientGets( server, "disconnect" );
		CL_CheckTimeout();
		Check( cls.state == CA_CONNECTED, "disconnected early" );
	}
	CL_CheckTimeout();
	Check( cls.state == CA_DISCONNECTED, "timed out" );

	/* a proxy may hand the connection on, with the client's challenge */
	Start( "proxy" );
	ClientConnect( "192.0.2.10:27960" );
	Resend(); Next( server );
	ClientGets( proxy, va( "challengeResponse 31 %i 71", clc.challenge ) );
	Check( cls.state == CA_CHALLENGING && NET_CompareAdr( clc.serverAddress, proxy ) && clc.challenge == 31, "proxy handoff" );
	Connecting( &version, &challenge );
	Check( NET_CompareAdr( wire[wireRead - 1].to, proxy ), "connect goes to the proxy's server" );
	Check( Quiet(), "no other datagrams" );
}

/** A protocol 71 fragment from the server. */
static int Fragment71( byte *out, int sequence, int challenge, int start, int size, const byte *data ) {
	PutLong( out, sequence | FRAGMENTED ); PutLong( out + 4, Checksum( challenge, sequence ) );
	PutShort( out + 8, start ); PutShort( out + 10, size ); memcpy( out + 12, data + start, size );
	return 12 + size;
}

/** Spoofed sequenced packets with the right address, qport and sequence, against a protocol 71 server and
 * client.  (Sequence 1's checksum is 0 whatever the challenge, in ioquake3 too, so they come later.) */
static void SpoofedPackets( void ) {
	static byte plain[MAX_MSGLEN], out[MAX_MSGLEN];
	message_t m;
	packet_t *p;
	int challenge, length, n, last, sequence, i, start;
	qboolean refused;

	Start( "spoofed packets" );
	ConnectBoth(); ServerToClient();
	challenge = Server_ClientChallenge( 0 );
	Check( cls.state == CA_CONNECTED && !clc.netchan.compat, "connected with protocol 71" );

	/* the server */
	ClientMessage( &m, 4380, 1, 0, "say hello" );
	length = WithEOF( &m, clc_EOF, plain );
	for ( sequence = 1; sequence <= 2; sequence++ ) {
		ToServer( client, out, Packet71( out, sequence, challenge, qtrue, plain, length ) );
		Check( Executed( plain, length, 10 ), "real packet" );
	}
	last = Server_ClientLastPacketTime( 0 );
	Server_SetTime( last + 1000 );
	n = Packet71( out, 3, challenge, qtrue, plain, length );
	for ( i = 0; i < 32; i++ ) {
		out[6 + i / 8] ^= 1 << ( i & 7 );
		ToServer( client, out, n );
		ToServer( clientOtherPort, out, n );
		out[6 + i / 8] ^= 1 << ( i & 7 );
	}
	ToServer( client, out, 8 );	/* cut inside the checksum */
	PutLong( out, 3 ); PutShort( out + 4, QPORT ); memcpy( out + 6, plain, length );
	ToServer( client, out, 6 + length );	/* protocol 68's header */
	Check( NothingExecuted() && Server_ClientLastPacketTime( 0 ) == last && NET_CompareAdr( Server_ClientAddress( 0 ), client ),
	       "server took a packet without the checksum, or moved the port for it" );
	Check( Quiet(), "server answered a spoofed packet" );
	ToServer( clientOtherPort, out, Packet71( out, 3, challenge, qtrue, plain, length ) );
	Check( Executed( plain, length, 10 ) && Server_ClientLastPacketTime( 0 ) == last + 1000
	       && NET_CompareAdr( Server_ClientAddress( 0 ), clientOtherPort ), "the real packet, from a translated port" );
	ToServer( attacker, out, n );
	Check( NothingExecuted() && Quiet(), "a stranger's sequenced packet is answered (with a disconnect)" );

	/* the client */
	ServerMessage( &m, 1, "print \"news\"", 20 );
	length = WithEOF( &m, svc_EOF, plain );
	for ( sequence = 1; sequence <= 2; sequence++ ) {
		ToClient( server, out, Packet71( out, sequence, challenge, qfalse, plain, length ) );
		Check( Parsed( plain, length, 8 ), "real packet" );
	}
	last = clc.lastPacketTime;
	cls.realtime += 1000;
	n = Packet71( out, 3, challenge, qfalse, plain, length );
	for ( i = 0; i < 32; i++ ) {
		out[4 + i / 8] ^= 1 << ( i & 7 );
		ToClient( server, out, n );
		out[4 + i / 8] ^= 1 << ( i & 7 );
	}
	ToClient( server, out, 6 );
	Check( NothingParsed() && clc.lastPacketTime == last && clc.netchan.incomingSequence == 2,
	       "client took a packet without the checksum" );
	ToClient( server, out, n );
	Check( Parsed( plain, length, 8 ) && clc.lastPacketTime == cls.realtime && clc.netchan.incomingSequence == 3, "the real packet" );

	/* spoofed fragments neither start nor add to a fragmented message */
	ServerMessage( &m, 1, "print \"news\"", 3000 );
	length = WithEOF( &m, svc_EOF, plain );
	Check( length % FRAGMENTLEN, "no empty last fragment" );
	for ( start = 0; start < length; start += FRAGMENTLEN ) {
		n = Fragment71( out, 4, challenge, start, length - start < FRAGMENTLEN ? length - start : FRAGMENTLEN, plain );
		out[5] ^= 0x40;
		ToClient( server, out, n );
		Check( clc.netchan.fragmentSequence != 4 || clc.netchan.fragmentLength == start, "spoofed fragment taken" );
		out[5] ^= 0x40;
		if ( start ) {
			n = Fragment71( out, 5, challenge, 0, FRAGMENTLEN, plain );
			out[6] ^= 1;
			ToClient( server, out, n );
			Check( clc.netchan.fragmentSequence == 4 && clc.netchan.fragmentLength == start, "spoofed fragment restarted the message" );
			n = Fragment71( out, 4, challenge, start, length - start < FRAGMENTLEN ? length - start : FRAGMENTLEN, plain );
		}
		ToClient( server, out, n );
	}
	Check( Parsed( plain, length, 4 ) && clc.netchan.incomingSequence == 4, "fragmented message" );
	Check( Quiet(), "no other datagrams" );

	/* A bare getchallenge spoofed from the client's address gets its echo too, not a bare challengeResponse
	 * the client would take, from the server's address, as protocol 68 with a real challenge. */
	Start( "spoofed getchallenge" );
	ClientConnect( "192.0.2.10:27960" );
	Resend(); ClientToServer();	/* the client's getchallenge, whose answer is still on its way */
	ServerGets( client, "getchallenge" );
	p = Next( client ); p = Next( client );	/* the spoof's answer overtakes it */
	Check( Server_Challenge( client, &challenge, &refused )
	       && !strcmp( Text( p ), va( "challengeResponse %i %i 71", challenge, clc.challenge ) ), "bare answer to a spoofed getchallenge" );
	ToClient( server, p->data, p->length );
	Check( cls.state == CA_CHALLENGING && !clc.compat && clc.challenge == challenge, "talked down to protocol 68" );
	Check( Quiet(), "no other datagrams" );
}

/** A datagram from the server reaching the client at once, the clock unmoved. */
static void Arrive( const packet_t *p ) {
	msg_t msg;
	MSG_Init( &msg, bufData, sizeof( bufData ) ); memcpy( bufData, p->data, p->length ); msg.cursize = p->length;
	CL_PacketEvent( server, &msg );
}
/** A bare getchallenge spoofed from `victim`, 50 msec after the last. */
static void SpoofGetchallenge( netadr_t victim, int *serverTime ) {
	msg_t msg;
	MSG_Init( &msg, bufData, sizeof( bufData ) );
	memcpy( bufData, OOB "getchallenge", 16 ); msg.cursize = 16;
	now += 50; *serverTime += 50; Server_SetTime( *serverTime );
	SV_PacketEvent( victim, &msg );
}
/** The reviewer's starvation attack on #458: bare getchallenges spoofed from the client's address, 20 a second,
 * use up its rate limit (#431), so that its own getchallenge never reaches SV_GetChallenge.  The answers to the
 * spoofs carry a real challenge, from the server's address; they must not be ones the client takes as a retail
 * server's, which would connect it with protocol 68. */
static void Starved( netadr_t victim, const char *name ) {
	packet_t *p;
	msg_t msg;
	int i, serverTime = 100000, answers = 0, challenge;
	qboolean refused;

	Start( name );
	for ( i = 0; i < 100; i++ ) SpoofGetchallenge( victim, &serverTime );	/* before the client connects */
	while ( wireRead < wireCount ) Arrive( &wire[wireRead++] );	/* an idle client ignores them */
	ClientConnect( "192.0.2.10:27960" ); Resend();
	p = Next( server );
	MSG_Init( &msg, bufData, sizeof( bufData ) ); memcpy( bufData, p->data, p->length ); msg.cursize = p->length;
	now += 10; SV_PacketEvent( victim, &msg );
	Check( Quiet(), "the client's own getchallenge got through the spoofs" );
	for ( i = 0; i < 100 && cls.state == CA_CONNECTING; i++ ) {
		SpoofGetchallenge( victim, &serverTime );
		while ( wireRead < wireCount ) {
			p = Next( victim );
			Check( Server_Challenge( victim, &challenge, &refused )
			       && !strcmp( Text( p ), va( "challengeResponse %i 0 71", challenge ) ), "answer to a spoofed getchallenge" );
			Arrive( p );
			answers++;
		}
	}
	Check( answers >= 3, "spoofs answered" );
	Check( cls.state == CA_CONNECTING && !clc.compat, "talked down to protocol 68 by a starved getchallenge" );
	/* once the spoofs stop, the client's own gets through and it connects with protocol 71 */
	now += 15000;
	Resend(); p = Next( server ); ToServer( victim, p->data, p->length );
	Check( Server_Challenge( victim, &challenge, &refused )
	       && !strcmp( Text( &wire[wireRead] ), va( "challengeResponse %i %i 71", challenge, clc.challenge ) ), "echo" );
	ToClient( server, wire[wireRead].data, wire[wireRead].length ); wireRead++;
	Check( cls.state == CA_CHALLENGING && !clc.compat && clc.challenge == challenge, "protocol 71 after the spoofs" );
	Check( Quiet(), "no other datagrams" );
}

/** Starved getchallenges, on the LAN and through the authorize server's timeout; and challenges that do not follow
 * rand(), which the renderer reseeds with a constant, the server with the clock. */
static void StarvedAndChallenges( void ) {
	int first, second;
	qboolean refused;

	Starved( client, "starved getchallenge, LAN" );
	Starved( wan, "starved getchallenge, authorize timeout" );

	Start( "client challenge" );
	srand( 1001 ); ClientConnect( "192.0.2.10:27960" ); first = clc.challenge;
	Start( "client challenge" );
	srand( 1001 ); ClientConnect( "192.0.2.10:27960" ); second = clc.challenge;
	Check( first && second && first != second, "client challenges follow rand() and the uptime" );
	Start( "server challenge" );
	srand( 1001 ); ServerGets( client, "getchallenge" ); Check( Server_Challenge( client, &first, &refused ), "record" );
	Start( "server challenge" );
	srand( 1001 ); ServerGets( client, "getchallenge" ); Check( Server_Challenge( client, &second, &refused ), "record" );
	Check( first && second && first != second, "server challenges follow rand() and the server time" );
}

/** com_protocol 68: protocol 68 only, on either side. */
static void Protocol68Only( void ) {
	static byte plain[MAX_MSGLEN], out[MAX_MSGLEN];
	message_t m;
	int version, challenge, length, n;
	qboolean refused;

	Start( "com_protocol 68 server" );
	protocol.integer = 68;
	ServerGets( client, "getchallenge 5 Quake3Arena" );
	Check( Server_Challenge( client, &challenge, &refused ), "challenge record" );
	Check( !strcmp( Text( Next( client ) ), va( "challengeResponse %i 5 68", challenge ) ), "names protocol 68" );
	ServerGetsConnect( client, va( "\\protocol\\71\\qport\\%i\\challenge\\%i", QPORT, challenge ) );
	Check( !strcmp( Text( Next( client ) ), "print\nServer uses protocol version 68.\n" ) && Server_Free( 0 ),
	       "protocol 71 refused" );

	/* this client to this server, both set to 68: what a retail client gets */
	Start( "com_protocol 68 both" );
	protocol.integer = 68;
	Check( ConnectBoth() == 68 && Server_ClientCompat( 0 ), "protocol 68" );
	challenge = Server_ClientChallenge( 0 );
	Check( !strcmp( Text( &wire[wireRead] ), "connectResponse" ), "bare connectResponse" );
	ServerToClient();
	Check( cls.state == CA_CONNECTED && clc.netchan.compat, "connected with protocol 68" );
	Q_strncpyz( clc.serverCommands[0], "", sizeof( clc.serverCommands[0] ) );
	ClientMessage( &m, 4380, 1, 0, "say hello" );
	length = WithEOF( &m, clc_EOF, plain );
	CL_Netchan_Transmit( &clc.netchan, &m.msg );
	n = RetailClientPacket( out, 1, challenge, 4380, 1, "", plain, length );
	Check( Next( server )->length == n && !memcmp( wire[wireRead - 1].data, out, n ), "client datagram is retail's" );
	ToServer( client, out, n );
	Check( Executed( plain, length, 6 ), "server decodes it" );

	/* a client set to 68 against a server offering 71 */
	Start( "com_protocol 68 client" );
	protocol.integer = 68;
	ClientConnect( "192.0.2.10:27960" );
	Resend(); Next( server );
	ClientGets( server, va( "challengeResponse 12 %i 71", clc.challenge ) );
	Check( cls.state == CA_CHALLENGING && clc.compat, "stays with protocol 68" );
	Check( !strcmp( Connecting( &version, &challenge ), RetailConnect( 12 ) ), "connect is master's" );
	Check( Quiet(), "no other datagrams" );
}

int main( int argc, char **argv ) {
	int which;

	Check( argc == 2, "usage: protocol71_regression <case>" );
	which = atoi( argv[1] );
	Sys_StringToAdr( "192.0.2.10", &server ); server.port = BigShort( 27960 );
	serverOtherPort = server; serverOtherPort.port = BigShort( 27961 );
	Sys_StringToAdr( "198.51.100.20", &client ); client.port = BigShort( 27960 );
	clientOtherPort = client; clientOtherPort.port = BigShort( 27999 );
	Sys_StringToAdr( "203.0.113.66", &attacker ); attacker.port = BigShort( 27960 );
	Sys_StringToAdr( "192.0.2.77", &proxy ); proxy.port = BigShort( 27960 );
	Sys_StringToAdr( "198.18.0.9", &wan ); wan.port = BigShort( 27960 );
	Netchan_Init( QPORT );
	cl_timeout = &timeout; cl_motd = &quiet;
	switch ( which ) {
	case 0: IoquakePackets(); break;
	case 1: RetailClient(); break;
	case 2: RetailServer(); break;
	case 3: Protocol71(); break;
	case 4: OtherServers(); break;
	case 5: SpoofedSetup(); break;
	case 6: SpoofedPackets(); break;
	case 7: Protocol68Only(); break;
	case 8: StarvedAndChallenges(); break;
	default: Check( 0, "case" );
	}
	printf( "Protocol 71 regressions passed (issue #37): %s\n", scenario );
	return 0;
}
