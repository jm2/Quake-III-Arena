/* Actual weapon setup and full factory/parser/native owner bodies. */
#define main WeaponConfigFixtureMain
#include "bot_weapon_config_regression.c"
#undef main
#ifdef MEMORYMANEGER
extern int numblocks, allocatedmemory, totalmemorysize;
#endif

static void SetupBegin(int occupied)
{
    Begin(); Check(!weaponconfig, "prior shared weapon root releases");
    Attempt(weaponText);
    if (occupied) {
        weaponconfig = LoadWeaponConfig("prior.c"); Values(weaponconfig, 32);
        Attempt(weaponText);
    }
}

static void SetupEnd(void)
{
    if (weaponconfig) FreeMemory(weaponconfig);
    weaponconfig = NULL; End();
#ifdef MEMORYMANEGER
    Check(!numblocks && !allocatedmemory && !totalmemorysize,
          "complete native tracked weapon owners release logically");
#endif
}

static size_t PriorSize(void)
{
    return sizeof(weaponconfig_t) + 32 * sizeof(weaponinfo_t) + 32 * sizeof(projectileinfo_t);
}

static void SetupNullable(int occupied, int position)
{
    weaponconfig_t *prior;
    unsigned char *saved = NULL;
    int baseline, arena, expected;
    SetupBegin(occupied); prior = weaponconfig; baseline = heapLive; arena = hunkLive;
    if (prior) { saved = malloc(PriorSize()); Check(saved != NULL, "physical prior snapshot"); memcpy(saved, prior, PriorSize()); }
    failAt = position;
    Check(BotSetupWeaponAI() == BLERR_CANNOTLOADWEAPONCONFIG && weaponconfig == prior &&
          requests >= failAt && errors > 0 && !messages && hunkLive == arena &&
          !numtokens && opens == closes,
          "every nullable setup import retains prior root and consumes no additional physical hunk");
    expected = position <= 2 ? 0 : occupied ? 2 : position <= 4 ? 2 : position <= 6 ? 4 : 6;
    Check(heapLive == baseline + expected, "only fully initialized shared variables survive failure");
    if (prior) Check(!memcmp(saved, prior, PriorSize()), "all prior header/weapon/projectile/unused/fixup bytes survive");
    free(saved); Attempt(weaponText);
    Check(BotSetupWeaponAI() == BLERR_NOERROR && weaponconfig != prior && hunkLive == arena + 1,
          "failed setup retries to a complete distinct native owner");
    Values(weaponconfig, 32); SetupEnd();
}

static void SetupMalformed(int occupied, int suffix)
{
    weaponconfig_t *prior;
    unsigned char *saved = NULL;
    int arena;
    char text[2048];
    SetupBegin(occupied); prior = weaponconfig; arena = hunkLive;
    if (prior) { saved = malloc(PriorSize()); Check(saved != NULL, "physical malformed prior snapshot"); memcpy(saved, prior, PriorSize()); }
    if (suffix) { snprintf(text, sizeof(text), "%s#unknown", weaponText); Attempt(text); }
    else Attempt("weaponinfo");
    Check(BotSetupWeaponAI() == BLERR_CANNOTLOADWEAPONCONFIG && weaponconfig == prior &&
          errors > 0 && !messages && heapLive == 6 && hunkLive == arena && !numtokens && opens == closes,
          "malformed setup keeps prior root, frees private source/staging and consumes no extra arena");
    if (prior) Check(!memcmp(saved, prior, PriorSize()), "malformed setup preserves complete prior payload");
    free(saved); Attempt(weaponText);
    Check(BotSetupWeaponAI() == BLERR_NOERROR && weaponconfig != prior && hunkLive == arena + 1,
          "malformed setup retries successfully");
    Values(weaponconfig, 32); SetupEnd();
}

static void FirstGolden(void)
{
    SetupBegin(0);
    Check(BotSetupWeaponAI() == BLERR_NOERROR && !errors && !warnings && messages == 1 &&
          heapLive == 6 && hunkLive == 1 && opens == 1 && closes == 1 && !numtokens &&
          !strcmp(LibVarGetString("weaponconfig"), "weapons.c"),
          "unchanged native first setup/default configuration golden");
    Values(weaponconfig, 32); SetupEnd();
}

static void ReplacementGolden(void)
{
    weaponconfig_t *prior;
    SetupBegin(0); Check(BotSetupWeaponAI() == BLERR_NOERROR, "native initial setup");
    prior = weaponconfig;
    LibVarSet("weaponconfig", "configured.c");
    LibVarSet("max_weaponinfo", "2.75");
    LibVarSet("max_projectileinfo", "2.75");
    Attempt(weaponText);
    Check(BotSetupWeaponAI() == BLERR_NOERROR && weaponconfig != prior && heapLive == 6 &&
          hunkLive == 2 && !errors && !warnings && messages == 1 && opens == closes && !numtokens,
          "complete configured replacement retains native fractional behavior and physical arena ownership");
    Values(weaponconfig, 2); SetupEnd();
}

static int SetupImports(int occupied)
{
    int count;
    SetupBegin(occupied); Check(BotSetupWeaponAI() == BLERR_NOERROR, "measure actual native setup imports");
    Values(weaponconfig, 32); count = requests; SetupEnd(); return count;
}

int main(int argc, char **argv)
{
    int occupied, position, count, total = 0;
    if (argc > 1) {
        position = atoi(argv[1]);
        if (position < 2) SetupNullable(1, position + 1);
        else if (position < 4) SetupMalformed(1, position - 2);
        else if (position == 4) FirstGolden();
        else ReplacementGolden();
        return 0;
    }
    FirstGolden(); ReplacementGolden();
    for (occupied = 0; occupied < 2; occupied++) {
        count = SetupImports(occupied); total += count;
        for (position = 1; position <= count; position++) SetupNullable(occupied, position);
        SetupMalformed(occupied, 0); SetupMalformed(occupied, 1);
    }
    printf("Actual weapon setup, %d nullable stages, complete prior payload, parser retries and native publication ownership pass (issue #48)\n", total);
    return 0;
}
