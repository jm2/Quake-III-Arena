/* Issue #45: actual BSP loading and native renderer model allocation at cache boundaries. */
#include "bsp_fixture.h"
#define __QGL_H__
typedef unsigned int GLuint;
#define GL_CLAMP 0x2900
#include "../code/renderer/tr_bsp.c"
#include "../code/renderer/tr_curve.c"
#include "../code/renderer/tr_model.c"

trGlobals_t tr;
glconfig_t glConfig;
refimport_t ri;
static cvar_t rendererVariable,subdivisions;
cvar_t *r_vertexLight=&rendererVariable,*r_lightmap=&rendererVariable,*r_mapOverBrightBits=&rendererVariable;
cvar_t *r_singleShader=&rendererVariable,*r_fullbright=&rendererVariable,*r_subdivisions=&subdivisions;
static shader_t knownShader;
static world_t retainedWorld;
static model_t cached[MAX_MOD_KNOWN],beforeCached[MAX_MOD_KNOWN];
static int rendererHunks,shaderCalls;
static void *RendererHunk(int size,ha_pref preference) { Check(preference==h_low,"native renderer hunk preference");rendererHunks++;return Hunk_Alloc(size,h_high); }
static void *RendererMalloc(int size) { Check(size>0,"native curve temporary size");return calloc(1,size); }
static void RendererFree(void *pointer) { free(pointer); }
shader_t *R_FindShader(const char *name,int lightmap,qboolean mipmap) { (void)name;(void)lightmap;(void)mipmap;shaderCalls++;return &knownShader; }
void R_SyncRenderThread(void) { }
void R_RemapShader(const char *oldName,const char *newName,const char *time) { (void)oldName;(void)newName;(void)time;Check(0,"unexpected cache fixture remap"); }
image_t *R_CreateImage(const char *name,const byte *pixels,int width,int height,qboolean mipmap,qboolean picmip,int wrap) { (void)name;(void)pixels;(void)width;(void)height;(void)mipmap;(void)picmip;(void)wrap;Check(0,"unexpected cache fixture upload");return NULL; }
static void QDECL Print(int level,const char *message,...) { (void)level;(void)message; }
static void Build(int models) {
	unsigned int offset;int i,j;
	BuildCM(2);offset=(sourceSize+3)&~3u;memset(source+sourceSize,0,offset-sourceSize+models*sizeof(dmodel_t));Lump(LUMP_MODELS,offset,models*sizeof(dmodel_t));sourceSize=offset+models*sizeof(dmodel_t);
	for(i=0;i<models;i++)for(j=0;j<3;j++) { Float(offset+i*sizeof(dmodel_t)+j*4,-1);Float(offset+i*sizeof(dmodel_t)+12+j*4,1); }
	Word(offset+offsetof(dmodel_t,numBrushes),1);
}
static void Cache(int count) {
	int i;
	memset(&tr,0,sizeof(tr));memset(cached,0,sizeof(cached));
	for(i=0;i<count && i<MAX_MOD_KNOWN;i++) { cached[i].index=i;cached[i].type=MOD_BAD;snprintf(cached[i].name,sizeof(cached[i].name),"cached-%d",i);tr.models[i]=cached+i; }
	tr.numModels=count;tr.world=&retainedWorld;tr.sunDirection[0]=.25f;
}
static void Header(dheader_t *header) { Check(!BSP_ValidateHeader(source,sourceSize,header) && !BSP_ValidateReferences(source,header) && !BSP_ValidateTree(source,header) && !R_ValidateBSPGeometry(source,header),"model-capacity fixtures retain valid layout/references/tree/geometry"); }
static void Reject(int existing,int models) {
	int a;dheader_t header;
	Build(models);Cache(existing);Header(&header);Check(R_ValidateBSPAllocations(&header)!=NULL,"native cache preflight rejects unavailable slots");
	for(a=0;a<4;a++) {
		trGlobals_t beforeTr=tr;world_t beforeWorld=s_worldData;
		int beforeReads=reads,beforeFrees=frees,beforeHunks=rendererHunks,beforeShaders=shaderCalls;
		memcpy(beforeCached,cached,sizeof(cached));alignment=a;readable=advertised=sourceSize;missing=0;expectError=1;
		if(!setjmp(errorJump)) { RE_LoadWorldMap("full-cache.bsp");Check(0,"unavailable model slots reached native loading"); }
		expectError=0;
		Check(reads==beforeReads+1 && frees==beforeFrees+1 && rendererHunks==beforeHunks && shaderCalls==beforeShaders && !fileAllocation && !memcmp(&tr,&beforeTr,sizeof(tr)) && !memcmp(&s_worldData,&beforeWorld,sizeof(s_worldData)) && !memcmp(cached,beforeCached,sizeof(cached)),"cache rejection changed registry/world or allocation ownership");
	}alignment=0;
}
static void NativeLoad(int existing,int models,qboolean bootstrap) {
	int i,base=existing;char name[40];dheader_t header;
	FreeHunks();Build(models);Cache(existing);
	if(bootstrap) { R_ModelInit();base=1;Check(tr.numModels==1 && tr.models[0]->index==0 && tr.models[0]->type==MOD_BAD,"actual native null-model bootstrap"); }
	Header(&header);Check(!R_ValidateBSPAllocations(&header),"safe native cache capacity accepted");
	memcpy(beforeCached,cached,sizeof(cached));readable=advertised=sourceSize;alignment=0;RE_LoadWorldMap("cache.bsp");
	Check(tr.worldMapLoaded && tr.world==&s_worldData && tr.numModels==base+models && !fileAllocation && !memcmp(cached,beforeCached,sizeof(cached)),"actual allocator publishes brush models without overwriting cached descriptors");
	for(i=0;i<models;i++) {
		model_t *model=tr.models[base+i];snprintf(name,sizeof(name),"*%d",i);
		Check(model && model->index==base+i && model->type==MOD_BRUSH && model->bmodel==s_worldData.bmodels+i && !strcmp(model->name,name) && model->bmodel->bounds[0][0]==-1 && model->bmodel->bounds[1][2]==1,"complete actual brush model/index/name/bounds at remaining cache slots");
		Check(R_GetModelByHandle(base+i)==model,"actual model handle lookup preserves native indexes");
	}
	for(i=0;i<existing;i++)Check(tr.models[i]==cached+i,"existing cache pointers retained");
}
int main(void) {
	ri.Error=Com_Error;ri.Printf=Print;ri.FS_ReadFile=FS_ReadFile;ri.FS_FreeFile=FS_FreeFile;ri.Hunk_Alloc=RendererHunk;ri.Malloc=RendererMalloc;ri.Free=RendererFree;subdivisions.value=4;
	NativeLoad(0,1,qtrue);NativeLoad(MAX_MOD_KNOWN-1,1,qfalse);NativeLoad(MAX_MOD_KNOWN-MAX_SUBMODELS,MAX_SUBMODELS,qfalse);
	Reject(MAX_MOD_KNOWN,1);Reject(MAX_MOD_KNOWN-1,2);Reject(MAX_MOD_KNOWN-MAX_SUBMODELS+1,MAX_SUBMODELS);
	Reject(-1,1);Reject(MAX_MOD_KNOWN+1,1);
	Check(!fileAllocation,"all cache fixture input ownership released");FreeHunks();
	puts("BSP native model-cache capacity, real allocation/bootstrap and world ownership regressions passed (issue #45)");return 0;
}
