/* Issue #44: actual MD4 registration/conversion/skinning, with isolated unused GL entry points. */
#include "../code/game/q_shared.h"
#include "../code/qcommon/qcommon.h"
#include "../code/qcommon/qfiles.h"
#include "../code/renderer/tr_public.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Supply only GL-free renderer types used by tr_model.c; no loader implementation is mocked. */
#define TR_LOCAL_H
#define MAX_MOD_KNOWN 1024
#define LIGHTMAP_NONE -1
#define SF_MD3 6
#define SF_MD4 7
typedef struct { qboolean defaultShader; int index; } shader_t;
typedef struct { vec3_t bounds[2]; } bmodel_t;
typedef enum { MOD_BAD, MOD_BRUSH, MOD_MESH, MOD_MD4 } modtype_t;
typedef struct model_s {
	char name[MAX_QPATH]; modtype_t type; int index, dataSize; bmodel_t *bmodel;
	md3Header_t *md3[MD3_MAX_LODS]; md4Header_t *md4; int numLods;
} model_t;
static struct { int numModels; model_t *models[MAX_MOD_KNOWN]; int viewCluster; qboolean registered; model_t *currentModel; } tr;
static glconfig_t glConfig;
refimport_t ri;
static shader_t *R_FindShader( const char *, int, qboolean );
static void R_SyncRenderThread( void );
void R_Init( void ); void R_ClearFlares( void ); void RE_ClearScene( void );
void RE_StretchPic( float,float,float,float,float,float,float,float,qhandle_t );
#include "../code/renderer/tr_model.c"

typedef struct { struct { int frame, oldframe; float backlerp; } e; } trRefEntity_t;
static struct { trRefEntity_t *currentEntity; } backEnd;
static struct { int numIndexes, numVertexes, indexes[SHADER_MAX_INDEXES]; float xyz[SHADER_MAX_VERTEXES][4], normal[SHADER_MAX_VERTEXES][4], texCoords[SHADER_MAX_VERTEXES][2][2]; } tess;
static shader_t *R_GetShaderByHandle(qhandle_t);
static void R_AddDrawSurf(void *,shader_t *,int,qboolean);
static void RB_CheckOverflow(int,int);
#include "../code/renderer/tr_animation.c"

static byte source[4000000], saved[4000000];
static void *hunks[16], *fileAllocations[3], *filePointers[3];
static int hunkSizes[16], sourceSize, allocations, shaderCalls, warnings, draws, reads, frees, alignment, negativeLength, present[3];
static model_t defaultModel;
static shader_t knownShader={qfalse,17};
static void Check(int ok,const char *message) { if(!ok) { fprintf(stderr,"MD4 regression failed: %s\n",message); exit(1); } }
static void Word(int offset,unsigned int value) { int i; for(i=0;i<4;i++) source[offset+i]=value>>(8*i); }
static void Float(int offset,float value) { unsigned int word; memcpy(&word,&value,4); Word(offset,word); }
#define Put(offset,type,field,value) Word((offset)+offsetof(type,field),(value))

