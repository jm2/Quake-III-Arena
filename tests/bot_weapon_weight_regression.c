/* Actual weapon weight pairs, fuzzy parser/evaluator and physical owners. */
#define main WeaponConfigFixtureMain
#include "bot_weapon_config_regression.c"
#undef main
#ifdef MEMORYMANEGER
extern int numblocks, allocatedmemory, totalmemorysize;
#endif
static const char *weightText = "weight \"NativeWeapon\" return 42;\n";

static int WeightBegin(int reload)
{
    int handle;
    Begin(); Check(!weaponconfig, "prior weapon table releases"); Attempt(weaponText);
    Check(BotSetupWeaponAI() == BLERR_NOERROR, "actual complete weapon table");
    Check(LibVar("bot_reloadcharacters", reload ? "1" : "0") != NULL, "native weight ownership policy");
    handle = BotAllocWeaponState();
    Check(handle == 1 && botweaponstates[handle] != NULL, "actual native weapon state");
    Attempt(weightText);
    Check(BotLoadWeaponWeights(handle, "prior.w") == BLERR_NOERROR, "actual initial weight pair");
    return handle;
}

static void PairValues(int handle)
{
    bot_weaponstate_t *state = BotWeaponStateFromHandle(handle);
    int i, inventory[4] = {0};
    Check(state && state->weaponweightconfig && state->weaponweightindex &&
          state->weaponweightconfig->numweights == 1 &&
          !strcmp(state->weaponweightconfig->weights[0].name, "NativeWeapon") &&
          state->weaponweightconfig->weights[0].firstseperator->weight == 42,
          "complete native weight name/tree and pair pointers");
    for (i = 0; i < weaponconfig->numweapons; i++)
        Check(state->weaponweightindex[i] == (i == 1 ? 0 : -1), "actual native weight-name index mapping");
    Check(BotChooseBestFightWeapon(handle, inventory) == 1,
          "actual native weight evaluation selects the same valid weapon");
}

static void WeightEnd(void)
{
    BotShutdownWeaponAI(); BotShutdownWeights();
    Check(!weaponconfig && !botweaponstates[1], "actual public shutdown clears table/state roots");
    End();
#ifdef MEMORYMANEGER
    Check(!numblocks && !allocatedmemory && !totalmemorysize,
          "public shutdown releases all native tracked logical records");
#endif
}

static int WeightImports(int reload)
{
    int handle = WeightBegin(reload), count;
    Attempt(weightText); Check(BotLoadWeaponWeights(handle, "candidate.w") == BLERR_NOERROR,
          "measure real candidate weight/index imports");
    PairValues(handle); count = requests; WeightEnd(); return count;
}

static void PairNullable(int reload, int position, int count)
{
    int handle = WeightBegin(reload), baseline = heapLive, arena = hunkLive;
    bot_weaponstate_t *state = botweaponstates[handle], saved = *state;
    weightconfig_t header = *saved.weaponweightconfig;
    fuzzyseperator_t tree = *header.weights[0].firstseperator;
    int indices[32];
    char name[80];
    memcpy(indices, saved.weaponweightindex, sizeof(indices));
    strcpy(name, header.weights[0].name);
    Attempt(weightText); failAt = position;
    Check(BotLoadWeaponWeights(handle, "candidate.w") == BLERR_CANNOTLOADWEAPONWEIGHTS &&
          requests >= failAt && errors > 0 && !numtokens && opens == closes &&
          !memcmp(state, &saved, sizeof(saved)) && hunkLive == arena,
          "nullable candidate retains working pair and releases partial source/tree/index owners");
    Check(heapLive == baseline + (!reload && position == count ? 3 : 0),
          "only a complete native cached candidate survives its index failure");
    Check(!memcmp(saved.weaponweightconfig, &header, sizeof(header)) &&
          !memcmp(header.weights[0].firstseperator, &tree, sizeof(tree)) &&
          !strcmp(header.weights[0].name, name) &&
          !memcmp(saved.weaponweightindex, indices, sizeof(indices)),
          "all prior weight header/tree/name/index bytes survive failure");
    PairValues(handle); Attempt(weightText);
    Check(BotLoadWeaponWeights(handle, "candidate.w") == BLERR_NOERROR,
          "nullable weight/index candidate retries");
    PairValues(handle); WeightEnd();
}

static void PairMalformed(int reload, int suffix)
{
    int handle = WeightBegin(reload), baseline = heapLive;
    bot_weaponstate_t saved = *botweaponstates[handle];
    char text[256];
    if (suffix) { snprintf(text, sizeof(text), "%s#unknown", weightText); Attempt(text); }
    else Attempt("weight \"broken\"");
    Check(BotLoadWeaponWeights(handle, "failed.w") == BLERR_CANNOTLOADWEAPONWEIGHTS &&
          !memcmp(&saved, botweaponstates[handle], sizeof(saved)) && heapLive == baseline &&
          !numtokens && errors > 0 && opens == closes,
          "malformed/suffix weight parse retains complete previous pair without private leaks");
    PairValues(handle); WeightEnd();
}

