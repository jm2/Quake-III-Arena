/* Issue #46 retail parity: the hardened shader parser must accept every
   construct retail 1.32c accepted with a warning, and build the same shader.
   This file is compiled three times by tests/run_shader_retail_parity_tests.sh:
   Q3_PARITY_SIDE=1 wraps the real code/renderer/tr_shader.c, Q3_PARITY_SIDE=2
   wraps the verbatim retail parser in shader_retail_parser_reference.h, and the
   plain build holds the shared stubs and compares the two sides field by field,
   including the warnings printed and the images/cinematics requested. */
#define __QGL_H__
typedef unsigned int GLuint;
#define GL_CLAMP 0x2900
#define GL_REPEAT 0x2901
#define GL_MODULATE 0x2100
#define GL_DECAL 0x2101
#define GL_ADD 0x0104
#if Q3_PARITY_SIDE == 1
static void (*qglActiveTextureARB)(unsigned int);
#include "../code/renderer/tr_shader.c"
#define PARITY_ENTRY MasterParityParse
#elif Q3_PARITY_SIDE == 2
#include "../code/renderer/tr_local.h"
#include "shader_retail_parser_reference.h"
#define PARITY_ENTRY RetailParityParse
#else
#include "../code/renderer/tr_local.h"
#endif
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	qboolean accepted;
	long consumed; /* -1 when the parser gave up on the text */
	shader_t shader;
	shaderStage_t stages[MAX_SHADER_STAGES];
	texModInfo_t texMods[MAX_SHADER_STAGES][TR_MAX_TEXMODS];
	vec3_t sunLight, sunDirection;
} parityResult_t;

void MasterParityParse( const char *name, int lightmapIndex, const char *source, parityResult_t *out );
void RetailParityParse( const char *name, int lightmapIndex, const char *source, parityResult_t *out );

#ifdef PARITY_ENTRY
/* The R_FindShader preamble shared by both versions. Retail never clears the
   static texture modifier slots, so a modifier it leaves incomplete keeps an
   earlier shader's fields; cleared slots are what retail has for a fresh slot. */
void PARITY_ENTRY( const char *name, int lightmapIndex, const char *source, parityResult_t *out ) {
	char *text = (char *)source;
	int i;
	Com_Memset( &shader, 0, sizeof(shader) );
	Com_Memset( stages, 0, sizeof(stages) );
	Com_Memset( texMods, 0, sizeof(texMods) );
	Q_strncpyz( shader.name, name, sizeof(shader.name) );
	shader.lightmapIndex = lightmapIndex;
	for ( i = 0; i < MAX_SHADER_STAGES; i++ ) stages[i].bundle[0].texMods = texMods[i];
	shader.needsNormal = shader.needsST1 = shader.needsST2 = shader.needsColor = qtrue;
	VectorClear( tr.sunLight );
	VectorClear( tr.sunDirection );
	out->accepted = ParseShader( &text );
	out->consumed = text ? (long)(text - source) : -1;
	out->shader = shader;
	memcpy( out->stages, stages, sizeof(stages) );
	memcpy( out->texMods, texMods, sizeof(texMods) );
	for ( i = 0; i < MAX_SHADER_STAGES; i++ ) out->stages[i].bundle[0].texMods = NULL;
	VectorCopy( tr.sunLight, out->sunLight );
	VectorCopy( tr.sunDirection, out->sunDirection );
}
#else

trGlobals_t tr;
glconfig_t glConfig;
refimport_t ri;
static cvar_t zero, one = { .integer = 1 };
cvar_t *r_ignoreFastPath = &one, *r_smp = &zero, *r_printShaders = &zero, *r_detailTextures = &one, *r_vertexLight = &zero, *r_uiFullScreen = &zero;
static backEndData_t commands;
backEndData_t *backEndData[SMP_FRAMES] = { &commands, &commands };

#define MAX_PARITY_IMAGES 8192
static image_t images[MAX_PARITY_IMAGES];
static int numImages;
static char logs[2][32768];
static int logSide, skyCalls[2];
static float skyHeights[2];
static int failures, compared;

