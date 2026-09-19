/* Actual encoded-message parser, lexer/source and destination publication. */
#define Q3_CHAT_CONSUMER_NO_MAIN
#include "bot_chat_consumer_regression.c"

static void ReadMessage(char *text, char *expected, int valid, int missing)
{
    source_t *source;
    struct { unsigned int before; char text[MAX_MESSAGE_SIZE]; unsigned int after; } output, saved;
    int result;
    ChatBegin(); Attempt(text); source = LoadSourceFile("native.c"); Check(source != NULL,"actual message source loads");
    memset(&output,0xa5,sizeof(output)); output.before = 0x12345678; output.after = 0x87654321; saved = output;
    result = BotLoadChatMessage(missing == 1 ? NULL : source,missing == 2 ? NULL : output.text);
    Check(result == valid,"message parser returns native success only for a complete bounded message");
    if (valid) Check(!errors && !strcmp(output.text,expected) && output.before == saved.before && output.after == saved.after,
          "native encoded literal/variable/random order, terminator and guard bytes remain");
    else Check(errors >= 1 && !memcmp(&output,&saved,sizeof(output)),"bad syntax/range/size/source/output preserves every destination byte");
    Check(!hunkLive,"message parser acquires no persistent arena storage");
    FreeSource(source); ChatEnd();
}

static void Failure(int kind)
{
    char text[2048], word[1024];
    if (kind == 0) { memset(word,'k',253); word[253]=0; snprintf(text,sizeof(text),"%s;",word); }
    else if (kind == 1) { memset(word,'k',1023); word[1023]=0; snprintf(text,sizeof(text),"%s;",word); }
    else if (kind == 2) strcpy(text,"8;");
    else if (kind == 3) strcpy(text,"4294967295;");
    else if (kind == 4) { memset(word,'x',256); word[256]=0; snprintf(text,sizeof(text),"\"%s\";",word); }
    else if (kind == 5) strcpy(text,"\"Native\",\"Later\"");
    else if (kind == 6) strcpy(text,"\"Native\",\"\\q\";");
    else if (kind == 7) strcpy(text,";");
    else strcpy(text,"\"Native\";");
    ReadMessage(text,NULL,0,kind == 8 ? 1 : kind == 9 ? 2 : 0);
}

static void GoldenMessages(void)
{
    char text[1024], expected[MAX_MESSAGE_SIZE], word[MAX_MESSAGE_SIZE];
    int i;
    ReadMessage("\"Native\",0,\"-\",7,keyword;","Native\001v0\001-\001v7\001\001rkeyword\001",1,0);
    ReadMessage("\"\";","",1,0);
    for (i=0;i<MAX_MATCHVARIABLES;i++) {
        snprintf(text,sizeof(text),"%d;",i); snprintf(expected,sizeof(expected),"\001v%d\001",i);
        ReadMessage(text,expected,1,0);
    }
    memset(word,'x',255); word[255]=0; snprintf(text,sizeof(text),"\"%s\";",word); ReadMessage(text,word,1,0);
    memset(word,'k',252); word[252]=0; snprintf(text,sizeof(text),"%s;",word);
    expected[0]=ESCAPE_CHAR;expected[1]='r';memcpy(expected+2,word,252);expected[254]=ESCAPE_CHAR;expected[255]=0;
    ReadMessage(text,expected,1,0);
    memset(word,'x',249);word[249]=0;snprintf(text,sizeof(text),"\"%s\",7;",word);
    memcpy(expected,word,249);memcpy(expected+249,"\001v7\001",5);ReadMessage(text,expected,1,0);
}

static void ExactComponent(void)
{
    char text[1024], expected[MAX_MESSAGE_SIZE], word[MAX_MESSAGE_SIZE];
    memset(word,'x',251);word[251]=0;snprintf(text,sizeof(text),"\"%s\",7;",word);
    memcpy(expected,word,251);memcpy(expected+251,"\001v7\001",5);ReadMessage(text,expected,1,0);
}

int main(int argc,char **argv)
{
    int proof;
    if (argc>1) { proof=atoi(argv[1]);if(proof<10)Failure(proof);else GoldenMessages();return 0; }
    GoldenMessages(); ExactComponent(); for(proof=0;proof<10;proof++)Failure(proof);
    puts("Actual encoded message component costs/ranges and complete parser publication preserve native bytes/order, exact capacity and physical ownership (issue #48)");
    return 0;
}