/** Create canonical variable-size frames/vertices and multiple disjoint LODs/surfaces. */
static void Build(int frames,int bones,int lods,int surfaces,int vertices,int triangles,int weights) {
	int i,j,k,l,s,start,position,frame,bone,vertex,lod;
	memset(source,0,sizeof(source));
	Check(sizeof(md4Header_t)==100 && sizeof(md4LOD_t)==12 && sizeof(md4Surface_t)==168 && offsetof(md4Frame_t,bones)==40 && offsetof(md4Vertex_t,weights)==24 && sizeof(md4Weight_t)==20,"disk sizes");
	Put(0,md4Header_t,ident,MD4_IDENT); Put(0,md4Header_t,version,MD4_VERSION);
	Put(0,md4Header_t,numFrames,frames); Put(0,md4Header_t,numBones,bones); Put(0,md4Header_t,numLODs,lods);
	position=100; Put(0,md4Header_t,ofsBoneNames,position); position+=bones*64;
	Put(0,md4Header_t,ofsFrames,position);
	for(i=0;i<frames;i++) {
		frame=position; for(j=0;j<3;j++) { Float(frame+j*4,-1); Float(frame+12+j*4,1); Float(frame+24+j*4,(float)j/4); }
		Float(frame+36,2); position+=40;
		for(j=0;j<bones;j++) {
			bone=position;
			for(k=0;k<3;k++) Float(bone+k*16+k*4,1);
			Float(bone+12,(float)(i*2+j*10)); position+=48;
		}
	}
	Put(0,md4Header_t,ofsLODs,position);
	for(l=0;l<lods;l++) {
		lod=position; position+=12; Put(lod,md4LOD_t,numSurfaces,surfaces); Put(lod,md4LOD_t,ofsSurfaces,12);
		for(s=0;s<surfaces;s++) {
			start=position; position+=168; Put(start,md4Surface_t,ident,MD4_IDENT);
			memcpy(source+start+offsetof(md4Surface_t,name),"PART_1",7); memcpy(source+start+offsetof(md4Surface_t,shader),"textures/test",14);
			Put(start,md4Surface_t,ofsHeader,0u-start); Put(start,md4Surface_t,numVerts,vertices); Put(start,md4Surface_t,numTriangles,triangles); Put(start,md4Surface_t,numBoneReferences,bones);
			Put(start,md4Surface_t,ofsVerts,position-start);
			for(j=0;j<vertices;j++) {
				vertex=position; Float(vertex+8,1); Float(vertex+12,-1); Float(vertex+16,0.5f); Put(vertex,md4Vertex_t,numWeights,weights); position+=24;
				for(k=0;k<weights;k++) {
					Word(position,(k+1)%bones); Float(position+4,1.0f/weights); Float(position+8,j+1); Float(position+12,0.5f); Float(position+16,2); position+=20;
				}
			}
			Put(start,md4Surface_t,ofsTriangles,position-start);
			for(j=0;j<triangles;j++) for(k=0;k<3;k++) { Word(position,k%vertices); position+=4; }
			Put(start,md4Surface_t,ofsBoneReferences,position-start);
			for(j=0;j<bones;j++) { Word(position,bones-1-j); position+=4; }
			Put(start,md4Surface_t,ofsEnd,position-start);
		}
		Put(lod,md4LOD_t,ofsEnd,position-lod);
	}
	sourceSize=position; Check(position<=sizeof(source),"fixture capacity"); Put(0,md4Header_t,ofsEnd,position); memcpy(saved,source,sourceSize);
}
static void *Allocate(int size,ha_pref preference) { void *p; Check(preference==h_low && size>0 && size<=sourceSize+4096 && allocations<16,"bounded hunk"); p=calloc(1,size); Check(p!=NULL,"host hunk"); hunks[allocations]=p; hunkSizes[allocations++]=size; return p; }
static int Read(const char *name,void **buffer) {
	int lod=strstr(name,"_2.md3")?2:strstr(name,"_1.md3")?1:0; reads++;
	if(!present[lod]) { *buffer=NULL; return -1; }
	Check(!fileAllocations[lod],"duplicate file read"); fileAllocations[lod]=malloc(sourceSize+alignment); Check(fileAllocations[lod]!=NULL,"exact FS input"); filePointers[lod]=(byte *)fileAllocations[lod]+alignment; memcpy(filePointers[lod],source,sourceSize); *buffer=filePointers[lod]; return negativeLength?-1:sourceSize;
}
static void FreeFile(void *p) { int i; for(i=0;i<3;i++) if(filePointers[i]==p) { free(fileAllocations[i]); fileAllocations[i]=filePointers[i]=NULL; frees++; return; } Check(0,"unowned FS free"); }
static void QDECL Print(int level,const char *format,...) { (void)format; Check(level==PRINT_WARNING || level==PRINT_ALL,"diagnostic category"); if(level==PRINT_WARNING) warnings++; }
static void QDECL Error(int level,const char *format,...) { (void)level; (void)format; Check(0,"unexpected native error"); }
void QDECL Com_Error(int level,const char *format,...) { (void)level; (void)format; Check(0,"unexpected shared error"); }
void QDECL Com_Printf(const char *format,...) { (void)format; }
#ifndef Com_Memcpy
void Com_Memcpy(void *out,const void *in,size_t size) { memcpy(out,in,size); }
#endif
static shader_t *R_FindShader(const char *name,int lightmap,qboolean mipmap) { Check(!strcmp(name,"textures/test") && lightmap==LIGHTMAP_NONE && mipmap,"shader registration"); shaderCalls++; return &knownShader; }
static shader_t *R_GetShaderByHandle(qhandle_t handle) { Check(handle==17,"native shader handle"); return &knownShader; }
static void R_AddDrawSurf(void *surface,shader_t *shader,int fog,qboolean dlight) { Check(surface && shader==&knownShader && !fog && !dlight,"actual animation draw submission"); draws++; }
static void RB_CheckOverflow(int vertices,int indexes) { Check(vertices+tess.numVertexes<SHADER_MAX_VERTEXES && indexes+tess.numIndexes<SHADER_MAX_INDEXES,"actual native tess limits"); }
static void R_SyncRenderThread(void) {}
static void Reset(void) {
	int i; for(i=0;i<3;i++) Check(!fileAllocations[i],"leaked FS input");
	for(i=0;i<allocations;i++) { free(hunks[i]); hunks[i]=NULL; }
	allocations=shaderCalls=warnings=draws=reads=frees=negativeLength=0; memset(&tr,0,sizeof(tr)); memset(&tess,0,sizeof(tess)); memset(&defaultModel,0,sizeof(defaultModel)); tr.numModels=1; tr.models[0]=&defaultModel; memset(present,0,sizeof(present));
}
static int Direct(int length,model_t *model) { byte *p=malloc(length+alignment?length+alignment:1),*input=p+alignment; int result; Check(p!=NULL,"exact input"); memcpy(input,source,length); result=R_LoadMD4(model,input,length,"test.md4"); Check(!memcmp(input,source,length),"FS input changed"); free(p); return result; }
static void Reject(void) { model_t model,before; Reset(); memset(&model,0,sizeof(model)); model.type=MOD_BRUSH; before=model; Check(!Direct(sourceSize,&model) && !allocations && !shaderCalls && !memcmp(&model,&before,sizeof(model)),"invalid model allocated or changed state"); }

