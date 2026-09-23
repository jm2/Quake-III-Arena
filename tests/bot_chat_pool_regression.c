/* Actual checked console pool, queue migration and chat setup failure gate. */
#define Q3_CHAT_STATE_NO_MAIN
#include "bot_chat_state_regression.c"
int botDeveloper;
#ifdef Q3_CHAT_POOL_ORIGINAL
static int consolemessageheapcount;
static int InitConsoleMessageHeapChecked(void)
{
    InitConsoleMessageHeap();
    consolemessageheapcount = (int)LibVarGetValue("max_messages");
    return qtrue;
}
#endif

static float Opaque(unsigned int bits)
{
    volatile unsigned int representation = bits;
    unsigned int copy = representation;
    float value;
    memcpy(&value, &copy, sizeof(value)); return value;
}

static void FreeChain(int count, int used)
{
    int i;
    bot_consolemessage_t *message = freeconsolemessages;
    Check(consolemessageheap && consolemessageheapcount == count, "complete native pool capacity publishes");
    Check(message == (used < count ? &consolemessageheap[used] : NULL), "free root follows complete migrated queues");
    for (i = used; i < count; i++) {
        Check(message == &consolemessageheap[i] && message->prev == (i > used ? &consolemessageheap[i - 1] : NULL) &&
              message->next == (i + 1 < count ? &consolemessageheap[i + 1] : NULL) &&
              !message->handle && !message->time && !message->type && !message->message[0],
              "every unused native slot/link is complete and cleared");
        message = message->next;
    }
    Check(!message, "bounded complete native free chain");
}

static void PoolGolden(int fraction, int singleton)
{
    ChatBegin();
    if (fraction || singleton) Check(LibVar("max_messages", singleton ? "1" : "2.75") != NULL, "native configured count");
    Check(InitConsoleMessageHeapChecked() && !errors && heapLive == 2 && hunkLive == 1,
          "default/fraction/singleton checked native pool initializes");
    FreeChain(singleton ? 1 : fraction ? 2 : 1024, 0); ChatEnd();
}

static void FreshNullable(int position)
{
    ChatBegin(); failAt = position;
    Check(!InitConsoleMessageHeapChecked() && requests == failAt && errors == 1 &&
          !consolemessageheap && !freeconsolemessages && !consolemessageheapcount && !hunkLive &&
          heapLive == (position > 2 ? 2 : 0), "every fresh pool import rejects with complete private owner cleanup");
    Attempt(nativeText); Check(InitConsoleMessageHeapChecked(), "fresh nullable pool retries");
    FreeChain(1024, 0); ChatEnd();
}

static void Queues(void)
{
    ChatBegin(); Check(LibVar("max_messages", "4") != NULL && InitConsoleMessageHeapChecked(), "native prior four-slot pool");
    Check(BotAllocChatState() == 1 && BotAllocChatState() == 2 && BotAllocChatState() == 3, "two active/one empty native states");
    BotQueueConsoleMessage(1, 7, "first"); BotQueueConsoleMessage(1, 8, "second");
    BotQueueConsoleMessage(2, 9, "third");
    Check(BotNumConsoleMessages(1) == 2 && BotNumConsoleMessages(2) == 1 && !BotNumConsoleMessages(3), "native prior queue counts");
}

static void Migrated(int count, int removed)
{
    bot_consolemessage_t output;
    bot_consolemessage_t *first = consolemessageheap;
    Check(botchatstates[1]->firstmessage == first && botchatstates[1]->lastmessage == first + (removed ? 0 : 1) &&
          botchatstates[2]->firstmessage == first + (removed ? 1 : 2) &&
          botchatstates[2]->lastmessage == botchatstates[2]->firstmessage &&
          !botchatstates[3]->firstmessage && !botchatstates[3]->lastmessage,
          "every complete queue root rebinds into the new native pool");
    Check(BotNextConsoleMessage(1, &output) == (removed ? 2 : 1) && output.time == 125.5f &&
          output.type == (removed ? 8 : 7) && !strcmp(output.message, removed ? "second" : "first") &&
          !output.prev && !output.next, "native first queued handle/time/type/text survives replacement");
    Check(BotNextConsoleMessage(2, &output) == 1 && output.type == 9 && !strcmp(output.message, "third") &&
          botchatstates[1]->handle == 2 && botchatstates[2]->handle == 1,
          "per-client native serials and second queue payload remain");
    FreeChain(count, removed ? 2 : 3);
}

static void Migration(int count, int removed)
{
    bot_consolemessage_t *prior;
    int baseline;
    char text[32];
    Queues(); if (removed) BotRemoveConsoleMessage(1, 1);
    prior = consolemessageheap; baseline = heapLive; snprintf(text, sizeof(text), "%d", count);
    LibVarSet("max_messages", text); Attempt(nativeText);
    Check(InitConsoleMessageHeapChecked() && consolemessageheap != prior && heapLive == baseline && hunkLive == 2 &&
          requests == 2 && !errors, "queued pool replacement acquires exactly one complete persistent owner");
    Migrated(count, removed); ChatEnd();
}

