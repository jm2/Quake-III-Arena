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
void FreeMemory(void *pointer) {int i;for(i=0;i<allocations;i++)if(arena[i]==pointer){Check(!released[i],"native logical release once");released[i]=1;releases++;return;}Check(0,"unknown native logical release");}
static void ResetArena(void) {while(allocations)free(arena[--allocations]);memset(&aasworld,0,sizeof(aasworld));releases=0;}
static int Open(const char *name,fileHandle_t *file,fsMode_t mode) {Check(name&&!strcmp(name,"fixture.aas")&&mode==FS_READ&&!opened,"native file open contract");opens++;if(missing){*file=0;return -1;}opened=1;*file=39;position=0;return advertised;}
static int Read(void *buffer,int length,fileHandle_t file) {int n;Check(opened&&file==39&&buffer&&length>=0&&position>=0,"native bounded read contract");reads++;n=position<readable?readable-position:0;if(n>length)n=length;if(shortRead==reads&&n)n--;memcpy(buffer,source+position,n);position+=n;return n;}
static int Seek(fileHandle_t file,long offset,int origin) {Check(opened&&file==39&&origin==FS_SEEK_SET&&offset>=0&&offset<=readable,"native bounded seek contract");seeks++;if(failSeek==seeks)return -1;position=(int)offset;return 0;}
static void Close(fileHandle_t file) {Check(opened&&file==39,"native file close once");opened=0;closes++;}
static void Word(int offset,uint32_t value) {int i;for(i=0;i<4;i++)source[offset+i]=(unsigned char)(value>>(8*i));}
static const int sizes[AAS_LUMPS]={sizeof(aas_bbox_t),sizeof(aas_vertex_t),sizeof(aas_plane_t),sizeof(aas_edge_t),sizeof(aas_edgeindex_t),sizeof(aas_face_t),sizeof(aas_faceindex_t),sizeof(aas_area_t),sizeof(aas_areasettings_t),sizeof(aas_reachability_t),sizeof(aas_node_t),sizeof(aas_portal_t),sizeof(aas_portalindex_t),sizeof(aas_cluster_t)};
static void Encode(int version) {int i;if(version==AASVERSION)for(i=0;i<(int)sizeof(aas_header_t)-8;i++)source[8+i]^=(unsigned char)(i*119);}
static void Build(int version,int empty) {int lump,offset=sizeof(aas_header_t);memset(source,0,sizeof(source));Word(0,AASID);Word(4,version);Word(8,12345);for(lump=0;lump<AAS_LUMPS;lump++){int n=empty?0:sizes[lump];Word(12+lump*8,offset);Word(16+lump*8,n);offset+=n;}sourceSize=advertised=readable=offset;Encode(version);}
static void Counters(void) {Check(!opened,"fixture starts with closed file");opens=closes=reads=seeks=missing=failSeek=shortRead=allocationRequests=failAllocation=0;}
static void OldWorld(void) {ResetArena();aasworld.vertexes=GetHunkMemory(2*sizeof(aas_vertex_t));aasworld.numvertexes=2;aasworld.loaded=aasworld.initialized=aasworld.savefile=1;aasworld.bspchecksum=54321;aasworld.time=37;strcpy(aasworld.mapname,"previous");}
static void RejectPreserving(int expected) {aas_t before=aasworld;int previousAllocations=allocations,previousReleases=releases,result;result=AAS_LoadAASFile("fixture.aas");Check(result==expected,"native rejection code");Check(!opened&&closes==(!missing)&&opens==1,"all rejection file paths close once");Check(!memcmp(&aasworld,&before,sizeof(before))&&allocations==previousAllocations&&releases==previousReleases,"layout rejection preserves prior world and arena ownership");}
static void HeaderFailures(void) {
    int prefix,version;
    for(version=AASVERSION_OLD;version<=AASVERSION;version++) {
        for(prefix=0;prefix<(int)sizeof(aas_header_t);prefix++) {
            Build(version,0);Counters();OldWorld();advertised=readable=prefix;RejectPreserving(BLERR_CANNOTREADAASLUMP);
            Build(version,0);Counters();OldWorld();readable=prefix;RejectPreserving(BLERR_CANNOTREADAASLUMP);
        }
        Build(version,0);Counters();OldWorld();missing=1;RejectPreserving(BLERR_CANNOTOPENAASFILE);
        Build(version,0);Counters();OldWorld();Word(0,0);RejectPreserving(BLERR_WRONGAASFILEID);
        Build(version,0);Counters();OldWorld();Word(4,999);RejectPreserving(BLERR_WRONGAASFILEVERSION);
        Build(version,0);Counters();OldWorld();Encode(version);Word(8,98765);Encode(version);RejectPreserving(BLERR_WRONGAASFILEVERSION);
        Build(version,0);Counters();OldWorld();shortRead=1;RejectPreserving(BLERR_CANNOTREADAASLUMP);
        Build(version,0);Counters();OldWorld();advertised=-1;RejectPreserving(BLERR_CANNOTREADAASLUMP);
    }
}
static void LumpFailures(void) {
    int version,lump,mode;
    for(version=AASVERSION_OLD;version<=AASVERSION;version++)for(lump=0;lump<AAS_LUMPS;lump++)for(mode=0;mode<10;mode++) {
        uint32_t offset=sizeof(aas_header_t),length=sizes[lump];
        Build(version,0);Counters();OldWorld();Encode(version);
        if(mode==0)offset=0xffffffffu;else if(mode==1)length=0xffffffffu;else if(mode==2)offset=sourceSize+1;
        else if(mode==3)length=sourceSize;else if(mode==4)length=sizes[lump]-1;else if(mode==5)offset=sizeof(aas_header_t)-1;
        else if(mode==6)offset=INT_MAX;else if(mode==7)length=INT_MAX;else if(mode==8){offset=sourceSize+1;length=0;}else{offset=0xffffffffu;length=0;}
        Word(12+lump*8,offset);Word(16+lump*8,length);Encode(version);RejectPreserving(BLERR_CANNOTREADAASLUMP);
    }
}
static void ValidLayouts(void) {
    const int wireSizes[AAS_LUMPS]={32,12,20,8,4,24,4,48,28,44,12,20,4,16};
    int version,lump,empty,reversed;
    for(lump=0;lump<AAS_LUMPS;lump++)Check(sizes[lump]==wireSizes[lump],"commercial fixed native element layouts");
    for(version=AASVERSION_OLD;version<=AASVERSION;version++)for(empty=0;empty<2;empty++)for(reversed=0;reversed<2;reversed++) {
        int offset;
        Build(version,empty);Counters();OldWorld();Encode(version);offset=sizeof(aas_header_t);
        if(reversed)for(lump=AAS_LUMPS-1;lump>=0;lump--){Word(12+lump*8,offset);offset+=empty?0:sizes[lump];}
        Encode(version);
        Check(AAS_LoadAASFile("fixture.aas")==BLERR_NOERROR&&aasworld.loaded&&!aasworld.initialized&&!aasworld.savefile&&aasworld.bspchecksum==12345,"native accepted v4/v5 layouts and scalar state");
        Check(!opened&&closes==1&&allocations==15&&releases==1,"one file close and native arena replacement ownership");
        Check(aasworld.numbboxes==!empty&&aasworld.numvertexes==!empty&&aasworld.numplanes==!empty&&aasworld.numedges==!empty&&aasworld.edgeindexsize==!empty&&aasworld.numfaces==!empty&&aasworld.faceindexsize==!empty&&aasworld.numareas==!empty&&aasworld.numareasettings==!empty&&aasworld.reachabilitysize==!empty&&aasworld.numnodes==!empty&&aasworld.numportals==!empty&&aasworld.portalindexsize==!empty&&aasworld.numclusters==!empty,"all fourteen native lump counts");
        if(empty)Check(reads==1&&!seeks,"empty lump dummy storage needs no payload reads");else Check(reads==15&&seeks==(reversed?14:0),"sequential and reversed native exact reads/seeks");
        AAS_DumpAASData();Check(!aasworld.loaded&&releases==15,"all native logical arena owners released");
    }
}
static void ReorderedPayload(void) {
    int version;
    for(version=AASVERSION_OLD;version<=AASVERSION;version++) {
        float value;uint32_t bits;
        Build(version,0);Counters();OldWorld();Encode(version);Word(12,136);Word(20,124);Encode(version);
        value=1.25f;memcpy(&bits,&value,4);Word(124,bits);value=2.5f;memcpy(&bits,&value,4);Word(180,bits);
        Check(AAS_LoadAASFile("fixture.aas")==BLERR_NOERROR&&seeks==3&&!opened&&closes==1,"mixed sequential/nonsequential tracking uses actual offset plus length");
        Check(aasworld.vertexes[0][0]==1.25f&&aasworld.planes[0].dist==2.5f,"reordered typed payload retains independent literal wire values");
    }
}

