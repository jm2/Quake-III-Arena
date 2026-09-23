/* Issue #241: the compiled cgame particles must be the retail 1.32 cg_marks.c section. */
#include "../code/cgame/cg_particles.c"
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RETAIL_PARTICLES	1024
#define RETAIL_FRAMES		23
#define MAX_REGISTERED		256

cg_t cg;
cgs_t cgs;
static snapshot_t snap;
static char registered[MAX_REGISTERED][MAX_QPATH];
static int numRegistered, polyCalls, polyShader, expectError;
static polyVert_t polyVerts[4];
static jmp_buf errorJump;

/** Fail when the compiled particle system differs from the retail cgame. */
static void Check( int ok, const char *message ) {
	if ( !ok ) { fprintf( stderr, "Cgame particle regression failed: %s\n", message ); exit( 1 ); }
}
/** Record every shader the particle system asks the renderer to load. */
qhandle_t trap_R_RegisterShader( const char *name ) {
	Check( numRegistered < MAX_REGISTERED, "too many shader registrations" );
	Q_strncpyz( registered[numRegistered], name, sizeof(registered[0]) );
	return ++numRegistered;
}
/** Capture the explosion sprite submitted to the scene. */
void trap_R_AddPolyToScene( qhandle_t shader, int numVerts, const polyVert_t *verts ) {
	Check( numVerts == 4, "explosion sprite vertex count" );
	memcpy( polyVerts, verts, sizeof(polyVerts) );
	polyShader = shader;
	polyCalls++;
}
/** Reject only the expected unknown animation lookup. */
void QDECL CG_Error( const char *msg, ... ) {
	(void)msg;
	Check( expectError, "unexpected cgame error" );
	longjmp( errorJump, 1 );
}
void QDECL Com_Error( int level, const char *error, ... ) { (void)level; (void)error; Check( 0, "engine error" ); }
void QDECL Com_Printf( const char *msg, ... ) { (void)msg; }
void QDECL CG_Printf( const char *msg, ... ) { (void)msg; }
const char *CG_ConfigString( int index ) { (void)index; return ""; }
void CG_Trace( trace_t *result, const vec3_t start, const vec3_t mins, const vec3_t maxs,
	const vec3_t end, int skipNumber, int mask ) {
	(void)start; (void)mins; (void)maxs; (void)end; (void)skipNumber; (void)mask;
	memset( result, 0, sizeof(*result) );
	result->fraction = 1.0f;
}

/** Require one sprite corner at the retail unstretched explosion size. */
static void CheckVert( int index, float y, float z, float s, float t ) {
	const polyVert_t *v = &polyVerts[index];
	Check( v->xyz[0] == 100.0f && v->xyz[1] == y && v->xyz[2] == z, "explode1 sprite geometry" );
	Check( v->st[0] == s && v->st[1] == t, "explode1 sprite texture coordinates" );
	Check( v->modulate[0] == 255 && v->modulate[1] == 255 && v->modulate[2] == 255 &&
	       v->modulate[3] == 255, "explode1 sprite modulate" );
}

int main( void ) {
	vec3_t origin = { 100, 0, 0 }, vel = { 0, 0, 0 };
	cparticle_t *p;
	char name[MAX_QPATH];
	int i, count;

	/* retail MAX_PARTICLES is 1024, not the orphan's 1024 * 8 */
	Check( sizeof(particles) / sizeof(particles[0]) == RETAIL_PARTICLES, "particle pool size" );
	Check( cl_numparticles == RETAIL_PARTICLES, "particle count" );

	/* only the 23 retail explode1 frames are registered */
	cg.time = 1000;
	CG_ClearParticles();
	Check( numShaderAnims == 1, "animation set count" );
	Check( numRegistered == RETAIL_FRAMES, "registered shader count" );
	for ( i = 0; i < RETAIL_FRAMES; i++ ) {
		Com_sprintf( name, sizeof(name), "explode1%i", i + 1 );
		Check( !strcmp( registered[i], name ), "registered shader name" );
	}
	for ( count = 0, p = free_particles; p; p = p->next ) {
		count++;
	}
	Check( count == RETAIL_PARTICLES && !active_particles, "free particle list" );

	/* the orphan's extra animation sets are unknown to retail */
	expectError = 1;
	if ( setjmp( errorJump ) == 0 ) {
		CG_ParticleExplosion( "blacksmokeanim", origin, vel, -1400, 20, 30 );
		Check( 0, "non-retail animation accepted" );
	}
	expectError = 0;
	Check( !active_particles, "rejected animation allocated a particle" );

	/* explode1 uses a 1.0 stretch ratio and 0.5 alpha */
	CG_ParticleExplosion( "explode1", origin, vel, -1400, 20, 30 );
	p = active_particles;
	Check( p && p->type == P_ANIM && p->shaderAnim == 0 && p->roll == 0, "explode1 particle" );
	Check( p->alpha == 0.5f && p->alphavel == 0, "explode1 alpha" );
	Check( p->width == 20 && p->height == 20 && p->endwidth == 30 && p->endheight == 30,
	       "explode1 stretch ratio" );
	Check( p->endtime == 2400, "explode1 duration" );

	/* halfway through, frame 12 is drawn as an unstretched 50x50 sprite */
	cg.snap = &snap;
	AxisClear( cg.refdef.viewaxis );
	cg.time = 1700;
	CG_AddParticles();
	Check( polyCalls == 1 && polyShader == 12 && !strcmp( registered[11], "explode112" ),
	       "explode1 frame" );
	CheckVert( 0, -25, -25, 0, 0 );
	CheckVert( 1, -25, 25, 0, 1 );
	CheckVert( 2, 25, 25, 1, 1 );
	CheckVert( 3, 25, -25, 1, 0 );

	puts( "cgame particle regression passed" );
	return 0;
}