static void PriorFailure(int capacity, int position)
{
    bot_consolemessage_t *prior, *freeRoot;
    bot_consolemessage_t saved[4];
    bot_chatstate_t states[3];
    int i, baseline;
    Queues(); prior = consolemessageheap; freeRoot = freeconsolemessages; baseline = heapLive;
    memcpy(saved, prior, sizeof(saved)); for (i = 0; i < 3; i++) states[i] = *botchatstates[i + 1];
    LibVarSet("max_messages", capacity ? "2" : "8"); Attempt(nativeText); if (!capacity) failAt = position;
    Check(!InitConsoleMessageHeapChecked() && errors == 1 && requests == (capacity ? 0 : position) &&
          consolemessageheap == prior && freeconsolemessages == freeRoot && consolemessageheapcount == 4 &&
          heapLive == baseline && hunkLive == 1 && !memcmp(saved, prior, sizeof(saved)),
          "nullable/too-small replacement keeps every prior pool/free/queue byte and spends no physical hunk");
    for (i = 0; i < 3; i++) Check(!memcmp(&states[i], botchatstates[i + 1], sizeof(states[i])), "all prior native state bytes survive");
    LibVarSet("max_messages", "8"); Attempt(nativeText);
    Check(InitConsoleMessageHeapChecked(), "failed occupied pool retries"); Migrated(8, 0); ChatEnd();
}

static void InvalidCount(unsigned int bits)
{
    libvar_t *variable;
    int baseline;
    Queues(); variable = LibVarGet("max_messages"); variable->value = Opaque(bits); baseline = heapLive;
    Attempt(nativeText);
    Check(!InitConsoleMessageHeapChecked() && !requests && errors == 1 && heapLive == baseline && hunkLive == 1 &&
          consolemessageheapcount == 4 && BotNumConsoleMessages(1) == 2 && BotNumConsoleMessages(2) == 1,
          "invalid finite/native signed count/cost rejects before integer conversion or arena import");
    ChatEnd();
}

static void InvalidQueue(int kind)
{
    bot_consolemessage_t original[4], changed[4];
    bot_chatstate_t states[3], changedStates[3];
    bot_consolemessage_t *prior, *freeRoot;
    int i, baseline;
    Queues(); prior = consolemessageheap; freeRoot = freeconsolemessages; baseline = heapLive;
    memcpy(original, prior, sizeof(original));
    for (i = 0; i < 3; i++) states[i] = *botchatstates[i + 1];
    if (kind == 0) botchatstates[1]->numconsolemessages = -1;
    else if (kind == 1) botchatstates[1]->numconsolemessages = 5;
    else if (kind == 2) botchatstates[1]->numconsolemessages = 1;
    else if (kind == 3) botchatstates[1]->firstmessage->prev = botchatstates[1]->lastmessage;
    else if (kind == 4) botchatstates[1]->lastmessage = botchatstates[1]->firstmessage;
    else botchatstates[1]->lastmessage->next = botchatstates[1]->firstmessage;
    memcpy(changed, prior, sizeof(changed));
    for (i = 0; i < 3; i++) changedStates[i] = *botchatstates[i + 1];
    Attempt(nativeText);
    Check(!InitConsoleMessageHeapChecked() && requests == (kind < 2 ? 0 : 1) && errors == 1 && heapLive == baseline &&
          hunkLive == 1 && consolemessageheap == prior && freeconsolemessages == freeRoot &&
          !memcmp(changed, prior, sizeof(changed)), "bad queue count/shape/cycle rejects before arena import without mutation");
    for (i = 0; i < 3; i++) Check(!memcmp(&changedStates[i], botchatstates[i + 1], sizeof(changedStates[i])),
          "invalid queue rejection retains every caller-provided state byte");
    memcpy(prior, original, sizeof(original));
    for (i = 0; i < 3; i++) *botchatstates[i + 1] = states[i];
    Attempt(nativeText); Check(InitConsoleMessageHeapChecked(), "repaired valid native queue retries");
    Migrated(4, 0); ChatEnd();
}

