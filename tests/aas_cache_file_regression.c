/* Native writer/reader round trips, untrusted dump bytes and loaded-cache ownership. */
#include <stddef.h>
#include <stdint.h>
#define main RoutingFailureFixtureMain
#include "aas_routing_failure_regression.c"
#undef main
static unsigned char dump[8192],input[8192];
static int dumpLength,inputLength,cursor,opens,closes,readCalls,shortReadCall;
static int expectedBytes;
void *GetMemory(unsigned long size) {return GetClearedMemory(size);}
void QDECL Com_Printf(const char *format,...) {(void)format;Check(0,"unexpected native formatting truncation");}
void QDECL AAS_Error(char *format,...) {(void)format;Check(0,"optional cache errors are bounded rejection");}
static void QDECL Print(int type,char *format,...) {(void)type;(void)format;}
static int Open(const char *name,fileHandle_t *file,fsMode_t mode) {
    Check(!strcmp(name,"maps/synthetic.rcd"),"native dump filename");opens++;*file=3;
    if(mode==FS_WRITE){dumpLength=0;return 0;}Check(mode==FS_READ,"native dump mode");cursor=0;return inputLength;
}
static int Read(void *out,int size,fileHandle_t file) {
    int count;Check(file==3&&size>=0,"bounded native file read");readCalls++;
    count=size;if(count>inputLength-cursor)count=inputLength-cursor;
    if(readCalls==shortReadCall&&count)count--;
    if(count)memcpy(out,input+cursor,count);cursor+=count;return count;
}
static int Write(const void *data,int size,fileHandle_t file) {
    Check(file==3&&size>=0&&size<=(int)sizeof(dump)-dumpLength,"bounded native writer bytes");
    memcpy(dump+dumpLength,data,size);dumpLength+=size;return size;
}
static void Close(fileHandle_t file) {Check(file==3&&opens==closes+1,"every opened cache handle closes once");closes++;}
static void FileWorld(void) {
    PortalWorld();strcpy(aasworld.mapname,"synthetic");
    botimport.FS_FOpenFile=Open;botimport.FS_Read=Read;botimport.FS_Write=Write;botimport.FS_FCloseFile=Close;botimport.Print=Print;
    cursor=opens=closes=readCalls=shortReadCall=0;
}
static void OriginalInput(void) {memcpy(input,dump,dumpLength);inputLength=dumpLength;}
static int PhysicalBytes(void) {int i,sum=0;for(i=0;i<requests;i++)if(owners[i].pointer)sum+=(int)owners[i].size;return sum;}
static void CheckOwnership(void) {
    aas_routingcache_t *cache,*previous=NULL;int count=0;
    for(cache=aasworld.oldestcache;cache;cache=cache->time_next){Check(++count<=32&&cache->time_prev==previous,"loaded cache LRU has native links only");previous=cache;}
    Check(previous==aasworld.newestcache&&count==Outstanding()&&routingcachesize==PhysicalBytes(),"loaded counter/LRU includes every physical cache owner");
}
static void Route(void) {
    int time=0,reach=0;vec3_t origin={0,0,0};
    Check(AAS_AreaRouteToGoalArea(4,origin,1,-1,&time,&reach)&&time==25&&reach==4,"literal native cross-cluster route survives dump loading/regeneration");
}
static void Generate(void) {
    routecacheheader_t header;FileWorld();Check(AAS_GetPortalRoutingCache(1,1,-1)!=NULL,"native source cache graph");Route();
    expectedBytes=routingcachesize;AAS_WriteRouteCache();Check(opens==1&&closes==1,"actual writer closes native dump");
    memcpy(&header,dump,sizeof(header));Check(header.version==2&&header.numportalcache==1&&header.numareacache==3&&dumpLength==(int)sizeof(header)+expectedBytes,"actual native version-two dump size/order");
    Cleanup();
}
static void ClearPointers(unsigned char *buffer,int length) {
    aas_routingcache_t header;int offset;
    for(offset=sizeof(routecacheheader_t);offset<length;offset+=header.size){
        memcpy(&header,buffer+offset,sizeof(header));header.prev=header.next=header.time_prev=header.time_next=NULL;header.reachabilities=NULL;memcpy(buffer+offset,&header,sizeof(header));
    }
}
static void SameDump(void) {
    /* Loaded native caches keep the written bytes: writing them again changes only runtime pointers. */
    static unsigned char original[8192];int length=dumpLength;
    memcpy(original,dump,length);AAS_WriteRouteCache();Check(dumpLength==length&&inputLength==length,"loaded dump writes back at its native size");
    ClearPointers(dump,length);ClearPointers(input,length);Check(!memcmp(dump,input,length),"loaded native caches keep every written byte");memcpy(dump,original,length);
}
static void RoundTrip(int poison) {
    int offset,i;aas_routingcache_t header;FileWorld();OriginalInput();
    if(poison)for(offset=sizeof(routecacheheader_t);offset<inputLength;offset+=header.size){
        memcpy(&header,input+offset,sizeof(header));header.prev=header.next=header.time_prev=header.time_next=(aas_routingcache_t *)(uintptr_t)UINTPTR_MAX;
        header.reachabilities=(unsigned char *)(uintptr_t)UINTPTR_MAX;memcpy(input+offset,&header,sizeof(header));
    }
    Check(AAS_ReadRouteCache()&&opens==1&&closes==1,"actual writer dump loads and closes");SameDump();
    Check(routingcachesize==expectedBytes&&Outstanding()==4,"all loaded caches enter byte accounting");CheckOwnership();
    for(i=0;i<4;i++)Check(owners[i].pointer!=NULL,"loaded native cache owners");
    Route();CheckOwnership();AAS_EnableRoutingArea(4,qfalse);CheckOwnership();
    Check(routingcachesize>=0,"state change cannot make loaded-cache accounting negative");
    AAS_EnableRoutingArea(4,qtrue);Route();CheckOwnership();Cleanup();
}
static void Rejected(int existing,int failure) {
    aas_routingcache_t *previous=NULL;unsigned char before[512];int priorBytes,priorOwners;
    FileWorld();if(existing){previous=AAS_GetAreaRoutingCache(1,1,-2);Check(previous!=NULL,"preexisting valid cache");memcpy(before,previous,previous->size);}
    priorBytes=routingcachesize;priorOwners=Outstanding();if(failure)failAt=requests+failure;
    { static int caseNumber; int accepted=AAS_ReadRouteCache();caseNumber++;
      if(accepted||opens!=1||closes!=1)fprintf(stderr,"Cache file rejection case %d: bytes=%d existing=%d failure=%d accepted=%d opens=%d closes=%d\n",caseNumber,inputLength,existing,failure,accepted,opens,closes);
      Check(!accepted&&opens==1&&closes==1,"malformed/nullable dump rejects and closes"); }
    Check(routingcachesize==priorBytes&&Outstanding()==priorOwners&&aasworld.oldestcache==previous&&aasworld.newestcache==previous&&!portalCaches[1],"failed dump publishes no entries and restores physical/counter/LRU ownership");
    if(previous)Check(areaSlots[1][0]==previous&&!memcmp(before,previous,previous->size),"preexisting native cache bytes remain unchanged");
    Cleanup();
}
static void BadHeader(int field,int value) {
    routecacheheader_t header;OriginalInput();memcpy(&header,input,sizeof(header));
    switch(field){case 0:header.ident=value;break;case 1:header.version=value;break;case 2:header.numareas=value;break;case 3:header.numclusters=value;break;case 4:header.areacrc=value;break;case 5:header.clustercrc=value;break;case 6:header.numportalcache=value;break;default:header.numareacache=value;}
    memcpy(input,&header,sizeof(header));Rejected(1,0);
}
static void BadRecord(int field,int value,int late) {
    aas_routingcache_t header;int offset=sizeof(routecacheheader_t),next;
    if(late){while(1){memcpy(&header,dump+offset,sizeof(header));next=offset+header.size;if(next==dumpLength)break;offset=next;}}
    OriginalInput();memcpy(&header,input+offset,sizeof(header));
    switch(field){case 0:header.size=value;break;case 1:header.cluster=value;break;case 2:header.areanum=value;break;default:header.type=value;}
    memcpy(input+offset,&header,sizeof(header));Rejected(1,0);
}
static void BadFloat(int field,unsigned int bits,int late) {
    union {float number;unsigned int bits;} value;aas_routingcache_t header;int offset=sizeof(routecacheheader_t),next;volatile unsigned int runtimeBits=bits;
    if(late){while(1){memcpy(&header,dump+offset,sizeof(header));next=offset+header.size;if(next==dumpLength)break;offset=next;}}
    OriginalInput();memcpy(&header,input+offset,sizeof(header));value.bits=runtimeBits;
    if(field==0)header.time=value.number;else if(field==1)header.starttraveltime=value.number;else header.origin[field-2]=value.number;
    memcpy(input+offset,&header,sizeof(header));Rejected(1,0);
}
static void Malformed(void) {
    int i,j,offset;aas_routingcache_t header;
    for(i=0;i<dumpLength;i++){OriginalInput();inputLength=i;Rejected(0,0);}
    for(i=0;i<8;i++)BadHeader(i,INT_MAX);
    BadHeader(6,-1);BadHeader(7,-1);
    for(i=0;i<2;i++){
        BadRecord(0,-1,i);BadRecord(0,INT_MAX,i);BadRecord(0,(int)sizeof(header)-1,i);BadRecord(0,(int)sizeof(header)+1,i);
        BadRecord(1,0,i);BadRecord(1,4,i);BadRecord(1,INT_MIN,i);BadRecord(2,0,i);BadRecord(2,6,i);BadRecord(2,4,i);BadRecord(3,255,i);
        for(j=0;j<5;j++){BadFloat(j,0x7fc00001U,i);BadFloat(j,0x7f800000U,i);BadFloat(j,0xff800000U,i);}
        BadFloat(1,0xbf800000U,i);
    }
    OriginalInput();input[inputLength++]=0;Rejected(1,0);
    OriginalInput();memcpy(&header,input+sizeof(routecacheheader_t),sizeof(header));
    input[sizeof(routecacheheader_t)+sizeof(header)+2*3+1]=255;Rejected(1,0);
    offset=sizeof(routecacheheader_t);while(1){memcpy(&header,dump+offset,sizeof(header));if(offset+header.size==dumpLength)break;offset+=header.size;}
    OriginalInput();input[offset+sizeof(header)+2*2+1]=255;Rejected(1,0);
    for(i=1;i<=4;i++){OriginalInput();Rejected(0,i);OriginalInput();Rejected(1,i);}
    for(i=1;i<=9;i++){
        FileWorld();OriginalInput();shortReadCall=i;Check(!AAS_ReadRouteCache()&&closes==1&&!routingcachesize&&!Outstanding(),"short read rejects without partial publication");Cleanup();
    }
    FileWorld();OriginalInput();routingcachesize=INT_MAX-((int)sizeof(header)+9);i=routingcachesize;
    Check(!AAS_ReadRouteCache()&&closes==1&&routingcachesize==i&&!Outstanding()&&!aasworld.oldestcache,"later cumulative signed-budget failure rolls back loaded costs");routingcachesize=0;Cleanup();
}
static void OriginalAccountingProof(void) {
    aas_routingcache_t header;int offset;FileWorld();OriginalInput();
    /* Exercise the original reader's size-first interpretation independently of its broken writer. */
    for(offset=sizeof(routecacheheader_t);offset<inputLength;offset+=header.size){
        memcpy(&header,input+offset,sizeof(header));header.prev=header.next=header.time_prev=header.time_next=NULL;header.reachabilities=NULL;
        memcpy(input+offset,&header,sizeof(header));memcpy(input+offset,&header.size,sizeof(header.size));
    }
    Check(AAS_ReadRouteCache(),"original reader-compatible dump reaches accounting check");
    Check(routingcachesize==expectedBytes,"original reader must account loaded cache sizes before state-change free");Cleanup();
}
static void ForgeEntry(int offset,int count,int index,unsigned short time,unsigned char reach) {
    memcpy(input+offset+offsetof(aas_routingcache_t,traveltimes)+index*sizeof(time),&time,sizeof(time));input[offset+sizeof(aas_routingcache_t)+count*sizeof(time)+index]=reach;
}
static void ForgedZeroTimeReach(int value) {
    /* Issue #307: a portal start reads its portal entry's reachability byte even at time zero. */
    static aas_reachabilityareas_t passes[6];aas_predictroute_t route;aas_routingcache_t portal,area;vec3_t origin={0,0,0};int offset=sizeof(routecacheheader_t),accepted,routed;
    FileWorld();aasworld.reachabilityareas=passes;OriginalInput();memcpy(&portal,input+offset,sizeof(portal));memcpy(&area,input+offset+portal.size,sizeof(area));
    Check(portal.type==CACHETYPE_PORTAL&&portal.areanum==1&&area.cluster==1&&area.areanum==1,"native dump starts with the goal's portal and cluster caches");
    /* Zero portal area 3's in-cluster time too, so the route from area 3 falls through to its portal entry. */
    ForgeEntry(offset+portal.size,3,2,0,0);ForgeEntry(offset,3,2,0,value);
    accepted=AAS_ReadRouteCache();routed=AAS_PredictRoute(&route,3,origin,1,-1,0,0,RSE_USETRAVELTYPE,0,0,0);
    Check(!accepted&&routed&&route.stopevent==RSE_NONE&&route.endarea==1,"forged zero-time portal reachability is rejected and prediction keeps the native route");
    Cleanup();Rejected(1,0);
}
static void EmptyDump(void) {
    FileWorld();AAS_WriteRouteCache();OriginalInput();
    Check(dumpLength==(int)sizeof(routecacheheader_t)&&AAS_ReadRouteCache()&&opens==2&&closes==2&&!Outstanding()&&!routingcachesize,"native writer's empty cache dump remains valid");Cleanup();
}
#ifndef Q3_AAS_CACHE_FILE_ENTRY
#define Q3_AAS_CACHE_FILE_ENTRY main
#endif
int Q3_AAS_CACHE_FILE_ENTRY(int argc,char **argv) {
    if(argc==1)EmptyDump();Generate();if(argc>1){if(atoi(argv[1]))OriginalAccountingProof();else RoundTrip(0);}
    else {RoundTrip(0);RoundTrip(1);Malformed();ForgedZeroTimeReach(3);ForgedZeroTimeReach(255);ForgedZeroTimeReach(1);puts("Native AAS cache writer/reader grammar, loaded accounting, links and rollback passed (issue #47)");}
    return 0;
}
