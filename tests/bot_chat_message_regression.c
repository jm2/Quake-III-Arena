/* Actual chat message loader, source parser, chat file loaders and integrity check (issue #300). */
#define Q3_CHAT_STATE_NO_MAIN
#include "bot_chat_state_regression.c"
#include <limits.h>
int botDeveloper;

static char lastError[1024];

static void QDECL RecordPrint(int level, char *format, ...)
{
    va_list ap;
    if (level == PRT_ERROR || level == PRT_FATAL) {
        errors++;
        va_start(ap, format); vsnprintf(lastError, sizeof(lastError), format, ap); va_end(ap);
    } else if (level == PRT_WARNING) warnings++;
    else messages++;
}

static void MessageBegin(const char *text)
{
    ChatBegin(); Attempt(text); lastError[0] = 0; botimport.Print = RecordPrint;
}

/* One message through the real source parser into a native MAX_MESSAGE_SIZE destination. */
static int LoadMessage(const char *text, char *message)
{
    source_t *source;
    int result;
    MessageBegin(text); source = LoadSourceFile("native.c"); Check(source != NULL, "chat message source loads");
    result = BotLoadChatMessage(source, message);
    FreeSource(source);
    return result;
}

static void Accept(const char *text, const char *expected)
{
    char message[MAX_MESSAGE_SIZE];
    Check(LoadMessage(text, message) && !errors && !strcmp(message, expected),
          "a message that fits loads the retail component bytes");
    ChatEnd();
}

static void Reject(const char *text)
{
    char message[MAX_MESSAGE_SIZE];
    Check(!LoadMessage(text, message) && errors == 1 && strstr(lastError, "chat message too long"),
          "a message past MAX_MESSAGE_SIZE is rejected with the chat source error");
    ChatEnd();
}

/* The retail escape for a variable index: %ld of the lexer's unsigned long. */
static int Variable(char *out, size_t size, unsigned long value)
{
    return snprintf(out, size, "%cv%ld%c", ESCAPE_CHAR, (long)value, ESCAPE_CHAR);
}

/* A literal of 'length' x characters (none for 0) followed by 'tail' tokens. */
static void Prefixed(char *text, size_t size, int length, const char *tail)
{
    char literal[1024];
    memset(literal, 'x', (size_t)length); literal[length] = 0;
    if (length) snprintf(text, size, "\"%s\", %s", literal, tail);
    else snprintf(text, size, "%s", tail);
}

/* Final component 'tail' encodes as 'escape'; the message must fit only up to 255 bytes. */
static void Boundary(const char *tail, const char *escape)
{
    char text[2048], expected[MAX_MESSAGE_SIZE];
    int escapeLength = (int)strlen(escape), length;
    for (length = MAX_MESSAGE_SIZE - 1; length <= MAX_MESSAGE_SIZE + 1; length++) {
        int prefix = length - escapeLength;
        Check(prefix >= 0, "boundary prefix");
        Prefixed(text, sizeof(text), prefix, tail);
        if (length < MAX_MESSAGE_SIZE) {
            memset(expected, 'x', (size_t)prefix); strcpy(expected + prefix, escape);
            Check((int)strlen(expected) == MAX_MESSAGE_SIZE - 1, "boundary message is MAX_MESSAGE_SIZE - 1 bytes");
            Accept(text, expected);
        } else Reject(text);
    }
}

static void Name(char *name, int length, char fill)
{
    memset(name, fill, (size_t)length); name[length] = 0;
}

static void RetailLines(void)
{
    char text[256], expected[256], escape[64];
    unsigned long values[] = { 0, 1, 7, 8, 9, 10, 42, 99, 255, 256, 1000, 65535, 2147483647UL, 2147483648UL,
                               4294967295UL, ULONG_MAX / 2, ULONG_MAX / 2 + 1, ULONG_MAX };
    int i;
    Accept("\"Nice shot, \", 0, \". Want some \", fighter, \"?\";", "Nice shot, \001v0\001. Want some \001rfighter\001?");
    Accept("\"You think you can take me, \", 1, \"?\";", "You think you can take me, \001v1\001?");
    Accept("#define HELLO_NATIVE \"Hello \", 0, \", \", greeting\nHELLO_NATIVE, \"!\";", "Hello \001v0\001, \001rgreeting\001!");
    Accept("victory;", "\001rvictory\001");
    Accept("\"\";", "");
    Accept("\"one\" \"two\", 3;", "onetwo\001v3\001");
    /* Retail prints every integer index with %ld and never range checks it. */
    for (i = 0; i < (int)(sizeof(values) / sizeof(values[0])); i++) {
        snprintf(text, sizeof(text), "\"idx \", %lu, \".\";", values[i]);
        Variable(escape, sizeof(escape), values[i]); snprintf(expected, sizeof(expected), "idx %s.", escape);
        Accept(text, expected);
    }
    Check(!strcmp(escape, "\001v-1\001"), "retail %ld prints the largest index as -1");
    Variable(escape, sizeof(escape), 16); Accept("0x10;", escape);
    Variable(escape, sizeof(escape), 8); Accept("010;", escape);
}

