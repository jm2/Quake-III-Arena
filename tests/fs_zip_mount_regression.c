/* Real malformed ZIP metadata and checked mount candidate ownership. */
#define Q3_ZIP_ALLOCATION_HOOK
#define main FixtureZipMain
#ifndef Q3_ZIP_NATIVE_FIXTURE
#define Q3_ZIP_NATIVE_FIXTURE "fs_zip_regression.c"
#endif
#include Q3_ZIP_NATIVE_FIXTURE
#undef main
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>

static unz_s *candidateStream;
static int faultPosition, candidateImports, faultArmed, faultTriggered;
static char mutablePath[1024];

static int FileOwners(void) {
    DIR *dir = opendir("/proc/self/fd");
    struct dirent *entry;
    int count = 0;
    Check(dir != NULL, "actual host file descriptor inventory");
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] != '.') count++;
    }
    Check(closedir(dir) == 0, "descriptor inventory releases its own stream");
    return count;
}

/* At real post-measurement allocation imports, truncate the actual open ZIP. */
static void FixtureZoneHook(void *owner, int size) {
    int fd;
    if (!faultArmed) return;
    if (!candidateStream) {
        if (size == sizeof(unz_s)) candidateStream = owner;
        return;
    }
    if (++candidateImports != faultPosition) return;
    Check(candidateStream->file != NULL, "actual candidate ZIP stream exists");
    Check(fflush(candidateStream->file) == 0, "discard native buffered input");
    fd = open(mutablePath, O_WRONLY);
    Check(fd >= 0, "open only the owned mutable fixture for truncation");
    Check(ftruncate(fd,
                    candidateStream->offset_central_dir +
                    candidateStream->byte_before_the_zipfile) == 0,
          "real candidate central directory truncates after measurement");
    Check(close(fd) == 0, "mutable fixture write descriptor releases");
    faultTriggered = 1;
    faultArmed = 0;
}

static void FreePack(pack_t *p) {
    unzClose(p->handle);
    Z_Free(p->buildBuffer);
    Z_Free(p);
}

static void Path(char *out, size_t capacity, const char *dir, const char *name) {
    Check(snprintf(out, capacity, "%s/%s", dir, name) < capacity,
          "bounded fixture path");
}

static void MountGolden(char *dir) {
    char path[1024], name[MAX_ZPATH], payload[32];
    pack_t *p, *empty;
    fileHandle_t f;
    int i, crcs[2], checksum, pure;
    Begin();
    Path(path, sizeof(path), dir, "maximum.pk3");
    memset(name, 'a', MAX_ZPATH - 1);
    name[MAX_ZPATH - 1] = 0;
    search.pack = p = FS_LoadZipFile(path, "maximum.pk3");
    Check(p && p->numfiles == 4 && p->hashSize == 8 && fs_packFiles == 4,
          "native complete mount count and hash capacity");
    Check(!strcmp(p->pakBasename, "maximum") && !strcmp(p->buildBuffer[0].name, name) &&
          !strcmp(p->buildBuffer[1].name, "native.txt") &&
          !strcmp(p->buildBuffer[2].name, "empty.bin") &&
          !strcmp(p->buildBuffer[3].name, "textures/"),
          "255-byte native filename, lowercase, entry order and basename");
    for (i = 0; i < 4; i++) {
        long hash = FS_HashFileName(p->buildBuffer[i].name, p->hashSize);
        fileInPack_t *entry;
        Check(unzSetCurrentFileInfoPosition(p->handle, p->buildBuffer[i].pos) == UNZ_OK,
              "native complete ZIP entry positions");
        for (entry = p->hashTable[hash]; entry; entry = entry->next) {
            if (entry == &p->buildBuffer[i]) break;
        }
        Check(entry == &p->buildBuffer[i], "every native entry is in its hash chain");
    }
    /* Independent Python zlib CRC32 of the exact native 12-byte payload. */
    crcs[0] = LittleLong((int)0xb070ee60u);
    crcs[1] = crcs[0];
    checksum = LittleLong(Com_BlockChecksum(crcs, sizeof(crcs)));
    pure = LittleLong(Com_BlockChecksumKey(crcs, sizeof(crcs), LittleLong(fs_checksumFeed)));
    Check(p->checksum == checksum && p->pure_checksum == pure,
          "native nonempty CRC order and pure checksum");
    Check(FS_FOpenFileRead(name, &f, qtrue) == 12 && f > 0 &&
          FS_Read(payload, sizeof(payload), f) == 12 &&
          !memcmp(payload, "native data\n", 12), "maximum native name payload");
    FS_FCloseFile(f);
    Check(FS_FOpenFileRead("native.txt", &f, qtrue) == 12 && f > 0 &&
          FS_Read(payload, sizeof(payload), f) == 12 &&
          !memcmp(payload, "native data\n", 12), "native stored entry payload");
    FS_FCloseFile(f);
    Path(path, sizeof(path), dir, "empty.pk3");
    empty = FS_LoadZipFile(path, "empty.pk3");
    Check(empty && !empty->numfiles && empty->hashSize == 1 && fs_packFiles == 4,
          "native supported prefixed empty archive remains valid");
    FreePack(empty);
    End();
}

static const char *invalidNames[] = {
    "long.pk3", "prefix-long.pk3", "nul.pk3", "first.pk3", "second.pk3",
    "count.pk3", "short-name.pk3", "native.pk3"
};

