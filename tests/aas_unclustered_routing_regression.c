/* Native clustering can leave isolated areas at cluster zero; routing must not alias caches. */
#include Q3_AAS_ROUTE_SOURCE
#include Q3_AAS_CLUSTER_SOURCE
botlib_import_t botimport;
aas_t aasworld;
int botDeveloper;
static aas_cluster_t clusters[2];
static aas_areasettings_t settings[4];
static aas_area_t areas[4];
static aas_reachability_t reaches[2];
static aas_portal_t portals[1];
static aas_reversedreachability_t reversed[4];
static aas_routingupdate_t areaUpdates[4],portalUpdates[2];
static aas_routingcache_t *areaSlots[1],**clusterCaches[2],*portalCaches[4];
static int contents[4],requests;
static void *owners[8];
static void Check(int condition,const char *message) {
    if(!condition){fprintf(stderr,"AAS unclustered routing regression failed: %s\n",message);exit(1);}
}
#ifndef Com_Memset
void Com_Memset(void *out,int value,size_t size) {memset(out,value,size);}
#endif
void *GetClearedMemory(unsigned long size) {
    Check(size>0&&size<=512&&requests<8,"bounded native cache allocation");
    owners[requests]=calloc(1,size);Check(owners[requests]!=NULL,"fixture cache allocation");return owners[requests++];
}
void FreeMemory(void *pointer) {(void)pointer;Check(0,"cache eviction is outside this fixture");}
int AvailableMemory(void) {return INT_MAX;}
float AAS_Time(void) {return 7;}
void QDECL Log_Write(char *format,...) {(void)format;}
void QDECL AAS_Error(char *format,...) {(void)format;Check(0,"unexpected native clustering error");}
int AAS_AreaReachability(int area) {Check(area>0&&area<4,"native numbered area");return settings[area].numreachableareas;}
int AAS_AreaDoNotEnter(int area) {Check(area>0&&area<4,"bounded route area");return 0;}
int AAS_AreaCrouch(int area) {(void)area;return 0;}
int AAS_AreaSwim(int area) {(void)area;return 0;}
static void NativeWorld(void) {
    memset(&aasworld,0,sizeof(aasworld));aasworld.numareas=aasworld.numareasettings=4;aasworld.numclusters=1;aasworld.numportals=1;
    aasworld.clusters=clusters;aasworld.areasettings=settings;aasworld.areas=areas;aasworld.reachability=reaches;aasworld.portals=portals;
    aasworld.reversedreachability=reversed;aasworld.areaupdate=areaUpdates;aasworld.portalupdate=portalUpdates;aasworld.areacontentstravelflags=contents;
    clusterCaches[0]=clusterCaches[1]=areaSlots;aasworld.clusterareacache=clusterCaches;aasworld.portalcache=portalCaches;
    settings[1].numreachableareas=1;settings[1].firstreachablearea=1;reaches[1].areanum=1;reaches[1].traveltype=TRAVEL_WALK;reaches[1].traveltime=10;
    AAS_FindClusters();AAS_InitTravelFlagFromType();aasworld.loaded=aasworld.initialized=qtrue;
    Check(aasworld.numclusters==2&&settings[1].cluster==1&&!settings[2].cluster&&!settings[3].cluster,"actual native clustering leaves isolated nonreachable areas in cluster zero");
    Check(clusters[0].numareas==0&&clusters[1].numareas==1&&clusters[1].numreachabilityareas==1,"actual native numbering preserves empty dummy and one reachable slot");
}
static void Route(int start,int goal) {
    int time=0,reach=0;vec3_t origin={0,0,0};
    Check(!AAS_AreaRouteToGoalArea(start,origin,goal,-1,&time,&reach),"distinct isolated-area routes remain unreachable");
    Check(!requests&&!routingcachesize&&!areaSlots[0]&&!portalCaches[goal]&&!aasworld.oldestcache&&!aasworld.newestcache,"unclustered queries cannot allocate or alias another cluster cache");
}
int main(int argc,char **argv) {
    NativeWorld();Route(1,2);
    if(argc==1){Route(2,1);Route(2,3);puts("Native AAS isolated-area clustering and routing cache ownership passed (issue #47)");}
    while(requests)free(owners[--requests]);return 0;
}
