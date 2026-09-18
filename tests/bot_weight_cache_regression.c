/* Actual shared/private weight ownership across public goal/weapon policies. */
#ifdef Q3_GOAL_STATE
#define Q3_GOAL_WEIGHT_NO_MAIN
#include "bot_goal_weight_regression.c"
#define state_t bot_goalstate_t
#define states botgoalstates
#define StateLoad BotLoadItemWeights
#define StateFreeWeights BotFreeItemWeights
#define StateFree BotFreeGoalState
#define StateWeights(s) ((s)->itemweightconfig)
#define StateIndex(s) ((s)->itemweightindex)
static int StateAlloc(void) { return BotAllocGoalState(17); }
#else
#define Q3_WEAPON_WEIGHT_NO_MAIN
#include "bot_weapon_weight_regression.c"
#define state_t bot_weaponstate_t
#define states botweaponstates
#define StateLoad BotLoadWeaponWeights
#define StateFreeWeights BotFreeWeaponWeights
#define StateFree BotFreeWeaponState
#define StateWeights(s) ((s)->weaponweightconfig)
#define StateIndex(s) ((s)->weaponweightindex)
static int StateAlloc(void) { return BotAllocWeaponState(); }
#endif
extern weightconfig_t *weightFileList[];
/* Each fixture starts with an empty cache and caches only prior.w in slot zero. */
static int Cached(weightconfig_t *config) { return weightFileList[0] == config; }

static void PrivatePolicy(void)
{
    int handle = WeightBegin(1), baseline = heapLive;
    weightconfig_t *config = StateWeights(states[handle]);
    Check(!Cached(config), "reload weight is physically private");
    LibVarSet("bot_reloadcharacters", "0"); Attempt(weightText);
    StateFreeWeights(handle);
    Check(!StateWeights(states[handle]) && !StateIndex(states[handle]) && !requests &&
          heapLive == baseline - 4, "public cleanup frees private tree/name/header/index despite changed cache policy");
    StateFreeWeights(handle); WeightEnd();
}

static void CachedPolicy(void)
{
    int handle = WeightBegin(0), second = StateAlloc(), baseline;
    weightconfig_t *config = StateWeights(states[handle]);
    weightconfig_t header = *config;
    fuzzyseperator_t tree = *header.weights[0].firstseperator;
    state_t saved;
    Check(second == 2 && Cached(config), "native second state and shared cache owner");
    Attempt(weightText); Check(StateLoad(second, "prior.w") == BLERR_NOERROR,
          "actual public cached lookup attaches a second complete pair");
    Check(StateWeights(states[second]) == config, "both native states share the same cached weight");
    saved = *states[second]; baseline = heapLive;
    LibVarSet("bot_reloadcharacters", "1"); Attempt(weightText); StateFreeWeights(handle);
    Check(heapLive == baseline - 1 && !StateWeights(states[handle]) && !StateIndex(states[handle]) &&
          Cached(config) && !memcmp(config, &header, sizeof(header)) &&
          !memcmp(header.weights[0].firstseperator, &tree, sizeof(tree)) &&
          !memcmp(states[second], &saved, sizeof(saved)),
          "changed reload policy detaches only one index and preserves all shared cached bytes/roots");
    PairValues(second); StateFree(second); WeightEnd();
}

static void ReplacementPolicy(void)
{
    int handle = WeightBegin(0), second = StateAlloc(), baseline;
    weightconfig_t *cached = StateWeights(states[handle]), *private;
    state_t sibling;
    Check(second == 2, "native replacement sibling state");
    Attempt(weightText); Check(StateLoad(second, "prior.w") == BLERR_NOERROR, "native sibling cached pair");
    sibling = *states[second]; baseline = heapLive;
    LibVarSet("bot_reloadcharacters", "1"); Attempt(weightText);
    Check(StateLoad(handle, "private.w") == BLERR_NOERROR &&
          StateWeights(states[handle]) != cached && Cached(cached) && heapLive == baseline + 3 &&
          !memcmp(states[second], &sibling, sizeof(sibling)),
          "cached-to-private replacement preserves sibling roots and retains the native cache owner");
    private = StateWeights(states[handle]); Check(!Cached(private), "replacement weight remains private");
    PairValues(handle); PairValues(second);
    LibVarSet("bot_reloadcharacters", "0"); Attempt(weightText);
    Check(StateLoad(handle, "prior.w") == BLERR_NOERROR && StateWeights(states[handle]) == cached &&
          heapLive == baseline && !opens && requests == 1,
          "private-to-cached alias replacement physically frees private tree/name/header and reuses cached data");
    PairValues(handle); PairValues(second); StateFree(second); WeightEnd();
}

static void DirectPrivate(void)
{
    int handle = WeightBegin(1), baseline = heapLive;
    weightconfig_t *config = StateWeights(states[handle]);
    LibVarSet("bot_reloadcharacters", "0"); FreeWeightConfig(config);
    Check(heapLive == baseline - 3, "direct cleanup follows actual private owner despite current policy");
    StateWeights(states[handle]) = NULL; StateFreeWeights(handle); WeightEnd();
}

static void DirectCached(void)
{
    int handle = WeightBegin(0), baseline = heapLive;
    weightconfig_t *config = StateWeights(states[handle]);
    LibVarSet("bot_reloadcharacters", "1"); Attempt(weightText); FreeWeightConfig(config);
    Check(heapLive == baseline && Cached(config) && !requests,
          "direct cleanup retains actual native cache owner despite current policy");
    PairValues(handle); WeightEnd();
}

static void CacheGolden(void)
{
    int handle, baseline;
    FirstGolden(0); FirstGolden(1);
    handle = WeightBegin(0); baseline = heapLive; FreeWeightConfig(StateWeights(states[handle]));
    Check(heapLive == baseline, "ordinary cached release retains native cache owner"); PairValues(handle); WeightEnd();
    handle = WeightBegin(1); baseline = heapLive; StateFreeWeights(handle);
    Check(heapLive == baseline - 4, "ordinary reload cleanup frees every private pair owner"); WeightEnd();
}

int main(int argc, char **argv)
{
    if (argc > 1) {
        switch (atoi(argv[1])) { case 0: PrivatePolicy(); break; case 1: CachedPolicy(); break;
        case 2: ReplacementPolicy(); break; case 3: DirectPrivate(); break; case 4: DirectCached(); break;
        default: CacheGolden(); break; } return 0;
    }
    CacheGolden(); PrivatePolicy(); CachedPolicy(); ReplacementPolicy(); DirectPrivate(); DirectCached();
    Begin(); FreeWeightConfig(NULL); Check(!requests && !heapLive && !errors, "missing private weight owner is a no-op"); End();
    puts("Actual goal/weapon cache ownership, policy transitions, sibling bytes, native aliases and physical cleanup pass (issue #48)");
    return 0;
}
