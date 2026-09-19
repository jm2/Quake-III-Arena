/* Actual archive/clone/buffer factories with physical native failure owners. */
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


static void ArchiveFailure(char *path, int clone, int position) {
    unzFile archive = NULL, candidate;
    unz_s saved;
    int live, files = FileOwners();
    Begin();
    if (clone) {
        archive = unzOpen(path);
        Check(archive != NULL, "actual prior native archive for clone");
        memcpy(&saved, archive, sizeof(saved));
    }
    live = zoneLive;
    files = FileOwners();
    Arm(position);
    candidate = clone ? unzReOpen(path, archive) : unzOpen(path);
    Check(!candidate && nullableCalls >= position && zoneLive == live && FileOwners() == files,
          "archive/clone failure publishes no owner and closes candidate OS descriptor");
    Arm(0);
    if (clone) Check(!memcmp(&saved, archive, sizeof(saved)), "failed clone preserves complete borrowed archive");
    candidate = clone ? unzReOpen(path, archive) : unzOpen(path);
    Check(candidate && unzGoToFirstFile(candidate) == UNZ_OK &&
          ((unz_s *)candidate)->cur_file_info.uncompressed_size == 12 &&
          unzClose(candidate) == UNZ_OK && zoneLive == live,
          "failed archive/clone factory retries native metadata and physical close");
    if (clone) Check(unzClose(archive) == UNZ_OK, "prior borrowed archive closes once");
    End();
}

static void BufferFailure(char *path, int active, int position) {
    fileHandle_t prior, shared = 0, f;
    fileHandleData_t saved;
    unz_s savedArchive;
    file_in_zip_read_info_s savedDecoder;
    char bytes[16], payload[12];
    int live, files;
    long cursor = 0;
    Begin();
    search.pack = FS_LoadZipFile(path, "native.pk3");
    Check(search.pack && FS_FOpenFileRead("native.txt", &prior, qtrue) == 12 &&
          FS_Read(bytes, 3, prior) == 3, "prior native buffered payload/cursor");
    memcpy(&saved, &fsh[prior], sizeof(saved));
    memcpy(payload, fsh[prior].buffer, sizeof(payload));
    if (active) {
        Check(FS_FOpenFileRead("native.txt", &shared, qfalse) == 12 &&
              FS_Read(bytes, 3, shared) == 3, "active prior native shared reader");
        memcpy(&savedArchive, search.pack->handle, sizeof(savedArchive));
        memcpy(&savedDecoder, savedArchive.pfile_in_zip_read, sizeof(savedDecoder));
        cursor = ftell(savedArchive.file);
    }
    live = zoneLive;
    files = FileOwners();
    f = 17;
    Arm(position);
    Check(FS_FOpenFileRead("native.txt", &f, qtrue) == -1 && !f,
          "failed native unique buffer/clone import clears candidate handle");
    Check(nullableCalls >= position && zoneLive == live && FileOwners() == files &&
          !memcmp(&saved, &fsh[prior], sizeof(saved)) &&
          !memcmp(payload, fsh[prior].buffer, sizeof(payload)),
          "buffer/clone failure frees candidate owners and retains complete prior buffer");
    Arm(0);
    if (active) Check(!memcmp(&savedArchive, search.pack->handle, sizeof(savedArchive)) &&
                      !memcmp(&savedDecoder, savedArchive.pfile_in_zip_read, sizeof(savedDecoder)) &&
                      ftell(savedArchive.file) == cursor,
                      "buffer/clone failure retains active archive/decoder and physical cursor");
    Check(FS_FOpenFileRead("native.txt", &f, qtrue) == 12 && f != prior && f != shared &&
          FS_Read(bytes, sizeof(bytes), f) == 12 && !memcmp(bytes, "native data\n", 12),
          "failed unique factory retries native complete buffering");
    FS_FCloseFile(f);
    if (active) {
        Check(FS_Read(bytes, sizeof(bytes), shared) == 9 && !memcmp(bytes, "ive data\n", 9),
              "active prior shared native payload survives failed unique factory");
        FS_FCloseFile(shared);
    }
    FS_FCloseFile(prior);
    End();
}

static void InvalidSource(char *path) {
    int live, files;
    Begin();
    live = zoneLive;
    files = FileOwners();
    Check(!unzOpen(NULL) && !unzReOpen(NULL, NULL) && !unzReOpen(path, NULL) &&
          zoneLive == live && FileOwners() == files,
          "missing native archive/clone path/source rejects before stdio/allocation");
    End();
}

static void NativeFactoryGolden(char *path) {
    unzFile archive, clone;
    char bytes[16];
    int files = FileOwners();
    Begin();
    archive = unzOpen(path);
    Check(archive && unzGoToFirstFile(archive) == UNZ_OK &&
          ((unz_s *)archive)->cur_file_info.uncompressed_size == 12,
          "native archive selected metadata");
    clone = unzReOpen(path, archive);
    Check(clone && clone != archive && ((unz_s *)clone)->file != ((unz_s *)archive)->file &&
          !((unz_s *)clone)->pfile_in_zip_read && unzOpenCurrentFile(clone) == UNZ_OK &&
          unzReadCurrentFile(clone, bytes, sizeof(bytes)) == 12 &&
          !memcmp(bytes, "native data\n", 12) && unzClose(clone) == UNZ_OK,
          "native clone owns independent FILE/decoder and complete payload");
    Check(unzOpenCurrentFile(archive) == UNZ_OK && unzReadCurrentFile(archive, bytes, sizeof(bytes)) == 12 &&
          !memcmp(bytes, "native data\n", 12) && unzClose(archive) == UNZ_OK,
          "native borrowed archive survives clone close and releases physically");
    End();
    Check(FileOwners() == files, "native factory OS descriptors return to baseline");
    Golden(path);
}

int main(int argc, char **argv) {
    int i;
    Check(argc >= 2, "actual complete native ZIP input");
    if (argc == 3) {
        i = atoi(argv[2]);
        if (i == 0) ArchiveFailure(argv[1], 0, 2);
        else if (i == 1) ArchiveFailure(argv[1], 1, 1);
        else if (i == 2) BufferFailure(argv[1], 0, 7);
        else if (i == 3) BufferFailure(argv[1], 1, 1);
        else if (i == 4) BufferFailure(argv[1], 1, 8);
        else if (i == 5) InvalidSource(argv[1]);
        else NativeFactoryGolden(argv[1]);
        return 0;
    }
    NativeFactoryGolden(argv[1]);
    ArchiveFailure(argv[1], 0, 1);
    ArchiveFailure(argv[1], 0, 2);
    ArchiveFailure(argv[1], 1, 1);
    BufferFailure(argv[1], 0, 7);
    BufferFailure(argv[1], 1, 1);
    BufferFailure(argv[1], 1, 8);
    InvalidSource(argv[1]);
    puts("Actual native archive/clone/buffer factories cleanly fail, preserve owners and retry");
    return 0;
}