static void Fail( const char *message ) { fprintf( stderr, "Shader retail parity failed: %s\n", message ); exit( 1 ); }
static void Log( const char *format, ... ) {
	char *log = logs[logSide];
	size_t used = strlen( log );
	va_list args;
	va_start( args, format );
	vsnprintf( log + used, sizeof(logs[0]) - used, format, args );
	va_end( args );
	if ( strlen( log ) >= sizeof(logs[0]) - 1 ) Fail( "event log overflow" );
}
static void QDECL Print( int level, const char *format, ... ) {
	char line[4096];
	va_list args;
	va_start( args, format );
	vsnprintf( line, sizeof(line), format, args );
	va_end( args );
	Log( "print %d: %s", level, line );
}
void QDECL Com_Error( int level, const char *format, ... ) { (void)level; (void)format; Fail( "unexpected error/drop" ); }
/* Com_sprintf reports truncated sky paths through Com_Printf. */
void QDECL Com_Printf( const char *format, ... ) {
	char line[4096];
	va_list args;
	va_start( args, format );
	vsnprintf( line, sizeof(line), format, args );
	va_end( args );
	Log( "console: %s", line );
}
static int Video( const char *name, int x, int y, int width, int height, int flags ) {
	Log( "cinematic %s %d %d %d %d %d\n", name, x, y, width, height, flags );
	return 0;
}
static image_t *Image( const char *name ) {
	int i;
	for ( i = 0; i < numImages; i++ ) if ( !strcmp( images[i].imgName, name ) ) return &images[i];
	if ( numImages == MAX_PARITY_IMAGES ) Fail( "image table full" );
	Q_strncpyz( images[numImages].imgName, name, sizeof(images[0].imgName) );
	return &images[numImages++];
}
/* Names starting with "missing" are not found, so both default-image paths run. */
image_t *R_FindImageFile( const char *name, qboolean mipmap, qboolean picmip, int wrap ) {
	Log( "image %s %d %d %#x\n", name, mipmap, picmip, wrap );
	return strncmp( name, "missing", 7 ) ? Image( name ) : NULL;
}
/* Retail builds the cloud table while parsing and master once the shader is
   accepted, so only the final layer of an accepted shader is compared. */
void R_InitSkyTexCoords( float height ) { skyCalls[logSide]++; skyHeights[logSide] = height; }
void R_SyncRenderThread( void ) { Fail( "unexpected threaded import" ); }
void RB_StageIteratorGeneric( void ) {}
void RB_StageIteratorSky( void ) {}
void RB_StageIteratorVertexLitTexture( void ) {}
void RB_StageIteratorLightmappedMultitexture( void ) {}
void R_DecomposeSort( unsigned int sort, int *entity, shader_t **material, int *fog, int *light ) { (void)sort; (void)entity; (void)material; (void)fog; (void)light; Fail( "unexpected live render command" ); }
#ifndef Com_Memset
void Com_Memset( void *destination, int value, size_t size ) { memset( destination, value, size ); }
#endif
#ifndef Com_Memcpy
void Com_Memcpy( void *destination, const void *source, size_t size ) { memcpy( destination, source, size ); }
#endif

static parityResult_t master, retail;

static int Field( const char *label, const char *field, const void *a, const void *b, size_t size ) {
	if ( !memcmp( a, b, size ) ) return 0;
	fprintf( stderr, "%s: field %s differs (master vs retail)\n", label, field );
	return 1;
}
#define SF( f ) bad += Field( label, "shader." #f, &m->f, &r->f, sizeof(m->f) )
#define WF( prefix, w ) bad += Field( label, prefix "." #w ".func", &w.func, &rw.func, sizeof(w.func) ) + Field( label, prefix "." #w ".base", &w.base, &rw.base, sizeof(w.base) ) + Field( label, prefix "." #w ".amplitude", &w.amplitude, &rw.amplitude, sizeof(w.amplitude) ) + Field( label, prefix "." #w ".phase", &w.phase, &rw.phase, sizeof(w.phase) ) + Field( label, prefix "." #w ".frequency", &w.frequency, &rw.frequency, sizeof(w.frequency) )

