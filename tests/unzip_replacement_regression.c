/* Actual active decoder replacement retains every prior owner on failure. */
#define Q3_ZIP_FREE_HOOK
#define Q3_ZIP_NULLABLE_IMPORT
#define main FixtureZipMain
#ifndef Q3_ZIP_NATIVE_FIXTURE
#define Q3_ZIP_NATIVE_FIXTURE "fs_zip_regression.c"
#endif
#include Q3_ZIP_NATIVE_FIXTURE
#undef main
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

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


static int priorReleaseWatch, priorFreed, priorCount;
static void *priorOwners[Q3_ZIP_ZONE_CAPACITY];
static void FixtureFreeHook(void *owner) {
    int i;
    if (priorReleaseWatch)
        for (i = 0; i < priorCount; i++) if (priorOwners[i] == owner) priorFreed++;
}

static int Pattern(int offset) {
    unsigned x = ((unsigned)offset + 1U) * 2654435761U;
    x ^= x >> 16;
    x *= 2246822519U;
    x ^= x >> 13;
    return x & 255;
}
static void Payload(const unsigned char *bytes, int size, int offset) {
    int i;
    for (i = 0; i < size; i++) Check(bytes[i] == Pattern(offset + i), "actual native pattern payload");
}

static void Active(char *path, int position, int selection) {
    static unsigned char input[UNZ_BUFSIZE];
    unsigned char bytes[UNZ_BUFSIZE + 17];
    unz_s saved;
    file_in_zip_read_info_s savedDecoder;
    int live, files, n, i, imports, writer = -1;
    unsigned char header[4];
    long cursor;
    Begin();
    search.pack = FS_LoadZipFile(path, "native.pk3");
    Check(search.pack && unzGoToFirstFile(search.pack->handle) == UNZ_OK &&
          unzOpenCurrentFile(search.pack->handle) == UNZ_OK &&
          unzReadCurrentFile(search.pack->handle, bytes, 13) == 13,
          "actual active native stored/deflated decoder");
    Payload(bytes, 13, 0);
    if (selection) {
        unsigned long original, target;
        Check(unzGetCurrentFileInfoPosition(search.pack->handle, &original) == UNZ_OK &&
              unzGoToNextFile(search.pack->handle) == UNZ_OK,
              "actual selection changes while prior decoder is active");
        if (selection == 2) {
            Check(unzGetCurrentFileInfoPosition(search.pack->handle, &target) == UNZ_OK &&
                  unzSetCurrentFileInfoPosition(search.pack->handle, original) == UNZ_OK &&
                  unzSetCurrentFileInfoPosition(search.pack->handle, target) == UNZ_OK,
                  "actual central position setter selects replacement with prior decoder active");
        }
    }
    memcpy(&saved, search.pack->handle, sizeof(saved));
    memcpy(&savedDecoder, saved.pfile_in_zip_read, sizeof(savedDecoder));
    memcpy(input, savedDecoder.read_buffer, sizeof(input));
    cursor = ftell(saved.file);
    live = zoneLive;
    files = FileOwners();
    priorCount = priorFreed = 0;
    for (i = 0; i < Q3_ZIP_ZONE_CAPACITY; i++) if (zone[i]) priorOwners[priorCount++] = zone[i];
    if (!position) {
        Check(!fflush(saved.file), "discard native input buffering before local header mutation");
        writer = open(path, O_RDWR);
        Check(writer >= 0 && pread(writer, header, sizeof(header), saved.cur_file_info_internal.offset_curfile) == sizeof(header) &&
              pwrite(writer, "BAD!", sizeof(header), saved.cur_file_info_internal.offset_curfile) == sizeof(header),
              "actual local header corruption after prior decoder opens");
    }
    priorReleaseWatch = 1;
    Arm(position);
    Check(unzOpenCurrentFile(search.pack->handle) != UNZ_OK && !priorFreed &&
          ((unz_s *)search.pack->handle)->pfile_in_zip_read == saved.pfile_in_zip_read,
          "failed active replacement releases no prior owner and retains decoder root");
    priorReleaseWatch = 0;
    imports = nullableCalls;
    Arm(0);
    if (!position) {
        Check(pwrite(writer, header, sizeof(header), saved.cur_file_info_internal.offset_curfile) == sizeof(header) && !close(writer) &&
              !fflush(saved.file), "native local header restored for prior reader and retry");
    }
    Check(imports >= position && zoneLive == live && FileOwners() == files &&
          !memcmp(&saved, search.pack->handle, sizeof(saved)) &&
          !memcmp(&savedDecoder, saved.pfile_in_zip_read, sizeof(savedDecoder)) &&
          !memcmp(input, savedDecoder.read_buffer, sizeof(input)) && ftell(saved.file) == cursor,
          "failed active replacement preserves complete archive/decoder/input and physical cursor");
    n = 13;
    while (n < 3 * UNZ_BUFSIZE + 17) {
        int request = 3 * UNZ_BUFSIZE + 17 - n;
        int got;
        if (request > (int)sizeof(bytes)) request = sizeof(bytes);
        got = unzReadCurrentFile(search.pack->handle, bytes, request);
        Check(got == request, "prior native decoder continues across repeated input refills");
        Payload(bytes, got, n);
        n += got;
    }
    Check(!unzReadCurrentFile(search.pack->handle, bytes, 1) &&
          unzOpenCurrentFile(search.pack->handle) == UNZ_OK,
          "preserved prior decoder reaches EOF and selected replacement retries");
    if (selection) {
        Check(unzReadCurrentFile(search.pack->handle, bytes, sizeof(bytes)) == 12 &&
              !memcmp(bytes, "replacement\n", 12) &&
              !unzReadCurrentFile(search.pack->handle, bytes, 1),
              "successful retry reads actual newly selected native entry");
    } else {
        Check(unzReadCurrentFile(search.pack->handle, bytes, 13) == 13,
              "successful same-entry replacement reads original payload");
        Payload(bytes, 13, 0);
    }
    Check(unzCloseCurrentFile(search.pack->handle) == UNZ_OK, "replacement decoder physically closes");
    End();
}

