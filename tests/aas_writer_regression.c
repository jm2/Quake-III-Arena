/* Issue #47: actual AAS writer ownership and exact I/O under endian models. */
#include "../code/game/q_shared.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <limits.h>
#include <stddef.h>
static int swapModel;
static uint32_t Reverse32(uint32_t value) {return(value>>24)|((value>>8)&0xff00u)|((value<<8)&0xff0000u)|(value<<24);}
static uint16_t Reverse16(uint16_t value) {return(uint16_t)((value>>8)|(value<<8));}
static int ModelLong(int value) {uint32_t bits;memcpy(&bits,&value,4);if(swapModel)bits=Reverse32(bits);memcpy(&value,&bits,4);return value;}
static short ModelShort(short value) {uint16_t bits;memcpy(&bits,&value,2);if(swapModel)bits=Reverse16(bits);memcpy(&value,&bits,2);return value;}
static float ModelFloat(float value) {uint32_t bits;memcpy(&bits,&value,4);if(swapModel)bits=Reverse32(bits);memcpy(&value,&bits,4);return value;}
#undef LittleLong
#undef LittleShort
#undef LittleFloat
#define LittleLong ModelLong
#define LittleShort ModelShort
#define LittleFloat ModelFloat
#include "../code/botlib/be_aas_file.c"

