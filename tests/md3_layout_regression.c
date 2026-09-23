/* Issue #44: actual renderer registration/MD3 conversion, with isolated unused GL entry points. */
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
static struct { int numModels; model_t *models[MAX_MOD_KNOWN]; int viewCluster; qboolean registered; } tr;
static glconfig_t glConfig;
refimport_t ri;
static shader_t *R_FindShader( const char *, int, qboolean );
static void R_SyncRenderThread( void );
void R_Init( void ); void R_ClearFlares( void ); void RE_ClearScene( void );
void RE_StretchPic( float,float,float,float,float,float,float,float,qhandle_t );
#include "../code/renderer/tr_model.c"

static byte source[17000000], saved[17000000], *fileAllocations[3], *filePointers[3];
static void *hunks[16]; static int hunkSizes[16];
static int sourceSize, reads, frees, allocations, shaderCalls, warnings, alignment, present[3], sizes[3], negativeLength;
static model_t defaultModel;
static shader_t knownShader={qfalse,17};

/** Fail on any incorrect file, allocation, native field or ownership result. */
static void Check( int ok, const char *message ) { if(!ok) { fprintf(stderr,"MD3 regression failed: %s\n",message); exit(1); } }
/** Write little-endian words/floats without native-casting fixture input. */
static void Word( int offset, unsigned int value ) { int i; for(i=0;i<4;i++) source[offset+i]=value>>(8*i); }
static void Float( int offset, float value ) { unsigned int word; memcpy(&word,&value,4); Word(offset,word); }
#define Put(offset,type,field,value) Word((offset)+offsetof(type,field),(value))

/** Construct disjoint canonical arrays and useful native pixel/triangle/tag goldens. */
static void Build( int frames, int tags, int surfaces, int vertices, int triangles, int shaders ) {
	int i,j,k,position,surface,start;
	memset(source,0,sizeof(source));
	Check(sizeof(md3Header_t)==108 && sizeof(md3Surface_t)==108 && sizeof(md3Frame_t)==56 && sizeof(md3Tag_t)==112,"disk structure sizes");
	Put(0,md3Header_t,ident,MD3_IDENT); Put(0,md3Header_t,version,MD3_VERSION);
	Put(0,md3Header_t,numFrames,frames); Put(0,md3Header_t,numTags,tags); Put(0,md3Header_t,numSurfaces,surfaces);
	position=108; Put(0,md3Header_t,ofsFrames,position);
	for(i=0;i<frames;i++) {
		for(j=0;j<3;j++) { Float(position+j*4,-1); Float(position+12+j*4,1); Float(position+24+j*4,(float)j/4); }
		Float(position+36,2); position+=56;
	}
	Put(0,md3Header_t,ofsTags,position);
	for(i=0;i<frames*tags;i++) {
		memcpy(source+position,"tag_test",9); Float(position+64,1); Float(position+76,1); Float(position+92,1); Float(position+108,1); position+=112;
	}
	Put(0,md3Header_t,ofsSurfaces,position);
	for(surface=0;surface<surfaces;surface++) {
		start=position; position+=108; Put(start,md3Surface_t,ident,MD3_IDENT); memcpy(source+start+4,"PART_1",7);
		Put(start,md3Surface_t,numFrames,frames); Put(start,md3Surface_t,numShaders,shaders); Put(start,md3Surface_t,numVerts,vertices); Put(start,md3Surface_t,numTriangles,triangles);
		Put(start,md3Surface_t,ofsShaders,position-start);
		for(i=0;i<shaders;i++) { memcpy(source+position,"textures/test",14); Word(position+64,999); position+=68; }
		Put(start,md3Surface_t,ofsTriangles,position-start);
		for(i=0;i<triangles;i++) for(j=0;j<3;j++) { Word(position,j%vertices); position+=4; }
		Put(start,md3Surface_t,ofsSt,position-start);
		for(i=0;i<vertices;i++) { Float(position,-1); Float(position+4,0.5f); position+=8; }
		Put(start,md3Surface_t,ofsXyzNormals,position-start);
		for(i=0;i<vertices*frames;i++) {
			for(k=0;k<4;k++) { unsigned int value=k==3?0xface:(unsigned int)(i*4+k+1); source[position++]=value; source[position++]=value>>8; }
		}
		Put(start,md3Surface_t,ofsEnd,position-start);
	}
	sourceSize=position; Put(0,md3Header_t,ofsEnd,sourceSize); Check(sourceSize<sizeof(source),"fixture capacity"); memcpy(saved,source,sourceSize);
}

