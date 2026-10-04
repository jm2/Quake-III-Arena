/* Initial chat files shared through the ichatdata cache must only be freed by the cache,
   while private (bot_reloadcharacters 1) chats are freed with their chat state. */
#define Q3_CHAT_STATE_NO_MAIN
#include "bot_chat_state_regression.c"
int botDeveloper;

static const char *chatText =
    "chat \"alpha\"\n{\ntype \"greet\"\n{\n\"hello alpha\";\n}\n}\n"
    "chat \"beta\"\n{\ntype \"greet\"\n{\n\"hello beta\";\n}\n}\n";

static int CachedChats(void)
{
    int i, count = 0;
    for (i = 0; i < MAX_CLIENTS; i++) if (ichatdata[i]) count++;
    return count;
}

/* Reads through the chat, so a freed one trips ASan rather than passing silently. */
static void ChatIntact(int handle, const char *message)
{
    bot_chat_t *chat = botchatstates[handle]->chat;
    Check(chat && chat->types && !strcmp(chat->types->name, "greet") && chat->types->firstchatmessage &&
          !strcmp(chat->types->firstchatmessage->chatmessage, message), "chat state keeps a live, complete chat");
}

static void OwnerBegin(char *reload)
{
    ChatBegin(); Attempt(chatText); LibVarSet("bot_reloadcharacters", reload);
}

/* Retail default: both states share the one cached chat; freeing states leaves the cache alone. */
static void SharedGolden(void)
{
    int a, b, live, files;
    OwnerBegin("0"); a = BotAllocChatState(); b = BotAllocChatState();
    Check(BotLoadChatFile(a, "bots/alpha_c.c", "alpha") == BLERR_NOERROR && opens > 0 && CachedChats() == 1, "cold cached chat loads from file");
    live = heapLive; files = opens;
    Check(BotLoadChatFile(b, "bots/alpha_c.c", "alpha") == BLERR_NOERROR && opens == files && heapLive == live &&
          botchatstates[a]->chat == botchatstates[b]->chat && CachedChats() == 1, "second state reuses the cached chat without imports");
    BotFreeChatState(a); ChatIntact(b, "hello alpha");
    BotFreeChatState(b); Check(CachedChats() == 1 && ichatdata[0]->chat->types, "freed states leave the cached chat to shutdown");
    ChatEnd();
}

/* Retail bot_reloadcharacters 1: every state gets its own chat and frees it. */
static void PrivateGolden(void)
{
    int a, b, live, files;
    OwnerBegin("1"); live = heapLive; a = BotAllocChatState(); b = BotAllocChatState();
    Check(BotLoadChatFile(a, "bots/alpha_c.c", "alpha") == BLERR_NOERROR, "private chat loads"); files = opens;
    Check(BotLoadChatFile(b, "bots/alpha_c.c", "alpha") == BLERR_NOERROR &&
          opens == 2 * files && botchatstates[a]->chat != botchatstates[b]->chat && !CachedChats(), "reload policy loads private chats");
    BotFreeChatState(a); BotFreeChatState(b); Check(heapLive == live, "private chats are freed with their states");
    ChatEnd();
}

/* Loading a chat file again into the same state must not free the cached chat it already holds. */
static void ReloadSameState(void)
{
    int a, live;
    OwnerBegin("0"); a = BotAllocChatState();
    Check(BotLoadChatFile(a, "bots/alpha_c.c", "alpha") == BLERR_NOERROR, "cached chat loads"); live = heapLive;
    Check(BotLoadChatFile(a, "bots/alpha_c.c", "alpha") == BLERR_NOERROR && heapLive == live &&
          botchatstates[a]->chat == ichatdata[0]->chat, "reloading the same cached chat keeps the cached owner");
    ChatIntact(a, "hello alpha"); ChatEnd();
}

/* Switching one state to another chat must not free the chat a sibling state still shares. */
static void SwitchSharedState(void)
{
    int a, b, live;
    OwnerBegin("0"); a = BotAllocChatState(); b = BotAllocChatState();
    Check(BotLoadChatFile(a, "bots/alpha_c.c", "alpha") == BLERR_NOERROR && BotLoadChatFile(b, "bots/alpha_c.c", "alpha") == BLERR_NOERROR,
          "two states share the cached chat"); live = heapLive;
    Check(BotLoadChatFile(a, "bots/beta_c.c", "beta") == BLERR_NOERROR && CachedChats() == 2, "switched state caches a second chat");
    Check(heapLive > live, "switching does not free the shared chat");
    ChatIntact(a, "hello beta"); ChatIntact(b, "hello alpha"); ChatEnd();
}

/* A state loaded under the cached policy, freed after bot_reloadcharacters becomes 1
   (as BotInterbreeding does), must leave the cached chat for shutdown to free once. */
static void CachedThenReload(void)
{
    int a, live;
    OwnerBegin("0"); a = BotAllocChatState();
    Check(BotLoadChatFile(a, "bots/alpha_c.c", "alpha") == BLERR_NOERROR, "cached chat loads");
    LibVarSet("bot_reloadcharacters", "1"); live = heapLive; BotFreeChatState(a);
    Check(heapLive == live - 1 && CachedChats() == 1 && ichatdata[0]->chat->types, "only the state is freed; the cached chat stays live");
    ChatEnd();
}

/* A private chat loaded under bot_reloadcharacters 1 is still freed if the policy is now 0. */
static void PrivateThenCached(void)
{
    int a, live;
    OwnerBegin("1"); live = heapLive; a = BotAllocChatState();
    Check(BotLoadChatFile(a, "bots/alpha_c.c", "alpha") == BLERR_NOERROR && !CachedChats(), "private chat loads");
    LibVarSet("bot_reloadcharacters", "0"); BotFreeChatState(a);
    Check(heapLive == live, "the private chat is freed with its state");
    ChatEnd();
}

int main(int argc, char **argv)
{
    static void (*const cases[])(void) = { SharedGolden, PrivateGolden, ReloadSameState, SwitchSharedState, CachedThenReload, PrivateThenCached };
    int i;
    if (argc > 1) { cases[atoi(argv[1])](); return 0; }
    for (i = 0; i < (int)(sizeof(cases) / sizeof(cases[0])); i++) cases[i]();
    puts("Shared cached initial chats survive reloads, siblings and policy changes; private chats are freed (issue #48)");
    return 0;
}