/* Escapes of 7 or more bytes, which the old 7-byte reservation let run past the destination. */
static void Boundaries(void)
{
    char tail[1100], escape[MAX_MESSAGE_SIZE + 8], name[1100];
    /* Literal text (already exact before the fix). */
    Boundary("\"y\";", "y");
    /* Long variable escapes. */
    Variable(escape, sizeof(escape), 1234); Boundary("1234;", escape);
    Variable(escape, sizeof(escape), 2147483648UL); Boundary("2147483648;", escape);
    Variable(escape, sizeof(escape), ULONG_MAX / 2 + 1); snprintf(tail, sizeof(tail), "%lu;", ULONG_MAX / 2 + 1); Boundary(tail, escape);
    /* Random-string escapes of several name lengths. */
    Name(name, 4, 'r'); snprintf(tail, sizeof(tail), "%s;", name); snprintf(escape, sizeof(escape), "\001r%s\001", name); Boundary(tail, escape);
    Name(name, 20, 'r'); snprintf(tail, sizeof(tail), "%s;", name); snprintf(escape, sizeof(escape), "\001r%s\001", name); Boundary(tail, escape);
    Name(name, 200, 'r'); snprintf(tail, sizeof(tail), "%s;", name); snprintf(escape, sizeof(escape), "\001r%s\001", name); Boundary(tail, escape);
    /* A lone random name: 252 characters fit, 253 and 254 do not. */
    Name(name, 252, 'k'); snprintf(tail, sizeof(tail), "%s;", name); snprintf(escape, sizeof(escape), "\001r%s\001", name); Accept(tail, escape);
    Name(name, 253, 'k'); snprintf(tail, sizeof(tail), "%s;", name); Reject(tail);
    Name(name, 254, 'k'); snprintf(tail, sizeof(tail), "%s;", name); Reject(tail);
    /* The longest lexer name, a 1023-character random string reference. */
    Name(name, MAX_TOKEN - 1, 'k'); snprintf(tail, sizeof(tail), "%s;", name); Reject(tail);
    Name(name, MAX_TOKEN - 1, 'k'); snprintf(tail, sizeof(tail), "\"xxxxxxxx\", %s;", name); Reject(tail);
}

/* Escapes shorter than 7 bytes fit up to the last byte; the old 7-byte reservation refused a 4- or 5-byte
   escape that ends in the last 1-2 bytes (retail rejected those messages, Quake3e loads them). */
static void ShortEscapes(void)
{
    char escape[16];
    Variable(escape, sizeof(escape), 7); Boundary("7;", escape);
    Variable(escape, sizeof(escape), 42); Boundary("42;", escape);
    Variable(escape, sizeof(escape), 999); Boundary("999;", escape);
    Boundary("q;", "\001rq\001");
    Boundary("abc;", "\001rabc\001");
}

/* 'count' copies of one component, each encoding as 'escape'. */
static void Repeated(const char *component, const char *escape, int count)
{
    char text[4096], expected[4096];
    int i, fits = (int)strlen(escape) * count < MAX_MESSAGE_SIZE;
    text[0] = expected[0] = 0;
    for (i = 0; i < count; i++) {
        strcat(text, component); strcat(text, i + 1 < count ? ", " : ";");
        strcat(expected, escape);
    }
    if (fits) Accept(text, expected);
    else Reject(text);
}

