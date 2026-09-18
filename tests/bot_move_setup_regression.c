/* Actual movement setup, native libvars and physical nullable heap imports. */
#include Q3_MOVE_SOURCE
static void *owners[64];
static int liveOwners,requests,failAt,errors,brushCalls;
botlib_import_t botimport;
extern libvar_t *libvarlist;
static const char *names[]={"sv_step","sv_maxbarrier","sv_gravity","weapindex_rocketlauncher","weapindex_bfg10k","weapindex_grapple","entitytypemissile","offhandgrapple","cmd_grappleon","cmd_grappleoff"};
static const char *values[]={"18","32","800","5","9","10","3","0","grappleon","grappleoff"};
static void Check(int condition,const char *message){if(!condition){fprintf(stderr,"Bot movement setup regression failed: %s\n",message);exit(1);}}
void *GetMemory(unsigned long size){int i;Check(size>0&&size<=4096,"bounded native variable import");requests++;if(requests==failAt)return NULL;for(i=0;i<64;i++)if(!owners[i]){owners[i]=malloc(size);Check(owners[i]!=NULL,"physical fixture allocation");liveOwners++;return owners[i];}Check(0,"bounded physical owner capacity");return NULL;}
void FreeMemory(void *pointer){int i;if(!pointer)return;for(i=0;i<64;i++)if(owners[i]==pointer){free(pointer);owners[i]=NULL;liveOwners--;return;}Check(0,"known physical owner releases once");}
#ifndef Com_Memset
void Com_Memset(void *out,int value,size_t size){memset(out,value,size);}
#endif
void QDECL Com_Printf(const char *format,...){(void)format;Check(0,"unexpected native format warning");}
static void QDECL Print(int level,char *format,...){char message[128];va_list args;Check(level==PRT_ERROR,"failed setup native error severity");va_start(args,format);vsnprintf(message,sizeof(message),format,args);va_end(args);Check(strstr(message,"couldn't initialize movement variable ")!=NULL,"complete native failure diagnostic");errors++;}
int AAS_NextBSPEntity(int ent){brushCalls++;return ent<2?ent+1:0;}
int AAS_ValueForBSPEpairKey(int ent,char *key,char *value,int size){const char *text;Check(ent==1||ent==2,"known native brush entity");if(!strcmp(key,"classname"))text=ent==1?"func_plat":"func_door";else {Check(!strcmp(key,"model"),"native model epair");text=ent==1?"*7":"*9";}Check(size>(int)strlen(text),"complete native model epair capacity");strcpy(value,text);return qtrue;}
static void References(libvar_t **out){out[0]=sv_maxstep;out[1]=sv_maxbarrier;out[2]=sv_gravity;out[3]=weapindex_rocketlauncher;out[4]=weapindex_bfg10k;out[5]=weapindex_grapple;out[6]=entitytypemissile;out[7]=offhandgrapple;out[8]=cmd_grappleon;out[9]=cmd_grappleoff;}
static void Assign(libvar_t **in){sv_maxstep=in[0];sv_maxbarrier=in[1];sv_gravity=in[2];weapindex_rocketlauncher=in[3];weapindex_bfg10k=in[4];weapindex_grapple=in[5];entitytypemissile=in[6];offhandgrapple=in[7];cmd_grappleon=in[8];cmd_grappleoff=in[9];}
static void Begin(void){libvar_t *empty[10]={0};int i;Check(!liveOwners&&!libvarlist,"previous shared variable owners released");Assign(empty);requests=failAt=errors=brushCalls=0;botimport.Print=Print;for(i=0;i<MAX_MODELS;i++)modeltypes[i]=77;}
static void End(void){libvar_t *empty[10]={0};LibVarDeAllocAll();Assign(empty);Check(!liveOwners&&!libvarlist,"all complete cached variables physically release");}
static void Complete(int priorOwners){libvar_t *actual[10];int i;References(actual);for(i=0;i<10;i++)Check(actual[i]&&actual[i]==LibVarGet((char *)names[i])&&!strcmp(actual[i]->string,values[i]),"complete native movement variable values/references");Check(liveOwners==priorOwners+20&&modeltypes[7]==MODELTYPE_FUNC_PLAT&&modeltypes[9]==MODELTYPE_FUNC_DOOR&&brushCalls==3,"native complete owners and brush classification");for(i=0;i<MAX_MODELS;i++)if(i!=7&&i!=9)Check(!modeltypes[i],"native unrelated model types clear");}
static void Failure(int occupied,int fault){
    libvar_t *prior[10]={0},*actual[10],saved[10];int modelSnapshot[MAX_MODELS];int i,priorOwners;
    Begin();if(occupied)for(i=0;i<10;i++){char name[32];snprintf(name,sizeof(name),"prior%d",i);prior[i]=LibVar(name,"101.25");Check(prior[i]!=NULL,"complete actual prior movement variables prepare");saved[i]=*prior[i];}Assign(prior);priorOwners=liveOwners;memcpy(modelSnapshot,modeltypes,sizeof(modelSnapshot));failAt=requests+fault;
    Check(BotSetupMoveAI()==BLERR_LIBRARYNOTSETUP&&errors==1&&requests==failAt&&!brushCalls,"each nullable native variable import rejects setup before brush/reference publication");References(actual);Check(!memcmp(actual,prior,sizeof(prior))&&!memcmp(modelSnapshot,modeltypes,sizeof(modelSnapshot))&&liveOwners==priorOwners+2*((fault-1)/2),"failed setup keeps complete prior references/model bytes and only complete cached owners");
    for(i=0;i<10;i++){if(occupied)Check(!memcmp(prior[i],&saved[i],sizeof(saved[i]))&&!strcmp(prior[i]->string,"101.25"),"prior complete variable/list/value/flag bytes remain unchanged");if(i<(fault-1)/2)Check(LibVarGet((char *)names[i])&&!strcmp(LibVarGet((char *)names[i])->string,values[i]),"completed shared variable cache remains valid");else Check(!LibVarGet((char *)names[i]),"failed/later variables are not partially published");}
    failAt=0;Check(BotSetupMoveAI()==BLERR_NOERROR&&errors==1,"failed native initialization retries successfully");Complete(priorOwners);End();
}
static void Golden(void){
    libvar_t *before[10],*after[10];int imports;
    Begin();Check(BotSetupMoveAI()==BLERR_NOERROR&&!errors,"native successful movement initialization");Complete(0);References(before);LibVarSet("sv_step","21.5");LibVarSet("cmd_grappleon","custom-on");imports=requests;brushCalls=0;failAt=imports+1;
    Check(BotSetupMoveAI()==BLERR_NOERROR&&requests==imports&&!errors&&brushCalls==3,"native cached setup reuses defaults without imports");References(after);Check(!memcmp(before,after,sizeof(before))&&sv_maxstep->value==21.5&&!strcmp(cmd_grappleon->string,"custom-on")&&liveOwners==20&&modeltypes[7]==MODELTYPE_FUNC_PLAT&&modeltypes[9]==MODELTYPE_FUNC_DOOR,"native configured values and reference identity remain");End();
}
int main(int argc,char **argv){int occupied,fault;if(argc>1){fault=atoi(argv[1]);if(fault<40)Failure(fault/20,fault%20+1);else Golden();return 0;}for(occupied=0;occupied<2;occupied++)for(fault=1;fault<=20;fault++)Failure(occupied,fault);Golden();puts("Real movement libvar initialization, nullable imports, prior state, retry and native brush/default values passed (issue #48)");return 0;}