/** Check actual native conversion, including fields omitted by the old big-endian loader. */
static void Golden(int frames,int bones,int lods,int surfaces,int vertices,int triangles,int weights) {
	model_t model; md4Header_t *header; md4LOD_t *lod; md4Surface_t *surface; md4Frame_t *frame; md4Bone_t *bone; md4Vertex_t *vertex; md4Weight_t *weight; md4Triangle_t *triangle; int *references; int i,j,k;
	Reset(); memset(&model,0,sizeof(model)); Check(Direct(sourceSize,&model) && allocations==1 && hunkSizes[0]==sourceSize && model.type==MOD_MD4 && model.dataSize==sourceSize,"valid native copy");
	header=model.md4; Check(header->numFrames==frames && header->numBones==bones && header->numLODs==lods && header->ofsBoneNames==100,"native header");
	frame=(md4Frame_t *)((byte *)header+header->ofsFrames); Check(frame->radius==2 && frame->bounds[0][0]==-1 && frame->localOrigin[2]==0.5f,"native frame");
	bone=(md4Bone_t *)((byte *)frame+40); if(bones) Check(bone[bones-1].matrix[2][2]==1 && bone[bones-1].matrix[0][3]==(bones-1)*10,"native bone matrix");
	lod=(md4LOD_t *)((byte *)header+header->ofsLODs);
	for(i=0;i<lods;i++) {
		Check(lod->numSurfaces==surfaces && lod->ofsSurfaces==12 && lod->ofsEnd>=12,"native LOD"); surface=(md4Surface_t *)((byte *)lod+lod->ofsSurfaces);
		for(j=0;j<surfaces;j++) {
			Check(surface->ident==SF_MD4 && !strcmp(surface->name,"part_1") && surface->shaderIndex==17 && surface->numVerts==vertices && surface->numTriangles==triangles && surface->numBoneReferences==bones && (byte *)surface+surface->ofsHeader==(byte *)header,"native surface/back-reference");
			vertex=(md4Vertex_t *)((byte *)surface+surface->ofsVerts); if(vertices) { Check(vertex->numWeights==weights && vertex->normal[2]==1 && vertex->texCoords[0]==-1,"native variable vertex"); weight=(md4Weight_t *)((byte *)vertex+24); for(k=0;k<weights;k++) Check(weight[k].boneIndex==(k+1)%bones && weight[k].boneWeight==1.0f/weights && weight[k].offset[0]==1,"native weight"); }
			triangle=(md4Triangle_t *)((byte *)surface+surface->ofsTriangles); if(triangles) Check(triangle[triangles-1].indexes[2]==2%vertices,"native triangle");
			references=(int *)((byte *)surface+surface->ofsBoneReferences); for(k=0;k<bones;k++) Check(references[k]==bones-1-k,"native bone references");
			surface=(md4Surface_t *)((byte *)surface+surface->ofsEnd);
		}
		lod=(md4LOD_t *)((byte *)lod+lod->ofsEnd);
	}
	Check(shaderCalls==lods*surfaces,"shader count");
	if(frames==2 && bones==2 && vertices==3 && triangles==1 && weights==1) {
		trRefEntity_t entity; tr.currentModel=&model; entity.e.frame=1; entity.e.oldframe=0; entity.e.backlerp=0.5f; backEnd.currentEntity=&entity;
		R_AddAnimSurfaces(&entity); Check(draws==surfaces,"actual first LOD traversal"); surface=(md4Surface_t *)((byte *)header+header->ofsLODs+12);
		for(i=0;i<3;i++) {
			memset(&tess,0,sizeof(tess)); tess.numVertexes=5; tess.numIndexes=3; tess.indexes[0]=1234;
			if(i==1) { entity.e.frame=INT_MIN; entity.e.oldframe=INT_MAX; }
			if(i==2) { entity.e.frame=entity.e.oldframe=INT_MAX; }
			RB_SurfaceAnim(surface); Check(tess.numVertexes==8 && tess.numIndexes==6 && tess.indexes[0]==1234 && tess.indexes[3]==5 && tess.indexes[5]==7,"append indexes use the vertex base");
			for(j=0;j<3;j++) Check(tess.xyz[5+j][0]==(i==2?13:12)+j && tess.xyz[5+j][1]==0.5f && tess.xyz[5+j][2]==2 && tess.normal[5+j][2]==1 && tess.texCoords[5+j][0][0]==-1,"native global-bone skinning/interpolation and bounded frames");
		}
	}
}

