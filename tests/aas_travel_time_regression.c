/* Actual native classification and area-distance conversion with literal goldens. */
#include "../code/game/q_shared.h"
#include "../code/botlib/aasfile.h"
#include "../code/game/botlib.h"
#include "../code/game/be_aas.h"
#include "../code/botlib/be_aas_def.h"
#include <float.h>
#include <stdint.h>
aas_t aasworld;
static aas_areasettings_t settings[2];
#include Q3_AAS_TRAVEL_BODY
static void Check(int condition,const char *message) {
    if(!condition){fprintf(stderr,"AAS travel-time regression failed: %s\n",message);exit(1);}
}
static void Mode(int kind) {
    memset(settings,0,sizeof(settings));
    aasworld.areasettings=settings;aasworld.numareasettings=aasworld.numareas=2;
    settings[1].presencetype=kind==1?PRESENCE_CROUCH:PRESENCE_NORMAL;
    settings[1].areaflags=kind==2?AREA_LIQUID:0;
}
static float Float(uint32_t bits) {union {float value;uint32_t bits;} r;r.bits=bits;return r.value;}
static void Goldens(void) {
    const struct {float distance;unsigned short times[3];} cases[]={
        {0,{1,1,1}},{0.5f,{1,1,1}},{5,{1,6,5}},{100,{33,130,100}},
        {400,{132,520,400}},{5000,{1650,6500,5000}}
    };
    const struct {float distance;unsigned short time;} boundaries[]={
        {65535,65535},{65536,0},{65537,1},{2147483520.0f,65408}
    };
    int kind,axis,reverse;size_t i;
    for(kind=0;kind<3;kind++)for(i=0;i<sizeof(cases)/sizeof(cases[0]);i++)for(axis=0;axis<3;axis++)for(reverse=0;reverse<2;reverse++) {
        vec3_t start={0,0,0},end={0,0,0};Mode(kind);
        (reverse?end:start)[axis]=cases[i].distance;
        Check(AAS_AreaTravelTime(1,start,end)==cases[i].times[kind],"literal native walk/crouch/swim distances and minimum time");
    }
    for(kind=0;kind<3;kind++) {
        vec3_t start={3,4,0},end={0,0,0};const unsigned short times[]={1,6,5};Mode(kind);
        Check(AAS_AreaTravelTime(1,start,end)==times[kind],"native multi-axis distance and classification");
    }
    Mode(2);
    for(i=0;i<sizeof(boundaries)/sizeof(boundaries[0]);i++) {
        vec3_t start={boundaries[i].distance,0,0},end={0,0,0};
        Check(AAS_AreaTravelTime(1,start,end)==boundaries[i].time,"defined signed conversion and legacy uint16 wrap remain unchanged");
    }
    for(kind=0;kind<3;kind++) {
        vec3_t start={Float(0x80000000u),0,0},end={0,0,0};Mode(kind);
        Check(AAS_AreaTravelTime(1,start,end)==1,"signed zero keeps minimum time");
        settings[1].presencetype=PRESENCE_CROUCH;settings[1].areaflags=AREA_LIQUID;
        start[0]=100;Check(AAS_AreaTravelTime(1,start,end)==130,"crouch classification precedes liquid speed");
    }
}
static void InvalidDistances(void) {
    const uint32_t nonfinite[]={0x7f800000u,0xff800000u,0x7fc00000u,0xffc00000u,0x7f800001u,0xff800001u};
    const float extremes[]={FLT_MAX,-FLT_MAX,1.0e20f,-1.0e20f};
    int kind,axis,side;size_t i;
    for(kind=0;kind<3;kind++)for(axis=0;axis<3;axis++)for(side=0;side<2;side++) {
        for(i=0;i<sizeof(nonfinite)/sizeof(nonfinite[0]);i++) {
            vec3_t start={0,0,0},end={0,0,0};Mode(kind);(side?end:start)[axis]=Float(nonfinite[i]);
            Check(AAS_AreaTravelTime(1,start,end)==65535,"nonfinite runtime coordinates return maximum native time");
        }
        for(i=0;i<sizeof(extremes)/sizeof(extremes[0]);i++) {
            vec3_t start={0,0,0},end={0,0,0};Mode(kind);(side?end:start)[axis]=extremes[i];
            Check(AAS_AreaTravelTime(1,start,end)==65535,"finite coordinates with overflowing derived length return maximum native time");
        }
    }
    Mode(2);
    {vec3_t start={2147483648.0f,0,0},end={0,0,0};Check(AAS_AreaTravelTime(1,start,end)==65535,"first unrepresentable signed distance rejects before cast");}
    {vec3_t start={FLT_MAX,FLT_MAX,FLT_MAX},end={-FLT_MAX,-FLT_MAX,-FLT_MAX};Check(AAS_AreaTravelTime(1,start,end)==65535,"finite subtraction overflow rejects before cast");}
}
int main(int argc,char **argv) {
    if(argc>1) {
        int proof=atoi(argv[1]);vec3_t start={0,0,0},end={0,0,0};Mode(2);
        start[0]=proof==0?Float(0x7fc00000u):proof==1?2147483648.0f:FLT_MAX;
        Check(AAS_AreaTravelTime(1,start,end)==65535,"unsafe original runtime distance conversion");
    } else {Goldens();InvalidDistances();puts("Native AAS area travel-time conversions and legacy distance results passed (issue #47)");}
    return 0;
}
