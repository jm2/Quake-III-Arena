/* Issue #35: refEntity_t fields from a cgame/ui QVM must not index outside an MD3 in the real R_AddMD3Surfaces. */
#include "renderer_image_gl_stub.h"
#include "../code/renderer/tr_mesh.c"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SHADER_COUNT 3
#define FRAME_COUNT 2

trGlobals_t tr;
refimport_t ri;
cvar_t *r_lodscale, *r_lodbias, *r_shadows;
static cvar_t lodScale, lodBias, shadows;
static shader_t defaultShader, shaders[1 + SHADER_COUNT], customShader, skinShader;
static model_t model;
static byte *buffer;
static md3Surface_t *surface;
static shader_t *drawn;
static int draws;

static void Check( int ok, const char *message ) {
	if ( !ok ) {
		fprintf( stderr, "MD3 entity bounds regression failed: %s\n", message );
		exit( 1 );
	}
}

void QDECL Com_Error( int level, const char *format, ... ) { (void)level; (void)format; Check( 0, "unexpected Com_Error" ); }
void QDECL Com_Printf( const char *format, ... ) { (void)format; }
static void QDECL Print( int level, const char *format, ... ) { (void)level; (void)format; }
shader_t *R_GetShaderByHandle( qhandle_t hShader ) {
	/* tr_shader.c's range check: out-of-range handles select the default shader */
	if ( hShader == 1 ) {
		return &customShader;
	}
	return tr.defaultShader;
}
skin_t *R_GetSkinByHandle( qhandle_t hSkin ) {
	Check( hSkin > 0 && hSkin < tr.numSkins, "R_AddMD3Surfaces only looks up skins inside tr.numSkins" );
	return tr.skins[hSkin];
}
int R_CullLocalBox( vec3_t bounds[2] ) { (void)bounds; return CULL_IN; }
int R_CullPointAndRadius( vec3_t origin, float radius ) { (void)origin; (void)radius; return CULL_IN; }
int R_CullLocalPointAndRadius( vec3_t origin, float radius ) { (void)origin; (void)radius; return CULL_IN; }
void R_SetupEntityLighting( const trRefdef_t *refdef, trRefEntity_t *ent ) { (void)refdef; (void)ent; }
void R_AddDrawSurf( surfaceType_t *surf, shader_t *shader, int fogIndex, int dlightMap ) {
	Check( (void *)surf == (void *)surface && fogIndex == 0 && !dlightMap, "one MD3 surface is drawn" );
	drawn = shader;
	draws++;
}

/*
Build one MD3 whose shader array starts the allocation, so any index before
it lands in ASan's left redzone instead of in the header or surface bytes.
*/
static void BuildModel( void ) {
	int shadersSize = SHADER_COUNT * sizeof( md3Shader_t );
	int headerOffset = shadersSize;
	int framesOffset = headerOffset + sizeof( md3Header_t );
	int surfaceOffset = framesOffset + FRAME_COUNT * sizeof( md3Frame_t );
	int total = surfaceOffset + sizeof( md3Surface_t );
	md3Header_t *header;
	md3Shader_t *md3Shaders;
	md3Frame_t *frames;
	int i;

	buffer = calloc( 1, total );
	Check( buffer != NULL, "model allocation" );
	md3Shaders = (md3Shader_t *)buffer;
	header = (md3Header_t *)( buffer + headerOffset );
	frames = (md3Frame_t *)( buffer + framesOffset );
	surface = (md3Surface_t *)( buffer + surfaceOffset );
	for ( i = 0 ; i < SHADER_COUNT ; i++ ) {
		md3Shaders[i].shaderIndex = 1 + i;
	}
	for ( i = 0 ; i < FRAME_COUNT ; i++ ) {
		VectorSet( frames[i].bounds[0], -1, -1, -1 );
		VectorSet( frames[i].bounds[1], 1, 1, 1 );
		frames[i].radius = 2;
	}
	header->ident = MD3_IDENT;
	header->version = MD3_VERSION;
	header->numFrames = FRAME_COUNT;
	header->numSurfaces = 1;
	header->ofsFrames = framesOffset - headerOffset;
	header->ofsSurfaces = surfaceOffset - headerOffset;
	header->ofsEnd = total - headerOffset;
	surface->ident = SF_MD3;
	Q_strncpyz( surface->name, "h_head", sizeof( surface->name ) );
	surface->numFrames = FRAME_COUNT;
	surface->numShaders = SHADER_COUNT;
	surface->ofsShaders = -surfaceOffset;
	surface->ofsEnd = sizeof( md3Surface_t );

	model.type = MOD_MESH;
	model.numLods = 1;
	model.md3[0] = header;
}