static int CompareShader( const char *label, const shader_t *m, const shader_t *r ) {
	int bad = 0, i;
	char name[96];
	SF( name ); SF( lightmapIndex ); SF( index ); SF( sortedIndex ); SF( sort ); SF( defaultShader );
	SF( explicitlyDefined ); SF( surfaceFlags ); SF( contentFlags ); SF( entityMergable ); SF( isSky );
	SF( sky.cloudHeight ); SF( sky.outerbox ); SF( sky.innerbox ); SF( fogParms.color ); SF( fogParms.depthForOpaque );
	SF( portalRange ); SF( multitextureEnv ); SF( cullType ); SF( polygonOffset ); SF( noMipMaps ); SF( noPicMip );
	SF( fogPass ); SF( needsNormal ); SF( needsST1 ); SF( needsST2 ); SF( needsColor ); SF( numDeforms );
	for ( i = 0; i < MAX_SHADER_DEFORMS; i++ ) {
		const deformStage_t *a = &m->deforms[i], *b = &r->deforms[i];
		const waveForm_t deformationWave = a->deformationWave, rw = b->deformationWave;
		snprintf( name, sizeof(name), "shader.deforms[%d]", i );
		bad += Field( label, name, &a->deformation, &b->deformation, sizeof(a->deformation) );
		bad += Field( label, name, a->moveVector, b->moveVector, sizeof(a->moveVector) );
		WF( "deform", deformationWave );
		bad += Field( label, name, &a->deformationSpread, &b->deformationSpread, sizeof(a->deformationSpread) );
		bad += Field( label, name, &a->bulgeWidth, &b->bulgeWidth, sizeof(a->bulgeWidth) );
		bad += Field( label, name, &a->bulgeHeight, &b->bulgeHeight, sizeof(a->bulgeHeight) );
		bad += Field( label, name, &a->bulgeSpeed, &b->bulgeSpeed, sizeof(a->bulgeSpeed) );
	}
	SF( numUnfoggedPasses ); SF( stages ); SF( optimalStageIteratorFunc ); SF( clampTime ); SF( timeOffset );
	SF( numStates ); SF( currentShader ); SF( parentShader ); SF( currentState ); SF( expireTime );
	SF( remappedShader ); SF( shaderStates ); SF( next );
	/* Catch anything a future field adds to the structure. */
	bad += Field( label, "shader (whole structure)", m, r, sizeof(*m) );
	return bad;
}

static int CompareStage( const char *label, int s, const shaderStage_t *m, const shaderStage_t *r ) {
	int bad = 0, b;
	char name[96];
	const waveForm_t rgbWave = m->rgbWave, alphaWave = m->alphaWave;
	waveForm_t rw;
#define STF( f ) ( snprintf( name, sizeof(name), "stages[%d]." #f, s ), bad += Field( label, name, &m->f, &r->f, sizeof(m->f) ) )
	STF( active );
	for ( b = 0; b < NUM_TEXTURE_BUNDLES; b++ ) {
		STF( bundle[b].image ); STF( bundle[b].numImageAnimations ); STF( bundle[b].imageAnimationSpeed );
		STF( bundle[b].tcGen ); STF( bundle[b].tcGenVectors ); STF( bundle[b].numTexMods ); STF( bundle[b].texMods );
		STF( bundle[b].videoMapHandle ); STF( bundle[b].isLightmap ); STF( bundle[b].vertexLightmap ); STF( bundle[b].isVideoMap );
	}
	rw = r->rgbWave; WF( "stage", rgbWave );
	STF( rgbGen );
	rw = r->alphaWave; WF( "stage", alphaWave );
	STF( alphaGen ); STF( constantColor ); STF( stateBits ); STF( adjustColorsForFog ); STF( isDetail );
	snprintf( name, sizeof(name), "stages[%d] (whole structure)", s );
	bad += Field( label, name, m, r, sizeof(*m) );
	return bad;
}

