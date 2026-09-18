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
    geometryOffsets[AASLUMP_AREAS]=areaOffset;geometryOffsets[AASLUMP_AREASETTINGS]=settingsOffset;
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
static void PortalSpanBuild(int version,int partial) {
    int offset,i;
    PortalBuild(version);Encode(version);
    if(partial) {
        offset=sourceSize;
        for(i=0;i<4;i++)Word(offset+i*4,1);
        Word(12+8*AASLUMP_PORTALINDEX,offset);Word(16+8*AASLUMP_PORTALINDEX,16);
        Word(clusterOffset+24,2);Word(clusterOffset+40,2);Word(clusterOffset+44,1);
        indexOffset=offset;sourceSize=advertised=readable=offset+16;
    } else Word(clusterOffset+44,0);
    Encode(version);
}
static void PortalSpanOwnership(void) {
    int version,kind;
    for(version=4;version<=5;version++) {
        for(kind=0;kind<2;kind++) {
            PortalSpanBuild(version,kind);Counters();OldWorld();GeometryReject();
            Check(workspaceRequests==3&&workspaceFrees==3&&!workspacePointer,"portal span rejection releases all validation heaps");
        }
        PortalBuild(version);Counters();OldWorld();PortalWord(version,clusterOffset+28,1);PortalWord(version,clusterOffset+44,0);
        Check(AAS_LoadAASFile("fixture.aas")==BLERR_NOERROR&&aasworld.loaded,"reordered disjoint cluster spans remain accepted");
        Check(!memcmp(aasworld.clusters,source+clusterOffset,48)&&!memcmp(aasworld.portalindex,source+indexOffset,8),"reordered cluster spans retain native bytes");
        Check(workspaceRequests==4&&workspaceFrees==4&&!workspacePointer,"portal ownership heaps physically release");
        PortalBuild(version);Counters();OldWorld();failWorkspace=3;GeometryReject();
        Check(workspaceRequests==3&&workspaceFrees==2&&!workspacePointer,"portal bitmap failure leaves no temporary owner");
    }
}
static void ReachablePortalBuild(int version) {
    int oldReach,offset;
    PortalBuild(version);Encode(version);oldReach=reachOffset;offset=sourceSize;
    memcpy(source+offset,source+oldReach,88);memcpy(source+offset+88,source+oldReach+44,44);
    Word(12+8*AASLUMP_REACHABILITY,offset);Word(16+8*AASLUMP_REACHABILITY,132);
    Word(settingsOffset+56+20,1);Word(settingsOffset+56+24,2);
    Word(clusterOffset+20,2);Word(clusterOffset+36,1);
    reachOffset=offset;sourceSize=advertised=readable=offset+132;Encode(version);
}
static void NormalSlotsBuild(int version) {
    int oldArea,oldSettings,oldReach,areaOffset,newSettings,newReach;
    PortalBuild(version);Encode(version);oldArea=geometryOffsets[7];oldSettings=settingsOffset;oldReach=reachOffset;
    areaOffset=sourceSize;newSettings=areaOffset+192;newReach=newSettings+112;
    memcpy(source+areaOffset,source+oldArea,144);memcpy(source+areaOffset+144,source+oldArea+48,48);Word(areaOffset+144,3);
    memcpy(source+newSettings,source+oldSettings,84);memcpy(source+newSettings+84,source+oldSettings+28,28);
    Word(newSettings+84+16,1);Word(newSettings+84+24,2);
    memcpy(source+newReach,source+oldReach,88);memcpy(source+newReach+88,source+oldReach+44,44);
    Word(12+8*AASLUMP_AREAS,areaOffset);Word(16+8*AASLUMP_AREAS,192);
    Word(12+8*AASLUMP_AREASETTINGS,newSettings);Word(16+8*AASLUMP_AREASETTINGS,112);
    Word(12+8*AASLUMP_REACHABILITY,newReach);Word(16+8*AASLUMP_REACHABILITY,132);
    Word(portalOffset+32,2);Word(clusterOffset+16,3);Word(clusterOffset+20,2);
    geometryOffsets[7]=areaOffset;geometryOffsets[8]=settingsOffset=newSettings;reachOffset=newReach;
    sourceSize=advertised=readable=newReach+132;Encode(version);
}
static void TwoPortalsBuild(int version) {
    int oldArea,oldSettings,oldPortal,areaOffset,newSettings,newPortal,newIndex;
    PortalBuild(version);Encode(version);oldArea=geometryOffsets[7];oldSettings=settingsOffset;oldPortal=portalOffset;
    areaOffset=sourceSize;newSettings=areaOffset+192;newPortal=newSettings+112;newIndex=newPortal+60;
    memcpy(source+areaOffset,source+oldArea,144);memcpy(source+areaOffset+144,source+oldArea+96,48);Word(areaOffset+144,3);
    memcpy(source+newSettings,source+oldSettings,84);memcpy(source+newSettings+84,source+oldSettings+56,28);Word(newSettings+84+12,0xfffffffeu);
    memcpy(source+newPortal,source+oldPortal,40);memcpy(source+newPortal+40,source+oldPortal+20,20);
    Word(newPortal+40,3);Word(newPortal+52,2);Word(newPortal+56,1);
    Word(newIndex,1);Word(newIndex+4,2);Word(newIndex+8,1);Word(newIndex+12,2);
    Word(12+8*AASLUMP_AREAS,areaOffset);Word(16+8*AASLUMP_AREAS,192);
    Word(12+8*AASLUMP_AREASETTINGS,newSettings);Word(16+8*AASLUMP_AREASETTINGS,112);
    Word(12+8*AASLUMP_PORTALS,newPortal);Word(16+8*AASLUMP_PORTALS,60);
    Word(12+8*AASLUMP_PORTALINDEX,newIndex);Word(16+8*AASLUMP_PORTALINDEX,16);
    Word(clusterOffset+16,3);Word(clusterOffset+24,2);Word(clusterOffset+32,2);Word(clusterOffset+40,2);Word(clusterOffset+44,2);
    geometryOffsets[7]=areaOffset;geometryOffsets[8]=settingsOffset=newSettings;portalOffset=newPortal;indexOffset=newIndex;
    sourceSize=advertised=readable=newIndex+16;Encode(version);
}
static void SameClusterBuild(int version) {
    PortalBuild(version);PortalWord(version,portalOffset+28,1);PortalWord(version,portalOffset+36,0);
    PortalWord(version,clusterOffset+24,2);PortalWord(version,clusterOffset+32,0);PortalWord(version,clusterOffset+40,0);
}
static void ClusterMapping(void) {
    int version,kind;
    for(version=4;version<=5;version++) {
        for(kind=0;kind<3;kind++) {
            if(kind==0)NormalSlotsBuild(version);else if(kind==1)ReachablePortalBuild(version);else TwoPortalsBuild(version);
            Counters();OldWorld();
            Check(AAS_LoadAASFile("fixture.aas")==BLERR_NOERROR&&aasworld.loaded,"native reachable-prefix and multi-area/portal mappings remain accepted");
            Check(!memcmp(aasworld.areasettings,source+settingsOffset,kind==1?84:112)&&!memcmp(aasworld.clusters,source+clusterOffset,48)&&!memcmp(aasworld.portals,source+portalOffset,kind==2?60:40),"native full mapping retains settings/cluster/portal bytes");
            Check(workspaceRequests==4&&workspaceFrees==4&&!workspacePointer,"all native mapping workspaces physically release");
        }
        NormalSlotsBuild(version);Counters();OldWorld();PortalWord(version,settingsOffset+84+16,0);GeometryReject();
        NormalSlotsBuild(version);Counters();OldWorld();PortalWord(version,settingsOffset+84+16,2);PortalWord(version,portalOffset+32,1);GeometryReject();
        ReachablePortalBuild(version);Counters();OldWorld();PortalWord(version,portalOffset+32,0);GeometryReject();
        SameClusterBuild(version);Counters();OldWorld();GeometryReject();
        PortalBuild(version);Counters();OldWorld();PortalWord(version,clusterOffset+40,0);GeometryReject();
        TwoPortalsBuild(version);Counters();OldWorld();PortalWord(version,indexOffset+12,1);GeometryReject();
        PortalBuild(version);Counters();OldWorld();PortalWord(version,clusterOffset+20,0);GeometryReject();
        PortalBuild(version);Counters();OldWorld();failWorkspace=4;GeometryReject();
        Check(workspaceRequests==4&&workspaceFrees==3&&!workspacePointer,"cluster mapping workspace failure retains no temporary owner");
    }
}
static void ReachableOrphanBuild(int version) {
    NormalSlotsBuild(version);PortalWord(version,settingsOffset+84+12,0);
    PortalWord(version,clusterOffset+16,2);PortalWord(version,clusterOffset+20,1);PortalWord(version,portalOffset+32,1);
}
static void ReachableOrphans(void) {
    int version;
    for(version=4;version<=5;version++) {
        ReachableOrphanBuild(version);Counters();OldWorld();GeometryReject();
        Check(workspaceRequests==3&&workspaceFrees==3&&!workspacePointer,"reachable orphan rejects before slot workspace");
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
    else if(proof==2){PortalSpanBuild(4,0);Counters();OldWorld();GeometryReject();}
    else if(proof==3){NormalSlotsBuild(4);Counters();OldWorld();PortalWord(4,settingsOffset+84+16,0);GeometryReject();}
    else if(proof==4){PortalBuild(4);Counters();OldWorld();PortalWord(4,clusterOffset+40,0);GeometryReject();}
    else if(proof==5){SameClusterBuild(4);Counters();OldWorld();GeometryReject();}
    else if(proof==6){ReachableOrphanBuild(4);Counters();OldWorld();GeometryReject();}
    else {ValidPortals();UnclusteredPortals();BadPortals();PortalSpanOwnership();ClusterMapping();ReachableOrphans();puts("Native AAS portal/cluster references, local slots, inverse ownership and spans passed (issue #47)");}
    ResetArena();return 0;
}
