/* Issue #45: shared BSP layout preflight and actual collision entry-point rejection. */
#include "../code/qcommon/cm_load.c"
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static byte source[24000000],saved[24000000];
static int sourceSize,readable,advertised,alignment,reads,frees,allocations,checksums,clears,floods,expectError,missing;
static void *hunks[64],*fileAllocation,*filePointer;
static int hunkSizes[64];
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
void *Hunk_Alloc(int size,ha_pref preference) { void *p; Check(size>=0 && size<100000 && preference==h_high && allocations<64,"bounded hunk"); p=calloc(1,size?size:1);Check(p!=NULL,"host hunk");hunks[allocations]=p;hunkSizes[allocations++]=size;return p; }
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
void CM_FloodAreaConnections(void) { floods++; }
struct patchCollide_s *CM_GeneratePatchCollide(int width,int height,vec3_t *points) { (void)width;(void)height;(void)points; Check(0,"unexpected patch fixture"); return NULL; }

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
static void BuildCM(void) {
	int position=sizeof(dheader_t),i,j,offset; Empty();
	Lump(LUMP_SHADERS,position,sizeof(dshader_t));memset(source+position,0,sizeof(dshader_t));memcpy(source+position,"textures/test",14);Word(position+68,1);position+=sizeof(dshader_t);
	Lump(LUMP_PLANES,position,12*sizeof(dplane_t));memset(source+position,0,12*sizeof(dplane_t));
	for(i=0;i<12;i++) { Float(position+i*16+((i/4)%3)*4,i%4<2?-1:1);Float(position+i*16+12,1); }position+=12*sizeof(dplane_t);
	Lump(LUMP_NODES,position,sizeof(dnode_t));memset(source+position,0,sizeof(dnode_t));Word(position+4,0xffffffffu);Word(position+8,0xffffffffu);position+=sizeof(dnode_t);
	Lump(LUMP_LEAFS,position,sizeof(dleaf_t));memset(source+position,0,sizeof(dleaf_t));Word(position+offsetof(dleaf_t,numLeafBrushes),1);position+=sizeof(dleaf_t);
	Lump(LUMP_LEAFBRUSHES,position,4);Word(position,0);position+=4;
	Lump(LUMP_MODELS,position,sizeof(dmodel_t));memset(source+position,0,sizeof(dmodel_t));for(j=0;j<3;j++) { Float(position+j*4,-1);Float(position+12+j*4,1); }Word(position+offsetof(dmodel_t,numBrushes),1);position+=sizeof(dmodel_t);
	Lump(LUMP_BRUSHES,position,sizeof(dbrush_t));memset(source+position,0,sizeof(dbrush_t));Word(position+4,6);position+=sizeof(dbrush_t);
	Lump(LUMP_BRUSHSIDES,position,6*sizeof(dbrushside_t));for(i=0;i<6;i++) { Word(position+i*8,i*2);Word(position+i*8+4,0); }position+=6*sizeof(dbrushside_t);
	offset=position;Lump(LUMP_ENTITIES,offset,3);memcpy(source+offset,"abc",3);position+=3;
	sourceSize=position;memcpy(saved,source,sourceSize);
}
int main(void) {
	int i,j,prefix,position,checksum,readBefore,freeBefore,allocateBefore,oldSize;
	dheader_t header,before; lump_t entity;
	const unsigned int strides[HEADER_LUMPS]={1,72,16,36,48,4,4,40,12,8,44,4,72,104,49152,8,1};
	const unsigned int caps[HEADER_LUMPS]={262144,1024,131072,131072,131072,131072,262144,1024,32768,131072,524288,524288,256,131072,8388608/49152,1048576,2097152};
	const unsigned int bad[]={0x80000000u,0xffffffffu,0x7fffffffu};
	Check(sizeof(dheader_t)==144,"disk header size");
	BuildCM();readable=advertised=sourceSize;CM_LoadMap("good.bsp",qfalse,&checksum);
	Check(frees==1 && checksums==1 && clears==1 && floods==1 && !strcmp(cm.name,"good.bsp") && cm.numLeafs==1 && cm.numSubModels==1 && cm.numBrushes==1 && cm.numClusters==1 && !strcmp(cm.entityString,"abc") && cm.numEntityChars==3,"native collision map golden");
	readBefore=reads;CM_LoadMap("good.bsp",qtrue,&i);Check(i==checksum && reads==readBefore,"native cached client checksum");
	for(alignment=0;alignment<4;alignment++) {
		Empty();position=144;
		for(i=0;i<HEADER_LUMPS;i++) { Lump(i,position,strides[i]); memset(source+position,0,strides[i]);if(i==LUMP_VISIBILITY) { Word(position,1);Word(position+4,1);Lump(i,position,9);position+=9; }else position+=strides[i];position=(position+3)&~3; }
		sourceSize=position;{ byte *p=malloc(sourceSize+alignment);Check(p!=NULL,"unaligned input");memcpy(p+alignment,source,sourceSize);Check(!BSP_ValidateHeader(p+alignment,sourceSize,&header) && header.ident==BSP_IDENT && header.version==46 && header.lumps[LUMP_SURFACES].filelen==104,"all-lump native header golden");free(p); }
		Empty();for(prefix=0;prefix<144;prefix++) { sourceSize=prefix;RejectHeader(); }sourceSize=144;
		Word(0,0);RejectHeader();Word(0,BSP_IDENT);Word(4,47);RejectHeader();Word(4,BSP_VERSION);
	}
	alignment=0;
	for(i=0;i<HEADER_LUMPS;i++) {
		for(j=0;j<3;j++) { Empty();Lump(i,bad[j],0);RejectHeader();Empty();Lump(i,144,bad[j]);RejectHeader(); }
		Empty();Lump(i,143,strides[i]);sourceSize=144+strides[i];RejectHeader();
		Empty();Lump(i,sourceSize,1);RejectHeader();
		if(strides[i]>1) { Empty();Lump(i,144,strides[i]-1);sourceSize+=strides[i];RejectHeader(); }
		if(i!=LUMP_ENTITIES && i!=LUMP_LIGHTMAPS && i!=LUMP_LIGHTGRID) { Empty();Lump(i,145,strides[i]);sourceSize=148+strides[i];RejectHeader(); }
		Empty();oldSize=(caps[i]+1)*strides[i];Lump(i,144,oldSize);sourceSize=144+oldSize;Check(sourceSize<=sizeof(source),"cap fixture capacity");RejectHeader();
	}
	Empty();Lump(LUMP_SHADERS,144,72);Lump(LUMP_FOGS,144,72);sourceSize=216;RejectHeader();
	for(i=1;i<8;i++) { Empty();Lump(LUMP_VISIBILITY,144,i);sourceSize=144+i;RejectHeader(); }
	Empty();Lump(LUMP_VISIBILITY,144,9);sourceSize=153;Word(144,1);Word(148,1);Check(!BSP_ValidateHeader(source,sourceSize,&header),"exact one-byte PVS");
	Word(148,0);RejectHeader();Word(148,2);RejectHeader();Word(148,1);Word(144,2);RejectHeader();
	Word(144,0xffffffffu);RejectHeader();Word(144,1);Word(148,0x80000000u);RejectHeader();
	Empty();Lump(LUMP_ENTITIES,144,1);sourceSize=145;source[144]='x';Check(!BSP_ValidateHeader(source,sourceSize,&header),"byte entity lump");
	Lump(LUMP_ENTITIES,0,0);Lump(LUMP_VISIBILITY,sourceSize,0);Check(!BSP_ValidateHeader(source,sourceSize,&header),"zero/end empty lumps");
	memset(&header,0xa5,sizeof(header));before=header;Check(BSP_ValidateHeader(NULL,144,&header) && BSP_ValidateHeader(source,-1,&header) && BSP_ValidateHeader(source,INT_MAX,&header) && BSP_ValidateHeader(source,144,NULL) && !memcmp(&header,&before,sizeof(header)),"invalid API lengths/output");
	previous=cm;readable=144;advertised=-1;missing=0;expectError=1;freeBefore=frees;
	if(!setjmp(errorJump)) { CM_LoadMap("negative.bsp",qfalse,&checksum);Check(0,"negative FS length accepted"); }expectError=0;Check(frees==freeBefore+1 && !memcmp(&cm,&previous,sizeof(cm)),"negative length cleanup/world retention");
	missing=1;expectError=1;freeBefore=frees;if(!setjmp(errorJump)) { CM_LoadMap("missing.bsp",qfalse,&checksum);Check(0,"missing input accepted"); }expectError=0;Check(frees==freeBefore && !memcmp(&cm,&previous,sizeof(cm)),"missing file retained world");missing=0;
	cmod_base=source;source[0]='x';source[1]='y';entity.fileofs=0;entity.filelen=2;allocateBefore=allocations;CMod_LoadEntityString(&entity);Check(hunkSizes[allocateBefore]==3 && !strcmp(cm.entityString,"xy"),"collision entity terminator");
	entity.filelen=0;allocateBefore=allocations;CMod_LoadEntityString(&entity);Check(hunkSizes[allocateBefore]==1 && !cm.entityString[0],"empty collision entity terminator");
	for(i=0;i<allocations;i++)free(hunks[i]);
	puts("BSP header, lump layout, visibility and collision ownership regressions passed (issue #45)");return 0;
}
