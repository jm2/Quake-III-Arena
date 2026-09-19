/* Actual shared ZIP seek semantics, chunking, selection errors and stream delegation. */
#define main FixtureZipMain
#include "fs_zip_regression.c"
#undef main

static int streamCalls, streamResult;
void Sys_StreamSeek(fileHandle_t f, int offset, int origin) {
    streamCalls++;
    Check(!fsh[f].streamed, "platform stream seek receives recursion-disabled handle");
    streamResult = FS_Seek(f, offset, origin);
}
static int Pattern(int offset) {
    unsigned x = ((unsigned)offset + 1U) * 2654435761U;
    x ^= x >> 16; x *= 2246822519U; x ^= x >> 13;
    return x & 255;
}
static void Payload(const unsigned char *bytes, int size, int offset) {
    int i; for (i = 0; i < size; i++) Check(bytes[i] == Pattern(offset+i), "actual sought native payload");
}
static fileHandle_t Open(char *path) {
    fileHandle_t f;
    Begin(); search.pack = FS_LoadZipFile(path, "native.pk3");
    Check(search.pack && FS_FOpenFileRead("native.bin", &f, qfalse) == 3*UNZ_BUFSIZE+17 &&
          f > 0 && fsh[f].zipFile && !fsh[f].buffer, "actual shared large native ZIP handle");
    return f;
}
static void Close(fileHandle_t f) { FS_FCloseFile(f); End(); }
static void NativeGolden(char *path) {
    unsigned char bytes[24]; fileHandle_t f=Open(path);
    Check(FS_Read(bytes,3,f)==3, "native shared initial read golden"); Payload(bytes,3,0);
    Check(FS_Seek(f,7,FS_SEEK_SET)==7 && FS_FTell(f)==7 && FS_Read(bytes,17,f)==17,
          "native legacy small absolute ZIP seek return/payload golden"); Payload(bytes,17,7);
    Close(f);
}
static void SeekCase(char *path, int kind) {
    unsigned char bytes[32]; int length=3*UNZ_BUFSIZE+17; fileHandle_t f=Open(path);
    if (kind == 0) {
        Check(FS_Seek(f,UNZ_BUFSIZE,FS_SEEK_SET)==UNZ_BUFSIZE && FS_FTell(f)==UNZ_BUFSIZE &&
              FS_Read(bytes,16,f)==16, "large absolute ZIP seek chunks without fatal error"); Payload(bytes,16,UNZ_BUFSIZE);
    } else if (kind == 1) {
        Check(FS_Read(bytes,3,f)==3 && FS_Seek(f,7,FS_SEEK_CUR)==7 && FS_FTell(f)==10 &&
              FS_Read(bytes,9,f)==9, "current-relative ZIP seek uses current cursor"); Payload(bytes,9,10);
    } else if (kind == 2) {
        Check(FS_Seek(f,-17,FS_SEEK_END)==-17 && FS_FTell(f)==length-17 && FS_Read(bytes,17,f)==17,
              "negative end-relative ZIP seek reaches native payload tail"); Payload(bytes,17,length-17);
    } else if (kind == 3) {
        Check(FS_Seek(f,100,FS_SEEK_END)==100 && FS_FTell(f)==length && !FS_Read(bytes,1,f),
              "positive end-relative ZIP seek clamps to native EOF");
    } else if (kind == 4) {
        Check(FS_Seek(f,3,999)==-1 && !FS_FTell(f), "unknown ZIP seek origin rejects without cursor change");
    } else {
        (void)FS_Seek(f,LONG_MAX,FS_SEEK_SET);
        Check(FS_FTell(f)==length,"full-width positive ZIP seek avoids arithmetic overflow");
        (void)FS_Seek(f,LONG_MIN,FS_SEEK_CUR);
        Check(FS_FTell(f)==0,"full-width negative ZIP seek avoids arithmetic overflow");
    }
    Close(f);
}
static void SetterFailure(char *path) {
    unsigned char bytes[32]; int saved; fileHandle_t f=Open(path);
    Check(FS_Read(bytes,13,f)==13, "actual decoder before selection failure"); Payload(bytes,13,0);
    saved=fsh[f].zipFilePos; fsh[f].zipFilePos=INT_MAX;
    Check(FS_Seek(f,0,FS_SEEK_SET)==-1 && FS_FTell(f)==13,
          "invalid selected-entry position reports failure and retains decoder");
    fsh[f].zipFilePos=saved;
    Check(FS_Read(bytes,sizeof(bytes),f)==sizeof(bytes), "selection failure retains prior payload cursor"); Payload(bytes,sizeof(bytes),13);
    Close(f);
}
static void Streamed(char *path) {
    unsigned char bytes[16]; fileHandle_t f=Open(path);
    Check(FS_Read(bytes,3,f)==3,"native cursor before platform stream seek");
    streamCalls=streamResult=0; fsh[f].streamed=qtrue;
    Check(!FS_Seek(f,7,FS_SEEK_CUR) && streamCalls==1 && streamResult==7 &&
          fsh[f].streamed && FS_FTell(f)==10, "platform stream seek delegates exactly once without double current offset");
    fsh[f].streamed=qfalse; Check(FS_Read(bytes,sizeof(bytes),f)==sizeof(bytes),"delegated stream cursor reads native payload"); Payload(bytes,sizeof(bytes),10);
    Close(f);
}
int main(int argc,char **argv) {
    int i; Check(argc>=2,"real large ZIP seek input");
    if(argc==3){i=atoi(argv[2]);if(i<=5)SeekCase(argv[1],i);else if(i==6)SetterFailure(argv[1]);else if(i==7)Streamed(argv[1]);else NativeGolden(argv[1]);return 0;}
    Check(argc==3,"one ZIP and suite selection"); return 0;
}
