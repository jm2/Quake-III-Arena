/* Actual initial/reply optional-variable aggregation and native expansion. */
#define Q3_CHAT_CONSUMER_NO_MAIN
#include "bot_chat_consumer_regression.c"

static void Variables(int replyMode, int kind)
{
    int handle = ConsumerBegin(), invalid = kind >= 3, result, i, length;
    char first[1025], second[257], expected[257], encoded[40];
    char *variables[MAX_MATCHVARIABLES] = {NULL};
    bot_chat_t chat;
    bot_chattype_t type;
    bot_chatmessage_t line, savedLine;
    bot_replychat_t reply, savedReply;
    bot_replychatkey_t key;
    bot_chatstate_t state;
    memset(&chat, 0, sizeof(chat)); memset(&type, 0, sizeof(type)); memset(&line, 0, sizeof(line));
    memset(&reply, 0, sizeof(reply)); memset(&key, 0, sizeof(key));
    strcpy(type.name, "Native"); type.numchatmessages = 1; type.firstchatmessage = &line; chat.types = &type;
    key.flags = RCKFL_STRING; key.string = "Native";
    reply.priority = 2; reply.keys = &key; reply.firstchatmessage = &line;
    if (replyMode) replychats = &reply; else botchatstates[handle]->chat = &chat;
    line.chatmessage = encoded;
    if (kind == 0) {
        strcpy(encoded,"\001v0\001-\001v7\001"); variables[0] = "Zero"; variables[7] = "Seven";
        strcpy(expected, "Zero-Seven");
    } else if (kind == 1 || kind == 3 || kind == 5) {
        length = kind == 5 ? 1024 : MAX_MESSAGE_SIZE - 1 - (replyMode ? 7 : 0) + (kind == 3);
        memset(first, 'x', sizeof(first)); first[length] = 0; variables[0] = first;
        strcpy(encoded,"\001v0\001");
        if (!invalid) { memcpy(expected, first, (size_t)length + 1); }
    } else if (kind == 2) {
        strcpy(encoded,"\001v0\001-\001v1\001-\001v2\001-\001v3\001-\001v4\001-\001v5\001-\001v6\001-\001v7\001");
        for (i = 0; i < MAX_MATCHVARIABLES; i++) variables[i] = "Var";
        strcpy(expected,"Var-Var-Var-Var-Var-Var-Var-Var");
    } else if (kind == 4) {
        memset(first, 'a', sizeof(first)); first[128] = 0;
        memset(second, 'b', sizeof(second)); second[128] = 0;
        variables[0] = first; variables[7] = second;
        strcpy(encoded,"\001v0\001-\001v7\001");
    } else {
        memset(first, 'z', sizeof(first)); first[256] = 0;
        variables[7] = first; strcpy(encoded,"\001v7\001");
    }
    state = *botchatstates[handle]; savedLine = line; savedReply = reply;
    if (replyMode) {
        result = BotReplyChat(handle,"Native ",0,0,variables[0],variables[1],variables[2],variables[3],variables[4],variables[5],variables[6],variables[7]);
        Check(result == !invalid, "native reply status reflects complete optional-variable capacity");
    } else BotInitialChat(handle,"Native",0,variables[0],variables[1],variables[2],variables[3],variables[4],variables[5],variables[6],variables[7]);
    if (invalid) Check(errors == 1 && !requests && !memcmp(&state,botchatstates[handle],sizeof(state)) &&
          !memcmp(&savedLine,&line,sizeof(line)) && !memcmp(&savedReply,&reply,sizeof(reply)),
          "excessive single/combined/late-slot variables preserve every pending/dictionary/timing byte");
    else Check(!errors && !requests && !strcmp(botchatstates[handle]->chatmessage,expected) && line.time == 145.5f,
          "native exact capacity, eight optional variables and real encoded expansion/timing stay");
    replychats = NULL; botchatstates[handle]->chat = NULL; ChatEnd();
}

static void GoldenVariables(void)
{
    int mode, kind;
    for (mode = 0; mode < 2; mode++) for (kind = 0; kind < 3; kind++) Variables(mode,kind);
}

int main(int argc,char **argv)
{
    int proof, mode, kind;
    if (argc > 1) {
        proof = atoi(argv[1]);
        if (proof < 8) Variables(proof / 4,3 + proof % 4); else GoldenVariables();
        return 0;
    }
    GoldenVariables();
    for (mode = 0; mode < 2; mode++) for (kind = 3; kind < 7; kind++) Variables(mode,kind);
    puts("Actual initial/reply optional variables reserve native NUL space, preserve prior state/timing on excess and retain encoded native expansion/ownership (issue #48)");
    return 0;
}
