/* Actual map API keeps the level items that fit when the native item pool runs out (issue #252). */
#define Q3_ITEM_NATIVE_INTERFACE
#define main ItemConfigFixtureMain
#include "bot_item_config_regression.c"
#undef main
extern int Export_BotLibLoadMap(const char *mapname);
static int bspItems,entities,mapLoads,brushCalls;
int AAS_Loaded(void){return 1;}
int AAS_LoadMap(const char *name){Check(!strcmp(name,"native"),"native map API name");mapLoads++;return BLERR_NOERROR;}
void BotSetBrushModelTypes(void){brushCalls++;}
int AAS_NextBSPEntity(int entity){Check(entity>=0&&entity<=bspItems,"bounded native BSP item entity");return entity<bspItems?entity+1:0;}
int AAS_ValueForBSPEpairKey(int entity,char *key,char *out,int size){Check(entity>=1&&entity<=bspItems&&!strcmp(key,"classname")&&size>13,"only native item classnames");strcpy(out,"weapon_native");return 1;}
int AAS_VectorForBSPEpairKey(int entity,char *key,vec3_t out){Check(!strcmp(key,"origin"),"native item origin key");out[0]=(float)entity*64;out[1]=out[2]=0;return 1;}
int AAS_IntForBSPEpairKey(int entity,char *key,int *out){(void)entity;(void)key;*out=0;return 0;}
int AAS_FloatForBSPEpairKey(int entity,char *key,float *out){(void)entity;(void)key;(void)out;Check(0,"no roam item weights");return 0;}
int AAS_PointAreaNum(vec3_t point){(void)point;Check(0,"no location or camp metadata");return 0;}
int AAS_PointContents(vec3_t point){(void)point;Check(0,"no floating items");return 0;}
bsp_trace_t AAS_Trace(vec3_t start,vec3_t mins,vec3_t maxs,vec3_t end,int entity,int contents){bsp_trace_t trace;memset(&trace,0,sizeof(trace));(void)start;(void)mins;(void)maxs;(void)end;(void)entity;(void)contents;Check(0,"no floating item traces");return trace;}
int AAS_BestReachableFromJumpPadArea(vec3_t origin,vec3_t mins,vec3_t maxs){(void)origin;(void)mins;(void)maxs;Check(0,"no floating jump pad items");return 0;}
int AAS_DropToFloor(vec3_t origin,vec3_t mins,vec3_t maxs){(void)origin;(void)mins;(void)maxs;return 1;}
int AAS_BestReachableArea(vec3_t origin,vec3_t mins,vec3_t maxs,vec3_t goal){(void)mins;(void)maxs;VectorCopy(origin,goal);return (int)(origin[0]/64);}
float AAS_Time(void){return 1;}
int AAS_NextEntity(int entity){Check(entity>=0&&entity<=entities,"bounded native item entity");return entity<entities?entity+1:0;}
int AAS_EntityType(int entity){(void)entity;return ET_ITEM;}
int AAS_EntityModelindex(int entity){(void)entity;return 7;}
void AAS_EntityInfo(int entity,aas_entityinfo_t *info){memset(info,0,sizeof(*info));info->origin[0]=info->lastvisorigin[0]=(float)entity*64;}
int AAS_AreaJumpPad(int area){(void)area;Check(0,"an exhausted pool never allocates an entity item");return 0;}
static void Load(int items,int expected){levelitem_t *li;int count=0,seen[257];memset(seen,0,sizeof(seen));bspItems=entities=items;Attempt(nativeText);mapLoads=brushCalls=0;botlibglobals.mapready=qfalse;
    Check(Export_BotLibLoadMap("native")==BLERR_NOERROR&&botlibglobals.mapready&&mapLoads==1&&brushCalls==1,"map API keeps bots enabled when the level item pool fills");
    Check(numlevelitems==expected&&errors==(items>expected),"pool keeps every item that fits and reports exhaustion once");
    for(li=levelitems;li;li=li->next){Check(++count<=expected&&li->number>=1&&li->number<=expected&&!seen[li->number]&&li->goalareanum==li->number&&!li->iteminfo&&li->origin[0]==li->number*64,"first native items stay published in load order");seen[li->number]=1;}
    Check(count==expected&&(expected<256)==(freelevelitems!=NULL),"complete published list and free pool");}
int main(void){levelitem_t *li;int i;Begin();botlibglobals.botlibsetup=1;itemconfig=LoadItemConfig("native.c");Check(itemconfig&&itemconfig->iteminfo[0].modelindex==7,"native item config");
    Load(300,256);
    for(i=0;i<3;i++)BotUpdateEntityItems();
    for(li=levelitems;li;li=li->next)Check(li->entitynum==li->number&&!li->timeout,"pooled items link to their native entities");
    Check(errors==1&&numlevelitems==256&&!freelevelitems,"item updates for the 44 unpooled entities add no repeated diagnostics");
    Load(300,256);Load(256,256);Load(2,2);
    BotFreeInfoEntities();FreeMemory(levelitemheap);levelitemheap=freelevelitems=levelitems=NULL;numlevelitems=0;FreeMemory(itemconfig);itemconfig=NULL;End();
    puts("Actual map API keeps the first 256 of 300 level items with one exhaustion diagnostic per map (issue #252)");return 0;}
