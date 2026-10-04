/* Issue #466: the real Upload32 scaling, resample and mip path with GL stubbed.
 * A power-of-two texture with an extreme aspect ratio used to halve a
 * dimension to 0 after the minimum clamp, so the mip loop never ended. */
#include "renderer_image_gl_stub.h"
#include "../code/renderer/tr_image.c"
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

trGlobals_t tr;
glconfig_t glConfig;
refimport_t ri;
static cvar_t roundDown, picmipLevel, textureBits, colorMips, simpleMips;
cvar_t *r_roundImagesDown = &roundDown, *r_picmip = &picmipLevel, *r_texturebits = &textureBits;
cvar_t *r_colorMipLevels = &colorMips, *r_simpleMipMaps = &simpleMips;

#define MAX_LEVELS 32
static int levels, levelWidth[MAX_LEVELS], levelHeight[MAX_LEVELS], levelNumber[MAX_LEVELS];
static unsigned int levelHash[MAX_LEVELS];
static int tempLive, expectError;
static jmp_buf errorJump;
static const char *currentCase = "";

static void Check( int ok, const char *message ) {
	if ( !ok ) { fprintf( stderr, "Upload32 regression failed (%s): %s\n", currentCase, message ); exit( 1 ); }
}
static void Timeout( int signal ) {
	static const char text[] = "Upload32 regression failed: timed out, the mip loop did not terminate (issue #466)\n";
	(void)signal; if ( write( 2, text, sizeof( text ) - 1 ) ) {} _exit( 1 );
}
void QDECL Com_Error( int level, const char *format, ... ) { (void)level; (void)format; Check( 0, "unexpected Com_Error" ); }
void QDECL Com_Printf( const char *format, ... ) { (void)format; }
#ifndef Com_Memcpy
void Com_Memcpy( void *out, const void *in, size_t n ) { memcpy( out, in, n ); }
#endif
#ifndef Com_Memset
void Com_Memset( void *out, int value, size_t n ) { memset( out, value, n ); }
#endif
static void QDECL Error( int level, const char *format, ... ) {
	(void)format; Check( expectError && level == ERR_DROP && !tempLive, "unexpected ri.Error" ); longjmp( errorJump, 1 );
}
/* Exact-size heap blocks so ASan sees any access past a scaled buffer.
 * R_MipMap2 legitimately asks for 0 bytes when a 1-pixel side halves. */
static void *TempAlloc( int size ) {
	void *p; Check( size >= 0, "negative temp allocation size" );
	p = malloc( size ? size : 1 ); Check( p != NULL, "temp allocation" ); memset( p, 0xa5, size ); tempLive++; return p;
}
static void TempFree( void *p ) { Check( p != NULL && tempLive > 0, "temp free" ); free( p ); tempLive--; }
void GL_CheckErrors( void ) {}
static void TexParameterf( GLenum target, GLenum name, GLfloat value ) { (void)target; (void)name; (void)value; }
/* Record every level and read all of its bytes, as a GL driver would. */
static void TexImage2D( GLenum target, GLint level, GLint internal, GLsizei width, GLsizei height,
	GLint border, GLenum format, GLenum type, const void *pixels ) {
	const byte *p = pixels; unsigned int hash = 2166136261u; size_t i;
	(void)target; (void)internal; (void)border; (void)format; (void)type;
	Check( levels < MAX_LEVELS, "too many uploaded levels" );
	Check( width >= 1 && height >= 1 && width <= glConfig.maxTextureSize && height <= glConfig.maxTextureSize, "uploaded level size" );
	for ( i = 0; i < (size_t)width * height * 4; i++ ) { hash ^= p[i]; hash *= 16777619u; }
	levelNumber[levels] = level; levelWidth[levels] = width; levelHeight[levels] = height; levelHash[levels++] = hash;
}
void (*qglTexImage2D)( GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void * ) = TexImage2D;
void (*qglTexParameterf)( GLenum, GLenum, GLfloat ) = TexParameterf;

