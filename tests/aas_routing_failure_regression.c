/* Actual cache providers: nullable creation/update, physical ownership and retry. */
#include Q3_AAS_ROUTE_SOURCE
botlib_import_t botimport;
aas_t aasworld;
int botDeveloper;
static aas_cluster_t clusters[4];
static aas_areasettings_t settings[6];
static aas_area_t areas[6];
static aas_portal_t portals[3];
static aas_portalindex_t indexes[4];
static aas_reachability_t reaches[6];
static aas_reversedreachability_t reversed[6];
static aas_reversedlink_t links[5];
static aas_routingupdate_t areaUpdates[6],portalUpdates[4];
static aas_routingcache_t *areaSlots[4][3],**clusterCaches[4],*portalCaches[6];
static unsigned short costs[6][2],*rows[6][1],**times[6];
static int contents[6],portalCosts[3],requests,releases,failAt;
static struct {void *pointer;unsigned long size;} owners[32];
static void Check(int condition,const char *message) {
    if(!condition){fprintf(stderr,"AAS routing failure regression failed: %s\n",message);exit(1);}
}
#ifndef Com_Memset
void Com_Memset(void *out,int value,size_t size) {memset(out,value,size);}
#endif
void *GetClearedMemory(unsigned long size) {
    Check(size>0&&size<=1024&&requests<32,"bounded exact native cache/workspace request");requests++;
    if(requests==failAt)return NULL;
    owners[requests-1].pointer=calloc(1,size);owners[requests-1].size=size;
    Check(owners[requests-1].pointer!=NULL,"fixture allocation");return owners[requests-1].pointer;
}
void FreeMemory(void *pointer) {
    int i;if(!pointer)return;
    for(i=0;i<requests;i++)if(owners[i].pointer==pointer){free(pointer);owners[i].pointer=NULL;releases++;return;}
    Check(0,"unknown/double native physical release");
}
int AvailableMemory(void) {return INT_MAX;}
float AAS_Time(void) {return 7;}
int AAS_AreaDoNotEnter(int area) {Check(area>0&&area<aasworld.numareas,"bounded route area");return 0;}
int AAS_AreaCrouch(int area) {Check(area>0&&area<aasworld.numareas,"bounded native metric area");return 0;}
int AAS_AreaSwim(int area) {Check(area>0&&area<aasworld.numareas,"bounded native metric area");return 0;}
static int Outstanding(void) {
    int i,total=0;for(i=0;i<requests;i++)if(owners[i].pointer)total++;return total;
}
static void Cleanup(void) {
    while(aasworld.oldestcache)AAS_FreeRoutingCache(aasworld.oldestcache);
    Check(!Outstanding()&&!routingcachesize&&!aasworld.newestcache,"all native LRU cache owners physically release");
}
static void Reset(void) {
    int i;Check(!Outstanding()&&!routingcachesize,"previous native cache ownership released");
    memset(&aasworld,0,sizeof(aasworld));memset(clusters,0,sizeof(clusters));memset(settings,0,sizeof(settings));
    memset(areas,0,sizeof(areas));memset(portals,0,sizeof(portals));memset(indexes,0,sizeof(indexes));memset(reaches,0,sizeof(reaches));
    memset(reversed,0,sizeof(reversed));memset(links,0,sizeof(links));memset(areaUpdates,0,sizeof(areaUpdates));memset(portalUpdates,0,sizeof(portalUpdates));
    memset(areaSlots,0,sizeof(areaSlots));memset(portalCaches,0,sizeof(portalCaches));memset(owners,0,sizeof(owners));memset(contents,0,sizeof(contents));
    requests=releases=failAt=0;
    aasworld.numareas=aasworld.numareasettings=6;aasworld.numclusters=4;aasworld.numportals=3;aasworld.reachabilitysize=6;aasworld.portalindexsize=4;
    aasworld.clusters=clusters;aasworld.areasettings=settings;aasworld.areas=areas;aasworld.portals=portals;aasworld.portalindex=indexes;aasworld.reachability=reaches;
    aasworld.reversedreachability=reversed;aasworld.areaupdate=areaUpdates;aasworld.portalupdate=portalUpdates;aasworld.areacontentstravelflags=contents;
    aasworld.clusterareacache=clusterCaches;aasworld.portalcache=portalCaches;aasworld.areatraveltimes=times;aasworld.portalmaxtraveltimes=portalCosts;
    for(i=0;i<4;i++)clusterCaches[i]=areaSlots[i];
    for(i=0;i<6;i++){costs[i][0]=costs[i][1]=1;rows[i][0]=costs[i];times[i]=rows[i];}
    for(i=0;i<3;i++)portalCosts[i]=1;
    aasworld.loaded=aasworld.initialized=qtrue;AAS_InitTravelFlagFromType();
}
static void AreaWorld(void) {
    Reset();aasworld.numareas=aasworld.numareasettings=3;aasworld.numclusters=2;aasworld.numportals=0;
    clusters[1].numareas=clusters[1].numreachabilityareas=2;
    settings[1].cluster=settings[2].cluster=1;settings[1].clusterareanum=0;settings[2].clusterareanum=1;
    settings[1].numreachableareas=settings[2].numreachableareas=1;settings[1].firstreachablearea=2;settings[2].firstreachablearea=1;
    reaches[1].areanum=1;reaches[2].areanum=2;reaches[1].traveltype=reaches[2].traveltype=TRAVEL_WALK;reaches[1].traveltime=reaches[2].traveltime=10;
    links[0].areanum=2;links[0].linknum=1;links[1].areanum=1;links[1].linknum=2;
    reversed[1].numlinks=reversed[2].numlinks=1;reversed[1].first=links;reversed[2].first=links+1;
}
static void PortalWorld(void) {
    int i;Reset();
    for(i=1;i<6;i++){settings[i].numreachableareas=1;settings[i].firstreachablearea=i;reaches[i].traveltype=TRAVEL_WALK;reaches[i].traveltime=i==3?20:10;}
    settings[1].cluster=1;settings[1].clusterareanum=0;settings[2].cluster=-1;settings[3].cluster=-2;
    settings[4].cluster=2;settings[4].clusterareanum=0;settings[5].cluster=3;settings[5].clusterareanum=0;
    reaches[1].areanum=2;reaches[2].areanum=reaches[3].areanum=1;reaches[4].areanum=2;reaches[5].areanum=3;
    for(i=1;i<3;i++){portals[i].areanum=i+1;portals[i].frontcluster=1;portals[i].backcluster=i+1;portals[i].clusterareanum[0]=i;portals[i].clusterareanum[1]=1;}
    indexes[0]=indexes[2]=1;indexes[1]=indexes[3]=2;
    clusters[1].numareas=clusters[1].numreachabilityareas=3;clusters[1].numportals=2;
    for(i=2;i<4;i++){clusters[i].numareas=clusters[i].numreachabilityareas=2;clusters[i].numportals=1;clusters[i].firstportal=i;}
    links[0].areanum=2;links[0].linknum=2;links[0].next=links+1;links[1].areanum=3;links[1].linknum=3;
    links[2].areanum=1;links[2].linknum=1;links[2].next=links+3;links[3].areanum=4;links[3].linknum=4;
    links[4].areanum=5;links[4].linknum=5;
    reversed[1].numlinks=reversed[2].numlinks=2;reversed[1].first=links;reversed[2].first=links+2;
    reversed[3].numlinks=1;reversed[3].first=links+4;
}
static void AreaSuccess(void) {
    aas_routingcache_t *cache;int prior;AreaWorld();cache=AAS_GetAreaRoutingCache(1,1,-1);
    Check(cache&&cache->traveltimes[0]==1&&cache->traveltimes[1]==11,"literal native cold area-cache costs");
    Check(cache->size==(int)sizeof(*cache)+6&&routingcachesize==cache->size&&Outstanding()==1,"native variable cache layout/counter retained");
    Check(cache->reachabilities==(unsigned char *)cache+sizeof(*cache)+4&&cache->reachabilities[1]==0,"native byte-index offset retained");
    prior=requests;Check(AAS_GetAreaRoutingCache(1,1,-1)==cache&&requests==prior,"warm native cache does not allocate");Cleanup();
}
static void AreaFailure(int existing,int position) {
    aas_routingcache_t *previous=NULL;unsigned char before[512];int priorSize,priorOwners;AreaWorld();
    if(existing){previous=AAS_GetAreaRoutingCache(1,1,-1);Check(previous!=NULL,"initial native cache succeeds");memcpy(before,previous,previous->size);}
    priorSize=routingcachesize;priorOwners=Outstanding();failAt=requests+position;
    Check(AAS_GetAreaRoutingCache(1,1,existing?-2:-1)==NULL,"failed native area update must not publish a blank cache");
    Check(areaSlots[1][0]==previous&&routingcachesize==priorSize&&Outstanding()==priorOwners,"failed creation preserves table/counter/physical owners");
    Check(aasworld.oldestcache==previous&&aasworld.newestcache==previous,"failed unlinked cache cannot erase existing LRU owners");
    if(previous)Check(!memcmp(before,previous,previous->size),"existing native cache bytes/list links remain unchanged");
    failAt=0;Check(AAS_GetAreaRoutingCache(1,1,existing?-2:-1)!=NULL,"nullable failure can retry instead of reusing a blank cache");Cleanup();
}
static void PortalFailure(int position) {
    int i;PortalWorld();failAt=position;
    Check(AAS_GetPortalRoutingCache(1,1,-1)==NULL,"nested cache/workspace failure propagates without portal publication");
    Check(!portalCaches[1]&&!Outstanding()&&!routingcachesize&&!aasworld.oldestcache&&!aasworld.newestcache,"cold failed portal creation leaves no cache ownership");
    for(i=0;i<4;i++)Check(!portalUpdates[i].inlist,"cold failed portal update retains no queued entry");
    failAt=0;Check(AAS_GetPortalRoutingCache(1,1,-1)!=NULL,"nested nullable portal failure can retry");Cleanup();
}
static void PortalQueuedFailure(void) {
    aas_routingcache_t *cache;int i;PortalWorld();failAt=4;
    Check(AAS_GetPortalRoutingCache(1,1,-1)==NULL,"later nested failure aborts unpublished portal cache");
    Check(!portalCaches[1]&&areaSlots[1][0]&&Outstanding()==1&&routingcachesize==areaSlots[1][0]->size,"valid nested cache stays physically owned and failed outer cache releases");
    Check(aasworld.oldestcache==areaSlots[1][0]&&aasworld.newestcache==areaSlots[1][0],"valid nested cache stays in native LRU after abort");
    for(i=0;i<4;i++)Check(!portalUpdates[i].inlist,"abort clears all pending portal queue flags");
    failAt=0;cache=AAS_GetPortalRoutingCache(1,1,-1);
    Check(cache&&cache->traveltimes[1]==12&&cache->traveltimes[2]==22,"retry processes both portal sides with literal native costs");Cleanup();
}
static void RouteFailures(void) {
    int time=0,reach=0;vec3_t origin={0,0,0};aas_routingcache_t *cache;AreaWorld();failAt=1;
    Check(!AAS_AreaRouteToGoalArea(2,origin,1,-1,&time,&reach)&&!Outstanding()&&!routingcachesize,"within-cluster query handles nullable cache provider");Cleanup();
    PortalWorld();failAt=1;
    Check(!AAS_AreaRouteToGoalArea(4,origin,1,-1,&time,&reach)&&!Outstanding()&&!routingcachesize,"cross-cluster query handles nullable portal provider");Cleanup();
    PortalWorld();Check(AAS_GetPortalRoutingCache(1,1,-1)!=NULL,"initial portal cache succeeds");cache=areaSlots[2][1];Check(cache!=NULL,"native side cache created");
    areaSlots[2][1]=NULL;AAS_FreeRoutingCache(cache);failAt=requests+1;
    Check(!AAS_AreaRouteToGoalArea(4,origin,1,-1,&time,&reach)&&!areaSlots[2][1],"mixed query handles nullable side-cache provider");Cleanup();
}
int main(int argc,char **argv) {
    if(argc>1){int proof=atoi(argv[1]);if(proof==1)AreaFailure(0,2);else if(proof==2)AreaFailure(0,1);else PortalQueuedFailure();}
    else {int existing,position;AreaSuccess();for(existing=0;existing<2;existing++)for(position=1;position<3;position++)AreaFailure(existing,position);for(position=1;position<4;position++)PortalFailure(position);PortalQueuedFailure();RouteFailures();puts("Native AAS nullable cache/workspace propagation, physical ownership and retry passed (issue #47)");}
    return 0;
}
