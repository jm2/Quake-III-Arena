/* Actual ZIP size/open/read paths, prior unique stream ownership and large entries. */
#define main FixtureZipMain
#ifndef Q3_ZIP_NATIVE_FIXTURE
#define Q3_ZIP_NATIVE_FIXTURE "fs_zip_regression.c"
#endif
#include Q3_ZIP_NATIVE_FIXTURE
#undef main
#include <dirent.h>
#include <signal.h>
#include <unistd.h>

static int FileOwners(void) {
    DIR *dir = opendir("/proc/self/fd");
    struct dirent *entry;
    int count = 0;
    Check(dir != NULL, "actual host file descriptor inventory");
    while ((entry = readdir(dir)) != NULL) if (entry->d_name[0] != '.') count++;
    Check(closedir(dir) == 0, "descriptor inventory closes");
    return count;
}

static void Path(char *out, size_t capacity, const char *dir, const char *name) {
    int length = snprintf(out, capacity, "%s/%s", dir, name);
    Check(length >= 0 && (size_t)length < capacity, "bounded fixture path");
}

static void Prior(fileHandle_t *f, fileHandleData_t *saved) {
    char bytes[4];
    Check(FS_FOpenFileRead("native.txt", f, qtrue) == 12 && *f > 0 &&
          fsh[*f].handleFiles.unique &&
          FS_Read(bytes, 3, *f) == 3 && !memcmp(bytes, "nat", 3),
          "prior complete unique stream owner with a native partial cursor");
    memcpy(saved, &fsh[*f], sizeof(*saved));
}

static void Preserved(fileHandle_t f, const fileHandleData_t *saved, int live, int files) {
    int i;
    Check(!memcmp(saved, &fsh[f], sizeof(*saved)) && zoneLive == live &&
          !temporaryLive && !fs_loadStack && FileOwners() == files,
          "failed entry keeps complete prior stream/cursor and releases private owners/streams");
    for (i = 1; i < MAX_FILE_HANDLES; i++) {
        if (i != f) Check(!fsh[i].handleFiles.file.o,
                          "failed entry clears every candidate handle");
    }
}

/* The prior stream still yields the rest of its payload, then closes. */
static void ClosePrior(fileHandle_t f) {
    char bytes[16];
    Check(FS_Read(bytes, sizeof(bytes), f) == 9 && !memcmp(bytes, "ive data\n", 9),
          "prior unique stream payload survives every other open");
    FS_FCloseFile(f);
}

static const char *oversized[] = {
    "intmax.pk3", "signed.pk3", "almost-uintmax.pk3", "uintmax.pk3"
};

static void Reject(char *dir, int kind, int occupied) {
    char path[1024];
    fileHandle_t prior = 0, f;
    fileHandleData_t saved;
    void *bytes;
    int unique, live, files, temps;
    Begin();
    Path(path, sizeof(path), dir, oversized[kind]);
    search.pack = FS_LoadZipFile(path, "sizes.pk3");
    Check(search.pack != NULL, "real declared-size ZIP mount");
    if (occupied) Prior(&prior, &saved);
    live = zoneLive;
    files = FileOwners();
    temps = temporaryRequests;
    for (unique = 0; unique <= 1; unique++) {
        f = 17;
        Check(FS_FOpenFileRead("bad.bin", &f, unique) == -1 && !f,
              "unrepresentable entry rejects before cast/allocation");
        if (occupied) Preserved(prior, &saved, live, files);
        else Check(zoneLive == live && FileOwners() == files && !temporaryLive,
                   "cold oversized entry releases every candidate owner");
    }
    bytes = (void *)1;
    Check(FS_ReadFile("bad.bin", &bytes) == -1 && bytes == NULL &&
          temporaryRequests == temps && !fs_loadStack,
          "oversized full-file read rejects before temporary allocation");
    Check(FS_ReadFile("bad.bin", NULL) == -1, "oversized length query rejects");
    if (occupied) {
        Preserved(prior, &saved, live, files);
        ClosePrior(prior);
    }
    End();
}

