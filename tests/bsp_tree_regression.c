/* Issue #45: bounded tree/forest validation and actual collision/renderer loading. */
#include <stdlib.h>
static void *GraphCalloc(size_t count,size_t size);
static void GraphFree(void *pointer);
#define calloc GraphCalloc
#define free GraphFree
#define BSP_FIXTURE_NATIVE_AREA_FLOOD
#include "bsp_fixture.h"
#undef calloc
#undef free
static void *graphTemporary;
static int failGraphTemporary,graphAllocations,graphFrees;
static void *GraphCalloc(size_t count,size_t size) {
	if(count==1 && size==4000000) return calloc(count,size); /* Shared native fixture arena. */
	Check(!graphTemporary && count>0 && size==sizeof(unsigned int) && count<500000,"bounded graph workspace");
	if(failGraphTemporary) { failGraphTemporary=0;return NULL; }
	graphTemporary=calloc(count,size);Check(graphTemporary!=NULL,"graph workspace");graphAllocations++;return graphTemporary;
}
static void GraphFree(void *pointer) { if(pointer==graphTemporary && pointer) { graphTemporary=NULL;graphFrees++; }free(pointer); }
#define __QGL_H__
typedef unsigned int GLuint;
#define GL_CLAMP 0x2900
#include "../code/renderer/tr_bsp.c"
#include "../code/renderer/tr_curve.c"