static int Pow2( int size, int round ) {
	int scaled; for ( scaled = 1; scaled < size; scaled <<= 1 ) {}
	return ( round && scaled > size ) ? scaled >> 1 : scaled;
}
/* Fixed order: picmip, halve to the limit, then clamp to 1.  When stock is
 * non-NULL, also report the stock (clamp-first) size, 0 meaning it collapsed. */
static void Model( int width, int height, int round, int picmip, int limit, int *w, int *h, int *stockW, int *stockH ) {
	int sw = Pow2( width, round ) >> picmip, sh = Pow2( height, round ) >> picmip, tw, th;
	tw = sw < 1 ? 1 : sw; th = sh < 1 ? 1 : sh;
	while ( tw > limit || th > limit ) { tw >>= 1; th >>= 1; }
	*stockW = tw; *stockH = th;
	while ( sw > limit || sh > limit ) { sw >>= 1; sh >>= 1; }
	*w = sw < 1 ? 1 : sw; *h = sh < 1 ? 1 : sh;
}

static unsigned int matrixHash = 2166136261u;
static int foldHash;
/* Upload one image through the real Upload32 and check the level chain. */
static void Upload( int width, int height, int round, int picmip, int limit, int mipmap, int simple, int uniform ) {
	static char name[160];
	unsigned *pixels; byte *p; int i, w, h, sw, sh, format = -1, upW = -1, upH = -1;
	snprintf( name, sizeof( name ), "%dx%d round=%d picmip=%d limit=%d mipmap=%d simple=%d", width, height, round, picmip, limit, mipmap, simple );
	currentCase = name;
	roundDown.integer = round; picmipLevel.integer = picmip; simpleMips.integer = simple;
	glConfig.maxTextureSize = limit;
	pixels = malloc( (size_t)width * height * 4 ); Check( pixels != NULL, "image allocation" ); p = (byte *)pixels;
	for ( i = 0; i < width * height * 4; i++ ) p[i] = uniform ? ( i & 3 ) * 60 + 20 : (byte)( i * 7 + ( i >> 9 ) * 13 );
	levels = 0;
	alarm( 20 );
	Upload32( pixels, width, height, mipmap, picmip != 0, qfalse, &format, &upW, &upH );
	alarm( 0 );
	free( pixels );
	Check( tempLive == 0, "temp memory released" );
	Model( width, height, round, picmip, limit, &w, &h, &sw, &sh );
	Check( upW == w && upH == h, "upload size matches clamp-after-limit model" );
	Check( levels >= 1 && levelNumber[0] == 0 && levelWidth[0] == w && levelHeight[0] == h, "level 0 size" );
	for ( i = 1; i < levels; i++ ) {
		w = w > 1 ? w >> 1 : 1; h = h > 1 ? h >> 1 : 1;
		Check( levelNumber[i] == i && levelWidth[i] == w && levelHeight[i] == h, "mip chain halves to 1x1" );
	}
	Check( mipmap ? ( w == 1 && h == 1 ) : levels == 1, "mip chain length" );
	if ( uniform ) {
		for ( i = 0; i < levels; i++ ) {
			unsigned int hash = 2166136261u; int n;
			for ( n = 0; n < levelWidth[i] * levelHeight[i]; n++ ) { int k; for ( k = 0; k < 4; k++ ) { hash ^= (byte)( k * 60 + 20 ); hash *= 16777619u; } }
			Check( levelHash[i] == hash, "uniform colour survives every level" );
		}
	}
	if ( foldHash ) {
		/* Stock never reached 0 here: fold into the byte-identity hash. */
		for ( i = 0; i < levels; i++ ) {
			matrixHash ^= levelHash[i] ^ ( levelWidth[i] << 16 ) ^ levelHeight[i]; matrixHash *= 16777619u;
		}
	}
}

