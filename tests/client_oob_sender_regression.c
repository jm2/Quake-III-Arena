/* Issue #275: drive the real CL_ConnectionlessPacket and CL_Rcon_f; discard other client code at link. */
#include "../code/client/cl_main.c"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char printed[4096];
static char sent[MAX_MSGLEN];
static int sentLength, sends;
static netadr_t sentTo;
static byte resolvedIp[4] = { 198, 51, 100, 20 };
static cvar_t quiet, password, rconTarget;
extern cvar_t *showpackets;

/** Stop on the first connectionless packet handled against the sender rules. */
static void Check( int ok, const char *message ) {
	if ( !ok ) { fprintf( stderr, "Client OOB sender regression failed: %s\n", message ); exit( 1 ); }
}
void QDECL Com_Error( int level, const char *format, ... ) { (void)level; (void)format; Check( 0, "engine error" ); }
/** Keep console output so tests can see whether "print" text reached it. */
void QDECL Com_Printf( const char *format, ... ) {
	va_list args;
	size_t used = strlen( printed );
	va_start( args, format );
	vsnprintf( printed + used, sizeof( printed ) - used, format, args );
	va_end( args );
}
void QDECL Com_DPrintf( const char *format, ... ) { (void)format; }
void Com_Memcpy( void *out, const void *in, size_t size ) { memcpy( out, in, size ); }
void Com_Memset( void *out, int value, size_t size ) { memset( out, value, size ); }
void *Z_Malloc( int size ) { void *p = calloc( 1, size ); Check( p != NULL, "allocation" ); return p; }
void Z_Free( void *p ) { free( p ); }
char *CopyString( const char *in ) { char *out = Z_Malloc( strlen( in ) + 1 ); strcpy( out, in ); return out; }
/* Kept by the linker for other connectionless commands; none of these tests reach them. */
cvar_t *com_cl_running;
int cl_connectedToPureServer;
vm_t *uivm;
void Cvar_Set( const char *name, const char *value ) { (void)name; (void)value; Check( 0, "Cvar_Set" ); }
float Cvar_VariableValue( const char *name ) { (void)name; Check( 0, "Cvar_VariableValue" ); return 0; }
int Com_Milliseconds( void ) { Check( 0, "Com_Milliseconds" ); return 0; }
int FS_Write( const void *buffer, int length, fileHandle_t f ) { (void)buffer; (void)f; Check( 0, "FS_Write" ); return length; }
void FS_FCloseFile( fileHandle_t f ) { (void)f; Check( 0, "FS_FCloseFile" ); }
int VM_CallArgs( vm_t *vm, int callNum, const int *args, int argCount ) {
	(void)vm; (void)callNum; (void)args; (void)argCount; Check( 0, "VM_CallArgs" ); return 0;
}
void CL_WritePacket( void ) { Check( 0, "CL_WritePacket" ); }
void SCR_StopCinematic( void ) { Check( 0, "SCR_StopCinematic" ); }
void S_ClearSoundBuffer( void ) { Check( 0, "S_ClearSoundBuffer" ); }
/* Connectionless packets are read out of band, never through the Huffman decoder. */
void Huff_offsetReceive( node_t *node, int *ch, byte *fin, int *offset, int maxoffset ) {
	(void)node; (void)ch; (void)fin; (void)offset; (void)maxoffset; Check( 0, "Huffman read" );
}
int Huff_getBit( byte *fout, int *offset ) { (void)fout; (void)offset; Check( 0, "Huffman read" ); return 0; }
/** Resolve every rconAddress name to one documentation address. */
qboolean Sys_StringToAdr( const char *name, netadr_t *address ) {
	Check( !strcmp( name, "rcon.example" ), "rcon host name" );
	memset( address, 0, sizeof( *address ) );
	address->type = NA_IP;
	memcpy( address->ip, resolvedIp, sizeof( resolvedIp ) );
	return qtrue;
}
/** Record every datagram the client puts on the wire. */
void Sys_SendPacket( int length, const void *data, netadr_t to ) {
	Check( length > 0 && length <= (int)sizeof( sent ), "packet length" );
	memcpy( sent, data, length );
	sentLength = length;
	sentTo = to;
	sends++;
}