static int CompareTexMods( const char *label, int s, const texModInfo_t *m, const texModInfo_t *r ) {
	int bad = 0, t;
	char name[96];
	for ( t = 0; t < TR_MAX_TEXMODS; t++ ) {
		const waveForm_t wave = m[t].wave, rw = r[t].wave;
#define TMF( f ) ( snprintf( name, sizeof(name), "texMods[%d][%d]." #f, s, t ), bad += Field( label, name, &m[t].f, &r[t].f, sizeof(m[t].f) ) )
		TMF( type ); WF( "texMod", wave ); TMF( matrix ); TMF( translate ); TMF( scale ); TMF( scroll ); TMF( rotateSpeed );
		snprintf( name, sizeof(name), "texMods[%d][%d] (whole structure)", s, t );
		bad += Field( label, name, &m[t], &r[t], sizeof(m[t]) );
	}
	return bad;
}

/* The one deliberate difference in accepted shaders (78fb5d8d, as in ioquake3):
   retail's skip test compares alphaGen with CGEN_IDENTITY, which is AGEN_ENTITY,
   so it skipped entity alpha instead of identity alpha. Re-derive the corrected
   choice from retail's result before comparing. */
static void CorrectRetailAlphaSkip( parityResult_t *result ) {
	int s;
	for ( s = 0; s < MAX_SHADER_STAGES; s++ ) {
		shaderStage_t *stage = &result->stages[s];
		if ( stage->alphaGen == AGEN_SKIP ) stage->alphaGen = AGEN_ENTITY;
		else if ( stage->alphaGen == AGEN_IDENTITY && ( stage->rgbGen == CGEN_IDENTITY || stage->rgbGen == CGEN_LIGHTING_DIFFUSE ) ) stage->alphaGen = AGEN_SKIP;
	}
}

/* Parse one definition with both parsers and compare everything retail produced. */
static int Compare( const char *label, const char *name, int lightmapIndex, const char *text ) {
	int bad = 0, s;
	logs[0][0] = logs[1][0] = 0;
	skyCalls[0] = skyCalls[1] = 0;
	logSide = 0; MasterParityParse( name, lightmapIndex, text, &master );
	logSide = 1; RetailParityParse( name, lightmapIndex, text, &retail );
	CorrectRetailAlphaSkip( &retail );
	compared++;
	if ( master.accepted != retail.accepted ) {
		fprintf( stderr, "%s: master %s, retail %s\n", label, master.accepted ? "accepts" : "rejects (default material)", retail.accepted ? "accepts" : "rejects" );
		bad++;
	}
	if ( strcmp( logs[0], logs[1] ) ) {
		fprintf( stderr, "%s: warnings/imports differ\n--- master\n%s--- retail\n%s---\n", label, logs[0], logs[1] );
		bad++;
	}
	if ( master.accepted && retail.accepted ) {
		if ( master.consumed != retail.consumed ) { fprintf( stderr, "%s: master stops at %ld, retail at %ld\n", label, master.consumed, retail.consumed ); bad++; }
		bad += CompareShader( label, &master.shader, &retail.shader );
		for ( s = 0; s < MAX_SHADER_STAGES; s++ ) {
			bad += CompareStage( label, s, &master.stages[s], &retail.stages[s] );
			bad += CompareTexMods( label, s, master.texMods[s], retail.texMods[s] );
		}
		bad += Field( label, "tr.sunLight", master.sunLight, retail.sunLight, sizeof(vec3_t) );
		bad += Field( label, "tr.sunDirection", master.sunDirection, retail.sunDirection, sizeof(vec3_t) );
		if ( !skyCalls[0] != !skyCalls[1] || ( skyCalls[0] && skyHeights[0] != skyHeights[1] ) ) {
			fprintf( stderr, "%s: cloud layer master %d/%g, retail %d/%g\n", label, skyCalls[0], skyHeights[0], skyCalls[1], skyHeights[1] );
			bad++;
		}
	}
	if ( bad ) failures++;
	return bad;
}

static char body[16384];

