/* R_StitchPatches' reversed loops (the grid1 edge walked from its last point
 * down) insert grid1's point k - 1 into grid2 with the LOD error at k + 1. On
 * the first pass k is the last point, so that read was one float past the
 * error array. In retail the array is a zone block, and the float after it
 * is usually the zone's trash tester, ZONEID (0x1d4a11): read as a float, a
 * denormal of about 2.7e-39, which counts as no error at all (or stale bytes,
 * when the zone left a small fragment unsplit). The read must stay in
 * bounds (AddressSanitizer reports it here otherwise) and give that zero LOD
 * error; every read inside the array keeps its retail value, so stitching on
 * valid maps is unchanged. Upstream id, ioquake3 and Quake3e have the read. */
#define __QGL_H__
typedef unsigned int GLuint;
#define GL_CLAMP 0x2900
#include "../code/renderer/tr_bsp.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
trGlobals_t tr;
glconfig_t glConfig;
refimport_t ri;

static int inserts;
static float insertedError;
static vec3_t insertedPoint;
static void Check( int condition, const char *message ) {
	if ( !condition ) { fprintf( stderr, "BSP stitch regression failed: %s\n", message ); exit( 1 ); }
}
srfGridMesh_t *R_GridInsertColumn( srfGridMesh_t *grid, int column, int row, vec3_t point, float loderror ) {
	(void)column; (void)row;
	inserts++; insertedError = loderror; VectorCopy( point, insertedPoint );
	return grid;
}
srfGridMesh_t *R_GridInsertRow( srfGridMesh_t *grid, int row, int column, vec3_t point, float loderror ) {
	return R_GridInsertColumn( grid, column, row, point, loderror );
}

/* A width x 2 grid whose first row runs along x at y = rowY, and whose second row is far away. */
static srfGridMesh_t *Grid( int width, const float *xs, float rowY, float farY ) {
	srfGridMesh_t *grid = calloc( 1, offsetof( srfGridMesh_t, verts ) + 2 * width * sizeof( drawVert_t ) );
	int i;
	Check( grid != NULL, "grid allocation" );
	grid->surfaceType = SF_GRID; grid->width = width; grid->height = 2;
	/* exact-size heap arrays, so a read past the end is caught */
	grid->widthLodError = malloc( width * sizeof( float ) );
	grid->heightLodError = malloc( 2 * sizeof( float ) );
	Check( grid->widthLodError && grid->heightLodError, "LOD error arrays" );
	for ( i = 0; i < width; i++ ) {
		grid->widthLodError[i] = 10.0f + i;
		grid->verts[i].xyz[0] = xs[i]; grid->verts[i].xyz[1] = rowY;
		grid->verts[width + i].xyz[0] = xs[i]; grid->verts[width + i].xyz[1] = farY;
	}
	grid->heightLodError[0] = grid->heightLodError[1] = 20.0f;
	return grid;
}

int main( void ) {
	static const float xs1[] = { 0, 1, 2 }, xs2[] = { 2, 0 };
	msurface_t surfaces[2];
	srfGridMesh_t *grid1 = Grid( 3, xs1, 0, 100 );
	/* grid2's edge runs the other way, so only the reversed loop matches it */
	srfGridMesh_t *grid2 = Grid( 2, xs2, 0, -100 );

	memset( surfaces, 0, sizeof( surfaces ) );
	surfaces[0].data = (void *)grid1; surfaces[1].data = (void *)grid2;
	s_worldData.surfaces = surfaces; s_worldData.numsurfaces = 2;

	Check( R_StitchPatches( 0, 1 ) && inserts == 1, "the reversed edge is stitched once" );
	Check( insertedPoint[0] == 1 && insertedPoint[1] == 0, "grid1's middle point is inserted" );
	Check( insertedError == 0.0f, "the last point's error past the array is retail's zero" );

	free( grid1->widthLodError ); free( grid1->heightLodError ); free( grid1 );
	free( grid2->widthLodError ); free( grid2->heightLodError ); free( grid2 );
	puts( "BSP stitch regression passed" );
	return 0;
}
