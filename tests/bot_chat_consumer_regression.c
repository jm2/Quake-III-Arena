/* Actual reply input, initial-count and match-span consumer boundaries. */
#define Q3_CHAT_STATE_NO_MAIN
#include "bot_chat_state_regression.c"

static int ConsumerBegin(void)
{
    int handle;
    ChatBegin(); handle = BotAllocChatState(); Check(handle == 1, "native consumer state");
    NativeState(handle); Attempt(nativeText); return handle;
}

static void ReplyInput(int length, int missing)
{
    int handle = ConsumerBegin(), valid = !missing && length < MAX_MESSAGE_SIZE;
    char text[1025];
    bot_replychat_t reply, savedReply;
    bot_replychatkey_t key;
    bot_chatmessage_t line, savedLine;
    bot_chatstate_t state;
    memset(&reply, 0, sizeof(reply)); memset(&key, 0, sizeof(key)); memset(&line, 0, sizeof(line));
    key.flags = RCKFL_STRING; key.string = "Native";
    line.chatmessage = "Native reply"; reply.priority = 2; reply.keys = &key; reply.firstchatmessage = &line;
    replychats = &reply;
    memset(text, 'x', sizeof(text)); memcpy(text, "Native ", 7); text[length] = 0;
    state = *botchatstates[handle]; savedLine = line; savedReply = reply;
    Check(BotReplyChat(handle, missing ? NULL : text, 0, 0, NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL) == valid,
          "native reply selection accepts bounded input and rejects missing/overlong input");
    if (valid) Check(!errors && !requests && !strcmp(botchatstates[handle]->chatmessage, "Native reply") &&
          line.time == 145.5f && !memcmp(&savedReply, &reply, sizeof(reply)),
          "native reply construction and selected-line timing are unchanged");
    else Check(errors == 1 && !requests && !memcmp(&state, botchatstates[handle], sizeof(state)) &&
          !memcmp(&savedLine, &line, sizeof(line)) && !memcmp(&savedReply, &reply, sizeof(reply)),
          "invalid reply input preserves complete pending state and dictionary/timing bytes");
    replychats = NULL; ChatEnd();
}

static void InitialCount(int mode)
{
    int handle = ConsumerBegin();
    bot_chat_t chat;
    bot_chattype_t type;
    bot_chatmessage_t line;
    bot_chatstate_t saved;
    memset(&chat, 0, sizeof(chat)); memset(&type, 0, sizeof(type)); memset(&line, 0, sizeof(line));
    strcpy(type.name, "Native"); type.numchatmessages = 1; type.firstchatmessage = &line;
    line.chatmessage = "Native initial"; chat.types = &type;
    if (mode != 0) botchatstates[handle]->chat = &chat;
    saved = *botchatstates[handle];
    Check(BotNumInitialChats(handle, mode == 1 ? NULL : "Native") == (mode == 2),
          "initial count handles absent files/types and keeps native loaded count");
    Check(!memcmp(&saved, botchatstates[handle], sizeof(saved)) && !errors && !requests,
          "count query does not change native state or owners");
    if (mode == 1) {
        BotInitialChat(handle, NULL, 0, NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL);
        Check(!memcmp(&saved, botchatstates[handle], sizeof(saved)) && line.time == 0 && !errors && !requests,
              "missing initial type rejects before line timing and pending-state changes");
    } else if (mode == 2) {
        BotInitialChat(handle, "Native", 0, NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL);
        Check(!strcmp(botchatstates[handle]->chatmessage, "Native initial") && line.time == 145.5f && !errors && !requests,
              "native loaded initial selection/construction and line timing stay");
    }
    botchatstates[handle]->chat = NULL; ChatEnd();
}

static void MatchSpan(int missing)
{
    bot_match_t match;
    struct { unsigned char before; char text[8]; unsigned char after; } output;
    ChatBegin(); memset(&match, 0, sizeof(match)); strcpy(match.string, "Native text");
    match.variables[0].offset = 7; match.variables[0].length = 4;
    memset(&output, 0xa5, sizeof(output));
    BotMatchVariable(missing ? NULL : &match, 0, output.text, sizeof(output.text));
    Check(output.before == 0xa5 && output.after == 0xa5 && !requests && !errors &&
          (missing ? output.text[0] == 0 && output.text[1] == (char)0xa5 : !strcmp(output.text,"text")),
          "missing match uses native empty-span result; valid substring and output canaries stay");
    ChatEnd();
}

static void MissingChoice(void)
{
    ChatBegin(); Check(!BotChooseInitialChatMessage(NULL, "Native") && !requests && !errors,
          "missing private initial state rejects before reading its chat root"); ChatEnd();
}

static void GoldenConsumers(void)
{
    ReplyInput(7, 0); ReplyInput(255, 0); InitialCount(2); InitialCount(1); MatchSpan(0);
}


#ifndef Q3_CHAT_CONSUMER_NO_MAIN
int main(int argc, char **argv)
{
    int proof;
    if (argc > 1) {
        proof = atoi(argv[1]);
        if (proof == 0) ReplyInput(256, 0);
        else if (proof == 1) ReplyInput(7, 1);
        else if (proof == 2) InitialCount(0);
        else if (proof == 3) InitialCount(1);
        else if (proof == 4) MatchSpan(1);
        else if (proof == 5) MissingChoice();
        else GoldenConsumers();
        return 0;
    }
    GoldenConsumers(); ReplyInput(256, 0); ReplyInput(1024, 0); ReplyInput(7, 1);
    InitialCount(0); InitialCount(1); MatchSpan(1); MissingChoice();
    puts("Actual reply/initial/match consumers reject invalid inputs, retain prior bytes and preserve native construction/physical ownership (issue #48)");
    return 0;
}

#endif /* Q3_CHAT_CONSUMER_NO_MAIN */