/** Track exact aligned hunk copies, including the harmless failed-model cache entry. */
static void *Allocate( int size, ha_pref preference ) {
	void *pointer; Check(preference==h_low && size>0 && size<=sourceSize+4096 && allocations<16,"bounded native allocation");
	pointer=calloc(1,size); Check(pointer!=NULL,"host hunk"); hunks[allocations]=pointer; hunkSizes[allocations++]=size; return pointer;
}
static int Read( const char *name, void **buffer ) {
	int lod=strstr(name,"_2.md3")?2:strstr(name,"_1.md3")?1:0; reads++;
	if(!present[lod]) { *buffer=NULL; return -1; }
	Check(!fileAllocations[lod],"duplicate staged read"); fileAllocations[lod]=malloc(sizes[lod]+alignment ? sizes[lod]+alignment : 1); Check(fileAllocations[lod]!=NULL,"exact file allocation");
	filePointers[lod]=fileAllocations[lod]+alignment; memcpy(filePointers[lod],source,sizes[lod]);
	if(lod && present[lod]==2) { unsigned int one=1; byte *data=filePointers[lod]+offsetof(md3Header_t,numFrames); int i; for(i=0;i<4;i++) data[i]=one>>(8*i); }
	if(!lod && present[lod]==2) memset(filePointers[lod]+offsetof(md3Header_t,ident),0,4);
	if(!lod && present[lod]==3) {
		byte *surface=filePointers[lod]+R_MODEL_FIELD(filePointers[lod],md3Header_t,ofsSurfaces);
		byte *triangle=surface+R_MODEL_FIELD(surface,md3Surface_t,ofsTriangles); unsigned int index=R_MODEL_FIELD(surface,md3Surface_t,numVerts); int i;
		for(i=0;i<4;i++) triangle[i]=index>>(8*i);
	}
	if(present[lod]==4) {
		/* A native-loadable empty MD4 must not replace the requested base through an optional path. */
		const size_t fields[]={offsetof(md4Header_t,ident),offsetof(md4Header_t,version),offsetof(md4Header_t,numFrames),offsetof(md4Header_t,numBones),offsetof(md4Header_t,ofsFrames),offsetof(md4Header_t,numLODs),offsetof(md4Header_t,ofsLODs),offsetof(md4Header_t,ofsEnd)};
		const unsigned int values[]={MD4_IDENT,MD4_VERSION,1,1,100,1,188,200}; int i,j;
		memset(filePointers[lod],0,sizes[lod]);
		for(i=0;i<8;i++) for(j=0;j<4;j++) filePointers[lod][fields[i]+j]=values[i]>>(8*j);
		filePointers[lod][188+offsetof(md4LOD_t,ofsSurfaces)]=12; filePointers[lod][188+offsetof(md4LOD_t,ofsEnd)]=12;
	}
	*buffer=filePointers[lod]; return negativeLength?-1:sizes[lod];
}
static void FreeFile( void *pointer ) { int i; for(i=0;i<3;i++) if(filePointers[i]==pointer) { free(fileAllocations[i]); fileAllocations[i]=filePointers[i]=NULL; frees++; return; } Check(0,"unowned file free"); }
static void QDECL Print( int level, const char *format, ... ) { (void)format; Check(level==PRINT_WARNING || level==PRINT_ALL,"diagnostic category"); if(level==PRINT_WARNING) warnings++; }
static void QDECL Error( int level, const char *format, ... ) { (void)level; (void)format; Check(0,"unexpected native error after validation"); }
void QDECL Com_Error( int level, const char *format, ... ) { (void)level; (void)format; Check(0,"unexpected shared error"); }
void QDECL Com_Printf( const char *format, ... ) { (void)format; }
#ifndef Com_Memcpy
void Com_Memcpy( void *destination, const void *input, size_t count ) { memcpy(destination,input,count); }
#endif
static shader_t *R_FindShader( const char *name, int lightmap, qboolean mipmap ) { Check(!strcmp(name,"textures/test") && lightmap==LIGHTMAP_NONE && mipmap,"shader registration"); shaderCalls++; return &knownShader; }
static void R_SyncRenderThread( void ) {}
/** Release prior exact hunk copies and initialize an empty renderer model cache. */
static void Reset( void ) {
	int i; for(i=0;i<3;i++) Check(!fileAllocations[i],"leaked input");
	for(i=0;i<allocations;i++) { free(hunks[i]); hunks[i]=NULL; }
	reads=frees=allocations=shaderCalls=warnings=negativeLength=0; memset(&tr,0,sizeof(tr)); memset(&defaultModel,0,sizeof(defaultModel)); tr.numModels=1; tr.models[0]=&defaultModel;
	for(i=0;i<3;i++) { present[i]=0; sizes[i]=sourceSize; }
}
/** Execute the actual native loader, with an input end exactly at the advertised length. */
static int Direct( int length, int lod, model_t *model ) {
	byte *allocation=malloc(length+alignment ? length+alignment : 1), *input=allocation+alignment; int result;
	Check(allocation!=NULL,"exact direct input"); memcpy(input,source,length); result=R_LoadMD3(model,lod,input,length,"test.md3"); Check(!memcmp(input,source,length),"input changed during validation/conversion"); free(allocation); return result;
}
static void Reject( void ) { model_t model,before; Reset(); memset(&model,0,sizeof(model)); model.type=MOD_BRUSH; before=model; Check(!Direct(sourceSize,0,&model) && !allocations && !shaderCalls && !memcmp(&model,&before,sizeof(model)),"invalid model allocated or changed state"); }
/** Verify native copy metadata, names, shaders, indexes, floats and packed vertex normals. */
static void Golden( int frames, int tags, int surfaces, int vertices, int triangles, int shaders ) {
	model_t model; md3Header_t *header; md3Surface_t *surface; md3Frame_t *frame; md3Tag_t *tag; md3Triangle_t *triangle; md3St_t *st; md3XyzNormal_t *xyz; md3Shader_t *shader; int i,j;
	Reset(); memset(&model,0,sizeof(model)); Check(Direct(sourceSize,0,&model) && allocations==1 && hunkSizes[0]==sourceSize && model.type==MOD_MESH && model.dataSize==sourceSize,"valid native copy");
	header=model.md3[0]; Check(header->numFrames==frames && header->numTags==tags && header->numSurfaces==surfaces,"native header counts");
	frame=(md3Frame_t *)((byte *)header+header->ofsFrames); Check(frame->radius==2 && frame->bounds[0][0]==-1 && frame->bounds[1][2]==1 && frame->localOrigin[2]==0.5f,"native frame metadata");
	if(tags) { tag=(md3Tag_t *)((byte *)header+header->ofsTags); Check(!strcmp(tag->name,"tag_test") && tag->origin[0]==1 && tag->axis[0][0]==1 && tag->axis[1][1]==1 && tag->axis[2][2]==1,"native tag metadata"); }
	surface=(md3Surface_t *)((byte *)header+header->ofsSurfaces);
	for(i=0;i<surfaces;i++) {
		Check(surface->ident==SF_MD3 && !strcmp(surface->name,"part") && surface->numFrames==frames,"native surface/name");
		triangle=(md3Triangle_t *)((byte *)surface+surface->ofsTriangles); for(j=0;j<triangles;j++) Check(triangle[j].indexes[0]==0 && triangle[j].indexes[2]==2%vertices,"native triangle");
		st=(md3St_t *)((byte *)surface+surface->ofsSt); if(vertices) Check(st[vertices-1].st[0]==-1 && st[vertices-1].st[1]==0.5f,"native texture coordinates");
		xyz=(md3XyzNormal_t *)((byte *)surface+surface->ofsXyzNormals); if(vertices) Check(xyz[0].xyz[0]==1 && xyz[0].xyz[2]==3 && xyz[0].normal==(short)0xface,"native packed vertex");
		shader=(md3Shader_t *)((byte *)surface+surface->ofsShaders); for(j=0;j<shaders;j++) Check(shader[j].shaderIndex==17,"native shader index");
		surface=(md3Surface_t *)((byte *)surface+surface->ofsEnd);
	}
	Check(shaderCalls==surfaces*shaders,"shader count");
	if(tags) {
		orientation_t orientation; tr.models[1]=&model; tr.numModels=2;
		Check(R_LerpTag(&orientation,1,INT_MIN,INT_MAX,0.5f,"tag_test") && orientation.origin[0]==1 && orientation.axis[0][0]==1 && orientation.axis[2][2]==1,"bounded runtime tag frame indexes");
	}
}