/* Wrap stage lines in one shader with a single stage. */
static void StageCase( const char *label, const char *lines, int mustAccept ) {
	int bad;
	snprintf( body, sizeof(body), "{\n{\nmap textures/parity/base.tga\n%s\nblendFunc add\n}\n}\nfollowing\n", lines );
	bad = Compare( label, "parity/stage", LIGHTMAP_NONE, body );
	if ( mustAccept && !master.accepted ) { fprintf( stderr, "%s: master falls back to the default material\n%s", label, logs[0] ); if ( !bad ) failures++; }
}
static void ShaderCase( const char *label, const char *lines, int mustAccept ) {
	int bad;
	snprintf( body, sizeof(body), "{\n%s\n{\nmap textures/parity/base.tga\n}\n}\nfollowing\n", lines );
	bad = Compare( label, "parity/shader", LIGHTMAP_NONE, body );
	if ( mustAccept && !master.accepted ) { fprintf( stderr, "%s: master falls back to the default material\n%s", label, logs[0] ); if ( !bad ) failures++; }
}

/* Every argument prefix of a complete form, from none to all but the last. */
static void Prefixes( const char *keyword, const char *complete, int stage ) {
	char words[16][64], line[512], label[640];
	char copy[512], *p = copy;
	int count = 0, n, i;
	Q_strncpyz( copy, complete, sizeof(copy) );
	while ( 1 ) {
		char *word = COM_ParseExt( &p, qfalse );
		if ( !word[0] ) break;
		if ( count == 16 ) Fail( "prefix fixture too long" );
		Q_strncpyz( words[count++], word, sizeof(words[0]) );
	}
	for ( n = 0; n <= count; n++ ) {
		snprintf( line, sizeof(line), "%s", keyword );
		for ( i = 0; i < n; i++ ) { strcat( line, " " ); strcat( line, words[i] ); }
		snprintf( label, sizeof(label), "'%s'", line );
		if ( stage ) StageCase( label, line, 1 );
		else ShaderCase( label, line, 1 );
	}
}

/* Item groups added after the first pass: other missing values, malformed
   vectors, long sky names and float-only overflow fields. */
