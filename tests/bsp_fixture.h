#ifndef BSP_FIXTURE_H
#define BSP_FIXTURE_H
/* Issue #45: shared BSP layout preflight and actual collision entry-point rejection. */
#include "../code/qcommon/cm_load.c"
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static byte source[24000000],saved[24000000];
static int sourceSize,readable,advertised,alignment,reads,frees,allocations,checksums,clears,floods,expectError,missing;
static void *hunks[1024],*fileAllocation,*filePointer;
static byte *hunkArena;
static unsigned int hunkUsed;
static int expectedPatch,patchCalls,patchToken;
static int hunkSizes[1024];
static cvar_t variable;
static jmp_buf errorJump;
static clipMap_t previous;
static void Check(int ok,const char *message) { if(!ok) { fprintf(stderr,"BSP header regression failed: %s\n",message); exit(1); } }
static void Word(unsigned int offset,unsigned int value) { int i; for(i=0;i<4;i++) source[offset+i]=value>>(8*i); }
static void Float(unsigned int offset,float value) { unsigned int word; memcpy(&word,&value,4); Word(offset,word); }
static void Lump(int index,unsigned int offset,unsigned int size) { Word(8+index*8,offset); Word(12+index*8,size); }
static void Empty(void) { memset(source,0,sizeof(dheader_t)); Word(0,BSP_IDENT); Word(4,BSP_VERSION); sourceSize=sizeof(dheader_t); }

int FS_ReadFile(const char *name,void **buffer) {
	(void)name; reads++; Check(!fileAllocation,"leaked input before read");
	if(missing) { *buffer=NULL; return -1; }
	fileAllocation=malloc(readable+alignment?readable+alignment:1); Check(fileAllocation!=NULL,"exact file allocation"); filePointer=(byte *)fileAllocation+alignment; memcpy(filePointer,source,readable); *buffer=filePointer; return advertised;
}
void FS_FreeFile(void *pointer) { Check(pointer==filePointer && fileAllocation,"unowned file free"); free(fileAllocation);fileAllocation=filePointer=NULL;frees++; }
/* Match the engine's single arena so synthesized submodel offsets stay within it. */
void *Hunk_Alloc(int size,ha_pref preference) {
	void *p;unsigned int reserved;
	Check(size>=0 && size<1000000 && preference==h_high && allocations<1024,"bounded hunk");
	reserved=((unsigned int)size+31)&~31u;Check(reserved<=4000000u-hunkUsed,"fixture arena capacity");
	if(!hunkArena) { hunkArena=calloc(1,4000000);Check(hunkArena!=NULL,"fixture hunk arena"); }
	p=hunkArena+hunkUsed;hunkUsed+=reserved;hunks[allocations]=p;hunkSizes[allocations++]=size;return p;
}
static void FreeHunks(void) { free(hunkArena);hunkArena=NULL;hunkUsed=0;allocations=0; }

cvar_t *Cvar_Get(const char *name,const char *value,int flags) { (void)name;(void)value;(void)flags;return &variable; }
void QDECL Com_Error(int level,const char *format,...) { (void)format; Check(level==ERR_DROP && expectError && !fileAllocation,"controlled rejection after input cleanup"); longjmp(errorJump,1); }
void QDECL Com_Printf(const char *format,...) { (void)format; }
void QDECL Com_DPrintf(const char *format,...) { (void)format; }
#ifndef Com_Memcpy
void Com_Memcpy(void *out,const void *in,size_t size) { memcpy(out,in,size); }
#endif
#ifndef Com_Memset
void Com_Memset(void *out,int value,size_t size) { memset(out,value,size); }
#endif
unsigned Com_BlockChecksum(const void *input,int size) { const byte *data=input; unsigned hash=2166136261u; int i; Check(size>=0 && size==readable,"checksum after length validation");checksums++;for(i=0;i<size;i++)hash=(hash^data[i])*16777619u;return hash; }
void CM_ClearLevelPatches(void) { clears++; }
#ifndef BSP_FIXTURE_NATIVE_AREA_FLOOD
void CM_FloodAreaConnections(void) { floods++; }
#endif
struct patchCollide_s *CM_GeneratePatchCollide(int width,int height,vec3_t *points) {
	Check(expectedPatch && width==3 && height==3 && points[0][0]==-1 && points[8][0]==1,"validated patch callback");patchCalls++;return (struct patchCollide_s *)&patchToken;
}

