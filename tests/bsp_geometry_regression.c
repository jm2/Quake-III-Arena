/* Issue #45: actual collision/world rejection and native face/triangle/curve parsing. */
#include "bsp_fixture.h"
#include <float.h>
#define __QGL_H__
typedef unsigned int GLuint;
#define GL_CLAMP 0x2900
#include "../code/renderer/tr_bsp.c"
void *RendererPatchMalloc(size_t size);
void RendererPatchFree(void *pointer);
#define malloc RendererPatchMalloc
#define free RendererPatchFree
#include "../code/renderer/tr_curve.c"
#undef malloc
#undef free

trGlobals_t tr;
glconfig_t glConfig;
refimport_t ri;
static cvar_t rendererVariable,subdivisions,mapOverbright;
static byte surfaceColor[4]={255,255,255,255},surfaceExpected[4]={255,255,255,255};
cvar_t *r_vertexLight=&rendererVariable,*r_lightmap=&rendererVariable,*r_mapOverBrightBits=&mapOverbright;
cvar_t *r_singleShader=&rendererVariable,*r_fullbright=&rendererVariable,*r_subdivisions=&subdivisions;
static int curveAmplitude,curveAxis;
static void *patchWorkspace;
static int failPatchWorkspace,patchAllocations,patchFrees;
static shader_t knownShader;
static model_t knownModel;
static world_t retainedWorld;
static trGlobals_t beforeTr;
static world_t beforeWorld;
static int shaderCalls,modelCalls,rendererHunks,heapCount,goldenSize;
static void *heap[16];
void *RendererPatchMalloc(size_t size) {
	Check(size==sizeof(patchGridWorkspace_t) && !patchWorkspace,"one exact renderer patch workspace");
	if(failPatchWorkspace) { failPatchWorkspace=0;return NULL; }
	patchWorkspace=malloc(size);Check(patchWorkspace!=NULL,"renderer preflight allocation");patchAllocations++;return patchWorkspace;
}
void RendererPatchFree(void *pointer) {
	Check(pointer && pointer==patchWorkspace,"owned renderer patch workspace release");free(pointer);patchWorkspace=NULL;patchFrees++;
}
static unsigned int At(int lump,int index,int stride) { return BSP_FileWord(source+8+lump*8)+index*stride; }
static unsigned int Append(int lump,int size) { unsigned int offset=(sourceSize+3)&~3u;memset(source+sourceSize,0,offset-sourceSize+size);Lump(lump,offset,size);sourceSize=offset+size;return offset; }
static void *RendererHunk(int size,ha_pref preference) { Check(preference==h_low,"native renderer hunk preference");rendererHunks++;return Hunk_Alloc(size,h_high); }
static void *RendererMalloc(int size) { void *p;Check(size>0 && size<1000000 && heapCount<16,"native curve temporary size");p=calloc(1,size);Check(p!=NULL,"curve allocation");heap[heapCount++]=p;return p; }
static void RendererFree(void *pointer) { int i;for(i=0;i<heapCount;i++)if(heap[i]==pointer) { free(pointer);heap[i]=heap[--heapCount];return; }Check(0,"unowned curve release"); }
shader_t *R_FindShader(const char *name,int lightmap,qboolean mipmap) { Check(!strcmp(name,"textures/test") && mipmap && lightmap>=-4,"native shader arguments");shaderCalls++;return &knownShader; }
model_t *R_AllocModel(void) { modelCalls++;return &knownModel; }
void R_SyncRenderThread(void) { }
void R_RemapShader(const char *oldName,const char *newName,const char *time) { (void)oldName;(void)newName;(void)time;Check(0,"unexpected geometry shader remap"); }
image_t *R_CreateImage(const char *name,const byte *pixels,int width,int height,qboolean mipmap,qboolean picmip,int wrap) { (void)name;(void)pixels;(void)width;(void)height;(void)mipmap;(void)picmip;(void)wrap;Check(0,"unexpected geometry texture upload");return NULL; }
static void QDECL Print(int level,const char *message,...) { (void)level;(void)message; }

