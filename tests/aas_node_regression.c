/* Issue #47: actual loader node/root/leaf and graph termination checks. */
#define main AASGeometryFixtureMain
#include "aas_geometry_regression.c"
#undef main
#ifdef Q3_AAS_POINT_BODY
#include Q3_AAS_POINT_BODY
#endif
static void NodeBuild(int version,int count) {
    int extra,offset;
    GeometryBuild(version,0);Encode(version);offset=geometryOffsets[AASLUMP_NODES];extra=count*12-24;
    memset(source+offset,0,count*12);sourceSize+=extra;advertised=readable=sourceSize;
    Word(16+8*AASLUMP_NODES,count*12);Word(offset+12+4,0xffffffffu);Encode(version);
}
static void NodeWord(int version,int node,int field,uint32_t value) {Encode(version);Word(geometryOffsets[AASLUMP_NODES]+node*12+field,value);Encode(version);}
static void NodeFailures(void) {
    int version,kind;
    for(version=4;version<=5;version++)for(kind=0;kind<10;kind++) {
        NodeBuild(version,4);Counters();OldWorld();
        if(kind==0)NodeWord(version,1,4,1);
        else if(kind==1)NodeWord(version,1,8,1);
        else if(kind==2){NodeWord(version,1,4,2);NodeWord(version,2,8,1);}
        else if(kind==3)NodeWord(version,2,4,2);
        else if(kind==4)NodeWord(version,1,4,4);
        else if(kind==5)NodeWord(version,1,4,0x80000000u);
        else if(kind==6)NodeWord(version,1,8,0xfffffffeu);
        else if(kind==7)NodeWord(version,1,0,0xffffffffu);
        else if(kind==8)NodeWord(version,1,0,2);
        else{Encode(version);Word(16+8*AASLUMP_AREASETTINGS,28);Encode(version);}
        GeometryReject();
    }
    for(version=4;version<=5;version++){NodeBuild(version,2);Counters();OldWorld();Encode(version);Word(16+8*AASLUMP_NODES,12);Encode(version);GeometryReject();}
}
static void WorkspaceFailure(void) {
    int version;
    for(version=4;version<=5;version++){
        NodeBuild(version,4);Counters();OldWorld();failWorkspace=1;GeometryReject();
        Check(workspaceRequests==1&&!workspaceFrees&&!workspacePointer,"workspace failure has no temporary ownership");
    }
}
static void NativeNodes(void) {
    int version,kind,count,i;
    for(version=4;version<=5;version++)for(kind=0;kind<5;kind++) {
        count=kind==0?2:kind==1?3:kind==2?5:kind==3?64:256;
        NodeBuild(version,count);Counters();OldWorld();
        for(i=1;i<count;i++){if(i+1<count)NodeWord(version,i,4,i+1);NodeWord(version,i,8,0);}
        NodeWord(version,count-1,4,0xffffffffu);
        if(kind==2){NodeWord(version,1,4,2);NodeWord(version,1,8,3);NodeWord(version,2,4,4);NodeWord(version,3,4,4);}
        Check(AAS_LoadAASFile("fixture.aas")==BLERR_NOERROR&&aasworld.loaded&&!opened&&closes==1,"native chains and shared acyclic node graph accepted without recursion");
        Check(workspaceRequests==1&&workspaceFrees==1&&!workspacePointer,"graph workspace physically released on success");
        #ifdef Q3_AAS_POINT_BODY
        {vec3_t front={4,4,16},back={4,4,-16};Check(AAS_PointAreaNum(front)==1&&AAS_PointAreaNum(back)==0,"actual native point query retains area/solid results on accepted graphs");}
#endif
        Check(!memcmp(aasworld.nodes,source+geometryOffsets[AASLUMP_NODES],count*12),"node validation retains every native literal node byte");
    }
}
int main(int argc,char **argv) {
    int proof=argc>1?atoi(argv[1]):-1;botimport.Print=Print;botimport.FS_FOpenFile=Open;botimport.FS_Read=Read;botimport.FS_Seek=Seek;botimport.FS_FCloseFile=Close;
    if(proof==0){NodeBuild(4,2);Counters();OldWorld();NodeWord(4,1,4,1);GeometryReject();}
    else if(proof==1){NodeBuild(4,2);Counters();OldWorld();NodeWord(4,1,0,INT_MAX);GeometryReject();}
    else{NodeFailures();NativeNodes();WorkspaceFailure();puts("Native AAS node/root/leaf and acyclic termination validation passed (issue #47)");}
    ResetArena();return 0;
}
