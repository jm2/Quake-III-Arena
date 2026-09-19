/* Actual public weapon range, state factory, getter and selection guards. */
#define Q3_WEAPON_WEIGHT_NO_MAIN
#include "bot_weapon_weight_regression.c"

typedef struct {
    unsigned int before;
    weaponinfo_t info;
    unsigned int after;
} output_t;

static void Output(output_t *out)
{
    memset(out, 0xa5, sizeof(*out)); out->before = 0x12345678U; out->after = 0x87654321U;
}

static void ApiGolden(int reload)
{
    int handle = WeightBegin(reload);
    bot_weaponstate_t saved = *botweaponstates[handle];
    output_t out;
    Output(&out); Attempt(weightText);
    BotGetWeaponInfo(handle, 1, &out.info);
    Check(!errors && !requests && !memcmp(&out.info, &weaponconfig->weaponinfo[1], sizeof(out.info)) &&
          out.before == 0x12345678U && out.after == 0x87654321U,
          "unchanged native public getter copies every field/padding byte within physical canaries");
    Check(BotValidWeaponNumber(1), "native valid weapon index remains accepted");
    BotResetWeaponState(handle);
    Check(!memcmp(&saved, botweaponstates[handle], sizeof(saved)), "native reset keeps working pair roots");
    PairValues(handle); WeightEnd();
}

static void FactoryNullable(int reload)
{
    int handle = WeightBegin(reload), baseline = heapLive, arena = hunkLive;
    bot_weaponstate_t saved = *botweaponstates[handle], empty;
    Attempt(weightText); failAt = 1;
    Check(BotAllocWeaponState() == 0 && requests == 1 && errors == 1 && !botweaponstates[2] &&
          heapLive == baseline && hunkLive == arena && !memcmp(&saved, botweaponstates[handle], sizeof(saved)),
          "nullable public factory returns zero and preserves every prior state/root/physical owner");
    Attempt(weightText);
    Check(BotAllocWeaponState() == 2 && requests == 1 && !errors && heapLive == baseline + 1,
          "failed public state import retries at the same first free native slot");
    memset(&empty, 0, sizeof(empty));
    Check(!memcmp(&empty, botweaponstates[2], sizeof(empty)), "retry publishes complete cleared native state");
    BotFreeWeaponState(2);
    Check(!botweaponstates[2] && heapLive == baseline, "actual public free releases the retry owner");
    PairValues(handle); WeightEnd();
}

static void Range(int weapon)
{
    int handle = WeightBegin(0);
    output_t out, saved;
    Output(&out); saved = out; Attempt(weightText);
    BotGetWeaponInfo(handle, weapon, &out.info);
    Check(errors == 1 && !requests && !memcmp(&out, &saved, sizeof(out)),
          "invalid public weapon index leaves every destination/canary byte unchanged");
    PairValues(handle); WeightEnd();
}

static void ApiMissingTable(void)
{
    int handle = WeightBegin(0);
    weaponconfig_t *table = weaponconfig;
    output_t out, saved;
    Output(&out); saved = out; Attempt(weightText); weaponconfig = NULL;
    BotGetWeaponInfo(handle, 1, &out.info);
    Check(errors == 1 && !requests && !memcmp(&out, &saved, sizeof(out)),
          "missing public table rejects before dereference and preserves destination bytes");
    weaponconfig = table; PairValues(handle); WeightEnd();
}

static void MissingOutput(void)
{
    int handle = WeightBegin(0);
    bot_weaponstate_t saved = *botweaponstates[handle];
    Attempt(weightText); BotGetWeaponInfo(handle, 1, NULL);
    Check(errors == 1 && !requests && !memcmp(&saved, botweaponstates[handle], sizeof(saved)),
          "missing getter output rejects before write without disturbing the native pair");
    PairValues(handle); WeightEnd();
}

static void SelectionInput(int index)
{
    int handle = WeightBegin(0), inventory[4] = {0};
    int *prior = botweaponstates[handle]->weaponweightindex;
    Attempt(weightText);
    if (index) botweaponstates[handle]->weaponweightindex = NULL;
    Check(BotChooseBestFightWeapon(handle, index ? inventory : NULL) == 0 && !requests && !errors,
          "incomplete public selection input returns no weapon before any index/inventory read");
    botweaponstates[handle]->weaponweightindex = prior; PairValues(handle); WeightEnd();
}

static void InvalidHandle(int handle)
{
    int valid = WeightBegin(0);
    output_t out, saved;
    Output(&out); saved = out; Attempt(weightText);
    BotGetWeaponInfo(handle, 1, &out.info);
    Check(errors == 1 && !requests && !memcmp(&out, &saved, sizeof(out)),
          "ordinary native invalid handle leaves the guarded output unchanged");
    PairValues(valid); WeightEnd();
}

int main(int argc, char **argv)
{
    int proof;
    if (argc > 1) {
        proof = atoi(argv[1]);
        if (proof == 0) FactoryNullable(0);
        else if (proof == 1) Range(32);
        else if (proof == 2) ApiMissingTable();
        else if (proof == 3) MissingOutput();
        else if (proof == 4) SelectionInput(1);
        else if (proof == 5) SelectionInput(0);
        else { ApiGolden(0); ApiGolden(1); }
        return 0;
    }
    ApiGolden(0); ApiGolden(1); FactoryNullable(0); FactoryNullable(1);
    Range(0); Range(-1); Range(32); Range(33); Range(INT_MAX);
    ApiMissingTable(); MissingOutput(); SelectionInput(0); SelectionInput(1);
    InvalidHandle(0); InvalidHandle(-1); InvalidHandle(MAX_CLIENTS + 1);
    puts("Actual public weapon nullable handle/ranges/output/selection, native field bytes and physical ownership pass (issue #48)");
    return 0;
}
