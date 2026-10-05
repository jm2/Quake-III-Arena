/* Actual unique/shared ZIP read/seek arithmetic and native ordinary behavior. */
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

static fileHandle_t Open(char *path, int unique) {
    fileHandle_t f;
    Begin();
    search.pack = FS_LoadZipFile(path, "native.pk3");
    Check(search.pack && FS_FOpenFileRead("native.txt", &f, unique) == 12 && f > 0,
          "actual complete native compressed owner");
    Check(fsh[f].zipFile && fsh[f].handleFiles.file.z &&
          (fsh[f].handleFiles.file.z != search.pack->handle) == unique,
          "native unique/shared archive selection");
    fs_readCount = 0;
    return f;
}

static void Close(fileHandle_t f) { FS_FCloseFile(f); End(); }

static void Invalid(char *path, int kind) {
    fileHandle_t f = Open(path, kind != 1);
    char bytes[64];
    int count;
    if (kind == 0) {
        Check(FS_Read(bytes, 4, f) == 4, "native partial cursor before large request");
        fs_readCount = 0;
        Check(FS_Read(bytes, INT_MAX, f) == 8 && !memcmp(bytes, "ve data\n", 8) &&
              FS_FTell(f) == 12, "large request clips safely to native remaining bytes");
    } else if (kind == 1) {
        memset(bytes, 0x5a, sizeof(bytes));
        Check(FS_Read(bytes, INT_MIN, f) == 0 && fs_readCount == 0,
              "negative stream request consumes no data or signed accounting");
        for (count = 0; count < sizeof(bytes); count++) {
            Check(bytes[count] == 0x5a, "negative stream request preserves destination");
        }
        Check(FS_Read(bytes, sizeof(bytes), f) == 12 && !memcmp(bytes, "native data\n", 12),
              "negative stream request retains native retry position");
    } else if (kind == 2) {
        fs_readCount = INT_MAX - 2;
        Check(FS_Read(bytes, 3, f) == 3 && !memcmp(bytes, "nat", 3) && fs_readCount == INT_MAX,
              "requested-byte statistic saturates without signed overflow");
        Check(FS_Read(bytes, 2, f) == 2 && !memcmp(bytes, "iv", 2) && fs_readCount == INT_MAX,
              "saturated statistic preserves ordinary payload/cursor");
    } else {
        /* ZIP seeks return the requested offset, as retail's did. */
        Check(FS_Seek(f, 5, FS_SEEK_SET) == 5 && FS_FTell(f) == 5, "native initial seek position");
        if (kind == 3) {
            Check(FS_Seek(f, INT_MAX, FS_SEEK_CUR) == INT_MAX && FS_FTell(f) == 12,
                  "positive int-limit relative seek clamps to EOF");
        } else if (kind == 4) {
#if LONG_MAX > INT_MAX
            Check(FS_Seek(f, LONG_MAX, FS_SEEK_SET) == -1 && FS_FTell(f) == 5,
                  "full-width positive absolute seek rejects before moving");
#else
            Check(FS_Seek(f, LONG_MAX, FS_SEEK_SET) == LONG_MAX && FS_FTell(f) == 12,
                  "full-width positive absolute seek clamps to EOF");
#endif
        } else if (kind == 5) {
#if LONG_MAX > INT_MAX
            Check(FS_Seek(f, LONG_MAX, FS_SEEK_CUR) == -1 && FS_FTell(f) == 5,
                  "full-width positive relative seek rejects before moving");
#else
            Check(FS_Seek(f, LONG_MAX, FS_SEEK_CUR) == LONG_MAX && FS_FTell(f) == 12,
                  "full-width positive relative seek avoids overflow");
#endif
        } else if (kind == 6) {
#if LONG_MAX > INT_MAX
            Check(FS_Seek(f, LONG_MIN, FS_SEEK_CUR) == -1 && FS_FTell(f) == 5,
                  "full-width negative relative seek rejects before moving");
#else
            Check(FS_Seek(f, LONG_MIN, FS_SEEK_CUR) == LONG_MIN && FS_FTell(f) == 0,
                  "full-width negative relative seek clamps to start");
#endif
        } else {
#if LONG_MAX > INT_MAX
            Check(FS_Seek(f, LONG_MIN, FS_SEEK_END) == -1 && FS_FTell(f) == 5,
                  "full-width negative end seek rejects before moving");
#else
            Check(FS_Seek(f, LONG_MIN, FS_SEEK_END) == LONG_MIN && FS_FTell(f) == 0,
                  "full-width negative end seek clamps to start");
#endif
        }
        count = FS_FTell(f);
        Check((count == 12 ? !FS_Read(bytes, sizeof(bytes), f) :
               FS_Read(bytes, sizeof(bytes), f) == 12 - count &&
               !memcmp(bytes, "native data\n" + count, 12 - count)),
              "seek leaves the streamed cursor on the matching payload");
    }
    Close(f);
}

