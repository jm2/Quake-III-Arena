/* Actual remaining native handle operations and ordinary FILE/ZIP behavior. */
#include <setjmp.h>
#include <errno.h>
#include <fcntl.h>
#define Q3_ZIP_ERROR_HOOK
#define main FixtureZipMain
#ifndef Q3_ZIP_NATIVE_FIXTURE
#define Q3_ZIP_NATIVE_FIXTURE "fs_zip_regression.c"
#endif
#include Q3_ZIP_NATIVE_FIXTURE
#undef main

void Sys_StreamSeek(fileHandle_t f, int offset, int origin) {
    (void)f; (void)offset; (void)origin;
    Check(0, "unexpected platform streaming seek");
}

static jmp_buf dropTarget;
static int expectDrop;
static void FixtureErrorHook(int level) {
    Check(expectDrop && level == ERR_DROP, "unexpected native handle error");
    longjmp(dropTarget, 1);
}

static void MissingFILE(fileHandle_t f) {
    expectDrop = 1;
    if (!setjmp(dropTarget)) {
        (void)FS_FileForHandle(f);
        Check(0, "invalid FILE import must raise native drop");
    }
    expectDrop = 0;
}

static fileHandle_t Open(char *path, int buffered) {
    fileHandle_t f;
    char bytes[4];
    Begin();
    search.pack = FS_LoadZipFile(path, "native.pk3");
    Check(search.pack && FS_FOpenFileRead("native.txt", &f, buffered) == 12 && f > 0 &&
          FS_Read(bytes, 3, f) == 3 && !memcmp(bytes, "nat", 3),
          "actual prior partial buffered/shared owner");
    return f;
}

static void Invalid(char *path, int kind) {
    fileHandle_t f = Open(path, kind != 6);
    fileHandleData_t saved;
    int live = zoneLive;
    char bytes[16], payload[12];
    memcpy(&saved, &fsh[f], sizeof(saved));
    if (fsh[f].buffer) memcpy(payload, fsh[f].buffer, sizeof(payload));
    if (kind == 0) MissingFILE(MAX_FILE_HANDLES);
    else if (kind == 1) FS_FCloseFile(-1);
    else if (kind == 2) FS_FCloseFile(MAX_FILE_HANDLES);
    else if (kind == 3) Check(FS_FTell(-1) == -1, "negative tell handle rejects");
    else if (kind == 4) Check(FS_FTell(MAX_FILE_HANDLES) == -1, "exclusive tell bound rejects");
    else if (kind == 5) Check(FS_filelength(MAX_FILE_HANDLES) == -1, "exclusive length bound rejects");
    else if (kind == 6) FS_Flush(f);
    else FS_ForceFlush(f);
    Check(!memcmp(&saved, &fsh[f], sizeof(saved)) && zoneLive == live &&
          (!fsh[f].buffer || !memcmp(payload, fsh[f].buffer, sizeof(payload))),
          "invalid handle/flush preserves complete prior native owner and payload");
    Check(FS_Read(bytes, sizeof(bytes), f) == 9 && !memcmp(bytes, "ive data\n", 9),
          "prior native buffered/shared payload and cursor remain usable");
    FS_FCloseFile(f);
    End();
}