static void MaximumQuery(char *dir) {
    char path[1024];
    fileHandle_t prior, f;
    fileHandleData_t saved;
    int unique, live, files, temps;
    Begin();
    Path(path, sizeof(path), dir, "maximum.pk3");
    search.pack = FS_LoadZipFile(path, "maximum.pk3");
    Check(search.pack != NULL, "representable maximum declared-size ZIP mount");
    Prior(&prior, &saved);
    live = zoneLive;
    files = FileOwners();
    temps = temporaryRequests;
    for (unique = 0; unique <= 1; unique++) {
        Check(FS_FOpenFileRead("bad.bin", &f, unique) == INT_MAX - 1 && f > 0 &&
              f != prior && fsh[f].handleFiles.file.z, "native representable scalar open boundary");
        FS_FCloseFile(f);
        Preserved(prior, &saved, live, files);
    }
    Check(FS_ReadFile("bad.bin", NULL) == INT_MAX - 1 && temporaryRequests == temps,
          "representable query retains length without a huge allocation");
    Preserved(prior, &saved, live, files);
    ClosePrior(prior);
    End();
}

static const char *broken[] = {
    "open-small.pk3", "open-large.pk3", "read-small.pk3", "read-large.pk3",
    "stored-small.pk3", "stored-large.pk3", "overrun-small.pk3", "overrun-large.pk3"
};
/* Only a bad local header fails at open; payload faults surface as short reads. */
static const int brokenStreams[] = {0, 0, 1, 1, 1, 1, 1, 1};
static const int brokenSizes[] = {12, 32 * 1024 * 1024, 12, 32 * 1024 * 1024,
                                  12, 32 * 1024 * 1024, 4096, 32 * 1024 * 1024};

/* An unguarded stored-entry decoder never returns, so bound every broken read. */
static void BrokenHung(int number) {
    static const char message[] = "ZIP regression failed: malformed entry read did not finish\n";
    (void)number;
    if (write(2, message, sizeof(message) - 1) < 0) _exit(2);
    _exit(1);
}

static void Broken(char *dir, int kind, int occupied) {
    char path[1024], text[64];
    fileHandle_t prior = 0, f;
    fileHandleData_t saved;
    void *bytes;
    int live, files, n, request;
    signal(SIGALRM, BrokenHung);
    alarm(60);
    Begin();
    Path(path, sizeof(path), dir, broken[kind]);
    search.pack = FS_LoadZipFile(path, "broken.pk3");
    Check(search.pack != NULL, "real malformed-payload ZIP metadata mounts");
    if (occupied) Prior(&prior, &saved);
    live = zoneLive;
    files = FileOwners();
    f = 17;
    n = FS_FOpenFileRead("bad.bin", &f, qtrue);
    if (!brokenStreams[kind]) {
        Check(n == -1 && !f, "failed unzip open clears its handle");
    } else {
        Check(n == brokenSizes[kind] && f > 0 && fsh[f].handleFiles.unique,
              "native streamed declared length of a damaged entry");
        request = brokenSizes[kind] < (int)sizeof(text) ? brokenSizes[kind] : (int)sizeof(text);
        n = FS_Read(text, request, f);
        Check(n < request, "truncated streamed payload does not manufacture a full read");
        if (kind == 4 || kind == 5) {
            Check(n == 6 && !memcmp(text, "native", 6) && FS_Read(text, sizeof(text), f) == 0,
                  "truncated stored stream returns its data, then a stable end of input");
        }
        FS_FCloseFile(f);
    }
    if (occupied) Preserved(prior, &saved, live, files);
    bytes = (void *)1;
    Check(FS_ReadFile("bad.bin", &bytes) == -1 && bytes == NULL,
          "failed full-file open/read returns no partial owner");
    if (occupied) Preserved(prior, &saved, live, files);
    else Check(zoneLive == live && FileOwners() == files && !temporaryLive && !fs_loadStack,
               "cold full-file failure releases physical and logical owners");
    Check(FS_FOpenFileRead("native.txt", &f, qtrue) == 12 && f > 0 && f != prior &&
          FS_Read(text, sizeof(text), f) == 12 && !memcmp(text, "native data\n", 12),
          "failed payload/open retries through an independent valid entry");
    FS_FCloseFile(f);
    if (occupied) {
        Preserved(prior, &saved, live, files);
        ClosePrior(prior);
    }
    End();
    alarm(0);
}

