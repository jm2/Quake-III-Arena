/* A character handle handed out through the cache may be shared by several bots and is
   freed only by BotShutdownCharacters; a character loaded under bot_reloadcharacters 1 is
   private and is freed with its bot. The decision must not follow the current policy. */
#define main CharacterFixtureMain
#include "bot_character_regression.c"
#undef main

static const char *ownerText =
    "skill 1 { 0 1 1 0.25 79 \"native\" }\n"
    "skill 4 { 0 42 1 1.25 79 \"native\" }\n"
    "skill 5 { 0 50 1 1.5 79 \"native\" }\n";

static void OwnerBegin(char *reload)
{
    Reset(ownerText); LibVarSet("bot_reloadcharacters", reload);
}

static void OwnerEnd(void)
{
    BotShutdownCharacters(); LibVarDeAllocAll();
    Check(!liveOwners && !numtokens && !errors, "shutdown frees every character exactly once without errors");
}

/* Reads through the character, so a freed or foreign handle fails here. */
static void Live(int handle, const char *message)
{
    char text[16];
    Check(handle > 0 && handle <= MAX_CLIENTS && botcharacters[handle] != NULL, message);
    Characteristic_String(handle, 79, text, sizeof(text));
    Check(!strcmp(text, "native") && !errors, message);
}

/* Retail default: two bots share one cached character; freeing the bots keeps it. */
static void SharedGolden(void)
{
    int a, b;
    OwnerBegin("0"); a = BotLoadCharacter("bots/owner_c.c", 4); b = BotLoadCharacter("bots/owner_c.c", 4);
    Check(a && a == b, "both bots share the cached character");
    BotFreeCharacter(a); Live(b, "the cached character survives the first bot");
    BotFreeCharacter(b); Live(a, "the cached character is left to shutdown");
    OwnerEnd();
}

/* Retail bot_reloadcharacters 1: each bot loads its own character and frees it. */
static void PrivateGolden(void)
{
    int a, b, live;
    OwnerBegin("1"); a = BotLoadCharacter("bots/owner_c.c", 4); live = liveOwners;
    b = BotLoadCharacter("bots/owner_c.c", 4);
    Check(a && b && a != b && liveOwners > live, "reload policy loads a private character per bot");
    BotFreeCharacter(a); Check(!botcharacters[a], "the first private character is freed"); Live(b, "the second bot keeps its own character");
    BotFreeCharacter(b); Check(!botcharacters[b], "the second private character is freed");
    OwnerEnd();
}

/* Retail default with an interpolated skill: the interpolated character is cached too. */
static void InterpolatedGolden(void)
{
    int a, b;
    OwnerBegin("0"); a = BotLoadCharacter("bots/owner_c.c", 2); b = BotLoadCharacter("bots/owner_c.c", 2);
    Check(a && a == b && botcharacters[a]->skill == 2, "both bots share the cached interpolated character");
    BotFreeCharacter(a); BotFreeCharacter(b); Live(a, "the interpolated character is left to shutdown");
    OwnerEnd();
}

/* Bots sharing a cached character when bot_reloadcharacters becomes 1:
   freeing one must leave the character for the other and for shutdown. */
static void CachedThenReload(void)
{
    int a, b;
    OwnerBegin("0"); a = BotLoadCharacter("bots/owner_c.c", 4); b = BotLoadCharacter("bots/owner_c.c", 4);
    LibVarSet("bot_reloadcharacters", "1");
    BotFreeCharacter(a); Live(b, "only the bot is let go; the shared cached character stays live");
    BotFreeCharacter(b); Live(a, "the cached character is left to shutdown");
    OwnerEnd();
}

/* After the flip a bot reloads the same file privately. It must get its own handle,
   and freeing the bots that share the cached handle must not free it. */
static void ReloadAfterFlip(void)
{
    int a, b, c;
    OwnerBegin("0"); a = BotLoadCharacter("bots/owner_c.c", 4); b = BotLoadCharacter("bots/owner_c.c", 4);
    LibVarSet("bot_reloadcharacters", "1");
    BotFreeCharacter(a);
    c = BotLoadCharacter("bots/owner_c.c", 4);
    Check(c && c != b, "the reloaded private character never aliases the shared handle");
    BotFreeCharacter(b); Live(c, "freeing a sharing bot keeps the reloaded private character");
    Live(b, "the cached character is still live");
    BotFreeCharacter(c); Check(!botcharacters[c], "the reloaded private character is freed with its bot");
    OwnerEnd();
}

/* A private character loaded under 1 is still freed if the policy is now 0. */
static void PrivateThenCached(void)
{
    int a;
    OwnerBegin("1"); a = BotLoadCharacter("bots/owner_c.c", 4);
    LibVarSet("bot_reloadcharacters", "0");
    BotFreeCharacter(a); Check(a && !botcharacters[a], "the private character is freed with its bot");
    OwnerEnd();
}

/* A private character found through the cache after the policy became 0 is shared from then on. */
static void PrivateSharedAfterFlip(void)
{
    int a, b;
    OwnerBegin("1"); a = BotLoadCharacter("bots/owner_c.c", 4);
    LibVarSet("bot_reloadcharacters", "0"); b = BotLoadCharacter("bots/owner_c.c", 4);
    Check(a && a == b, "the cache hands the private character to a second bot");
    BotFreeCharacter(a); Live(b, "the now shared character survives its first bot");
    BotFreeCharacter(b); Live(a, "the shared character is left to shutdown");
    OwnerEnd();
}

/* Under 1 a second bot with the same interpolated skill is given the first bot's
   character by BotLoadCharacter's cache check; only the last of the two frees it. */
static void InterpolatedSharedReload(void)
{
    int a, b;
    OwnerBegin("1"); a = BotLoadCharacter("bots/owner_c.c", 2); b = BotLoadCharacter("bots/owner_c.c", 2);
    Check(a && a == b, "the cache hands the interpolated character to a second bot");
    BotFreeCharacter(a); Live(b, "the shared interpolated character survives its first bot");
    BotFreeCharacter(b); Check(!botcharacters[b], "the private interpolated character is freed with its last bot");
    OwnerEnd();
}

/* A map change shuts the botlib down; both policies then start again from scratch. */
static void MapChange(void)
{
    int first, again;
    OwnerBegin("0"); first = BotLoadCharacter("bots/owner_c.c", 4); BotFreeCharacter(first); OwnerEnd();
    OwnerBegin("1"); again = BotLoadCharacter("bots/owner_c.c", 4); BotFreeCharacter(again);
    Check(!botcharacters[again], "a private character is freed after a map change"); OwnerEnd();
    OwnerBegin("0"); again = BotLoadCharacter("bots/owner_c.c", 4);
    Check(again == first, "a new map loads the cached character into the same handle"); OwnerEnd();
}

int main(int argc, char **argv)
{
    static void (*const cases[])(void) = { SharedGolden, PrivateGolden, InterpolatedGolden, CachedThenReload,
        ReloadAfterFlip, PrivateThenCached, PrivateSharedAfterFlip, InterpolatedSharedReload, MapChange };
    int i;
    if (argc > 1) { cases[atoi(argv[1])](); return 0; }
    for (i = 0; i < (int)(sizeof(cases) / sizeof(cases[0])); i++) cases[i]();
    puts("Shared cached characters survive policy changes and reloads; private characters are freed (issue #48)");
    return 0;
}
