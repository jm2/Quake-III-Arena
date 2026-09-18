/* Issue #47: actual native AAS file layout/read handling, isolated FS/arena. */
#include "../code/botlib/be_aas_file.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <stdarg.h>
aas_t aasworld;
botlib_import_t botimport;
static unsigned char source[8192];
static int sourceSize,advertised,readable,position,opened,opens,closes,reads,seeks,missing,failSeek,shortRead,allocations,releases,allocationRequests,failAllocation;
static void *workspacePointer;
static int workspaceRequests,workspaceFrees,failWorkspace;
static void *arena[64];static int released[64];
static void Check(int condition,const char *message) {if(!condition){fprintf(stderr,"AAS layout regression failed: %s\n",message);exit(1);}}
void QDECL Com_Error(int level,const char *format,...) {(void)level;(void)format;Check(0,"unexpected native error");exit(1);}
void QDECL Com_Printf(const char *format,...) {(void)format;}
#ifndef Com_Memcpy
void Com_Memcpy(void *out,const void *in,size_t n) {memcpy(out,in,n);}
#endif
#ifndef Com_Memset
void Com_Memset(void *out,int value,size_t n) {memset(out,value,n);}
#endif
void QDECL AAS_Error(char *format,...) {(void)format;}
static void QDECL Print(int type,char *format,...) {(void)type;(void)format;}
char *LibVarGetString(char *name) {Check(!strcmp(name,"sv_mapChecksum"),"native checksum variable");return "12345";}
void *GetHunkMemory(unsigned long size) {void *p;allocationRequests++;if(failAllocation==allocationRequests)return NULL;Check(size>0&&size<8192&&allocations<64,"bounded native arena request");p=calloc(1,size);Check(p!=NULL,"exact fixture arena allocation");released[allocations]=0;arena[allocations++]=p;return p;}
void *GetMemory(unsigned long size) {Check(!workspacePointer&&size>0&&size<8192,"bounded temporary node workspace");workspaceRequests++;if(failWorkspace)return NULL;workspacePointer=calloc(1,size);Check(workspacePointer!=NULL,"workspace fixture allocation");return workspacePointer;}
void FreeMemory(void *pointer) {int i;if(pointer==workspacePointer&&pointer){free(workspacePointer);workspacePointer=NULL;workspaceFrees++;return;}for(i=0;i<allocations;i++)if(arena[i]==pointer){Check(!released[i],"native logical release once");released[i]=1;releases++;return;}Check(0,"unknown native logical release");}
static void ResetArena(void) {Check(!workspacePointer,"temporary workspace physically released before arena reset");while(allocations)free(arena[--allocations]);memset(&aasworld,0,sizeof(aasworld));releases=0;}
static int Open(const char *name,fileHandle_t *file,fsMode_t mode) {Check(name&&!strcmp(name,"fixture.aas")&&mode==FS_READ&&!opened,"native file open contract");opens++;if(missing){*file=0;return -1;}opened=1;*file=39;position=0;return advertised;}
static int Read(void *buffer,int length,fileHandle_t file) {int n;Check(opened&&file==39&&buffer&&length>=0&&position>=0,"native bounded read contract");reads++;n=position<readable?readable-position:0;if(n>length)n=length;if(shortRead==reads&&n)n--;memcpy(buffer,source+position,n);position+=n;return n;}
static int Seek(fileHandle_t file,long offset,int origin) {Check(opened&&file==39&&origin==FS_SEEK_SET&&offset>=0&&offset<=readable,"native bounded seek contract");seeks++;if(failSeek==seeks)return -1;position=(int)offset;return 0;}
static void Close(fileHandle_t file) {Check(opened&&file==39,"native file close once");opened=0;closes++;}
static void Word(int offset,uint32_t value) {int i;for(i=0;i<4;i++)source[offset+i]=(unsigned char)(value>>(8*i));}
static const int sizes[AAS_LUMPS]={sizeof(aas_bbox_t),sizeof(aas_vertex_t),sizeof(aas_plane_t),sizeof(aas_edge_t),sizeof(aas_edgeindex_t),sizeof(aas_face_t),sizeof(aas_faceindex_t),sizeof(aas_area_t),sizeof(aas_areasettings_t),sizeof(aas_reachability_t),sizeof(aas_node_t),sizeof(aas_portal_t),sizeof(aas_portalindex_t),sizeof(aas_cluster_t)};
static void Encode(int version) {int i;if(version==AASVERSION)for(i=0;i<(int)sizeof(aas_header_t)-8;i++)source[8+i]^=(unsigned char)(i*119);}
static void Build(int version,int empty) {int lump,offset=sizeof(aas_header_t);memset(source,0,sizeof(source));Word(0,AASID);Word(4,version);Word(8,12345);for(lump=0;lump<AAS_LUMPS;lump++){int n=empty?0:sizes[lump];Word(12+lump*8,offset);Word(16+lump*8,n);offset+=n;}sourceSize=advertised=readable=offset;Encode(version);}
static void Counters(void) {Check(!opened,"fixture starts with closed file");workspaceRequests=workspaceFrees=failWorkspace=0;opens=closes=reads=seeks=missing=failSeek=shortRead=allocationRequests=failAllocation=0;}
static void OldWorld(void) {ResetArena();aasworld.vertexes=GetHunkMemory(2*sizeof(aas_vertex_t));aasworld.numvertexes=2;aasworld.loaded=aasworld.initialized=aasworld.savefile=1;aasworld.bspchecksum=54321;aasworld.time=37;strcpy(aasworld.mapname,"previous");}
static void RejectPreserving(int expected) {aas_t before=aasworld;int previousAllocations=allocations,previousReleases=releases,result;result=AAS_LoadAASFile("fixture.aas");Check(result==expected,"native rejection code");Check(!opened&&closes==(!missing)&&opens==1,"all rejection file paths close once");Check(!memcmp(&aasworld,&before,sizeof(before))&&allocations==previousAllocations&&releases==previousReleases,"layout rejection preserves prior world and arena ownership");}
static void GeometryFailures(void) {
    int version,kind;
    for(version=AASVERSION_OLD;version<=AASVERSION;version++)for(kind=0;kind<8;kind++) {
        int offset;
        Build(version,0);Counters();OldWorld();Encode(version);
        if(kind==0){offset=124+32;Word(offset,0x7f800000u);}
        else if(kind==1){offset=124+32+12;Word(offset+16,0xffffffffu);}
        else if(kind==2){offset=124+32+12+20;Word(offset,INT_MAX);}
        else if(kind==3){offset=124+32+12+20+8;Word(offset,0x80000000u);}
        else if(kind==4){offset=124+32+12+20+8+4;Word(offset+8,INT_MAX);}
        else if(kind==5){offset=124+32+12+20+8+4+24;Word(offset,0x80000000u);}
        else if(kind==6){offset=124+32+12+20+8+4+24+4;Word(offset+4,INT_MAX);}
        else{offset=124;Word(offset+8,0x7fc00000u);}
        Encode(version);
        Check(AAS_LoadAASFile("fixture.aas")==BLERR_CANNOTREADAASLUMP&&!opened&&closes==1&&!aasworld.loaded&&releases==allocations,"invalid numeric/reference geometry rejects before loaded publication and clears logical owners");
    }
}

