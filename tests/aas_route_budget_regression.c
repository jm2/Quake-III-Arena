/* Issue #47: routing tables and caches stay within the zone memory left to botlib. */
#include "../code/botlib/be_aas_route.c"
#include <stdlib.h>

aas_t aasworld;
botlib_import_t botimport;

static int available, requests, overdrawn;
static void *live;

static void Check( int ok, const char *message ) {
	if ( !ok ) { fprintf( stderr, "AAS route budget regression failed: %s\n", message ); exit( 1 ); }
}
/** The engine zone raises ERR_FATAL for a request larger than what it has left. */
void *GetClearedMemory( unsigned long size ) {
	requests++;
	if ( size > (unsigned long)available ) { overdrawn++; return NULL; }
	Check( live == NULL, "one fixture allocation at a time" );
	live = calloc( 1, size );
	return live;
}
void FreeMemory( void *ptr ) { Check( ptr == live, "frees the fixture allocation" ); free( ptr ); live = NULL; }
int AvailableMemory( void ) { return available; }
int AAS_AreaCrouch( int areanum ) { (void)areanum; return 0; }
int AAS_AreaSwim( int areanum ) { (void)areanum; return 0; }
int Sys_MilliSeconds( void ) { return 0; }

/** Areas 1 and 2 each have two reachabilities into the other. */
static void Build( void ) {
	static aas_areasettings_t settings[3];
	static aas_area_t areas[3];
	static aas_reachability_t reaches[5];
	static aas_reversedreachability_t reversed[3];
	static aas_reversedlink_t links[4];
	int i;

	memset( &aasworld, 0, sizeof(aasworld) );
	memset( settings, 0, sizeof(settings) );
	memset( reaches, 0, sizeof(reaches) );
	for ( i = 1; i <= 2; i++ ) {
		settings[i].numreachableareas = 2;
		settings[i].firstreachablearea = 2 * i - 1;
		settings[i].presencetype = PRESENCE_NORMAL;
		reversed[i].numlinks = 2;
		reversed[i].first = &links[2 * i - 2];
		links[2 * i - 2].next = &links[2 * i - 1];
		links[2 * i - 1].next = NULL;
		// area i is reached by the two reachabilities owned by the other area
		links[2 * i - 2].linknum = 2 * (3 - i) - 1;
		links[2 * i - 1].linknum = 2 * (3 - i);
		links[2 * i - 2].areanum = links[2 * i - 1].areanum = 3 - i;
	}
	for ( i = 1; i <= 4; i++ ) {
		reaches[i].areanum = i <= 2 ? 2 : 1;
		reaches[i].traveltype = TRAVEL_WALK;
		reaches[i].start[0] = 10.0f * i;
		reaches[i].end[0] = 10.0f * i + 100.0f;
	}
	aasworld.areasettings = settings; aasworld.areas = areas;
	aasworld.numareas = aasworld.numareasettings = 3;
	aasworld.reachability = reaches; aasworld.reachabilitysize = 5;
	aasworld.reversedreachability = reversed;
}
int main( void ) {
	int size = 3 * (int)sizeof(unsigned short **) + 4 * (int)sizeof(unsigned short *) +
		8 * (int)sizeof(unsigned short);
	int cachesize = (int)sizeof(aas_routingcache_t) + 10 * 3;
	aas_routingcache_t *cache;

	Build();
	available = size - 1;
	Check( !AAS_CalculateAreaTravelTimes() && !aasworld.areatraveltimes,
		"travel-time table larger than the zone has left fails" );
	Check( !overdrawn && !requests, "no request the engine zone cannot satisfy" );
	available = size;
	Check( AAS_CalculateAreaTravelTimes() && aasworld.areatraveltimes && requests == 1,
		"travel-time table that fits is built" );
	Check( aasworld.areatraveltimes[1][0][0] == AAS_AreaTravelTime( 1, aasworld.reachability[3].end,
		aasworld.reachability[1].start ), "travel time from link to reachability" );
	FreeMemory( aasworld.areatraveltimes ); aasworld.areatraveltimes = NULL;

	requests = 0; routingcachesize = 0;
	available = cachesize - 1;
	Check( AAS_AllocRoutingCache( 10 ) == NULL && !requests && !overdrawn && !routingcachesize,
		"routing cache larger than the zone has left is refused" );
	available = cachesize;
	cache = AAS_AllocRoutingCache( 10 );
	Check( cache != NULL && requests == 1 && routingcachesize == cachesize && cache->size == cachesize,
		"routing cache that fits is allocated" );
	FreeMemory( cache );
	puts( "AAS routing table and cache zone budget regression passed (issue #47)" );
	return 0;
}