int main( void ) {
	static const int sizes[] = { 1, 3, 64, 100, 257, 640, 1024 };
	static const int extremes[][2] = { { 4096, 1 }, { 1, 4096 }, { 2048, 1 }, { 1, 2048 }, { 4096, 2 }, { 2, 4096 }, { 2048, 3 }, { 8192, 1 } };
	int limit, picmip, mipmap, simple, round, a, b, i, w, h, sw, sh, collapsed = 0;
	unsigned *huge = NULL;
	signal( SIGALRM, Timeout );
	for ( i = 0; i < 256; i++ ) { s_gammatable[i] = i; s_intensitytable[i] = i; }
	glConfig.deviceSupportsGamma = qtrue;
	ri.Hunk_AllocateTempMemory = TempAlloc; ri.Hunk_FreeTempMemory = TempFree; ri.Error = Error;

	/* The issue's shapes, each a stock 0-size collapse, at limits 1024 and 2048. */
	for ( limit = 1024; limit <= 2048; limit <<= 1 )
		for ( picmip = 0; picmip <= 1; picmip++ )
			for ( i = 0; i < (int)( sizeof( extremes ) / sizeof( extremes[0] ) ); i++ )
				for ( mipmap = 0; mipmap <= 1; mipmap++ )
					for ( simple = 0; simple <= 1; simple++ ) {
						Model( extremes[i][0], extremes[i][1], 1, picmip, limit, &w, &h, &sw, &sh );
						collapsed += !sw || !sh;
						Upload( extremes[i][0], extremes[i][1], 1, picmip, limit, mipmap, simple, 1 );
					}
	Check( collapsed > 0, "matrix includes stock 0-size cases" );

	/* Ordinary sizes: real path matches the model, and bytes match stock. */
	foldHash = 1;
	for ( a = 0; a < (int)( sizeof( sizes ) / sizeof( sizes[0] ) ); a++ )
		for ( b = 0; b < (int)( sizeof( sizes ) / sizeof( sizes[0] ) ); b++ )
			for ( limit = 256; limit <= 2048; limit <<= 2 )
				for ( picmip = 0; picmip <= 2; picmip++ )
					for ( round = 0; round <= 1; round++ )
						for ( mipmap = 0; mipmap <= 1; mipmap++ ) {
							Model( sizes[a], sizes[b], round, picmip, limit, &w, &h, &sw, &sh );
							if ( sw && sh ) Upload( sizes[a], sizes[b], round, picmip, limit, mipmap, mipmap, 0 );
						}
	foldHash = 0;
	printf( "byte-identity hash of non-collapsing uploads: %08x\n", matrixHash );
	Check( matrixHash == 0x97d86b2du, "non-collapsing uploads are byte-identical to stock" );

	/* Every size where stock never reached 0 gets the stock size. */
	for ( a = 1; a <= 8192; a = a < 8 ? a + 1 : a * 2 - 3 )
		for ( b = 1; b <= 8192; b = b < 8 ? b + 1 : b * 2 - 3 )
			for ( limit = 64; limit <= 4096; limit <<= 1 )
				for ( picmip = 0; picmip <= 16; picmip++ )
					for ( round = 0; round <= 1; round++ ) {
						Model( a, b, round, picmip, limit, &w, &h, &sw, &sh );
						Check( ( !sw || !sh ) || ( w == sw && h == sh ), "model keeps every stock non-zero size" );
						Check( w >= 1 && h >= 1 && w <= limit && h <= limit, "model sizes in range" );
					}

	/* A rounded-up size whose byte count overflows int is rejected before allocation. */
	currentCase = "1x(2^28+1) overflow guard"; roundDown.integer = 0; picmipLevel.integer = 0; glConfig.maxTextureSize = 2048;
	expectError = 1; levels = 0;
	if ( !setjmp( errorJump ) ) {
		int format, upW, upH; huge = (unsigned *)&huge;
		Upload32( huge, 1, ( 1 << 28 ) + 1, qtrue, qfalse, qfalse, &format, &upW, &upH ); Check( 0, "overflowing size accepted" );
	}
	expectError = 0; Check( levels == 0 && tempLive == 0, "overflow rejected before allocation or upload" );

	puts( "Upload32 extreme aspect ratios (4096x1, 1x4096, 2048x1, 1x2048 at limits 1024/2048, picmip 0/1), ordinary sizes and size guard passed (issue #466)" );
	return 0;
}
