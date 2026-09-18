/* Actual encoded expansion and complete construction publication. */
#define Q3_CHAT_CONSUMER_NO_MAIN
#include "bot_chat_consumer_regression.c"

static void Expansion(int kind, int golden)
{
    struct { unsigned int before; char text[MAX_MESSAGE_SIZE]; unsigned int after; } output, saved;
    bot_match_t match;
    bot_randomlist_t group;
    bot_randomstring_t entry;
    char message[1025], randomValue[MAX_MESSAGE_SIZE];
    char *input = message;
    bot_match_t *owner = &match;
    int result;
    ChatBegin(); memset(&match,0,sizeof(match)); strcpy(match.string,"Native");
    match.variables[0].offset = 0; match.variables[0].length = 6;
    memset(&group,0,sizeof(group)); memset(&entry,0,sizeof(entry));
    group.string = "ONE"; group.numstrings = 1; group.firstrandomstring = &entry; entry.string = "Native random";
    if (golden) {
        if (kind == 0) { strcpy(message,"Native literal"); owner = NULL; }
        else if (kind == 1) strcpy(message,"<\001v0\001>");
        else if (kind == 2) { strcpy(message,"<\001v0\001>"); match.variables[0].offset = -1; }
        else if (kind == 3) { strcpy(message,"\001rONE\001"); randomstrings = &group; }
        else if (kind == 4) { memset(message,'x',255); message[255] = 0; }
        else { memset(randomValue,'x',255); randomValue[255] = 0; entry.string = randomValue; randomstrings = &group; strcpy(message,"\001rONE\001"); }
    } else {
        if (kind == 0) { memset(message,'x',256); message[256] = 0; }
        else if (kind == 1) strcpy(message,"\001v8\001");
        else if (kind == 2) strcpy(message,"\001v9999999999999999999999999999999999\001");
        else if (kind == 3) { strcpy(message,"\001v0\001"); match.variables[0].length = 999; }
        else if (kind == 4) { strcpy(message,"\001v0\001"); match.variables[0].offset = 7; match.variables[0].length = 1; }
        else if (kind == 5) { strcpy(message,"\001v0\001"); match.variables[0].length = -1; }
        else if (kind == 6) { strcpy(message,"\001v0\001"); memset(match.string,'x',sizeof(match.string)); }
        else if (kind == 7) { strcpy(message,"\001v0\001"); owner = NULL; }
        else if (kind == 8) { strcpy(message,"Native"); input = NULL; }
        else if (kind == 9) strcpy(message,"Prefix \001q\001");
        else if (kind == 10) strcpy(message,"Prefix \001rMISSING\001");
        else if (kind == 11) strcpy(message,"\001v\001");
        else if (kind == 12) strcpy(message,"\001v0");
        else { memset(randomValue,'x',255); randomValue[255] = 0; entry.string = randomValue; randomstrings = &group; strcpy(message,"Prefix \001rONE\001"); }
    }
    memset(&output,0xa5,sizeof(output)); output.before = 0x12345678; output.after = 0x87654321; saved = output;
    result = BotExpandChatMessage(output.text,input,0,owner,0,0);
    if (golden) {
        char *expected = kind == 0 ? "Native literal" : kind == 1 ? "<Native>" : kind == 2 ? "<>" : kind == 3 ? "Native random" : kind == 5 ? randomValue : message;
        Check(result == (kind == 3 || kind == 5) && !errors && !warnings && !requests && !strcmp(output.text,expected) &&
              output.before == saved.before && output.after == saved.after,
              "legacy expansion flag, literal/variable/unset/random/exact native output and canaries stay");
    } else Check(!result && errors == 1 && !requests && !memcmp(&output,&saved,sizeof(output)),
          "invalid encoded input/span/index/boundary keeps every destination/canary byte");
    randomstrings = NULL; ChatEnd();
}

