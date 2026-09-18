/* Actual initialization pipeline and continuation: derived costs, ownership and publication. */
#include Q3_AAS_ROUTE_SOURCE
libvar_t *saveroutingcache;
#include Q3_AAS_INIT_CONTINUATION
botlib_import_t botimport;
aas_t aasworld;
int bot_developer;
static aas_areasettings_t settings[3];
static aas_area_t areas[3];
static aas_cluster_t clusters[2];
static aas_portal_t portals[1];
static aas_reachability_t reaches[3];
static int requests,releases,failAt,cacheReads,initializedMessages,errors;
static unsigned long lastRequest;
static libvar_t saveVariable;
static int pendingReachability,cacheWrites,cacheCloses,saveResets,unlinks,invalidations;
static struct {void *pointer;unsigned long size;} owners[32];
static void Check(int condition,const char *message) {
    if(!condition){fprintf(stderr,"AAS routing init regression failed: %s\n",message);exit(1);}
}
#ifndef Com_Memset
void Com_Memset(void *out,int value,size_t size) {memset(out,value,size);}
#endif
void *GetClearedMemory(unsigned long size) {
    requests++;lastRequest=size;
    if(requests==failAt||size>2*1024*1024)return NULL;
    Check(size>0&&size<=INT_MAX&&requests<=32,"positive representable native table allocation");
    owners[requests-1].pointer=calloc(1,size);owners[requests-1].size=size;
    Check(owners[requests-1].pointer!=NULL,"exact fixture table allocation");return owners[requests-1].pointer;
}
void *GetMemory(unsigned long size) {(void)size;Check(0,"route cache file is absent in this initialization fixture");return NULL;}
void FreeMemory(void *pointer) {
    int i;if(!pointer)return;
    for(i=0;i<requests&&i<32;i++)if(owners[i].pointer==pointer){free(pointer);owners[i].pointer=NULL;releases++;return;}
    Check(0,"unknown/double routing table release");
}
static int Outstanding(void) {int i,n=0;for(i=0;i<requests&&i<32;i++)if(owners[i].pointer)n++;return n;}
static int Open(const char *name,fileHandle_t *file,fsMode_t mode) {
    Check(!strcmp(name,"maps/synthetic.rcd"),"bounded native cache filename");
    if(mode==FS_WRITE){cacheWrites++;*file=3;return 0;}
    Check(mode==FS_READ,"bounded native cache lookup");cacheReads++;*file=0;return -1;
}
static int Write(const void *data,int size,fileHandle_t file) {
    routecacheheader_t header;Check(file==3&&size==(int)sizeof(header),"actual empty native cache save header");memcpy(&header,data,sizeof(header));
    Check(header.version==2&&!header.numportalcache&&!header.numareacache&&header.numareas==aasworld.numareas&&header.numclusters==aasworld.numclusters,"valid initialized frame saves native cache format");return size;
}
static void Close(fileHandle_t file) {Check(file==3,"native cache save handle");cacheCloses++;}
static void QDECL Print(int type,char *format,...) {(void)format;if(type==PRT_ERROR)errors++;if(!strcmp(format,"AAS initialized.\n"))initializedMessages++;}
void QDECL AAS_Error(char *format,...) {(void)format;Check(0,"unexpected native cache error");}
int AAS_ContinueInitReachability(float time) {(void)time;return pendingReachability;}
void AAS_InitClustering(void) {Check(aasworld.numclusters>=1,"native completed/dummy clustering skips rebuilding with unset force flags");}
float LibVarGetValue(char *name) {Check(!strcmp(name,"forcewrite"),"native continuation force variable");return 0;}
float LibVarValue(char *name,char *value) {Check(!strcmp(name,"max_routingcache")&&!strcmp(value,"4096"),"native cache limit default");return 4096;}
void AAS_Optimize(void) {Check(0,"native optimization not forced");}
qboolean AAS_WriteAASFile(char *name) {(void)name;Check(0,"native writer not forced");return 0;}
int Sys_MilliSeconds(void) {return 7;}
float AAS_Time(void) {return 7;}
void QDECL Com_Printf(const char *format,...) {(void)format;Check(0,"unexpected native formatting truncation");}
int AvailableMemory(void) {return INT_MAX;}
void AAS_UnlinkInvalidEntities(void) {unlinks++;}
void AAS_InvalidateEntities(void) {invalidations++;}
void PrintUsedMemorySize(void) {Check(0,"developer memory diagnostics disabled in fixture");}
void PrintMemoryLabels(void) {Check(0,"developer memory diagnostics disabled in fixture");}
void LibVarSet(char *name,char *value) {Check(!strcmp(name,"saveroutingcache")&&!strcmp(value,"0"),"only successful native save resets the request");saveVariable.value=0;saveResets++;}
int AAS_AreaDoNotEnter(int area) {Check(area>0&&area<aasworld.numareas,"bounded native route area");return 0;}
int AAS_AreaCrouch(int area) {Check(area>0&&area<aasworld.numareas,"bounded native metric area");return 0;}
int AAS_AreaSwim(int area) {Check(area>0&&area<aasworld.numareas,"bounded native metric area");return 0;}
int AAS_TraceAreas(vec3_t start,vec3_t end,int *out,vec3_t *points,int capacity) {(void)start;(void)end;(void)out;(void)points;(void)capacity;Check(0,"walk reaches have no pass-area trace");return 0;}
static void Reset(int empty) {
    int i;Check(!Outstanding()&&!routingcachesize,"previous routing owners released");
    memset(&aasworld,0,sizeof(aasworld));memset(settings,0,sizeof(settings));memset(areas,0,sizeof(areas));memset(clusters,0,sizeof(clusters));memset(portals,0,sizeof(portals));memset(reaches,0,sizeof(reaches));memset(owners,0,sizeof(owners));
    requests=releases=failAt=cacheReads=initializedMessages=errors=0;lastRequest=0;
    memset(&saveVariable,0,sizeof(saveVariable));saveroutingcache=&saveVariable;
    pendingReachability=cacheWrites=cacheCloses=saveResets=unlinks=invalidations=0;bot_developer=0;
    aasworld.numareas=aasworld.numareasettings=3;aasworld.numclusters=empty?1:2;aasworld.numportals=empty?0:1;aasworld.reachabilitysize=empty?0:3;
    aasworld.areasettings=settings;aasworld.areas=areas;aasworld.clusters=clusters;aasworld.portals=portals;aasworld.reachability=empty?NULL:reaches;
    strcpy(aasworld.mapname,"synthetic");aasworld.loaded=qtrue;
    if(!empty){clusters[1].numareas=clusters[1].numreachabilityareas=2;for(i=1;i<3;i++){settings[i].cluster=1;settings[i].clusterareanum=i-1;settings[i].numreachableareas=1;settings[i].firstreachablearea=i;settings[i].presencetype=PRESENCE_NORMAL;reaches[i].areanum=3-i;reaches[i].traveltype=TRAVEL_WALK;reaches[i].traveltime=10;}}
    botimport.Print=Print;botimport.FS_FOpenFile=Open;botimport.FS_Write=Write;botimport.FS_FCloseFile=Close;
}
static void EmptyFields(void) {
    Check(!aasworld.areacontentstravelflags&&!aasworld.areaupdate&&!aasworld.portalupdate&&!aasworld.reversedreachability&&!aasworld.clusterareacache&&!aasworld.portalcache&&!aasworld.areatraveltimes&&!aasworld.portalmaxtraveltimes&&!aasworld.reachabilityareas&&!aasworld.reachabilityareaindex,"failed pipeline clears every partial routing table owner");
}
static void Success(void) {
    int time=0,reach=0;vec3_t origin={0,0,0};Reset(0);AAS_ContinueInit(7);
    Check(aasworld.initialized&&aasworld.loaded&&initializedMessages==1&&!errors&&cacheReads==1&&requests==10&&Outstanding()==10,"complete native pipeline publishes once with ten physical table owners");
    Check(aasworld.reversedreachability[1].numlinks==1&&aasworld.reversedreachability[1].first->areanum==2&&aasworld.reversedreachability[1].first->linknum==2,"literal native reversed reach ownership");
    Check(aasworld.areatraveltimes[1][0][0]==1&&aasworld.areatraveltimes[2][0][0]==1&&!aasworld.portalmaxtraveltimes[0],"literal native travel matrix and dummy portal cost");
    Check(aasworld.areacontentstravelflags[1]==TFL_AIR&&!aasworld.reachabilityareas[1].numareas&&!aasworld.reachabilityareas[2].numareas,"native contents and walk pass-area classifications");
    Check(AAS_AreaRouteToGoalArea(2,origin,1,-1,&time,&reach)&&time==12&&reach==2,"native initialized-world route result retained");
    AAS_FreeRoutingCaches();EmptyFields();Check(!Outstanding()&&!routingcachesize,"native successful tables/cache physically free");
}
static void Nullable(int position) {
    int prior;Reset(0);failAt=position;AAS_ContinueInit(7);
    Check(!aasworld.initialized&&!aasworld.loaded&&!initializedMessages&&errors==1&&!cacheReads,"nullable stage cannot publish an initialized world");
    EmptyFields();Check(!Outstanding()&&!routingcachesize&&requests==position,"every earlier partial table physically releases");
    prior=requests;AAS_ContinueInit(8);Check(requests==prior,"failed world does not repeat allocations every frame");
}
static void FrameFailure(int position) {
    int prior;Reset(0);failAt=position;saveVariable.value=1;
    Check(AAS_StartFrame(7)==BLERR_NOERROR,"native frame return remains compatible");
    Check(!aasworld.initialized&&!aasworld.loaded&&!cacheWrites&&!cacheCloses&&!saveResets&&saveVariable.value==1&&errors==1,"failed frame cannot serialize freed routing tables");
    EmptyFields();Check(!Outstanding()&&!routingcachesize&&requests==position&&unlinks==1&&invalidations==1,"failed frame releases all partial owners");
    prior=requests;Check(AAS_StartFrame(8)==BLERR_NOERROR&&requests==prior&&!cacheWrites&&!saveResets&&unlinks==2&&invalidations==2,"later failed-world frame neither retries nor writes cache");
}
static void FramePending(void) {
    Reset(0);pendingReachability=1;saveVariable.value=1;
    Check(AAS_StartFrame(7)==BLERR_NOERROR&&!aasworld.initialized&&aasworld.loaded&&!requests&&!cacheWrites&&!saveResets&&saveVariable.value==1,"pending native reachability keeps save request without using absent tables");
    pendingReachability=0;
    Check(AAS_StartFrame(8)==BLERR_NOERROR&&aasworld.initialized&&requests==10&&cacheReads==1&&cacheWrites==1&&cacheCloses==1&&saveResets==1&&!saveVariable.value,"successful later frame initializes and performs pending native save");
    Check(aasworld.numframes==2&&unlinks==2&&invalidations==2,"native frame bookkeeping retained");AAS_FreeRoutingCaches();EmptyFields();Check(!Outstanding()&&!routingcachesize,"successful native frame owners release");
}
static void EmptyDummy(void) {
    Reset(1);AAS_ContinueInit(7);Check(aasworld.initialized&&aasworld.loaded&&requests==6&&Outstanding()==6,"native empty dummy world handles zero-size arrays without imports");
    Check(!aasworld.areaupdate&&!aasworld.portalmaxtraveltimes&&!aasworld.reachabilityareas&&!aasworld.reachabilityareaindex,"zero-capacity table owners remain null");
    AAS_FreeRoutingCaches();EmptyFields();Check(!Outstanding(),"empty dummy table owners release physically");
}
static void MatrixOverflow(void) {
    const int count=65536;int i;
    aas_areasettings_t *largeSettings=calloc(count+2,sizeof(*largeSettings));
    aas_reachability_t *largeReaches=calloc(2*count+1,sizeof(*largeReaches));
    aas_reversedreachability_t *largeReverse=calloc(count+2,sizeof(*largeReverse));
    aas_reversedlink_t *largeLinks=calloc(count+128,sizeof(*largeLinks));
    Check(largeSettings&&largeReaches&&largeReverse&&largeLinks,"bounded large-graph fixture inputs");Reset(0);
    aasworld.numareas=aasworld.numareasettings=count+2;aasworld.reachabilitysize=2*count+1;aasworld.areasettings=largeSettings;aasworld.reachability=largeReaches;aasworld.reversedreachability=largeReverse;
    largeSettings[1].numreachableareas=count;largeSettings[1].firstreachablearea=1;largeReverse[1].numlinks=count;
    for(i=0;i<count;i++) {
        largeReaches[i+1].areanum=i+2;largeReaches[i+1].traveltype=TRAVEL_WALK;
        largeSettings[i+2].numreachableareas=1;largeSettings[i+2].firstreachablearea=count+i+1;
        largeReaches[count+i+1].areanum=1;largeReaches[count+i+1].traveltype=TRAVEL_WALK;
        largeLinks[i].areanum=i+2;largeLinks[i].linknum=count+i+1;largeLinks[i].next=i?largeLinks+i-1:NULL;
        if(i<128){largeLinks[count+i].areanum=1;largeLinks[count+i].linknum=i+1;largeReverse[i+2].numlinks=1;largeReverse[i+2].first=largeLinks+count+i;}
    }
    largeReverse[1].first=largeLinks+count-1;
    AAS_CalculateAreaTravelTimes();Check(!requests&&!aasworld.areatraveltimes,"fully backed graph's eight-gigabyte matrix rejects before import/wrap");
    free(largeLinks);free(largeReverse);free(largeReaches);free(largeSettings);memset(&aasworld,0,sizeof(aasworld));
}
static void ShapeCosts(void) {
    Reset(0);aasworld.numareas=aasworld.numareasettings=INT_MAX;AAS_ContinueInit(7);Check(!requests&&!Outstanding()&&!aasworld.initialized&&!aasworld.loaded,"unrepresentable area flags reject before lookup/import");
    Reset(0);clusters[1].numreachabilityareas=INT_MAX;AAS_InitRoutingUpdate();Check(!requests&&!aasworld.areaupdate&&!aasworld.portalupdate,"unrepresentable update cost rejects before import");
    Reset(0);aasworld.numportals=INT_MAX;AAS_InitRoutingUpdate();Check(!requests&&!aasworld.portalupdate,"portal update plus-one rejects before signed overflow");
    Reset(0);aasworld.reachabilitysize=INT_MAX;AAS_CreateReversedReachability();Check(!requests&&!aasworld.reversedreachability,"unrepresentable reversed-link sum rejects before import");
    Reset(0);clusters[1].numareas=INT_MAX;AAS_InitClusterAreaCache();Check(!requests&&!aasworld.clusterareacache,"unrepresentable cluster slot cost rejects before import");
    Reset(0);aasworld.reachabilitysize=INT_MAX/32+1;AAS_InitReachabilityAreas();Check(!requests&&!aasworld.reachabilityareas&&!aasworld.reachabilityareaindex,"pass-area flat product rejects before either allocation");
    MatrixOverflow();
}
int main(int argc,char **argv) {
    int i;if(argc>1){int proof=atoi(argv[1]);if(proof==0)Nullable(1);else if(proof==1){Reset(0);aasworld.numareas=aasworld.numareasettings=INT_MAX;AAS_ContinueInit(7);}else if(proof==3)FrameFailure(1);else MatrixOverflow();}
    else {Success();for(i=1;i<=10;i++){Nullable(i);FrameFailure(i);}FramePending();EmptyDummy();ShapeCosts();puts("Native AAS routing initialization costs, nullable stages and publication passed (issue #47)");}
    return 0;
}
