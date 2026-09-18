/* Actual AAS/public setup and native heap/hunk metadata, owners and callbacks. */
#include Q3_AAS_SETUP_SOURCE
extern int Export_BotLibSetup(void);
extern int botlibsetup;
static void *heap[64],*hunk[16];
static int heapLive,hunkLive,requests,failAt,errors,setupCalls,logCalls;
static void Check(int condition,const char *message){if(!condition){fprintf(stderr,"AAS setup regression failed: %s\n",message);exit(1);}}
static void *Allocate(int size,int arena){void **slots=arena?hunk:heap;int cap=arena?16:64,i;Check(size>0,"complete signed native allocation");requests++;if(requests==failAt||size>1048576)return NULL;for(i=0;i<cap;i++)if(!slots[i]){slots[i]=malloc((size_t)size);Check(slots[i]!=NULL,"physical native allocation");if(arena)hunkLive++;else heapLive++;return slots[i];}Check(0,"bounded physical fixture owners");return NULL;}
static void *HeapAlloc(int size){return Allocate(size,0);}
static void *HunkAlloc(int size){return Allocate(size,1);}
static void HeapFree(void *pointer){int i;for(i=0;i<64;i++)if(heap[i]==pointer){free(pointer);heap[i]=NULL;heapLive--;return;}Check(0,"only physical heap owners free through native callback");}
#ifndef Com_Memset
void Com_Memset(void *out,int value,size_t size){memset(out,value,size);}
#endif
#ifndef Com_Memcpy
void Com_Memcpy(void *out,const void *in,size_t size){memcpy(out,in,size);}
#endif
void QDECL Com_Error(int level,const char *format,...){(void)level;(void)format;Check(0,"unexpected native fatal");}
void QDECL Com_Printf(const char *format,...){(void)format;}
static void QDECL Print(int level,char *format,...){(void)format;if(level==PRT_ERROR||level==PRT_FATAL)errors++;else Check(level==PRT_MESSAGE||level==PRT_WARNING,"native AAS setup severity");}
void Log_Open(char *filename){Check(!strcmp(filename,"botlib.log"),"native setup log path");logCalls++;}
int EA_Setup(void){setupCalls++;return BLERR_NOERROR;}
int BotSetupWeaponAI(void){setupCalls++;return BLERR_NOERROR;}
int BotSetupGoalAI(void){setupCalls++;return BLERR_NOERROR;}
int BotSetupChatAI(void){setupCalls++;return BLERR_NOERROR;}
int BotSetupMoveAI(void){setupCalls++;return BLERR_NOERROR;}
static void Begin(void){Check(!heapLive&&!hunkLive,"prior native arenas physically reset");memset(&aasworld,0,sizeof(aasworld));memset(&botlibglobals,0,sizeof(botlibglobals));saveroutingcache=NULL;botlibsetup=0;requests=failAt=errors=setupCalls=logCalls=0;botimport.GetMemory=HeapAlloc;botimport.HunkAlloc=HunkAlloc;botimport.FreeMemory=HeapFree;botimport.Print=Print;}
static void End(void){int i;if(aasworld.entities)FreeMemory(aasworld.entities);aasworld.entities=NULL;saveroutingcache=NULL;LibVarDeAllocAll();Check(!heapLive,"native cache heap owners physically release");for(i=0;i<16;i++)if(hunk[i]){free(hunk[i]);hunk[i]=NULL;hunkLive--;}Check(!hunkLive,"owning engine resets physical hunk arena after logical teardown");}
static void Complete(int clients,int entities){int i;Check(aasworld.maxclients==clients&&aasworld.maxentities==entities&&aasworld.entities&&saveroutingcache&&aasworld.numframes==0,"all complete native AAS fields publish");for(i=0;i<entities;i++)Check(!aasworld.entities[i].i.valid&&aasworld.entities[i].i.number==i,"actual entity invalidation numbers every native entity");}
static void Failure(int occupied,int fault){
    aas_t saved;aas_entity_t snapshot[2];libvar_t *priorroute=NULL;int imports,priorHeap,priorHunk;
    Begin();if(occupied){aasworld.maxclients=2;aasworld.maxentities=2;aasworld.numframes=91;aasworld.entities=GetClearedHunkMemory(2*sizeof(aas_entity_t));Check(aasworld.entities!=NULL,"actual prior hunk metadata/entities prepare");aasworld.entities[0].i.valid=1;aasworld.entities[1].i.number=71;memcpy(snapshot,aasworld.entities,sizeof(snapshot));priorroute=LibVar("priorroute","1");Check(priorroute!=NULL,"complete prior route variable");saveroutingcache=priorroute;}
    saved=aasworld;imports=requests;priorHeap=heapLive;priorHunk=hunkLive;failAt=imports+fault;
    Check(AAS_Setup()==BLERR_LIBRARYNOTSETUP&&errors==1&&requests==failAt&&!memcmp(&saved,&aasworld,sizeof(saved))&&saveroutingcache==priorroute&&hunkLive==priorHunk,"nullable count/cache/entity imports reject before prior world publication or hunk consumption");
    if(occupied)Check(!memcmp(snapshot,aasworld.entities,sizeof(snapshot)),"complete prior entity payload remains unchanged");
    Check(heapLive==priorHeap+2*((fault-1)/2),"only complete shared cache variables retain physical heap owners");
    failAt=0;Check(AAS_Setup()==BLERR_NOERROR&&errors==1,"failed setup retries");Complete(128,1024);Check(hunkLive==priorHunk+1,"prior hunk logical release leaves physical arena ownership with engine");End();
}
static float Opaque(unsigned int bits){volatile unsigned int representation=bits;unsigned int copy=representation;float value;memcpy(&value,&copy,sizeof(value));return value;}
static void Invalid(int field,unsigned int bits,int exported){
    libvar_t *variable;aas_t saved;int imports;
    Begin();variable=LibVar(field?"maxentities":"maxclients",field?"1024":"128");Check(variable!=NULL,"actual invalid-count cache prepares");variable->value=Opaque(bits);saved=aasworld;imports=requests;
    Check((exported?Export_BotLibSetup():AAS_Setup())==BLERR_LIBRARYNOTSETUP&&errors==1&&!hunkLive&&!memcmp(&saved,&aasworld,sizeof(saved))&&!setupCalls&&!botlibglobals.botlibsetup&&!botlibsetup,"invalid count rejects before world/hunk/downstream setup or public integer publication");
    if(exported)Check(!botlibglobals.maxclients&&!botlibglobals.maxentities&&logCalls==1,"public failure keeps cleared library state before count casts");
    Check(requests==imports+(field?2:0),"invalid count stops before later cache/arena imports");End();
}
static void Golden(void){int imports;Begin();Check(Export_BotLibSetup()==BLERR_NOERROR&&!errors&&setupCalls==5&&logCalls==1&&botlibsetup&&botlibglobals.botlibsetup&&botlibglobals.maxclients==128&&botlibglobals.maxentities==1024,"actual exported setup publishes native defaults only after valid AAS initialization");Complete(128,1024);Check(heapLive==6&&hunkLive==1,"native default cache and physical hunk ownership");imports=requests;LibVarSet("maxclients","2.75");LibVarSet("maxentities","4.75");Check(AAS_Setup()==BLERR_NOERROR&&!errors,"native fractional positive counts retain truncation");Complete(2,4);Check(heapLive==6&&hunkLive==2&&requests>imports,"complete replacement retains native arena lifetime");End();}
int main(int argc,char **argv){int occupied,fault,field,exported;unsigned int bad[]={0,0xbf800000U,0x3f000000U,0x4f000000U,0x7f800000U,0xff800000U,0x7fc00000U};size_t i;if(argc>1){fault=atoi(argv[1]);if(fault<14)Failure(fault/7,fault%7+1);else if(fault<18)Invalid((fault-14)/2,0x7fc00000U,(fault-14)%2);else if(fault<20)Invalid(1,0x4b800000U,fault-18);else Golden();return 0;}for(occupied=0;occupied<2;occupied++)for(fault=1;fault<=7;fault++)Failure(occupied,fault);for(field=0;field<2;field++)for(exported=0;exported<2;exported++)for(i=0;i<sizeof(bad)/sizeof(bad[0]);i++)Invalid(field,bad[i],exported);Invalid(1,0x4b800000U,0);Invalid(1,0x4b800000U,1);Golden();puts("Real AAS/export setup counts, native metadata, prior world rollback and physical heap/hunk lifetime passed (issues #47/#48)");return 0;}
