/* Actual out-of-band printing must bound datagrams and preserve text data. */
#include Q3_FORMAT_NET_SOURCE

static byte capturedPacket[MAX_MSGLEN + 1];
static int capturedLength;
static int sends;

static void Check( int condition, const char *message ) {
	if ( !condition ) {
		fprintf( stderr, "Network format regression failed: %s\n", message );
		exit( 1 );
	}
}

void Sys_SendPacket( int length, const void *data, netadr_t to ) {
	Check( to.type == NA_IP, "packet retained its destination" );
	Check( length >= 4 && length < sizeof(capturedPacket),
		"packet length stays within the capture buffer" );
	memcpy( capturedPacket, data, length );
	capturedPacket[length] = 0;
	capturedLength = length;
	sends++;
}

void QDECL Com_Printf( const char *format, ... ) {
	(void)format;
}

void Com_Memcpy( void *dest, const void *src, const size_t count ) {
	memcpy( dest, src, count );
}

int main( void ) {
	cvar_t quiet;
	netadr_t address;
	char oversized[MAX_MSGLEN * 2];
	const char *hostile = "status %n %08x %s %% text";

	memset( &quiet, 0, sizeof(quiet) );
	memset( &address, 0, sizeof(address) );
	showpackets = &quiet;
	address.type = NA_IP;

	NET_OutOfBandPrint( NS_CLIENT, address, "%s", hostile );
	Check( sends == 1, "one hostile-text packet was sent" );
	Check( capturedPacket[0] == 0xff && capturedPacket[1] == 0xff
		&& capturedPacket[2] == 0xff && capturedPacket[3] == 0xff,
		"packet has the out-of-band header" );
	Check( capturedLength == 4 + strlen(hostile)
		&& !strcmp( (char *)capturedPacket + 4, hostile ),
		"percent-bearing text remains literal data" );

	memset( oversized, 'q', sizeof(oversized) );
	oversized[sizeof(oversized) - 1] = 0;
	NET_OutOfBandPrint( NS_SERVER, address, "%s", oversized );
	Check( sends == 2, "one oversized-text packet was sent" );
	Check( capturedLength == MAX_MSGLEN - 1,
		"oversized packet is truncated to the protocol buffer" );
	Check( !memcmp( capturedPacket + 4, oversized, MAX_MSGLEN - 5 )
		&& capturedPacket[capturedLength] == 0,
		"truncated packet contains bounded, terminated text" );

	puts( "Out-of-band literal formatting and packet capacity pass" );
	return 0;
}
