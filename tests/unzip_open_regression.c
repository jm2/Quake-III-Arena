/* Actual decoder initialization rejects each failed native allocation import. */
#define Q3_ZIP_NULLABLE_IMPORT
#define main FixtureZipMain
#ifndef Q3_ZIP_NATIVE_FIXTURE
#define Q3_ZIP_NATIVE_FIXTURE "fs_zip_regression.c"
#endif
#include Q3_ZIP_NATIVE_FIXTURE
#undef main
#include <dirent.h>

static int nullablePosition, nullableCalls;
static qboolean FixtureRejectZoneImport(int size) {
    (void)size;
    if (!nullablePosition) return qfalse;
    nullableCalls++;
    return nullableCalls == nullablePosition;
}

static int FileOwners(void) {
    DIR *dir = opendir("/proc/self/fd");
    struct dirent *entry;
    int count = 0;
    Check(dir != NULL, "actual decoder OS descriptor inventory");
    while ((entry = readdir(dir)) != NULL) if (entry->d_name[0] != '.') count++;
    Check(!closedir(dir), "descriptor inventory closes");
    return count;
}

static void Arm(int position) { nullableCalls = 0; nullablePosition = position; }

static void Direct(char *path, int position) {
    char bytes[16];
    int live, files;
    Begin();
    search.pack = FS_LoadZipFile(path, "native.pk3");
    Check(search.pack != NULL, "native complete compressed metadata mount");
    live = zoneLive;
    files = FileOwners();
    Arm(position);
    Check(unzOpenCurrentFile(search.pack->handle) != UNZ_OK,
          "failed decoder initialization reports actual failure");
    Check(nullableCalls >= position && !((unz_s *)search.pack->handle)->pfile_in_zip_read &&
          zoneLive == live && FileOwners() == files,
          "failed decoder import publishes no root and frees every candidate owner");
    Arm(0);
    Check(unzOpenCurrentFile(search.pack->handle) == UNZ_OK &&
          unzReadCurrentFile(search.pack->handle, bytes, sizeof(bytes)) == 12 &&
          !memcmp(bytes, "native data\n", 12), "failed decoder initialization retries natively");
    Check(unzCloseCurrentFile(search.pack->handle) == UNZ_OK && zoneLive == live,
          "retried decoder closes every physical owner");
    End();
}

static void Public(char *path, int position) {
    fileHandle_t prior, f;
    fileHandleData_t saved;
    char bytes[16], payload[12];
    int live, files;
    Begin();
    search.pack = FS_LoadZipFile(path, "native.pk3");
    Check(search.pack && FS_FOpenFileRead("native.txt", &prior, qtrue) == 12 &&
          FS_Read(bytes, 3, prior) == 3, "prior native independent buffered cursor");
    memcpy(&saved, &fsh[prior], sizeof(saved));
    memcpy(payload, fsh[prior].buffer, sizeof(payload));
    live = zoneLive;
    files = FileOwners();
    f = 17;
    Arm(position);
    Check(FS_FOpenFileRead("native.txt", &f, qfalse) == -1 && !f,
          "public shared stream rejects failed decoder initialization");
    Check(nullableCalls >= position && !((unz_s *)search.pack->handle)->pfile_in_zip_read &&
          zoneLive == live && FileOwners() == files &&
          !memcmp(&saved, &fsh[prior], sizeof(saved)) &&
          !memcmp(payload, fsh[prior].buffer, sizeof(payload)),
          "decoder failure clears candidate and preserves complete prior native buffer owner");
    Arm(0);
    Check(FS_FOpenFileRead("native.txt", &f, qfalse) == 12 && f != prior &&
          FS_Read(bytes, sizeof(bytes), f) == 12 && !memcmp(bytes, "native data\n", 12),
          "public failed initialization retries through actual native stream");
    FS_FCloseFile(f);
    Check(FS_Read(bytes, sizeof(bytes), prior) == 9 && !memcmp(bytes, "ive data\n", 9),
          "prior partial buffer remains usable after failure and retry");
    FS_FCloseFile(prior);
    End();
}

static void NativeOpenGolden(char *path) {
    char bytes[16];
    int live, files = FileOwners();
    Begin();
    search.pack = FS_LoadZipFile(path, "native.pk3");
    Check(search.pack != NULL, "native stored/deflated decoder golden mount");
    live = zoneLive;
    Check(unzOpenCurrentFile(search.pack->handle) == UNZ_OK &&
          ((unz_s *)search.pack->handle)->pfile_in_zip_read &&
          ((unz_s *)search.pack->handle)->pfile_in_zip_read->stream_initialised ==
          (((unz_s *)search.pack->handle)->cur_file_info.compression_method == Z_DEFLATED),
          "native successful stored/deflated decoder state");
    Check(unzReadCurrentFile(search.pack->handle, bytes, 3) == 3 && !memcmp(bytes, "nat", 3) &&
          unzReadCurrentFile(search.pack->handle, bytes, sizeof(bytes)) == 9 &&
          !memcmp(bytes, "ive data\n", 9) && !unzReadCurrentFile(search.pack->handle, bytes, 1),
          "native successful partial payload and EOF");
    Check(unzCloseCurrentFile(search.pack->handle) == UNZ_OK && zoneLive == live,
          "native successful decoder physically closes");
    End();
    Check(FileOwners() == files, "native stored/deflated mount descriptor closes");
}

int main(int argc, char **argv) {
    int i;
    Check(argc >= 2, "real complete compressed native ZIP input");
    if (argc == 3) {
        i = atoi(argv[2]);
        if (i < 4) Direct(argv[1], i + 3);
        else if (i < 8) Public(argv[1], i - 1);
        else NativeOpenGolden(argv[1]);
        return 0;
    }
    Check(argc == 4 && !strcmp(argv[3], "all"), "native stored ZIP and full-suite selection");
    NativeOpenGolden(argv[1]);
    NativeOpenGolden(argv[2]);
    for (i = 1; i <= 6; i++) {
        Direct(argv[1], i);
        Public(argv[1], i);
    }
    puts("Actual decoder NULL imports, private cleanup and native stored/deflated retry pass");
    return 0;
}
