/* Actual public chat state factory, message lists and shutdown ownership. */
#define Q3_ITEM_OWNER_ONLY
#include "bot_item_config_regression.c"
#ifdef MEMORYMANEGER
extern int numblocks, allocatedmemory, totalmemorysize;
#endif
float AAS_Time(void) { return 125.5f; }

static void ChatBegin(void)
{
    int i;
    Begin();
    for (i = 0; i <= MAX_CLIENTS; i++) Check(!botchatstates[i], "prior chat state roots release");
    Check(!consolemessageheap && !freeconsolemessages, "prior console roots release");
}

static void ChatEnd(void)
{
    int i;
    BotShutdownChatAI();
    for (i = 0; i <= MAX_CLIENTS; i++) Check(!botchatstates[i], "shutdown clears every native state including last slot");
    Check(!consolemessageheap && !freeconsolemessages, "shutdown clears both console roots");
    End();
#ifdef MEMORYMANEGER
    Check(!numblocks && !allocatedmemory && !totalmemorysize, "all native tracked chat records release logically");
#endif
}

static void NativeState(int handle)
{
    BotSetChatName(handle, "Native", 9); BotSetChatGender(handle, CHAT_GENDERFEMALE);
    Check(botchatstates[handle]->client == 9 && botchatstates[handle]->gender == CHAT_GENDERFEMALE &&
          !strcmp(botchatstates[handle]->name, "Native") && !botchatstates[handle]->numconsolemessages &&
          !botchatstates[handle]->firstmessage && !botchatstates[handle]->lastmessage,
          "ordinary native chat name/client/gender and empty queue values");
}

static void Golden(void)
{
    bot_chatstate_t empty;
    int handle;
    ChatBegin(); memset(&empty, 0, sizeof(empty)); handle = BotAllocChatState();
    Check(handle == 1 && botchatstates[handle] && heapLive == 1 && requests == 1 && !errors &&
          !memcmp(botchatstates[handle], &empty, sizeof(empty)), "native first state is complete and cleared");
    NativeState(handle); BotFreeChatState(handle);
    Check(!botchatstates[handle] && !heapLive, "actual public free releases its physical heap owner");
    ChatEnd();
}

static void Nullable(int occupied)
{
    bot_chatstate_t saved, empty;
    int prior = 0, baseline, target;
    ChatBegin();
    if (occupied) { prior = BotAllocChatState(); Check(prior == 1, "native prior state"); NativeState(prior); saved = *botchatstates[prior]; }
    baseline = heapLive; target = occupied ? 2 : 1; Attempt(nativeText); failAt = 1;
    Check(BotAllocChatState() == 0 && requests == 1 && errors == 1 && !botchatstates[target] &&
          heapLive == baseline && !hunkLive, "nullable chat import returns zero and publishes no incomplete state");
    if (prior) Check(!memcmp(&saved, botchatstates[prior], sizeof(saved)), "every prior chat byte/root survives nullable factory");
    Attempt(nativeText); Check(BotAllocChatState() == target && requests == 1 && !errors &&
          heapLive == baseline + 1, "nullable chat state retries at the same first free native slot");
    memset(&empty, 0, sizeof(empty)); Check(!memcmp(&empty, botchatstates[target], sizeof(empty)),
          "retry publishes all cleared state bytes");
    ChatEnd();
}

static void LastSlot(void)
{
    int i, imports;
    ChatBegin();
    for (i = 1; i <= MAX_CLIENTS; i++) Check(BotAllocChatState() == i && botchatstates[i],
          "actual public allocation reaches every native client slot");
    imports = requests; Check(BotAllocChatState() == 0 && requests == imports && heapLive == MAX_CLIENTS,
          "native exhausted handle space imports no additional owner");
    NativeState(MAX_CLIENTS); ChatEnd();
    ChatBegin(); Check(BotAllocChatState() == 1 && requests == 1, "complete shutdown permits native first-slot reuse"); ChatEnd();
}

static void ConsoleShutdown(void)
{
    int handle;
    bot_consolemessage_t message;
    ChatBegin(); Check(LibVar("max_messages", "2") != NULL, "native two-message pool count");
    InitConsoleMessageHeap(); Check(consolemessageheap && freeconsolemessages && hunkLive == 1, "actual native console heap");
    handle = BotAllocChatState(); Check(handle == 1, "native queue state");
    BotQueueConsoleMessage(handle, 7, "one"); BotQueueConsoleMessage(handle, 8, "two");
    Check(BotNumConsoleMessages(handle) == 2 && BotNextConsoleMessage(handle, &message) == 1 &&
          message.time == 125.5f && message.type == 7 && !strcmp(message.message, "one") &&
          !message.prev && !message.next, "unchanged native FIFO/time/type/handle and public output links");
    BotRemoveConsoleMessage(handle, 1);
    Check(BotNextConsoleMessage(handle, &message) == 2 && message.type == 8 && !strcmp(message.message, "two") &&
          BotNumConsoleMessages(handle) == 1, "actual native queue removal preserves the next message");
    BotShutdownChatAI();
    Check(!botchatstates[handle] && !consolemessageheap && !freeconsolemessages && !AllocConsoleMessage() &&
          heapLive == 2 && hunkLive == 1 && !errors,
          "queued-state shutdown releases logical owners and clears stale pool roots before engine physical reset");
    ChatEnd();
}

int main(int argc, char **argv)
{
    int proof;
    if (argc > 1) {
        proof = atoi(argv[1]);
        if (proof < 2) Nullable(proof);
        else if (proof == 2) LastSlot();
        else if (proof == 3) ConsoleShutdown();
        else Golden();
        return 0;
    }
    Golden(); Nullable(0); Nullable(1); LastSlot(); ConsoleShutdown();
    puts("Actual chat nullable handles, last native slot, FIFO values, stale-root clearing and physical/logical shutdown pass (issue #48)");
    return 0;
}
