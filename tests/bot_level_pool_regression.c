/* Actual level pool/list operations and public initializer failure guard. */
#define main ItemConfigFixtureMain
#include "bot_item_config_regression.c"
#undef main
static int infoCalls;
#ifdef MEMORYMANEGER
extern int numblocks,allocatedmemory,totalmemorysize;
#endif
#ifndef Q3_ITEM_NATIVE_INTERFACE
int bot_developer;
#endif
#ifndef Q3_POOL_BSP_HOOKS
int AAS_NextBSPEntity(int ent){Check(ent==0,"native empty BSP iteration");infoCalls++;return 0;}
int AAS_ValueForBSPEpairKey(int ent,char *key,char *value,int size){(void)ent;(void)key;(void)value;(void)size;Check(0,"unreachable empty BSP lookup");return 0;}
int AAS_VectorForBSPEpairKey(int ent,char *key,vec3_t out){(void)ent;(void)key;(void)out;Check(0,"unreachable empty BSP vector");return 0;}
int AAS_FloatForBSPEpairKey(int ent,char *key,float *out){(void)ent;(void)key;(void)out;Check(0,"unreachable empty BSP float");return 0;}
int AAS_IntForBSPEpairKey(int ent,char *key,int *out){(void)ent;(void)key;(void)out;Check(0,"unreachable empty BSP integer");return 0;}
int AAS_PointAreaNum(vec3_t point){(void)point;Check(0,"unreachable empty BSP area");return 0;}
#endif
int AAS_Loaded(void){return 0;}
int AAS_PointContents(vec3_t point){(void)point;Check(0,"unreachable empty BSP contents");return 0;}
bsp_trace_t AAS_Trace(vec3_t start,vec3_t mins,vec3_t maxs,vec3_t end,int entity,int contents){bsp_trace_t trace;memset(&trace,0,sizeof(trace));(void)start;(void)mins;(void)maxs;(void)end;(void)entity;(void)contents;Check(0,"unreachable empty BSP trace");return trace;}
int AAS_BestReachableFromJumpPadArea(vec3_t origin,vec3_t mins,vec3_t maxs){(void)origin;(void)mins;(void)maxs;Check(0,"unreachable empty BSP jump query");return 0;}
int AAS_DropToFloor(vec3_t origin,vec3_t mins,vec3_t maxs){(void)origin;(void)mins;(void)maxs;Check(0,"unreachable empty BSP drop");return 0;}
int AAS_BestReachableArea(vec3_t origin,vec3_t mins,vec3_t maxs,vec3_t goal){(void)origin;(void)mins;(void)maxs;(void)goal;Check(0,"unreachable empty BSP reachable query");return 0;}
static void PoolBegin(void){Begin();Check(!levelitemheap&&!freelevelitems&&!levelitems&&!numlevelitems,"prior level pool/lists reset");infoCalls=0;itemconfig=NULL;}
static void PoolEnd(void){if(levelitemheap)FreeMemory(levelitemheap);levelitemheap=freelevelitems=levelitems=NULL;numlevelitems=0;End();
#ifdef MEMORYMANEGER
Check(!numblocks&&!allocatedmemory&&!totalmemorysize,"native tracked pool/cache logical ownership fully releases");
#endif
}
static void PriorPool(int occupied){PoolBegin();if(occupied){levelitemheap=GetClearedMemory(2*sizeof(levelitem_t));Check(levelitemheap!=NULL,"actual prior physical heap pool prepares");levelitemheap[0].next=&levelitemheap[1];freelevelitems=levelitemheap;AddLevelItemToList(AllocLevelItem());levelitems->number=71;numlevelitems=1;Attempt(nativeText);}}
static void PoolNullable(int occupied,int fault,int public){levelitem_t *prior,*freehead,*active,saved[2];int baseline;PriorPool(occupied);prior=levelitemheap;freehead=freelevelitems;active=levelitems;if(prior)memcpy(saved,prior,sizeof(saved));baseline=heapLive;failAt=fault;if(public)BotInitLevelItems();else Check(!InitLevelItemHeap(),"private nullable pool returns failure");Check(errors==1&&requests==failAt&&!infoCalls&&levelitemheap==prior&&freelevelitems==freehead&&levelitems==active&&numlevelitems==occupied&&!hunkLive&&heapLive==baseline+(fault==3?2:0),"nullable pool preserves prior lists/count/pointers before map information mutation");if(prior)Check(!memcmp(saved,prior,sizeof(saved)),"nullable replacement retains every active/free pool byte");Attempt(nativeText);Check(InitLevelItemHeap()&&levelitemheap&&freelevelitems==levelitemheap&&!levelitems&&!numlevelitems&&heapLive==3,"nullable pool retries and physically frees prior heap storage");PoolEnd();}

