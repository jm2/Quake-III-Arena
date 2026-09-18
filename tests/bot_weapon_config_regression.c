/* Actual weapon factory, parser, native fields and physical engine owners. */
#define Q3_ITEM_OWNER_ONLY
#include "bot_item_config_regression.c"

static const char *weaponText =
    "projectileinfo { name \"NativeShot\" model \"models/shot.md3\" flags 117 gravity 0.5 damage 25 radius 96 }\n"
    "weaponinfo { number 1 name \"NativeWeapon\" model \"models/weapon.md3\" projectile \"NativeShot\" numprojectiles 2 speed 900 ammoamount 3 reload 0.75 }\n";

static void Values(weaponconfig_t *config, int capacity)
{
    weaponinfo_t *weapon;
    projectileinfo_t *projectile;
    Check(config && config->numweapons == capacity && config->numprojectiles == 1,
          "complete native weapon/projectile capacities");
    Check(config->weaponinfo == (weaponinfo_t *)((char *)config + sizeof(*config)) &&
          config->projectileinfo == (projectileinfo_t *)(config->weaponinfo + capacity),
          "both native inline arrays rebase into persistent result");
    weapon = &config->weaponinfo[1];
    projectile = &config->projectileinfo[0];
    Check(!config->weaponinfo[0].valid && weapon->valid && weapon->number == 1 &&
          !strcmp(weapon->name, "NativeWeapon") && !strcmp(weapon->model, "models/weapon.md3") &&
          !strcmp(weapon->projectile, "NativeShot") && weapon->numprojectiles == 2 &&
          weapon->speed == 900 && weapon->ammoamount == 3 && weapon->reload == 0.75f,
          "native weapon fields and unused cleared slot survive publication");
    Check(!strcmp(projectile->name, "NativeShot") && !strcmp(projectile->model, "models/shot.md3") &&
          projectile->flags == 117 && projectile->gravity == 0.5f && projectile->damage == 25 &&
          projectile->radius == 96 && !memcmp(&weapon->proj, projectile, sizeof(*projectile)),
          "native projectile fields and actual weapon fixup survive staging copy");
}

static void Golden(void)
{
    weaponconfig_t *config;
    Begin(); Attempt(weaponText);
    config = LoadWeaponConfig("native.c"); Values(config, 32);
    Check(opens == 1 && closes == 1 && !errors && !warnings && messages == 1 &&
          heapLive == 4 && hunkLive == 1 && !numtokens,
          "native defaults keep only two complete caches and one physical hunk");
    FreeMemory(config); End();
}

static void Malformed(const char *text)
{
    Begin(); Attempt(text);
    Check(!LoadWeaponConfig("native.c") && opens == 1 && closes == 1 && errors > 0 &&
          !messages && heapLive == 4 && !hunkLive && !numtokens,
          "malformed file rejects with all parser/staging owners freed and no physical hunk");
    End();
}

static void Nullable(int position)
{
    weaponconfig_t *config;
    Begin(); Attempt(weaponText); failAt = position;
    Check(!LoadWeaponConfig("native.c") && requests >= failAt && errors > 0 &&
          !messages && !hunkLive && !numtokens && opens == closes,
          "every nullable native import rejects without persistent arena consumption");
    Check(heapLive == (position <= 2 ? 0 : position <= 4 ? 2 : 4),
          "only complete count caches survive a failed import");
    Attempt(weaponText); config = LoadWeaponConfig("native.c"); Values(config, 32);
    FreeMemory(config); End();
}

static float Opaque(unsigned int bits)
{
    volatile unsigned int representation = bits;
    unsigned int copy = representation;
    float value;
    memcpy(&value, &copy, sizeof(value));
    return value;
}

static void InvalidCount(int projectile, unsigned int bits)
{
    int imports;
    libvar_t *variable;
    Begin(); Attempt(weaponText);
    Check(LibVar("max_weaponinfo", "32") && LibVar("max_projectileinfo", "32"),
          "complete native count caches prepare");
    variable = LibVarGet(projectile ? "max_projectileinfo" : "max_weaponinfo");
    variable->value = Opaque(bits); imports = requests;
    Check(!LoadWeaponConfig("native.c") && requests == imports && !opens && !hunkLive &&
          heapLive == 4 && errors == 1,
          "nonfinite, unrepresentable or oversized count stops before conversion/VFS/arena");
    End();
}

static void CombinedCost(void)
{
    int imports;
    float count = (float)((unsigned long)INT_MAX /
        (sizeof(weaponinfo_t) + sizeof(projectileinfo_t)) + 256);
    libvar_t *weapons, *projectiles;
    Begin(); Attempt(weaponText);
    weapons = LibVar("max_weaponinfo", "32");
    projectiles = LibVar("max_projectileinfo", "32");
    Check(weapons && projectiles, "combined native count caches prepare");
    weapons->value = projectiles->value = count; imports = requests;
    Check((unsigned long)(int)count * sizeof(weaponinfo_t) + sizeof(weaponconfig_t) < INT_MAX &&
          (unsigned long)(int)count * sizeof(projectileinfo_t) < INT_MAX,
          "each array fits separately in the native signed request");
    Check(!LoadWeaponConfig("native.c") && requests == imports && !opens && !hunkLive &&
          heapLive == 4 && errors == 1, "combined array sum rejects before imports");
    End();
}

static void ProjectileCapacity(void)
{
    Begin(); Attempt("projectileinfo { name \"NativeShot\" }");
    Check(LibVar("max_projectileinfo", "0") != NULL, "native zero projectile cache prepares");
    Check(!LoadWeaponConfig("native.c") && errors == 1 && !messages && heapLive == 4 &&
          !hunkLive && !numtokens && opens == closes,
          "zero projectile capacity rejects a definition without consuming persistent arena");
    End();
}

