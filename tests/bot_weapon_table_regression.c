/* Actual complete weapon-table and live weight-index replacement. */
#define Q3_WEAPON_WEIGHT_NO_MAIN
#include "bot_weapon_weight_regression.c"
static const char *tableText =
    "projectileinfo { name \"NativeShot\" model \"models/shot.md3\" flags 117 gravity 0.5 damage 25 radius 96 }\n"
    "weaponinfo { number 4 name \"NativeWeapon\" model \"models/weapon.md3\" projectile \"NativeShot\" numprojectiles 2 speed 900 ammoamount 3 reload 0.75 }\n";

static size_t TableSize(int capacity)
{
    return sizeof(weaponconfig_t) + (size_t)capacity * sizeof(weaponinfo_t) + 32 * sizeof(projectileinfo_t);
}

static int TableBegin(int reload, int small)
{
    int i, handle;
    Begin(); Check(!weaponconfig, "prior live table releases");
    Check(LibVar("max_weaponinfo", small ? "2.75" : "32") != NULL, "native prior capacity cache");
    Attempt(weaponText); Check(BotSetupWeaponAI() == BLERR_NOERROR, "native prior table setup");
    Values(weaponconfig, small ? 2 : 32);
    Check(LibVar("bot_reloadcharacters", reload ? "1" : "0") != NULL, "native live weight ownership");
    for (i = 1; i <= 3; i++) {
        handle = BotAllocWeaponState(); Check(handle == i && botweaponstates[i], "three actual native states");
        if (i < 3) {
            Attempt(weightText); Check(BotLoadWeaponWeights(handle, "prior.w") == BLERR_NOERROR,
                  "two live native pairs share or own actual weight trees");
            PairValues(handle);
        }
    }
    LibVarSet("max_weaponinfo", "8.75"); Attempt(tableText);
    return small ? 2 : 32;
}

static void TableEnd(void)
{
    WeightEnd(); Check(!botweaponstates[2] && !botweaponstates[3], "public shutdown releases every live/empty state");
}

static void NewValues(void)
{
    int i, j, inventory[4] = {0};
    Check(weaponconfig && weaponconfig->numweapons == 8 && weaponconfig->numprojectiles == 1 &&
          weaponconfig->weaponinfo == (weaponinfo_t *)((char *)weaponconfig + sizeof(*weaponconfig)) &&
          weaponconfig->projectileinfo == (projectileinfo_t *)(weaponconfig->weaponinfo + 8) &&
          weaponconfig->weaponinfo[4].valid && !weaponconfig->weaponinfo[1].valid &&
          !strcmp(weaponconfig->weaponinfo[4].name, "NativeWeapon") &&
          !memcmp(&weaponconfig->weaponinfo[4].proj, &weaponconfig->projectileinfo[0], sizeof(projectileinfo_t)),
          "complete new table keeps native arrays/fields/projectile fixup at moved weapon");
    for (i = 1; i < 3; i++) {
        Check(botweaponstates[i]->weaponweightconfig && botweaponstates[i]->weaponweightindex,
              "both live pairs receive complete replacement indices");
        for (j = 0; j < 8; j++) Check(botweaponstates[i]->weaponweightindex[j] == (j == 4 ? 0 : -1),
              "all resized index entries use the new table name mapping");
        Check(BotChooseBestFightWeapon(i, inventory) == 4, "actual evaluation selects the moved weapon");
    }
    Check(!botweaponstates[3]->weaponweightconfig && !botweaponstates[3]->weaponweightindex,
          "empty native state remains empty across table replacement");
}

static int TableImports(int reload, int small)
{
    int count;
    TableBegin(reload, small); Check(BotSetupWeaponAI() == BLERR_NOERROR, "measure actual full live-table imports");
    NewValues(); count = requests; TableEnd(); return count;
}

static void TableFailure(int reload, int small, int position, int suffix)
{
    int capacity = TableBegin(reload, small), baseline = heapLive, arena = hunkLive, i;
    weaponconfig_t *prior = weaponconfig;
    bot_weaponstate_t states[3];
    weightconfig_t weights[2];
    fuzzyseperator_t trees[2];
    int indices[2][32];
    unsigned char *saved = malloc(TableSize(capacity));
    char text[2048];
    Check(saved != NULL, "physical complete prior table snapshot"); memcpy(saved, prior, TableSize(capacity));
    for (i = 0; i < 3; i++) states[i] = *botweaponstates[i + 1];
    for (i = 0; i < 2; i++) {
        weights[i] = *states[i].weaponweightconfig;
        trees[i] = *weights[i].weights[0].firstseperator;
        memcpy(indices[i], states[i].weaponweightindex, (size_t)capacity * sizeof(int));
    }
    if (position) failAt = position;
    else if (suffix) { snprintf(text, sizeof(text), "%s#unknown", tableText); Attempt(text); }
    else Attempt("weaponinfo");
    Check(BotSetupWeaponAI() == BLERR_CANNOTLOADWEAPONCONFIG && weaponconfig == prior &&
          errors > 0 && heapLive == baseline && hunkLive == arena && !numtokens && opens == closes &&
          (!position || requests >= failAt),
          "failed live replacement frees private owners and retains complete prior table without extra physical hunk");
    Check(!memcmp(prior, saved, TableSize(capacity)), "every prior table/header/array/fixup/unused byte survives");
    for (i = 0; i < 3; i++) Check(!memcmp(botweaponstates[i + 1], &states[i], sizeof(states[i])),
          "all prior live and empty state roots survive failure");
    for (i = 0; i < 2; i++) {
        Check(!memcmp(states[i].weaponweightconfig, &weights[i], sizeof(weights[i])) &&
              !memcmp(weights[i].weights[0].firstseperator, &trees[i], sizeof(trees[i])) &&
              !memcmp(states[i].weaponweightindex, indices[i], (size_t)capacity * sizeof(int)),
              "every existing weight/tree/index byte survives failure");
        PairValues(i + 1);
    }
    free(saved); Attempt(tableText);
    Check(BotSetupWeaponAI() == BLERR_NOERROR && weaponconfig != prior && heapLive == baseline &&
          hunkLive == arena + 1 && !errors && !warnings && messages == 1 && opens == closes,
          "failed live replacement retries with one complete persistent owner");
    for (i = 0; i < 2; i++) Check(botweaponstates[i + 1]->weaponweightconfig == states[i].weaponweightconfig &&
          botweaponstates[i + 1]->weaponweightindex != states[i].weaponweightindex,
          "native weight owners stay while complete indices replace logically released old owners");
    NewValues(); TableEnd();
}