static void NativeReplacementGolden(char *path) {
    unsigned char bytes[32];
    int files = FileOwners();
    Begin();
    search.pack = FS_LoadZipFile(path, "native.pk3");
    Check(search.pack && unzGoToFirstFile(search.pack->handle) == UNZ_OK &&
          unzOpenCurrentFile(search.pack->handle) == UNZ_OK &&
          unzReadCurrentFile(search.pack->handle, bytes, 13) == 13,
          "native valid prior decoder golden");
    Payload(bytes, 13, 0);
    Check(unzOpenCurrentFile(search.pack->handle) == UNZ_OK &&
          unztell(search.pack->handle) == 0 &&
          unzReadCurrentFile(search.pack->handle, bytes, sizeof(bytes)) == sizeof(bytes),
          "native valid replacement resets decoder payload and cursor");
    Payload(bytes, sizeof(bytes), 0);
    Check(unzCloseCurrentFile(search.pack->handle) == UNZ_OK, "native valid replacement closes");
    End();
    Check(FileOwners() == files, "native valid replacement returns descriptors to baseline");
}

int main(int argc, char **argv) {
    int i;
    Check(argc >= 2, "actual native replacement ZIP input");
    if (argc == 3) {
        i = atoi(argv[2]);
        if (i <= 6) Active(argv[1], i, 0);
        else if (i >= 10 && i <= 16) Active(argv[1], i - 10, 1);
        else if (i >= 20 && i <= 26) Active(argv[1], i - 20, 2);
        else NativeReplacementGolden(argv[1]);
        return 0;
    }
    Check(argc == 4 && !strcmp(argv[3], "all"), "complete stored/deflated replacement selection");
    NativeReplacementGolden(argv[1]);
    NativeReplacementGolden(argv[2]);
    for (i = 0; i <= 6; i++) {
        Active(argv[1], i, 0);
        Active(argv[1], i, 1);
        Active(argv[1], i, 2);
    }
    for (i = 0; i <= 2; i++) {
        Active(argv[2], i, 0);
        Active(argv[2], i, 1);
        Active(argv[2], i, 2);
    }
    puts("Actual active native decoder failures preserve owners, metadata, input and physical cursor");
    return 0;
}
