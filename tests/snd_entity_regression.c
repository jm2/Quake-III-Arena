/* Issue #238: the real sound entity guards accept the last slot and reject MAX_GENTITIES. */
#include "../code/client/snd_dma.c"
#include <limits.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

static loopSound_t beforeLoops[MAX_GENTITIES];
static int expectError;
static jmp_buf errorJump;

/** Fail when a sound entity bound differs. */
static void Check( int ok, const char *message ) {
	if ( !ok ) { fprintf( stderr, "Sound entity regression failed: %s\n", message ); exit( 1 ); }
}
/** Catch controlled drops and prove the loop-sound table did not change. */
void QDECL Com_Error( int level, const char *format, ... ) {
	(void)format;
	Check( expectError && level == ERR_DROP, "unexpected engine error" );
	Check( !memcmp( beforeLoops, loopSounds, sizeof(loopSounds) ), "rejection changed loop sounds" );
	longjmp( errorJump, 1 );
}
/** Ignore the out-of-range handle warning used to stop a valid start before mixing. */
void QDECL Com_Printf( const char *format, ... ) { (void)format; }
/** Supply the sound clock for paths that pass the entity guard. */
int Com_Milliseconds( void ) { return 0; }
/** Fail if a start passes the handle check; these fixtures never register sounds. */
qboolean S_LoadSound( sfx_t *sfx ) { (void)sfx; Check( 0, "unexpected sound load" ); return qfalse; }

/** Require one entity number to drop before the position or start is used. */
static void Reject( int start, int entityNum ) {
	vec3_t origin = {7, 8, 9};
	memcpy( beforeLoops, loopSounds, sizeof(loopSounds) );
	expectError = 1;
	if ( setjmp( errorJump ) == 0 ) {
		if ( start ) S_StartSound( NULL, entityNum, CHAN_AUTO, 0 );
		else S_UpdateEntityPosition( entityNum, origin );
		Check( 0, "invalid sound entity accepted" );
	}
	expectError = 0;
}
/** Cover both native entity guards at, below, and far outside the table end. */
int main( void ) {
	const int bad[] = {-2, -1, MAX_GENTITIES, INT_MIN, INT_MAX};
	vec3_t origin = {1, 2, 3};
	int i;
	S_UpdateEntityPosition( 0, origin );
	S_UpdateEntityPosition( MAX_GENTITIES - 1, origin );
	Check( loopSounds[0].origin[0] == 1 && loopSounds[MAX_GENTITIES - 1].origin[2] == 3, "valid positions" );
	for ( i = 0; i < (int)(sizeof(bad) / sizeof(bad[0])); i++ ) Reject( 0, bad[i] );
	s_soundStarted = 1;
	s_numSfx = 0;
	S_StartSound( NULL, 0, CHAN_AUTO, 0 );
	S_StartSound( NULL, MAX_GENTITIES - 1, CHAN_AUTO, 0 );
	for ( i = 0; i < (int)(sizeof(bad) / sizeof(bad[0])); i++ ) Reject( 1, bad[i] );
	puts( "Sound entity regressions passed (issue #238)" );
	return 0;
}