static void Setup( void ) {
	static skinSurface_t skinSurface;
	static skin_t defaultSkin, skin;
	int i;

	ri.Printf = Print;
	lodScale.value = 5;
	r_lodscale = &lodScale;
	r_lodbias = &lodBias;
	r_shadows = &shadows;
	tr.defaultShader = &defaultShader;
	for ( i = 0 ; i <= SHADER_COUNT ; i++ ) {
		shaders[i].index = i;
		shaders[i].sort = SS_OPAQUE;
		tr.shaders[i] = &shaders[i];
	}
	tr.numShaders = 1 + SHADER_COUNT;
	Q_strncpyz( skinSurface.name, "h_head", sizeof( skinSurface.name ) );
	skinSurface.shader = &skinShader;
	skin.numSurfaces = 1;
	skin.surfaces[0] = &skinSurface;
	tr.skins[0] = &defaultSkin;
	tr.skins[1] = &skin;
	tr.numSkins = 2;
	tr.refdef.rdflags = RDF_NOWORLDMODEL;
	tr.currentModel = &model;
	BuildModel();
}

/* Run the real renderer front end on one entity and return the shader it picked. */
static shader_t *Draw( int skinNum, int customSkin, int customShaderHandle, int frame, int oldframe, int renderfx ) {
	trRefEntity_t ent;

	memset( &ent, 0, sizeof( ent ) );
	ent.e.reType = RT_MODEL;
	ent.e.skinNum = skinNum;
	ent.e.customSkin = customSkin;
	ent.e.customShader = customShaderHandle;
	ent.e.frame = frame;
	ent.e.oldframe = oldframe;
	ent.e.renderfx = renderfx;
	AxisClear( ent.e.axis );
	drawn = NULL;
	draws = 0;
	R_AddMD3Surfaces( &ent );
	Check( draws == 1, "entity draws its surface" );
	Check( ent.e.frame >= 0 && ent.e.frame < FRAME_COUNT && ent.e.oldframe >= 0 && ent.e.oldframe < FRAME_COUNT,
		"frames are validated before use" );
	return drawn;
}

int main( void ) {
	int skinNum;

	Setup();

	/* Non-negative skin numbers keep retail's modulo selection */
	for ( skinNum = 0 ; skinNum < 2 * SHADER_COUNT ; skinNum++ ) {
		Check( Draw( skinNum, 0, 0, 0, 0, 0 ) == &shaders[1 + skinNum % SHADER_COUNT], "retail skinNum selection" );
	}
	Check( Draw( INT_MAX, 0, 0, 0, 0, 0 ) == &shaders[1 + INT_MAX % SHADER_COUNT], "largest skinNum" );

	/* Negative skin numbers wrap into the shader array instead of reading before it */
	Check( Draw( -1, 0, 0, 0, 0, 0 ) == &shaders[SHADER_COUNT], "skinNum -1 wraps to the last shader" );
	Check( Draw( -SHADER_COUNT, 0, 0, 0, 0, 0 ) == &shaders[1], "skinNum -numShaders wraps to the first shader" );
	Check( Draw( INT_MIN, 0, 0, 0, 0, 0 ) == &shaders[1 + ( SHADER_COUNT + INT_MIN % SHADER_COUNT ) % SHADER_COUNT],
		"INT_MIN skinNum stays in range" );

	/* The other refEntity indexes were already bounded; keep them that way */
	Check( Draw( -1, 1, 0, 0, 0, 0 ) == &skinShader, "a valid customSkin overrides skinNum" );
	Check( Draw( -1, -1, 0, 0, 0, 0 ) == &shaders[SHADER_COUNT], "negative customSkin is ignored" );
	Check( Draw( -1, tr.numSkins, 0, 0, 0, 0 ) == &shaders[SHADER_COUNT], "customSkin past tr.numSkins is ignored" );
	Check( Draw( -1, 1, 1, 0, 0, 0 ) == &customShader, "customShader wins" );
	Check( Draw( 0, 0, -7, 0, 0, 0 ) == &defaultShader, "negative customShader selects the default shader" );
	Check( Draw( 0, 0, 0, -1, FRAME_COUNT, 0 ) == &shaders[1], "out-of-range frames reset to zero" );
	Check( Draw( 0, 0, 0, INT_MIN, INT_MAX, 0 ) == &shaders[1], "extreme frames reset to zero" );
	Check( Draw( 0, 0, 0, -1, -FRAME_COUNT - 1, RF_WRAP_FRAMES ) == &shaders[1], "negative wrapped frames reset to zero" );
	Check( Draw( 0, 0, 0, FRAME_COUNT + 1, INT_MAX, RF_WRAP_FRAMES ) == &shaders[1], "large wrapped frames stay in range" );
	Check( Draw( 0, 0, 0, 0, 0, ~0 & ~RF_THIRD_PERSON ) == &shaders[1], "arbitrary renderfx bits" );

	free( buffer );
	printf( "MD3 entity bounds regression passed\n" );
	return 0;
}
