/* Issue #47: actual loader area reach spans and travel-dependent payloads. */
#define main AASGeometryFixtureMain
#include "aas_geometry_regression.c"
#undef main
static int reachOffset;
static void ReachBuild(int version,int type,int face,int edge) {
    int offset,lump;GeometryBuild(version,0);Encode(version);reachOffset=sourceSize;
    memset(source+reachOffset,0,88);Word(12+8*AASLUMP_REACHABILITY,reachOffset);Word(16+8*AASLUMP_REACHABILITY,88);
    sourceSize+=88;advertised=readable=sourceSize;
    offset=reachOffset+44;Word(offset,1);Word(offset+4,(uint32_t)face);Word(offset+8,(uint32_t)edge);
    Word(offset+12,0x3fa00000u);Word(offset+16,0xc0200000u);Word(offset+24,0x40a80000u);Word(offset+36,(uint32_t)type);source[offset+40]=0xff;source[offset+41]=0xff;
    Word(geometryOffsets[AASLUMP_AREASETTINGS]+28+20,1);Word(geometryOffsets[AASLUMP_AREASETTINGS]+28+24,1);
    for(lump=AASLUMP_PORTALS;lump<AAS_LUMPS;lump++)Word(12+8*lump,sourceSize);
    Encode(version);
}
static void ReachWord(int version,int offset,uint32_t bits) {Encode(version);Word(offset,bits);Encode(version);}
static void ValidReachability(void) {
    int version,type,signedRef;const int special[]={TRAVEL_ELEVATOR,TRAVEL_JUMPPAD,TRAVEL_FUNCBOB};size_t i;
    for(version=4;version<=5;version++)for(type=0;type<MAX_TRAVELTYPES;type++)for(signedRef=0;signedRef<2;signedRef++) {
        ReachBuild(version,type|TRAVELFLAG_NOTTEAM1|TRAVELFLAG_NOTTEAM2,signedRef?-1:1,signedRef?-3:3);Counters();OldWorld();
        Check(AAS_LoadAASFile("fixture.aas")==BLERR_NOERROR&&aasworld.loaded&&closes==1&&!opened,"all native travel slots/team flags and signed references retain acceptance");
        Check(!memcmp(aasworld.reachability,source+reachOffset,88)&&aasworld.reachability[1].traveltime==65535,"all reachability bytes and uint16 time retain literal native payload");
    }
    for(version=4;version<=5;version++)for(i=0;i<sizeof(special)/sizeof(special[0]);i++) {
        ReachBuild(version,special[i]|TRAVELFLAG_NOTTEAM1,(int)0x81230123u,(int)0x8abc4567u);Counters();OldWorld();
        Check(AAS_LoadAASFile("fixture.aas")==BLERR_NOERROR&&aasworld.loaded,"mover/velocity packed fields retain full signed bits without geometry-index reinterpretation");
        Check(!memcmp(aasworld.reachability,source+reachOffset,88),"packed travel fields retain every native byte");
    }
}
static void DuplicateSpanFailure(int version) {
    int oldArea,oldSettings,areaOffset,settingsOffset,i;
    ReachBuild(version,TRAVEL_WALK,1,3);Encode(version);oldArea=geometryOffsets[7];oldSettings=geometryOffsets[8];areaOffset=sourceSize;settingsOffset=areaOffset+4*48;
    memcpy(source+areaOffset,source+oldArea,96);memcpy(source+settingsOffset,source+oldSettings,56);
    for(i=2;i<4;i++){memcpy(source+areaOffset+i*48,source+oldArea+48,48);Word(areaOffset+i*48,i);memcpy(source+settingsOffset+i*28,source+oldSettings+28,28);}
    Word(12+8*AASLUMP_AREAS,areaOffset);Word(16+8*AASLUMP_AREAS,4*48);Word(12+8*AASLUMP_AREASETTINGS,settingsOffset);Word(16+8*AASLUMP_AREASETTINGS,4*28);
    sourceSize=settingsOffset+4*28;advertised=readable=sourceSize;Encode(version);Counters();OldWorld();GeometryReject();
}
static void BadReachability(void) {
    const uint32_t badFloats[]={0x7f800000u,0xff800000u,0x7fc00000u,0xffc00000u,0x7f800001u,0xff800001u};
    const uint32_t badRefs[]={0x80000000u,0x7fffffffu};int version,field;size_t i;
    for(version=4;version<=5;version++) {
        DuplicateSpanFailure(version);
        for(field=12;field<36;field+=4)for(i=0;i<sizeof(badFloats)/sizeof(badFloats[0]);i++){ReachBuild(version,TRAVEL_WALK,1,3);Counters();OldWorld();ReachWord(version,reachOffset+44+field,badFloats[i]);GeometryReject();}
        for(field=0;field<=8;field+=4)for(i=0;i<2;i++){ReachBuild(version,TRAVEL_WALK,1,3);Counters();OldWorld();ReachWord(version,reachOffset+44+field,badRefs[i]);GeometryReject();}
        for(field=20;field<=24;field+=4)for(i=0;i<2;i++){ReachBuild(version,TRAVEL_WALK,1,3);Counters();OldWorld();ReachWord(version,geometryOffsets[8]+28+field,badRefs[i]);GeometryReject();}
        ReachBuild(version,TRAVEL_WALK,1,3);Counters();OldWorld();ReachWord(version,reachOffset+44,2);GeometryReject();
        ReachBuild(version,TRAVEL_WALK,1,3);Counters();OldWorld();ReachWord(version,reachOffset+48,2);GeometryReject();
        ReachBuild(version,TRAVEL_WALK,1,3);Counters();OldWorld();ReachWord(version,reachOffset+52,4);GeometryReject();
        ReachBuild(version,TRAVEL_WALK,1,3);Counters();OldWorld();ReachWord(version,reachOffset+80,32);GeometryReject();
        ReachBuild(version,TRAVEL_WALK,1,3);Counters();OldWorld();ReachWord(version,geometryOffsets[8]+28+20,2);GeometryReject();
        ReachBuild(version,TRAVEL_WALK,1,3);Counters();OldWorld();ReachWord(version,geometryOffsets[8]+28+24,0);GeometryReject();
    }
}
#ifndef Q3_REACHABILITY_FIXTURE_ONLY
int main(int argc,char **argv) {
    int proof=argc>1?atoi(argv[1]):-1;botimport.Print=Print;botimport.FS_FOpenFile=Open;botimport.FS_Read=Read;botimport.FS_Seek=Seek;botimport.FS_FCloseFile=Close;
    if(proof==0){ReachBuild(4,TRAVEL_WALK,1,3);Counters();OldWorld();ReachWord(4,reachOffset+44,INT_MAX);GeometryReject();}
    else if(proof==1){ReachBuild(4,TRAVEL_WALK,1,3);Counters();OldWorld();ReachWord(4,geometryOffsets[8]+28+20,INT_MAX);GeometryReject();}
    else {ValidReachability();BadReachability();puts("Native AAS reachability spans, travel-dependent fields, signed references and finite endpoints passed (issue #47)");}
    ResetArena();return 0;
}

#endif
