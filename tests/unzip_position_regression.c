/* Actual selected-entry metadata errors, filesystem rejection and retry. */
#define main FixtureZipMain
#ifndef Q3_ZIP_NATIVE_FIXTURE
#define Q3_ZIP_NATIVE_FIXTURE "fs_zip_regression.c"
#endif
#include Q3_ZIP_NATIVE_FIXTURE
#undef main
#include "fs_zip_fixture_io.h"
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

static int FileOwners(void) {
    DIR *dir = opendir("/proc/self/fd");
    struct dirent *entry;
    int count = 0;
    Check(dir != NULL, "native descriptor inventory");
    while ((entry = readdir(dir)) != NULL) if (entry->d_name[0] != '.') count++;
    Check(!closedir(dir), "descriptor inventory closes");
    return count;
}

static int ReadArchive(char *path, unsigned char *bytes, int capacity) {
    int fd = open(path, O_RDONLY);
    ssize_t size;
    Check(fd >= 0, "actual ZIP fixture copy descriptor");
    size = read(fd, bytes, capacity);
    Check(size > 0 && size < capacity && !close(fd), "bounded complete native ZIP image");
    return size;
}

static void Scalar(int kind) {
    static const unsigned char full[] = {0xff, 0xff, 0xff, 0xff};
    static const unsigned char sign[] = {0, 0, 0, 0x80};
    FILE *file = FixtureTemporaryFile();
    uLong value = 17;
    const unsigned char *bytes = kind == 7 ? sign : full;
    int size = kind == 4 ? 1 : kind == 5 ? 3 : kind == 6 ? 2 : 4;
    Check(file && fwrite(bytes, 1, size, file) == size && !fseek(file, 0, SEEK_SET),
          "actual native scalar ZIP input");
    if (kind == 4) Check(unzlocal_getShort(file, &value) != UNZ_OK && !value,
                         "truncated two-byte scalar rejects and initializes output");
    else if (kind == 5) Check(unzlocal_getLong(file, &value) != UNZ_OK && !value,
                              "truncated four-byte scalar rejects and initializes output");
    else if (kind == 6) Check(unzlocal_getShort(file, &value) == UNZ_OK && value == 65535,
                              "native unsigned two-byte maximum does not sign-extend");
    else Check(unzlocal_getLong(file, &value) == UNZ_OK &&
               value == (kind == 7 ? 0x80000000UL : 0xffffffffUL),
               "native unsigned four-byte boundary does not sign-extend");
    Check(!fclose(file), "native scalar owner closes");
}

static void Change(char *path, unzFile archive, unsigned long pos, int kind,
                   const unsigned char *original, int size, int restore) {
    int fd;
    unsigned char bad[4] = {0, 0, 0, 0};
    Check(!fflush(((unz_s *)archive)->file), "discard native input cache before actual mutation");
    fd = open(path, O_WRONLY);
    Check(fd >= 0, "actual ZIP mutation descriptor");
    if (restore) {
        Check(!ftruncate(fd, size) && pwrite(fd, original, size, 0) == size,
              "restore complete native ZIP bytes for retry");
    } else if (kind == 0) {
        Check(pwrite(fd, bad, sizeof(bad), pos) == sizeof(bad), "actual corrupt central signature");
    } else if (kind == 1 || kind == 2) {
        Check(!ftruncate(fd, pos + (kind == 1 ? 0 : 8)), "actual truncated central metadata");
    }
    Check(!close(fd), "mutation descriptor closes");
}

static void Invalid(char *path, int kind) {
    unsigned char bytes[512];
    unsigned long pos, target;
    int size, result;
    Begin();
    size = ReadArchive(path, bytes, sizeof(bytes));
    search.pack = FS_LoadZipFile(path, "native.pk3");
    Check(search.pack != NULL, "actual native central metadata mounts before mutation");
    pos = ((unz_s *)search.pack->handle)->pos_in_central_dir;
    target = kind == 3 ? 0 : pos;
    Change(path, search.pack->handle, pos, kind, bytes, size, 0);
    result = unzSetCurrentFileInfoPosition(search.pack->handle, target);
    if (result == UNZ_OK) fprintf(stderr, "Unexpected metadata success for actual mutation kind %d\n", kind);
    Check(result != UNZ_OK,
          "selected-entry setter propagates actual malformed/truncated metadata error");
    Check(!((unz_s *)search.pack->handle)->current_file_ok,
          "failed selected metadata remains marked invalid");
    Change(path, search.pack->handle, pos, kind, bytes, size, 1);
    Check(unzSetCurrentFileInfoPosition(search.pack->handle, pos) == UNZ_OK &&
          ((unz_s *)search.pack->handle)->current_file_ok,
          "actual restored selected metadata retries successfully");
    End();
}