static void MoreConstructs( void ) {
	static const char *overflow[] = { "1e39", "-1e39", "1e400", "3.4028235e38", "-3.40282356e38", "3.4028236e38" };
	/* A read past the end of a line takes the next line's token (COM_ParseExt
	   skips the newline), so some malformed forms make both parsers reject. */
	static const char *vectors[] = { "", "bad", "( 0.1 0.2 )", "( 0.1 0.2 0.3", "( 0.25", "(", "( 0.1 0.2 0.3 ) extra", "( 0.5 0.25 0.75 )" };
	static const int vectorsAccepted[] = { 1, 1, 1, 1, 1, 1, 0, 1 };
	static const char *tcVectors[] = { "", "bad bad", "( 1 0 0 )", "( 1 0 0 ) bad", "( 1 0", "( 0.5 0.25 )", "( 1 0 0 ) ( 0 1", "( 1 0 0 ) ( 0 1 0 )" };
	static const int tcVectorsAccepted[] = { 0, 1, 0, 1, 0, 0, 1, 1 };
	static const int lengths[] = { 50, 56, 57, 58, 62, 63, 64, 100, 1000 };
	char line[2048], label[2560], name[1024];
	int i, j;

	ShaderCase( "'clampTime'", "clampTime", 1 );
	ShaderCase( "'clampTime 12.5'", "clampTime 12.5", 1 );
	ShaderCase( "'fogParms ( 0.2 0.3 0.4 )'", "fogParms ( 0.2 0.3 0.4 )", 1 );
	ShaderCase( "'fogParms' without depth then a depth", "fogParms ( 0.2 0.3 0.4 ) 128\nfogParms ( 0.5 0.6 0.7 )", 1 );
	ShaderCase( "'fogParms ( 0.2 0.3 )' (retail rejects)", "fogParms ( 0.2 0.3 )", 0 );
	ShaderCase( "'fogParms' (retail rejects)", "fogParms", 0 );
	snprintf( body, sizeof(body), "{\nsurfaceparm fog\nfogParms ( 0.1 0.2 0.3 )\n}\nfollowing\n" );
	Compare( "stage-less fog without depth", "parity/fog", LIGHTMAP_NONE, body );
	for ( i = 0; i <= 6; i++ ) {
		static const char *sun[] = { "1", "-2", "3", "100", "45", "60" };
		snprintf( line, sizeof(line), "q3map_sun" );
		for ( j = 0; j < i; j++ ) { strcat( line, " " ); strcat( line, sun[j] ); }
		snprintf( label, sizeof(label), "'%s'", line );
		ShaderCase( label, line, i >= 5 );
	}
	StageCase( "'alphaGen const'", "alphaGen const", 1 );
	StageCase( "'alphaGen const 0.5'", "alphaGen const 0.5", 1 );
	for ( i = 0; i < (int)(sizeof(vectors) / sizeof(vectors[0])); i++ ) {
		snprintf( line, sizeof(line), "rgbGen const %s", vectors[i] );
		snprintf( label, sizeof(label), "'%s'", line );
		StageCase( label, line, vectorsAccepted[i] );
	}
	for ( i = 0; i < (int)(sizeof(tcVectors) / sizeof(tcVectors[0])); i++ ) {
		snprintf( line, sizeof(line), "tcGen vector %s", tcVectors[i] );
		snprintf( label, sizeof(label), "'%s'", line );
		StageCase( label, line, tcVectorsAccepted[i] );
	}
	/* long sky box names: Com_sprintf truncates each face path to MAX_QPATH */
	for ( i = 0; i < (int)(sizeof(lengths) / sizeof(lengths[0])); i++ ) {
		memset( name, 'x', lengths[i] ); name[lengths[i]] = 0;
		for ( j = 0; j < 4; j++ ) {
			snprintf( line, sizeof(line), j == 0 ? "skyparms %s 512 -" : j == 1 ? "skyparms - 512 %s" : j == 2 ? "skyparms %s 512 %s" : "skyparms %s", name, name );
			snprintf( label, sizeof(label), "skyparms with a %d-byte name (form %d)", lengths[i], j );
			ShaderCase( label, line, 1 );
		}
	}
	/* overflow in float-only fields keeps retail's float */
	for ( i = 0; i < (int)(sizeof(overflow) / sizeof(overflow[0])); i++ ) {
		const char *forms[] = { "tcMod scale %s 1", "tcMod scale 1 %s", "tcMod transform %s 0 0 1 0 0", "tcMod transform 1 %s 0 1 0 0", "tcMod transform 1 0 %s 1 0 0", "tcMod transform 1 0 0 %s 0 0", "tcMod transform 1 0 0 1 %s 0", "tcMod transform 1 0 0 1 0 %s", "tcMod turb %s 0.1 0 1", "tcMod turb 0 %s 0 1", "tcMod stretch sin %s 0.1 0 1", "tcMod stretch sin 1 %s 0 1", "tcGen vector ( %s 0 0 ) ( 0 1 0 )", "tcGen vector ( 1 0 0 ) ( 0 0 %s )" };
		for ( j = 0; j < (int)(sizeof(forms) / sizeof(forms[0])); j++ ) {
			snprintf( line, sizeof(line), forms[j], overflow[i] );
			snprintf( label, sizeof(label), "'%s'", line );
			StageCase( label, line, 1 );
		}
		snprintf( line, sizeof(line), "sort %s", overflow[i] );
		snprintf( label, sizeof(label), "'%s'", line );
		ShaderCase( label, line, 1 );
	}
}