static void CapGolden(char *dir, int occupied) {
    static const char *names[] = {"below.bin", "at.bin", "above.bin", "above.txt"};
    static const int sizes[] = {32 * 1024 * 1024 - 1, 32 * 1024 * 1024,
                               32 * 1024 * 1024 + 1, 32 * 1024 * 1024 + 1};
    char path[1024], text[64];
    fileHandle_t prior = 0, f;
    fileHandleData_t saved;
    int i, j, live, files;
    unsigned char chunk[65536];
    Begin();
    Path(path, sizeof(path), dir, "caps.pk3");
    search.pack = FS_LoadZipFile(path, "caps.pk3");
    Check(search.pack != NULL, "native complete cap-boundary compressed ZIP");
    if (occupied) Prior(&prior, &saved);
    live = zoneLive;
    files = FileOwners();
    for (i = 0; i < 4; i++) {
        Check(FS_FOpenFileRead(names[i], &f, qtrue) == sizes[i] && f > 0 && f != prior,
              "native actual payload length at every former buffering boundary");
        Check(fsh[f].handleFiles.unique && fsh[f].handleFiles.file.z != search.pack->handle &&
              zoneLive - live < 16,
              "every unique entry streams through its own archive without a whole-entry copy");
        Check(FS_Read(text, sizeof(text), f) == sizeof(text), "complete native zero payload read");
        for (j = 0; j < sizeof(text); j++) Check(text[j] == 0, "native cap payload bytes");
        {
            int consumed = sizeof(text);
            while (consumed < sizes[i]) {
                int request = sizes[i] - consumed;
                if (request > (int)sizeof(chunk)) request = sizeof(chunk);
                Check(FS_Read(chunk, request, f) == request,
                      "native streamed cap payload reads completely through refills");
                for (j = 0; j < request; j++)
                    Check(chunk[j] == 0, "every native streamed cap payload byte remains correct");
                consumed += request;
            }
            Check(FS_Read(chunk, sizeof(chunk), f) == 0 &&
                  FS_Read(chunk, 1, f) == 0,
                  "complete native streamed cap payload reaches stable EOF");
        }
        FS_FCloseFile(f);
        if (occupied) Preserved(prior, &saved, live, files);
        Check(FS_ReadFile(names[i], NULL) == sizes[i] && !temporaryLive && !fs_loadStack,
              "native cap scalar queries close their streams without full temporary allocation");
        if (occupied) Preserved(prior, &saved, live, files);
    }
    if (occupied) ClosePrior(prior);
    End();
}

int main(int argc, char **argv) {
    int i;
    Check(argc >= 2, "real entry-boundary ZIP directory");
    if (argc > 2) {
        i = atoi(argv[2]);
        if (i < 4) Reject(argv[1], i, 0);
        else if (i < 12) Broken(argv[1], i - 4, 0);
        else CapGolden(argv[1], 0);
        return 0;
    }
    CapGolden(argv[1], 1);
    MaximumQuery(argv[1]);
    for (i = 0; i < 4; i++) Reject(argv[1], i, 1);
    for (i = 0; i < 8; i++) Broken(argv[1], i, 1);
    puts("Actual ZIP size/cap/open/truncation paths preserve owners and cleanly retry");
    return 0;
}