static void Inputs(char *path) {
    fileHandle_t f = Open(path, 1);
    fileHandleData_t saved;
    char bytes[32];
    int i;
    memcpy(&saved, &fsh[f], sizeof(saved));
    memset(bytes, 0x5a, sizeof(bytes));
    Check(!FS_Read(bytes, INT_MIN, f) && !FS_Read(bytes, -1, f) &&
          !FS_Read(NULL, 1, f) && !FS_Read(NULL, 0, f) &&
          !FS_Read(bytes, 1, -1) && !FS_Read(bytes, 1, MAX_FILE_HANDLES) &&
          !FS_Read(bytes, 1, 2) && !FS_Read(bytes, 1, 0),
          "missing output/nonpositive/free/out-of-range native reads reject");
    Check(FS_Seek(-1, 0, FS_SEEK_SET) == -1 &&
          FS_Seek(MAX_FILE_HANDLES, 0, FS_SEEK_SET) == -1 &&
          FS_Seek(2, 0, FS_SEEK_SET) == -1 && FS_Seek(0, 0, FS_SEEK_SET) == -1,
          "free/reserved/out-of-range seek handles reject before table access");
    Check(!fs_readCount && !memcmp(&saved, &fsh[f], sizeof(saved)),
          "invalid requests preserve complete native owner and statistics");
    for (i = 0; i < sizeof(bytes); i++) Check(bytes[i] == 0x5a, "invalid requests preserve destination");
    Check(FS_Read(bytes, sizeof(bytes), f) == 12 && !memcmp(bytes, "native data\n", 12),
          "invalid requests preserve the complete native payload");
    Close(f);
}

static void NativeReadGolden(char *path) {
    fileHandle_t f = Open(path, 1);
    char bytes[32];
    Check(FS_Read(bytes, 4, f) == 4 && !memcmp(bytes, "nati", 4), "native initial partial payload");
    Check(FS_Seek(f, 1, FS_SEEK_SET) == 1 && FS_Read(bytes, 3, f) == 3 &&
          !memcmp(bytes, "ati", 3), "native ordinary absolute seek/read");
    Check(FS_Seek(f, 2, FS_SEEK_CUR) == 2 && FS_Read(bytes, 1, f) == 1 && bytes[0] == ' ',
          "native ordinary relative seek/read");
    Check(FS_Seek(f, -2, FS_SEEK_END) == -2 && FS_Read(bytes, 2, f) == 2 &&
          !memcmp(bytes, "a\n", 2) && !FS_Read(bytes, 1, f), "native end-relative payload and EOF");
    Check(FS_Seek(f, 2, FS_SEEK_SET) == 2 && FS_Seek(f, 3, 999) == -1 &&
          FS_Read(bytes, 1, f) == 1 && bytes[0] == 't', "native unknown origin preserves cursor");
    Check(fs_readCount == 12, "ordinary requested-byte accounting remains native");
    Check(FS_Seek(f, 100, FS_SEEK_SET) == 100 && FS_FTell(f) == 12 &&
          FS_Seek(f, -100, FS_SEEK_CUR) == -100 && !FS_FTell(f) &&
          FS_Seek(f, 100, FS_SEEK_END) == 100 && FS_FTell(f) == 12,
          "native ordinary out-of-range seek clamping");
    Close(f);
}

int main(int argc, char **argv) {
    int i;
    Check(argc >= 2, "real compressed native ZIP input");
    if (argc > 2) {
        i = atoi(argv[2]);
        if (i < 8) Invalid(argv[1], i);
        else NativeReadGolden(argv[1]);
        return 0;
    }
    NativeReadGolden(argv[1]);
    for (i = 0; i < 8; i++) Invalid(argv[1], i);
    Inputs(argv[1]);
    puts("Actual native read/seek bounds, counts, payloads and teardown pass");
    return 0;
}