static void BadCount(unsigned int bits,int public){libvar_t *variable;levelitem_t *prior,*freehead,*active,saved[2];int imports;PriorPool(1);prior=levelitemheap;freehead=freelevelitems;active=levelitems;memcpy(saved,prior,sizeof(saved));variable=LibVar("max_levelitems","256");Check(variable!=NULL,"invalid pool count cache prepares");variable->value=Opaque(bits);imports=requests;if(public)BotInitLevelItems();else Check(!InitLevelItemHeap(),"private invalid count/cost rejects");Check(requests==imports&&errors==1&&!infoCalls&&heapLive==3&&!hunkLive&&levelitemheap==prior&&freelevelitems==freehead&&levelitems==active&&numlevelitems==1&&!memcmp(saved,prior,sizeof(saved)),"invalid native count/cost rejects before conversion/imports and retains all prior pool/list bytes");PoolEnd();}
static void PoolGolden(int public){levelitem_t *node,*li,zero;int i;PoolBegin();if(public)BotInitLevelItems();else Check(InitLevelItemHeap(),"native default private pool initialization");Check(levelitemheap&&freelevelitems==levelitemheap&&!levelitems&&!numlevelitems&&heapLive==3&&!hunkLive&&!errors&&infoCalls==public,"native default complete heap/list state and public empty-BSP initializer");node=freelevelitems;for(i=0;i<256;i++){Check(node==&levelitemheap[i],"native ascending default free chain");node=node->next;}Check(!node,"native final free link terminates");memset(&zero,0,sizeof(zero));for(i=0;i<256;i++){li=AllocLevelItem();Check(li==&levelitemheap[i]&&!memcmp(&zero,li,sizeof(zero)),"actual native allocation clears every node byte in ascending order");AddLevelItemToList(li);numlevelitems++;}Check(!freelevelitems&&levelitems==&levelitemheap[255]&&numlevelitems==256,"actual native allocation/list exhaustion");Check(!AllocLevelItem()&&errors==1,"native exhausted pool returns null with fatal diagnostic");PoolEnd();}
static void Configured(void){levelitem_t *node;PriorPool(1);Check(LibVar("max_levelitems","2.75")!=NULL,"complete configured fractional count");Check(InitLevelItemHeap()&&heapLive==3&&freelevelitems==levelitemheap&&!levelitems&&!numlevelitems&&!errors,"complete configured replacement physically releases prior pool/list and truncates count");node=AllocLevelItem();Check(node==&levelitemheap[0]&&freelevelitems==&levelitemheap[1],"configured first native free node");Check(AllocLevelItem()==&levelitemheap[1]&&!freelevelitems,"configured fractional count provides two nodes");FreeLevelItem(node);Check(AllocLevelItem()==node&&!freelevelitems,"actual native free/allocate round trip");PoolEnd();}
static void ConfiguredFirstGolden(void){PoolBegin();Check(LibVar("max_levelitems","2.75")!=NULL&&InitLevelItemHeap(),"native configured first pool initializes");Check(heapLive==3&&freelevelitems==levelitemheap&&levelitemheap[0].next==&levelitemheap[1]&&!levelitemheap[1].next&&!levelitems&&!numlevelitems&&!errors,"unchanged native fractional two-node free chain");PoolEnd();}
#ifndef Q3_LEVEL_POOL_ENTRY
#define Q3_LEVEL_POOL_ENTRY main
#endif
int Q3_LEVEL_POOL_ENTRY(int argc,char **argv){int i,occupied,fault,public;unsigned int bad[]={0,0x3f000000U,0xbf800000U,0x4f000000U,0x7f800000U,0xff800000U,0x7fc00000U,0x4c000000U};if(argc>1){i=atoi(argv[1]);if(i<12)PoolNullable((i/3)%2,i%3+1,i/6);else if(i<20)BadCount(bad[i-12],0);else if(i==20)PoolGolden(0);else if(i==21)PoolGolden(1);else if(i==22)Configured();else ConfiguredFirstGolden();return 0;}for(public=0;public<2;public++)for(occupied=0;occupied<2;occupied++)for(fault=1;fault<=3;fault++)PoolNullable(occupied,fault,public);for(public=0;public<2;public++)for(i=0;i<8;i++)BadCount(bad[i],public);PoolGolden(0);PoolGolden(1);Configured();ConfiguredFirstGolden();puts("Actual level pool/list/public guard counts, nullable rollback, physical heap owners and native free-chain goldens passed (issues #47/#48)");return 0;}