static void Construction(int kind, int golden)
{
    int handle = ConsumerBegin();
    bot_chatstate_t saved = *botchatstates[handle];
    bot_match_t match;
    bot_randomlist_t first, second;
    bot_randomstring_t one, two;
    char message[257];
    memset(&match,0,sizeof(match)); strcpy(match.string,"Native"); match.variables[0].length = 6;
    memset(&first,0,sizeof(first)); memset(&second,0,sizeof(second)); memset(&one,0,sizeof(one)); memset(&two,0,sizeof(two));
    first.string = "ONE"; first.numstrings = 1; first.firstrandomstring = &one;
    second.string = "TWO"; second.numstrings = 1; second.firstrandomstring = &two;
    first.next = &second; one.string = "\001rTWO\001"; two.string = "Native final";
    strcpy(message,"\001rONE\001"); randomstrings = &first;
    if (golden) { if (kind == 1) one.string = "\001rONE\001"; }
    else if (kind == 0) { memset(message,'x',256); message[256] = 0; }
    else if (kind == 1) one.string = "Partial \001rMISSING\001";
    else if (kind == 2) { randomstrings = NULL; strcpy(message,"\001v8\001"); }
    else if (kind == 3) { randomstrings = NULL; strcpy(message,"Native"); }
    BotConstructChatMessage(kind == 3 && !golden ? NULL : botchatstates[handle],message,0,&match,0,0);
    if (golden) Check(!errors && !requests && warnings == (kind == 1 ? 2 : 0) &&
          !strcmp(botchatstates[handle]->chatmessage,kind == 1 ? "\001rONE\001" : "Native final"),
          "native nested random output and ten-iteration cycle limit/warnings remain");
    else Check(errors == 1 && !requests && !memcmp(&saved,botchatstates[handle],sizeof(saved)),
          "initial or later construction failure preserves every prior pending-state byte");
    randomstrings = NULL; ChatEnd();
}

static void PublicFailure(int replyMode)
{
    int handle = ConsumerBegin();
    bot_chat_t chat;
    bot_chattype_t type;
    bot_chatmessage_t line, savedLine;
    bot_replychat_t reply;
    bot_replychatkey_t key;
    bot_chatstate_t saved;
    memset(&chat,0,sizeof(chat)); memset(&type,0,sizeof(type)); memset(&line,0,sizeof(line));
    memset(&reply,0,sizeof(reply)); memset(&key,0,sizeof(key));
    line.chatmessage = "Prefix \001rMISSING\001";
    strcpy(type.name,"Native"); type.numchatmessages = 1; type.firstchatmessage = &line; chat.types = &type;
    key.flags = RCKFL_STRING; key.string = "Native"; reply.priority = 2;
    reply.keys = &key; reply.firstchatmessage = &line;
    if (replyMode) replychats = &reply; else botchatstates[handle]->chat = &chat;
    saved = *botchatstates[handle]; savedLine = line;
    if (replyMode) Check(!BotReplyChat(handle,"Native",0,0,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL),
          "public reply propagates checked construction failure as native false");
    else BotInitialChat(handle,"Native",0,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL);
    Check(errors == 1 && !requests && !memcmp(&saved,botchatstates[handle],sizeof(saved)) &&
          !memcmp(&savedLine,&line,sizeof(line)), "public initial/reply failure preserves pending state and selected-line timing");
    replychats = NULL; botchatstates[handle]->chat = NULL; ChatEnd();
}

static void MissingExpansionOutput(void)
{
    ChatBegin(); Check(!BotExpandChatMessage(NULL,"Native",0,NULL,0,0) && errors == 1 && !requests,
          "missing expansion output rejects before any copy"); ChatEnd();
}

static void RecentFallback(void)
{
    int handle = ConsumerBegin();
    bot_chat_t chat;
    bot_chattype_t type;
    bot_chatmessage_t line;
    memset(&chat,0,sizeof(chat)); memset(&type,0,sizeof(type)); memset(&line,0,sizeof(line));
    strcpy(type.name,"Native"); type.numchatmessages = 1; type.firstchatmessage = &line;
    line.chatmessage = "Native fallback"; line.time = 200; chat.types = &type;
    botchatstates[handle]->chat = &chat;
    BotInitialChat(handle,"Native",0,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL);
    Check(!errors && !requests && line.time == 200 && !strcmp(botchatstates[handle]->chatmessage,"Native fallback"),
          "native all-recent fallback preserves its original timing behavior");
    botchatstates[handle]->chat = NULL; ChatEnd();
}

static void NativeExpansion(void)
{
    int i;
    for (i=0;i<6;i++) Expansion(i,1);
    Construction(0,1); Construction(1,1); RecentFallback();
}

int main(int argc,char **argv)
{
    int proof;
    if (argc > 1) {
        proof = atoi(argv[1]);
        if (proof < 14) Expansion(proof,0);
        else if (proof < 18) Construction(proof-14,0);
        else if (proof < 20) PublicFailure(proof-18);
        else if (proof == 20) MissingExpansionOutput();
        else NativeExpansion();
        return 0;
    }
    NativeExpansion();
    for (proof=0;proof<14;proof++) Expansion(proof,0);
    for (proof=0;proof<4;proof++) Construction(proof,0);
    PublicFailure(0); PublicFailure(1); MissingExpansionOutput();
    puts("Actual encoded chat/construct bounds, checked spans/indexes and prior publication preserve native flags/recursive-limit output and physical ownership (issue #48)");
    return 0;
}
