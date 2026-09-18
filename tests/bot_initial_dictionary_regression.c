/* Actual two-pass packed initial chats, alignment and private heap ownership. */
#define Q3_MATCH_PIECE_NO_MAIN
#include "bot_match_piece_regression.c"

static const char *initials="chat \"Other\" { skipped { nested } } chat \"Native\" { type \"First\" { \"First07\"; \"Last007\"; } type \"Second\" { \"Only007\"; } }";
static const char *nextInitial;
static int initialBytes,failInitial,initialBaseline;
static void *initialStorage;
static void *InitialHeap(int size)
{
    int stage=opens==1 && closes==1 && heapLive==initialBaseline && !numtokens && !initialBytes;
    void *pointer;
    if(stage){initialBytes=size;if(failInitial){failAt=requests+1;failInitial=0;}}
    pointer=HeapAlloc(size);if(stage)initialStorage=pointer;return pointer;
}
static int InitialOpen(const char *path,fileHandle_t *file,fsMode_t mode)
{
    if(opens==1 && nextInitial)fileText=nextInitial;return Open(path,file,mode);
}
static void InitialAttempt(const char *text)
{
    Attempt(text);nextInitial=NULL;initialBytes=failInitial=0;initialStorage=NULL;initialBaseline=heapLive;
}
static int InitialBegin(void)
{
    int handle;ChatBegin();handle=BotAllocChatState();NativeState(handle);InitialAttempt(initials);
    botimport.GetMemory=InitialHeap;botimport.FS_FOpenFile=InitialOpen;return handle;
}
static void InitialEnd(int handle)
{
    BotFreeChatFile(handle);ChatEnd();
}
static void InitialValues(bot_chat_t *root,int handle)
{
    bot_chattype_t *second;bot_chatmessage_t *line;
    Check(root && root->types && !strcmp(root->types->name,"Second") && root->types->numchatmessages==1 &&
          root->types->firstchatmessage && !strcmp(root->types->firstchatmessage->chatmessage,"Only007"),
          "native reverse initial type order and complete line values remain");
    second=root->types->next;Check(second && !strcmp(second->name,"First") && second->numchatmessages==2 && !second->next &&
          second->firstchatmessage && !strcmp(second->firstchatmessage->chatmessage,"Last007") && second->firstchatmessage->next &&
          !strcmp(second->firstchatmessage->next->chatmessage,"First07") && !second->firstchatmessage->next->next,
          "native reverse initial message order and counts remain");
    botchatstates[handle]->chat=root;
    Check(BotNumInitialChats(handle,"first")==2 && BotNumInitialChats(handle,"Second")==1,
          "actual native initial type/count public queries remain case insensitive");
    BotInitialChat(handle,"Second",0,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL);
    Check(!strcmp(botchatstates[handle]->chatmessage,"Only007") && root->types->firstchatmessage->time==145.5f,
          "actual native initial selection/construction/timing remain");
    for(second=root->types;second;second=second->next)for(line=second->firstchatmessage;line;line=line->next)line->time=-2*CHATMESSAGE_RECENTTIME;
}
static void GoldenInitials(void)
{
    char name[MAX_CHATTYPE_NAME],text[1024];int handle=InitialBegin();bot_chat_t *root=BotLoadInitialChat("native.c","nAtIvE");InitialValues(root,handle);
    Check(!errors && opens==2 && closes==2 && heapLive==2 && !hunkLive && !numtokens,
          "native two-pass initial dictionary returns one complete heap owner after source release");InitialEnd(handle);
    handle=InitialBegin();InitialAttempt("chat \"Native\" { }");root=BotLoadInitialChat("native.c","Native");
    Check(root && !root->types && !errors && heapLive==2 && !hunkLive,"native empty selected chat still returns its owned header");botchatstates[handle]->chat=root;InitialEnd(handle);
    handle=InitialBegin();InitialAttempt("chat \"Native\" { type \"Empty\" { } } chat \"Native\" { type \"Second\" { \"Only007\"; } }");root=BotLoadInitialChat("native.c","Native");
    Check(root && root->types && !strcmp(root->types->name,"Second") && root->types->next && !strcmp(root->types->next->name,"Empty") &&
          !root->types->next->numchatmessages && !root->types->next->firstchatmessage && !errors,
          "native repeated selected chat blocks and empty types remain merged in reverse order");botchatstates[handle]->chat=root;InitialEnd(handle);
    handle=InitialBegin();memset(name,'x',sizeof(name)-1);name[sizeof(name)-1]=0;
    snprintf(text,sizeof(text),"chat \"Native\" { type \"%s\" { \"Only007\"; } }",name);InitialAttempt(text);root=BotLoadInitialChat("native.c","Native");
    Check(root && root->types && !strcmp(root->types->name,name) && !errors,"native maximum terminated type name remains complete");
    botchatstates[handle]->chat=root;Check(BotNumInitialChats(handle,name)==1,"actual native boundary-name public lookup remains");InitialEnd(handle);
}
static void InitialAlignment(void)
{
    int handle=InitialBegin();bot_chat_t *root;volatile uintptr_t address;
    InitialAttempt("chat \"Native\" { type \"First\" { \"one\"; \"three\"; } type \"Second\" { \"two\"; } }");root=BotLoadInitialChat("native.c","Native");
    Check(root && root->types && root->types->firstchatmessage,"actual short initial dictionary loads");address=(uintptr_t)root->types;
    Check(address%__alignof__(bot_chattype_t)==0,"actual pointer-bearing initial type is aligned before dereference");
    address=(uintptr_t)root->types->next->firstchatmessage;
    Check(address%__alignof__(bot_chatmessage_t)==0 && !strcmp(root->types->next->firstchatmessage->chatmessage,"three") &&
          !strcmp(root->types->firstchatmessage->chatmessage,"two") && !errors,"actual short initial message records are aligned with native payload/order");
    botchatstates[handle]->chat=root;InitialEnd(handle);
}
static int SnapshotInitial(bot_chat_t *root,unsigned char *saved)
{
    int bytes=initialBytes-(int)((char *)root-(char *)initialStorage);Check(bytes>0 && bytes<4096,"actual complete packed initial payload snapshot");
    memcpy(saved,root,bytes);return bytes;
}
static void BadInitial(int kind)
{
    int handle=InitialBegin(),owners,bytes;bot_chat_t *prior,*retry;unsigned char saved[4096];char text[2200],word[1024];
    prior=BotLoadInitialChat("native.c","Native");InitialValues(prior,handle);bytes=SnapshotInitial(prior,saved);owners=heapLive;InitialAttempt(initials);
    if(kind==0)nextInitial="chat \"Native\" { type \"First\" { \"bad\"; }";
    else if(kind==1)nextInitial="chat \"Native\" { type \"First\" { \"a much larger changed dictionary line that exceeds the original measured capacity several times in total\"; \"another much larger changed dictionary line that also increases measured capacity\"; } }";
    else if(kind==2)nextInitial="chat \"Native\" { }";
    else if(kind==3)nextInitial="chat \"Other\" { }";
    else if(kind==4)nextInitial="chat \"Native\" { type \"First\" { \"First07\"; } } #unknown";
    else if(kind==5){snprintf(text,sizeof(text),"%s #unknown",initials);fileText=text;}
    else if(kind==6){snprintf(text,sizeof(text),"%s \"\\q\"",initials);fileText=text;}
    else{memset(word,'x',kind==7?MAX_CHATTYPE_NAME:1021);word[kind==7?MAX_CHATTYPE_NAME:1021]=0;
        snprintf(text,sizeof(text),"chat \"Native\" { type \"%s\" { \"Only007\"; } }",word);fileText=text;}
    Check(!BotLoadInitialChat("native.c","Native") && errors>=1 && !messages && heapLive==owners && !hunkLive && !numtokens && opens==closes &&
          botchatstates[handle]->chat==prior && !memcmp(saved,prior,bytes),"failed/changed/type-overlong initial source releases stage and preserves every prior payload/pointer byte");
    InitialValues(prior,handle);InitialAttempt(initials);retry=BotLoadInitialChat("native.c","Native");Check(retry!=NULL,"failed initial factory retries");FreeMemory(retry);InitialEnd(handle);
}
static void FailedInitialStorage(void)
{
    int handle=InitialBegin();failInitial=1;
    Check(!BotLoadInitialChat("native.c","Native") && !failInitial && failAt && requests>=failAt && errors>=1 && !messages && heapLive==1 &&
          !hunkLive && !numtokens && opens==closes && !botchatstates[handle]->chat,"actual NULL packed heap rejects before pointer arithmetic and releases source");InitialEnd(handle);
}
static void NullableInitial(int position,int priorMode)
{
    int handle=InitialBegin(),owners,bytes=0;bot_chat_t *prior=NULL,*candidate;unsigned char saved[4096];
    if(priorMode){prior=BotLoadInitialChat("native.c","Native");InitialValues(prior,handle);bytes=SnapshotInitial(prior,saved);}
    owners=heapLive;InitialAttempt(initials);failAt=position;candidate=BotLoadInitialChat("native.c","Native");
    Check(!candidate && errors>=1 && requests>=failAt && !messages && heapLive==owners && !hunkLive && !numtokens && opens==closes &&
          botchatstates[handle]->chat==prior,"every nullable pass/source/packed-owner import rejects with physical cleanup and no pending root changes");
    if(prior){Check(!memcmp(saved,prior,bytes),"nullable initial factory preserves complete prior payload/pointer bytes");InitialValues(prior,handle);}
    InitialAttempt(initials);candidate=BotLoadInitialChat("native.c","Native");Check(candidate!=NULL,"nullable initial factory retries");
    if(prior)FreeMemory(prior);InitialValues(candidate,handle);InitialEnd(handle);
}
int main(int argc,char **argv)
{
    int handle,count,i,mode;bot_chat_t *root;
    if(argc>1){i=atoi(argv[1]);if(i<9)BadInitial(i);else if(i==9)InitialAlignment();else if(i==10)FailedInitialStorage();else GoldenInitials();return 0;}
    GoldenInitials();InitialAlignment();FailedInitialStorage();for(i=0;i<9;i++)BadInitial(i);
    handle=InitialBegin();root=BotLoadInitialChat("native.c","Native");InitialValues(root,handle);count=requests;InitialEnd(handle);
    for(mode=0;mode<2;mode++)for(i=1;i<=count;i++)NullableInitial(i,mode);
    printf("Actual aligned complete initial dictionaries, %d nullable imports, two-pass/source rollback, native selection/timing and physical ownership (issue #48)\n",count*2);
    return 0;
}
