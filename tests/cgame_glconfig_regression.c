/* Issue #236: GetGlconfig must hand a QVM the exact retail 1.32c glconfig_t. */
#include "../code/client/cl_cgame.c"
#include "../code/qcommon/vm_local.h"
#include <setjmp.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define IMAGE_SIZE 16384
#define RETAIL_SIZE 11332
#define RETAIL(field, offset) { #field, offsetof(glconfig_t, field), offset }

/* Offsets id's lcc bytecode target assigns to the 1.32c tr_types.h (4-byte ints, enums, floats). */
static const struct { const char *name; size_t native; int retail; } layout[] = {
	RETAIL(renderer_string, 0), RETAIL(vendor_string, 1024), RETAIL(version_string, 2048),
	RETAIL(extensions_string, 3072), RETAIL(maxTextureSize, 11264),
	RETAIL(maxActiveTextures, 11268), RETAIL(colorBits, 11272), RETAIL(depthBits, 11276),
	RETAIL(stencilBits, 11280), RETAIL(driverType, 11284), RETAIL(hardwareType, 11288),
	RETAIL(deviceSupportsGamma, 11292), RETAIL(textureCompression, 11296),
	RETAIL(textureEnvAddAvailable, 11300), RETAIL(vidWidth, 11304), RETAIL(vidHeight, 11308),
	RETAIL(windowAspect, 11312), RETAIL(displayFrequency, 11316), RETAIL(isFullscreen, 11320),
	RETAIL(stereoEnabled, 11324), RETAIL(smpActive, 11328)
};
clientStatic_t cls;
static vm_t vm;
static int expectError;
static jmp_buf errorJump;

/** Fail when the module ABI differs from what retail QVMs were compiled against. */
static void Check( int ok, const char *message ) {
	if ( !ok ) { fprintf( stderr, "Cgame glconfig regression failed: %s\n", message ); exit( 1 ); }
}
/** Accept only the expected out-of-range rejection, which must leave the image untouched. */
void QDECL Com_Error( int level, const char *format, ... ) {
	int i;
	(void)format;
	Check( expectError && level == ERR_DROP, "GetGlconfig rejected a retail-sized QVM buffer" );
	for ( i = 0; i < IMAGE_SIZE; i++ ) {
		Check( vm.dataBase[i] == 'x', "rejected GetGlconfig changed QVM data" );
	}
	longjmp( errorJump, 1 );
}
/** Ignore diagnostics outside the glconfig copy. */
void QDECL Com_Printf( const char *format, ... ) { (void)format; }
/** Run the CG_GETGLCONFIG argument check and copy exactly as the syscall does. */
static void GetGlconfig( int pointer ) {
	int args[2];
	args[0] = CG_GETGLCONFIG; args[1] = pointer;
	vm.interpretFaulted = qfalse; vm.currentlyInterpreting = qtrue;
	memset( vm.dataBase, 'x', IMAGE_SIZE );
	CL_GetGlconfig( VMAP(1, glconfig_t) );
}
/** Read a 4-byte field the way a retail QVM does: a native LOAD4 at the retail offset. */
static int RetailWord( int base, int offset ) {
	int value;
	memcpy( &value, vm.dataBase + base + offset, sizeof(value) );
	return value;
}
/** Copy into a guarded slot and require exactly the retail bytes and field positions. */
static void CopyAt( int base ) {
	float aspect;
	int i;
	GetGlconfig( base );
	for ( i = 0; i < IMAGE_SIZE; i++ ) {
		if ( i < base || i >= base + RETAIL_SIZE ) {
			Check( vm.dataBase[i] == 'x', "GetGlconfig wrote outside the retail glconfig_t" );
		}
	}
	Check( !memcmp( vm.dataBase + base, &cls.glconfig, RETAIL_SIZE ), "copied glconfig bytes" );
	Check( !strcmp( (char *)vm.dataBase + base + 3072, "GL_EXT_texture_env_add" ), "extensions" );
	Check( RetailWord( base, 11300 ) == qtrue, "textureEnvAddAvailable" );
	Check( RetailWord( base, 11304 ) == 1024, "vidWidth (cgs.screenXScale input)" );
	Check( RetailWord( base, 11308 ) == 768, "vidHeight (cgs.screenYScale/uis.scale input)" );
	memcpy( &aspect, vm.dataBase + base + 11312, sizeof(aspect) );
	Check( aspect == 1.25f, "windowAspect" );
	Check( RetailWord( base, 11316 ) == 75, "displayFrequency" );
	Check( RetailWord( base, 11320 ) == qtrue, "isFullscreen" );
	Check( RetailWord( base, 11324 ) == qfalse && RetailWord( base, 11328 ) == qtrue,
	       "stereoEnabled/smpActive" );
}
/** Prove the native layout, an end-of-image retail slot, and rejection one word past it. */
int main( void ) {
	int i;
	Check( sizeof(glconfig_t) == RETAIL_SIZE, "sizeof(glconfig_t)" );
	for ( i = 0; i < (int)(sizeof(layout) / sizeof(layout[0])); i++ ) {
		if ( layout[i].native != (size_t)layout[i].retail ) {
			fprintf( stderr, "glconfig_t.%s is at %d, retail 1.32c uses %d\n",
			         layout[i].name, (int)layout[i].native, layout[i].retail );
			Check( 0, "glconfig_t field offset" );
		}
	}
	vm.dataBase = malloc( IMAGE_SIZE );
	Check( vm.dataBase != NULL, "allocation" );
	vm.dataMask = IMAGE_SIZE - 1; currentVM = &vm;
	Q_strncpyz( cls.glconfig.renderer_string, "retail renderer", sizeof(cls.glconfig.renderer_string) );
	Q_strncpyz( cls.glconfig.extensions_string, "GL_EXT_texture_env_add",
	            sizeof(cls.glconfig.extensions_string) );
	cls.glconfig.textureEnvAddAvailable = qtrue;
	cls.glconfig.vidWidth = 1024; cls.glconfig.vidHeight = 768;
	cls.glconfig.windowAspect = 1.25f; cls.glconfig.displayFrequency = 75;
	cls.glconfig.isFullscreen = qtrue; cls.glconfig.smpActive = qtrue;
	CopyAt( 4 );
	CopyAt( IMAGE_SIZE - RETAIL_SIZE );
	expectError = 1;
	if ( setjmp( errorJump ) == 0 ) {
		GetGlconfig( IMAGE_SIZE - RETAIL_SIZE + 4 );
		Check( 0, "GetGlconfig accepted a slot that overruns the QVM image" );
	}
	Check( vm.interpretFaulted && !vm.currentlyInterpreting, "fault state" );
	free( vm.dataBase );
	puts( "Cgame retail glconfig_t regressions passed (issue #236)" );
	return 0;
}