static void Invalid(char *dir, int kind) {
    char path[1024];
    pack_t *prior, *candidate;
    unsigned char *header, *buffer;
    fileHandleData_t saved;
    fileHandle_t f;
    char payload[12];
    size_t headerSize, bufferSize;
    int live, count, files;
    Begin();
    Path(path, sizeof(path), dir, "native.pk3");
    search.pack = prior = FS_LoadZipFile(path, "native.pk3");
    Check(prior != NULL && FS_FOpenFileRead("native.txt", &f, qtrue) == 12,
          "complete prior native mount and buffered owner");
    headerSize = sizeof(*prior) + prior->hashSize * sizeof(fileInPack_t *);
    bufferSize = sizeof(fileInPack_t) + strlen(prior->buildBuffer[0].name) + 1;
    header = malloc(headerSize);
    buffer = malloc(bufferSize);
    Check(header && buffer, "physical complete prior mount snapshots");
    memcpy(header, prior, headerSize);
    memcpy(buffer, prior->buildBuffer, bufferSize);
    memcpy(&saved, &fsh[f], sizeof(saved));
    memcpy(payload, fsh[f].buffer, sizeof(payload));
    if (kind == 7) fs_packFiles = INT_MAX;
    live = zoneLive;
    count = fs_packFiles;
    files = FileOwners();
    Path(path, sizeof(path), dir, invalidNames[kind]);
    candidate = FS_LoadZipFile(path, "invalid.pk3");
    Check(candidate == NULL, "invalid complete mount rejects");
    Check(zoneLive == live && FileOwners() == files && fs_packFiles == count && search.pack == prior &&
          !memcmp(header, prior, headerSize) && !memcmp(buffer, prior->buildBuffer, bufferSize) &&
          !memcmp(&saved, &fsh[f], sizeof(saved)) &&
          !memcmp(payload, fsh[f].buffer, sizeof(payload)),
          "failed metadata keeps complete prior mount, buffer, count and physical owners");
    free(header);
    free(buffer);
    fs_packFiles = 1;
    Path(path, sizeof(path), dir, "native.pk3");
    candidate = FS_LoadZipFile(path, "retry.pk3");
    Check(candidate && candidate->numfiles == 1 && fs_packFiles == 2,
          "invalid metadata retries to complete native candidate");
    FreePack(candidate);
    Check(FileOwners() == files, "retry mount stream physically releases");
    Check(FS_Read(path, sizeof(path), f) == 12 && !memcmp(path, "native data\n", 12),
          "prior buffered payload remains usable after failed mount and retry");
    FS_FCloseFile(f);
    End();
}

static void SecondPass(char *dir, int position) {
    char path[1024];
    pack_t *candidate, *prior;
    unsigned char *header, *buffer;
    size_t headerSize, bufferSize;
    fileHandleData_t saved;
    fileHandle_t f;
    char payload[12];
    int live, files;
    Begin();
    Path(path, sizeof(path), dir, "native.pk3");
    search.pack = prior = FS_LoadZipFile(path, "prior.pk3");
    Check(search.pack != NULL, "prior native root before second-pass failure");
    Check(FS_FOpenFileRead("native.txt", &f, qtrue) == 12,
          "prior real buffered owner before second-pass failure");
    headerSize = sizeof(*prior) + prior->hashSize * sizeof(fileInPack_t *);
    bufferSize = sizeof(fileInPack_t) + strlen(prior->buildBuffer[0].name) + 1;
    header = malloc(headerSize);
    buffer = malloc(bufferSize);
    Check(header && buffer, "physical second-pass prior snapshots");
    memcpy(header, prior, headerSize);
    memcpy(buffer, prior->buildBuffer, bufferSize);
    memcpy(&saved, &fsh[f], sizeof(saved));
    memcpy(payload, fsh[f].buffer, sizeof(payload));
    live = zoneLive;
    files = FileOwners();
    candidateStream = NULL;
    candidateImports = faultTriggered = 0;
    faultPosition = position;
    faultArmed = 1;
    Path(path, sizeof(path), dir, position == 1 ? "mutable1.pk3" :
         position == 2 ? "mutable2.pk3" : "mutable3.pk3");
    memcpy(mutablePath, path, strlen(path) + 1);
    candidate = FS_LoadZipFile(path, "mutable.pk3");
    faultArmed = 0;
    Check(faultTriggered && candidate == NULL && zoneLive == live && FileOwners() == files &&
          fs_packFiles == 1 && search.pack == prior &&
          !memcmp(header, prior, headerSize) && !memcmp(buffer, prior->buildBuffer, bufferSize) &&
          !memcmp(&saved, &fsh[f], sizeof(saved)) &&
          !memcmp(payload, fsh[f].buffer, sizeof(payload)),
          "second-pass real truncation releases every private candidate owner");
    free(header);
    free(buffer);
    candidateStream = NULL;
    Path(path, sizeof(path), dir, "native.pk3");
    candidate = FS_LoadZipFile(path, "retry.pk3");
    Check(candidate && candidate->numfiles == 1 && fs_packFiles == 2,
          "second-pass failure retries after complete cleanup");
    FreePack(candidate);
    Check(FileOwners() == files, "second-pass retry physically releases its stream");
    Check(FS_Read(path, sizeof(path), f) == 12 && !memcmp(path, "native data\n", 12),
          "second-pass failure and retry keep the prior buffer readable");
    FS_FCloseFile(f);
    End();
}

int main(int argc, char **argv) {
    int i;
    Check(argc >= 2, "real ZIP fixture directory");
    if (argc > 2) {
        i = atoi(argv[2]);
        if (i < 8) Invalid(argv[1], i);
        else if (i < 11) SecondPass(argv[1], i - 7);
        else MountGolden(argv[1]);
        return 0;
    }
    MountGolden(argv[1]);
    for (i = 0; i < 8; i++) Invalid(argv[1], i);
    for (i = 1; i <= 3; i++) SecondPass(argv[1], i);
    puts("Actual ZIP mount metadata, native checksums and candidate cleanup pass");
    return 0;
}