static void AllocationFailures(void) {
    int empty,request;
    for(empty=0;empty<2;empty++)for(request=2;request<=15;request++) {
        Build(AASVERSION,empty);Counters();OldWorld();failAllocation=request;
        Check(AAS_LoadAASFile("fixture.aas")==BLERR_CANNOTREADAASLUMP&&!opened&&closes==1&&!aasworld.loaded&&!aasworld.initialized&&releases==allocations&&!aasworld.numvertexes&&!aasworld.vertexes,"all real/dummy native allocation failures close and clear partial logical world");
    }
}
static void ReadFailures(void) {
    int version,request;
    for(version=AASVERSION_OLD;version<=AASVERSION;version++)for(request=2;request<=15;request++) {
        Build(version,0);Counters();OldWorld();shortRead=request;
        Check(AAS_LoadAASFile("fixture.aas")==BLERR_CANNOTREADAASLUMP&&!opened&&closes==1&&!aasworld.loaded&&!aasworld.initialized&&!aasworld.numvertexes&&!aasworld.vertexes&&allocations==request&&releases==allocations,"each partial lump read closes once and clears partial logical owners");
    }
    for(request=1;request<=14;request++) {
        int lump,offset=sizeof(aas_header_t);Build(AASVERSION,0);Counters();OldWorld();Encode(AASVERSION);
        for(lump=AAS_LUMPS-1;lump>=0;lump--){Word(12+lump*8,offset);offset+=sizes[lump];}Encode(AASVERSION);failSeek=request;
        Check(AAS_LoadAASFile("fixture.aas")==BLERR_CANNOTREADAASLUMP&&!opened&&closes==1&&!aasworld.loaded&&releases==allocations,"each failed nonsequential seek closes/clears logical owners once");
    }
}

int main(int argc,char **argv) {int proof=argc>1?atoi(argv[1]):-1;botimport.Print=Print;botimport.FS_FOpenFile=Open;botimport.FS_Read=Read;botimport.FS_Seek=Seek;botimport.FS_FCloseFile=Close;
    if(proof==0){Build(AASVERSION_OLD,0);Counters();OldWorld();missing=1;RejectPreserving(BLERR_CANNOTOPENAASFILE);}
    else if(proof==1){Build(AASVERSION_OLD,0);Counters();OldWorld();Word(12,(uint32_t)-1);RejectPreserving(BLERR_CANNOTREADAASLUMP);}
    else if(proof==2){Build(AASVERSION_OLD,0);Counters();readable=sizeof(aas_header_t)-1;OldWorld();RejectPreserving(BLERR_CANNOTREADAASLUMP);}
    else {HeaderFailures();LumpFailures();ValidLayouts();ReorderedPayload();ReadFailures();AllocationFailures();puts("Native AAS v4/v5 layout, exact reads, prior-world preflight and file/logical-arena ownership checks passed (issue #47)");}
    ResetArena();return 0;
}
