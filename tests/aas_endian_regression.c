/* Issue #47: actual AAS swapping under independently modeled endian helpers. */
#include "../code/game/q_shared.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
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
static aas_reachability_t reach;
static void Check(int condition,const char *message) {if(!condition){fprintf(stderr,"AAS endian regression failed: %s\n",message);exit(1);}}
#ifndef Com_Memcpy
void Com_Memcpy(void *out,const void *in,size_t n) {memcpy(out,in,n);}
#endif
#ifndef Com_Memset
void Com_Memset(void *out,int value,size_t n) {memset(out,value,n);}
#endif
static void Setup(void) {memset(&aasworld,0,sizeof(aasworld));memset(&bbox,0xa5,sizeof(bbox));memset(&reach,0xa5,sizeof(reach));aasworld.bboxes=&bbox;aasworld.numbboxes=1;aasworld.reachability=&reach;aasworld.reachabilitysize=1;}
static void EncodeBBox(void) {
    int axis;uint32_t bits;
    memcpy(&bits,&bbox.presencetype,4);bits=Reverse32(bits);memcpy(&bbox.presencetype,&bits,4);
    memcpy(&bits,&bbox.flags,4);bits=Reverse32(bits);memcpy(&bbox.flags,&bits,4);
    for(axis=0;axis<3;axis++){memcpy(&bits,&bbox.mins[axis],4);bits=Reverse32(bits);memcpy(&bbox.mins[axis],&bits,4);memcpy(&bits,&bbox.maxs[axis],4);bits=Reverse32(bits);memcpy(&bbox.maxs[axis],&bits,4);}
}
static void BBoxes(void) {
    int axis,model;size_t i;const float samples[]={-15.25f,-1.5f,-0.0f,0.0f,1.25f,32.5f,FLT_MIN,-FLT_MIN,FLT_MAX,-FLT_MAX};
    for(model=0;model<2;model++)for(i=0;i<sizeof(samples)/sizeof(samples[0]);i++) {
        aas_bbox_t native,encoded;Setup();aasworld.reachabilitysize=0;swapModel=model;bbox.presencetype=PRESENCE_NORMAL;bbox.flags=1234567;
        for(axis=0;axis<3;axis++){bbox.mins[axis]=samples[i];bbox.maxs[axis]=samples[(i+1)%(sizeof(samples)/sizeof(samples[0]))];}native=bbox;
        if(model)EncodeBBox();encoded=bbox;AAS_SwapAASData();Check(!memcmp(&bbox,&native,sizeof(bbox)),"all native bbox float/int bits decoded including fractions/extremes/signed zero");
        AAS_SwapAASData();Check(!memcmp(&bbox,&encoded,sizeof(bbox)),"bbox endian round trip retains all bytes");
    }
}
static void TravelTimes(void) {
    const uint16_t times[]={0,1,255,256,12345,32767,32768,65535};size_t i;int model;
    for(model=0;model<2;model++)for(i=0;i<sizeof(times)/sizeof(times[0]);i++) {
        uint16_t encoded;Setup();aasworld.numbboxes=0;swapModel=model;encoded=model?Reverse16(times[i]):times[i];reach.traveltime=encoded;
        AAS_SwapAASData();Check(reach.traveltime==times[i],"native uint16 travel time decoded");AAS_SwapAASData();Check(reach.traveltime==encoded,"travel time endian round trip retains exact bits");
    }
}
int main(int argc,char **argv) {int proof=argc>1?atoi(argv[1]):-1;if(proof==0){swapModel=1;Setup();aasworld.reachabilitysize=0;bbox.mins[0]=1.25f;EncodeBBox();AAS_SwapAASData();Check(bbox.mins[0]==1.25f,"fractional bbox preserves native float representation");}else{BBoxes();TravelTimes();puts("Native AAS bbox float and uint16 reachability time endian-model round trips passed (issue #47)");}return 0;}
