/* Real shared and unique native streams must own separate decoder storage. */
#define Q3_ZIP_FREE_HOOK
#define main FixtureZipMain
#ifndef Q3_ZIP_NATIVE_FIXTURE
#define Q3_ZIP_NATIVE_FIXTURE "fs_zip_regression.c"
#endif
#include Q3_ZIP_NATIVE_FIXTURE
#undef main
#include <stdint.h>

#define NATIVE_LARGE_SIZE (32 * 1024 * 1024)
static uintptr_t retained[Q3_ZIP_ZONE_CAPACITY];
static int retainedCount, watchFrees, priorFrees;

static void FixtureFreeHook(void *owner) {
    int i;
    if (!watchFrees) return;
    for (i = 0; i < retainedCount; i++) {
        if (retained[i] == (uintptr_t)owner) priorFrees++;
    }
}

static void ZeroRead(fileHandle_t f, int length) {
    unsigned char bytes[64];
    int i;
    Check(length <= sizeof(bytes), "bounded native read fixture");
    memset(bytes, 0xff, sizeof(bytes));
    Check(FS_Read(bytes, length, f) == length, "actual complete native stream read");
    for (i = 0; i < length; i++) Check(bytes[i] == 0, "native compressed payload");
}

static void Reopen(char *path, int uniqueFirst) {
    fileHandle_t shared, unique;
    file_in_zip_read_info_s saved;
    file_in_zip_read_info_s *readOwner;
    int i;
    Begin();
    search.pack = FS_LoadZipFile(path, "large.pk3");
    Check(search.pack != NULL, "actual large compressed native mount");
    Check(FS_FOpenFileRead("large.bin", &shared, qfalse) == NATIVE_LARGE_SIZE &&
          shared > 0 && !fsh[shared].buffer, "native shared stream at buffer cap");
    ZeroRead(shared, 13);
    readOwner = ((unz_s *)fsh[shared].handleFiles.file.z)->pfile_in_zip_read;
    Check(readOwner && readOwner->stream.total_out == 13, "active native shared decoder");
    memcpy(&saved, readOwner, sizeof(saved));
    retainedCount = priorFrees = 0;
    for (i = 0; i < Q3_ZIP_ZONE_CAPACITY; i++) {
        if (zone[i]) retained[retainedCount++] = (uintptr_t)zone[i];
    }
    watchFrees = 1;
    Check(FS_FOpenFileRead("large.bin", &unique, qtrue) == NATIVE_LARGE_SIZE &&
          unique > 0 && unique != shared && !fsh[unique].buffer,
          "native unique stream clones only ZIP metadata");
    watchFrees = 0;
    Check(!priorFrees, "reopen never frees an active shared decoder owner");
    Check(((unz_s *)fsh[shared].handleFiles.file.z)->pfile_in_zip_read == readOwner &&
          ((unz_s *)fsh[unique].handleFiles.file.z)->pfile_in_zip_read != readOwner &&
          !memcmp(&saved, readOwner, sizeof(saved)),
          "complete shared decoder and cursor survive unique open");
    ZeroRead(unique, 19);
    ZeroRead(shared, 23);
    Check(readOwner->stream.total_out == 36 &&
          ((unz_s *)fsh[unique].handleFiles.file.z)->pfile_in_zip_read->stream.total_out == 19,
          "shared and unique cursors advance independently");
    if (uniqueFirst) {
        FS_FCloseFile(unique);
        ZeroRead(shared, 11);
        Check(readOwner->stream.total_out == 47, "shared stream survives unique close");
        FS_FCloseFile(shared);
    } else {
        FS_FCloseFile(shared);
        ZeroRead(unique, 11);
        Check(((unz_s *)fsh[unique].handleFiles.file.z)->pfile_in_zip_read->stream.total_out == 30,
              "unique stream survives shared close");
        FS_FCloseFile(unique);
    }
    End();
}

static void DirectReopen(char *path) {
    fileHandle_t shared;
    unz_s *clone;
    file_in_zip_read_info_s saved;
    file_in_zip_read_info_s *readOwner;
    Begin();
    search.pack = FS_LoadZipFile(path, "large.pk3");
    Check(search.pack && FS_FOpenFileRead("large.bin", &shared, qfalse) == NATIVE_LARGE_SIZE,
          "native shared stream before direct decoder clone");
    ZeroRead(shared, 13);
    readOwner = ((unz_s *)search.pack->handle)->pfile_in_zip_read;
    memcpy(&saved, readOwner, sizeof(saved));
    clone = (unz_s *)unzReOpen(path, search.pack->handle);
    Check(clone && !clone->pfile_in_zip_read && clone->file != ((unz_s *)search.pack->handle)->file,
          "direct reopen copies metadata without borrowing a decoder owner");
    Check(clone->cur_file_info.uncompressed_size == NATIVE_LARGE_SIZE,
          "direct reopen retains native entry metadata");
    unzClose((unzFile)clone);
    Check(((unz_s *)search.pack->handle)->pfile_in_zip_read == readOwner &&
          !memcmp(&saved, readOwner, sizeof(saved)),
          "direct clone close keeps the complete shared decoder");
    ZeroRead(shared, 11);
    Check(readOwner->stream.total_out == 24, "direct clone leaves native cursor usable");
    FS_FCloseFile(shared);
    End();
}

