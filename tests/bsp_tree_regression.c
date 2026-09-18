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
#include "../code/renderer/tr_marks.c"

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
		if(!(child&0x80000000u)) Check(s_worldData.nodes[child].parent==node && cm.nodes[child].parent==i,"every native renderer and collision decision-node parent");
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
static int queryLeaves[8192],queryCount;
static void QueryLeaf(leafList_t *ll,int encoded) {
	(void)ll;Check(queryCount<8192,"bounded captured query callbacks");queryLeaves[queryCount++]=-1-encoded;
}
/* Independent stock recursion is restricted to small trusted fixture trees. */
static void QueryOracle(leafList_t *ll,int node,int *expected,int *count) {
	int side;if(node<0) { expected[(*count)++]=-1-node;return; }
	side=BoxOnPlaneSide(ll->bounds[0],ll->bounds[1],cm.nodes[node].plane);
	if(side!=2)QueryOracle(ll,cm.nodes[node].children[0],expected,count);
	if(side!=1)QueryOracle(ll,cm.nodes[node].children[1],expected,count);
}
static void SmallMarkQueries(cNode_t *collision,leafList_t *ll,int root,const int *expected,int number) {
	mnode_t nodes[3],leaves[2];msurface_t surfaces[2],*marks[2];shader_t shaders[2];
	surfaceType_t types[2]={SF_GRID,SF_GRID},*list[3];vec3_t dir={0,0,-1};int i,j,length=0,seen[2]={0},wanted[2],count=0;
	memset(nodes,0,sizeof(nodes));memset(leaves,0,sizeof(leaves));memset(surfaces,0,sizeof(surfaces));memset(shaders,0,sizeof(shaders));
	for(i=0;i<2;i++) { surfaces[i].data=types+i;surfaces[i].shader=shaders+i;marks[i]=surfaces+i;leaves[i].firstmarksurface=marks+i;leaves[i].nummarksurfaces=1; }
	for(i=0;i<3;i++) {
		nodes[i].contents=-1;nodes[i].plane=collision[i].plane;
		for(j=0;j<2;j++)nodes[i].children[j]=children[i][j]>=0?nodes+children[i][j]:leaves+(-1-children[i][j]);
	}
	for(i=0;i<3;i++)for(j=0;j<2;j++)if(children[i][j]>=0)nodes[children[i][j]].parent=nodes+i;
	for(i=0;i<number;i++)if(!seen[expected[i]]) { seen[expected[i]]=1;wanted[count++]=expected[i]; }
	tr.viewCount=501;R_BoxSurfaces_r(nodes+root,ll->bounds[0],ll->bounds[1],list,2,&length,dir);
	Check(length==count,"every small renderer mark query has native unique surface count");
	for(i=0;i<count;i++)Check(list[i]==types+wanted[i],"every accepted renderer forest/subtree retains front-first mark order");
}
static void SmallQueries(void) {
	clipMap_t retained=cm;cNode_t nodes[3];leafList_t ll;int expected[64],number,i,j,k,root;
	const float boxes[][2]={{-2,0},{-2,-2},{0,0},{1,2},{-1,-1}};
	for(i=0;i<3;i++) { nodes[i].plane=retained.planes+2*i;nodes[i].parent=-1;for(j=0;j<2;j++)nodes[i].children[j]=children[i][j]; }
	for(i=0;i<3;i++)for(j=0;j<2;j++)if(children[i][j]>=0)nodes[children[i][j]].parent=i;
	cm.nodes=nodes;cm.numNodes=3;memset(&ll,0,sizeof(ll));ll.storeLeafs=QueryLeaf;
	for(k=0;k<sizeof(boxes)/sizeof(boxes[0]);k++)for(root=0;root<3;root++) {
		for(i=0;i<3;i++) { ll.bounds[0][i]=boxes[k][0];ll.bounds[1][i]=boxes[k][1]; }
		queryCount=number=0;CM_BoxLeafnums_r(&ll,root);QueryOracle(&ll,root,expected,&number);
		Check(queryCount==number && !memcmp(queryLeaves,expected,number*sizeof(int)),"every accepted small forest/subtree and box matches stock callback order");SmallMarkQueries(nodes,&ll,root,expected,number);
	}
	cm=retained;
}
static void BoxQueries(int count) {
	leafList_t ll;int expected[64],expectedCount=0,last=-1,list[5],result,i; cbrush_t *brushes[2];
	vec3_t mins={-2,-1,-1},maxs={0,1,1},front={-2,0,0},back={0,0,0};
	memset(&ll,0,sizeof(ll));VectorCopy(mins,ll.bounds[0]);VectorCopy(maxs,ll.bounds[1]);ll.storeLeafs=QueryLeaf;
	queryCount=0;CM_BoxLeafnums_r(&ll,0);
	if(count<=32) {
		QueryOracle(&ll,0,expected,&expectedCount);
		Check(queryCount==expectedCount && !memcmp(queryLeaves,expected,queryCount*sizeof(int)),"all native front-first leaf callbacks match stock oracle");
	} else {
		Check(queryCount==count+1 && queryLeaves[0]==1,"deep crossing query visits visible leaf first");
		for(i=1;i<queryCount;i++)Check(queryLeaves[i]==0,"deep crossing query preserves every repeated opaque-leaf callback");
	}
	memset(list,0x5a,sizeof(list));result=CM_BoxLeafnums(mins,maxs,list+1,2,&last);
	Check(result==2 && list[1]==queryLeaves[0] && list[2]==queryLeaves[1] && list[0]==0x5a5a5a5a && list[3]==0x5a5a5a5a && last==1,"bounded leaf-list prefix, guard and native last visible leaf");
	last=-1;result=CM_BoxLeafnums(mins,maxs,NULL,0,&last);Check(!result && last==1,"zero-capacity traversal still updates last visible leaf");
	result=CM_BoxLeafnums(front,front,list,5,&last);Check(result==1 && list[0]==1 && last==1,"front-only deep query");
	result=CM_BoxLeafnums(back,back,list,5,&last);Check(result==1 && list[0]==0 && last==0,"back-only query preserves native last-leaf default");
	cm.leafs[0].firstLeafBrush=cm.leafs[1].firstLeafBrush;cm.leafs[0].numLeafBrushes=1;
	result=CM_BoxBrushes(mins,maxs,brushes,2);Check(result==1 && brushes[0]==cm.brushes,"actual brush query retains deduplication across repeated/shared leaves");
	result=CM_BoxBrushes(mins,maxs,NULL,0);Check(!result,"zero-capacity brush query");cm.leafs[0].numLeafBrushes=0;
	queryCount=0;CM_BoxLeafnums_r(&ll,-2);Check(queryCount==1 && queryLeaves[0]==1,"direct leaf query");
	if(count==3) { queryCount=0;CM_BoxLeafnums_r(&ll,1);expectedCount=0;QueryOracle(&ll,1,expected,&expectedCount);Check(queryCount==expectedCount && !memcmp(queryLeaves,expected,queryCount*sizeof(int)),"query stops at requested subtree root"); }
}
static void MarkQueries(int count) {
	mnode_t *front=s_worldData.nodes+count+1,*back=s_worldData.nodes+count;
	mnode_t savedFront=*front,savedBack=*back;msurface_t surfaces[9],*frontMarks[8],*backMarks[3];shader_t shaders[9];srfSurfaceFace_t faces[9];
	surfaceType_t *list[5],*guard=(surfaceType_t *)&knownShader;vec3_t mins={-2,-1,-1},maxs={0,1,1},dir={0,0,-1};int i,capacity,length;
	memset(surfaces,0,sizeof(surfaces));memset(shaders,0,sizeof(shaders));memset(faces,0,sizeof(faces));
	for(i=0;i<9;i++) { faces[i].surfaceType=SF_GRID;surfaces[i].data=&faces[i].surfaceType;surfaces[i].shader=shaders+i;if(i<8)frontMarks[i]=surfaces+i; }
	faces[0].surfaceType=faces[2].surfaceType=faces[3].surfaceType=SF_FACE;
	faces[0].plane.normal[2]=faces[2].plane.normal[2]=1;faces[0].plane.type=faces[2].plane.type=2;faces[2].plane.dist=10;
	faces[3].plane.normal[0]=1;faces[3].plane.type=0;faces[3].plane.dist=-1;
	shaders[4].surfaceFlags=SURF_NOIMPACT;shaders[5].contentFlags=CONTENTS_FOG;faces[6].surfaceType=SF_TRIANGLES;shaders[7].surfaceFlags=SURF_NOMARKS;
	backMarks[0]=surfaces+1;backMarks[1]=surfaces+8;backMarks[2]=surfaces;
	front->firstmarksurface=frontMarks;front->nummarksurfaces=8;back->firstmarksurface=backMarks;back->nummarksurfaces=3;
	for(capacity=0;capacity<=3;capacity++) {
		for(i=0;i<9;i++)surfaces[i].viewCount=0;for(i=0;i<5;i++)list[i]=guard;tr.viewCount=502;length=0;
		R_BoxSurfaces_r(s_worldData.nodes,mins,maxs,list+1,capacity,&length,dir);
		Check(length==capacity && list[0]==guard && list[capacity+1]==guard,"actual deep mark-list capacity, exact prefix and guards");
		if(capacity>0)Check(list[1]==surfaces[0].data,"front-first face mark");
		if(capacity>1)Check(list[2]==surfaces[1].data,"front-first shared grid mark");
		if(capacity>2)Check(list[3]==surfaces[8].data,"back grid appended once after rejected/shared marks");
		if(capacity==0)for(i=0;i<9;i++)Check(!surfaces[i].viewCount,"zero-capacity query leaves surface view counters untouched");
		if(capacity==3)for(i=0;i<9;i++)Check(surfaces[i].viewCount==tr.viewCount,"native inclusion and rejection view counters");
	}
	for(i=0;i<9;i++)surfaces[i].viewCount=0;tr.viewCount=503;length=0;maxs[0]=-2;
	R_BoxSurfaces_r(s_worldData.nodes,mins,maxs,list,5,&length,dir);Check(length==2 && list[0]==surfaces[0].data && list[1]==surfaces[1].data,"front-only mark query retains plane/direction/shader filtering");
	for(i=0;i<9;i++)surfaces[i].viewCount=0;tr.viewCount=504;length=0;mins[0]=maxs[0]=0;
	R_BoxSurfaces_r(s_worldData.nodes,mins,maxs,list,5,&length,dir);Check(length==3 && list[0]==surfaces[1].data && list[1]==surfaces[8].data && list[2]==surfaces[0].data,"back-only mark query retains native ordering");
	for(i=0;i<9;i++)surfaces[i].viewCount=0;length=0;
	R_BoxSurfaces_r(back,mins,maxs,list,5,&length,dir);Check(length==3,"direct leaf mark query");
	*front=savedFront;*back=savedBack;
}
int main(void) {
	int i,j,code,n,valid,checksum;unsigned int offset;dheader_t header;const char *error;
	ri.Error=Com_Error;ri.Printf=Print;ri.FS_ReadFile=FS_ReadFile;ri.FS_FreeFile=FS_FreeFile;ri.Hunk_Alloc=RendererHunk;ri.Malloc=RendererMalloc;ri.Free=RendererFree;subdivisions.value=4;
	Build(3);NativeLoad(3,1,2);BoxQueries(3);MarkQueries(3);
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
		valid=BSP_ValidateTree(source,&header)==NULL;Check(valid==Expected() && !graphTemporary,"exhaustive small-graph acceptance matches independent oracle");if(valid)SmallQueries();
	}
	/* Long valid trees use bounded heap/linear parent initialization, with no depth cap. */
	FreeHunks();Build(4096);NativeLoad(4096,1,4095);BoxQueries(4096);MarkQueries(4096);
	Build(MAX_MAP_NODES+1);Header(&header);Check(!CM_ValidateBSPAllocations(&header,NULL) && !R_ValidateBSPAllocations(&header) && !BSP_ValidateTree(source,&header) && !graphTemporary,"raised compiler node budget remains supported by native storage");
	/* Retained collision state still answers its actual loaded deep tree after validation. */
	{ vec3_t point={-2,0,0};checksum=CM_PointLeafnum(point);Check(checksum==1,"deep loaded native point query retained"); }
	Check(graphAllocations==graphFrees && !graphTemporary && !fileAllocation,"all graph/input ownership released");FreeHunks();
	puts("BSP tree/forest topology, iterative box queries, exhaustive goldens and native ownership regressions passed (issue #45)");return 0;
}