static void Build(int type,int vertices,int indexes,int width,int height) {
	unsigned int offset;int i,j;
	BuildCM(2);offset=Append(LUMP_DRAWVERTS,vertices*sizeof(drawVert_t));
	for(i=0;i<vertices;i++) {
		Float(offset+i*sizeof(drawVert_t),width?(i%width)-1:i);Float(offset+i*sizeof(drawVert_t)+4,width?(i/width)-1:0);
		if(type==MST_PATCH && curveAmplitude)Float(offset+i*sizeof(drawVert_t)+8,((curveAxis==1?i%width:curveAxis==2?i/width:i%width+i/width)%2)?curveAmplitude:0);
		Float(offset+i*sizeof(drawVert_t)+offsetof(drawVert_t,normal)+8,1);
		for(j=0;j<4;j++)source[offset+i*sizeof(drawVert_t)+offsetof(drawVert_t,color)+j]=surfaceColor[j];
	}
	offset=Append(LUMP_DRAWINDEXES,indexes*4);for(i=0;i<indexes;i++)Word(offset+i*4,i%vertices);
	offset=Append(LUMP_SURFACES,sizeof(dsurface_t));Word(offset+offsetof(dsurface_t,fogNum),0xffffffffu);Word(offset+offsetof(dsurface_t,lightmapNum),0xffffffffu);Word(offset+offsetof(dsurface_t,surfaceType),type);Word(offset+offsetof(dsurface_t,numVerts),vertices);Word(offset+offsetof(dsurface_t,numIndexes),indexes);Word(offset+offsetof(dsurface_t,patchWidth),width);Word(offset+offsetof(dsurface_t,patchHeight),height);
	Float(offset+offsetof(dsurface_t,lightmapVecs)+8*4,1);
	if(type==MST_PATCH) { Float(offset+offsetof(dsurface_t,lightmapVecs),-1);Float(offset+offsetof(dsurface_t,lightmapVecs)+4,-1);Float(offset+offsetof(dsurface_t,lightmapVecs)+3*4,width-2);Float(offset+offsetof(dsurface_t,lightmapVecs)+4*4,height-2); }
	goldenSize=sourceSize;memcpy(saved,source,sourceSize);
}
static void Restore(void) { sourceSize=goldenSize;memcpy(source,saved,sourceSize); }
static void Header(dheader_t *h) { Check(!BSP_ValidateHeader(source,sourceSize,h) && !BSP_ValidateReferences(source,h),"geometry mutation retains valid layout/references"); }
static void RejectRenderer(void) {
	int readBefore=reads,freeBefore=frees,hunkBefore=rendererHunks,shadersBefore=shaderCalls,modelsBefore=modelCalls;
	beforeTr=tr;beforeWorld=s_worldData;readable=advertised=sourceSize;missing=0;expectError=1;
	if(!setjmp(errorJump)) { RE_LoadWorldMap("bad-geometry.bsp");Check(0,"renderer accepted bad geometry"); }
	expectError=0;
	Check(reads==readBefore+1 && frees==freeBefore+1 && rendererHunks==hunkBefore && shaderCalls==shadersBefore && modelCalls==modelsBefore && !fileAllocation && !heapCount && !memcmp(&tr,&beforeTr,sizeof(tr)) && !memcmp(&s_worldData,&beforeWorld,sizeof(s_worldData)),"renderer preflight changed map/shader/model/heap ownership");
}
static void RejectGeometry(qboolean collisionToo) {
	dheader_t h;int a;Header(&h);Check(R_ValidateBSPGeometry(source,&h)!=NULL,"renderer geometry preflight");
	if(collisionToo)Check(BSP_ValidateGeometry(source,&h,CM_MAX_PATCH_GRID_SIZE,MAX_PATCH_VERTS)!=NULL,"collision geometry preflight");
	for(a=0;a<4;a++) { alignment=a;if(collisionToo)RejectCM();RejectRenderer(); }alignment=0;
}
static void BadWord(unsigned int offset,unsigned int word,qboolean collisionToo) { Restore();Word(offset,word);RejectGeometry(collisionToo); }
static void NativeSurface(int type,int vertices,int indexes,int width,int height) {
	dheader_t h;msurface_t surf;dsurface_t *surface;drawVert_t *verts;int *ids;int i;
	Build(type,vertices,indexes,width,height);Header(&h);Check(!R_ValidateBSPGeometry(source,&h),"valid native storage boundary");
	surface=(void *)(source+h.lumps[LUMP_SURFACES].fileofs);verts=(void *)(source+h.lumps[LUMP_DRAWVERTS].fileofs);ids=(void *)(source+h.lumps[LUMP_DRAWINDEXES].fileofs);memset(&surf,0,sizeof(surf));
	s_worldData.numShaders=2;s_worldData.shaders=(void *)(source+h.lumps[LUMP_SHADERS].fileofs);
	if(type==MST_PLANAR) {
		srfSurfaceFace_t *face;ParseFace(surface,verts,&surf,ids);face=(void *)surf.data;
		Check(face->numPoints==vertices && face->numIndices==indexes && face->ofsIndices==(int)(offsetof(srfSurfaceFace_t,points)+vertices*sizeof(face->points[0])) && face->plane.normal[2]==1,"complete native face layout and plane");
		for(i=0;i<vertices;i++)Check(face->points[i][0]==i && !memcmp((byte *)&face->points[i][7],surfaceExpected,4),"face points beyond old 64-point clamp and geometry alpha");
		for(i=0;i<indexes;i++)Check(((int *)((byte *)face+face->ofsIndices))[i]==i%vertices,"native face indices");
	} else if(type==MST_TRIANGLE_SOUP) {
		srfTriangles_t *tri;ParseTriSurf(surface,verts,&surf,ids);tri=(void *)surf.data;Check(tri->numVerts==vertices && tri->numIndexes==indexes && (void *)tri->verts==(void *)(tri+1) && tri->indexes==(int *)(tri->verts+vertices),"native triangle allocation layout");
		for(i=0;i<vertices;i++)Check(tri->verts[i].xyz[0]==i && !memcmp(tri->verts[i].color,surfaceExpected,4),"native triangle vertices");
		for(i=0;i<indexes;i++)Check(tri->indexes[i]==i%vertices,"native triangle indices");
	} else {
		srfGridMesh_t *grid;ParseMesh(surface,verts,&surf);grid=(void *)surf.data;
		Check(grid->width>=2 && grid->height>=2 && grid->width<=MAX_GRID_SIZE && grid->height<=MAX_GRID_SIZE && grid->meshBounds[0][0]==-1 && grid->meshBounds[1][0]==width-2 && grid->meshBounds[1][1]==height-2,"actual native patch subdivision boundary and endpoints");
		for(i=0;i<grid->width*grid->height;i++)Check(isfinite(grid->verts[i].xyz[0]) && isfinite(grid->verts[i].normal[2]) && !memcmp(grid->verts[i].color,surfaceExpected,4),"native subdivided geometry and alpha");
		R_FreeSurfaceGridMesh(grid);Check(!heapCount,"curve temporary ownership released");
	}
}
static void NativeLighting(void) {
	static const byte expected[][4]={{18,10,6,173},{4,2,1,173},{255,141,85,173},{0,0,0,173}};
	int maps[]={1,0,INT_MAX,INT_MIN},hardware[]={0,1,0,2},i;
	surfaceColor[0]=9;surfaceColor[1]=5;surfaceColor[2]=3;surfaceColor[3]=173;
	for(i=0;i<4;i++) {
		mapOverbright.integer=maps[i];tr.overbrightBits=hardware[i];memcpy(surfaceExpected,expected[i],4);
		NativeSurface(MST_PLANAR,3,3,0,0);NativeSurface(MST_TRIANGLE_SOUP,3,3,0,0);NativeSurface(MST_PATCH,9,0,3,3);
		FreeHunks();
	}
	mapOverbright.integer=tr.overbrightBits=0;memset(surfaceColor,255,4);memset(surfaceExpected,255,4);
}
static unsigned int HashWords(unsigned int hash,const void *data,int size) {
	int i;const byte *bytes=data;
	for(i=0;i<size;i+=4) { unsigned int word;memcpy(&word,bytes+i,4);hash=(hash^word)*16777619u; }return hash;
}
static unsigned int CurveFingerprint(srfGridMesh_t *grid) {
	unsigned int hash=2166136261u;int i;
	hash=HashWords(hash,&grid->width,4);hash=HashWords(hash,&grid->height,4);
	hash=HashWords(hash,grid->meshBounds,sizeof(grid->meshBounds));hash=HashWords(hash,grid->localOrigin,sizeof(grid->localOrigin));hash=HashWords(hash,&grid->meshRadius,4);
	hash=HashWords(hash,grid->lodOrigin,sizeof(grid->lodOrigin));hash=HashWords(hash,&grid->lodRadius,4);
	hash=HashWords(hash,grid->widthLodError,grid->width*4);hash=HashWords(hash,grid->heightLodError,grid->height*4);
	for(i=0;i<grid->width*grid->height;i++)hash=HashWords(hash,&grid->verts[i],sizeof(drawVert_t));
	return hash;
}
static void CurveGoldens(void) {
	const int cases[][4]={{3,3,0,0},{31,31,0,0},{65,15,0,0},{15,65,0,0},{3,3,1,32},{3,3,2,32},{7,5,3,64},{5,7,3,64}};
	const unsigned int goldens[]={0xbe426bb9u,0xa4843a85u,0xfe9767ccu,0xf5df67ccu,0xc06f5cb2u,0x4f03cee4u,0x50b25eb8u,0xaf885cf6u};
	int i;
	for(i=0;i<8;i++) {
		dheader_t h;msurface_t surf;srfGridMesh_t *grid;
		curveAxis=cases[i][2];curveAmplitude=cases[i][3];Build(MST_PATCH,cases[i][0]*cases[i][1],0,cases[i][0],cases[i][1]);Header(&h);
		memset(&surf,0,sizeof(surf));s_worldData.numShaders=2;s_worldData.shaders=(void *)(source+h.lumps[LUMP_SHADERS].fileofs);
		ParseMesh((void *)(source+h.lumps[LUMP_SURFACES].fileofs),(void *)(source+h.lumps[LUMP_DRAWVERTS].fileofs),&surf);grid=(void *)surf.data;
		Check(CurveFingerprint(grid)==goldens[i],"unchanged native renderer vertices/normals/bounds/LOD/errors golden");
		R_FreeSurfaceGridMesh(grid);Check(!heapCount,"golden curve ownership");
	}
	curveAxis=curveAmplitude=0;
}
static void NumericCurves(void) {
	unsigned int offset;int mode,i,a;
	for(mode=0;mode<7;mode++) {
		curveAmplitude=mode==3 || mode==4?32:0;curveAxis=1;
		Build(MST_PATCH,9,0,3,3);offset=At(LUMP_DRAWVERTS,0,sizeof(drawVert_t));
		for(i=0;i<9;i++) {
			if(mode==0)Float(offset+i*sizeof(drawVert_t),FLT_MAX);
			if(mode==1) { Float(offset+i*sizeof(drawVert_t),(i%3-1)*1e20f);Float(offset+i*sizeof(drawVert_t)+4,(i/3-1)*1e20f); }
			if(mode==2)Float(offset+i*sizeof(drawVert_t),(i%3-1)*FLT_MAX);
			if(mode==3)Float(offset+i*sizeof(drawVert_t)+offsetof(drawVert_t,st),FLT_MAX);
			if(mode==4)Float(offset+i*sizeof(drawVert_t)+offsetof(drawVert_t,lightmap)+4,FLT_MAX);
		}
		if(mode==5) { Float(At(LUMP_SURFACES,0,sizeof(dsurface_t))+offsetof(dsurface_t,lightmapVecs),FLT_MAX);Float(At(LUMP_SURFACES,0,sizeof(dsurface_t))+offsetof(dsurface_t,lightmapVecs)+12,FLT_MAX); }
		if(mode==6) { Float(At(LUMP_SURFACES,0,sizeof(dsurface_t))+offsetof(dsurface_t,lightmapVecs),-1e20f);Float(At(LUMP_SURFACES,0,sizeof(dsurface_t))+offsetof(dsurface_t,lightmapVecs)+12,1e20f); }
		RejectGeometry(qfalse);
		Check(!patchWorkspace,"derived renderer failure releases native workspace");
	}
	curveAmplitude=curveAxis=0;
	Build(MST_PATCH,9,0,3,3);
	for(a=0;a<4;a++) { alignment=a;failPatchWorkspace=1;RejectRenderer();Check(!failPatchWorkspace && !patchWorkspace,"workspace failure retains renderer state and releases input"); }
	alignment=0;
}
static void NativeNodraw(void) {
	dheader_t h;msurface_t surf;int before=patchAllocations;
	Build(MST_PATCH,129*3,0,129,3);
	Word(At(LUMP_SHADERS,0,sizeof(dshader_t))+offsetof(dshader_t,surfaceFlags),SURF_NODRAW);
	Header(&h);failPatchWorkspace=1;
	Check(!R_ValidateBSPGeometry(source,&h) && failPatchWorkspace && patchAllocations==before,"unused nodraw controls bypass native renderer refinement capacity/workspace");
	failPatchWorkspace=0;s_worldData.numShaders=2;s_worldData.shaders=(void *)(source+h.lumps[LUMP_SHADERS].fileofs);memset(&surf,0,sizeof(surf));
	ParseMesh((void *)(source+h.lumps[LUMP_SURFACES].fileofs),(void *)(source+h.lumps[LUMP_DRAWVERTS].fileofs),&surf);
	Check(surf.data && *surf.data==SF_SKIP && !heapCount && !patchWorkspace,"actual native nodraw skip before control access/allocation");
}
int main(void) {
	unsigned int nonfinite[]={0x7f800000u,0xff800000u,0x7fc00001u},offset;int i,j,k,checksum;dheader_t h;
	ri.Error=Com_Error;ri.Printf=Print;ri.FS_ReadFile=FS_ReadFile;ri.FS_FreeFile=FS_FreeFile;ri.Hunk_Alloc=RendererHunk;ri.Malloc=RendererMalloc;ri.Free=RendererFree;subdivisions.value=4;
	tr.world=&retainedWorld;tr.sunDirection[0]=0.25f;tr.numModels=7;
	Build(MST_PATCH,9,0,3,3);expectedPatch=1;readable=advertised=sourceSize;CM_LoadMap("geometry.bsp",qfalse,&checksum);
	for(i=0;i<12;i++)for(j=0;j<4;j++)for(k=0;k<3;k++)BadWord(At(LUMP_PLANES,i,sizeof(dplane_t))+j*4,nonfinite[k],qtrue);
	for(j=0;j<6;j++)for(k=0;k<3;k++)BadWord(At(LUMP_MODELS,0,sizeof(dmodel_t))+j*4,nonfinite[k],qtrue);
	for(i=0;i<9;i++)for(j=0;j<10;j++)for(k=0;k<3;k++)BadWord(At(LUMP_DRAWVERTS,i,sizeof(drawVert_t))+j*4,nonfinite[k],qtrue);
	for(j=0;j<6;j++)for(k=0;k<3;k++)BadWord(At(LUMP_SURFACES,0,sizeof(dsurface_t))+offsetof(dsurface_t,lightmapVecs)+j*4,nonfinite[k],qtrue);
	offset=At(LUMP_SURFACES,0,sizeof(dsurface_t));
	for(i=0;i<3;i++) { unsigned int invalid[]={0,2,4};BadWord(offset+offsetof(dsurface_t,patchWidth),invalid[i],qtrue);BadWord(offset+offsetof(dsurface_t,patchHeight),invalid[i],qtrue); }
	BadWord(offset+offsetof(dsurface_t,patchWidth),0xffffffffu,qtrue);BadWord(offset+offsetof(dsurface_t,patchHeight),0x80000000u,qtrue);BadWord(offset+offsetof(dsurface_t,patchWidth),CM_MAX_PATCH_GRID_SIZE+2,qtrue);BadWord(offset+offsetof(dsurface_t,numVerts),8,qtrue);
	for(j=0;j<3;j++) { Restore();Float(At(LUMP_MODELS,0,sizeof(dmodel_t))+j*4,2);RejectGeometry(qtrue); }
	Build(MST_PATCH,201,0,67,3);RejectGeometry(qfalse);Header(&h);Check(!BSP_ValidateGeometry(source,&h,CM_MAX_PATCH_GRID_SIZE,MAX_PATCH_VERTS),"collision retains its larger native grid capacity");
	Build(MST_PATCH,1089,0,33,33);RejectGeometry(qtrue);
	Build(MST_PLANAR,1000,3,0,0);RejectGeometry(qfalse);Build(MST_TRIANGLE_SOUP,999,SHADER_MAX_INDEXES,0,0);RejectGeometry(qfalse);Build(MST_PLANAR,0,0,0,0);RejectGeometry(qfalse);
	Build(MST_PLANAR,3,3,0,0);for(j=6;j<9;j++)BadWord(At(LUMP_SURFACES,0,sizeof(dsurface_t))+offsetof(dsurface_t,lightmapVecs)+j*4,nonfinite[0],qtrue);
	Build(MST_FLARE,0,0,0,0);for(j=0;j<3;j++) { BadWord(At(LUMP_SURFACES,0,sizeof(dsurface_t))+offsetof(dsurface_t,lightmapOrigin)+j*4,nonfinite[0],qtrue);BadWord(At(LUMP_SURFACES,0,sizeof(dsurface_t))+offsetof(dsurface_t,lightmapVecs)+j*4,nonfinite[0],qtrue);BadWord(At(LUMP_SURFACES,0,sizeof(dsurface_t))+offsetof(dsurface_t,lightmapVecs)+(j+6)*4,nonfinite[0],qtrue); }
	NativeSurface(MST_PLANAR,999,5999,0,0);NativeSurface(MST_PLANAR,1,0,0,0);NativeSurface(MST_TRIANGLE_SOUP,999,5999,0,0);NativeSurface(MST_TRIANGLE_SOUP,0,0,0,0);
	NativeSurface(MST_PATCH,31*31,0,31,31);NativeSurface(MST_PATCH,65*15,0,65,15);
	Build(MST_PATCH,129*3,0,129,3);Header(&h);Check(!BSP_ValidateGeometry(source,&h,CM_MAX_PATCH_GRID_SIZE,MAX_PATCH_VERTS),"collision native 129-column boundary");
	CurveGoldens();NumericCurves();NativeNodraw();NativeLighting();Check(!heapCount && !fileAllocation && !patchWorkspace && patchAllocations==patchFrees,"all fixture ownership released");FreeHunks();puts("BSP finite geometry, native storage, renderer rejection and surface/curve regressions passed (issue #45)");return 0;
}