static int geometryOffsets[AAS_LUMPS];
static const int geometryCounts[AAS_LUMPS]={2,3,2,4,3,2,1,2,2,0,2,0,0,0};
static void GeometryBuild(int version,int reverse) {
    int i,lump,offset=124;
    memset(source,0,sizeof(source));Word(0,AASID);Word(4,version);Word(8,12345);
    for(i=0;i<AAS_LUMPS;i++){lump=reverse?AAS_LUMPS-1-i:i;geometryOffsets[lump]=offset;Word(12+8*lump,offset);Word(16+8*lump,sizes[lump]*geometryCounts[lump]);offset+=sizes[lump]*geometryCounts[lump];}
    sourceSize=advertised=readable=offset;
    for(i=0;i<2;i++){offset=geometryOffsets[0]+i*32;Word(offset,i?4:2);Word(offset+8,0xc1700000u);Word(offset+12,0xc1700000u);Word(offset+16,0xc1c00000u);Word(offset+20,0x41700000u);Word(offset+24,0x41700000u);Word(offset+28,i?0x41000000u:0x42000000u);}
    Word(geometryOffsets[1]+12,0x41800000u);Word(geometryOffsets[1]+28,0x41800000u);
    Word(geometryOffsets[2]+8,0x3f800000u);Word(geometryOffsets[2]+16,2);Word(geometryOffsets[2]+28,0xbf800000u);Word(geometryOffsets[2]+36,2);
    Word(geometryOffsets[3]+8,0);Word(geometryOffsets[3]+12,1);Word(geometryOffsets[3]+16,1);Word(geometryOffsets[3]+20,2);Word(geometryOffsets[3]+24,2);Word(geometryOffsets[3]+28,0);
    for(i=0;i<3;i++)Word(geometryOffsets[4]+4*i,i+1);
    offset=geometryOffsets[5]+24;Word(offset+4,FACE_GROUND);Word(offset+8,3);Word(offset+16,1);Word(geometryOffsets[6],1);
    offset=geometryOffsets[7]+48;Word(offset,1);Word(offset+4,1);Word(offset+24,0x41800000u);Word(offset+28,0x41800000u);Word(offset+36,0x40a80000u);Word(offset+40,0x40a80000u);
    Word(geometryOffsets[8]+28+8,PRESENCE_NORMAL);Word(geometryOffsets[10]+12+4,0xffffffffu);
    Encode(version);
}
static void GeometryReject(void) {
    Check(AAS_LoadAASFile("fixture.aas")==BLERR_CANNOTREADAASLUMP&&!opened&&closes==1&&!aasworld.loaded&&!aasworld.initialized&&releases==allocations&&!aasworld.numareas&&!aasworld.areas,"invalid geometry cannot publish loaded or retain partial logical owners");
}
static void ValidGeometry(void) {
    int version,reverse,signedIndexes,planeType;
    for(version=4;version<=5;version++)for(reverse=0;reverse<2;reverse++)for(signedIndexes=0;signedIndexes<2;signedIndexes++)for(planeType=0;planeType<=5;planeType++) {
        const void *data[AAS_LUMPS];int lump;
        GeometryBuild(version,reverse);Counters();OldWorld();Encode(version);
        Word(geometryOffsets[2]+16,planeType);Word(geometryOffsets[2]+36,planeType);
        Word(geometryOffsets[5]+28,signedIndexes?0xffffffffu:0x7fffffffu);
        if(signedIndexes){Word(geometryOffsets[4],0xffffffffu);Word(geometryOffsets[6],0xffffffffu);}Encode(version);
        Check(AAS_LoadAASFile("fixture.aas")==BLERR_NOERROR&&aasworld.loaded&&!opened&&closes==1,"native finite geometry, paired planes, all six plane types and signed orientations remain accepted");
        data[0]=aasworld.bboxes;data[1]=aasworld.vertexes;data[2]=aasworld.planes;data[3]=aasworld.edges;data[4]=aasworld.edgeindex;data[5]=aasworld.faces;data[6]=aasworld.faceindex;data[7]=aasworld.areas;data[8]=aasworld.areasettings;data[9]=aasworld.reachability;data[10]=aasworld.nodes;data[11]=aasworld.portals;data[12]=aasworld.portalindex;data[13]=aasworld.clusters;
        for(lump=0;lump<AAS_LUMPS;lump++)Check(!memcmp(data[lump],source+geometryOffsets[lump],sizes[lump]*geometryCounts[lump]),"accepted native typed geometry retains every literal wire byte");
    }
}
static void NonfiniteGeometry(void) {
    const uint32_t bad[]={0x7f800000u,0xff800000u,0x7fc00000u,0xffc00000u,0x7f800001u,0xff800001u};
    int fields[64],count=0,i,j,version;size_t pattern;
    GeometryBuild(4,0);
    for(i=0;i<2;i++)for(j=8;j<32;j+=4)fields[count++]=geometryOffsets[0]+i*32+j;
    for(j=0;j<36;j+=4)fields[count++]=geometryOffsets[1]+j;
    for(i=0;i<2;i++)for(j=0;j<16;j+=4)fields[count++]=geometryOffsets[2]+i*20+j;
    for(i=0;i<2;i++)for(j=12;j<48;j+=4)fields[count++]=geometryOffsets[7]+i*48+j;
    Check(count==47,"all independently enumerated geometric float fields covered");
    for(version=4;version<=5;version++)for(i=0;i<count;i++)for(pattern=0;pattern<sizeof(bad)/sizeof(bad[0]);pattern++) {
        GeometryBuild(version,0);Counters();OldWorld();Encode(version);Word(fields[i],bad[pattern]);Encode(version);GeometryReject();
    }
}
static void ReferenceGeometry(void) {
    const uint32_t huge[]={0x7fffffffu,0x80000000u};int version,lump,field,pattern;
    const struct {int lump,field;uint32_t bad;} cases[]={
        {0,8,0x42040000u},{7,60,0x41880000u},{7,48,2},
        {2,16,0xffffffffu},{2,16,6},{2,36,6},
        {3,8,3},{3,12,0xffffffffu},{3,16,3},{3,20,3},{3,24,3},{3,28,3},
        {4,0,4},{4,0,0xfffffffcu},{6,0,2},{6,0,0xfffffffeu},
        {5,24,2},{5,32,4},{5,36,1},{5,40,2},{5,44,2},
        {7,52,2},{7,56,2}
    };size_t i;
    for(version=4;version<=5;version++) {
        for(i=0;i<sizeof(cases)/sizeof(cases[0]);i++){GeometryBuild(version,0);Counters();OldWorld();Encode(version);Word(geometryOffsets[cases[i].lump]+cases[i].field,cases[i].bad);Encode(version);GeometryReject();}
        for(pattern=0;pattern<2;pattern++)for(lump=3;lump<=7;lump++) {
            int widths=lump==3?8:lump==4?3:lump==5?6:lump==6?1:3;
            for(field=0;field<widths;field++){int byte; if(lump==5&&field==1)continue; byte=lump==5?24+field*4:lump==7?48+field*4:field*4;GeometryBuild(version,0);Counters();OldWorld();Encode(version);Word(geometryOffsets[lump]+byte,huge[pattern]);Encode(version);GeometryReject();}
        }
        GeometryBuild(version,0);Counters();OldWorld();Encode(version);Word(16+8*AASLUMP_PLANES,20);Encode(version);GeometryReject();
    }
}
int main(int argc,char **argv) {
    int proof=argc>1?atoi(argv[1]):-1;botimport.Print=Print;botimport.FS_FOpenFile=Open;botimport.FS_Read=Read;botimport.FS_Seek=Seek;botimport.FS_FCloseFile=Close;
    if(proof==0){Build(AASVERSION_OLD,0);Counters();OldWorld();Word(164,0x7f800000u);Check(AAS_LoadAASFile("fixture.aas")==BLERR_CANNOTREADAASLUMP,"nonfinite AAS vertex rejected before loaded");}
    else if(proof==1){Build(AASVERSION_OLD,0);Counters();OldWorld();Word(124+32+12+20,INT_MAX);Check(AAS_LoadAASFile("fixture.aas")==BLERR_CANNOTREADAASLUMP,"out-of-range AAS vertex reference rejected before loaded");}
    else{GeometryFailures();ValidGeometry();NonfiniteGeometry();ReferenceGeometry();puts("Native AAS geometry numeric/reference rejection and logical ownership passed (issue #47)");}
    ResetArena();return 0;
}
