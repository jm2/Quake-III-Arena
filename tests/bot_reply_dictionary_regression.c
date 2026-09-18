/* Actual reply dictionary imports, names, source lifetime and selection. */
#define Q3_MATCH_PIECE_NO_MAIN
#include "bot_match_piece_regression.c"

static const char *replies="[&\"Native\",!\"Blocked\",name,female,male,it,<\"Native\",\"Other\">,(\"hello \",0,\"!\",7)] = 2.75 { \"reply \",0; }";
static void ReplyValues(bot_replychat_t *root,int handle)
{
    bot_replychatkey_t *key;int count=0;unsigned int flags=0;
    Check(root && root->priority==2.75f && root->numchatmessages==1 && root->firstchatmessage &&
          !strcmp(root->firstchatmessage->chatmessage,"reply \001v0\001") && !root->next,
          "native fractional priority/count/encoded line fields remain");
    key=root->keys;Check(key && key->flags==RCKFL_VARIABLES && key->match && key->next &&
          key->next->flags==RCKFL_BOTNAMES && !strcmp(key->next->string,"Native\\Other"),
          "native reverse key order, owned match pieces and combined bot-name separator remain");
    for(key=root->keys;key;key=key->next){count++;flags|=key->flags;}
    Check(count==8 && flags==(RCKFL_AND|RCKFL_NOT|RCKFL_NAME|RCKFL_GENDERFEMALE|RCKFL_GENDERMALE|
          RCKFL_GENDERLESS|RCKFL_BOTNAMES|RCKFL_VARIABLES|RCKFL_STRING),"all native key branches and prefix flags remain");
    Check(BotReplyChat(handle,"hello Native!Tail",0,0,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL) &&
          !strcmp(botchatstates[handle]->chatmessage,"reply Native") && root->firstchatmessage->time==145.5f,
          "actual dictionary public selection/captures/construction and line timing remain");
    root->firstchatmessage->time=-2*CHATMESSAGE_RECENTTIME;
}
static int ReplyBegin(void)
{
    int handle;ChatBegin();handle=BotAllocChatState();NativeState(handle);return handle;
}
static void GoldenReplies(void)
{
    int handle=ReplyBegin();Attempt(replies);replychats=BotLoadReplyChat("native.c");ReplyValues(replychats,handle);
    Check(!errors && messages==1 && opens==closes && !numtokens,"successful native reply source releases before public selection");ChatEnd();
    ChatBegin();Attempt("");Check(!BotLoadReplyChat("native.c") && !errors && messages==2 && !heapLive && !hunkLive,
          "native empty optional reply file remains distinct from error");ChatEnd();
    ChatBegin();Attempt("[\"Native\"] = 0.5 { }");replychats=BotLoadReplyChat("native.c");
    Check(replychats && replychats->priority==0.5f && !replychats->numchatmessages && !replychats->firstchatmessage && !errors,
          "native zero-message dictionary and fractional priority remain");ChatEnd();
    ChatBegin();Attempt("[\"One\"] = 1 { \"first\"; \"last\"; } [\"Two\"] = 2 { \"only\"; }");replychats=BotLoadReplyChat("native.c");
    Check(replychats && !strcmp(replychats->keys->string,"Two") && !strcmp(replychats->firstchatmessage->chatmessage,"only") &&
          replychats->next && !strcmp(replychats->next->keys->string,"One") && replychats->next->numchatmessages==2 &&
          !strcmp(replychats->next->firstchatmessage->chatmessage,"last") &&
          !strcmp(replychats->next->firstchatmessage->next->chatmessage,"first") && !replychats->next->next && !errors,
          "native reverse dictionary and message ordering remain");ChatEnd();
}
static void Names(int length,int combined,int valid)
{
    char text[2200],word[1024],other[1024];
    ChatBegin();memset(word,'x',length);word[length]=0;memset(other,'y',128);other[128]=0;
    if(combined)snprintf(text,sizeof(text),"[<\"%s\",\"%s\">] = 1 { \"native\"; }",word,other);
    else snprintf(text,sizeof(text),"[<\"%s\">] = 1 { \"native\"; }",word);
    Attempt(text);replychats=BotLoadReplyChat("native.c");
    if(valid)Check(replychats && replychats->keys && strlen(replychats->keys->string)==(size_t)(combined?length+129:length) &&
          !strncmp(replychats->keys->string,word,length) && !errors,"exact native name capacity remains complete and terminated");
    else Check(!replychats && errors>=1 && !messages && !heapLive && !hunkLive && !numtokens && opens==closes,
          "oversized single/combined names reject with full physical cleanup");ChatEnd();
}
static void BadReplies(int kind)
{
    const char *bad[]={"[\"Native\"] = 1 { \"native\";","[\"Native\"] = 1 { \"native\"; } #unknown",
          "[\"Native\"] = 1 { \"native\"; } \"\\q\"","[\"Native\",(8)] = 1 { \"native\"; }",
          "[\"Native\"] = 1 { \"native\"; } bad","[\"Native\"] = 2147483648.0 { \"native\"; }",
          "[\"Native\"] = 2147483647.0 { \"native\"; }"};
    bot_replychat_t *prior,header;bot_replychatkey_t key;bot_chatmessage_t line;int owners,spent,handle=ReplyBegin();
    Attempt(replies);prior=BotLoadReplyChat("native.c");replychats=prior;ReplyValues(prior,handle);header=*prior;key=*prior->keys;line=*prior->firstchatmessage;
    owners=heapLive;spent=hunkLive;Attempt(bad[kind]);
    Check(!BotLoadReplyChat("native.c") && errors>=1 && !messages && heapLive==owners && hunkLive==spent && !numtokens && opens==closes &&
          replychats==prior && !memcmp(&header,prior,sizeof(header)) && !memcmp(&key,prior->keys,sizeof(key)) &&
          !memcmp(&line,prior->firstchatmessage,sizeof(line)),"malformed/source-invalid/unsafe-priority candidates release and preserve prior fields/pointers/timing");
    ReplyValues(prior,handle);Attempt(replies);{bot_replychat_t *retry=BotLoadReplyChat("native.c");Check(retry!=NULL,"invalid reply factory retries");BotFreeReplyChat(retry);}ChatEnd();
}
static void NullableReplies(int position,int priorMode)
{
    bot_replychat_t *prior=NULL,*candidate,header;bot_replychatkey_t key;bot_chatmessage_t line;int owners,handle=ReplyBegin();
    if(priorMode){Attempt(replies);prior=BotLoadReplyChat("native.c");replychats=prior;ReplyValues(prior,handle);header=*prior;key=*prior->keys;line=*prior->firstchatmessage;}
    owners=heapLive;Attempt(replies);failAt=position;candidate=BotLoadReplyChat("native.c");
    Check(!candidate && errors>=1 && requests>=failAt && !messages && heapLive==owners && !hunkLive && !numtokens && opens==closes && replychats==prior,
          "every nullable source/container/key/string/piece/message import rejects with physical cleanup and no root mutation");
    if(prior){Check(!memcmp(&header,prior,sizeof(header)) && !memcmp(&key,prior->keys,sizeof(key)) && !memcmp(&line,prior->firstchatmessage,sizeof(line)),
          "nullable candidate preserves prior header/key/line pointers and timing bytes");ReplyValues(prior,handle);}
    Attempt(replies);candidate=BotLoadReplyChat("native.c");Check(candidate!=NULL,"nullable reply factory retries");
    if(prior)BotFreeReplyChat(prior);replychats=candidate;ReplyValues(candidate,handle);ChatEnd();
}
static void InvalidReplyPath(int kind)
{
    char path[MAX_PATH+1];ChatBegin();memset(path,'x',sizeof(path));path[sizeof(path)-1]=0;
    Check(!BotLoadReplyChat(kind==0?NULL:kind==1?"":path) && errors==1 && !requests && !opens && !heapLive && !hunkLive,
          "invalid full reply filename rejects before imports");ChatEnd();
}
int main(int argc,char **argv)
{
    int handle,count,i,mode;
    if(argc>1){i=atoi(argv[1]);if(i<7)BadReplies(i);else if(i==7)Names(256,0,0);else if(i==8)Names(1023,0,0);else if(i==9)Names(127,1,0);
        else{GoldenReplies();Names(255,0,1);Names(126,1,1);}return 0;}
    GoldenReplies();Names(255,0,1);Names(126,1,1);Names(256,0,0);Names(1023,0,0);Names(127,1,0);Names(128,1,0);
    for(i=0;i<7;i++)BadReplies(i);for(i=0;i<3;i++)InvalidReplyPath(i);
    handle=ReplyBegin();Attempt(replies);replychats=BotLoadReplyChat("native.c");ReplyValues(replychats,handle);count=requests;ChatEnd();
    for(mode=0;mode<2;mode++)for(i=1;i<=count;i++)NullableReplies(i,mode);
    printf("Actual complete reply dictionaries, %d nullable imports, bounded names/priority, source rollback, native selection/timing and physical ownership (issue #48)\n",count*2);
    return 0;
}
