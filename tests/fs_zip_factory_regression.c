/* Actual archive/clone/stream factories with physical native failure owners. */
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

/* unz_s clone, entry reader, read buffer and the zlib inflate state. */
#define Q3_STREAM_IMPORTS 7


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

/* A unique open whose archive clone fails reads through the shared archive;
 * a failure inside the clone's decoder setup cleans up and reports -1. */
static void StreamFailure(char *path, int active, int position) {
    fileHandle_t prior, shared = 0, f;
    fileHandleData_t saved;
    unz_s savedArchive;
    file_in_zip_read_info_s savedDecoder;
    char bytes[16];
    int live, files, n;
    long cursor = 0;
    Begin();
    search.pack = FS_LoadZipFile(path, "native.pk3");
    Check(search.pack && FS_FOpenFileRead("native.txt", &prior, qtrue) == 12 &&
          FS_Read(bytes, 3, prior) == 3, "prior native unique stream payload/cursor");
    memcpy(&saved, &fsh[prior], sizeof(saved));
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
    n = FS_FOpenFileRead("native.txt", &f, qtrue);
    Check(nullableCalls >= position && !memcmp(&saved, &fsh[prior], sizeof(saved)) &&
          FileOwners() == files, "failed clone/decoder import retains the prior stream");
    Arm(0);
    if (position == 1) {
        Check(n == 12 && f > 0 && f != prior && f != shared &&
              fsh[f].handleFiles.file.z == search.pack->handle && !fsh[f].handleFiles.unique,
              "failed archive clone falls back to the shared archive");
        Check(FS_Read(bytes, sizeof(bytes), f) == 12 && !memcmp(bytes, "native data\n", 12),
              "shared fallback reads the complete payload");
        FS_FCloseFile(f);
        /* Closing a shared reader also ends any other shared reader's decoder. */
        Check((active ? zoneLive < live : zoneLive == live) && search.pack->handle != NULL,
              "shared fallback close keeps the archive");
    } else {
        Check(n == -1 && !f && zoneLive == live,
              "failed native unique decoder import clears candidate handle and owners");
        if (active) Check(!memcmp(&savedArchive, search.pack->handle, sizeof(savedArchive)) &&
                          !memcmp(&savedDecoder, savedArchive.pfile_in_zip_read, sizeof(savedDecoder)) &&
                          ftell(savedArchive.file) == cursor,
                          "clone failure retains active archive/decoder and physical cursor");
    }
    Check(FS_FOpenFileRead("native.txt", &f, qtrue) == 12 && f != prior && f != shared &&
          fsh[f].handleFiles.unique &&
          FS_Read(bytes, sizeof(bytes), f) == 12 && !memcmp(bytes, "native data\n", 12),
          "failed unique factory retries a complete native stream");
    FS_FCloseFile(f);
    if (active) {
        Check(FS_Read(bytes, sizeof(bytes), shared) == 9 && !memcmp(bytes, "ive data\n", 9),
              "active prior shared native payload survives failed unique factory");
        FS_FCloseFile(shared);
    }
    Check(FS_Read(bytes, sizeof(bytes), prior) == 9 && !memcmp(bytes, "ive data\n", 9),
          "prior unique stream survives failed unique factory");
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
        else if (i == 2) StreamFailure(argv[1], 0, 7);
        else if (i == 3) StreamFailure(argv[1], 1, 1);
        else if (i == 4) StreamFailure(argv[1], 1, 2);
        else if (i == 5) InvalidSource(argv[1]);
        else NativeFactoryGolden(argv[1]);
        return 0;
    }
    NativeFactoryGolden(argv[1]);
    ArchiveFailure(argv[1], 0, 1);
    ArchiveFailure(argv[1], 0, 2);
    ArchiveFailure(argv[1], 1, 1);
    for (i = 1; i <= Q3_STREAM_IMPORTS; i++) {
        StreamFailure(argv[1], 0, i);
        StreamFailure(argv[1], 1, i);
    }
    InvalidSource(argv[1]);
    puts("Actual native archive/clone/stream factories cleanly fail, preserve owners and retry");
    return 0;
}