static void InvalidPath(int kind)
{
    char path[MAX_PATH + 1];
    Begin(); memset(path, 'x', sizeof(path)); path[sizeof(path) - 1] = 0;
    Check(!LoadWeaponConfig(kind == 0 ? NULL : kind == 1 ? "" : path) && !requests &&
          !opens && !heapLive && !hunkLive && errors == 1,
          "invalid full filename rejects before any cache, VFS or arena import");
    End();
}

static void NativeCounts(void)
{
    weaponconfig_t *config;
    libvar_t *weapons, *projectiles;
    int i;
    for (i = 0; i < 4; i++) {
        Begin(); Attempt(i == 1 || i == 2 ? "" : weaponText);
        weapons = LibVar("max_weaponinfo", "32");
        projectiles = LibVar("max_projectileinfo", "32");
        Check(weapons && projectiles, "native caches prepare");
        weapons->value = projectiles->value = Opaque(i == 0 ? 0x40300000U :
            i == 1 ? 0x3f000000U : i == 2 ? 0xbf000000U : 0xbf800000U);
        config = LoadWeaponConfig("native.c");
        if (i == 1 || i == 2)
            Check(config && !config->numweapons && !config->numprojectiles && warnings == 1 &&
                  !errors, "native zero capacity and truncated negative fraction accept empty file");
        else Values(config, i == 0 ? 2 : 32);
        Check(errors == (i == 3 ? 2 : 0) && weapons->value ==
              (i == 0 ? 2.75f : i == 1 ? 0.5f : i == 2 ? -0.5f : 32.0f) &&
              projectiles->value == weapons->value,
              "both native fractional counts and negative fallback cache values remain");
        FreeMemory(config); End();
    }
}

static void FallbackFailure(int projectile)
{
    libvar_t *variable;
    char *prior;
    weaponconfig_t *config;
    Begin(); Attempt(weaponText);
    Check(LibVar("max_weaponinfo", "32") && LibVar("max_projectileinfo", "32"),
          "native fallback caches prepare");
    variable = LibVarGet(projectile ? "max_projectileinfo" : "max_weaponinfo");
    variable->value = Opaque(0xbf800000U); prior = variable->string; failAt = requests + 1;
    Check(!LoadWeaponConfig("native.c") && requests == failAt && !opens && !hunkLive &&
          heapLive == 4 && variable->string == prior && variable->value == -1.0f,
          "failed native fallback string update preserves its previous owner and stops");
    Attempt(weaponText); config = LoadWeaponConfig("native.c"); Values(config, 32);
    Check(variable->value == 32.0f, "fallback update retries successfully");
    FreeMemory(config); End();
}

static void PriorFailure(void)
{
    weaponconfig_t *prior, header;
    weaponinfo_t savedWeapon;
    projectileinfo_t savedProjectile;
    Begin(); Attempt(weaponText); prior = LoadWeaponConfig("native.c"); Values(prior, 32);
    header = *prior; savedWeapon = prior->weaponinfo[1]; savedProjectile = prior->projectileinfo[0];
    Attempt("weaponinfo");
    Check(!LoadWeaponConfig("failed.c") && hunkLive == 1 && heapLive == 4 && !numtokens &&
          !memcmp(&header, prior, sizeof(header)) &&
          !memcmp(&savedWeapon, &prior->weaponinfo[1], sizeof(savedWeapon)) &&
          !memcmp(&savedProjectile, &prior->projectileinfo[0], sizeof(savedProjectile)),
          "failed distinct configuration keeps prior header, arrays, fixups and physical arena");
    FreeMemory(prior); End();
}

int main(int argc, char **argv)
{
    weaponconfig_t *config;
    int i, j, count;
    unsigned int invalid[] = {0x7f800000U, 0xff800000U, 0x7fc00000U,
        0x4f000000U, 0xcf800000U, 0x4b800000U};
    char text[2048];
    if (argc > 1) {
        i = atoi(argv[1]);
        if (i == 0) Malformed("weaponinfo");
        else if (i == 1) { snprintf(text, sizeof(text), "%s#unknown", weaponText); Malformed(text); }
        else if (i == 2) InvalidCount(0, 0x7fc00000U);
        else if (i == 3) InvalidCount(1, 0x4b800000U);
        else if (i == 4) InvalidPath(0);
        else if (i == 5) InvalidPath(2);
        else { Golden(); NativeCounts(); }
        return 0;
    }
    Golden(); NativeCounts(); CombinedCost(); ProjectileCapacity(); FallbackFailure(0); FallbackFailure(1); PriorFailure();
    Malformed("weaponinfo"); Malformed("projectileinfo { unknown 1 }");
    Malformed("weaponinfo { number 32 name \"Bad\" }");
    Malformed("weaponinfo { number 1 projectile \"NativeShot\" }");
    Malformed("weaponinfo { number 1 name \"Bad\" }");
    Malformed("weaponinfo { number 1 name \"Bad\" projectile \"Missing\" }");
    Malformed("garbage");
    snprintf(text, sizeof(text), "%s#unknown", weaponText); Malformed(text);
    snprintf(text, sizeof(text), "%s\"\\q\"", weaponText); Malformed(text);
    for (i = 0; i < 3; i++) InvalidPath(i);
    for (i = 0; i < 2; i++) for (j = 0; j < 6; j++) InvalidCount(i, invalid[j]);
    Begin(); Attempt(weaponText); config = LoadWeaponConfig("native.c"); Values(config, 32);
    count = requests; FreeMemory(config); End();
    for (i = 1; i <= count; i++) Nullable(i);
    printf("Actual weapon fields/fixups, native counts, %d nullable imports, source rollback and physical ownership pass (issue #48)\n", count);
    return 0;
}
