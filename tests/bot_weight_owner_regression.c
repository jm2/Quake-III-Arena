/* A weight config cached in weightFileList may be shared by several weapon (or goal) states
   and is freed only by BotShutdownWeights; a config loaded under bot_reloadcharacters 1 is
   private and is freed with its state. The decision must not follow the current policy. */
#define Q3_WEAPON_WEIGHT_NO_MAIN
#include "bot_weapon_weight_regression.c"

#define MAX_WEIGHT_FILES 128 /* be_ai_weight.c */
extern weightconfig_t *weightFileList[MAX_WEIGHT_FILES];

static int CachedWeights(void)
{
    int i, count = 0;
    for (i = 0; i < MAX_WEIGHT_FILES; i++) if (weightFileList[i]) count++;
    return count;
}

static void OwnerBegin(char *reload)
{
    Begin(); Attempt(weaponText);
    Check(BotSetupWeaponAI() == BLERR_NOERROR, "actual complete weapon table");
    Check(LibVar("bot_reloadcharacters", reload) != NULL, "weight ownership policy");
}

static int State(const char *filename)
{
    int handle = BotAllocWeaponState();
    Check(handle > 0, "actual weapon state");
    Attempt(weightText);
    Check(BotLoadWeaponWeights(handle, (char *)filename) == BLERR_NOERROR, "actual weight config loads");
    return handle;
}

/* Retail default: two states share one cached config; freeing the states keeps it. */
static void SharedGolden(void)
{
    int a, b;
    OwnerBegin("0"); a = State("bots/owner_w.c"); b = State("bots/owner_w.c");
    Check(botweaponstates[a]->weaponweightconfig == botweaponstates[b]->weaponweightconfig &&
          CachedWeights() == 1, "both states share the cached config");
    BotFreeWeaponState(a); PairValues(b);
    BotFreeWeaponState(b); Check(CachedWeights() == 1, "freed states leave the cached config to shutdown");
    WeightEnd();
}

/* Retail bot_reloadcharacters 1: every state loads its own config and frees it. */
static void PrivateGolden(void)
{
    int a, b, live;
    OwnerBegin("1"); live = heapLive; a = State("bots/owner_w.c"); b = State("bots/owner_w.c");
    Check(botweaponstates[a]->weaponweightconfig != botweaponstates[b]->weaponweightconfig &&
          !CachedWeights(), "reload policy loads a private config per state");
    BotFreeWeaponState(a); PairValues(b); BotFreeWeaponState(b);
    Check(heapLive == live, "private configs are freed with their states");
    WeightEnd();
}

/* States sharing a cached config when bot_reloadcharacters becomes 1:
   freeing one must leave the config for the other and for shutdown. */
static void CachedThenReload(void)
{
    int a, b;
    OwnerBegin("0"); a = State("bots/owner_w.c"); b = State("bots/owner_w.c");
    LibVarSet("bot_reloadcharacters", "1");
    BotFreeWeaponState(a); PairValues(b);
    BotFreeWeaponState(b); Check(CachedWeights() == 1, "the cached config is left to shutdown");
    WeightEnd();
}

/* After the flip one state reloads another file: its old, shared, cached config must stay. */
static void SwitchAfterFlip(void)
{
    int a, b;
    OwnerBegin("0"); a = State("bots/owner_w.c"); b = State("bots/owner_w.c");
    LibVarSet("bot_reloadcharacters", "1"); Attempt(weightText);
    Check(BotLoadWeaponWeights(a, "bots/other_w.c") == BLERR_NOERROR && CachedWeights() == 1 &&
          botweaponstates[a]->weaponweightconfig != botweaponstates[b]->weaponweightconfig, "the state reloads a private config");
    PairValues(a); PairValues(b);
    BotFreeWeaponState(a); BotFreeWeaponState(b);
    WeightEnd();
}

/* A private config loaded under 1 is still freed if the policy is now 0. */
static void PrivateThenCached(void)
{
    int a, live;
    OwnerBegin("1"); live = heapLive; a = State("bots/owner_w.c");
    LibVarSet("bot_reloadcharacters", "0"); BotFreeWeaponState(a);
    Check(heapLive == live, "the private config is freed with its state");
    WeightEnd();
}

/* Reloading under 0 a state that holds a private config replaces and frees the private one. */
static void ReloadAfterFlip(void)
{
    int a;
    weightconfig_t *prior;
    OwnerBegin("1"); a = State("bots/owner_w.c"); prior = botweaponstates[a]->weaponweightconfig;
    LibVarSet("bot_reloadcharacters", "0"); Attempt(weightText);
    Check(BotLoadWeaponWeights(a, "bots/owner_w.c") == BLERR_NOERROR && CachedWeights() == 1 &&
          botweaponstates[a]->weaponweightconfig != prior, "the state reloads the cached config");
    PairValues(a);
    BotFreeWeaponState(a); Check(CachedWeights() == 1, "the cached config is left to shutdown");
    WeightEnd(); /* fails if the replaced private config leaked */
}

int main(int argc, char **argv)
{
    static void (*const cases[])(void) = { SharedGolden, PrivateGolden, CachedThenReload,
        SwitchAfterFlip, PrivateThenCached, ReloadAfterFlip };
    int i;
    if (argc > 1) { cases[atoi(argv[1])](); return 0; }
    for (i = 0; i < (int)(sizeof(cases) / sizeof(cases[0])); i++) cases[i]();
    puts("Shared cached weight configs survive policy changes and reloads; private configs are freed (issue #48)");
    return 0;
}
