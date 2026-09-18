/* Issue #35: exercise real server registration and persistent array access. */
#include "../code/server/sv_game.c"
#include "../code/qcommon/vm_local.h"
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

#define IMAGE_SIZE 32768
server_t sv;
static cvar_t maxclients;
cvar_t *sv_maxclients = &maxclients;
static server_t beforeServer;
static byte before[IMAGE_SIZE];
static vm_t vm, otherVM;
vm_t *gvm = &vm;
static int expectError;
static jmp_buf errorJump;

/** Fail when storage ownership, indices, or registration differ. */
static void Check( int ok, const char *message ) {
	if ( !ok ) { fprintf( stderr, "Server game-data regression failed: %s\n", message ); exit( 1 ); }
}
/** Catch expected drops and require unchanged VM data and server registration. */
void QDECL Com_Error( int level, const char *format, ... ) {
	(void)format;
	Check( expectError && level == ERR_DROP, "unexpected engine error" );
	Check( vm.interpretFaulted && !vm.currentlyInterpreting && !otherVM.interpretFaulted, "responsible VM fault state" );
	Check( !memcmp( before, vm.dataBase, IMAGE_SIZE ), "rejection changed VM data" );
	Check( !memcmp( &beforeServer, &sv, sizeof(sv) ), "rejection changed registration" );
	longjmp( errorJump, 1 );
}
/** Ignore diagnostics from unused engine paths in this isolated fixture. */
void QDECL Com_Printf( const char *format, ... ) { (void)format; }
/** Reject malformed registration, index, or entity slot before any mutation. */
static void Reject( int op, int *args ) {
	vm.interpretFaulted = qfalse; vm.currentlyInterpreting = qtrue;
	currentVM = op == 0 ? &vm : &otherVM;
	memcpy( before, vm.dataBase, IMAGE_SIZE ); beforeServer = sv;
	expectError = 1;
	if ( setjmp( errorJump ) == 0 ) {
		if ( op == 0 ) SV_GameLocateData( args );
		else if ( op == 1 ) SV_GentityNum( args[0] );
		else if ( op == 2 ) SV_GameClientNum( args[0] );
		else if ( op == 3 ) SV_NumForGentity( (sharedEntity_t *)((unsigned long)vm.dataBase + args[0]) );
		else SV_SvEntityForGentity( (sharedEntity_t *)sv.gentities );
		Check( 0, "invalid game data accepted" );
	}
	expectError = 0;
}
/** Verify private strides, image boundaries, retained capacities, and controlled errors. */
int main( void ) {
	int entityStride = sizeof(sharedEntity_t) + 32, clientStride = sizeof(playerState_t) + 32;
	int args[6] = {G_LOCATE_GAME_DATA, 4, 3, entityStride, IMAGE_SIZE - 2 * clientStride, clientStride};
	int bad[] = {-1, INT_MIN, INT_MAX};
	int indices[1], i, field, original;
	sharedEntity_t *last;
	vm.dataBase = malloc( IMAGE_SIZE ); Check( vm.dataBase != NULL, "allocation" );
	vm.dataMask = IMAGE_SIZE - 1; currentVM = &vm; maxclients.integer = 2;
	memset( vm.dataBase, 0, IMAGE_SIZE );
	SV_GameLocateData( args );
	Check( (byte *)sv.gentities == vm.dataBase + 4 && sv.num_entities == 3 &&
	       sv.gentitySize == entityStride, "entity registration" );
	Check( (byte *)sv.gameClients == vm.dataBase + args[4] && sv.gameClientCount == 2 &&
	       sv.gameClientSize == clientStride, "client registration" );
	last = SV_GentityNum( 2 );
	Check( (byte *)last == vm.dataBase + 4 + 2 * entityStride && SV_NumForGentity(last) == 2,
	       "private entity stride" );
	Check( (byte *)SV_GameClientNum( 1 ) == vm.dataBase + IMAGE_SIZE - clientStride,
	       "last registered client" );
	sv.gentities->s.number = 0;
	Check( SV_SvEntityForGentity(sv.gentities) == &sv.svEntities[0], "server entity lookup" );
	args[1] += IMAGE_SIZE; args[4] += IMAGE_SIZE; SV_GameLocateData( args );
	args[1] -= IMAGE_SIZE; args[4] -= IMAGE_SIZE;
	args[1] += INT_MIN; args[4] += INT_MIN; SV_GameLocateData( args );
	args[1] -= INT_MIN; args[4] -= INT_MIN;
	for ( field = 1; field <= 5; field++ ) {
		original = args[field];
		for ( i = 0; i < sizeof(bad) / sizeof(bad[0]); i++ ) {
			if ( (field == 1 || field == 4) && bad[i] == INT_MIN ) continue;
			args[field] = bad[i]; Reject( 0, args );
		}
		args[field] = 0; Reject( 0, args ); args[field] = original;
	}
	original = args[2]; args[2] = MAX_GENTITIES + 1; Reject( 0, args );
	args[2] = 1; Reject( 0, args ); args[2] = original;
	original = args[3]; args[3] = sizeof(sharedEntity_t) - 4; Reject( 0, args );
	args[3] = original + 1; Reject( 0, args ); args[3] = original;
	original = args[5]; args[5] = sizeof(playerState_t) - 4; Reject( 0, args );
	args[5] = original + 1; Reject( 0, args ); args[5] = original;
	args[1] = IMAGE_SIZE - 4; Reject( 0, args ); args[1] = 5; Reject( 0, args ); args[1] = 4;
	args[4] += 4; Reject( 0, args ); args[4] -= 3; Reject( 0, args ); args[4] -= 1;
	for ( i = 0; i < sizeof(bad) / sizeof(bad[0]); i++ ) {
		indices[0] = bad[i]; Reject( 1, indices ); Reject( 2, indices );
	}
	indices[0] = sv.num_entities; Reject( 1, indices );
	indices[0] = sv.gameClientCount; Reject( 2, indices );
	maxclients.integer = MAX_CLIENTS; Reject( 2, indices ); maxclients.integer = 2;
	indices[0] = 0; Reject( 3, indices );
	indices[0] = 5; Reject( 3, indices );
	indices[0] = 4 + 3 * entityStride; Reject( 3, indices );
	sv.gentities->s.number = -1; Reject( 4, indices );
	sv.gentities->s.number = MAX_GENTITIES; Reject( 4, indices );
	free( vm.dataBase );
	puts( "Server game-data registration regressions passed (issue #35)" );
	return 0;
}
