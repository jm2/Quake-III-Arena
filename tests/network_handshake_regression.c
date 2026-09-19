/* Actual server challenge-response formatter and compatibility split. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../code/server/server.h"

static netsrc_t capturedSocket;
static netadr_t capturedAddress;
static char capturedText[256];

static void Check( int condition, const char *message ) {
	if ( !condition ) {
		fprintf( stderr, "Network handshake regression failed: %s\n", message );
		exit( 1 );
	}
}

void QDECL NET_OutOfBandPrint( netsrc_t socket, netadr_t address,
	const char *format, ... ) {
	va_list arguments;

	capturedSocket = socket;
	capturedAddress = address;
	va_start( arguments, format );
	vsnprintf( capturedText, sizeof(capturedText), format, arguments );
	va_end( arguments );
}

static challenge_t Challenge( void ) {
	challenge_t challenge;

	memset( &challenge, 0, sizeof(challenge) );
	challenge.adr.type = NA_IP;
	challenge.adr.ip[0] = 198;
	challenge.adr.ip[1] = 51;
	challenge.adr.ip[2] = 100;
	challenge.adr.ip[3] = 22;
	challenge.adr.port = 0x1234;
	return challenge;
}

static void LegacyResponse( void ) {
	challenge_t challenge;

	challenge = Challenge();
	challenge.challenge = 1234;
	SV_SendChallengeResponse( &challenge );
	Check( capturedSocket == NS_SERVER &&
		!memcmp( &capturedAddress, &challenge.adr, sizeof(capturedAddress) ),
		"legacy response destination" );
	Check( !strcmp( capturedText, "challengeResponse 1234" ),
		"stock request receives stock response" );
}

static void SecureResponse( void ) {
	challenge_t challenge;

	challenge = Challenge();
	challenge.challenge = -7;
	challenge.clientChallengePresent = qtrue;
	challenge.clientChallenge = 19;
	SV_SendChallengeResponse( &challenge );
	Check( capturedSocket == NS_SERVER &&
		!memcmp( &capturedAddress, &challenge.adr, sizeof(capturedAddress) ),
		"secure response destination" );
	Check( !strcmp( capturedText, "challengeResponse -7 19 69" ),
		"extended request receives echoed nonce and protocol" );
}

int main( int argc, char **argv ) {
	Check( argc == 2, "one isolated case" );
	if ( atoi( argv[1] ) == 0 ) {
		LegacyResponse();
	} else {
		SecureResponse();
	}
	return 0;
}
