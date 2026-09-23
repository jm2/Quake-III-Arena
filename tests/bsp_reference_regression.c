/* Issue #45: exact FS payloads through actual collision reference preflight. */
#include <stdlib.h>
static void *IndexCalloc(size_t,size_t);
#define calloc IndexCalloc
#include "bsp_fixture.h"
#undef calloc
static int failNextAllocation;
static void *IndexCalloc(size_t count,size_t size) { if(failNextAllocation) { failNextAllocation=0;return NULL; }return calloc(count,size); }

static int goldenSize;
static unsigned int At(int lump,int index,int stride) { return BSP_FileWord(source+8+lump*8)+index*stride; }
static unsigned int Append(int lump,int size) { unsigned int offset=(sourceSize+3)&~3u;memset(source+sourceSize,0,offset-sourceSize+size);Lump(lump,offset,size);sourceSize=offset+size;return offset; }
static void Restore(void) { sourceSize=goldenSize;memcpy(source,saved,sourceSize); }
static void ReferenceGolden(void) {
	unsigned int offset;int i,j;
	BuildCM(2);
	offset=Append(LUMP_DRAWVERTS,12*sizeof(drawVert_t));
	for(i=0;i<12;i++) { Float(offset+i*sizeof(drawVert_t),(i%3)-1);Float(offset+i*sizeof(drawVert_t)+4,(i/3)%3-1);Float(offset+i*sizeof(drawVert_t)+offsetof(drawVert_t,normal)+8,1); }
	offset=Append(LUMP_DRAWINDEXES,12);for(i=0;i<3;i++)Word(offset+i*4,i);
	offset=Append(LUMP_SURFACES,3*sizeof(dsurface_t));
	for(i=0;i<3;i++) { Word(offset+i*sizeof(dsurface_t)+offsetof(dsurface_t,fogNum),0xffffffffu);Word(offset+i*sizeof(dsurface_t)+offsetof(dsurface_t,lightmapNum),0xffffffffu); }
	Word(offset+offsetof(dsurface_t,surfaceType),MST_TRIANGLE_SOUP);Word(offset+offsetof(dsurface_t,numVerts),3);Word(offset+offsetof(dsurface_t,numIndexes),3);Word(offset+offsetof(dsurface_t,fogNum),0);
	offset+=sizeof(dsurface_t);Word(offset+offsetof(dsurface_t,surfaceType),MST_PATCH);Word(offset+offsetof(dsurface_t,firstVert),3);Word(offset+offsetof(dsurface_t,numVerts),9);Word(offset+offsetof(dsurface_t,patchWidth),3);Word(offset+offsetof(dsurface_t,patchHeight),3);
	/* Native patches ignore index-array fields, and flares ignore both geometry arrays. */
	Word(offset+offsetof(dsurface_t,firstIndex),0xffffffffu);Word(offset+offsetof(dsurface_t,numIndexes),0xffffffffu);
	offset+=sizeof(dsurface_t);Word(offset+offsetof(dsurface_t,surfaceType),MST_FLARE);Word(offset+offsetof(dsurface_t,firstVert),0xffffffffu);Word(offset+offsetof(dsurface_t,numVerts),0xffffffffu);Word(offset+offsetof(dsurface_t,firstIndex),0xffffffffu);Word(offset+offsetof(dsurface_t,numIndexes),0xffffffffu);
	offset=Append(LUMP_LEAFSURFACES,12);for(i=0;i<3;i++)Word(offset+i*4,i);Word(At(LUMP_LEAFS,0,sizeof(dleaf_t))+offsetof(dleaf_t,numLeafSurfaces),3);
	offset=Append(LUMP_MODELS,2*sizeof(dmodel_t));
	for(i=0;i<2;i++) { for(j=0;j<3;j++) { Float(offset+i*sizeof(dmodel_t)+j*4,-1);Float(offset+i*sizeof(dmodel_t)+12+j*4,1); }Word(offset+i*sizeof(dmodel_t)+offsetof(dmodel_t,numSurfaces),i?1:3);Word(offset+i*sizeof(dmodel_t)+offsetof(dmodel_t,numBrushes),1); }
	offset=Append(LUMP_FOGS,sizeof(dfog_t));memcpy(source+offset,"textures/fog",13);Word(offset+offsetof(dfog_t,visibleSide),0xffffffffu);
	offset=Append(LUMP_VISIBILITY,9);Word(offset,1);Word(offset+4,1);source[offset+8]=255;
	goldenSize=sourceSize;memcpy(saved,source,sourceSize);expectedPatch=1;
}
static void AcceptReferences(void) {
	dheader_t header;int a;
	Check(!BSP_ValidateHeader(source,sourceSize,&header) && !BSP_ValidateReferences(source,&header),"native reference golden");
	for(a=0;a<4;a++) { byte *copy=malloc(sourceSize+a);Check(copy!=NULL,"unaligned reference input");memcpy(copy+a,source,sourceSize);Check(!BSP_ValidateReferences(copy+a,&header),"reference golden at all alignments");free(copy); }
}
static void RejectReferences(void) {
	dheader_t header;int a;
	Check(!BSP_ValidateHeader(source,sourceSize,&header) && BSP_ValidateReferences(source,&header),"reference mutation retained valid layout and rejected payload");
	for(a=0;a<4;a++) { alignment=a;RejectCM(); }alignment=0;
}
static void BadWord(unsigned int offset,unsigned int value) { Restore();Word(offset,value);RejectReferences(); }
/* Issue #243: retail q3dm17 has no fogs, and q3map leaves its flares at fogNum 0. */
static void NoFogs(void) { Restore();Word(At(LUMP_SURFACES,0,sizeof(dsurface_t))+offsetof(dsurface_t,fogNum),0xffffffffu);Word(At(LUMP_SURFACES,2,sizeof(dsurface_t))+offsetof(dsurface_t,fogNum),0);Word(12+LUMP_FOGS*8,0); }
static void LoadGolden(const char *name) {
	int checksum=0,patchBefore=patchCalls;readable=advertised=sourceSize;alignment=0;missing=0;
	CM_LoadMap(name,qfalse,&checksum);Check(cm.numShaders==2 && cm.numSurfaces==3 && cm.surfaces[1] && patchCalls==patchBefore+1 && !fileAllocation,"actual valid collision materials and patch callback");
	Check(cm.cmodels[1].leaf.numLeafBrushes==1 && cm.leafbrushes[cm.cmodels[1].leaf.firstLeafBrush]==0 && cm.leafsurfaces[cm.cmodels[1].leaf.firstLeafSurface]==0,"synthesized submodel indices within owned hunk");
}
int main(void) {
	unsigned int offset,bad[]={0xffffffffu,0x80000000u,0x7fffffffu};int i,j;dheader_t header;
	ReferenceGolden();AcceptReferences();LoadGolden("references.bsp");
	for(i=0;i<2;i++) { Restore();memset(source+At(LUMP_SHADERS,i,sizeof(dshader_t)), 'x', MAX_QPATH);RejectReferences(); }
	for(i=0;i<6;i++) { offset=At(LUMP_BRUSHSIDES,i,sizeof(dbrushside_t));BadWord(offset+offsetof(dbrushside_t,planeNum),12);BadWord(offset+offsetof(dbrushside_t,shaderNum),2);for(j=0;j<3;j++) { BadWord(offset+offsetof(dbrushside_t,planeNum),bad[j]);BadWord(offset+offsetof(dbrushside_t,shaderNum),bad[j]); } }
	offset=At(LUMP_BRUSHES,0,sizeof(dbrush_t));BadWord(offset+offsetof(dbrush_t,firstSide),1);BadWord(offset+offsetof(dbrush_t,numSides),7);BadWord(offset+offsetof(dbrush_t,shaderNum),2);
	for(i=0;i<6;i++)BadWord(offset+offsetof(dbrush_t,numSides),i);
	for(i=0;i<3;i++) { BadWord(offset+offsetof(dbrush_t,firstSide),bad[i]);BadWord(offset+offsetof(dbrush_t,numSides),bad[i]); }
	offset=At(LUMP_NODES,0,sizeof(dnode_t));BadWord(offset+offsetof(dnode_t,planeNum),12);
	for(i=0;i<2;i++) { BadWord(offset+offsetof(dnode_t,children)+i*4,1);BadWord(offset+offsetof(dnode_t,children)+i*4,0xfffffffeu);BadWord(offset+offsetof(dnode_t,children)+i*4,0x80000000u); }
	for(i=0;i<3;i++)BadWord(At(LUMP_LEAFSURFACES,i,4),3);BadWord(At(LUMP_LEAFBRUSHES,0,4),1);
	offset=At(LUMP_LEAFS,0,sizeof(dleaf_t));
	BadWord(offset+offsetof(dleaf_t,cluster),1);BadWord(offset+offsetof(dleaf_t,cluster),0xfffffffeu);BadWord(offset+offsetof(dleaf_t,cluster),0x80000000u);BadWord(offset+offsetof(dleaf_t,area),256);BadWord(offset+offsetof(dleaf_t,area),0xffffffffu);
	BadWord(offset+offsetof(dleaf_t,firstLeafSurface),4);BadWord(offset+offsetof(dleaf_t,numLeafSurfaces),4);BadWord(offset+offsetof(dleaf_t,firstLeafBrush),1);BadWord(offset+offsetof(dleaf_t,numLeafBrushes),2);
	for(i=0;i<3;i++) { BadWord(offset+offsetof(dleaf_t,firstLeafSurface),bad[i]);BadWord(offset+offsetof(dleaf_t,numLeafSurfaces),bad[i]);BadWord(offset+offsetof(dleaf_t,firstLeafBrush),bad[i]);BadWord(offset+offsetof(dleaf_t,numLeafBrushes),bad[i]); }
	for(i=0;i<2;i++) { offset=At(LUMP_MODELS,i,sizeof(dmodel_t));BadWord(offset+offsetof(dmodel_t,firstSurface),4);BadWord(offset+offsetof(dmodel_t,numSurfaces),4);BadWord(offset+offsetof(dmodel_t,firstBrush),1);BadWord(offset+offsetof(dmodel_t,numBrushes),2);for(j=0;j<3;j++) { BadWord(offset+offsetof(dmodel_t,firstSurface),bad[j]);BadWord(offset+offsetof(dmodel_t,numSurfaces),bad[j]);BadWord(offset+offsetof(dmodel_t,firstBrush),bad[j]);BadWord(offset+offsetof(dmodel_t,numBrushes),bad[j]); } }
	offset=At(LUMP_FOGS,0,sizeof(dfog_t));BadWord(offset+offsetof(dfog_t,brushNum),1);BadWord(offset+offsetof(dfog_t,visibleSide),6);BadWord(offset+offsetof(dfog_t,visibleSide),0xfffffffeu);Restore();memset(source+offset,'x',MAX_QPATH);RejectReferences();
	for(i=0;i<3;i++) { offset=At(LUMP_SURFACES,i,sizeof(dsurface_t));BadWord(offset+offsetof(dsurface_t,shaderNum),2);BadWord(offset+offsetof(dsurface_t,surfaceType),MST_BAD);BadWord(offset+offsetof(dsurface_t,surfaceType),MST_FLARE+1); }
	for(i=0;i<2;i++) { offset=At(LUMP_SURFACES,i,sizeof(dsurface_t));BadWord(offset+offsetof(dsurface_t,fogNum),1);BadWord(offset+offsetof(dsurface_t,fogNum),0xfffffffeu);for(j=1;j<3;j++)BadWord(offset+offsetof(dsurface_t,fogNum),bad[j]); }
	/* Non-flare surfaces keep the strict fog bound in a map without fogs, including planar faces. */
	for(i=0;i<3;i++) { NoFogs();offset=At(LUMP_SURFACES,i/2,sizeof(dsurface_t));if(i==1)Word(offset+offsetof(dsurface_t,surfaceType),MST_PLANAR);AcceptReferences();Word(offset+offsetof(dsurface_t,fogNum),0);RejectReferences(); }
	for(i=0;i<2;i++) { offset=At(LUMP_SURFACES,i,sizeof(dsurface_t));BadWord(offset+offsetof(dsurface_t,firstVert),13);BadWord(offset+offsetof(dsurface_t,numVerts),13);for(j=0;j<3;j++) { BadWord(offset+offsetof(dsurface_t,firstVert),bad[j]);BadWord(offset+offsetof(dsurface_t,numVerts),bad[j]); } }
	offset=At(LUMP_SURFACES,0,sizeof(dsurface_t));BadWord(offset+offsetof(dsurface_t,firstIndex),3);BadWord(offset+offsetof(dsurface_t,numIndexes),4);for(i=0;i<3;i++) { BadWord(offset+offsetof(dsurface_t,firstIndex),bad[i]);BadWord(offset+offsetof(dsurface_t,numIndexes),bad[i]);BadWord(At(LUMP_DRAWINDEXES,i,4),3);BadWord(At(LUMP_DRAWINDEXES,i,4),0xffffffffu); }
	offset=At(LUMP_SURFACES,1,sizeof(dsurface_t));BadWord(offset+offsetof(dsurface_t,lightmapNum),0xfffffffbu);BadWord(offset+offsetof(dsurface_t,lightmapNum),0x80000000u);
	for(i=0;i<5;i++) { int required[]={LUMP_SHADERS,LUMP_PLANES,LUMP_NODES,LUMP_LEAFS,LUMP_MODELS};Restore();Word(12+required[i]*8,0);RejectReferences(); }
	Restore();Word(At(LUMP_LEAFS,0,sizeof(dleaf_t))+offsetof(dleaf_t,area),255);AcceptReferences();LoadGolden("area255.bsp");Check(cm.numAreas==256,"retail area mask boundary");
	Restore();Word(At(LUMP_LEAFS,0,sizeof(dleaf_t))+offsetof(dleaf_t,cluster),0xffffffffu);AcceptReferences();LoadGolden("opaque.bsp");
	Word(At(LUMP_LEAFS,0,sizeof(dleaf_t))+offsetof(dleaf_t,area),0xffffffffu);AcceptReferences();LoadGolden("opaque-unassigned.bsp");Check(cm.numAreas==0,"opaque leaf area -1 convention preserved");
	for(i=0;i<4;i++) { Restore();Word(At(LUMP_SURFACES,1,sizeof(dsurface_t))+offsetof(dsurface_t,lightmapNum),0xfffffffcu+i);AcceptReferences(); }
	Restore();Word(At(LUMP_SURFACES,1,sizeof(dsurface_t))+offsetof(dsurface_t,lightmapNum),0x7fffffffu);AcceptReferences();
	/* Flare fogNum is unused by collision, and the renderer maps an out-of-range value to no fog. */
	NoFogs();AcceptReferences();LoadGolden("flare-fog0-nofogs.bsp");
	for(i=0;i<5;i++) { unsigned int flareFogs[]={1,0xfffffffeu,0x80000000u,0x7fffffffu,0xffffffffu};NoFogs();Word(At(LUMP_SURFACES,2,sizeof(dsurface_t))+offsetof(dsurface_t,fogNum),flareFogs[i]);AcceptReferences();Restore();Word(At(LUMP_SURFACES,2,sizeof(dsurface_t))+offsetof(dsurface_t,fogNum),flareFogs[i]);AcceptReferences(); }
	Restore();Word(At(LUMP_LEAFS,0,sizeof(dleaf_t))+offsetof(dleaf_t,firstLeafSurface),3);Word(At(LUMP_LEAFS,0,sizeof(dleaf_t))+offsetof(dleaf_t,numLeafSurfaces),0);AcceptReferences();
	Restore();offset=Append(LUMP_MODELS,256*sizeof(dmodel_t));Check(!BSP_ValidateHeader(source,sourceSize,&header) && !BSP_ValidateReferences(source,&header),"reference-valid 256 submodels");readable=advertised=sourceSize;CM_LoadMap("models256.bsp",qfalse,&j);Check(cm.numSubModels==256,"native submodel boundary preserved");
	Restore();Append(LUMP_MODELS,257*sizeof(dmodel_t));Check(!BSP_ValidateHeader(source,sourceSize,&header) && !BSP_ValidateReferences(source,&header),"renderer references do not inherit collision handle cap");RejectCM();
	/* Exhaust every alignment/length boundary against a simple independent range maximum. */
	{
		byte words[257*4];unsigned int begin,length,k,want,result;bspIndexRanges_t ranges;
		for(k=0;k<257;k++) { unsigned int word=(k*73u)%997u;int b;for(b=0;b<4;b++)words[k*4+b]=word>>(8*b); }
		Check(!BSP_BuildIndexRanges(words,257,&ranges),"range tree fixture");
		for(begin=0;begin<257;begin++) for(length=1;length<=257-begin;length++) {
			want=0;for(k=begin;k<begin+length;k++)if((k*73u)%997u>want)want=(k*73u)%997u;
			result=BSP_IndexRangeMax(&ranges,begin,length);Check(result==want,"all partial/block/tree range maxima");
		}
		free(ranges.maxima);
	}
	Restore();failNextAllocation=1;RejectCM();Check(!failNextAllocation,"temporary index allocation failure retained world and input ownership");
	/* About 16 MiB with shared spans previously caused about 69 billion repeated word scans. */
	Restore();offset=Append(LUMP_DRAWINDEXES,524286*4);for(i=0;i<524286;i++)Word(offset+i*4,i%3);
	offset=Append(LUMP_SURFACES,131072*sizeof(dsurface_t));
	for(i=0;i<131072;i++) {
		unsigned int record=offset+i*sizeof(dsurface_t);
		Word(record+offsetof(dsurface_t,surfaceType),MST_PLANAR);Word(record+offsetof(dsurface_t,numVerts),3);Word(record+offsetof(dsurface_t,numIndexes),524286);
		Word(record+offsetof(dsurface_t,fogNum),0xffffffffu);Word(record+offsetof(dsurface_t,lightmapNum),0xffffffffu);
	}
	AcceptReferences();Word(At(LUMP_DRAWINDEXES,524285,4),0xffffffffu);RejectReferences();
	FreeHunks();puts("BSP payload reference, material and collision ownership regressions passed (issue #45)");return 0;
}
