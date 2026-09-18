/* Issue #35: run the actual cgame array filters before renderer callbacks. */
#include "../code/client/cl_cgame.c"
#include "../code/qcommon/vm_local.h"
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

#define IMAGE_SIZE 512
refexport_t re;
static vm_t vm;
static byte before[IMAGE_SIZE];
static int expectError, polyCalls, fragmentCalls;
static int expectedVertices, expectedPolys, expectedOffset;
static jmp_buf errorJump;

/** Fail when dispatch, memory boundaries, or fault state differ. */
static void Check( int ok, const char *message ) {
	if ( !ok ) { fprintf( stderr, "Cgame array regression failed: %s\n", message ); exit( 1 ); }
}
/** Catch controlled drops and prove no renderer call or data mutation occurred. */
void QDECL Com_Error( int level, const char *format, ... ) {
	(void)format;
	Check( expectError && level == ERR_DROP, "unexpected engine error" );
	Check( vm.interpretFaulted && !vm.currentlyInterpreting, "fault state" );
	Check( !memcmp( before, vm.dataBase, IMAGE_SIZE ), "rejection changed data" );
	longjmp( errorJump, 1 );
}
/** Ignore diagnostics outside the isolated array filters. */
void QDECL Com_Printf( const char *format, ... ) { (void)format; }
/** Verify the native callback sees the complete checked polygon batch. */
static void AddPolys( qhandle_t shader, int count, const polyVert_t *vertices, int batches ) {
	const byte *bytes = (const byte *)vertices;
	int length = count * batches * sizeof(polyVert_t);
	Check( !expectError && shader == 7, "unexpected polygon callback" );
	Check( count == expectedVertices && batches == expectedPolys, "polygon dimensions" );
	Check( bytes == vm.dataBase + expectedOffset, "masked polygon pointer" );
	Check( bytes[0] == 'x' && bytes[length - 1] == 'x', "complete polygon range" );
	polyCalls++;
}
/** Touch full checked output arrays and return a visible renderer result. */
static int MarkFragments( int count, const vec3_t *points, const vec3_t projection,
	int maxPoints, vec3_t pointBuffer, int maxFragments, markFragment_t *fragments ) {
	Check( !expectError && count == 3 && maxPoints == 6 && maxFragments == 2,
	       "fragment dimensions" );
	Check( (const byte *)points == vm.dataBase + 4 &&
	       (const byte *)projection == vm.dataBase + 40, "fragment input pointers" );
	memset( pointBuffer, 0, maxPoints * sizeof(vec3_t) );
	memset( fragments, 0, maxFragments * sizeof(markFragment_t) );
	fragmentCalls++;
	return 2;
}
/** Restore a live VM and preserve its complete data image for rejection checks. */
static void Reset( void ) {
	vm.interpretFaulted = qfalse;
	vm.currentlyInterpreting = qtrue;
	memset( vm.dataBase, 'x', IMAGE_SIZE );
	memcpy( before, vm.dataBase, IMAGE_SIZE );
}
/** Require malformed polygon dimensions or ranges to fail before native dispatch. */
static void RejectPolys( int count, int pointer, int batches ) {
	int calls = polyCalls;
	Reset(); expectError = 1;
	if ( setjmp( errorJump ) == 0 ) {
		CL_CgameAddPolys( 7, count, pointer, batches );
		Check( 0, "invalid polygon accepted" );
	}
	expectError = 0;
	Check( polyCalls == calls, "rejected polygon dispatched" );
}
/** Require malformed fragment counts or ranges to fail before native dispatch. */
static void RejectFragments( int *args ) {
	int calls = fragmentCalls;
	Reset(); expectError = 1;
	if ( setjmp( errorJump ) == 0 ) {
		CL_CgameMarkFragments( args );
		Check( 0, "invalid fragment request accepted" );
	}
	expectError = 0;
	Check( fragmentCalls == calls, "rejected fragment request dispatched" );
}
/** Cover count multiplication, exact-end batches, empty capacities, and each array. */
int main( void ) {
	int args[8] = {CG_CM_MARKFRAGMENTS, 3, 4, 40, 6, IMAGE_SIZE - 6 * sizeof(vec3_t), 2, 320};
	int i, original;
	vm.dataBase = malloc( IMAGE_SIZE );
	Check( vm.dataBase != NULL, "allocation" );
	vm.dataMask = IMAGE_SIZE - 1; currentVM = &vm;
	re.AddPolyToScene = AddPolys; re.MarkFragments = MarkFragments;
	Reset();
	expectedVertices = 3; expectedPolys = 2;
	expectedOffset = IMAGE_SIZE - 6 * sizeof(polyVert_t);
	CL_CgameAddPolys( 7, 3, expectedOffset, 2 );
	CL_CgameAddPolys( 7, 3, IMAGE_SIZE + expectedOffset, 2 );
	Check( polyCalls == 2, "valid polygon callbacks" );
	CL_CgameAddPolys( 7, 0, 0, INT_MAX ); CL_CgameAddPolys( 7, INT_MAX, 0, 0 );
	Check( polyCalls == 2, "empty polygon dispatched" );
	RejectPolys( -1, 4, 1 ); RejectPolys( 1, 4, -1 );
	RejectPolys( 0, 0, -1 ); RejectPolys( -1, 0, 0 );
	RejectPolys( INT_MAX, 4, 2 ); RejectPolys( INT_MAX, 4, 1 );
	RejectPolys( 3, expectedOffset + 4, 2 ); RejectPolys( 3, 5, 1 );
	RejectPolys( 3, 0, 1 );
	Reset();
	Check( CL_CgameMarkFragments( args ) == 2 && fragmentCalls == 1, "valid fragment result" );
	for ( i = 1; i <= 6; i += (i == 1 ? 3 : 2) ) {
		original = args[i]; args[i] = 0;
		Check( CL_CgameMarkFragments( args ) == 0 && fragmentCalls == 1, "empty capacity dispatched" );
		args[i] = -1; RejectFragments( args );
		args[i] = INT_MAX; RejectFragments( args ); args[i] = original;
	}
	for ( i = 2; i <= 7; i++ ) {
		if ( i == 4 || i == 6 ) continue;
		original = args[i]; args[i] = 0; RejectFragments( args );
		args[i] = IMAGE_SIZE - 4; RejectFragments( args );
		args[i] = 5; RejectFragments( args ); args[i] = original;
	}
	free( vm.dataBase );
	puts( "Cgame checked array regressions passed (issue #35)" );
	return 0;
}
