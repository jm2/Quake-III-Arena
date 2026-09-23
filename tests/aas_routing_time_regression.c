/* Actual area/portal updates and route selection: literal costs and uint16 saturation. */
#include Q3_AAS_TIME_ROUTE_SOURCE
#include <stdint.h>
botlib_import_t botimport;
aas_t aasworld;
int botDeveloper;
static aas_cluster_t clusters[3];
static aas_areasettings_t settings[4];
static aas_area_t areas[4];
static aas_portal_t portals[2];
static aas_portalindex_t indexes[2];
static aas_reachability_t reaches[4];
static aas_reversedreachability_t reversed[4];
static aas_reversedlink_t links[3];
static aas_routingupdate_t areaUpdates[4],portalUpdates[3];
static int contentFlags[4],portalCosts[2];
static unsigned short crossingCost;
static unsigned short *areaCosts[1];
static unsigned short **times[4];
static unsigned short emptyCosts[2];
static unsigned short *emptyTimes[1];
static void *workspace;
static unsigned char areaBytes[sizeof(aas_routingcache_t)+64] __attribute__((aligned(32)));
static unsigned char portalBytes[sizeof(aas_routingcache_t)+64] __attribute__((aligned(32)));
static void Check(int condition,const char *message) {
    if(!condition){fprintf(stderr,"AAS routing-time regression failed: %s\n",message);exit(1);}
}
#ifndef Com_Memset
void Com_Memset(void *out,int value,size_t size) {memset(out,value,size);}
#endif
void *GetClearedMemory(unsigned long size) {Check(!workspace&&size>0&&size<=16,"bounded native update workspace");workspace=calloc(1,size);Check(workspace!=NULL,"fixture workspace allocation");return workspace;}
void FreeMemory(void *pointer) {if(!pointer)return;Check(pointer==workspace,"only native update workspace is released");free(pointer);workspace=NULL;}
int AvailableMemory(void) {return INT_MAX;}
int AAS_AreaDoNotEnter(int area) {Check(area>0&&area<4,"valid native query areas");return 0;}
int AAS_AreaCrouch(int area) {Check(area>0&&area<4,"valid native metric area");return 0;}
int AAS_AreaSwim(int area) {Check(area>0&&area<4,"valid native metric area");return 0;}
aas_routingcache_t *AAS_GetAreaRoutingCache(int cluster,int area,int flags) {
    (void)flags;Check(cluster>0&&cluster<3&&area>0&&area<4,"bounded fixture area cache");return (aas_routingcache_t *)areaBytes;
}
aas_routingcache_t *AAS_GetPortalRoutingCache(int cluster,int area,int flags) {
    (void)flags;Check(cluster>0&&cluster<3&&area>0&&area<4,"bounded fixture portal cache");return (aas_routingcache_t *)portalBytes;
}
static float Float(uint32_t bits) {union {float value;uint32_t bits;} r;volatile uint32_t input=bits;r.bits=input;return r.value;}
static void Reset(void) {
    int i;Check(workspace==NULL,"workspace released before next native case");memset(emptyCosts,0,sizeof(emptyCosts));emptyTimes[0]=emptyCosts;for(i=0;i<4;i++)times[i]=emptyTimes;
    memset(&aasworld,0,sizeof(aasworld));memset(clusters,0,sizeof(clusters));memset(settings,0,sizeof(settings));
    memset(areas,0,sizeof(areas));memset(portals,0,sizeof(portals));memset(indexes,0,sizeof(indexes));memset(reaches,0,sizeof(reaches));
    memset(reversed,0,sizeof(reversed));memset(links,0,sizeof(links));memset(areaUpdates,0,sizeof(areaUpdates));memset(portalUpdates,0,sizeof(portalUpdates));
    memset(areaBytes,0,sizeof(areaBytes));memset(portalBytes,0,sizeof(portalBytes));memset(contentFlags,0,sizeof(contentFlags));memset(portalCosts,0,sizeof(portalCosts));
    aasworld.numareas=aasworld.numareasettings=4;aasworld.numclusters=3;aasworld.numportals=2;aasworld.reachabilitysize=4;aasworld.portalindexsize=2;
    aasworld.areas=areas;aasworld.areasettings=settings;aasworld.clusters=clusters;aasworld.portals=portals;aasworld.portalindex=indexes;
    aasworld.reachability=reaches;aasworld.reversedreachability=reversed;aasworld.areaupdate=areaUpdates;aasworld.portalupdate=portalUpdates;
    aasworld.areacontentstravelflags=contentFlags;aasworld.areatraveltimes=times;aasworld.portalmaxtraveltimes=portalCosts;
    aasworld.loaded=aasworld.initialized=qtrue;AAS_InitTravelFlagFromType();
    ((aas_routingcache_t *)areaBytes)->reachabilities=areaBytes+sizeof(aas_routingcache_t)+16;
    ((aas_routingcache_t *)portalBytes)->reachabilities=portalBytes+sizeof(aas_routingcache_t)+16;
}
static aas_routingcache_t *AreaChain(float base,int first,int second,int crossing) {
    aas_routingcache_t *cache;int i;Reset();cache=(aas_routingcache_t *)areaBytes;
    aasworld.numclusters=2;aasworld.numportals=0;clusters[1].numareas=clusters[1].numreachabilityareas=3;
    for(i=1;i<4;i++){settings[i].cluster=1;settings[i].clusterareanum=i-1;settings[i].numreachableareas=1;settings[i].firstreachablearea=i==1?3:i-1;}
    reaches[1].areanum=1;reaches[1].traveltype=TRAVEL_WALK;reaches[1].traveltime=first;
    reaches[2].areanum=2;reaches[2].traveltype=TRAVEL_WALK;reaches[2].traveltime=second;
    reaches[3].areanum=1;reaches[3].traveltype=TRAVEL_WALK;reaches[3].traveltime=1;
    links[0].areanum=2;links[0].linknum=1;links[0].next=links+2;
    links[1].areanum=3;links[1].linknum=2;
    links[2].areanum=1;links[2].linknum=3;
    reversed[1].numlinks=2;reversed[1].first=links;reversed[2].numlinks=1;reversed[2].first=links+1;
    crossingCost=crossing;areaCosts[0]=&crossingCost;times[2]=areaCosts;
    cache->cluster=1;cache->areanum=1;cache->travelflags=-1;cache->starttraveltime=base;
    return cache;
}
static void AreaSums(void) {
    const struct {float base;int first,second,crossing;unsigned short one,two;} cases[]={
        {1,10,20,5,11,36},{7,10,20,0,17,37},{32768,10,20,5,32778,32803},
        {1,65500,100,10,65501,65535},{65534,1,1,0,65535,65535},
        {1,10,20,65535,11,65535}
    };size_t i;
    for(i=0;i<sizeof(cases)/sizeof(cases[0]);i++) {
        aas_routingcache_t *cache=AreaChain(cases[i].base,cases[i].first,cases[i].second,cases[i].crossing);
        AAS_UpdateAreaRoutingCache(cache);
        Check(cache->traveltimes[1]==cases[i].one&&cache->traveltimes[2]==cases[i].two,"native multi-area sums retain literal normal costs and saturate overflow");
    }
    {const uint32_t starts[]={0x7fc00000u,0xffc00000u,0x7f800000u,0xff800000u,0x7f800001u,0xff800001u,0x47800000u,0x7f7fffffu};
     for(i=0;i<sizeof(starts)/sizeof(starts[0]);i++) {
        aas_routingcache_t *cache=AreaChain(Float(starts[i]),10,20,5);AAS_UpdateAreaRoutingCache(cache);
        Check(cache->traveltimes[0]==65535&&cache->traveltimes[1]==65535&&cache->traveltimes[2]==65535,"nonfinite and oversized cache starts saturate before uint16 conversion");
     }}
}
static void PortalWorld(void) {
    int i;Reset();
    for(i=1;i<4;i++){settings[i].numreachableareas=1;settings[i].firstreachablearea=i;settings[i].presencetype=PRESENCE_NORMAL;reaches[i].traveltype=TRAVEL_WALK;}
    settings[1].cluster=1;settings[1].clusterareanum=0;settings[2].cluster=-1;settings[3].cluster=2;settings[3].clusterareanum=0;
    reaches[1].areanum=2;reaches[2].areanum=3;reaches[3].areanum=2;
    portals[1].areanum=2;portals[1].frontcluster=1;portals[1].backcluster=2;portals[1].clusterareanum[0]=portals[1].clusterareanum[1]=1;
    indexes[0]=indexes[1]=1;
    clusters[1].numareas=clusters[1].numreachabilityareas=clusters[2].numareas=clusters[2].numreachabilityareas=2;
    clusters[1].firstportal=0;clusters[2].firstportal=1;clusters[1].numportals=clusters[2].numportals=1;
}
static void PortalSums(void) {
    const struct {float base;unsigned short area;int crossing;unsigned short total,next;} cases[]={
        {1,200,50,201,251},{200,65400,100,65535,65535},{1,65535,1,65535,65535}
    };size_t i;
    for(i=0;i<sizeof(cases)/sizeof(cases[0]);i++) {
        aas_routingcache_t *cache;PortalWorld();cache=(aas_routingcache_t *)portalBytes;
        cache->cluster=1;cache->areanum=1;cache->starttraveltime=cases[i].base;cache->travelflags=-1;
        ((aas_routingcache_t *)areaBytes)->traveltimes[1]=cases[i].area;portalCosts[1]=cases[i].crossing;
        AAS_UpdatePortalRoutingCache(cache);
        Check(cache->traveltimes[1]==cases[i].total&&portalUpdates[1].tmptraveltime==cases[i].next,"native portal/cache crossing sums saturate without cheap wrap");
    }
}
static void RouteSums(void) {
    const struct {unsigned short area,portal;int crossing,time;} cases[]={{7,5,9,22},{200,65400,50,65535},{65535,1,1,65535}};size_t i;
    for(i=0;i<sizeof(cases)/sizeof(cases[0]);i++) {
        vec3_t origin={0,0,0};int time=0,reach=0;PortalWorld();
        ((aas_routingcache_t *)areaBytes)->traveltimes[0]=cases[i].area;((aas_routingcache_t *)portalBytes)->traveltimes[1]=cases[i].portal;portalCosts[1]=cases[i].crossing;
        Check(AAS_AreaRouteToGoalArea(1,origin,3,-1,&time,&reach)&&time==cases[i].time&&reach==1,"native route selection retains normal total and cannot prefer overflowed penalties");
    }
}
static void HideSums(void) {
    const struct {unsigned short reach;float end,enemy;unsigned short total;} cases[]={
        {10,10,10000,111},{65500,10,10000,65535},{10,10000,20000,65535}
    };size_t i;Reset();
    settings[1].numreachableareas=1;settings[1].firstreachablearea=1;
    reaches[1].areanum=2;reaches[1].traveltype=TRAVEL_WALK;
    for(i=0;i<sizeof(cases)/sizeof(cases[0]);i++) {
        vec3_t origin={0,0,0},enemy={cases[i].enemy,0,0};
        memset(areaUpdates,0,sizeof(areaUpdates));reaches[1].traveltime=cases[i].reach;reaches[1].end[0]=cases[i].end;
        Check(AAS_NearestHideArea(0,origin,1,0,enemy,3,-1)==2&&areaUpdates[2].tmptraveltime==cases[i].total,"native hide-area distance penalties and path costs saturate without wrapping");
    }
    FreeMemory(workspace);
}
int main(int argc,char **argv) {
    if(argc>1){aas_routingcache_t *cache=AreaChain(1,10,20,65535);AAS_UpdateAreaRoutingCache(cache);Check(cache->traveltimes[2]==65535,"original maximum area penalty wrapped cheap");}
    else {AreaSums();PortalSums();RouteSums();HideSums();puts("Native AAS area/portal/route uint16 accumulation checks passed (issue #47)");}
    return 0;
}