/** Issue #244: q3data hand models are tag-only, with cleared bounds (mins 99999, maxs -99999). */
static void TagOnly( void ) {
	static const char *names[]={"tag_flash","tag_weapon","tag_barrel"};
	orientation_t orientation; vec3_t mins,maxs; md3Frame_t *frame; int i,j,frames,tag;
	Build(16,3,0,0,0,0); frames=R_MODEL_FIELD(source,md3Header_t,ofsFrames); tag=R_MODEL_FIELD(source,md3Header_t,ofsTags);
	for(i=0;i<16;i++) {
		for(j=0;j<3;j++) { Float(frames+i*56+j*4,99999); Float(frames+i*56+12+j*4,-99999); Float(frames+i*56+24+j*4,0); }
		Float(frames+i*56+36,173203.34375f);
		for(j=0;j<3;j++) { memset(source+tag,0,64); memcpy(source+tag,names[j],strlen(names[j])+1); Float(tag+64,(float)(i*10+j)); tag+=112; }
	}
	memcpy(saved,source,sourceSize);
	for(alignment=0;alignment<4;alignment++) {
		Reset(); present[0]=1; Check(RE_RegisterModel("shotgun_hand.md3")==1 && allocations==2 && frees==1 && !warnings && !shaderCalls && tr.models[1]->type==MOD_MESH && tr.models[1]->numLods==1 && !tr.models[1]->md3[0]->numSurfaces,"tag-only cleared-bounds registration");
		frame=(md3Frame_t *)((byte *)tr.models[1]->md3[0]+tr.models[1]->md3[0]->ofsFrames); Check(frame[15].bounds[0][0]==99999 && frame[15].bounds[1][2]==-99999 && frame[15].radius==173203.34375f,"native cleared bounds");
		R_ModelBounds(1,mins,maxs); Check(mins[1]==99999 && maxs[1]==-99999,"retail cleared model bounds");
		for(j=0;j<3;j++) Check(R_LerpTag(&orientation,1,15,15,0,names[j]) && orientation.origin[0]==150+j && orientation.axis[0][0]==1 && orientation.axis[1][1]==1 && orientation.axis[2][2]==1,"tag-only named tag");
		Check(R_LerpTag(&orientation,1,2,4,0.5f,"tag_weapon") && orientation.origin[0]==31 && !orientation.origin[1],"tag-only interpolated tag");
		Check(!R_LerpTag(&orientation,1,0,0,0,"tag_test") && !orientation.origin[0] && orientation.axis[0][0]==1,"tag-only missing tag");
	}
	alignment=0; tag=R_MODEL_FIELD(source,md3Header_t,ofsTags);
	memcpy(source,saved,sourceSize); Word(frames+15*56+12,0xff800000u); Reject();
	memcpy(source,saved,sourceSize); Word(frames+15*56,0x7f800000u); Reject();
	memcpy(source,saved,sourceSize); Word(frames+15*56+36,0x7fc00000u); Reject();
	memcpy(source,saved,sourceSize); Float(frames+15*56+36,-1); Reject();
	memcpy(source,saved,sourceSize); Word(tag+47*112+64,0x7f800000u); Reject();
}

