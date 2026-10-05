/* Issue #35: Cmd_ExecuteString forwards an unknown command to the server after
 * the client game, server game and UI command hooks have run.  A QVM EXEC_NOW
 * command passes text that lives in that module's image, and a hook can run a
 * nested command (vid_restart, map) that frees the module, so the forward must
 * not read the caller's text afterwards.  The real cmd.c runs here; the server
 * game hook frees the heap copy the command came from, as a nested vid_restart
 * frees the UI image, and the forward must still carry the original command. */
#include "../code/game/q_shared.h"
#include "../code/qcommon/qcommon.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

cvar_t *com_cl_running, *com_sv_running;
static cvar_t running;
static char *image, forwarded[MAX_STRING_CHARS];
static int forwards, hooks, freeInHook;

static void Check( int ok, const char *message ) {
	if ( !ok ) { fprintf( stderr, "Cmd forward text regression failed: %s\n", message ); exit( 1 ); }
}
void QDECL Com_Error( int level, const char *format, ... ) { (void)level; (void)format; Check( 0, "unexpected engine error" ); }
void QDECL Com_Printf( const char *fmt, ... ) { (void)fmt; }
void QDECL Com_DPrintf( const char *fmt, ... ) { (void)fmt; }
void Com_Memcpy( void *dest, const void *src, const size_t count ) { memcpy( dest, src, count ); }
void Com_Memset( void *dest, const int val, const size_t count ) { memset( dest, val, count ); }
qboolean Cvar_Command( void ) { return qfalse; }
qboolean CL_GameCommand( void ) { hooks++; return qfalse; }
/* a game QVM that runs an EXEC_NOW vid_restart: the UI image holding the text is freed */
qboolean SV_GameCommand( void ) {
	hooks++;
	if ( freeInHook ) {
		free( image );
		image = NULL;
	}
	return qfalse;
}
qboolean UI_GameCommand( void ) { hooks++; return qfalse; }
/* cl_main.c: a reliable command keeps MAX_STRING_CHARS - 1 characters */
void CL_ForwardCommandToServer( const char *string ) {
	Q_strncpyz( forwarded, string, sizeof( forwarded ) );
	forwards++;
}

/* Run one command from a heap "module image" and return what reached the server. */
static const char *Execute( const char *command, int freeImage ) {
	image = malloc( strlen( command ) + 1 );
	Check( image != NULL, "image allocation" );
	strcpy( image, command );
	freeInHook = freeImage;
	forwards = hooks = 0;
	forwarded[0] = 0;
	Cmd_ExecuteString( image );
	Check( forwards == 1 && hooks == 3, "unknown command reaches every hook and is forwarded once" );
	free( image );
	image = NULL;
	return forwarded;
}

int main( void ) {
	char longCommand[BIG_INFO_STRING];
	char expected[MAX_STRING_CHARS];

	running.integer = 1;
	com_cl_running = com_sv_running = &running;

	Check( !strcmp( Execute( "say hello world", 0 ), "say hello world" ), "ordinary forward" );
	Check( !strcmp( Execute( "say hello world", 1 ), "say hello world" ), "forward after the image is freed" );
	Check( !strcmp( Execute( "team", 1 ), "team" ), "single-token forward" );

	/* a long line forwards exactly what a reliable command keeps */
	memset( longCommand, 'a', sizeof( longCommand ) - 1 );
	memcpy( longCommand, "say ", 4 );
	longCommand[sizeof( longCommand ) - 1] = 0;
	Q_strncpyz( expected, longCommand, sizeof( expected ) );
	Check( !strcmp( Execute( longCommand, 0 ), expected ), "long forward keeps the reliable command length" );
	Check( !strcmp( Execute( longCommand, 1 ), expected ), "long forward after the image is freed" );

	puts( "Cmd forward text regression passed (issue #35)" );
	return 0;
}