static void BadOwnership(int kind)
{
    bot_consolemessage_t original[4], changed[4], foreign;
    bot_chatstate_t states[3], changedStates[3];
    bot_consolemessage_t *prior, *freeRoot, *stale = NULL;
    int i, baseline, arena;
    Queues();
    if (kind == 4) { stale = consolemessageheap; Check(InitConsoleMessageHeapChecked(), "create genuine physically live stale pool"); }
    prior = consolemessageheap; freeRoot = freeconsolemessages; baseline = heapLive; arena = hunkLive;
    memcpy(original, prior, sizeof(original));
    for (i = 0; i < 3; i++) states[i] = *botchatstates[i + 1];
    memset(&foreign, 0, sizeof(foreign)); foreign.handle = 77;
    if (kind == 0 || kind == 1 || kind == 2 || kind == 4) {
        bot_consolemessage_t *bad = kind == 0 ? &foreign : kind == 1 ? prior + 4 :
            kind == 2 ? (bot_consolemessage_t *)((char *)prior + 1) : stale;
        botchatstates[1]->firstmessage = botchatstates[1]->lastmessage = bad;
        botchatstates[1]->numconsolemessages = 1;
    } else {
        botchatstates[2]->firstmessage = botchatstates[1]->firstmessage;
        botchatstates[2]->lastmessage = botchatstates[1]->lastmessage;
        botchatstates[2]->numconsolemessages = 2;
    }
    memcpy(changed, prior, sizeof(changed));
    for (i = 0; i < 3; i++) changedStates[i] = *botchatstates[i + 1];
    Attempt(nativeText);
    Check(!InitConsoleMessageHeapChecked() && requests == 1 && errors == 1 && heapLive == baseline &&
          hunkLive == arena && consolemessageheap == prior && freeconsolemessages == freeRoot &&
          !memcmp(changed, prior, sizeof(changed)), "foreign/one-past/misaligned/stale/shared node rejects before dereference/arena import");
    for (i = 0; i < 3; i++) Check(!memcmp(&changedStates[i], botchatstates[i + 1], sizeof(changedStates[i])),
          "bad node ownership preserves every caller-provided state byte");
    memcpy(prior, original, sizeof(original));
    for (i = 0; i < 3; i++) *botchatstates[i + 1] = states[i];
    Attempt(nativeText); Check(InitConsoleMessageHeapChecked(), "valid unique node ownership retries");
    Migrated(4, 0); ChatEnd();
}

static void SetupFailure(void)
{
    bot_synonymlist_t *oldSyn;
    bot_randomlist_t *oldRandom;
    bot_matchtemplate_t *oldMatch;
    bot_replychat_t *oldReply;
    int baseline;
    Queues();
    synonyms = GetClearedMemory(sizeof(*synonyms)); randomstrings = GetClearedMemory(sizeof(*randomstrings));
    matchtemplates = GetClearedMemory(sizeof(*matchtemplates)); replychats = GetClearedMemory(sizeof(*replychats));
    Check(synonyms && randomstrings && matchtemplates && replychats, "complete distinct prior dictionary owners");
    oldSyn = synonyms; oldRandom = randomstrings; oldMatch = matchtemplates; oldReply = replychats; baseline = heapLive;
    Attempt(""); failAt = 1;
    Check(BotSetupChatAI() == BLERR_LIBRARYNOTSETUP && requests == 1 && errors == 1 && !opens &&
          synonyms == oldSyn && randomstrings == oldRandom && matchtemplates == oldMatch && replychats == oldReply &&
          heapLive == baseline && hunkLive == 1 && BotNumConsoleMessages(1) == 2,
          "actual chat setup propagates pool failure before dictionary/source mutation");
    Attempt(nativeText); Check(InitConsoleMessageHeapChecked(), "chat setup pool failure retries privately");
    Migrated(4, 0); ChatEnd();
}

int main(int argc, char **argv)
{
    int proof, count, removed;
    unsigned int invalid[] = {0x7f800000U, 0xff800000U, 0x7fc00000U, 0x4f000000U, 0xcf800000U,
        0x4b800000U, 0, 0xbf800000U, 0x3f000000U};
    if (argc > 1) {
        proof = atoi(argv[1]);
        if (proof < 3) FreshNullable(proof + 1);
        else if (proof == 3) PriorFailure(0, 1);
        else if (proof == 4) Migration(8, 0);
        else if (proof == 5) PoolGolden(0, 1);
        else if (proof == 6) PriorFailure(1, 0);
        else if (proof >= 8 && proof <= 12) BadOwnership(proof - 8);
        else { PoolGolden(0, 0); PoolGolden(1, 0); }
        return 0;
    }
    PoolGolden(0, 0); PoolGolden(1, 0); PoolGolden(0, 1);
    for (proof = 1; proof <= 3; proof++) FreshNullable(proof);
    PriorFailure(0, 1); PriorFailure(0, 2); PriorFailure(1, 0); SetupFailure();
    for (proof = 0; proof < 5; proof++) BadOwnership(proof);
    for (proof = 0; proof < 6; proof++) InvalidQueue(proof);
    for (count = 3; count <= 8; count += count == 3 ? 1 : 4)
        for (removed = 0; removed < 2; removed++) Migration(count, removed);
    for (proof = 0; proof < 9; proof++) InvalidCount(invalid[proof]);
    puts("Actual console pool counts, nullable/source gates, queued migration/prior bytes, native links and physical ownership pass (issue #48)");
    return 0;
}