static void MissingTable(int reload)
{
    int handle = WeightBegin(reload), baseline = heapLive;
    weaponconfig_t *table = weaponconfig;
    bot_weaponstate_t saved = *botweaponstates[handle];
    Attempt(weightText); weaponconfig = NULL;
    Check(BotLoadWeaponWeights(handle, "candidate.w") == BLERR_CANNOTLOADWEAPONCONFIG &&
          !requests && !opens && heapLive == baseline &&
          !memcmp(&saved, botweaponstates[handle], sizeof(saved)),
          "missing weapon table rejects before candidate imports or old-pair release");
    weaponconfig = table; PairValues(handle); WeightEnd();
}

static void RepeatedCleanup(int reload)
{
    int handle = WeightBegin(reload), inventory[4] = {0}, baseline;
    PairValues(handle); BotFreeWeaponWeights(handle);
    Check(!botweaponstates[handle]->weaponweightconfig && !botweaponstates[handle]->weaponweightindex &&
          BotChooseBestFightWeapon(handle, inventory) == 0, "public cleanup clears both roots before reuse");
    baseline = heapLive; BotFreeWeaponWeights(handle);
    Check(heapLive == baseline, "repeated pair cleanup releases no stale owners");
    Attempt(weightText); Check(BotLoadWeaponWeights(handle, "prior.w") == BLERR_NOERROR,
          "cleaned native pair reloads");
    PairValues(handle); WeightEnd();
}

static void CacheAlias(void)
{
    int handle = WeightBegin(0), baseline = heapLive;
    bot_weaponstate_t saved = *botweaponstates[handle];
    Attempt(weightText); failAt = 1;
    Check(BotLoadWeaponWeights(handle, "prior.w") == BLERR_CANNOTLOADWEAPONWEIGHTS &&
          requests == 1 && !opens && errors == 1 && heapLive == baseline &&
          !memcmp(&saved, botweaponstates[handle], sizeof(saved)),
          "cached alias plus nullable replacement index keeps both old roots and native cache owner");
    PairValues(handle); Attempt(weightText);
    Check(BotLoadWeaponWeights(handle, "prior.w") == BLERR_NOERROR && requests == 1 &&
          !opens && heapLive == baseline && botweaponstates[handle]->weaponweightconfig == saved.weaponweightconfig,
          "native cache alias replaces only complete index storage");
    PairValues(handle); WeightEnd();
}

static void InvalidIndex(void)
{
    int handle = WeightBegin(0), baseline = heapLive, imports, i;
    weaponconfig_t invalid = *weaponconfig;
    int counts[] = {-1, INT_MAX};
    for (i = 0; i < 2; i++) {
        invalid.numweapons = counts[i]; Attempt(weightText); imports = requests;
        Check(!WeaponWeightIndex(botweaponstates[handle]->weaponweightconfig, &invalid) &&
              requests == imports && errors == 1 && heapLive == baseline,
              "invalid index signed count/cost stops before allocation/write");
    }
    Attempt(weightText);
    Check(!WeaponWeightIndex(NULL, weaponconfig) && !WeaponWeightIndex(botweaponstates[handle]->weaponweightconfig, NULL) &&
          !requests && errors == 2, "missing private index inputs reject before imports");
    invalid.numweapons = 0; Attempt(weightText);
    {
        int *empty = WeaponWeightIndex(botweaponstates[handle]->weaponweightconfig, &invalid);
        Check(empty != NULL && requests == 1 && !errors && heapLive == baseline + 1,
              "native zero index capacity keeps its ordinary complete empty owner");
        FreeMemory(empty);
    }
    PairValues(handle); WeightEnd();
}

static void InvalidFilename(int reload)
{
    int handle = WeightBegin(reload), baseline = heapLive;
    bot_weaponstate_t saved = *botweaponstates[handle];
    Attempt(weightText);
    Check(BotLoadWeaponWeights(handle, NULL) == BLERR_CANNOTLOADWEAPONWEIGHTS &&
          !requests && !opens && errors == 2 && heapLive == baseline &&
          !memcmp(&saved, botweaponstates[handle], sizeof(saved)),
          "NULL filename rejects safely without disturbing the working pair");
    PairValues(handle); WeightEnd();
}

static void FirstGolden(int reload)
{
    int handle = WeightBegin(reload);
    PairValues(handle); WeightEnd();
}

int main(int argc, char **argv)
{
    int reload, position, count, total = 0;
    if (argc > 1) {
        position = atoi(argv[1]);
        if (position < 4) PairMalformed(position / 2, position % 2);
        else if (position == 4) CacheAlias();
        else if (position == 5) RepeatedCleanup(1);
        else if (position == 6) MissingTable(0);
        else if (position == 7) InvalidIndex();
        else { FirstGolden(0); FirstGolden(1); }
        return 0;
    }
    FirstGolden(0); FirstGolden(1); CacheAlias(); InvalidIndex();
    for (reload = 0; reload < 2; reload++) {
        count = WeightImports(reload); total += count;
        for (position = 1; position <= count; position++) PairNullable(reload, position, count);
        PairMalformed(reload, 0); PairMalformed(reload, 1);
        MissingTable(reload); RepeatedCleanup(reload); InvalidFilename(reload);
    }
    printf("Actual weapon weight pair, %d nullable stages, cached aliases, prior bytes, evaluation/retry and public physical shutdown pass (issue #48)\n", total);
    return 0;
}