aas_t aasworld;
botlib_import_t botimport;
static aas_bbox_t bbox;
static aas_vertex_t vertex;
static aas_plane_t plane;
static aas_edge_t edge;
static aas_edgeindex_t edgeindex;
static aas_face_t face;
static aas_faceindex_t faceindex;
static aas_area_t area;
static aas_areasettings_t settings;
static aas_reachability_t reach;
static aas_node_t node;
static aas_portal_t portal;
static aas_portalindex_t portalindex;
static aas_cluster_t cluster;
static void *data[AAS_LUMPS]={&bbox,&vertex,&plane,&edge,&edgeindex,&face,&faceindex,&area,&settings,&reach,&node,&portal,&portalindex,&cluster};
static const int sizes[AAS_LUMPS]={32,12,20,8,4,24,4,48,28,44,12,20,4,16};
static unsigned char before[AAS_LUMPS][48],written[4096];
static int opened,opens,closes,writes,seeks,position,outputSize,missing,failWrite,failSeek;
static void Check(int condition,const char *message) {if(!condition){fprintf(stderr,"AAS writer regression failed: %s\n",message);exit(1);}}
#ifndef Com_Memcpy
void Com_Memcpy(void *out,const void *in,size_t n) {memcpy(out,in,n);}
#endif
#ifndef Com_Memset
void Com_Memset(void *out,int value,size_t n) {memset(out,value,n);}
#endif
static void QDECL Print(int type,char *format,...) {(void)type;(void)format;}
static int Open(const char *name,fileHandle_t *file,fsMode_t mode) {Check(name&&!strcmp(name,"fixture.aas")&&mode==FS_WRITE&&!opened,"native writer open contract");opens++;if(missing){*file=0;return -1;}opened=1;*file=39;return 0;}
static int Write(const void *buffer,int length,fileHandle_t file) {int n=length;Check(opened&&file==39&&buffer&&length>0&&position>=0&&length<=4096-position,"bounded native writer request");writes++;if(failWrite==writes)n--;memcpy(written+position,buffer,n);position+=n;if(position>outputSize)outputSize=position;return n;}
static int Seek(fileHandle_t file,long offset,int origin) {Check(opened&&file==39&&offset==0&&origin==FS_SEEK_SET,"native header rewrite seek contract");seeks++;if(failSeek)return -1;position=0;return 0;}
static void Close(fileHandle_t file) {Check(opened&&file==39,"writer file close exactly once");opened=0;closes++;}
static void Setup(int empty) {
    int lump,word;memset(&aasworld,0,sizeof(aasworld));memset(written,0xa5,sizeof(written));
    opened=opens=closes=writes=seeks=position=outputSize=missing=failWrite=failSeek=0;
    for(lump=0;lump<AAS_LUMPS;lump++){for(word=0;word<sizes[lump];word+=4){uint32_t bits=0x3fa00000u+(uint32_t)(lump*256+word);memcpy((unsigned char *)data[lump]+word,&bits,4);}memcpy(before[lump],data[lump],sizes[lump]);}
    aasworld.bboxes=&bbox;aasworld.numbboxes=!empty;aasworld.vertexes=&vertex;aasworld.numvertexes=!empty;
    aasworld.planes=&plane;aasworld.numplanes=!empty;aasworld.edges=&edge;aasworld.numedges=!empty;
    aasworld.edgeindex=&edgeindex;aasworld.edgeindexsize=!empty;aasworld.faces=&face;aasworld.numfaces=!empty;
    aasworld.faceindex=&faceindex;aasworld.faceindexsize=!empty;aasworld.areas=&area;aasworld.numareas=!empty;
    aasworld.areasettings=&settings;aasworld.numareasettings=!empty;aasworld.reachability=&reach;aasworld.reachabilitysize=!empty;
    aasworld.nodes=&node;aasworld.numnodes=!empty;aasworld.portals=&portal;aasworld.numportals=!empty;
    aasworld.portalindex=&portalindex;aasworld.portalindexsize=!empty;aasworld.clusters=&cluster;aasworld.numclusters=!empty;
    aasworld.loaded=aasworld.initialized=aasworld.savefile=qtrue;aasworld.bspchecksum=12345;aasworld.time=37;
}
static void Preserved(const aas_t *world) {int lump;Check(!memcmp(&aasworld,world,sizeof(aasworld)),"complete native world metadata unchanged");for(lump=0;lump<AAS_LUMPS;lump++)Check(!memcmp(data[lump],before[lump],sizes[lump]),"every native lump byte unchanged after writing");}
static void InvalidWorlds(void) {
    int *counts[AAS_LUMPS]={&aasworld.numbboxes,&aasworld.numvertexes,&aasworld.numplanes,&aasworld.numedges,&aasworld.edgeindexsize,&aasworld.numfaces,&aasworld.faceindexsize,&aasworld.numareas,&aasworld.numareasettings,&aasworld.reachabilitysize,&aasworld.numnodes,&aasworld.numportals,&aasworld.portalindexsize,&aasworld.numclusters};
    const size_t pointerOffsets[AAS_LUMPS]={offsetof(aas_t,bboxes),offsetof(aas_t,vertexes),offsetof(aas_t,planes),offsetof(aas_t,edges),offsetof(aas_t,edgeindex),offsetof(aas_t,faces),offsetof(aas_t,faceindex),offsetof(aas_t,areas),offsetof(aas_t,areasettings),offsetof(aas_t,reachability),offsetof(aas_t,nodes),offsetof(aas_t,portals),offsetof(aas_t,portalindex),offsetof(aas_t,clusters)};
    int model,lump,mode;
    for(model=0;model<2;model++)for(lump=0;lump<AAS_LUMPS;lump++)for(mode=0;mode<3;mode++) {
        aas_t world;void *nil=NULL;swapModel=model;Setup(0);
        if(mode==0)*counts[lump]=-1;else if(mode==1)*counts[lump]=INT_MAX;else memcpy((unsigned char *)&aasworld+pointerOffsets[lump],&nil,sizeof(nil));
        world=aasworld;Check(!AAS_WriteAASFile("fixture.aas"),"negative/overflow/null native output source rejected");Check(!opens&&!closes&&!writes&&!seeks,"all output roots preflight before file effects");Preserved(&world);
    }
    for(model=0;model<2;model++){
        aas_t world;swapModel=model;Setup(0);aasworld.numbboxes=(INT_MAX-124)/32;world=aasworld;
        Check(!AAS_WriteAASFile("fixture.aas")&&!opens&&!writes,"individually representable lump rejected on aggregate signed offset overflow");Preserved(&world);
    }
}
static void StoreWord(unsigned char *out,uint32_t value) {int byte;if(swapModel)value=Reverse32(value);for(byte=0;byte<4;byte++)out[byte]=(unsigned char)(value>>(byte*8));}
static void Expected(int empty) {
    unsigned char expected[4096];int lump,byte,offset=124;memset(expected,0,sizeof(expected));
    StoreWord(expected,AASID);StoreWord(expected+4,5);StoreWord(expected+8,12345);
    for(lump=0;lump<AAS_LUMPS;lump++){
        int length=empty?0:sizes[lump];StoreWord(expected+12+lump*8,offset);StoreWord(expected+16+lump*8,length);
        memcpy(expected+offset,before[lump],length);
        if(swapModel&&!empty){for(byte=0;byte<(lump==AASLUMP_REACHABILITY?40:length);byte+=4){unsigned char t=expected[offset+byte];expected[offset+byte]=expected[offset+byte+3];expected[offset+byte+3]=t;t=expected[offset+byte+1];expected[offset+byte+1]=expected[offset+byte+2];expected[offset+byte+2]=t;}if(lump==AASLUMP_REACHABILITY){unsigned char t=expected[offset+40];expected[offset+40]=expected[offset+41];expected[offset+41]=t;}}
        offset+=length;
    }
    for(byte=0;byte<116;byte++)expected[8+byte]^=(unsigned char)(byte*119);
    Check(outputSize==offset&&!memcmp(written,expected,offset),"complete independent header/lump/padding output matches existing native format and endian model");
}
static void Successes(void) {int model,empty;for(model=0;model<2;model++)for(empty=0;empty<2;empty++){aas_t world;swapModel=model;Setup(empty);world=aasworld;Check(AAS_WriteAASFile("fixture.aas"),"valid native writer succeeds");Check(opens==1&&closes==1&&!opened&&seeks==1&&writes==(empty?2:16),"native successful file ownership");Preserved(&world);Expected(empty);position=outputSize=0;Check(AAS_WriteAASFile("fixture.aas"),"second write succeeds without toggling native world");Preserved(&world);Expected(empty);}}
static void Failures(void) {int model,request;for(model=0;model<2;model++)for(request=0;request<=17;request++){aas_t world;swapModel=model;Setup(0);world=aasworld;if(request==0)missing=1;else if(request==17)failSeek=1;else failWrite=request;Check(!AAS_WriteAASFile("fixture.aas"),"open/every write/header seek failure reports false");Check(opens==1&&closes==(request!=0)&&!opened,"all failures close owned file once");Preserved(&world);}}
int main(int argc,char **argv) {
    int proof=argc>1?atoi(argv[1]):-1;botimport.Print=Print;botimport.FS_FOpenFile=Open;botimport.FS_Write=Write;botimport.FS_Seek=Seek;botimport.FS_FCloseFile=Close;
    if(proof==0){aas_t world;swapModel=1;Setup(0);world=aasworld;missing=1;Check(!AAS_WriteAASFile("fixture.aas"),"missing native output rejected");Preserved(&world);}
    else if(proof==1){aas_t world;swapModel=1;Setup(0);world=aasworld;Check(AAS_WriteAASFile("fixture.aas"),"native success");Preserved(&world);}
    else if(proof==2){swapModel=0;Setup(0);failWrite=2;Check(!AAS_WriteAASFile("fixture.aas"),"short payload write reported");}
    else {Successes();Failures();InvalidWorlds();puts("Native AAS writer endian ownership, exact header/lump writes and file cleanup passed (issue #47)");}
    return 0;
}
