/* Issue #270: engine "no pass entity" sentinels must not fault the bounded game entity lookup. */
#include "../code/server/sv_world.c"
#include "../code/game/botlib.h"
#include "../code/qcommon/vm_local.h"
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

#define ENTITY_COUNT 5
#define ENTITY_STRIDE ((int)sizeof(sharedEntity_t) + 32)
#define WALL_X 80
#define BOX_HANDLE 255
void BotImport_Trace( bsp_trace_t *bsptrace, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int passent, int contentmask );
void BotImport_EntityTrace( bsp_trace_t *bsptrace, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int entnum, int contentmask );
int BotImport_PointContents( vec3_t point );
void SV_LocateGameData( sharedEntity_t *gEnts, int numGEntities, int sizeofGEntity_t, playerState_t *clients, int sizeofGameClient );
server_t sv;
static cvar_t maxclients;
cvar_t *sv_maxclients = &maxclients;
static vm_t gameVM;
vm_t *gvm = &gameVM;
static int image[ENTITY_COUNT * ENTITY_STRIDE / sizeof(int)];
static int expectError, clipped;
static jmp_buf errorJump;

/** Stop on any unexpected drop, entity clip, or trace result. */
static void Check( int ok, const char *message ) {
	if ( !ok ) { fprintf( stderr, "Server trace pass-entity regression failed: %s\n", message ); exit( 1 ); }
}
/** Accept only expected drops, attributed to the game VM. */
void QDECL Com_Error( int level, const char *format, ... ) {
	(void)format;
	Check( expectError && level == ERR_DROP && gameVM.interpretFaulted && !gameVM.currentlyInterpreting,
	       "unexpected server error" );
	longjmp( errorJump, 1 );
}
/** Ignore diagnostics from unused engine paths in this isolated fixture. */
void QDECL Com_Printf( const char *format, ... ) { (void)format; }
void QDECL Com_DPrintf( const char *format, ... ) { (void)format; }
void Com_Memset( void *dest, const int val, const size_t count ) { memset( dest, val, count ); }
/** Address registered slots directly so the fixture never masks SV_GentityNum faults. */
static sharedEntity_t *Entity( int num ) { return (sharedEntity_t *)( (byte *)image + num * ENTITY_STRIDE ); }
static int EntityForOrigin( const float *origin ) {
	int i;
	for ( i = 0; i < ENTITY_COUNT; i++ ) {
		if ( origin == Entity( i )->r.currentOrigin || origin == Entity( i )->s.origin ) return i;
	}
	Check( 0, "clip against an unregistered entity" ); return -1;
}
/** Collision stubs: the world is a solid plane at x = WALL_X, each entity a 16-unit box. */
clipHandle_t CM_InlineModel( int index ) { Check( index == 0, "inline model" ); return 0; }
clipHandle_t CM_TempBoxModel( const vec3_t mins, const vec3_t maxs, int capsule ) {
	(void)mins; (void)maxs; (void)capsule; return BOX_HANDLE;
}
void CM_ModelBounds( clipHandle_t model, vec3_t mins, vec3_t maxs ) {
	(void)model; VectorSet( mins, -512, -512, -512 ); VectorSet( maxs, 512, 512, 512 );
}
int CM_BoxLeafnums( const vec3_t mins, const vec3_t maxs, int *list, int listsize, int *lastLeaf ) {
	(void)mins; (void)maxs; (void)listsize; list[0] = 0; *lastLeaf = 0; return 1;
}
int CM_LeafArea( int leafnum ) { (void)leafnum; return 0; }
int CM_LeafCluster( int leafnum ) { (void)leafnum; return 0; }
/** Clip a +x move against a plane at x, filling the fields SV_Trace and botlib consume. */
static void PlaneTrace( trace_t *results, const vec3_t start, const vec3_t end, float x ) {
	int i;
	Com_Memset( results, 0, sizeof( *results ) );
	results->fraction = 1;
	if ( start[0] < x && end[0] >= x ) {
		results->fraction = ( x - start[0] ) / ( end[0] - start[0] );
		VectorSet( results->plane.normal, -1, 0, 0 ); results->plane.dist = -x;
		results->contents = CONTENTS_SOLID;
	}
	for ( i = 0; i < 3; i++ ) {
		results->endpos[i] = start[i] + results->fraction * ( end[i] - start[i] );
	}
}
void CM_BoxTrace( trace_t *results, const vec3_t start, const vec3_t end, vec3_t mins, vec3_t maxs,
                  clipHandle_t model, int brushmask, int capsule ) {
	(void)mins; (void)maxs; (void)capsule;
	Check( model == 0, "world trace model" );
	PlaneTrace( results, start, end, brushmask & CONTENTS_SOLID ? WALL_X : 1e9f );
}
void CM_TransformedBoxTrace( trace_t *results, const vec3_t start, const vec3_t end, vec3_t mins, vec3_t maxs,
                             clipHandle_t model, int brushmask, const vec3_t origin, const vec3_t angles, int capsule ) {
	(void)mins; (void)maxs; (void)brushmask; (void)angles; (void)capsule;
	Check( model == BOX_HANDLE, "entity trace model" );
	clipped |= 1 << EntityForOrigin( origin );
	PlaneTrace( results, start, end, fabs( start[1] - origin[1] ) <= 8 ? origin[0] - 8 : 1e9f );
}
int CM_PointContents( const vec3_t p, clipHandle_t model ) { (void)p; Check( model == 0, "world contents" ); return CONTENTS_FOG; }
int CM_TransformedPointContents( const vec3_t p, clipHandle_t model, const vec3_t origin, const vec3_t angles ) {
	(void)angles; Check( model == BOX_HANDLE, "entity contents model" );
	clipped |= 1 << EntityForOrigin( origin );
	return fabs( p[0] - origin[0] ) <= 8 && fabs( p[1] - origin[1] ) <= 8 ? CONTENTS_BODY : 0;
}
/** Link a registered box entity through the real server world sectors. */
static void Spawn( int num, float x, float y, int ownerNum, int contents ) {
	sharedEntity_t *ent = SV_GentityNum( num );
	ent->s.number = num; ent->r.ownerNum = ownerNum; ent->r.contents = contents;
	VectorSet( ent->r.mins, -8, -8, -8 ); VectorSet( ent->r.maxs, 8, 8, 8 );
	VectorSet( ent->r.currentOrigin, x, y, 0 ); VectorCopy( ent->r.currentOrigin, ent->s.origin );
	SV_LinkEntity( ent );
}
/** Trace a point along +x on row y and require the game VM to stay healthy. */
static trace_t Trace( float y, float length, int passEntityNum, int contentmask ) {
	trace_t trace;
	vec3_t start, end;
	VectorSet( start, 0, y, 0 ); VectorSet( end, length, y, 0 );
	clipped = 0;
	SV_Trace( &trace, start, NULL, NULL, end, passEntityNum, contentmask, qfalse );
	Check( !gameVM.interpretFaulted && gameVM.currentlyInterpreting, "trace faulted the game VM" );
	return trace;
}
/** Every out-of-range index must fault the game VM through the entity lookups. */
static void Reject( int operation, int num ) {
	trace_t trace;
	bsp_trace_t bsptrace;
	vec3_t start = {0, 0, 0}, end = {100, 0, 0}, box = {0, 0, 0};
	gameVM.interpretFaulted = qfalse; gameVM.currentlyInterpreting = qtrue; expectError = 1;
	if ( setjmp( errorJump ) == 0 ) {
		if ( operation == 0 ) SV_GentityNum( num );
		else if ( operation == 1 ) SV_ClipToEntity( &trace, start, vec3_origin, vec3_origin, end, num, CONTENTS_SOLID, qfalse );
		else BotImport_EntityTrace( &bsptrace, start, box, box, end, num, CONTENTS_SOLID );
		Check( 0, "out-of-range entity index accepted" );
	}
	expectError = 0; gameVM.interpretFaulted = qfalse; gameVM.currentlyInterpreting = qtrue;
}
/** Cover botlib's passent -1, other unregistered indices, valid pass entities, and strict lookups. */
int main( void ) {
	int sentinels[] = {-1, -2, INT_MIN, ENTITY_COUNT, ENTITYNUM_WORLD, MAX_GENTITIES, INT_MAX};
	int i, mask = CONTENTS_SOLID | CONTENTS_PLAYERCLIP;
	trace_t none, trace;
	bsp_trace_t bsptrace;
	vec3_t start = {0, 200, 0}, end = {100, 200, 0}, box = {0, 0, 0}, point = {40, 0, 0};

	maxclients.integer = 2;
	SV_LocateGameData( (sharedEntity_t *)image, ENTITY_COUNT, ENTITY_STRIDE, NULL, sizeof(playerState_t) );
	SV_ClearWorld();
	Spawn( 0, 20, 0, 3, CONTENTS_BODY );			// pass entity, owned by entity 3
	Spawn( 1, 40, 0, 0, CONTENTS_SOLID );			// its own missile
	Spawn( 2, 50, 0, 3, CONTENTS_SOLID );			// another missile from its owner
	Spawn( 3, 60, 0, ENTITYNUM_NONE, CONTENTS_SOLID );	// the owner itself
	Spawn( 4, 40, 200, ENTITYNUM_NONE, CONTENTS_TRIGGER );	// floating item trigger
	gameVM.currentlyInterpreting = qtrue;

	// ENTITYNUM_NONE clips every entity; a valid pass entity keeps the 1.32c filters
	none = Trace( 0, 100, ENTITYNUM_NONE, mask | CONTENTS_BODY );
	Check( none.entityNum == 0 && none.fraction == 0.12f && clipped == 0xf, "ENTITYNUM_NONE entity clipping" );
	trace = Trace( 0, 100, 0, mask | CONTENTS_BODY );
	Check( trace.entityNum == 3 && trace.fraction == 0.52f && clipped == 1 << 3, "valid pass entity filters" );

	// out-of-range pass entities, led by botlib's -1, behave exactly like ENTITYNUM_NONE
	for ( i = 0; i < sizeof(sentinels) / sizeof(sentinels[0]); i++ ) {
		trace = Trace( 0, 100, sentinels[i], mask | CONTENTS_BODY );
		Check( !memcmp( &trace, &none, sizeof(trace) ) && clipped == 0xf, "sentinel pass entity clipping" );
		trace = Trace( 200, 100, sentinels[i], mask );
		Check( trace.entityNum == ENTITYNUM_WORLD && trace.fraction == 0.8f && trace.endpos[0] == WALL_X &&
		       trace.plane.normal[0] == -1 && !clipped, "sentinel world-only clip" );
		trace = Trace( 200, 32, sentinels[i], mask );
		Check( trace.entityNum == ENTITYNUM_NONE && trace.fraction == 1 && trace.endpos[0] == 32, "sentinel open trace" );
	}

	// the BotInitLevelItems import path: passent -1 over a floating item
	BotImport_Trace( &bsptrace, start, box, box, end, -1, mask );
	Check( !gameVM.interpretFaulted && bsptrace.ent == ENTITYNUM_WORLD && bsptrace.fraction == 0.8f &&
	       bsptrace.endpos[0] == WALL_X, "botlib passent -1 trace" );

	// point contents: -1 is only compared, never looked up
	clipped = 0;
	Check( SV_PointContents( point, ENTITYNUM_NONE ) == ( CONTENTS_FOG | CONTENTS_BODY ) && clipped == 1 << 1,
	       "ENTITYNUM_NONE point contents" );
	Check( SV_PointContents( point, -1 ) == ( CONTENTS_FOG | CONTENTS_BODY ), "-1 point contents" );
	Check( BotImport_PointContents( point ) == ( CONTENTS_FOG | CONTENTS_BODY ), "botlib point contents" );
	Check( SV_PointContents( point, 1 ) == CONTENTS_FOG, "valid pass entity point contents" );
	Check( !gameVM.interpretFaulted, "point contents faulted the game VM" );

	// direct lookups keep rejecting every unregistered index
	for ( i = 0; i < sizeof(sentinels) / sizeof(sentinels[0]); i++ ) {
		Reject( 0, sentinels[i] ); Reject( 1, sentinels[i] ); Reject( 2, sentinels[i] );
	}
	Reject( 0, ENTITYNUM_NONE );
	puts( "Server trace pass-entity sentinel regressions passed (issue #270)" );
	return 0;
}
