/* Actual item weight pairs, fuzzy parser/evaluator and physical owners. */
#define main ItemConfigFixtureMain
#include "bot_item_config_regression.c"
#undef main
#ifdef MEMORYMANEGER
extern int numblocks, allocatedmemory, totalmemorysize;
#endif
static const char *weightText = "weight \"weapon_native\" return 42;\n";

static int WeightBegin(int reload)
{
    int handle;
    Begin(); Check(!itemconfig, "prior item table releases"); itemconfig = LoadItemConfig("items.c");
    Values(itemconfig);
    Check(LibVar("bot_reloadcharacters", reload ? "1" : "0") != NULL, "native weight ownership policy");
    handle = BotAllocGoalState(17);
    Check(handle == 1 && botgoalstates[handle] != NULL, "actual native item state");
    Attempt(weightText);
    Check(BotLoadItemWeights(handle, "prior.w") == BLERR_NOERROR, "actual initial weight pair");
    return handle;
}

static void PairValues(int handle)
{
    bot_goalstate_t *state = BotGoalStateFromHandle(handle);
    int i, inventory[64] = {0};
    Check(state && state->itemweightconfig && state->itemweightindex &&
          state->itemweightconfig->numweights == 1 &&
          !strcmp(state->itemweightconfig->weights[0].name, "weapon_native") &&
          state->itemweightconfig->weights[0].firstseperator->weight == 42,
          "complete native weight name/tree and pair pointers");
    for (i = 0; i < itemconfig->numiteminfo; i++)
        Check(state->itemweightindex[i] == 0, "actual native classname index mapping");
    Check(FuzzyWeight(inventory, state->itemweightconfig, state->itemweightindex[0]) == 42,
          "actual native classname mapping and fuzzy evaluation retain the same weight");
}

static void WeightEnd(void)
{
    BotShutdownGoalAI(); BotShutdownWeights();
    Check(!itemconfig && !botgoalstates[1], "actual public shutdown clears table/state roots");
    End();
#ifdef MEMORYMANEGER
    Check(!numblocks && !allocatedmemory && !totalmemorysize,
          "public shutdown releases all native tracked logical records");
#endif
}

static int WeightImports(int reload)
{
    int handle = WeightBegin(reload), count;
    Attempt(weightText); Check(BotLoadItemWeights(handle, "candidate.w") == BLERR_NOERROR,
          "measure real candidate weight/index imports");
    PairValues(handle); count = requests; WeightEnd(); return count;
}

static void PairNullable(int reload, int position, int count)
{
    int handle = WeightBegin(reload), baseline = heapLive, arena = hunkLive;
    bot_goalstate_t *state = botgoalstates[handle], saved = *state;
    weightconfig_t header = *saved.itemweightconfig;
    fuzzyseperator_t tree = *header.weights[0].firstseperator;
    int indices[1];
    char name[80];
    memcpy(indices, saved.itemweightindex, sizeof(indices));
    strcpy(name, header.weights[0].name);
    Attempt(weightText); failAt = position;
    Check(BotLoadItemWeights(handle, "candidate.w") == BLERR_CANNOTLOADITEMWEIGHTS &&
          requests >= failAt && errors > 0 && !numtokens && opens == closes &&
          !memcmp(state, &saved, sizeof(saved)) && hunkLive == arena,
          "nullable candidate retains working pair and releases partial source/tree/index owners");
    Check(heapLive == baseline + (!reload && position == count ? 3 : 0),
          "only a complete native cached candidate survives its index failure");
    Check(!memcmp(saved.itemweightconfig, &header, sizeof(header)) &&
          !memcmp(header.weights[0].firstseperator, &tree, sizeof(tree)) &&
          !strcmp(header.weights[0].name, name) &&
          !memcmp(saved.itemweightindex, indices, sizeof(indices)),
          "all prior weight header/tree/name/index bytes survive failure");
    PairValues(handle); Attempt(weightText);
    Check(BotLoadItemWeights(handle, "candidate.w") == BLERR_NOERROR,
          "nullable weight/index candidate retries");
    PairValues(handle); WeightEnd();
}

static void PairMalformed(int reload, int suffix)
{
    int handle = WeightBegin(reload), baseline = heapLive;
    bot_goalstate_t saved = *botgoalstates[handle];
    char text[256];
    if (suffix) { snprintf(text, sizeof(text), "%s#unknown", weightText); Attempt(text); }
    else Attempt("weight \"broken\"");
    Check(BotLoadItemWeights(handle, "failed.w") == BLERR_CANNOTLOADITEMWEIGHTS &&
          !memcmp(&saved, botgoalstates[handle], sizeof(saved)) && heapLive == baseline &&
          !numtokens && errors > 0 && opens == closes,
          "malformed/suffix weight parse retains complete previous pair without private leaks");
    PairValues(handle); WeightEnd();
}

static void MissingTable(int reload)
{
    int handle = WeightBegin(reload), baseline = heapLive;
    itemconfig_t *table = itemconfig;
    bot_goalstate_t saved = *botgoalstates[handle];
    Attempt(weightText); itemconfig = NULL;
    Check(BotLoadItemWeights(handle, "candidate.w") == BLERR_CANNOTLOADITEMWEIGHTS &&
          !requests && !opens && heapLive == baseline &&
          !memcmp(&saved, botgoalstates[handle], sizeof(saved)),
          "missing item table rejects before candidate imports or old-pair release");
    itemconfig = table; PairValues(handle); WeightEnd();
}

