/* Issue #46: native shader archive input, allocation and restart ownership. */
#define main ShaderStageFixtureMain
#include "shader_stage_regression.c"
#undef main
#include <setjmp.h>
#include <limits.h>

static jmp_buf drop;
static int listCount, listPresent, listReleased, fileCount, fileReleased;
static int failRead, reportedLength, checkReverse;
static int reported[MAX_SHADER_FILES + 2];
static char names[MAX_SHADER_FILES + 2][32];
static char *files[MAX_SHADER_FILES + 3];
static void *fileOwned[MAX_SHADER_FILES + 2];
static const char *contents[MAX_SHADER_FILES + 2];
static char longName[MAX_QPATH + 1];
static int caught;

static void QDECL Drop(int level, const char *format, ...) {
    (void)format;
    Check(level == ERR_DROP, "native archive rejects with drop severity");
    caught++;
    longjmp(drop, 1);
}
static char **ArchiveList(const char *path, const char *extension, int *count) {
    Check(!strcmp(path,"scripts") && !strcmp(extension,".shader"), "archive list parameters");
    *count = listCount;
    return listPresent ? files : NULL;
}
static void ArchiveListFree(char **list) {
    Check(list == files && listPresent && !listReleased, "archive list released once");
    listReleased++;
}
static int ArchiveRead(const char *path, void **buffer) {
    int i = fileCount++, n;
    char expected[MAX_QPATH];
    Check(i < MAX_SHADER_FILES, "legacy maximum archive count");
    Com_sprintf(expected, sizeof(expected), "scripts/%s", files[i]);
    Check(!strcmp(path, expected), "native archive file read order/path");
    *buffer = NULL;
    if (i == failRead) return -1;
    n = strlen(contents[i]);
    fileOwned[i] = malloc(n + 1);
    Check(fileOwned[i] != NULL, "fixture archive input allocation");
    memcpy(fileOwned[i], contents[i], n + 1);
    *buffer = fileOwned[i];
    return reported[i] ? reported[i] : reportedLength ? reportedLength : n;
}
static void ArchiveFree(void *pointer) {
    int i, last = -1;
    for (i = 0; i < fileCount; i++) if (fileOwned[i]) last = i;
    Check(pointer && last >= 0, "archive frees an owned input");
    for (i = 0; i < fileCount; i++) if (fileOwned[i] == pointer) break;
    Check(i < fileCount && (!checkReverse || i == last), "archive inputs freed in reverse acquisition order");
    free(pointer); fileOwned[i] = NULL; fileReleased++;
}
static void Setup(int count) {
    int i;
    Release();
    memset(shaderTextHashTable, 0, sizeof(shaderTextHashTable));
    s_shaderText = NULL;
    memset(fileOwned, 0, sizeof(fileOwned));
    memset(reported, 0, sizeof(reported));
    listCount=count; listPresent=1; listReleased=0; fileCount=0; fileReleased=0;
    failRead=-1; reportedLength=0; checkReverse=1; caught=0;
    for(i=0;i<MAX_SHADER_FILES+2;i++) {
        snprintf(names[i], sizeof(names[i]), "fixture-%d.shader", i);
        files[i]=names[i]; contents[i]="";
    }
    files[MAX_SHADER_FILES+2]=NULL;
    ri.Printf=Print; ri.Error=Drop; ri.Hunk_Alloc=Allocate;
    ri.FS_ListFiles=ArchiveList; ri.FS_FreeFileList=ArchiveListFree;
    ri.FS_ReadFile=ArchiveRead; ri.FS_FreeFile=ArchiveFree;
    ri.CIN_PlayCinematic=Video;
    tr.whiteImage=&white; tr.defaultImage=&white;
}
static void Balanced(int reads, int frees) {
    int i;
    Check(fileCount==reads && fileReleased==frees && listReleased==listPresent, "complete archive input/list ownership");
    for(i=0;i<fileCount;i++) Check(!fileOwned[i], "no archive file remains owned");
}
static void NoArchives(void) {
    int present;
    for(present=0;present<2;present++) {
        shader_t *material; int before;
        Setup(0); listPresent=present;
        R_InitShaders(); Balanced(0,0);
        Check(s_shaderText==NULL, "no archive text is published");
        material=R_FindShader("textures/implicit",LIGHTMAP_NONE,qtrue);
        Check(material && !material->defaultShader && material->numUnfoggedPasses==1, "no-file startup preserves implicit shaders");
        before=allocations;
        Check(R_FindShader("textures/implicit",LIGHTMAP_NONE,qtrue)==material && before==allocations, "implicit shader cache reuse without archives");
    }
}
static void EmptyList(void) { Setup(0); ScanAndLoadShaderFiles(); Balanced(0,0); }
static void ReadFailure(void) {
    Setup(3); contents[0]="tests/first\n{\n{\nmap $whiteimage\n}\n}\n";
    failRead=1;
    if(!setjmp(drop)) { ScanAndLoadShaderFiles(); Check(0,"failed read must reject"); }
    Check(caught==1, "one controlled read failure"); Balanced(2,1);
    Check(!s_shaderText && !allocations, "read failure precedes permanent archive allocation");
}
static void LengthFailure(void) {
    int mode;
    for(mode=0;mode<3;mode++) {
        Setup(2); reportedLength=mode==0?-1:mode==1?INT_MAX:INT_MAX-3;
        if(!setjmp(drop)) { ScanAndLoadShaderFiles(); Check(0,"invalid archive length must reject"); }
        Check(caught==1, "one controlled archive length failure"); Balanced(1,1);
        Check(!s_shaderText && !allocations, "length failure precedes archive allocation/publication");
    }
}
static void AggregateFailure(void) {
    Setup(2); reported[0]=INT_MAX-4;contents[1]="x";
    if(!setjmp(drop)) {ScanAndLoadShaderFiles();Check(0,"aggregate archive size must reject");}
    Check(caught==1,"one controlled aggregate length failure");Balanced(2,2);
    Check(!s_shaderText && !allocations,"aggregate failure precedes permanent archive allocation");
}
static void BadPath(void) {
    int mode;
    for(mode=0;mode<2;mode++) {
        Setup(3); memset(longName,'x',sizeof(longName)-1);longName[sizeof(longName)-1]=0;
        files[1]=mode?NULL:longName;
        if(!setjmp(drop)) { ScanAndLoadShaderFiles();Check(0,"invalid archive path must reject"); }
        Check(caught==1,"one controlled archive path failure");Balanced(1,1);
        Check(!s_shaderText && !allocations,"path failure precedes archive allocation/publication");
    }
}
static void ValidArchives(void) {
    int bucket, before; char *p; char golden[2000]; shader_t *first,*second,*duplicate;
    Setup(2);
    contents[0]="// first file\ntests/duplicate\n{\n{\nmap $whiteimage\nrgbGen vertex\n}\n}\ntests/first\n{\n{\nmap $whiteimage\n}\n}\n";
    contents[1]="/* second file */\ntests/duplicate\n{\n{\nmap $whiteimage\nrgbGen identity\n}\n}\ntests/second\n{\n{\nmap $whiteimage\n}\n}\n";
    golden[0]=0;
    for(bucket=1;bucket>=0;bucket--) {
        strcat(golden,"\n");p=golden+strlen(golden);strcat(golden,contents[bucket]);COM_Compress(p);
    }
    ScanAndLoadShaderFiles(); Balanced(2,2);
    Check(!strcmp(s_shaderText,golden),"complete archive text matches stock concatenation/compression");
    Check(s_shaderText && s_shaderText[0]=='\n',"native reversed archive concatenation");
    p=s_shaderText;Check(!strcmp(COM_ParseExt(&p,qtrue),"tests/duplicate"),"last listed file remains first in text fallback");
    first=R_FindShader("tests/first",LIGHTMAP_NONE,qtrue);second=R_FindShader("tests/second",LIGHTMAP_NONE,qtrue);
    duplicate=R_FindShader("tests/duplicate",LIGHTMAP_NONE,qtrue);
    Check(!first->defaultShader && !second->defaultShader && !duplicate->defaultShader && duplicate->stages[0]->rgbGen==CGEN_VERTEX,"native valid labels and first-label duplicate priority");
    before=allocations;Check(R_FindShader("tests/duplicate",LIGHTMAP_NONE,qtrue)==duplicate && allocations==before,"indexed duplicate cache reuse");
    /* Actual hunk teardown leaves the private archive pointers dangling. */
    Release(); listCount=0;listReleased=0;fileCount=0;fileReleased=0;
    tr.whiteImage=&white;tr.defaultImage=&white;
    R_InitShaders();Balanced(0,0);
    Check(s_shaderText==NULL,"restart clears obsolete archive text");
    for(bucket=0;bucket<MAX_SHADERTEXT_HASH;bucket++)Check(!shaderTextHashTable[bucket],"restart clears obsolete archive indexes");
    Check(!R_FindShader("tests/first",LIGHTMAP_NONE,qtrue)->explicitlyDefined,"restart never reuses freed archive definitions");
}
static void EmptyArchives(void) {
    int count;
    for(count=1;count<=4;count++) { Setup(count); ScanAndLoadShaderFiles(); Balanced(count,count); Check(!FindShaderInShaderText("missing"),"empty archives have safe empty hash buckets"); }
    Setup(1);memset(longName,'x',55);longName[55]=0;files[0]=longName;
    ScanAndLoadShaderFiles();Balanced(1,1);Check(!FindShaderInShaderText("missing"),"maximum native archive path remains accepted");
    Setup(-1);ScanAndLoadShaderFiles();Balanced(0,0);Check(!s_shaderText,"negative list count rejected before file iteration");
    Setup(MAX_SHADER_FILES+2);ScanAndLoadShaderFiles();Balanced(MAX_SHADER_FILES,MAX_SHADER_FILES);Check(!FindShaderInShaderText("missing"),"legacy archive cap remains safe");
}
int main(int argc,char **argv) {
    int proof=argc>1?atoi(argv[1]):-1;
    if(proof==0)NoArchives();
    else if(proof==1)EmptyList();
    else if(proof==2)ReadFailure();
    else if(proof==3)LengthFailure();
    else if(proof==4)ValidArchives();
    else {NoArchives();EmptyList();ReadFailure();LengthFailure();AggregateFailure();BadPath();ValidArchives();EmptyArchives();}
    Release();puts("Native shader archive ownership, checked lengths, no-file startup and restart checks passed (issue #46)");return 0;
}
