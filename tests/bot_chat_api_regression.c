/* Actual chat API string/output boundaries and native FIFO/property values. */
#define Q3_CHAT_STATE_NO_MAIN
#include "bot_chat_state_regression.c"

static int ApiBegin(void)
{
    int handle;
    ChatBegin(); Check(LibVar("max_messages", "2") != NULL, "complete native two-slot count");
    InitConsoleMessageHeap(); handle = BotAllocChatState(); Check(handle == 1, "actual native API state");
    NativeState(handle); Attempt(nativeText); return handle;
}

static void QueueText(int length)
{
    int handle = ApiBegin(), expected = length < MAX_MESSAGE_SIZE ? length : MAX_MESSAGE_SIZE - 1;
    char text[1025];
    struct { unsigned int before; bot_consolemessage_t message; unsigned int after; } output;
    int i;
    for (i = 0; i < length; i++) text[i] = (char)('a' + i % 26); text[length] = 0;
    memset(&output, 0xa5, sizeof(output)); output.before = 0x12345678U; output.after = 0x87654321U;
    BotQueueConsoleMessage(handle, 7, text);
    Check(BotNextConsoleMessage(handle, &output.message) == 1 && output.message.message[expected] == 0 &&
          !memcmp(output.message.message, text, (size_t)expected) && output.message.time == 125.5f &&
          output.message.type == 7 && !output.message.prev && !output.message.next &&
          output.before == 0x12345678U && output.after == 0x87654321U && !errors && !requests,
          "queued text is terminated at its native boundary while FIFO bytes/time/type/canaries remain");
    ChatEnd();
}

static void MissingQueue(void)
{
    int handle = ApiBegin(), baseline = heapLive;
    bot_chatstate_t state = *botchatstates[handle];
    bot_consolemessage_t pool[2];
    bot_consolemessage_t *root = freeconsolemessages;
    memcpy(pool, consolemessageheap, sizeof(pool));
    BotQueueConsoleMessage(handle, 7, NULL);
    Check(errors == 1 && !requests && heapLive == baseline && freeconsolemessages == root &&
          !memcmp(pool, consolemessageheap, sizeof(pool)) && !memcmp(&state, botchatstates[handle], sizeof(state)),
          "missing queued input rejects before any pool/client/serial mutation");
    ChatEnd();
}

static void MissingConsoleOutput(void)
{
    int handle = ApiBegin();
    bot_chatstate_t state;
    bot_consolemessage_t message;
    BotQueueConsoleMessage(handle, 7, "Native"); state = *botchatstates[handle]; Attempt(nativeText);
    Check(BotNextConsoleMessage(handle, NULL) == 0 && errors == 1 && !requests &&
          !memcmp(&state, botchatstates[handle], sizeof(state)), "missing FIFO output leaves every queued client byte");
    Check(BotNextConsoleMessage(handle, &message) == 1 && !strcmp(message.message, "Native"),
          "rejected FIFO output keeps the same native queued message");
    ChatEnd();
}

static void ChatOutput(int size, int missing)
{
    int handle = ApiBegin(), baseline = heapLive;
    bot_chatstate_t state;
    struct { unsigned char before; char text[32]; unsigned char after; } output, saved;
    strcpy(botchatstates[handle]->chatmessage, "~Native~Hello"); state = *botchatstates[handle];
    memset(&output, 0xa5, sizeof(output)); saved = output;
    BotGetChatMessage(handle, missing ? NULL : output.text, size);
    if (missing || size < 1) Check(errors == 1 && !requests && heapLive == baseline &&
          !memcmp(&state, botchatstates[handle], sizeof(state)) && !memcmp(&output, &saved, sizeof(output)),
          "invalid chat output pointer/size preserves all pending state and destination bytes");
    else {
        int count = size < 12 ? size - 1 : 11, i;
        Check(!errors && !requests && output.text[count] == 0 &&
              !memcmp(output.text, "NativeHello", (size_t)count) && !botchatstates[handle]->chatmessage[0] &&
              output.before == 0xa5 && output.after == 0xa5,
              "valid native chat output removes tildes, truncates, terminates and consumes pending text");
        for (i = count + 1; i < 32; i++)
            Check(output.text[i] == (i < size ? 0 : (char)0xa5), "native strncpy tail padding and bounds remain");
    }
    ChatEnd();
}

static void MissingName(void)
{
    int handle = ApiBegin();
    bot_chatstate_t state = *botchatstates[handle];
    BotSetChatName(handle, NULL, 77);
    Check(errors == 1 && !requests && !memcmp(&state, botchatstates[handle], sizeof(state)),
          "missing name rejects before changing client/name/payload metadata");
    ChatEnd();
}

static void MatchInput(int missing)
{
    bot_match_t match, saved;
    ChatBegin(); memset(&match, 0xa5, sizeof(match)); saved = match;
    Check(!BotFindMatch(missing ? "Native" : NULL, missing ? NULL : &match, 0) && errors == 1 &&
          !requests && !memcmp(&match, &saved, sizeof(match)), "missing match input/output rejects before destination mutation");
    ChatEnd();
}

static void MatchGolden(void)
{
    bot_match_t match;
    char text[1025];
    ChatBegin(); memset(&match, 0xa5, sizeof(match));
    Check(!BotFindMatch("Native\n\n", &match, 0) && !strcmp(match.string, "Native") && !errors && !requests,
          "unchanged native match copying and trailing-enter removal");
    memset(text, 'x', sizeof(text)); text[sizeof(text) - 1] = 0;
    Check(!BotFindMatch(text, &match, 0) && match.string[MAX_MESSAGE_SIZE - 1] == 0 &&
          strlen(match.string) == MAX_MESSAGE_SIZE - 1, "existing bounded match behavior remains");
    ChatEnd();
}

static void NativeGolden(void)
{
    int lengths[] = {0, 1, 31, 254, 255}, i;
    Golden();
    for (i = 0; i < 5; i++) QueueText(lengths[i]);
    ChatOutput(1, 0); ChatOutput(5, 0); ChatOutput(32, 0); MatchGolden();
}

int main(int argc, char **argv)
{
    int proof;
    if (argc > 1) {
        proof = atoi(argv[1]);
        if (proof == 0) QueueText(256);
        else if (proof == 1) MissingQueue();
        else if (proof == 2) MissingConsoleOutput();
        else if (proof == 3) ChatOutput(0, 0);
        else if (proof == 4) ChatOutput(1, 1);
        else if (proof == 5) MissingName();
        else if (proof == 6) MatchInput(0);
        else if (proof == 7) MatchInput(1);
        else NativeGolden();
        return 0;
    }
    NativeGolden(); QueueText(256); QueueText(1024); MissingQueue(); MissingConsoleOutput();
    ChatOutput(0, 0); ChatOutput(-1, 0); ChatOutput(INT_MIN, 0); ChatOutput(1, 1);
    MissingName(); MatchInput(0); MatchInput(1);
    puts("Actual public chat string termination/null/output-size guards, native FIFO/field bytes and physical ownership pass (issue #48)");
    return 0;
}