static void RepeatedCleanup(int reload)
{
    int handle = WeightBegin(reload), baseline;
    PairValues(handle); BotFreeItemWeights(handle);
    Check(!botgoalstates[handle]->itemweightconfig && !botgoalstates[handle]->itemweightindex, "public cleanup clears both roots before reuse");
    baseline = heapLive; BotFreeItemWeights(handle);
    Check(heapLive == baseline, "repeated pair cleanup releases no stale owners");
    Attempt(weightText); Check(BotLoadItemWeights(handle, "prior.w") == BLERR_NOERROR,
          "cleaned native pair reloads");
    PairValues(handle); WeightEnd();
}

static void CacheAlias(void)
{
    int handle = WeightBegin(0), baseline = heapLive;
    bot_goalstate_t saved = *botgoalstates[handle];
    Attempt(weightText); failAt = 1;
    Check(BotLoadItemWeights(handle, "prior.w") == BLERR_CANNOTLOADITEMWEIGHTS &&
          requests == 1 && !opens && errors == 1 && heapLive == baseline &&
          !memcmp(&saved, botgoalstates[handle], sizeof(saved)),
          "cached alias plus nullable replacement index keeps both old roots and native cache owner");
    PairValues(handle); Attempt(weightText);
    Check(BotLoadItemWeights(handle, "prior.w") == BLERR_NOERROR && requests == 1 &&
          !opens && heapLive == baseline && botgoalstates[handle]->itemweightconfig == saved.itemweightconfig,
          "native cache alias replaces only complete index storage");
    PairValues(handle); WeightEnd();
}

static void InvalidIndex(void)
{
    int handle = WeightBegin(0), baseline = heapLive, imports, i;
    itemconfig_t invalid = *itemconfig;
    int counts[] = {-1, INT_MAX};
    for (i = 0; i < 2; i++) {
        invalid.numiteminfo = counts[i]; Attempt(weightText); imports = requests;
        Check(!ItemWeightIndex(botgoalstates[handle]->itemweightconfig, &invalid) &&
              requests == imports && errors == 1 && heapLive == baseline,
              "invalid index signed count/cost stops before allocation/write");
    }
    Attempt(weightText);
    Check(!ItemWeightIndex(NULL, itemconfig) && !ItemWeightIndex(botgoalstates[handle]->itemweightconfig, NULL) &&
          !requests && errors == 2, "missing private index inputs reject before imports");
    invalid = *itemconfig; invalid.iteminfo = NULL; Attempt(weightText);
    Check(!ItemWeightIndex(botgoalstates[handle]->itemweightconfig, &invalid) && !requests && errors == 1 &&
          heapLive == baseline, "nonempty absent item array rejects before index import or read");
    invalid.numiteminfo = 0; Attempt(weightText);
    {
        int *empty = ItemWeightIndex(botgoalstates[handle]->itemweightconfig, &invalid);
        Check(empty != NULL && requests == 1 && !errors && heapLive == baseline + 1,
              "native zero index capacity keeps its ordinary complete empty owner");
        FreeMemory(empty);
    }
    PairValues(handle); WeightEnd();
}

static void InvalidFilename(int reload)
{
    int handle = WeightBegin(reload), baseline = heapLive;
    bot_goalstate_t saved = *botgoalstates[handle];
    Attempt(weightText);
    Check(BotLoadItemWeights(handle, NULL) == BLERR_CANNOTLOADITEMWEIGHTS &&
          !requests && !opens && errors == 2 && heapLive == baseline &&
          !memcmp(&saved, botgoalstates[handle], sizeof(saved)),
          "NULL filename rejects safely without disturbing the working pair");
    PairValues(handle); WeightEnd();
}

static void NativeIndexLayout(void)
{
    int handle = WeightBegin(0), *index;
    itemconfig_t table;
    iteminfo_t items[3];
    memset(items, 0, sizeof(items));
    strcpy(items[0].classname, "weapon_native"); strcpy(items[1].classname, "absent");
    strcpy(items[2].classname, "weapon_native"); table.numiteminfo = 3; table.iteminfo = items;
    Attempt(weightText); index = ItemWeightIndex(botgoalstates[handle]->itemweightconfig, &table);
    Check(index && index[0] == 0 && index[1] == -1 && index[2] == 0 && !errors && requests == 1,
          "native classname order, duplicate mapping and missing-weight sentinel remain");
    FreeMemory(index); PairValues(handle); WeightEnd();
}

static void FirstGolden(int reload)
{
    int handle = WeightBegin(reload);
    PairValues(handle); WeightEnd();
}

#ifndef Q3_GOAL_WEIGHT_NO_MAIN
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
        else { FirstGolden(0); FirstGolden(1); NativeIndexLayout(); }
        return 0;
    }
    FirstGolden(0); FirstGolden(1); NativeIndexLayout(); CacheAlias(); InvalidIndex();
    for (reload = 0; reload < 2; reload++) {
        count = WeightImports(reload); total += count;
        for (position = 1; position <= count; position++) PairNullable(reload, position, count);
        PairMalformed(reload, 0); PairMalformed(reload, 1);
        MissingTable(reload); RepeatedCleanup(reload); InvalidFilename(reload);
    }
    printf("Actual item weight pair, %d nullable stages, cached aliases, prior bytes, evaluation/retry and public physical shutdown pass (issue #48)\n", total);
    return 0;
}

#endif /* Q3_GOAL_WEIGHT_NO_MAIN */