int main(void) {
	int i,j,baseline,lod,surface,secondLod,lastSurface,vertex,weight,triangle,references,frame;
	model_t model;
	const unsigned int bad[]={0x80000000u,0xffffffffu,0x7fffffffu};
	const size_t headerFields[]={offsetof(md4Header_t,numFrames),offsetof(md4Header_t,numBones),offsetof(md4Header_t,numLODs),offsetof(md4Header_t,ofsBoneNames),offsetof(md4Header_t,ofsFrames),offsetof(md4Header_t,ofsLODs),offsetof(md4Header_t,ofsEnd)};
	const size_t lodFields[]={offsetof(md4LOD_t,numSurfaces),offsetof(md4LOD_t,ofsSurfaces),offsetof(md4LOD_t,ofsEnd)};
	const size_t surfaceFields[]={offsetof(md4Surface_t,ofsHeader),offsetof(md4Surface_t,numVerts),offsetof(md4Surface_t,ofsVerts),offsetof(md4Surface_t,numTriangles),offsetof(md4Surface_t,ofsTriangles),offsetof(md4Surface_t,numBoneReferences),offsetof(md4Surface_t,ofsBoneReferences),offsetof(md4Surface_t,ofsEnd)};
	ri.Hunk_Alloc=Allocate; ri.FS_ReadFile=Read; ri.FS_FreeFile=FreeFile; ri.Printf=Print; ri.Error=Error;
	Build(2,2,2,2,3,1,1); baseline=sourceSize;
	for(alignment=0;alignment<4;alignment++) {
		Golden(2,2,2,2,3,1,1);
		for(i=0;i<baseline;i++) { Reset(); memset(&model,0,sizeof(model)); Check(!Direct(i,&model) && !allocations && !shaderCalls && !model.md4,"prefix allocated/published"); }
	}
	alignment=0;
	for(i=0;i<sizeof(headerFields)/sizeof(headerFields[0]);i++) for(j=0;j<3;j++) { memcpy(source,saved,baseline); Word(headerFields[i],bad[j]); Reject(); }
	memcpy(source,saved,baseline); lod=R_MODEL_FIELD(source,md4Header_t,ofsLODs); surface=lod+12;
	for(i=0;i<sizeof(lodFields)/sizeof(lodFields[0]);i++) for(j=0;j<3;j++) { memcpy(source,saved,baseline); Word(lod+lodFields[i],bad[j]); Reject(); }
	for(i=0;i<sizeof(surfaceFields)/sizeof(surfaceFields[0]);i++) for(j=0;j<3;j++) { memcpy(source,saved,baseline); Word(surface+surfaceFields[i],bad[j]); Reject(); }
	memcpy(source,saved,baseline); secondLod=lod+R_MODEL_FIELD(source+lod,md4LOD_t,ofsEnd); lastSurface=secondLod+12; lastSurface+=R_MODEL_FIELD(source+lastSurface,md4Surface_t,ofsEnd);
	for(i=0;i<3;i++) { memcpy(source,saved,baseline); if(i<2) { Put(i==0?lod:secondLod,md4LOD_t,ofsEnd,0); } else { Put(lastSurface,md4Surface_t,ofsEnd,0); } Reject(); }
	memcpy(source,saved,baseline); vertex=surface+R_MODEL_FIELD(source+surface,md4Surface_t,ofsVerts); weight=vertex+24; triangle=surface+R_MODEL_FIELD(source+surface,md4Surface_t,ofsTriangles); references=surface+R_MODEL_FIELD(source+surface,md4Surface_t,ofsBoneReferences); frame=R_MODEL_FIELD(source,md4Header_t,ofsFrames);
	for(j=0;j<3;j++) { memcpy(source,saved,baseline); Put(vertex,md4Vertex_t,numWeights,bad[j]); Reject(); }
	memcpy(source,saved,baseline); Put(weight,md4Weight_t,boneIndex,2); Reject();
	memcpy(source,saved,baseline); Word(references,2); Reject();
	memcpy(source,saved,baseline); Word(triangle+4,3); Reject();
	memcpy(source,saved,baseline); Word(weight+4,0x7fc00000u); Reject();
	memcpy(source,saved,baseline); Word(weight+12,0x7f800000u); Reject();
	memcpy(source,saved,baseline); Word(vertex,0xff800000u); Reject();
	memcpy(source,saved,baseline); Float(frame+36,-1); Reject();
	memcpy(source,saved,baseline); Float(frame,2); Reject();
	memcpy(source,saved,baseline); Word(frame+40,0x7f800000u); Reject();
	memcpy(source,saved,baseline); memset(source+surface+4,'x',64); Reject();
	memcpy(source,saved,baseline); memset(source+surface+68,'x',64); Reject();
	memcpy(source,saved,baseline); Put(surface,md4Surface_t,ofsHeader,0); Reject();
	memcpy(source,saved,baseline); Put(surface,md4Surface_t,ofsTriangles,vertex-surface); Reject();
	memcpy(source,saved,baseline); Put(surface,md4Surface_t,ofsVerts,169); Reject();
	memcpy(source,saved,baseline); Put(0,md4Header_t,ofsLODs,frame); Reject();
	memcpy(source,saved,baseline); Put(0,md4Header_t,ofsBoneNames,frame); Reject();
	memcpy(source,saved,baseline); Put(surface,md4Surface_t,numVerts,1000); Reject();
	memcpy(source,saved,baseline); Put(surface,md4Surface_t,numTriangles,2000); Reject();
	memcpy(source,saved,baseline); Put(0,md4Header_t,numBones,129); Reject();
	memcpy(source,saved,baseline); Put(0,md4Header_t,numLODs,0); Reject();
	memcpy(source,saved,baseline); Put(0,md4Header_t,numFrames,0); Reject();
	memcpy(source,saved,baseline); Put(0,md4Header_t,version,99); Reject();
	memcpy(source,saved,baseline); Put(lastSurface+R_MODEL_FIELD(source+lastSurface,md4Surface_t,ofsVerts)+24,md4Weight_t,boneIndex,99); Reject();
	memcpy(source,saved,baseline); Reset(); memset(&model,0,sizeof(model)); model.dataSize=INT_MAX-baseline+1; Check(!Direct(baseline,&model) && !allocations,"aggregate allocation overflow");
	memcpy(source,saved,baseline); Reset(); memset(&model,0,sizeof(model)); Check(Direct(baseline+7,&model) && hunkSizes[0]==baseline,"trailing metadata");
	memcpy(source,saved,baseline); Reset(); present[0]=present[2]=1; Check(RE_RegisterModel("test.md4")==1 && allocations==2 && frees==2 && tr.models[1]->type==MOD_MD4 && !tr.models[1]->md3[0],"actual base MD4 registration/optional cleanup");
	memcpy(source,saved,baseline); Put(lastSurface,md4Surface_t,ofsEnd,0); Reset(); present[0]=present[2]=1; Check(!RE_RegisterModel("test.md4") && allocations==1 && frees==2 && !shaderCalls && !tr.models[1]->md4 && !tr.models[1]->dataSize,"failed registration has no partial payload"); i=reads; Check(!RE_RegisterModel("test.md4") && reads==i,"failed cache reused");
	memcpy(source,saved,baseline); Reset(); present[0]=1; negativeLength=1; Check(!RE_RegisterModel("test.md4") && allocations==1 && frees==1,"negative FS length cleanup");
	Build(1,128,1,1,999,1999,128); Golden(1,128,1,1,999,1999,128);
	Build(2048,2,4,1,3,1,2); Golden(2048,2,4,1,3,1,2);
	Build(1,2,1,40,0,0,0); Golden(1,2,1,40,0,0,0);
	Build(1,0,1,1,3,1,0); Golden(1,0,1,1,3,1,0);
	Build(1,2,1,1,3,1,1); Put(0,md4Header_t,ofsBoneNames,0); Reset(); memset(&model,0,sizeof(model)); Check(Direct(sourceSize,&model),"absent unused bone names");
	Reset(); puts("MD4 layout, native conversion, registration and skinning regressions passed (issue #44)"); return 0;
}
