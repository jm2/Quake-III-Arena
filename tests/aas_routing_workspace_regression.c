/* Actual routing cache update: incoming degree, native costs and temporary ownership. */
#include Q3_AAS_ROUTE_SOURCE
botlib_import_t botimport;
aas_t aasworld;
static aas_cluster_t clusters[2];
static aas_areasettings_t settings[1026];
static aas_reachability_t reachability[1026];
static aas_reversedreachability_t reversed[1026];
static aas_reversedlink_t links[1024];
static aas_routingupdate_t updates[1026];
static aas_reversedlink_t goalLink;
static int contents[1026], requests, releases, failRequest;
static unsigned long lastRequest;
static void *workspace;
static unsigned short goalCosts[1024], sourceCost;
static unsigned short *areaCosts[1026][1];
static unsigned short **times[1026];
static unsigned char cacheBytes[sizeof(aas_routingcache_t)+4096] __attribute__((aligned(32)));
static void Check(int condition,const char *message) {
    if(!condition){fprintf(stderr,"AAS routing workspace regression failed: %s\n",message);exit(1);}
}
void *GetClearedMemory(unsigned long size) {
    requests++;lastRequest=size;
    Check(!workspace&&size>0&&size<=2048,"bounded fixture workspace request");
    if(failRequest)return NULL;
    workspace=calloc(1,size);Check(workspace!=NULL,"exact fixture workspace allocation");return workspace;
}
void FreeMemory(void *memory) {
    if(!memory)return;
    Check(memory==workspace,"known temporary owner released once");
    free(workspace);workspace=NULL;releases++;
}
#ifndef Com_Memset
void Com_Memset(void *out,int value,size_t size) {memset(out,value,size);}
#endif
static aas_routingcache_t *Reset(int incoming) {
    aas_routingcache_t *cache=(aas_routingcache_t *)cacheBytes;int i;
    Check(!workspace,"previous routing workspace physically released");
    memset(&aasworld,0,sizeof(aasworld));memset(clusters,0,sizeof(clusters));memset(settings,0,sizeof(settings));
    memset(reachability,0,sizeof(reachability));memset(reversed,0,sizeof(reversed));memset(links,0,sizeof(links));
    memset(updates,0,sizeof(updates));memset(cacheBytes,0,sizeof(cacheBytes));memset(goalCosts,0,sizeof(goalCosts));sourceCost=0;
    memset(areaCosts,0,sizeof(areaCosts));memset(times,0,sizeof(times));memset(&goalLink,0,sizeof(goalLink));
    requests=releases=failRequest=0;lastRequest=0;
    aasworld.numareas=aasworld.numareasettings=incoming?incoming+2:3;aasworld.numclusters=2;aasworld.reachabilitysize=incoming+2;
    aasworld.clusters=clusters;aasworld.areasettings=settings;aasworld.reachability=reachability;
    aasworld.reversedreachability=reversed;aasworld.areaupdate=updates;aasworld.areacontentstravelflags=contents;aasworld.areatraveltimes=times;
    clusters[1].numareas=aasworld.numareas-1;clusters[1].numreachabilityareas=incoming+1;
    settings[1].cluster=1;settings[1].clusterareanum=0;settings[1].numreachableareas=1;settings[1].firstreachablearea=incoming+1;
    reachability[incoming+1].areanum=2;reachability[incoming+1].traveltype=TRAVEL_WALK;reachability[incoming+1].traveltime=10;
    reversed[1].numlinks=incoming;reversed[1].first=incoming?links:NULL;
    for(i=0;i<incoming;i++) {
        reachability[i+1].areanum=1;reachability[i+1].traveltype=TRAVEL_WALK;reachability[i+1].traveltime=10;
        settings[i+2].cluster=1;settings[i+2].clusterareanum=i+1;settings[i+2].numreachableareas=1;settings[i+2].firstreachablearea=i+1;
        links[i].areanum=i+2;links[i].linknum=i+1;links[i].next=i+1<incoming?links+i+1:NULL;
        times[i+2]=areaCosts[i+2];
    }
    if(!incoming){settings[2].cluster=1;settings[2].clusterareanum=1;}
    goalLink.areanum=1;goalLink.linknum=incoming+1;reversed[2].numlinks=1;reversed[2].first=&goalLink;
    areaCosts[2][0]=&sourceCost;times[2]=areaCosts[2];
    areaCosts[1][0]=goalCosts;times[1]=areaCosts[1];AAS_InitTravelFlagFromType();
    cache->cluster=1;cache->areanum=1;cache->travelflags=-1;cache->starttraveltime=1;
    cache->reachabilities=cacheBytes+sizeof(aas_routingcache_t)+(incoming+1)*sizeof(unsigned short);
    return cache;
}
static void Successes(void) {
    const int degrees[]={0,1,2,32,127,128,129,255,256,512,1024};size_t i;int blocked,source;
    for(i=0;i<sizeof(degrees)/sizeof(degrees[0]);i++)for(blocked=0;blocked<2;blocked++) {
        aas_routingcache_t *cache=Reset(degrees[i]);
        if(blocked)settings[1].areaflags|=AREA_DISABLED;
        AAS_UpdateAreaRoutingCache(cache);
        Check(cache->traveltimes[0]==1,"literal native destination travel cost");
        for(source=1;source<=degrees[i];source++) {
            Check(cache->traveltimes[source]==(blocked?0:11),"literal source travel costs and disabled-area filtering");
            Check(cache->reachabilities[source]==0,"one outgoing reachability per source retains cache byte index");
        }
        Check(requests==(degrees[i]>0)&&releases==requests&&!workspace,"workspace lifetime matches incoming count");
        Check(lastRequest==(unsigned long)degrees[i]*sizeof(unsigned short),"exact incoming-degree workspace length");
        Check(updates[0].areatraveltimes==NULL,"start entry retains no released temporary pointer");
        Check(aasworld.frameroutingupdates==1,"one native routing attempt");
    }
}
static void Failures(void) {
    const int counts[]={INT_MIN,-1,INT_MAX,INT_MAX/2+1};size_t i;
    for(i=0;i<sizeof(counts)/sizeof(counts[0])+1;i++) {
        aas_routingcache_t *cache=Reset(129);unsigned char before[sizeof(cacheBytes)];aas_routingupdate_t prior[1026];
        if(i<sizeof(counts)/sizeof(counts[0]))reversed[1].numlinks=counts[i];else failRequest=1;
        memcpy(before,cacheBytes,sizeof(before));memcpy(prior,updates,sizeof(prior));
        AAS_UpdateAreaRoutingCache(cache);
        Check(!memcmp(before,cacheBytes,sizeof(before))&&!memcmp(prior,updates,sizeof(prior)),"invalid/nullable workspace leaves cache and update owners unchanged");
        Check(requests==(i==sizeof(counts)/sizeof(counts[0]))&&!releases&&!workspace,"failure has no temporary owner or import for unrepresentable count");
    }
}
int main(int argc,char **argv) {
    if(argc>1){aas_routingcache_t *cache=Reset(129);AAS_UpdateAreaRoutingCache(cache);}
    else {Successes();Failures();puts("Native AAS routing start workspace and degree/filter/ownership checks passed (issue #47)");}
    return 0;
}