static void NativeGolden(char *path) {
    fileHandle_t first, second;
    Begin();
    search.pack = FS_LoadZipFile(path, "large.pk3");
    Check(search.pack != NULL, "native cold large-file mount");
    Check(FS_FOpenFileRead("large.bin", &first, qtrue) == NATIVE_LARGE_SIZE &&
          first > 0 && !fsh[first].buffer, "native cold unique stream selection");
    ZeroRead(first, 17);
    Check(FS_FOpenFileRead("large.bin", &second, qtrue) == NATIVE_LARGE_SIZE &&
          second != first && !fsh[second].buffer, "native independent unique streams");
    ZeroRead(second, 7);
    FS_FCloseFile(first);
    ZeroRead(second, 8);
    Check(((unz_s *)fsh[second].handleFiles.file.z)->pfile_in_zip_read->stream.total_out == 15,
          "native remaining unique payload and cursor");
    FS_FCloseFile(second);
    End();
}

static void PatternRead(fileHandle_t f, int offset, int length) {
    unsigned char bytes[4096];
    while (length) {
        int i, count = length < sizeof(bytes) ? length : sizeof(bytes);
        Check(FS_Read(bytes, count, f) == count, "complete stored stream refills");
        for (i = 0; i < count; i++) {
            Check(bytes[i] == (unsigned char)(offset + i), "stored payload survives each physical refill");
        }
        length -= count;
        offset += count;
    }
}

static void StoredRefill(char *path, int small) {
    fileHandle_t shared, unique;
    unz_s savedArchive;
    file_in_zip_read_info_s savedRead;
    unz_s *parent;
    long cursor;
    int i;
    char payload[16];
    Begin();
    search.pack = FS_LoadZipFile(path, "large.pk3");
    Check(search.pack && FS_FOpenFileRead("stored.bin", &shared, qfalse) == NATIVE_LARGE_SIZE,
          "real complete stored ZIP reader");
    PatternRead(shared, 0, 13);
    parent = (unz_s *)search.pack->handle;
    cursor = ftell(parent->file);
    Check(cursor >= 0, "actual shared physical file position");
    memcpy(&savedArchive, parent, sizeof(savedArchive));
    memcpy(&savedRead, parent->pfile_in_zip_read, sizeof(savedRead));
    retainedCount = priorFrees = 0;
    for (i = 0; i < Q3_ZIP_ZONE_CAPACITY; i++) {
        if (zone[i]) retained[retainedCount++] = (uintptr_t)zone[i];
    }
    watchFrees = 1;
    Check(FS_FOpenFileRead(small ? "native.txt" : "large.bin", &unique, qtrue) ==
          (small ? 12 : NATIVE_LARGE_SIZE) && unique > 0 && unique != shared,
          "unique target opens beside an active stored reader");
    watchFrees = 0;
    Check(!priorFrees, "active shared decoder survives a buffered or streamed unique target");
    Check(ftell(parent->file) == cursor, "active shared FILE cursor remains intact");
    Check(!memcmp(&savedArchive, parent, sizeof(savedArchive)) &&
          !memcmp(&savedRead, parent->pfile_in_zip_read, sizeof(savedRead)),
          "complete active shared metadata and decoder remain intact");
    if (small) {
        Check(fsh[unique].buffer && FS_Read(payload, sizeof(payload), unique) == 12 &&
              !memcmp(payload, "native data\n", 12), "native small-target buffering remains");
    } else ZeroRead(unique, 19);
    FS_FCloseFile(unique);
    PatternRead(shared, 13, 2 * UNZ_BUFSIZE + 17);
    Check(parent->pfile_in_zip_read->stream.total_out == 13 + 2 * UNZ_BUFSIZE + 17,
          "active stored cursor crosses multiple native input refills");
    FS_FCloseFile(shared);
    End();
}

int main(int argc, char **argv) {
    Check(argc >= 2, "real complete 32 MiB compressed ZIP input");
    if (argc > 2) {
        int kind = atoi(argv[2]);
        if (kind < 2) Reopen(argv[1], kind);
        else if (kind == 2) DirectReopen(argv[1]);
        else if (kind < 5) StoredRefill(argv[1], kind == 4);
        else NativeGolden(argv[1]);
        return 0;
    }
    NativeGolden(argv[1]);
    Reopen(argv[1], 0);
    Reopen(argv[1], 1);
    DirectReopen(argv[1]);
    StoredRefill(argv[1], 0);
    StoredRefill(argv[1], 1);
    puts("Actual shared/unique ZIP streams retain independent decoder owners");
    return 0;
}
