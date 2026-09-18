/* Actual bot adapters/imports and native zone allocator, including debug metadata. */
#include <stdint.h>
#include <setjmp.h>
void Hunk_SmallLog(void);
#include Q3_ZONE_COMMON_SOURCE
#include "../code/botlib/l_memory.c"
#include Q3_ZONE_IMPORT_SOURCE
botlib_import_t botimport;
static unsigned char zoneArena[2048] __attribute__((aligned(32)));
static jmp_buf rejection;
static int expectingError, errors;
static void Check(int condition, const char *message) {
    if (!condition) { fprintf(stderr, "Bot zone regression failed: %s\n", message); exit(1); }
}
void QDECL Com_Error(int level, const char *format, ...) {
    (void)format;
    Check(expectingError && level == ERR_FATAL, "controlled native zone rejection");
    errors++;
    longjmp(rejection, 1);
}
void Z_LogHeap(void) {}
void Hunk_Log(void) {}
void Hunk_SmallLog(void) {}
#ifndef Com_Memset
void Com_Memset(void *out, int value, size_t size) { memset(out, value, size); }
#endif
static void Reset(int cost) {
    memset(zoneArena, 0xa5, sizeof(zoneArena));
    mainzone = smallzone = (memzone_t *)(zoneArena + 32);
    Z_ClearZone(mainzone, (int)sizeof(memzone_t) + cost);
}
static void Guards(int cost) {
    int i, end = 32 + (int)sizeof(memzone_t) + cost;
    for (i = 0; i < 32; i++) Check(zoneArena[i] == 0xa5, "leading zone guard");
    for (i = end; i < (int)sizeof(zoneArena); i++) Check(zoneArena[i] == 0xa5, "trailing zone guard");
}
#ifdef Q3_ZONE_ORIGINAL_PROOF
int main(void) {
    int request = INT_MAX - (int)sizeof(memblock_t) - 4;
    Reset(1024);
    botimport.GetMemory = BotImport_GetMemory;
    GetClearedMemory((unsigned long)request - sizeof(unsigned long));
    Check(0, "original native overhead overflow accepted");
    return 0;
}
#else
static void Reject(int size) {
    unsigned char before[sizeof(zoneArena)];
    int count = errors;
    Reset(1024);
    memcpy(before, zoneArena, sizeof(before));
    Check(BotImport_GetMemory(size) == NULL, "invalid native heap import returns null");
    Check(!memcmp(before, zoneArena, sizeof(before)), "failed import retains all native zone state");
    expectingError = 1;
    if (!setjmp(rejection)) { Z_TagMalloc(size, TAG_BOTLIB); Check(0, "invalid direct zone request accepted"); }
    expectingError = 0;
    Check(errors == count + 1 && !memcmp(before, zoneArena, sizeof(before)), "direct rejection retains native zone state");
}
int main(void) {
    int size, mode, offset, request, cost, i;
    unsigned char *memory, before[sizeof(zoneArena)];
    int64_t expected;
    botimport.GetMemory = BotImport_GetMemory;
    botimport.FreeMemory = Z_Free;
    botimport.HunkAlloc = BotImport_HunkAlloc;
    for (mode = 0; mode < 2; mode++) for (size = 0; size <= 65; size++) {
        request = size + (int)sizeof(unsigned long);
        cost = (request + (int)sizeof(memblock_t) + 4 + 3) / 4 * 4;
        Check(Z_AllocationSize(request) == cost, "native header/trailer/alignment cost");
        /* Exact blocks avoid unrelated 64-bit host fragment alignment differences. */
        Reset(cost);
        memory = mode ? GetClearedMemory(size) : GetMemory(size);
        Check(memory != NULL && mainzone->used == cost, "native import and bot prefix allocate exact zone cost");
        Check(memory == (byte *)mainzone->blocklist.next + sizeof(memblock_t) + sizeof(unsigned long), "native payload address retains both ownership prefixes");
        for (i = 0; i < size; i++) Check(memory[i] == (mode ? 0 : 0xa5), "raw and cleared native payload bytes");
        memset(memory, 0x5a, size);
        FreeMemory(memory);
        Check(mainzone->used == 0 && mainzone->blocklist.next->tag == 0, "native heap release restores zone owner");
        Guards(cost);
    }
    /* A naturally aligned split exercises native fragment and coalescing ownership. */
    request = 0;
    while ((request + (int)sizeof(unsigned long) + (int)sizeof(memblock_t) + 4 + 3) / 4 * 4 % _Alignof(memblock_t)) request++;
    Reset(1024);
    memory = GetClearedMemory(request);
    Check(memory != NULL && mainzone->blocklist.next->next != &mainzone->blocklist, "native allocation splits a free fragment");
    FreeMemory(memory);
    Check(mainzone->used == 0 && mainzone->blocklist.next->size == 1024 && mainzone->blocklist.next->next == &mainzone->blocklist, "native release coalesces fragment");
    Guards(1024);
    for (offset = 0; offset < 128; offset++) {
        request = INT_MAX - offset;
        expected = ((int64_t)request + sizeof(memblock_t) + 4 + 3) / 4 * 4;
        cost = Z_AllocationSize(request);
        Check(cost == (expected > INT_MAX ? -1 : (int)expected), "signed maximum native overhead boundaries");
        if (cost < 0) {
            Reject(request);
            Reset(1024); memcpy(before, zoneArena, sizeof(before));
            Check(GetClearedMemory((unsigned long)request - sizeof(unsigned long)) == NULL, "native bot clearing propagates overhead rejection");
            Check(!memcmp(before, zoneArena, sizeof(before)), "nullable clearing retains zone state");
        }
        if (Hunk_AllocationSize(request) < 0) {
            Check(BotImport_HunkAlloc(request) == NULL, "invalid native hunk import returns null before allocator");
            Check(GetClearedHunkMemory((unsigned long)request - sizeof(unsigned long)) == NULL, "native bot hunk clearing propagates metadata/alignment rejection");
        }
    }
    Reject(-1); Reject(INT_MIN);
    Check(BotImport_HunkAlloc(-1) == NULL && BotImport_HunkAlloc(INT_MIN) == NULL, "negative hunk imports reject");
    printf("Native bot imports and zone header/trailer/alignment ownership passed (issues #47/#48)\n");
    return 0;
}
#endif