static void TableGolden(int reload, int small)
{
    int baseline, arena, i;
    weightconfig_t *weights[2];
    TableBegin(reload, small); baseline = heapLive; arena = hunkLive;
    for (i = 0; i < 2; i++) weights[i] = botweaponstates[i + 1]->weaponweightconfig;
    Check(BotSetupWeaponAI() == BLERR_NOERROR && heapLive == baseline && hunkLive == arena + 1 &&
          !errors && !warnings && messages == 1 && opens == closes && !numtokens,
          "native live capacity growth/shrink publishes complete table and indices");
    for (i = 0; i < 2; i++) Check(botweaponstates[i + 1]->weaponweightconfig == weights[i],
          "cached or private native weight ownership remains unchanged");
    NewValues(); TableEnd();
}

static void PublicGrowth(int reload)
{
    int inventory[4] = {0};
    TableBegin(reload, 1);
    Check(BotSetupWeaponAI() == BLERR_NOERROR, "public growth candidate publishes");
    Check(BotChooseBestFightWeapon(1, inventory) == 4 && BotChooseBestFightWeapon(2, inventory) == 4,
          "direct public evaluation stays inside rebuilt arrays after capacity growth");
    TableEnd();
}

static void EmptyTable(int reload)
{
    int inventory[4] = {0}, baseline, i;
    weightconfig_t *weights[2];
    TableBegin(reload, 0); baseline = heapLive;
    for (i = 0; i < 2; i++) weights[i] = botweaponstates[i + 1]->weaponweightconfig;
    LibVarSet("max_weaponinfo", "0.5"); LibVarSet("max_projectileinfo", "0.5"); Attempt("");
    Check(BotSetupWeaponAI() == BLERR_NOERROR && !weaponconfig->numweapons &&
          !weaponconfig->numprojectiles && heapLive == baseline && hunkLive == 2 &&
          !errors && warnings == 1 && messages == 1 && !numtokens,
          "native empty zero-capacity table remains valid across live replacement");
    for (i = 0; i < 2; i++) Check(botweaponstates[i + 1]->weaponweightconfig == weights[i] &&
          botweaponstates[i + 1]->weaponweightindex && BotChooseBestFightWeapon(i + 1, inventory) == 0,
          "native zero-size indices retain weight owners and select no weapon");
    TableEnd();
}

static void InvalidLiveCount(int reload, int kind)
{
    int i;
    weaponconfig_t *prior;
    bot_weaponstate_t states[3];
    libvar_t *variable;
    TableBegin(reload, 0); prior = weaponconfig;
    for (i = 0; i < 3; i++) states[i] = *botweaponstates[i + 1];
    variable = LibVarGet(kind % 2 ? "max_projectileinfo" : "max_weaponinfo");
    variable->value = Opaque(kind < 2 ? 0x7fc00000U : 0x4b800000U);
    Check(BotSetupWeaponAI() == BLERR_CANNOTLOADWEAPONCONFIG && !requests && !opens &&
          errors == 1 && weaponconfig == prior && hunkLive == 1,
          "invalid live count/cost rejects before private index imports or persistent arena");
    for (i = 0; i < 3; i++) Check(!memcmp(botweaponstates[i + 1], &states[i], sizeof(states[i])),
          "invalid live count keeps all previous pair roots");
    PairValues(1); PairValues(2); TableEnd();
}

static void OriginalOrdinaryGolden(int reload)
{
    int handle = WeightBegin(reload);
    PairValues(handle); WeightEnd();
}

int main(int argc, char **argv)
{
    int reload, small, position, count, kind, total = 0;
    if (argc > 1) {
        position = atoi(argv[1]);
        if (position < 4) TableGolden(position / 2, position % 2);
        else if (position == 4) PublicGrowth(0);
        else { OriginalOrdinaryGolden(0); OriginalOrdinaryGolden(1); }
        return 0;
    }
    PublicGrowth(0); PublicGrowth(1); EmptyTable(0); EmptyTable(1);
    for (reload = 0; reload < 2; reload++) for (kind = 0; kind < 4; kind++) InvalidLiveCount(reload, kind);
    for (reload = 0; reload < 2; reload++) for (small = 0; small < 2; small++) {
        TableGolden(reload, small); count = TableImports(reload, small); total += count;
        for (position = 1; position <= count; position++) TableFailure(reload, small, position, 0);
        TableFailure(reload, small, 0, 0); TableFailure(reload, small, 0, 1);
    }
    printf("Actual live weapon table/indices, %d nullable stages, capacity growth/shrink, prior bytes and physical rollback/publication pass (issue #48)\n", total);
    return 0;
}