static void Constructs( void ) {
	static const char *tcMods[] = { "turb 0.2 0.3 0.4 0.5", "scale 0.5 -0.25", "scroll 0.5 -0.25", "stretch sin 0.2 0.3 0.4 0.5", "transform 1 2 3 4 -0.5 0.25", "rotate -120.5", "entityTranslate" };
	static const char *deforms[] = { "wave 100 sin 0.2 0.3 0.4 0.5", "bulge 1.5 -2.5 3.5", "move 1 -2 3 sin 0.2 0.3 0.4 0.5", "normal 0.5 0.25", "projectionShadow", "autosprite", "autosprite2", "text3", "text9" };
	static const char *scrolls[] = { "1e39", "-1e39", "1e400", "-1e400", "3.4028235e38", "-3.4028235e38", "3.40282356e38", "3.4028236e38", "1e-39", "3.4028234e38" };
	static const char *skies[] = { "skyparms", "skyparms -", "skyparms env/parity", "skyparms missing/parity", "skyparms - 0", "skyparms env/parity 1024", "skyparms env/parity 0", "skyparms - 256 -", "skyparms env/parity 512 env/inner", "skyparms - 512 missing/inner" };
	char line[512], label[640];
	int i, n;

	/* tcMod: every missing-argument prefix, unknown and empty modifiers */
	for ( i = 0; i < (int)(sizeof(tcMods) / sizeof(tcMods[0])); i++ ) {
		char keyword[64];
		const char *space = strchr( tcMods[i], ' ' );
		snprintf( keyword, sizeof(keyword), "tcMod %.*s", space ? (int)(space - tcMods[i]) : (int)strlen( tcMods[i] ), tcMods[i] );
		Prefixes( keyword, space ? space + 1 : "", 1 );
	}
	StageCase( "'tcMod'", "tcMod", 1 );
	StageCase( "'tcMod bogus'", "tcMod bogus", 1 );
	StageCase( "'tcMod bogus 1 2'", "tcMod bogus 1 2", 1 );
	StageCase( "incomplete then complete modifiers", "tcMod scale 0.5\ntcMod scroll 1 2\ntcMod rotate", 1 );
	StageCase( "four modifiers with incomplete ones", "tcMod turb 1\ntcMod bogus\ntcMod scroll 1 2\ntcMod stretch", 1 );
	/* the 1e39 scroll and the float rounding edges around FLT_MAX */
	for ( i = 0; i < (int)(sizeof(scrolls) / sizeof(scrolls[0])); i++ ) {
		snprintf( line, sizeof(line), "tcMod scroll %s 0.25", scrolls[i] );
		snprintf( label, sizeof(label), "'%s'", line );
		StageCase( label, line, 1 );
		snprintf( line, sizeof(line), "tcMod scroll 0.5 %s", scrolls[i] );
		snprintf( label, sizeof(label), "'%s'", line );
		StageCase( label, line, 1 );
	}
	/* rgbGen/alphaGen wave: every missing-argument prefix */
	Prefixes( "rgbGen wave", "sin 0.2 0.3 0.4 0.5", 1 );
	Prefixes( "alphaGen wave", "square 0.2 0.3 0.4 0.5", 1 );
	StageCase( "'rgbGen wave bogus 1'", "rgbGen wave bogus 1", 1 );
	StageCase( "partial wave keeps the next line", "rgbGen wave sin 0.2\nalphaGen wave triangle", 1 );
	/* deformVertexes: every missing-argument prefix, unknown subtypes, the cap */
	for ( i = 0; i < (int)(sizeof(deforms) / sizeof(deforms[0])); i++ ) {
		char keyword[64];
		const char *space = strchr( deforms[i], ' ' );
		snprintf( keyword, sizeof(keyword), "deformVertexes %.*s", space ? (int)(space - deforms[i]) : (int)strlen( deforms[i] ), deforms[i] );
		Prefixes( keyword, space ? space + 1 : "", 0 );
	}
	ShaderCase( "'deformVertexes'", "deformVertexes", 1 );
	ShaderCase( "'deformVertexes bogus'", "deformVertexes bogus", 1 );
	ShaderCase( "'deformVertexes bogus 1 2' (retail rejects the stray tokens)", "deformVertexes bogus 1 2", 0 );
	ShaderCase( "'deformVertexes wave 0 sin'", "deformVertexes wave 0 sin", 1 );
	for ( n = 1; n <= MAX_SHADER_DEFORMS + 3; n++ ) {
		static const char *single[] = { "autosprite", "projectionShadow", "text2", "autosprite2", "bogus", "autosprite", "text1" };
		line[0] = 0;
		for ( i = 0; i < n; i++ ) { strcat( line, "deformVertexes " ); strcat( line, single[i] ); strcat( line, "\n" ); }
		snprintf( label, sizeof(label), "%d single-token deforms", n );
		ShaderCase( label, line, 1 );
	}
	ShaderCase( "fourth deform with arguments (retail rejects the stray tokens)", "deformVertexes autosprite\ndeformVertexes autosprite2\ndeformVertexes text0\ndeformVertexes bulge 1 2 3", 0 );
	ShaderCase( "fourth deform with missing arguments", "deformVertexes autosprite\ndeformVertexes bulge 1\ndeformVertexes move 1 2\ndeformVertexes wave", 1 );
	/* sort and skyParms with missing values */
	ShaderCase( "'sort'", "sort", 1 );
	ShaderCase( "'sort' after a sort value", "sort additive\nsort", 1 );
	for ( i = 0; i < (int)(sizeof(skies) / sizeof(skies[0])); i++ ) {
		snprintf( label, sizeof(label), "'%s'", skies[i] );
		ShaderCase( label, skies[i], 1 );
		snprintf( label, sizeof(label), "'%s' without stages", skies[i] );
		snprintf( body, sizeof(body), "{\n%s\n}\nfollowing\n", skies[i] );
		Compare( label, "parity/sky", LIGHTMAP_NONE, body );
		snprintf( label, sizeof(label), "complete sky then '%s'", skies[i] );
		snprintf( line, sizeof(line), "skyparms env/first 2048 -\n%s", skies[i] );
		ShaderCase( label, line, 1 );
		snprintf( label, sizeof(label), "'%s' then complete sky", skies[i] );
		snprintf( line, sizeof(line), "%s\nskyparms - 128 env/last", skies[i] );
		ShaderCase( label, line, 1 );
	}
	/* a shader using several tolerated constructs together keeps its stages */
	snprintf( body, sizeof(body), "{\nsort\ndeformVertexes normal 0.5\nskyparms env/mixed\n{\nmap $lightmap\nrgbGen wave\ntcMod rotate\n}\n{\nmap textures/parity/detail.tga\nalphaGen wave sin 1\ntcMod scroll 1e39 1\ntcMod bogus\nblendFunc filter\n}\n}\n" );
	if ( !Compare( "combined tolerated constructs (lightmap 0)", "parity/combined", 0, body ) && !master.accepted ) {
		fprintf( stderr, "combined tolerated constructs: master falls back to the default material\n" );
		failures++;
	}
}

