/* Issue #47: AAS and BSP entity model numbers reach the engine's inline-model lookup. */
#include "../code/server/sv_bot.c"
#include <stdlib.h>
#include <string.h>

static int inlineCalls;

static void Check( int ok, const char *message ) {
	if ( !ok ) { fprintf( stderr, "Server bot model regression failed: %s\n", message ); exit( 1 ); }
}
int CM_NumInlineModels( void ) { return 3; }
/** The real lookup raises ERR_DROP and ends the map for a number out of range. */
clipHandle_t CM_InlineModel( int index ) {
	Check( index >= 0 && index < CM_NumInlineModels(), "CM_InlineModel would drop the server" );
	inlineCalls++;
	return index;
}
void CM_ModelBounds( clipHandle_t model, vec3_t mins, vec3_t maxs ) {
	VectorSet( mins, -1.0f - model, -2, -3 );
	VectorSet( maxs, 1.0f + model, 2, 3 );
}
void QDECL Com_DPrintf( const char *fmt, ... ) { (void)fmt; }

int main( void ) {
	static const int bad[] = { -1, 3, 4, 0xffff, INT_MAX, INT_MIN };
	vec3_t still = { 0, 0, 0 }, turned = { 0, 90, 0 }, mins, maxs, origin;
	int i;

	for ( i = 0; i < (int)(sizeof(bad) / sizeof(bad[0])); i++ ) {
		memset( mins, 0x7f, sizeof(mins) ); memset( maxs, 0x7f, sizeof(maxs) ); memset( origin, 0x7f, sizeof(origin) );
		BotImport_BSPModelMinsMaxsOrigin( bad[i], still, mins, maxs, origin );
		Check( VectorCompare( mins, vec3_origin ) && VectorCompare( maxs, vec3_origin ) &&
			VectorCompare( origin, vec3_origin ), "bad model number gives empty bounds" );
		BotImport_BSPModelMinsMaxsOrigin( bad[i], turned, NULL, NULL, NULL );
	}
	Check( !inlineCalls, "bad model numbers never reach CM_InlineModel" );
	BotImport_BSPModelMinsMaxsOrigin( 2, still, mins, maxs, origin );
	Check( inlineCalls == 1 && mins[0] == -3 && maxs[0] == 3 && maxs[2] == 3 && VectorCompare( origin, vec3_origin ),
		"valid model keeps its bounds" );
	BotImport_BSPModelMinsMaxsOrigin( 0, turned, mins, maxs, NULL );
	VectorSet( origin, 1, 2, 3 );
	Check( inlineCalls == 2 && mins[1] == -RadiusFromBounds( vec3_origin, origin ) &&
		maxs[2] == RadiusFromBounds( vec3_origin, origin ), "rotated valid model keeps its radius bounds" );
	puts( "Server bot inline-model bounds regression passed (issue #47)" );
	return 0;
}