trGlobals_t tr;
glconfig_t glConfig;
refimport_t ri;
int c_pointcontents;
static cvar_t rendererVariable,subdivisions;
cvar_t *r_vertexLight=&rendererVariable,*r_lightmap=&rendererVariable,*r_mapOverBrightBits=&rendererVariable;
cvar_t *r_singleShader=&rendererVariable,*r_fullbright=&rendererVariable,*r_subdivisions=&subdivisions;
static shader_t knownShader;
static model_t knownModels[4];
static int shaderCalls,modelCalls,rendererHunks;
static unsigned int At(int lump,int offset) { return BSP_FileWord(source+8+lump*8)+offset; }
static unsigned int Append(int lump,int size) { unsigned int offset=(sourceSize+3)&~3u;memset(source+sourceSize,0,offset-sourceSize+size);Lump(lump,offset,size);sourceSize=offset+size;return offset; }
static void *RendererHunk(int size,ha_pref preference) { Check(preference==h_low,"native renderer hunk preference");rendererHunks++;return Hunk_Alloc(size,h_high); }
static void *RendererMalloc(int size) { Check(size>0,"native curve temporary size");return calloc(1,size); }
static void RendererFree(void *pointer) { free(pointer); }
shader_t *R_FindShader(const char *name,int lightmap,qboolean mipmap) { (void)name;(void)lightmap;(void)mipmap;shaderCalls++;return &knownShader; }
model_t *R_AllocModel(void) { return &knownModels[modelCalls++%4]; }
void R_SyncRenderThread(void) { }
void R_RemapShader(const char *oldName,const char *newName,const char *time) { (void)oldName;(void)newName;(void)time;Check(0,"unexpected graph fixture remap"); }
image_t *R_CreateImage(const char *name,const byte *pixels,int width,int height,qboolean mipmap,qboolean picmip,int wrap) { (void)name;(void)pixels;(void)width;(void)height;(void)mipmap;(void)picmip;(void)wrap;Check(0,"unexpected graph fixture upload");return NULL; }
static void QDECL Print(int level,const char *message,...) { (void)level;(void)message; }
static void Header(dheader_t *header) { Check(!BSP_ValidateHeader(source,sourceSize,header) && !BSP_ValidateReferences(source,header) && !R_ValidateBSPGeometry(source,header),"graph mutation retains valid layout/references/finite geometry"); }
static void Child(int node,int side,unsigned int child) { Word(At(LUMP_NODES,node*sizeof(dnode_t)+offsetof(dnode_t,children)+side*4),child); }
static void Build(int count) {
	unsigned int offset;int i;
	BuildCM(2);offset=Append(LUMP_NODES,count*sizeof(dnode_t));
	for(i=0;i<count;i++) { Child(i,0,i+1<count?(unsigned int)i+1:0xfffffffeu);Child(i,1,0xffffffffu); }
	offset=Append(LUMP_LEAFS,2*sizeof(dleaf_t));Word(offset+offsetof(dleaf_t,cluster),0xffffffffu);Word(offset+offsetof(dleaf_t,area),0xffffffffu);Word(offset+sizeof(dleaf_t)+offsetof(dleaf_t,numLeafBrushes),1);
}
static void RejectRenderer(void) {
	trGlobals_t beforeTr=tr;world_t beforeWorld=s_worldData;
	int beforeReads=reads,beforeFrees=frees,beforeHunks=rendererHunks,beforeShaders=shaderCalls,beforeModels=modelCalls;
	readable=advertised=sourceSize;missing=0;expectError=1;
	if(!setjmp(errorJump)) { RE_LoadWorldMap("bad-tree.bsp");Check(0,"renderer accepted unsafe tree"); }
	expectError=0;
	Check(reads==beforeReads+1 && frees==beforeFrees+1 && rendererHunks==beforeHunks && shaderCalls==beforeShaders && modelCalls==beforeModels && !graphTemporary && !fileAllocation && !memcmp(&tr,&beforeTr,sizeof(tr)) && !memcmp(&s_worldData,&beforeWorld,sizeof(s_worldData)),"tree rejection changed world/ownership/callback state");
}
static void RejectTree(void) {
	int a;dheader_t header;Header(&header);Check(BSP_ValidateTree(source,&header)!=NULL && !graphTemporary,"tree preflight controlled rejection");
	tr.worldMapLoaded=qfalse;
	for(a=0;a<4;a++) { alignment=a;RejectCM();Check(!graphTemporary,"collision graph rejection cleanup");RejectRenderer(); }alignment=0;
}
static void NativeLoad(int count,int models,int visibleParent) {
	int checksum,i,beforeModels=modelCalls;dheader_t header;vec3_t visible={-2,0,0},opaque={0,0,0};
	Header(&header);Check(!BSP_ValidateTree(source,&header) && !graphTemporary,"valid native tree/forest");
	readable=advertised=sourceSize;alignment=0;CM_LoadMap("tree.bsp",qfalse,&checksum);
	Check(cm.numNodes==count && CM_PointLeafnum(visible)==1 && CM_PointLeafnum(opaque)==0,"native iterative point-to-leaf queries and collision tree publication");
	tr.worldMapLoaded=qfalse;RE_LoadWorldMap("tree.bsp");
	Check(tr.worldMapLoaded && tr.world==&s_worldData && s_worldData.numDecisionNodes==count && !s_worldData.nodes[0].parent && modelCalls==beforeModels+models && !graphTemporary && !fileAllocation,"actual renderer tree/world publication");
	for(i=0;i<count;i++) {
		mnode_t *node=s_worldData.nodes+i;unsigned int child=BSP_FileWord(source+At(LUMP_NODES,i*sizeof(dnode_t)+offsetof(dnode_t,children)));
		if(!(child&0x80000000u)) Check(s_worldData.nodes[child].parent==node,"every native unique decision-node parent");
	}
	Check(s_worldData.nodes[count+1].parent==s_worldData.nodes+visibleParent,"visible-leaf native ancestor");
}
static int children[3][2],color[3];
static int Acyclic(int node) {
	int j;if(color[node]==1)return 0;if(color[node]==2)return 1;color[node]=1;
	for(j=0;j<2;j++)if(children[node][j]>=0 && !Acyclic(children[node][j]))return 0;
	color[node]=2;return 1;
}
/* Small independent recursive oracle, never used for large/untrusted-depth inputs. */
static int Expected(void) {
	int incoming[3]={0},visibleParent=-1,i,j,child;
	for(i=0;i<3;i++)for(j=0;j<2;j++) {
		child=children[i][j];
		if(child>=0) { if(child==0 || ++incoming[child]>1)return 0; }
		else if(child==-2) { if(visibleParent!=-1 && visibleParent!=i)return 0;visibleParent=i; }
	}
	memset(color,0,sizeof(color));for(i=0;i<3;i++)if(!Acyclic(i))return 0;return 1;
}
int main(void) {
	int i,j,code,n,valid,checksum;unsigned int offset;dheader_t header;const char *error;
	ri.Error=Com_Error;ri.Printf=Print;ri.FS_ReadFile=FS_ReadFile;ri.FS_FreeFile=FS_FreeFile;ri.Hunk_Alloc=RendererHunk;ri.Malloc=RendererMalloc;ri.Free=RendererFree;subdivisions.value=4;
	Build(3);NativeLoad(3,1,2);
	Build(3);Child(0,0,0);RejectTree(); /* Root self-cycle. */
	Build(3);Child(1,0,0);RejectTree(); /* Mutual root cycle. */
	Build(3);Child(0,1,1);RejectTree(); /* Duplicate decision edge. */
	Build(4);Child(0,1,2);RejectTree(); /* Acyclic diamond/multiple incoming edges. */
	Build(3);Child(0,1,0xfffffffeu);RejectTree(); /* Visible leaf has different parents. */
	Build(6);for(i=0;i<6;i++)Child(i,0,0xffffffffu);Child(0,0,0xfffffffeu);Child(1,0,2);Child(2,0,1);Header(&header);error=BSP_ValidateTree(source,&header);Check(error && strstr(error,"cyclic"),"unreachable closed cycle checked in its component");RejectTree();
	Build(40);for(i=0;i<39;i++)Child(i,1,i+1);RejectTree(); /* Would repeat 2^39 paths in recursive initialization. */
	Build(3);Header(&header);for(i=0;i<4;i++) { alignment=i;failGraphTemporary=1;RejectCM();Check(!failGraphTemporary && !graphTemporary,"collision temporary OOM retains map");failGraphTemporary=1;RejectRenderer();Check(!failGraphTemporary && !graphTemporary,"renderer temporary OOM retains map"); }alignment=0;
	Build(6);Child(1,0,0xfffffffeu);Child(3,0,0xffffffffu);Child(5,0,0xffffffffu);
	offset=Append(LUMP_MODELS,3*sizeof(dmodel_t));for(i=0;i<3;i++) { for(j=0;j<3;j++) { Float(offset+i*sizeof(dmodel_t)+j*4,-1);Float(offset+i*sizeof(dmodel_t)+12+j*4,1); } }Word(offset+offsetof(dmodel_t,numBrushes),1);
	NativeLoad(6,3,1);Check(!s_worldData.nodes[2].parent && !s_worldData.nodes[4].parent,"inline-model forest roots retain NULL parents");
	/* Every 3-node/two-leaf graph: legal child references, independent topology oracle. */
	Build(3);Header(&header);
	for(code=0;code<15625;code++) {
		n=code;for(i=0;i<3;i++)for(j=0;j<2;j++) { int choice=n%5;n/=5;children[i][j]=choice<3?choice:choice==3?-1:-2;Child(i,j,(unsigned int)children[i][j]); }
		valid=BSP_ValidateTree(source,&header)==NULL;Check(valid==Expected() && !graphTemporary,"exhaustive small-graph acceptance matches independent oracle");
	}
	/* Long valid trees use bounded heap/linear parent initialization, with no depth cap. */
	FreeHunks();Build(4096);NativeLoad(4096,1,4095);
	Build(MAX_MAP_NODES+1);Header(&header);Check(!CM_ValidateBSPAllocations(&header) && !R_ValidateBSPAllocations(&header) && !BSP_ValidateTree(source,&header) && !graphTemporary,"raised compiler node budget remains supported by native storage");
	/* Retained collision state still answers its actual loaded deep tree after validation. */
	{ vec3_t point={-2,0,0};checksum=CM_PointLeafnum(point);Check(checksum==1,"deep loaded native point query retained"); }
	Check(graphAllocations==graphFrees && !graphTemporary && !fileAllocation,"all graph/input ownership released");FreeHunks();
	puts("BSP tree/forest topology, exhaustive graph, native parent and collision/renderer ownership regressions passed (issue #45)");return 0;
}
