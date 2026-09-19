/* Actual character cache hits at native handle capacity and physical owners. */
#define Q3_ITEM_OWNER_ONLY
#define Q3_ITEM_TEST_HEAP_CAPACITY 256
#include "bot_item_config_regression.c"
#ifdef MEMORYMANEGER
extern int numblocks, allocatedmemory, totalmemorysize;
#endif
static const char *characterText = "skill 4 { 0 42 1 1.25 79 \"native\" }";

static void CharacterBegin(void)
{
    int i;
    Begin();
    for (i = 0; i <= MAX_CLIENTS; i++) Check(!botcharacters[i], "prior cached character roots release");
    Attempt(characterText);
}

static void CharacterEnd(void)
{
    int i;
    BotShutdownCharacters();
    for (i = 0; i <= MAX_CLIENTS; i++) Check(!botcharacters[i], "shutdown clears all native cache roots");
    Check(!heapLive && !hunkLive && !numtokens, "cached character/string/source owners physically release before engine reset");
    End();
#ifdef MEMORYMANEGER
    Check(!numblocks && !allocatedmemory && !totalmemorysize, "all tracked cached owners release logically");
#endif
}

static void CharacterValues(int handle)
{
    char text[16];
    Check(botcharacters[handle] && botcharacters[handle]->skill == 4 &&
          Characteristic_Integer(handle, 0) == 42 && Characteristic_Float(handle, 1) == 1.25f,
          "native cached skill/integer/float fields remain");
    Characteristic_String(handle, 79, text, sizeof(text));
    Check(!strcmp(text, "native"), "native final accessible string characteristic remains");
}

static void Golden(void)
{
    int handle;
    CharacterBegin(); handle = BotLoadCachedCharacter("bots/native.c", 4, 0);
    Check(handle == 1 && opens == 1 && closes == 1 && heapLive == 2 && !errors && !numtokens,
          "native cold cache load publishes complete character/string owners"); CharacterValues(handle);
    Attempt(characterText); failAt = 1;
    Check(BotLoadCachedCharacter("bots/native.c", 4, 0) == handle && !requests && !opens && !errors,
          "ordinary native exact cached hit requires no slot or import"); CharacterValues(handle);
    Attempt(characterText);
    Check(BotLoadCachedCharacter("bots/native.c", 4, 1) == 2 && opens == 1 && closes == 1 && heapLive == 4,
          "native reload bypasses cache and allocates a distinct complete owner"); CharacterValues(2); CharacterEnd();
}

static void FullCache(int kind)
{
    int i, baseline, handle;
    char path[MAX_QPATH];
    size_t size = sizeof(bot_character_t) + MAX_CHARACTERISTICS * sizeof(bot_characteristic_t);
    unsigned char *saved = malloc(size * MAX_CLIENTS);
    bot_character_t *roots[MAX_CLIENTS + 1];
    CharacterBegin(); Check(saved != NULL, "physical complete cached character snapshot");
    for (i = 1; i <= MAX_CLIENTS; i++) {
        snprintf(path, sizeof(path), "bots/cache%d.c", i); Attempt(characterText);
        Check(BotLoadCachedCharacter(path, 4, 0) == i, "actual cached factory fills all 64 native slots");
        CharacterValues(i); roots[i] = botcharacters[i]; memcpy(saved + (size_t)(i - 1) * size, roots[i], size);
    }
    baseline = heapLive; Check(baseline == 2 * MAX_CLIENTS, "full cache has exactly its physical character/string owners");
    Attempt(characterText); failAt = 1;
    handle = kind == 0 ? 1 : kind == 1 ? MAX_CLIENTS : 17;
    snprintf(path, sizeof(path), "bots/cache%d.c", handle);
    Check(BotLoadCachedCharacter(path, kind == 2 ? -1 : kind == 3 ? 4.005f : 4, 0) == handle &&
          !requests && !opens && !closes && !errors && heapLive == baseline,
          "full native cache returns existing exact/any-skill/tolerance hit without imports");
    for (i = 1; i <= MAX_CLIENTS; i++) Check(botcharacters[i] == roots[i] &&
          !memcmp(saved + (size_t)(i - 1) * size, roots[i], size) && !strcmp(roots[i]->c[79].value.string, "native"),
          "every full-cache root/header/field/string survives cached lookup");
    CharacterValues(handle); Attempt(characterText); failAt = 1;
    Check(!BotLoadCachedCharacter(path, 4, 1) && !BotLoadCachedCharacter("bots/missing.c", 4, 0) &&
          !requests && !opens && !errors && heapLive == baseline,
          "full cache still rejects reload/missing-key loads before imports");
    BotFreeCharacter2(MAX_CLIENTS); Attempt(characterText);
    Check(BotLoadCachedCharacter("bots/retry.c", 4, 0) == MAX_CLIENTS && heapLive == baseline && !errors,
          "full native cache frees and reuses the final slot"); CharacterValues(MAX_CLIENTS);
    free(saved); CharacterEnd();
}

int main(int argc, char **argv)
{
    int i;
    if (argc > 1) { i = atoi(argv[1]); if (i < 4) FullCache(i); else Golden(); return 0; }
    Golden(); for (i = 0; i < 4; i++) FullCache(i);
    puts("Actual full character cache/native 64-slot hits, reload/exhaustion, prior bytes and physical shutdown pass (issue #48)");
    return 0;
}
