/* Issue #238: drive the real cgame dispatcher with key numbers and sound entity numbers. */
#include "../code/client/cl_cgame.c"
#include "client_syscall_stubs.h"

#define COUNT(array) ((int)(sizeof(array) / sizeof((array)[0])))

/* cl_ui.c owns the catcher accessors in the engine. */
int Key_GetCatcher( void ) { Unexpected( __func__ ); return 0; }
void Key_SetCatcher( int catcher ) { Unexpected( __func__ ); }

/** Reject out-of-table indices on every key and sound entity trap and keep valid results unchanged. */
int main( void ) {
	const int badKeys[] = {-2, MAX_KEYS, INT_MIN, INT_MAX};
	const int goodKeys[] = {-1, 0, MAX_KEYS - 1};
	const int badEntities[] = {-2, -1, MAX_GENTITIES, INT_MIN, INT_MAX};
	const int goodEntities[] = {0, ENTITYNUM_WORLD, MAX_GENTITIES - 1};
	const int entityTraps[] = {CG_S_ADDLOOPINGSOUND, CG_S_ADDREALLOOPINGSOUND, CG_S_UPDATEENTITYPOSITION,
	                           CG_S_STOPLOOPINGSOUND, CG_S_RESPATIALIZE};
	int args[5], i, j, entity;
	float *origin;
	SetupVM();
	origin = (float *)(vm.dataBase + 16);
	VectorSet( origin, 1, 2, 3 );
	for ( i = 0; i < COUNT(badKeys); i++ ) {
		args[0] = CG_KEY_ISDOWN; args[1] = badKeys[i];
		Reject( CL_CgameSystemCalls, args );
	}
	for ( i = 0; i < COUNT(goodKeys); i++ ) {
		args[0] = CG_KEY_ISDOWN; args[1] = goodKeys[i];
		if ( goodKeys[i] != -1 ) keyDown[goodKeys[i]] = qtrue;
		Check( Accept( CL_CgameSystemCalls, args, goodKeys[i] ) == (goodKeys[i] != -1), "valid key state" );
	}
	// origin, velocity or axis, then sfx handle or inwater
	for ( j = 0; j < COUNT(entityTraps); j++ ) {
		for ( i = 0; i < COUNT(badEntities); i++ ) {
			args[0] = entityTraps[j]; args[1] = badEntities[i]; args[2] = 16; args[3] = 64; args[4] = 0;
			Reject( CL_CgameSystemCalls, args );
		}
		for ( i = 0; i < COUNT(goodEntities); i++ ) {
			entity = goodEntities[i];
			args[0] = entityTraps[j]; args[1] = entity; args[2] = 16; args[3] = 64; args[4] = 0;
			VectorClear( soundOrigins[entity] ); soundActive[entity] = qtrue; soundListener = -1;
			Accept( CL_CgameSystemCalls, args, entity );
			if ( entityTraps[j] == CG_S_STOPLOOPINGSOUND ) {
				Check( !soundActive[entity], "valid loop stopped" );
			} else if ( entityTraps[j] == CG_S_RESPATIALIZE ) {
				Check( soundListener == entity, "valid listener" );
			} else {
				Check( soundOrigins[entity][0] == 1 && soundOrigins[entity][2] == 3, "valid entity origin" );
			}
		}
	}
	// an unpositioned sound follows its entity; a positioned one only tags its channel
	for ( i = 0; i < COUNT(badEntities); i++ ) {
		args[0] = CG_S_STARTSOUND; args[1] = 0; args[2] = badEntities[i]; args[3] = CHAN_AUTO; args[4] = 0;
		Reject( CL_CgameSystemCalls, args );
	}
	for ( i = 0; i < COUNT(goodEntities); i++ ) {
		args[0] = CG_S_STARTSOUND; args[1] = 0; args[2] = goodEntities[i]; args[3] = CHAN_AUTO; args[4] = 0;
		Accept( CL_CgameSystemCalls, args, goodEntities[i] );
	}
	args[0] = CG_S_STARTSOUND; args[1] = 16; args[2] = -1; args[3] = CHAN_VOICE; args[4] = 0;
	Accept( CL_CgameSystemCalls, args, -1 );
	args[2] = ENTITYNUM_NONE;
	Accept( CL_CgameSystemCalls, args, ENTITYNUM_NONE );
	free( vm.dataBase );
	puts( "Cgame syscall index regressions passed (issue #238)" );
	return 0;
}
