/* Actual filesystem/mount/read functions and legacy unzip against real ZIPs. */
#include "../code/qcommon/files.c"
#ifndef Q3_UNZIP_SOURCE
#define Q3_UNZIP_SOURCE "../code/qcommon/unzip.c"
#endif
#include Q3_UNZIP_SOURCE
/* Allocation imports own real storage; filesystem and decoder bodies are native. */
static void *zone[32], *temporary[16];
static int zoneLive, temporaryLive, zoneRequests, temporaryRequests;
static unsigned long largestZone;
qboolean com_fullyInitialized;
cvar_t *com_journal;
fileHandle_t com_journalDataFile;
static cvar_t debugVar, restrictVar, copyVar;
static searchpath_t search;
static void Check(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr,"ZIP regression failed: %s\n", message);
        exit(1);
    }
}

void QDECL Com_Error(int level, const char *format, ...) {
    (void)level;
    (void)format;
    Check(0,"unexpected engine fatal/drop");
}

void QDECL Com_Printf(const char *format, ...) {
    (void)format;
}

void QDECL Com_DPrintf(const char *format, ...) {
    (void)format;
}

void *Z_Malloc(int size) {
    int i;
    Check(size >= 0 && size <= 67108864,"bounded signed native zone request");
    zoneRequests++;
    if ((unsigned long)size > largestZone) largestZone = size;
    for(i=0;i<32;i++)if(!zone[i]){
        zone[i]=calloc(1,size?size:1);
        Check(zone[i]!=NULL,"physical native zone owner");
        zoneLive++;
        return zone[i];
    }
    Check(0,"bounded zone owners");
    return NULL;
}

void Z_Free(void *p) {
    int i;
    for(i=0;i<32;i++)if(zone[i]==p){
        free(p);
        zone[i]=NULL;
        zoneLive--;
        return;
    }
    Check(0,"physical zone owner frees once");
}

void *Hunk_AllocateTempMemory(int size) {
    int i;
    Check(size >= 0 && size <= 67108864,"bounded signed temporary request");
    temporaryRequests++;
    for(i=0;i<16;i++)if(!temporary[i]){
        temporary[i]=malloc(size?size:1);
        Check(temporary[i]!=NULL,"physical native temporary owner");
        temporaryLive++;
        return temporary[i];
    }
    Check(0,"bounded temporary owners");
    return NULL;
}

void Hunk_FreeTempMemory(void *p) {
    int i;
    for(i=0;i<16;i++)if(temporary[i]==p){
        free(p);
        temporary[i]=NULL;
        temporaryLive--;
        return;
    }
    Check(0,"physical temporary owner frees once");
}

void Hunk_ClearTempMemory(void) {
    Check(!temporaryLive,"temporary arena clears only after all owners release");
}

void Sys_EndStreamedFile(fileHandle_t f) {
    (void)f;
    Check(0,"unexpected stream callback");
}

void Sys_Mkdir(const char *p) {
    (void)p;
}

#ifndef Com_Memset
void Com_Memset(void *out,int value,size_t size){
    memset(out,value,size);
}

#endif
#ifndef Com_Memcpy
void Com_Memcpy(void *out,const void *in,size_t size){
    memcpy(out,in,size);
}

#endif
static void Begin(void) {
    Check(!zoneLive&&!temporaryLive,"prior physical owners release");
    memset(fsh,0,sizeof(fsh));
    memset(&search,0,sizeof(search));
    fs_searchpaths=&search;
    fs_debug=&debugVar;
    fs_restrict=&restrictVar;
    fs_copyfiles=&copyVar;
    zoneRequests=temporaryRequests=0;
    largestZone=0;
    fs_packFiles=fs_loadCount=fs_loadStack=0;
}

static void End(void) {
    int i;
    if(search.pack){
        unzClose(search.pack->handle);
        Z_Free(search.pack->buildBuffer);
        Z_Free(search.pack);
        search.pack=NULL;
    }
    for(i=0;i<MAX_FILE_HANDLES;i++)Check(!fsh[i].handleFiles.file.o&&!fsh[i].buffer,"all native handles clear");
    Check(!zoneLive&&!temporaryLive&&!fs_loadStack,"all filesystem owners release physically");
    fs_searchpaths=NULL;
}

static void Golden(char *path) {
    fileHandle_t f;
    char text[64];
    void *bytes=NULL;
    int n;
    Begin();
    search.pack=FS_LoadZipFile(path,"native.pk3");
    Check(search.pack&&search.pack->numfiles==1,"actual native ZIP mount");
    n=FS_FOpenFileRead("native.txt",&f,qtrue);
    Check(n==12&&f>0&&fsh[f].buffer&&!fsh[f].handleFiles.file.z,"native unique buffered open");
    Check(FS_Read(text,64,f)==12&&!memcmp(text,"native data\n",12),"native buffered payload and EOF");
    FS_FCloseFile(f);
    n=FS_ReadFile("native.txt",&bytes);
    Check(n==12&&bytes&&!memcmp(bytes,"native data\n",12)&&((char*)bytes)[12]==0,"native shared full-file payload and trailing byte");
    FS_FreeFile(bytes);
    End();
}

static void AllocationGolden(void) {
    void *p;
    int before;
    Begin();
    before=zoneRequests;
    p=zcalloc(NULL,32,4);
    Check(p&&zoneRequests==before+1&&zoneLive==1&&largestZone==128,"native complete inflate allocation");
    {
        int i;
        for(i=0;i<128;i++)Check(!((unsigned char*)p)[i],"native inflate allocation remains cleared");
    }
    zcfree(NULL,p);
    p=zcalloc(NULL,0,4);
    Check(p&&zoneLive==1,"native zero product retains ordinary zone owner");
    zcfree(NULL,p);
    End();
}

static void AllocationInvalid(int kind) {
    int before;
    Begin();
    before=zoneRequests;
    Check(!zcalloc(NULL,kind==0?UINT_MAX:kind==1?INT_MAX:UINT_MAX,kind==0?4:kind==1?2:UINT_MAX)&&zoneRequests==before&&!zoneLive,"unrepresentable inflate allocation rejects before native zone imports");
    End();
}

int main(int argc,char **argv){
    int i;
    Check(argc>=2,"one real ZIP input");
    if(argc>2){
        i=atoi(argv[2]);
        if(i<3)AllocationInvalid(i);
        else if(i==99)AllocationGolden();
        else{
            Golden(argv[1]);
            AllocationGolden();
        }
        return 0;
    }
    Golden(argv[1]);
    AllocationGolden();
    for(i=0;i<3;i++)AllocationInvalid(i);
    puts("Actual native ZIP/inflate callbacks, checked allocation costs, payload and physical ownership pass");
    return 0;
}
