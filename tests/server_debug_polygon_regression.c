/* Issue #35: test the actual fixed native debug-polygon storage. */
#include "../code/server/sv_bot.c"
#include <stdlib.h>
#include <string.h>

/** Supply exact byte copying for the native debug import routines. */
void Com_Memcpy( void *dest, const void *src, size_t length ) { memcpy( dest, src, length ); }
/** Fail on unexpected point data or native debug-array mutation. */
static void Check( int ok ) {
	if ( !ok ) { fprintf( stderr, "Server debug-polygon regression failed\n" ); exit( 1 ); }
}
/** Exercise full point capacity, invalid counts/handles, and zero-point line reservation. */
int main( void ) {
	vec3_t points[MAX_DEBUG_POLY_POINTS];
	bot_debugpoly_t before[3];
	int id, i, bad[] = {-1, INT_MIN, INT_MAX, MAX_DEBUG_POLY_POINTS + 1};
	bot_maxdebugpolys = 3;
	debugpolygons = calloc( bot_maxdebugpolys, sizeof(*debugpolygons) );
	Check( debugpolygons != NULL ); memset( points, 0x5a, sizeof(points) );
	id = BotImport_DebugPolygonCreate( 7, MAX_DEBUG_POLY_POINTS, points );
	Check( id == 1 && debugpolygons[1].numPoints == MAX_DEBUG_POLY_POINTS &&
	       !memcmp( points, debugpolygons[1].points, sizeof(points) ) );
	BotImport_DebugPolygonShow( id, 8, MAX_DEBUG_POLY_POINTS, points );
	Check( debugpolygons[1].color == 8 ); memcpy( before, debugpolygons, sizeof(before) );
	for ( i = 0; i < sizeof(bad) / sizeof(bad[0]); i++ ) {
		Check( BotImport_DebugPolygonCreate( 0, bad[i], points ) == 0 );
		BotImport_DebugPolygonShow( id, 0, bad[i], points );
		Check( !memcmp( before, debugpolygons, sizeof(before) ) );
	}
	Check( BotImport_DebugPolygonCreate( 0, 1, NULL ) == 0 );
	BotImport_DebugPolygonShow( id, 0, 1, NULL );
	for ( i = -1; i <= bot_maxdebugpolys; i++ ) {
		if ( i == 1 || i == 2 ) continue;
		BotImport_DebugPolygonShow( i, 0, 1, points ); BotImport_DebugPolygonDelete( i );
	}
	BotImport_DebugPolygonShow( INT_MAX, 0, 1, points ); BotImport_DebugPolygonDelete( INT_MAX );
	Check( !memcmp( before, debugpolygons, sizeof(before) ) );
	Check( BotImport_DebugPolygonCreate( 9, 0, NULL ) == 2 );
	Check( debugpolygons[2].inuse && debugpolygons[2].numPoints == 0 );
	Check( BotImport_DebugPolygonCreate( 9, 0, NULL ) == 0 );
	BotImport_DebugPolygonShow( 2, 10, 0, NULL );
	Check( debugpolygons[2].color == 10 && debugpolygons[2].numPoints == 0 );
	BotImport_DebugPolygonDelete( 1 ); Check( !debugpolygons[1].inuse );
	free( debugpolygons ); debugpolygons = NULL;
	Check( BotImport_DebugPolygonCreate( 0, 1, points ) == 0 );
	BotImport_DebugPolygonShow( 1, 0, 1, points ); BotImport_DebugPolygonDelete( 1 );
	puts( "Server native debug-polygon regressions passed (issue #35)" );
	return 0;
}
