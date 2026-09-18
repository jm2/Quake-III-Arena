/* Issue #47: actual loader portal/cluster references and local routing slots. */
#define Q3_REACHABILITY_FIXTURE_ONLY
#include "aas_reachability_regression.c"
#undef Q3_REACHABILITY_FIXTURE_ONLY
static int portalOffset,clusterOffset,indexOffset,settingsOffset;
static void PortalBuild(int version) {
    int oldArea,oldSettings,areaOffset;
    ReachBuild(version,TRAVEL_WALK,1,3);Encode(version);oldArea=geometryOffsets[7];oldSettings=geometryOffsets[8];areaOffset=sourceSize;settingsOffset=areaOffset+144;
    memcpy(source+areaOffset,source+oldArea,96);memcpy(source+areaOffset+96,source+oldArea+48,48);Word(areaOffset+96,2);
    memcpy(source+settingsOffset,source+oldSettings,56);memcpy(source+settingsOffset+56,source+oldSettings+28,28);
    Word(settingsOffset+28+12,1);Word(settingsOffset+28+16,0);
    Word(settingsOffset+56+12,0xffffffffu);Word(settingsOffset+56+20,0);
    portalOffset=settingsOffset+84;indexOffset=portalOffset+40;clusterOffset=indexOffset+8;
    memset(source+portalOffset,0,96);Word(portalOffset+20,2);Word(portalOffset+24,1);Word(portalOffset+28,2);Word(portalOffset+32,1);Word(portalOffset+36,0);
    Word(indexOffset,1);Word(indexOffset+4,1);
    Word(clusterOffset+16,2);Word(clusterOffset+20,1);Word(clusterOffset+24,1);Word(clusterOffset+28,0);
    Word(clusterOffset+32,1);Word(clusterOffset+36,0);Word(clusterOffset+40,1);Word(clusterOffset+44,1);
    Word(12+8*AASLUMP_AREAS,areaOffset);Word(16+8*AASLUMP_AREAS,144);
    Word(12+8*AASLUMP_AREASETTINGS,settingsOffset);Word(16+8*AASLUMP_AREASETTINGS,84);
    Word(12+8*AASLUMP_PORTALS,portalOffset);Word(16+8*AASLUMP_PORTALS,40);
    Word(12+8*AASLUMP_PORTALINDEX,indexOffset);Word(16+8*AASLUMP_PORTALINDEX,8);
    Word(12+8*AASLUMP_CLUSTERS,clusterOffset);Word(16+8*AASLUMP_CLUSTERS,48);
    sourceSize=advertised=readable=clusterOffset+48;Encode(version);
}
static void PortalWord(int version,int offset,uint32_t bits) {Encode(version);Word(offset,bits);Encode(version);}
static void ValidPortals(void) {
    int version,swapped;
    for(version=4;version<=5;version++)for(swapped=0;swapped<2;swapped++) {
        PortalBuild(version);Counters();OldWorld();
        if(swapped){PortalWord(version,portalOffset+24,2);PortalWord(version,portalOffset+28,1);PortalWord(version,portalOffset+32,0);PortalWord(version,portalOffset+36,1);}
        Check(AAS_LoadAASFile("fixture.aas")==BLERR_NOERROR&&aasworld.loaded&&!opened&&closes==1,"native portal sides and cluster-local slots remain accepted");
        Check(!memcmp(aasworld.portals,source+portalOffset,40)&&!memcmp(aasworld.portalindex,source+indexOffset,8)&&!memcmp(aasworld.clusters,source+clusterOffset,48)&&!memcmp(aasworld.areasettings,source+settingsOffset,84),"every independent portal/index/cluster/settings byte unchanged");
    }
}
static void UnclusteredPortals(void) {
    int version;
    for(version=4;version<=5;version++){
        PortalBuild(version);Counters();OldWorld();Encode(version);Word(16+8*AASLUMP_CLUSTERS,16);
        Word(settingsOffset+28+12,0);Word(settingsOffset+56+12,0);
        Word(portalOffset+24,0);Word(portalOffset+28,0);Word(portalOffset+32,0);Word(portalOffset+36,0);Encode(version);
        Check(AAS_LoadAASFile("fixture.aas")==BLERR_NOERROR&&aasworld.loaded&&!opened&&closes==1,"native unclustered roots remain eligible for clustering initialization");
        Check(!memcmp(aasworld.portals,source+portalOffset,40)&&!memcmp(aasworld.areasettings,source+settingsOffset,84),"unclustered payload remains unchanged");
    }
}
static void BadPortals(void) {
    const uint32_t huge[]={0x80000000u,0x7fffffffu};int version,group,field;size_t i;
    for(version=4;version<=5;version++) {
        for(group=0;group<4;group++)for(field=0;field<(group==0?5:group==1?1:group==2?4:2);field++)for(i=0;i<2;i++) {
            int offset;PortalBuild(version);Counters();OldWorld();offset=group==0?portalOffset+20+field*4:group==1?indexOffset:group==2?clusterOffset+16+field*4:settingsOffset+28+12+field*4;
            PortalWord(version,offset,huge[i]);GeometryReject();
        }
        PortalBuild(version);Counters();OldWorld();PortalWord(version,portalOffset+24,0);GeometryReject();
        PortalBuild(version);Counters();OldWorld();PortalWord(version,portalOffset+28,0);GeometryReject();
        PortalBuild(version);Counters();OldWorld();PortalWord(version,indexOffset,2);GeometryReject();
        PortalBuild(version);Counters();OldWorld();PortalWord(version,portalOffset+20,3);GeometryReject();
        PortalBuild(version);Counters();OldWorld();PortalWord(version,portalOffset+24,3);GeometryReject();
        PortalBuild(version);Counters();OldWorld();PortalWord(version,portalOffset+32,2);GeometryReject();
        PortalBuild(version);Counters();OldWorld();PortalWord(version,settingsOffset+28+16,2);GeometryReject();
        PortalBuild(version);Counters();OldWorld();PortalWord(version,settingsOffset+56+12,0xfffffffeu);GeometryReject();
        PortalBuild(version);Counters();OldWorld();PortalWord(version,portalOffset+20,1);GeometryReject();
        PortalBuild(version);Counters();OldWorld();PortalWord(version,clusterOffset+20,3);GeometryReject();
        PortalBuild(version);Counters();OldWorld();PortalWord(version,clusterOffset+24,2);GeometryReject();
        PortalBuild(version);Counters();OldWorld();PortalWord(version,indexOffset,0);GeometryReject();
    }
}
int main(int argc,char **argv) {
    int proof=argc>1?atoi(argv[1]):-1;botimport.Print=Print;botimport.FS_FOpenFile=Open;botimport.FS_Read=Read;botimport.FS_Seek=Seek;botimport.FS_FCloseFile=Close;
    if(proof==0){PortalBuild(4);Counters();OldWorld();PortalWord(4,settingsOffset+28+12,INT_MAX);GeometryReject();}
    else if(proof==1){PortalBuild(4);Counters();OldWorld();PortalWord(4,indexOffset,INT_MIN);GeometryReject();}
    else {ValidPortals();UnclusteredPortals();BadPortals();puts("Native AAS portal/cluster references, local slots, inverse ownership and spans passed (issue #47)");}
    ResetArena();return 0;
}
