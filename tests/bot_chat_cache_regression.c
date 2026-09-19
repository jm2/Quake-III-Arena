/* Actual cached/private chat ownership, replacement and full cache lifecycle. */
#define Q3_ITEM_TEST_HEAP_CAPACITY 512
#define Q3_MATCH_PIECE_NO_MAIN
#include "bot_match_piece_regression.c"

static const char *cacheText="chat \"Native\" { type \"Type\" { \"Only007\"; } }";
static int CacheBegin(int mode)
{
    int handle;ChatBegin();Check(LibVar("bot_reloadcharacters",mode?"1":"0")!=NULL,"actual native cache policy variable");
    handle=BotAllocChatState();NativeState(handle);Attempt(cacheText);return handle;
}
static void CacheValues(int handle)
{
    bot_chat_t *chat=botchatstates[handle]->chat;
    Check(chat && chat->types && !strcmp(chat->types->name,"Type") && chat->types->numchatmessages==1 &&
          chat->types->firstchatmessage && !strcmp(chat->types->firstchatmessage->chatmessage,"Only007") &&
          BotNumInitialChats(handle,"Type")==1,"complete native cached/private payload and public lookup remain");
}
static void GoldenCache(void)
{
    int handle=CacheBegin(0),second=BotAllocChatState(),owners;bot_chat_t *chat;
    Check(BotLoadChatFile(handle,"native.c","Native")==BLERR_NOERROR,"native first cached load");CacheValues(handle);
    chat=botchatstates[handle]->chat;Check(ichatdata[0] && ichatdata[0]->chat==chat && !strcmp(ichatdata[0]->filename,"native.c") &&
          !strcmp(ichatdata[0]->chatname,"Native") && !ichatdata[1] && !errors,"native complete cache key/root values");
    owners=heapLive;Attempt(cacheText);Check(BotLoadChatFile(second,"native.c","Native")==BLERR_NOERROR &&
          botchatstates[second]->chat==chat && !requests && !opens && heapLive==owners && !errors,"native second state reuses complete cache without imports");CacheValues(second);ChatEnd();
    handle=CacheBegin(1);Check(BotLoadChatFile(handle,"native.c","Native")==BLERR_NOERROR && !ichatdata[0],"native reload mode has private ownership");
    CacheValues(handle);BotFreeChatFile(handle);Check(!botchatstates[handle]->chat && heapLive==3,"native private public file-free releases physical data");ChatEnd();
}
static void CachedDetach(int mode)
{
    int handle=CacheBegin(0),second=BotAllocChatState(),owners;bot_chat_t *chat;bot_chat_t header;bot_chattype_t type;bot_chatmessage_t line;
    Check(BotLoadChatFile(handle,"native.c","Native")==BLERR_NOERROR && BotLoadChatFile(second,"native.c","Native")==BLERR_NOERROR,"two actual cached aliases load");
    chat=botchatstates[handle]->chat;header=*chat;type=*chat->types;line=*chat->types->firstchatmessage;owners=heapLive;Attempt(cacheText);
    if(mode==0)BotFreeChatFile(handle);
    else if(mode==1){Check(BotLoadChatFile(handle,"native.c","Native")==BLERR_NOERROR,"same cached key reload remains native");}
    else{LibVarSet("bot_reloadcharacters","1");owners=heapLive;Attempt(cacheText);BotFreeChatState(handle);}
    Check(heapLive==owners-(mode==2) && botchatstates[second]->chat==chat && ichatdata[0] && ichatdata[0]->chat==chat &&
          !memcmp(&header,chat,sizeof(header)) && !memcmp(&type,chat->types,sizeof(type)) && !memcmp(&line,chat->types->firstchatmessage,sizeof(line)),
          "detach/same-key/policy-change state free preserves complete cached payload/pointers and sibling alias");CacheValues(second);
    if(mode==0){Check(!botchatstates[handle]->chat,"public free detaches only its handle");Attempt(cacheText);
        Check(BotLoadChatFile(handle,"native.c","Native")==BLERR_NOERROR && !requests && !opens && botchatstates[handle]->chat==chat,"detached cached handle reattaches without imports");}
    ChatEnd();
}
static void PrivateToggle(void)
{
    int handle=CacheBegin(1),owners;Check(BotLoadChatFile(handle,"native.c","Native")==BLERR_NOERROR,"native private chat loads before policy change");
    LibVarSet("bot_reloadcharacters","0");owners=heapLive;Attempt(cacheText);BotFreeChatState(handle);
    Check(!botchatstates[handle] && heapLive==owners-2 && !ichatdata[0] && !requests,"private data releases physically after reload policy turns off");ChatEnd();
}
static void BadReplacement(int cached)
{
    int handle=CacheBegin(!cached),owners;bot_chat_t *prior,header;bot_chattype_t type;bot_chatmessage_t line;
    Check(BotLoadChatFile(handle,"native.c","Native")==BLERR_NOERROR,"native prior chat loads");prior=botchatstates[handle]->chat;
    header=*prior;type=*prior->types;line=*prior->types->firstchatmessage;owners=heapLive;Attempt("chat \"Native\" { type \"Type\" { \"bad\";");
    Check(BotLoadChatFile(handle,"failed.c","Native")==BLERR_CANNOTLOADICHAT && errors>=1 && !messages && heapLive==owners &&
          !numtokens && opens==closes && botchatstates[handle]->chat==prior && !memcmp(&header,prior,sizeof(header)) &&
          !memcmp(&type,prior->types,sizeof(type)) && !memcmp(&line,prior->types->firstchatmessage,sizeof(line)),
          "failed replacement preserves complete prior owner/pointers/line timing and releases staged cache/source data");
    CacheValues(handle);Attempt(cacheText);Check(BotLoadChatFile(handle,"retry.c","Native")==BLERR_NOERROR,"failed replacement retries");CacheValues(handle);ChatEnd();
}
static void NullableReplacement(int position,int priorMode,int cached)
{
    int handle=CacheBegin(!cached),owners;bot_chat_t *prior=NULL,header;bot_chattype_t type;bot_chatmessage_t line;bot_ichatdata_t *cachedRoot=NULL,cacheHeader;
    if(priorMode){Check(BotLoadChatFile(handle,"prior.c","Native")==BLERR_NOERROR,"prior owner before nullable replacement");prior=botchatstates[handle]->chat;
        header=*prior;type=*prior->types;line=*prior->types->firstchatmessage;if(cached){cachedRoot=ichatdata[0];cacheHeader=*cachedRoot;}}
    owners=heapLive;Attempt(cacheText);failAt=position;
    Check(BotLoadChatFile(handle,"candidate.c","Native")==BLERR_CANNOTLOADICHAT && errors>=1 && requests>=failAt && !messages &&
          heapLive==owners && !numtokens && opens==closes && !hunkLive && botchatstates[handle]->chat==prior,
          "every nullable private/cache/source/packed import rejects with physical cleanup and preserves pending handle root");
    if(prior){Check(!memcmp(&header,prior,sizeof(header)) && !memcmp(&type,prior->types,sizeof(type)) &&
          !memcmp(&line,prior->types->firstchatmessage,sizeof(line)),"nullable replacement preserves all prior header/type/line bytes and pointers");CacheValues(handle);}
    if(cachedRoot)Check(ichatdata[0]==cachedRoot && !memcmp(&cacheHeader,cachedRoot,sizeof(cacheHeader)) && !ichatdata[1],"nullable replacement preserves complete prior cache key/root bytes");
    else Check(!ichatdata[0],"nullable candidate publishes no incomplete cache root");
    Attempt(cacheText);Check(BotLoadChatFile(handle,"candidate.c","Native")==BLERR_NOERROR,"nullable public replacement retries");CacheValues(handle);ChatEnd();
}
static void FullCache(void)
{
    int handle=CacheBegin(0),i,owners;char filename[32];bot_chat_t *prior;bot_ichatdata_t *entries[MAX_CLIENTS];
    for(i=0;i<MAX_CLIENTS;i++){Attempt(cacheText);snprintf(filename,sizeof(filename),"native%d.c",i);
        Check(BotLoadChatFile(handle,filename,"Native")==BLERR_NOERROR && ichatdata[i],"actual public loads fill every native cache slot");entries[i]=ichatdata[i];}
    owners=heapLive;prior=botchatstates[handle]->chat;Attempt(cacheText);
    Check(BotLoadChatFile(handle,"overflow.c","Native")==BLERR_CANNOTLOADICHAT && errors==1 && !requests && !opens &&
          heapLive==owners && botchatstates[handle]->chat==prior,"native full cache rejects before imports while preserving prior handle ownership");
    for(i=0;i<MAX_CLIENTS;i++)Check(ichatdata[i]==entries[i],"full cache failure preserves every actual cache root");
    Attempt(cacheText);Check(BotLoadChatFile(handle,"native0.c","Native")==BLERR_NOERROR && !requests && !opens &&
          botchatstates[handle]->chat==ichatdata[0]->chat,"native existing key still reuses its owner at full capacity");ChatEnd();
}
static void LongName(int length,int golden)
{
    int handle=CacheBegin(0),owners;char name[1024],text[2200];bot_chat_t *prior;
    memset(name,'x',length);name[length]=0;snprintf(text,sizeof(text),"chat \"%s\" { type \"Type\" { \"Only007\"; } }",name);Attempt(text);
    Check(BotLoadChatFile(handle,"native.c",name)==BLERR_NOERROR,"native longer selected names still load without cache truncation");CacheValues(handle);prior=botchatstates[handle]->chat;
    if(!golden){Check(!ichatdata[0],"non-fitting cache keys have explicit private ownership");owners=heapLive;BotFreeChatState(handle);
        Check(heapLive==owners-2,"long-key private chat physically releases under default cache policy");}
    else Check(ichatdata[0] && !strcmp(ichatdata[0]->chatname,name) && ichatdata[0]->chat==prior,"maximum native cache key remains complete");ChatEnd();
}
static void *cacheProbeStorage;
static int failedCacheSize,armCache;
static void *CacheProbeHeap(int size)
{
    cacheProbeStorage=HeapAlloc(size);return cacheProbeStorage;
}
static void *NullableCacheHeap(int size)
{
    if(armCache && size==failedCacheSize){failAt=requests+1;armCache=0;}return HeapAlloc(size);
}
static void FailedCacheEntry(void)
{
    int handle=CacheBegin(0);void *probe;botimport.GetMemory=CacheProbeHeap;
    probe=GetMemory(0);Check(probe && cacheProbeStorage,"actual cache ownership prefix probe");
    failedCacheSize=(int)sizeof(bot_ichatdata_t)+(int)((char *)probe-(char *)cacheProbeStorage);FreeMemory(probe);
    Attempt(cacheText);armCache=1;botimport.GetMemory=NullableCacheHeap;
    Check(BotLoadChatFile(handle,"native.c","Native")==BLERR_CANNOTLOADICHAT && !armCache && requests>=failAt && errors>=1 &&
          !messages && heapLive==3 && !numtokens && !hunkLive && opens==closes && !ichatdata[0] && !botchatstates[handle]->chat,
          "actual NULL cache header import rejects, releases candidate/source owners and publishes no incomplete roots");ChatEnd();
}
static void MissingCacheInput(int mode)
{
    int handle=CacheBegin(0),owners;bot_chat_t *prior;
    Check(BotLoadChatFile(handle,"native.c","Native")==BLERR_NOERROR,"native prior before missing input");prior=botchatstates[handle]->chat;owners=heapLive;Attempt(cacheText);
    Check(BotLoadChatFile(handle,mode==0?NULL:mode==1?"":"candidate.c",mode==2?NULL:"Native")==BLERR_CANNOTLOADICHAT &&
          errors==1 && !requests && !opens && heapLive==owners && botchatstates[handle]->chat==prior,"missing input rejects before owner/root mutation");CacheValues(handle);ChatEnd();
}
int main(int argc,char **argv)
{
    int handle,count,cached,mode,i;
    if(argc>1){i=atoi(argv[1]);if(i<3)CachedDetach(i);else if(i==3)PrivateToggle();else if(i<6)BadReplacement(i-4);else if(i==6)FullCache();
        else if(i==7)LongName(64,0);else if(i<11)MissingCacheInput(i-8);else if(i==11)FailedCacheEntry();else{GoldenCache();LongName(63,1);}return 0;}
    GoldenCache();FailedCacheEntry();for(i=0;i<3;i++)CachedDetach(i);PrivateToggle();BadReplacement(0);BadReplacement(1);FullCache();LongName(63,1);LongName(64,0);LongName(100,0);
    for(i=0;i<3;i++)MissingCacheInput(i);
    for(cached=0;cached<2;cached++){handle=CacheBegin(!cached);Check(BotLoadChatFile(handle,"candidate.c","Native")==BLERR_NOERROR,"actual nullable public path baseline");count=requests;ChatEnd();
        for(mode=0;mode<2;mode++)for(i=1;i<=count;i++)NullableReplacement(i,mode,cached);printf("Actual %s cache-policy replacement checks: %d nullable imports\n",cached?"shared":"private",count*2);}
    puts("Actual cache aliases/detach, policy changes, replacement/full-capacity/long keys and native lookup retain physical ownership (issue #48)");return 0;
}