static void PublicFailure(char *path, int kind, int active) {
    unsigned char bytes[512], payload[12];
    char text[16];
    fileHandle_t prior, shared = 0, f;
    fileHandleData_t saved;
    unz_s savedArchive;
    file_in_zip_read_info_s savedDecoder;
    unsigned long pos;
    long cursor = 0;
    int size, live, files;
    Begin();
    size = ReadArchive(path, bytes, sizeof(bytes));
    search.pack = FS_LoadZipFile(path, "native.pk3");
    Check(search.pack && FS_FOpenFileRead("native.txt", &prior, qtrue) == 12 &&
          FS_Read(text, 3, prior) == 3, "prior native buffered owner/cursor");
    memcpy(&saved, &fsh[prior], sizeof(saved));
    memcpy(payload, fsh[prior].buffer, sizeof(payload));
    if (active) {
        Check(FS_FOpenFileRead("native.txt", &shared, qfalse) == 12 && shared != prior &&
              FS_Read(text, 3, shared) == 3 && !memcmp(text, "nat", 3),
              "active native shared decoder before unique metadata failure");
    }
    pos = ((unz_s *)search.pack->handle)->pos_in_central_dir;
    Change(path, search.pack->handle, pos, kind, bytes, size, 0);
    if (active) {
        memcpy(&savedArchive, search.pack->handle, sizeof(savedArchive));
        memcpy(&savedDecoder, savedArchive.pfile_in_zip_read, sizeof(savedDecoder));
        cursor = ftell(savedArchive.file);
    }
    live = zoneLive;
    files = FileOwners();
    f = 17;
    Check(FS_FOpenFileRead("native.txt", &f, qtrue) == -1 && !f,
          "actual changed selected metadata rejects before entry allocation/open");
    Check(!memcmp(&saved, &fsh[prior], sizeof(saved)) &&
          !memcmp(payload, fsh[prior].buffer, sizeof(payload)) &&
          zoneLive == live && FileOwners() == files && !temporaryLive && !fs_loadStack,
          "metadata failure preserves prior buffer/cursor and releases private clone/handle");
    if (active) {
        Check(!memcmp(&savedArchive, search.pack->handle, sizeof(savedArchive)) &&
              !memcmp(&savedDecoder, savedArchive.pfile_in_zip_read, sizeof(savedDecoder)) &&
              ftell(savedArchive.file) == cursor,
              "unique metadata failure preserves entire active archive/decoder and physical cursor");
    }
    Change(path, search.pack->handle, pos, kind, bytes, size, 1);
    Check(FS_FOpenFileRead("native.txt", &f, qtrue) == 12 && f != prior && f != shared &&
          FS_Read(text, sizeof(text), f) == 12 && !memcmp(text, "native data\n", 12),
          "restored complete metadata retries through actual native buffered open/read");
    FS_FCloseFile(f);
    if (active) {
        Check(FS_Read(text, sizeof(text), shared) == 9 && !memcmp(text, "ive data\n", 9),
              "shared prior continues its native payload after failure and retry");
        FS_FCloseFile(shared);
    }
    FS_FCloseFile(prior);
    End();
}

static void NativePositionGolden(char *path) {
    fileHandle_t f;
    char bytes[16], name[256];
    unz_file_info info;
    unsigned long pos;
    int files = FileOwners();
    Begin();
    search.pack = FS_LoadZipFile(path, "native.pk3");
    Check(search.pack != NULL, "native ZIP golden mount");
    Check(unzGetCurrentFileInfoPosition(search.pack->handle, &pos) == UNZ_OK &&
          unzSetCurrentFileInfoPosition(search.pack->handle, pos) == UNZ_OK &&
          unzGetCurrentFileInfo(search.pack->handle, &info, name, sizeof(name), NULL, 0, NULL, 0) == UNZ_OK &&
          info.uncompressed_size == 12 && !strcmp(name, "native.txt"),
          "native valid position, filename and length remain unchanged");
    Check(FS_FOpenFileRead("native.txt", &f, qtrue) == 12 &&
          FS_Read(bytes, sizeof(bytes), f) == 12 && !memcmp(bytes, "native data\n", 12),
          "native selected position buffered payload golden");
    FS_FCloseFile(f);
    End();
    Check(FileOwners() == files, "native selected metadata releases all OS descriptors");
    {
        static const unsigned char scalar[] = {0x34, 0x12, 0x78, 0x56, 0x34, 0x12};
        FILE *file = FixtureTemporaryFile();
        uLong value;
        Check(file && fwrite(scalar, 1, sizeof(scalar), file) == sizeof(scalar) &&
              !fseek(file, 0, SEEK_SET), "actual ordinary native scalar golden");
        Check(unzlocal_getShort(file, &value) == UNZ_OK && value == 0x1234 &&
              unzlocal_getLong(file, &value) == UNZ_OK && value == 0x12345678 && !fclose(file),
              "native ordinary two/four-byte values and cursor remain unchanged");
    }
}

int main(int argc, char **argv) {
    int i, active;
    Check(argc >= 2, "actual mutable native ZIP input");
    if (argc > 2) {
        i = atoi(argv[2]);
        if (i < 4) Invalid(argv[1], i);
        else if (i < 9) Scalar(i);
        else NativePositionGolden(argv[1]);
        return 0;
    }
    NativePositionGolden(argv[1]);
    for (i = 0; i < 4; i++) Invalid(argv[1], i);
    for (i = 4; i < 9; i++) Scalar(i);
    for (active = 0; active <= 1; active++)
        for (i = 0; i < 3; i++) PublicFailure(argv[1], i, active);
    puts("Actual selected ZIP metadata errors, prior ownership and native retry pass");
    return 0;
}