/* Every definition in the given .shader files, in both lighting modes. */
static int Corpus( int count, char **paths ) {
	int names = 0, i, mode;
	for ( i = 0; i < count; i++ ) {
		FILE *file = fopen( paths[i], "rb" );
		long size;
		char *data, *p;
		if ( !file ) Fail( "cannot open corpus file" );
		fseek( file, 0, SEEK_END ); size = ftell( file ); fseek( file, 0, SEEK_SET );
		data = malloc( size + 1 );
		if ( !data || fread( data, 1, size, file ) != (size_t)size ) Fail( "cannot read corpus file" );
		data[size] = 0; fclose( file );
		p = data;
		while ( 1 ) {
			char name[MAX_QPATH], label[MAX_QPATH + 512];
			char *token = COM_ParseExt( &p, qtrue );
			if ( !token[0] ) break;
			Q_strncpyz( name, token, sizeof(name) );
			for ( mode = 0; mode < 2; mode++ ) {
				snprintf( label, sizeof(label), "%s: %s (lightmap %d)", paths[i], name, mode ? 0 : LIGHTMAP_NONE );
				Compare( label, name, mode ? 0 : LIGHTMAP_NONE, p );
			}
			names++;
			SkipBracedSection( &p );
		}
		free( data );
	}
	return names;
}

int main( int argc, char **argv ) {
	int names, before;
	ri.Printf = Print; ri.Error = Com_Error; ri.CIN_PlayCinematic = Video;
	tr.defaultImage = Image( "*default" ); tr.whiteImage = Image( "*white" ); tr.scratchImage[0] = Image( "*scratch" );
	tr.numLightmaps = 1; tr.lightmaps[0] = Image( "*lightmap0" );
	Constructs();
	MoreConstructs();
	before = compared;
	names = Corpus( argc - 1, argv + 1 );
	if ( failures ) { fprintf( stderr, "Shader retail parity failed: %d of %d definitions differ from retail 1.32c\n", failures, compared ); return 1; }
	printf( "Shader retail parity: %d tolerated-construct definitions", before );
	if ( names ) printf( " and %d corpus shader names (%d parses)", names, compared - before );
	printf( " match retail 1.32c field by field (issue #46)\n" );
	return 0;
}
#endif