/** Mutate signed offsets/counts and consumed serialized data after saving a canonical model. */
int main( void ) {
	int i,j,offset,surface,triangle,shader,st,tag,normal,baseline;
	model_t model; const unsigned int bad[]={0x80000000u,0xffffffffu,0x7fffffffu};
	const size_t headerFields[]={offsetof(md3Header_t,ofsFrames),offsetof(md3Header_t,ofsTags),offsetof(md3Header_t,ofsSurfaces),offsetof(md3Header_t,ofsEnd),offsetof(md3Header_t,numFrames),offsetof(md3Header_t,numTags),offsetof(md3Header_t,numSurfaces),offsetof(md3Header_t,numSkins)};
	const size_t surfaceFields[]={offsetof(md3Surface_t,numFrames),offsetof(md3Surface_t,numShaders),offsetof(md3Surface_t,numVerts),offsetof(md3Surface_t,numTriangles),offsetof(md3Surface_t,ofsTriangles),offsetof(md3Surface_t,ofsShaders),offsetof(md3Surface_t,ofsSt),offsetof(md3Surface_t,ofsXyzNormals),offsetof(md3Surface_t,ofsEnd)};
	ri.Hunk_Alloc=Allocate; ri.FS_ReadFile=Read; ri.FS_FreeFile=FreeFile; ri.Printf=Print; ri.Error=Error;
	Build(2,1,2,3,1,1); baseline=sourceSize;
	for(alignment=0;alignment<4;alignment++) {
		Golden(2,1,2,3,1,1);
		for(i=0;i<baseline;i++) { Reset(); memset(&model,0,sizeof(model)); Check(!Direct(i,0,&model) && !allocations && !shaderCalls && !model.md3[0],"truncation allocated/published"); }
	}
	alignment=0;
	for(i=0;i<sizeof(headerFields)/sizeof(headerFields[0]);i++) for(j=0;j<3;j++) { memcpy(source,saved,baseline); Word(headerFields[i],bad[j]); if(headerFields[i]==offsetof(md3Header_t,numSkins) && bad[j]==0x7fffffffu) continue; Reject(); }
	memcpy(source,saved,baseline); surface=R_MODEL_FIELD(source,md3Header_t,ofsSurfaces);
	for(i=0;i<sizeof(surfaceFields)/sizeof(surfaceFields[0]);i++) for(j=0;j<3;j++) { memcpy(source,saved,baseline); Word(surface+surfaceFields[i],bad[j]); Reject(); }
	memcpy(source,saved,baseline); Put(surface,md3Surface_t,ofsEnd,0); Reject();
	memcpy(source,saved,baseline); offset=surface+R_MODEL_FIELD(source+surface,md3Surface_t,ofsEnd); Put(offset,md3Surface_t,ofsEnd,0); Reject();
	memcpy(source,saved,baseline); Put(surface,md3Surface_t,numFrames,1); Reject();
	memcpy(source,saved,baseline); Put(surface,md3Surface_t,numVerts,SHADER_MAX_VERTEXES); Reject();
	memcpy(source,saved,baseline); Put(surface,md3Surface_t,numTriangles,SHADER_MAX_INDEXES/3); Reject();
	memcpy(source,saved,baseline); Put(0,md3Header_t,numFrames,0); Reject();
	memcpy(source,saved,baseline); Put(0,md3Header_t,version,14); Reject();
	memcpy(source,saved,baseline); Put(surface,md3Surface_t,ident,0); Reject();
	memcpy(source,saved,baseline); memset(source+surface+4,'x',64); Reject();
	memcpy(source,saved,baseline); tag=R_MODEL_FIELD(source,md3Header_t,ofsTags); memset(source+tag,'x',64); Reject();
	memcpy(source,saved,baseline); shader=surface+R_MODEL_FIELD(source+surface,md3Surface_t,ofsShaders); memset(source+shader,'x',64); Reject();
	memcpy(source,saved,baseline); triangle=surface+R_MODEL_FIELD(source+surface,md3Surface_t,ofsTriangles); Word(triangle+4,3); Reject();
	memcpy(source,saved,baseline); Word(triangle+4,0xffffffffu); Reject();
	memcpy(source,saved,baseline); st=R_MODEL_FIELD(source+surface,md3Surface_t,ofsSt); Put(surface,md3Surface_t,ofsTriangles,st); Reject();
	memcpy(source,saved,baseline); Put(0,md3Header_t,ofsTags,R_MODEL_FIELD(source,md3Header_t,ofsFrames)); Reject();
	memcpy(source,saved,baseline); Put(0,md3Header_t,ofsFrames,R_MODEL_FIELD(source,md3Header_t,ofsFrames)+1); Reject();
	memcpy(source,saved,baseline); normal=R_MODEL_FIELD(source+surface,md3Surface_t,ofsXyzNormals); Put(surface,md3Surface_t,ofsXyzNormals,normal+1); Reject();
	memcpy(source,saved,baseline); offset=R_MODEL_FIELD(source,md3Header_t,ofsFrames); Word(offset,0x7fc00000u); Reject();
	memcpy(source,saved,baseline); Float(offset+36,-1); Reject();
	memcpy(source,saved,baseline); Float(offset,2); Reset(); memset(&model,0,sizeof(model)); Check(Direct(baseline,0,&model) && allocations==1,"unordered frame bounds");
	memcpy(source,saved,baseline); Word(surface+st,0x7f800000u); Reject();
	memcpy(source,saved,baseline); Word(tag+64,0xff800000u); Reject();
	memcpy(source,saved,baseline); Reset(); memset(&model,0,sizeof(model)); model.dataSize=INT_MAX-baseline+1; Check(!Direct(baseline,0,&model) && !allocations,"aggregate size overflow");
	Reset(); memset(&model,0,sizeof(model)); Check(!Direct(baseline,-1,&model) && !allocations,"negative LOD"); Check(!Direct(baseline,3,&model) && !allocations,"oversized LOD");
	memcpy(source,saved,baseline); Reset(); memset(&model,0,sizeof(model)); Check(Direct(baseline+7,0,&model) && allocations==1 && hunkSizes[0]==baseline,"trailing metadata");
	Build(1,0,1,999,1999,256); Golden(1,0,1,999,1999,256);
	Build(1024,0,1,999,1999,0); Golden(1024,0,1,999,1999,0);
	Build(1024,16,0,0,0,0); Golden(1024,16,0,0,0,0);
	Build(1,0,32,0,0,0); Golden(1,0,32,0,0,0);
	TagOnly();
	Build(2,1,1,3,1,1);
	Reset(); present[0]=present[2]=1; Check(RE_RegisterModel("test.md3")==1 && allocations==3 && tr.models[1]->numLods==3 && tr.models[1]->md3[0] && tr.models[1]->md3[1]==tr.models[1]->md3[2] && frees==2,"staged LOD registration/fallback");
	Reset(); present[2]=1; Check(RE_RegisterModel("test.md3")==1 && allocations==2 && tr.models[1]->numLods==3 && tr.models[1]->md3[0]==tr.models[1]->md3[2] && frees==1,"coarse-only fallback");
	Reset(); present[0]=1; present[1]=2; Check(RE_RegisterModel("test.md3")==1 && allocations==2 && tr.models[1]->numLods==1 && frees==2 && warnings,"incompatible optional frame count");
	Reset(); present[0]=present[1]=1; sizes[1]=sourceSize-1; Check(RE_RegisterModel("test.md3")==1 && allocations==2 && tr.models[1]->numLods==1 && frees==2 && warnings,"malformed optional LOD fallback");
	Reset(); present[0]=present[1]=1; sizes[1]=3; Check(RE_RegisterModel("test.md3")==1 && allocations==2 && tr.models[1]->numLods==1 && frees==2 && warnings,"short optional LOD fallback");
	Reset(); present[0]=1; present[2]=4; Check(RE_RegisterModel("test.md3")==1 && allocations==2 && tr.models[1]->type==MOD_MESH && !tr.models[1]->md4 && frees==2 && warnings,"optional MD4 replaced requested base");
	Reset(); present[2]=4; Check(!RE_RegisterModel("test.md3") && allocations==1 && !tr.models[1]->dataSize && frees==1 && !shaderCalls,"optional MD4 loaded without requested base");
	Reset(); present[2]=1; present[0]=2; Check(!RE_RegisterModel("test.md3") && allocations==1 && !shaderCalls && !tr.models[1]->dataSize && !tr.models[1]->numLods && !tr.models[1]->md3[0] && !tr.models[1]->md3[2] && frees==2,"failed primary retained partial model");
	Reset(); present[2]=1; present[0]=3; Check(!RE_RegisterModel("test.md3") && allocations==1 && !shaderCalls && !tr.models[1]->dataSize && !tr.models[1]->md3[2] && frees==2,"late malformed primary retained partial payload");
	baseline=reads; Check(!RE_RegisterModel("test.md3") && reads==baseline && allocations==1,"failed model cache");
	for(i=0;i<4;i++) { Reset(); present[0]=1; sizes[0]=i; Check(!RE_RegisterModel("test.md3") && allocations==1 && frees==1 && !shaderCalls,"short identification cleanup"); }
	Reset(); present[0]=1; negativeLength=1; Check(!RE_RegisterModel("test.md3") && allocations==1 && frees==1,"negative file length");
	Reset(); Check(!RE_RegisterModel("missing.md3") && allocations==1 && reads==3 && !frees,"missing model");
	Reset(); Check(!RE_RegisterModel(NULL) && !RE_RegisterModel("") && !allocations && !reads,"invalid name");
	Reset(); tr.numModels=MAX_MOD_KNOWN; Check(!R_AllocModel() && !allocations,"model limit");
	Reset(); puts("MD3 layout, native conversion, LOD staging and ownership regressions passed (issue #44)"); return 0;
}