static void ManyEscapes(void)
{
    char escape[64], component[64], text[2048], expected[MAX_MESSAGE_SIZE], name[MAX_MESSAGE_SIZE];
    const char *mixed = "abc\001v12345678\001\001rname_of_twenty_chars\001\001v0\001d\001v2147483647\001\001rr\001";
    int length, count;
    Variable(escape, sizeof(escape), 0); length = (int)strlen(escape); count = (MAX_MESSAGE_SIZE - 1) / length;
    Repeated("0", escape, count); Repeated("0", escape, count + 1);
    Variable(escape, sizeof(escape), 2147483648UL); length = (int)strlen(escape); count = (MAX_MESSAGE_SIZE - 1) / length;
    Repeated("2147483648", escape, count); Repeated("2147483648", escape, count + 1); Repeated("2147483648", escape, count + 4);
    Variable(escape, sizeof(escape), ULONG_MAX / 2 + 1); snprintf(component, sizeof(component), "%lu", ULONG_MAX / 2 + 1);
    length = (int)strlen(escape); count = (MAX_MESSAGE_SIZE - 1) / length;
    Repeated(component, escape, count); Repeated(component, escape, count + 1);
    snprintf(escape, sizeof(escape), "\001rinsult_native\001"); length = (int)strlen(escape); count = (MAX_MESSAGE_SIZE - 1) / length;
    Repeated("insult_native", escape, count); Repeated("insult_native", escape, count + 1); Repeated("insult_native", escape, count + 3);
    /* Mixed literal, variable and random components, then a random name that ends at 255 or 256 bytes. */
    for (length = MAX_MESSAGE_SIZE - 1; length <= MAX_MESSAGE_SIZE; length++) {
        Name(name, length - (int)strlen(mixed) - 3, 'n');
        snprintf(text, sizeof(text), "\"abc\", 12345678, name_of_twenty_chars, 0, \"d\", 2147483647, \"\", r, %s;", name);
        snprintf(expected, sizeof(expected), "%s\001r%s\001", mixed, name);
        if (length < MAX_MESSAGE_SIZE) Accept(text, expected);
        else Reject(text);
    }
}

/* A chat file through the real two-pass initial loader and its bot_developer integrity check;
   'expected' lists the messages in file order, NULL for a file that must be rejected. */
static void InitialChat(const char *text, const char **expected, int count)
{
    bot_chat_t *chat;
    MessageBegin(text); botDeveloper = 1;
    chat = BotLoadInitialChat("native.c", "native");
    botDeveloper = 0;
    if (expected) {
        bot_chatmessage_t *message;
        int i = count;
        Check(chat != NULL && chat->types && !chat->types->next && !strcmp(chat->types->name, "native_type") &&
              chat->types->numchatmessages == count && !errors, "fitting initial chat file loads");
        /* The loader prepends, so the list runs from the last message to the first. */
        for (message = chat->types->firstchatmessage; message; message = message->next)
            Check(--i >= 0 && !strcmp(message->chatmessage, expected[i]), "every initial message keeps its retail bytes");
        Check(i == 0, "every initial message is published");
        FreeMemory(chat);
    } else Check(!chat && errors == 1 && strstr(lastError, "chat message too long"), "overlong initial message rejects the chat file");
    ChatEnd();
}

static void ReplyChat(const char *messageText, const char *expected)
{
    char text[4096];
    bot_replychat_t *reply;
    snprintf(text, sizeof(text), "[\"native\"] = 1\n{\n%s\n\"Native reply\";\n}\n", messageText);
    MessageBegin(text); botDeveloper = 1;
    reply = BotLoadReplyChat("native.c");
    botDeveloper = 0;
    if (expected) {
        Check(reply != NULL && !reply->next && reply->numchatmessages == 2 && !errors &&
              !strcmp(reply->firstchatmessage->chatmessage, "Native reply") && reply->firstchatmessage->next &&
              !strcmp(reply->firstchatmessage->next->chatmessage, expected), "fitting reply chat file loads");
        replychats = reply;
    } else Check(!reply && errors == 1 && strstr(lastError, "chat message too long"), "overlong reply message rejects the reply chat file");
    ChatEnd();
}

static void RandomStrings(const char *messageText, const char *expected)
{
    char text[4096];
    bot_randomlist_t *list;
    snprintf(text, sizeof(text), "native_random_1 = {\n\"Native random!!\";\n%s\n}\n", messageText);
    MessageBegin(text);
    list = BotLoadRandomStrings("native.c");
    if (expected) {
        Check(list != NULL && !list->next && list->numstrings == 2 && !strcmp(list->string, "native_random_1") && !errors &&
              !strcmp(list->firstrandomstring->string, expected) && list->firstrandomstring->next &&
              !strcmp(list->firstrandomstring->next->string, "Native random!!"), "fitting random strings load");
        randomstrings = list;
    } else Check(!list && errors == 1 && strstr(lastError, "chat message too long"), "overlong random string rejects the random file");
    ChatEnd();
}

