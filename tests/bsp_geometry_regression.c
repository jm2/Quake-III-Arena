/* Issue #45: actual collision/world rejection and native face/triangle/curve parsing. */
#include "bsp_fixture.h"
#define __QGL_H__
typedef unsigned int GLuint;
#define GL_CLAMP 0x2900
#include "../code/renderer/tr_bsp.c"
#include "../code/renderer/tr_curve.c"

trGlobals_t tr;
glconfig_t glConfig;
refimport_t ri;
static cvar_t rendererVariable,subdivisions;
cvar_t *r_vertexLight=&rendererVariable,*r_lightmap=&rendererVariable,*r_mapOverBrightBits=&rendererVariable;
cvar_t *r_singleShader=&rendererVariable,*r_fullbright=&rendererVariable,*r_subdivisions=&subdivisions;
static shader_t knownShader;
static model_t knownModel;
static world_t retainedWorld;
static trGlobals_t beforeTr;
static world_t beforeWorld;
static int shaderCalls,modelCalls,rendererHunks,heapCount,goldenSize;
static void *heap[16];
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
		Float(offset+i*sizeof(drawVert_t)+offsetof(drawVert_t,normal)+8,1);
		for(j=0;j<4;j++)source[offset+i*sizeof(drawVert_t)+offsetof(drawVert_t,color)+j]=255;
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
		for(i=0;i<vertices;i++)Check(face->points[i][0]==i && ((byte *)&face->points[i][7])[3]==255,"face points beyond old 64-point clamp and geometry alpha");
		for(i=0;i<indexes;i++)Check(((int *)((byte *)face+face->ofsIndices))[i]==i%vertices,"native face indices");
	} else if(type==MST_TRIANGLE_SOUP) {
		srfTriangles_t *tri;ParseTriSurf(surface,verts,&surf,ids);tri=(void *)surf.data;Check(tri->numVerts==vertices && tri->numIndexes==indexes && (void *)tri->verts==(void *)(tri+1) && tri->indexes==(int *)(tri->verts+vertices),"native triangle allocation layout");
		for(i=0;i<vertices;i++)Check(tri->verts[i].xyz[0]==i && tri->verts[i].color[3]==255,"native triangle vertices");
		for(i=0;i<indexes;i++)Check(tri->indexes[i]==i%vertices,"native triangle indices");
	} else {
		srfGridMesh_t *grid;ParseMesh(surface,verts,&surf);grid=(void *)surf.data;
		Check(grid->width>=2 && grid->height>=2 && grid->width<=MAX_GRID_SIZE && grid->height<=MAX_GRID_SIZE && grid->meshBounds[0][0]==-1 && grid->meshBounds[1][0]==width-2 && grid->meshBounds[1][1]==height-2,"actual native patch subdivision boundary and endpoints");
		for(i=0;i<grid->width*grid->height;i++)Check(isfinite(grid->verts[i].xyz[0]) && isfinite(grid->verts[i].normal[2]) && grid->verts[i].color[3]==255,"native subdivided geometry and alpha");
		R_FreeSurfaceGridMesh(grid);Check(!heapCount,"curve temporary ownership released");
	}
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
	Check(!heapCount && !fileAllocation,"all fixture ownership released");FreeHunks();puts("BSP finite geometry, native storage, renderer rejection and surface/curve regressions passed (issue #45)");return 0;
}
