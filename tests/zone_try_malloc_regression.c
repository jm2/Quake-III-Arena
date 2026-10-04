/* Issue #43: Z_TryTagMalloc, the renderer's ri.TryMalloc, returns NULL instead of ERR_FATAL
   exactly when Z_TagMalloc would fail, including when the free space is split into fragments. */
#include <setjmp.h>
void Hunk_SmallLog(void);
#include Q3_ZONE_COMMON_SOURCE
static unsigned char zoneArena[4096] __attribute__((aligned(32)));
static jmp_buf fatal;
static int expectingError, errors;
static void Check(int condition, const char *message) {
    if (!condition) { fprintf(stderr, "Zone try-malloc regression failed: %s\n", message); exit(1); }
}
void QDECL Com_Error(int level, const char *format, ...) {
    (void)format;
    Check(expectingError && level == ERR_FATAL, "a non-fatal zone request raised an engine error");
    errors++;
    longjmp(fatal, 1);
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
/* A payload whose zone cost keeps every split fragment header naturally aligned. */
static int Payload(int minimum) {
    while (Z_AllocationSize(minimum) % (int)_Alignof(memblock_t)) minimum++;
    return minimum;
}
/* Z_TryTagMalloc must leave the zone byte-identical when it refuses, and Z_TagMalloc must agree. */
static void Refuse(int size, const char *message) {
    unsigned char before[sizeof(zoneArena)];
    int count = errors;
    memcpy(before, zoneArena, sizeof(before));
    Check(Z_TryTagMalloc(size, TAG_RENDERER) == NULL && errors == count, message);
    Check(!memcmp(before, zoneArena, sizeof(before)), "a refused request changes no zone state");
    expectingError = 1;
    if (!setjmp(fatal)) { Z_TagMalloc(size, TAG_RENDERER); Check(0, "Z_TagMalloc accepted a request Z_TryTagMalloc refused"); }
    expectingError = 0;
    Check(errors == count + 1, "Z_TagMalloc is fatal where Z_TryTagMalloc returns NULL");
}
int main(void) {
    int small = Payload(100), big, cost = 1024, freeBytes;
    void *a, *b, *c, *d;
    /* One free block: a request that fits it exactly succeeds, one more byte does not. */
    big = cost - (int)sizeof(memblock_t) - 4;
    Reset(cost);
    Refuse(big + 1, "one byte over the only free block");
    a = Z_TryTagMalloc(big, TAG_RENDERER);
    Check(a != NULL && mainzone->used == cost, "a request that fills the zone exactly");
    Refuse(1, "any request in a full zone");
    Z_Free(a);
    Check(mainzone->used == 0, "release restores the zone");
    /* Split the free space: enough bytes in total, but no single block is large enough. */
    Reset(cost);
    a = Z_TryTagMalloc(small, TAG_RENDERER); b = Z_TryTagMalloc(small, TAG_RENDERER);
    c = Z_TryTagMalloc(small, TAG_RENDERER); d = Z_TryTagMalloc(small, TAG_RENDERER);
    Check(a && b && c && d, "four small blocks");
    Z_Free(a); Z_Free(c);
    freeBytes = Z_AvailableMemory();
    Check(freeBytes > Z_AllocationSize(small) * 2, "fragmented zone has room in total");
    Refuse(freeBytes - (int)sizeof(memblock_t) - 4, "no single fragment holds the total free space");
    a = Z_TryTagMalloc(small, TAG_RENDERER);
    Check(a != NULL, "a request that fits one fragment");
    Z_Free(a); Z_Free(b); Z_Free(d);
    Check(mainzone->used == 0, "fragmented zone coalesces");
    /* Invalid requests are refused without an engine error. */
    Reset(cost);
    freeBytes = errors;
    Check(Z_TryTagMalloc(-1, TAG_RENDERER) == NULL && Z_TryTagMalloc(INT_MAX, TAG_RENDERER) == NULL &&
          Z_TryTagMalloc(16, 0) == NULL && errors == freeBytes && mainzone->used == 0, "negative, overflowing and untagged requests");
    printf("Non-fatal zone allocation regressions passed (issue #43)\n");
    return 0;
}