/** Build an IPv4 source address with a host-order port. */
static netadr_t Address( int a, int b, int c, int d, int port ) {
	netadr_t address;
	memset( &address, 0, sizeof( address ) );
	address.type = NA_IP;
	address.ip[0] = a; address.ip[1] = b; address.ip[2] = c; address.ip[3] = d;
	address.port = BigShort( (short)port );
	return address;
}
/** Deliver "\xff\xff\xff\xff" + text through an exact-sized heap message. */
static void Deliver( netadr_t from, const char *text ) {
	msg_t msg;
	int length = 4 + (int)strlen( text );
	byte *data = malloc( length );
	Check( data != NULL, "packet allocation" );
	memset( data, 0xff, 4 );
	memcpy( data + 4, text, length - 4 );
	memset( &msg, 0, sizeof( msg ) );
	msg.data = data;
	msg.cursize = msg.maxsize = length;
	printed[0] = 0;
	CL_ConnectionlessPacket( from, &msg );
	free( data );
}
/** Send "print" text and report whether it reached the console and connect screen. */
static int PrintShown( netadr_t from, const char *text ) {
	char packet[256];
	clc.serverMessage[0] = 0;
	Com_sprintf( packet, sizeof( packet ), "print\n%s", text );
	Deliver( from, packet );
	if ( !strcmp( printed, text ) && !strcmp( clc.serverMessage, text ) ) {
		return 1;
	}
	Check( !printed[0] && !clc.serverMessage[0], "print partly shown" );
	return 0;
}
/** Send "echo" and report whether the client answered, checking any answer goes back to the sender. */
static int EchoAnswered( netadr_t from ) {
	int before = sends;
	Deliver( from, "echo payload" );
	if ( sends == before ) {
		return 0;
	}
	Check( sends == before + 1 && NET_CompareAdr( sentTo, from ), "echo reply destination" );
	Check( sentLength == 11 && !memcmp( sent, "\xff\xff\xff\xffpayload", 11 ), "echo reply bytes" );
	return 1;
}
/** Run the real console command "rcon status" and return where it went. */
static netadr_t Rcon( void ) {
	int before = sends;
	Cmd_TokenizeString( "rcon status" );
	CL_Rcon_f();
	Check( sends == before + 1 && !strcmp( sent + 4, "rcon secret status" ), "rcon packet" );
	return sentTo;
}

int main( void ) {
	netadr_t server, stranger, serverOtherPort, rcon, loopback;

	showpackets = &quiet;
	password.string = "secret";
	rcon_client_password = &password;
	rconTarget.string = "";
	rconAddress = &rconTarget;
	server = Address( 192, 0, 2, 10, 27960 );
	serverOtherPort = Address( 192, 0, 2, 10, 27961 );
	stranger = Address( 203, 0, 113, 66, 27960 );
	rcon = Address( 198, 51, 100, 20, 27960 );
	memset( &loopback, 0, sizeof( loopback ) );
	loopback.type = NA_LOOPBACK;

	// idle client: no one can make it print or answer
	cls.state = CA_DISCONNECTED;
	Check( !PrintShown( stranger, "Spoofed notice\n" ), "print from anyone while disconnected" );
	Check( !EchoAnswered( stranger ), "echo reflected while disconnected" );
	Check( sends == 0, "no packets while disconnected" );

	// connecting: the server's refusal reason still shows; others, even on the same host, are ignored
	cls.state = CA_CONNECTING;
	clc.serverAddress = server;
	Check( !PrintShown( stranger, "Server is full.\n" ), "print from stranger while connecting" );
	Check( !PrintShown( serverOtherPort, "Server is full.\n" ), "print from server host, other port" );
	Check( PrintShown( server, "Server is full.\n" ), "server refusal reason while connecting" );
	cls.state = CA_CHALLENGING;
	Check( PrintShown( server, "Server uses protocol version 68.\n" ), "server refusal reason while challenging" );
	Check( !EchoAnswered( stranger ), "echo reflected to stranger while challenging" );
	Check( EchoAnswered( server ), "server echo while challenging" );

	// a listen server talks over loopback
	clc.serverAddress = loopback;
	Check( PrintShown( loopback, "Server is for low pings only\n" ), "loopback server print" );
	Check( !PrintShown( server, "Server is full.\n" ), "old server after switching to loopback" );

	// a stale server address no longer counts once disconnected
	cls.state = CA_DISCONNECTED;
	clc.serverAddress = server;
	Check( !PrintShown( server, "Stale notice\n" ), "print from old server while disconnected" );
	Check( !EchoAnswered( server ), "echo to old server while disconnected" );

	// rcon without a connection: only the rconAddress target may answer
	rconTarget.string = "rcon.example";
	Check( NET_CompareAdr( Rcon(), rcon ), "rcon destination while disconnected" );
	Check( PrintShown( rcon, "map: q3dm17\n" ), "rcon reply while disconnected" );
	Check( !PrintShown( stranger, "map: fake\n" ), "print from stranger after rcon" );
	Check( EchoAnswered( rcon ), "rcon target echo" );
	Check( !EchoAnswered( stranger ), "echo reflected to stranger after rcon" );

	// rcon while connected goes to (and is answered from) the netchan peer, retiring the old target
	cls.state = CA_ACTIVE;
	clc.serverAddress = server;
	clc.netchan.remoteAddress = server;
	Check( NET_CompareAdr( Rcon(), server ), "rcon destination while connected" );
	Check( PrintShown( server, "status output\n" ), "rcon reply while connected" );
	Check( !PrintShown( rcon, "map: q3dm17\n" ), "previous rcon target after new rcon" );
	Check( !PrintShown( stranger, "Spoofed notice\n" ), "print from stranger while connected" );
	Check( !EchoAnswered( stranger ), "echo reflected to stranger while connected" );

	puts( "Client connectionless sender regressions passed (issue #275)" );
	return 0;
}
