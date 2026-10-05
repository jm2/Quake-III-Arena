/* Issue #47: AAS_PointReachabilityAreaIndex for a point inside a portal area. */
#include "../code/botlib/be_aas_sample.c"
#include <stdlib.h>

aas_t aasworld;
botlib_import_t botimport;

static void Check( int ok, const char *message ) {
	if ( !ok ) { fprintf( stderr, "AAS point index regression failed: %s\n", message ); exit( 1 ); }
}
int AAS_AreaReachability( int areanum ) {
	return aasworld.areasettings[areanum].numreachableareas;
}
/** One node puts every point in area 1, which is the portal between clusters 1 and 2. */
static void Build( int frontcluster, int backcluster ) {
	static aas_plane_t planes[2];
	static aas_node_t nodes[2];
	static aas_areasettings_t settings[2];
	static aas_cluster_t clusters[4];
	aas_portal_t *portals;

	memset( &aasworld, 0, sizeof(aasworld) );
	memset( settings, 0, sizeof(settings) );
	memset( clusters, 0, sizeof(clusters) );
	planes[0].normal[2] = 1; planes[0].type = 2;
	nodes[1].planenum = 0; nodes[1].children[0] = nodes[1].children[1] = -1;
	settings[1].cluster = -1; settings[1].numreachableareas = 1; settings[1].firstreachablearea = 1;
	clusters[1].numreachabilityareas = 5;
	clusters[2].numreachabilityareas = 7;
	clusters[3].numreachabilityareas = 11;
	// a heap array, so reading before portals[0] is caught by ASan
	portals = calloc( 2, sizeof(*portals) );
	Check( portals != NULL, "portal allocation" );
	portals[1].areanum = 1;
	portals[1].frontcluster = frontcluster; portals[1].backcluster = backcluster;
	portals[1].clusterareanum[0] = 3; portals[1].clusterareanum[1] = 4;
	aasworld.planes = planes; aasworld.numplanes = 2;
	aasworld.nodes = nodes; aasworld.numnodes = 2;
	aasworld.areasettings = settings; aasworld.numareas = aasworld.numareasettings = 2;
	aasworld.portals = portals; aasworld.numportals = 2;
	aasworld.clusters = clusters; aasworld.numclusters = 4;
	aasworld.loaded = aasworld.initialized = qtrue;
}
int main( void ) {
	vec3_t origin = { 8, 8, 8 };

	// front cluster 2: clusters 0 and 1 hold 0 + 5 slots, then the portal's front slot 3
	Build( 2, 1 );
	Check( AAS_PointAreaNum( origin ) == 1, "fixture point lies in the portal area" );
	Check( AAS_PointReachabilityAreaIndex( origin ) == 8, "portal index uses its front cluster and slot" );
	free( aasworld.portals );
	// front cluster 3: 0 + 5 + 7 slots before it, then slot 3
	Build( 3, 1 );
	Check( AAS_PointReachabilityAreaIndex( origin ) == 15, "portal index from a later front cluster" );
	Check( AAS_PointReachabilityAreaIndex( NULL ) == 23, "NULL origin still totals every cluster" );
	free( aasworld.portals );
	puts( "AAS portal-area reachability index regression passed (issue #47)" );
	return 0;
}