static void Loaders(void)
{
    char text[4096], name[1100], literal[300], message[1300], lone[MAX_MESSAGE_SIZE], random[MAX_MESSAGE_SIZE], variable[MAX_MESSAGE_SIZE];
    const char *expected[4];
    /* Longest messages each loader accepts: a lone 252-character random name, and literals holding an
       unterminated escape whose 253-character name the integrity check copies into its temp.
       Initial and random texts are 8n - 1 bytes: retail packs the next record right after each text. */
    Name(name, 252, 'k'); Name(literal, 253, 'k');
    snprintf(lone, sizeof(lone), "\001r%s\001", name);
    snprintf(random, sizeof(random), "\001r%s", literal);
    snprintf(variable, sizeof(variable), "\001v%s", literal);
    expected[0] = "Hello, \001v0\001!!!!"; expected[1] = lone; expected[2] = random; expected[3] = variable;
    snprintf(text, sizeof(text), "chat \"native\"\n{\ntype \"native_type\"\n{\n\"Hello, \", 0, \"!!!!\";\n%s;\n\"\\1r%s\";\n\"\\1v%s\";\n}\n}\n",
             name, literal, literal);
    InitialChat(text, expected, 4);
    snprintf(message, sizeof(message), "%s;", name); ReplyChat(message, lone);
    snprintf(message, sizeof(message), "\"\\1r%s\";", literal); ReplyChat(message, random);
    snprintf(message, sizeof(message), "%s;", name); RandomStrings(message, lone);
    /* Overlong escapes in each loader. */
    Name(name, MAX_TOKEN - 1, 'k');
    snprintf(text, sizeof(text), "chat \"native\"\n{\ntype \"native_type\"\n{\n\"Hello, \", 0, \"!!!!\";\n%s;\n}\n}\n", name);
    InitialChat(text, NULL, 0);
    snprintf(text, sizeof(text), "chat \"native\"\n{\ntype \"native_type\"\n{\n\"%.*s\", 2147483648;\n}\n}\n", 243, literal);
    InitialChat(text, NULL, 0);
    snprintf(message, sizeof(message), "%s;", name); ReplyChat(message, NULL); RandomStrings(message, NULL);
    Name(name, 253, 'k'); snprintf(message, sizeof(message), "%s;", name); ReplyChat(message, NULL); RandomStrings(message, NULL);
}

/* The integrity check directly on the longest message the loader can publish. */
static void Integrity(void)
{
    char *message = malloc(MAX_MESSAGE_SIZE);
    bot_stringlist_t *list, *next;
    int count = 0;
    Check(message != NULL, "integrity message storage");
    MessageBegin(nativeText);
    message[0] = ESCAPE_CHAR; message[1] = 'r'; memset(message + 2, 'k', MAX_MESSAGE_SIZE - 3); message[MAX_MESSAGE_SIZE - 1] = 0;
    list = BotCheckChatMessageIntegrety(message, NULL);
    Check(list && !list->next && strlen(list->string) == MAX_MESSAGE_SIZE - 3, "longest random name reaches the missing-string list");
    message[1] = 'v'; list = BotCheckChatMessageIntegrety(message, list);
    message[0] = ESCAPE_CHAR; message[1] = 'r'; message[MAX_MESSAGE_SIZE - 2] = ESCAPE_CHAR;
    list = BotCheckChatMessageIntegrety(message, list);
    for (; list; list = next) { next = list->next; FreeMemory(list); count++; }
    Check(count == 2 && !errors, "integrity check copies at most MAX_MESSAGE_SIZE - 3 name bytes");
    ChatEnd(); free(message);
}

int main(int argc, char **argv)
{
    int proof;
    if (argc > 1) {
        proof = atoi(argv[1]);
        if (proof == 0) Boundaries();
        else if (proof == 1) ManyEscapes();
        else if (proof == 2) Loaders();
        else if (proof == 3) ShortEscapes();
        else if (proof == 4) Integrity();
        else RetailLines();
        return 0;
    }
    RetailLines(); Boundaries(); ShortEscapes(); ManyEscapes(); Loaders(); Integrity();
    puts("Actual chat message escapes load byte-identically when they fit, reject past MAX_MESSAGE_SIZE with a source error, and keep the integrity check in bounds (issue #300)");
    return 0;
}
