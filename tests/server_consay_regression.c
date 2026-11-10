/* SV_ConSay_f (code/server/sv_ccmds.c): the server console's "say" built
 * "console: " plus Cmd_Args() in a 1024-byte buffer with strcpy/strcat, and
 * Cmd_Args can return 1023 characters, so a long say overran the stack by
 * up to 9 bytes (from the console, or rcon with the password).  It now stops
 * at the buffer; short messages are unchanged, quotes included. */
#include "../code/game/q_shared.h"
#include "../code/qcommon/qcommon.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct client_s client_t;
static cvar_t	running;
cvar_t			*com_sv_running = &running;
static char		sent[4096];
static int		sends;

void QDECL Com_Printf( const char *fmt, ... ) { (void)fmt; }
void QDECL Com_Error( int level, const char *fmt, ... ) { (void)level; fprintf( stderr, "Com_Error: %s\n", fmt ); exit( 1 ); }
void SV_SendServerCommand( client_t *cl, const char *fmt, ... ) {
	va_list	ap;
	(void)cl;
	va_start( ap, fmt );
	vsnprintf( sent, sizeof( sent ), fmt, ap );
	va_end( ap );
	sends++;
}

#include "server_consay_extracted.c"

static int failures;
static void Check( int ok, const char *what ) {
	if ( !ok ) {
		fprintf( stderr, "server console say regression failed: %s (sent \"%.80s\")\n", what, sent );
		failures++;
	}
}

static void Say( const char *line ) {
	sends = 0;
	sent[0] = 0;
	Cmd_TokenizeString( line );
	SV_ConSay_f();
}

int main( void ) {
	char	line[MAX_STRING_CHARS + 16];

	running.integer = 1;
	Say( "say hello there" );
	Check( sends == 1 && !strcmp( sent, "chat \"console: hello there\n\"" ), "a short say is unchanged" );
	Say( "say \"quoted words\"" );
	Check( !strcmp( sent, "chat \"console: quoted words\n\"" ), "a quoted say loses its quotes" );

	/* the longest Cmd_Args can return: 1023 characters */
	memset( line, 'x', sizeof( line ) );
	memcpy( line, "say ", 4 );
	line[4 + MAX_STRING_CHARS - 1] = 0;
	Say( line );
	Check( sends == 1 && strlen( sent ) == strlen( "chat \"" ) + 1023 + strlen( "\n\"" ),
		"a long say stops at the 1024-byte buffer" );
	Check( !strncmp( sent, "chat \"console: xxx", 18 ), "and keeps its start" );

	running.integer = 0;
	Say( "say hello" );
	Check( sends == 0, "nothing is sent without a server" );
	if ( failures ) {
		return 1;
	}
	puts( "server console say regression passed" );
	return 0;
}