/** Invalid layouts must not change map state, checksum, allocation or patch ownership. */
static void RejectCM(void) {
	int checksum=123456,readBefore=reads,freeBefore=frees,allocBefore=allocations,checksumBefore=checksums,clearBefore=clears;
	previous=cm; readable=sourceSize;advertised=sourceSize;missing=0;expectError=1;
	if(!setjmp(errorJump)) { CM_LoadMap("bad.bsp",qfalse,&checksum); Check(0,"invalid layout accepted"); }
	expectError=0;
	Check(reads==readBefore+1 && frees==freeBefore+1 && allocations==allocBefore && checksums==checksumBefore && clears==clearBefore && checksum==123456 && !memcmp(&cm,&previous,sizeof(cm)),"rejection lost world state/ownership");
}
static void RejectHeader(void) {
	dheader_t header,before; memset(&header,0xa5,sizeof(header));before=header;
	Check(BSP_ValidateHeader(source,sourceSize,&header)!=NULL && !memcmp(&header,&before,sizeof(header)),"invalid header published partial output");
	RejectCM();
}
/** A minimal native collision map exercises successful loading, checksum and entity termination. */
static void BuildCM(int shaders) {
	int position=sizeof(dheader_t),i,j,offset; Empty();
	Lump(LUMP_SHADERS,position,shaders*sizeof(dshader_t));memset(source+position,0,shaders*sizeof(dshader_t));for(i=0;i<shaders;i++) { memcpy(source+position+i*72,"textures/test",14);Word(position+i*72+68,1); }position+=shaders*sizeof(dshader_t);
	Lump(LUMP_PLANES,position,12*sizeof(dplane_t));memset(source+position,0,12*sizeof(dplane_t));
	for(i=0;i<12;i++) { Float(position+i*16+((i/4)%3)*4,(i%4==0 || i%4==3)?-1:1);Float(position+i*16+12,i%2?-1:1); }position+=12*sizeof(dplane_t);
	Lump(LUMP_NODES,position,sizeof(dnode_t));memset(source+position,0,sizeof(dnode_t));Word(position+4,0xffffffffu);Word(position+8,0xffffffffu);position+=sizeof(dnode_t);
	Lump(LUMP_LEAFS,position,sizeof(dleaf_t));memset(source+position,0,sizeof(dleaf_t));Word(position+offsetof(dleaf_t,numLeafBrushes),1);position+=sizeof(dleaf_t);
	Lump(LUMP_LEAFBRUSHES,position,4);Word(position,0);position+=4;
	Lump(LUMP_MODELS,position,sizeof(dmodel_t));memset(source+position,0,sizeof(dmodel_t));for(j=0;j<3;j++) { Float(position+j*4,-1);Float(position+12+j*4,1); }Word(position+offsetof(dmodel_t,numBrushes),1);position+=sizeof(dmodel_t);
	Lump(LUMP_BRUSHES,position,sizeof(dbrush_t));memset(source+position,0,sizeof(dbrush_t));Word(position+4,6);position+=sizeof(dbrush_t);
	Lump(LUMP_BRUSHSIDES,position,6*sizeof(dbrushside_t));for(i=0;i<6;i++) { Word(position+i*8,i*2);Word(position+i*8+4,0); }position+=6*sizeof(dbrushside_t);
	offset=position;Lump(LUMP_ENTITIES,offset,3);memcpy(source+offset,"abc",3);position+=3;
	sourceSize=position;memcpy(saved,source,sourceSize);
}

#endif
