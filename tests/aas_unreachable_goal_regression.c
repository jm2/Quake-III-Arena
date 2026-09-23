/* Goal areas without reachabilities keep native empty caches, zero-time portal routes and dumps (issue #252). */
#define Q3_AAS_CACHE_FILE_ENTRY CacheFileFixtureMain
#include "aas_cache_file_regression.c"
static void PitWorld(void) {
    FileWorld();
    /* Areas 1 and 3 have no reachabilities, so native numbering puts them after each cluster's reachability areas. */
    settings[1].numreachableareas=settings[3].numreachableareas=0;
    settings[1].clusterareanum=1;portals[1].clusterareanum[0]=0;portals[2].clusterareanum[0]=2;
    clusters[1].numreachabilityareas=clusters[3].numreachabilityareas=1;
    reversed[1].numlinks=1;links[0].next=NULL;reversed[2].numlinks=1;reversed[2].first=links+3;
}
static void Expect(int start,int goal,int routed,int expectedTime,int expectedReach,const char *message) {
    int time=-1,reach=-1;vec3_t origin={0,0,0};
    Check(AAS_AreaRouteToGoalArea(start,origin,goal,-1,&time,&reach)==routed&&(!routed||(time==expectedTime&&reach==expectedReach)),message);
    Check(AAS_AreaTravelTimeToGoalArea(start,origin,goal,-1)==expectedTime&&AAS_AreaReachabilityToGoalArea(start,origin,goal,-1)==(routed?expectedReach:0),message);
}
static void Queries(void) {
    Expect(2,1,qtrue,0,2,"portal start keeps the native zero-time route to a goal without reachabilities");
    Expect(4,1,qfalse,0,0,"cluster start still has no route to a goal without reachabilities");
    Expect(2,3,qtrue,0,2,"portal start keeps the native zero-time route to a goal portal without reachabilities");
    Expect(4,2,qtrue,12,4,"ordinary routes keep literal native costs beside empty caches");
}
static void EmptyCaches(void) {
    Check(areaSlots[1][1]&&!areaSlots[1][1]->traveltimes[0]&&areaSlots[1][2]&&!areaSlots[1][2]->traveltimes[0],"goal caches outside the reachability range are published empty");
    Check(portalCaches[1]&&!portalCaches[1]->traveltimes[1]&&!portalCaches[1]->traveltimes[2],"portal cache for an unreachable goal is published empty");
    Check(portalCaches[3]&&portalCaches[3]->traveltimes[2]==1&&!portalCaches[3]->traveltimes[1],"goal portal cache keeps only its native start entry");
    Check(Outstanding()==5&&routingcachesize==PhysicalBytes(),"three area and two portal caches own all routing bytes");
}
int main(void) {
    routecacheheader_t header;int prior;
    PitWorld();Queries();EmptyCaches();prior=requests;Queries();
    Check(requests==prior&&Outstanding()==5,"warm empty caches are reused without allocation");
    expectedBytes=routingcachesize;AAS_WriteRouteCache();memcpy(&header,dump,sizeof(header));
    Check(header.numportalcache==2&&header.numareacache==3&&dumpLength==(int)sizeof(header)+expectedBytes,"native writer dumps the empty caches");Cleanup();
    PitWorld();OriginalInput();
    Check(AAS_ReadRouteCache()&&routingcachesize==expectedBytes&&Outstanding()==5,"reader accepts the writer's empty goal caches");CheckOwnership();EmptyCaches();
    prior=requests;Queries();Check(requests==prior,"loaded caches answer every query");CheckOwnership();Cleanup();
    puts("Native AAS goals without reachabilities keep empty caches, zero-time portal routes and dump round trips (issue #252)");
    return 0;
}