static void NativeHandleGolden(char *path) {
    fileHandle_t f;
    char bytes[16];
    int kind, ordinaryFD;
    for (kind = 0; kind <= 1; kind++) {
        f = Open(path, kind);
        Check(FS_FTell(f) == 3, "native buffered/shared tell after partial read");
        if (kind) Check(FS_filelength(f) == 12 && FS_FTell(f) == 3,
                        "native buffered length retains partial cursor");
        Check(FS_Read(bytes, sizeof(bytes), f) == 9 && !memcmp(bytes, "ive data\n", 9),
              "native buffered/shared close golden payload");
        FS_FCloseFile(f);
        Check(!memcmp(&fsh[f], &(fileHandleData_t){0}, sizeof(fsh[f])),
              "native complete handle record clears after close");
        End();
    }
    Begin();
    f = MAX_FILE_HANDLES - 1;
    fsh[f].handleFiles.file.o = tmpfile();
    Check(fsh[f].handleFiles.file.o != NULL, "actual ordinary native FILE owner");
    ordinaryFD = fileno(fsh[f].handleFiles.file.o);
    FS_ForceFlush(f);
    fsh[f].handleSync = qtrue;
    Check(FS_Write("native data\n", 12, f) == 12 && FS_FTell(f) == 12,
          "native ordinary write/sync and final handle tell");
    FS_Flush(f);
    Check(FS_Seek(f, 3, FS_SEEK_SET) == 0 && FS_filelength(f) == 12 && FS_FTell(f) == 3,
          "native ordinary length query retains FILE position");
    Check(FS_Read(bytes, sizeof(bytes), f) == 9 && !memcmp(bytes, "ive data\n", 9),
          "native ordinary final-slot FILE read payload");
    FS_FCloseFile(f);
    Check(fcntl(ordinaryFD, F_GETFD) == -1 && errno == EBADF,
          "native ordinary final-slot OS descriptor closes physically");
    Check(!memcmp(&fsh[f], &(fileHandleData_t){0}, sizeof(fsh[f])),
          "native ordinary final-slot owner releases and clears");
    End();
}

static void Inputs(char *path) {
    fileHandle_t f = Open(path, 1);
    fileHandleData_t saved;
    int handles[] = {-1, 0, 2, MAX_FILE_HANDLES, INT_MAX};
    int i;
    memcpy(&saved, &fsh[f], sizeof(saved));
    for (i = 0; i < sizeof(handles) / sizeof(handles[0]); i++) {
        fileHandle_t missing = handles[i];
        Check(FS_FTell(missing) == -1 && FS_filelength(missing) == -1 &&
              !FS_Write("x", 1, missing), "missing native tell/length/write rejects");
        FS_FCloseFile(missing);
        FS_Flush(missing);
        FS_ForceFlush(missing);
        MissingFILE(missing);
    }
    Check(!FS_Write(NULL, 1, f) && !FS_Write("x", INT_MIN, f) &&
          !FS_Write("x", 0, f) && !FS_Write("x", 1, f),
          "buffered writes and invalid native requests reject");
    FS_Flush(f);
    FS_ForceFlush(f);
    Check(!memcmp(&saved, &fsh[f], sizeof(saved)), "invalid operations retain buffered owner/cursor");
    FS_FCloseFile(f);
    FS_FCloseFile(f);
    End();
    f = Open(path, 0);
    Check(!FS_Write("x", 1, f), "native ZIP stream is not a writable FILE");
    FS_Flush(f);
    FS_ForceFlush(f);
    FS_FCloseFile(f);
    End();
    Begin();
    f = MAX_FILE_HANDLES - 1;
    fsh[f].handleFiles.file.o = tmpfile();
    Check(fsh[f].handleFiles.file.o != NULL, "ordinary invalid-write FILE owner");
    Check(!FS_Write(NULL, 1, f) && !FS_Write("x", -1, f) &&
          !FS_Write("x", INT_MIN, f) && !FS_Write(NULL, 0, f) &&
          FS_FTell(f) == 0 && FS_filelength(f) == 0,
          "invalid ordinary FILE writes consume no output");
    FS_FCloseFile(f);
    End();
}

int main(int argc, char **argv) {
    int i;
    Check(argc >= 2, "real native compressed ZIP input");
    if (argc > 2) {
        i = atoi(argv[2]);
        if (i < 8) Invalid(argv[1], i);
        else NativeHandleGolden(argv[1]);
        return 0;
    }
    NativeHandleGolden(argv[1]);
    for (i = 0; i < 8; i++) Invalid(argv[1], i);
    Inputs(argv[1]);
    puts("Actual native handle/FILE bounds, payloads and physical teardown pass");
    return 0;
}
