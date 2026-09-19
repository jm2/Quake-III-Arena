/* Real buffered opens must continue owning their native handle slots. */
#define Q3_ZIP_ZONE_CAPACITY 128
#define main FixtureZipMain
#ifndef Q3_ZIP_NATIVE_FIXTURE
#define Q3_ZIP_NATIVE_FIXTURE "fs_zip_regression.c"
#endif
#include Q3_ZIP_NATIVE_FIXTURE
#undef main

static void Pair(char *path) {
    fileHandle_t first, second, retry;
    fileHandleData_t prior;
    char bytes[32], payload[12];
    int before;
    Begin();
    search.pack = FS_LoadZipFile(path, "native.pk3");
    Check(search.pack != NULL, "actual compressed ZIP mount");
    Check(FS_FOpenFileRead("native.txt", &first, qtrue) == 12 && first > 0,
          "first complete native buffered owner");
    Check(FS_Read(bytes, 4, first) == 4 && !memcmp(bytes, "nati", 4),
          "first native partial read");
    memcpy(&prior, &fsh[first], sizeof(prior));
    memcpy(payload, fsh[first].buffer, sizeof(payload));
    before = zoneLive;
    Check(FS_FOpenFileRead("native.txt", &second, qtrue) == 12 &&
          second > 0 && second != first, "simultaneous buffers own distinct slots");
    Check(zoneLive == before + 1 && !memcmp(&prior, &fsh[first], sizeof(prior)) &&
          !memcmp(payload, fsh[first].buffer, sizeof(payload)),
          "second open preserves complete first owner and cursor");
    Check(FS_Read(bytes, sizeof(bytes), second) == 12 &&
          !memcmp(bytes, "native data\n", 12), "second independent payload");
    FS_FCloseFile(second);
    Check(!memcmp(&prior, &fsh[first], sizeof(prior)),
          "second close preserves complete first owner");
    retry = 17;
    Check(FS_FOpenFileRead("missing.txt", &retry, qtrue) == -1 && retry == 0,
          "missing native entry fails cleanly");
    Check(!memcmp(&prior, &fsh[first], sizeof(prior)) && zoneLive == before,
          "failed open preserves occupied buffered owner");
    Check(FS_FOpenFileRead("native.txt", &retry, qtrue) == 12 && retry == second,
          "closed buffered slot retries without displacing its sibling");
    Check(FS_Read(bytes, sizeof(bytes), first) == 8 &&
          !memcmp(bytes, "ve data\n", 8) && FS_Read(bytes, 1, first) == 0,
          "first cursor and native EOF survive other opens");
    FS_FCloseFile(first);
    Check(FS_Read(bytes, sizeof(bytes), retry) == 12 &&
          !memcmp(bytes, "native data\n", 12), "retry payload survives first close");
    FS_FCloseFile(retry);
    End();
}

static void AllSlots(char *path) {
    fileHandle_t handles[MAX_FILE_HANDLES - 1], replacement;
    fileHandleData_t snapshots[MAX_FILE_HANDLES - 1];
    char bytes[16];
    int i, j, live;
    Begin();
    search.pack = FS_LoadZipFile(path, "native.pk3");
    Check(search.pack != NULL, "native full-slot ZIP mount");
    for (i = 0; i < MAX_FILE_HANDLES - 1; i++) {
        Check(FS_FOpenFileRead("native.txt", &handles[i], qtrue) == 12 &&
              handles[i] == i + 1, "every native slot retains its buffered owner");
        memcpy(&snapshots[i], &fsh[handles[i]], sizeof(snapshots[i]));
        for (j = 0; j <= i; j++) {
            Check(!memcmp(&snapshots[j], &fsh[handles[j]], sizeof(snapshots[j])) &&
                  !memcmp(fsh[handles[j]].buffer, "native data\n", 12),
                  "every earlier complete buffer remains intact");
        }
    }
    live = zoneLive;
    FS_FCloseFile(handles[MAX_FILE_HANDLES - 2]);
    Check(zoneLive == live - 1, "final native slot physically releases");
    Check(FS_FOpenFileRead("native.txt", &replacement, qtrue) == 12 &&
          replacement == MAX_FILE_HANDLES - 1 && zoneLive == live,
          "final native slot reuses only its released owner");
    handles[MAX_FILE_HANDLES - 2] = replacement;
    for (i = MAX_FILE_HANDLES - 2; i >= 0; i--) {
        Check(FS_Read(bytes, sizeof(bytes), handles[i]) == 12 &&
              !memcmp(bytes, "native data\n", 12), "all native payloads remain readable");
        FS_FCloseFile(handles[i]);
    }
    End();
}

int main(int argc, char **argv) {
    Check(argc >= 2, "real compressed ZIP input");
    if (argc > 2) {
        int kind = atoi(argv[2]);
        if (kind == 0) Pair(argv[1]);
        else if (kind == 1) AllSlots(argv[1]);
        else { Golden(argv[1]); AllocationGolden(); }
        return 0;
    }
    Golden(argv[1]);
    Pair(argv[1]);
    AllSlots(argv[1]);
    puts("Actual buffered handles preserve all native slots and physical owners");
    return 0;
}
