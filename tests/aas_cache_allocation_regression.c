/* Actual cache allocation/accounting: native layout, signed costs and nullable imports. */
#include Q3_AAS_ROUTE_SOURCE
botlib_import_t botimport;
aas_t aasworld;
static int requests,failImport;
static unsigned long lastRequest;
static void *owner;
static void Check(int condition,const char *message) {
    if(!condition){fprintf(stderr,"AAS cache allocation regression failed: %s\n",message);exit(1);}
}
void *GetClearedMemory(unsigned long size) {
    Check(!owner&&size>0&&size<=INT_MAX,"only representable positive cache costs reach native import");requests++;lastRequest=size;
    if(failImport||size>4096)return NULL;
    owner=calloc(1,size);Check(owner!=NULL,"exact fixture cache allocation");return owner;
}
void FreeMemory(void *pointer) {Check(pointer&&pointer==owner,"known native cache released once");free(pointer);owner=NULL;}
static void Reset(int counter) {
    Check(!owner,"previous cache physically released");memset(&aasworld,0,sizeof(aasworld));routingcachesize=counter;requests=failImport=0;lastRequest=0;
}
static void Release(aas_routingcache_t *cache,int prior) {
    AAS_LinkCache(cache);AAS_FreeRoutingCache(cache);
    Check(!owner&&routingcachesize==prior&&!aasworld.oldestcache&&!aasworld.newestcache,"native free restores signed byte counter and physical/LRU ownership");
}
static void Goldens(void) {
    const struct {int count,bytes,reachOffset;} cases[]={
        {0,0,0},{1,3,2},{2,6,4},{127,381,254},{128,384,256},
        {129,387,258},{255,765,510},{256,768,512},{512,1536,1024},{1024,3072,2048}
    };size_t i;
    for(i=0;i<sizeof(cases)/sizeof(cases[0]);i++) {
        aas_routingcache_t *cache;int size=(int)sizeof(aas_routingcache_t)+cases[i].bytes;Reset(37);cache=AAS_AllocRoutingCache(cases[i].count);
        Check(cache&&requests==1&&lastRequest==(unsigned long)size&&cache->size==size&&routingcachesize==37+size,"literal native variable-size cache costs");
        Check(cache->reachabilities==(unsigned char *)cache+sizeof(*cache)+cases[i].reachOffset,"literal native reachability byte offset retained");
        Check(cache->cluster==0&&cache->areanum==0&&cache->time_next==NULL&&cache->time_prev==NULL,"native clearing preserves initial cache metadata");
        if(cases[i].count){int last=cases[i].count-1;Check(!cache->traveltimes[0]&&!cache->traveltimes[last]&&!cache->reachabilities[0]&&!cache->reachabilities[last],"first/last native payload elements are cleared");cache->traveltimes[0]=11;cache->traveltimes[last]=65535;cache->reachabilities[0]=7;cache->reachabilities[last]=255;}
        Release(cache,37);
    }
}
static void CountRejection(int count) {
    Reset(23);Check(AAS_AllocRoutingCache(count)==NULL,"invalid/wrapped count rejects before returning undersized cache");
    Check(!requests&&!owner&&routingcachesize==23&&!aasworld.oldestcache&&!aasworld.newestcache,"invalid count has no native import/accounting/list mutation");
}
static void BadCounts(void) {
    int limit=(INT_MAX-(int)sizeof(aas_routingcache_t))/3;
    const int counts[]={INT_MIN,-1,0x55555555,INT_MAX};size_t i;
    for(i=0;i<sizeof(counts)/sizeof(counts[0]);i++)CountRejection(counts[i]);
    CountRejection(limit+1);CountRejection(limit+2);
}
static void BudgetRejection(int counter) {
    Reset(counter);Check(AAS_AllocRoutingCache(1)==NULL,"native signed cache budget rejects before overflow");
    Check(!requests&&!owner&&routingcachesize==counter&&!aasworld.oldestcache&&!aasworld.newestcache,"rejected byte budget has no native allocation or mutation");
}
static void Budgets(void) {
    aas_routingcache_t *cache;int size=(int)sizeof(aas_routingcache_t)+3;
    BudgetRejection(INT_MAX-size+1);BudgetRejection(INT_MAX);BudgetRejection(-1);BudgetRejection(INT_MIN);
    Reset(INT_MAX-size);cache=AAS_AllocRoutingCache(1);Check(cache&&routingcachesize==INT_MAX&&requests==1,"exact native signed byte capacity remains supported");Release(cache,INT_MAX-size);
}
static void Nullable(void) {
    int limit=(INT_MAX-(int)sizeof(aas_routingcache_t))/3,count;
    for(count=limit-2;count<=limit;count++){Reset(0);Check(AAS_AllocRoutingCache(count)==NULL&&requests==1&&lastRequest<=INT_MAX&&!owner&&!routingcachesize,"representable large costs remain nullable with unchanged accounting");}
    Reset(29);failImport=1;Check(AAS_AllocRoutingCache(2)==NULL&&requests==1&&!owner&&routingcachesize==29,"ordinary nullable native import preserves accounting");
}
int main(int argc,char **argv) {
    if(argc>1){int proof=atoi(argv[1]);if(proof==0)CountRejection(-1);else if(proof==1)CountRejection(0x55555555);else BudgetRejection(INT_MAX-(int)sizeof(aas_routingcache_t)-2);}
    else {Goldens();BadCounts();Budgets();Nullable();puts("Native AAS cache allocation layout, signed costs/accounting and nullable ownership passed (issue #47)");}
    return 0;
}
