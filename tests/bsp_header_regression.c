/* Issue #45: BSP layout and actual collision entry-point rejection. */
#include "bsp_fixture.h"

int main(void) {
	int i,j,prefix,position,checksum,readBefore,freeBefore,allocateBefore,oldSize;
	dheader_t header,before; lump_t entity;
	const unsigned int strides[HEADER_LUMPS]={1,72,16,36,48,4,4,40,12,8,44,4,72,104,49152,8,1};
	const unsigned int caps[HEADER_LUMPS]={262144,1024,131072,131072,131072,131072,262144,1024,32768,131072,524288,524288,256,131072,8388608/49152,1048576,2097152};
	const unsigned int bad[]={0x80000000u,0xffffffffu,0x7fffffffu};
	Check(sizeof(dheader_t)==144,"disk header size");
	BuildCM(1025);readable=advertised=sourceSize;CM_LoadMap("good.bsp",qfalse,&checksum);
	Check(frees==1 && checksums==1 && clears==1 && floods==1 && !strcmp(cm.name,"good.bsp") && cm.numLeafs==1 && cm.numSubModels==1 && cm.numBrushes==1 && cm.numShaders==1025 && cm.numClusters==1 && !strcmp(cm.entityString,"abc") && cm.numEntityChars==3,"native collision map golden");
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
		Empty();oldSize=(caps[i]+1)*strides[i];Lump(i,144,oldSize);sourceSize=144+oldSize;Check(sourceSize<=sizeof(source),"raised-limit fixture capacity");if(i==LUMP_VISIBILITY) { Word(144,0);Word(148,0); }Check(!BSP_ValidateHeader(source,sourceSize,&header),"compiler defaults must not cap the runtime file format");
	}
	Empty();Lump(LUMP_SHADERS,144,72);Lump(LUMP_FOGS,144,72);sourceSize=216;RejectHeader();
	for(i=1;i<8;i++) { Empty();Lump(LUMP_VISIBILITY,144,i);sourceSize=144+i;RejectHeader(); }
	Empty();Lump(LUMP_VISIBILITY,144,9);sourceSize=153;Word(144,1);Word(148,1);Check(!BSP_ValidateHeader(source,sourceSize,&header),"exact one-byte PVS");
	Word(148,0);RejectHeader();Word(148,2);RejectHeader();Word(148,1);Word(144,2);RejectHeader();
	Word(144,0xffffffffu);RejectHeader();Word(144,1);Word(148,0x80000000u);RejectHeader();
	Empty();Lump(LUMP_ENTITIES,144,1);sourceSize=145;source[144]='x';Check(!BSP_ValidateHeader(source,sourceSize,&header),"byte entity lump");
	Lump(LUMP_ENTITIES,0,0);Lump(LUMP_VISIBILITY,sourceSize,0);Check(!BSP_ValidateHeader(source,sourceSize,&header),"zero/end empty lumps");
	memset(&header,0xa5,sizeof(header));before=header;Check(BSP_ValidateHeader(NULL,144,&header) && BSP_ValidateHeader(source,-1,&header) && BSP_ValidateHeader(source,INT_MAX,&header) && BSP_ValidateHeader(source,144,NULL) && !memcmp(&header,&before,sizeof(header)),"invalid API lengths/output");
	/* Exercise exact signed-allocation boundaries and reservation arithmetic without allocating. */
	for(i=0;i<5;i++) {
		const unsigned int sizes[]={1,4,20,40,112};bspArrayAllocation_t array;array.elementSize=sizes[i];array.extraElements=12;array.count=(INT_MAX-4096u)/array.elementSize-12;
		Check(!BSP_ValidateAllocations(&array,1),"exact allocation capacity");array.count++;Check(BSP_ValidateAllocations(&array,1)!=NULL,"allocation overflow boundary");array.count=0;array.extraElements=0xffffffffu;Check(BSP_ValidateAllocations(&array,1)!=NULL,"reservation overflow");array.elementSize=0;Check(BSP_ValidateAllocations(&array,1)!=NULL,"zero allocation size");
	}
	Empty();Lump(LUMP_VISIBILITY,144,8+8192*1024);sourceSize=144+8+8192*1024;Word(144,8192);Word(148,1024);Check(!BSP_ValidateHeader(source,sourceSize,&header),"raised PVS byte budget");
	previous=cm;readable=144;advertised=-1;missing=0;expectError=1;freeBefore=frees;
	if(!setjmp(errorJump)) { CM_LoadMap("negative.bsp",qfalse,&checksum);Check(0,"negative FS length accepted"); }expectError=0;Check(frees==freeBefore+1 && !memcmp(&cm,&previous,sizeof(cm)),"negative length cleanup/world retention");
	missing=1;expectError=1;freeBefore=frees;if(!setjmp(errorJump)) { CM_LoadMap("missing.bsp",qfalse,&checksum);Check(0,"missing input accepted"); }expectError=0;Check(frees==freeBefore && !memcmp(&cm,&previous,sizeof(cm)),"missing file retained world");missing=0;
	cmod_base=source;source[0]='x';source[1]='y';entity.fileofs=0;entity.filelen=2;allocateBefore=allocations;CMod_LoadEntityString(&entity);Check(hunkSizes[allocateBefore]==3 && !strcmp(cm.entityString,"xy"),"collision entity terminator");
	entity.filelen=0;allocateBefore=allocations;CMod_LoadEntityString(&entity);Check(hunkSizes[allocateBefore]==1 && !cm.entityString[0],"empty collision entity terminator");
	FreeHunks();
	puts("BSP header, lump layout, visibility and collision ownership regressions passed (issue #45)");return 0;
}
