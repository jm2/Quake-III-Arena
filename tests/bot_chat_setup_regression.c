/* Actual complete chat-library setup with private candidates and live queues. */
#define Q3_MATCH_PIECE_NO_MAIN
#include "bot_match_piece_regression.c"

static const char *nativeFiles[]={"1 { [(\"hi\",1),(\"hello\",2)] }","ONE = { \"Native\"; }","1 { \"hello \",0 = (2,3); }","[\"Native\"] = 2 { \"reply\"; }"};
static const char *setupFiles[4];
static int missingFile,failPool;
static void *SetupHunk(int size)
{
    if(failPool){failAt=requests+1;failPool=0;}return HunkAlloc(size);
}
static int SetupOpen(const char *path,fileHandle_t *file,fsMode_t mode)
{
    int i;const char *names[]={"syn.c","rnd.c","match.c","rchat.c"};
    for(i=0;i<4;i++)if(strstr(path,names[i]))break;Check(i<4,"actual setup requests a configured dictionary source");
    if(i==missingFile){opens++;*file=0;return -1;}fileText=setupFiles[i];return Open(path,file,mode);
}
static void SetupAttempt(void)
{
    int i;Attempt("");for(i=0;i<4;i++)setupFiles[i]=nativeFiles[i];missingFile=-1;failPool=0;
}
static int SetupBegin(void)
{
    int handle;ChatBegin();Check(LibVar("synfile","syn.c") && LibVar("rndfile","rnd.c") && LibVar("matchfile","match.c") &&
          LibVar("rchatfile","rchat.c") && LibVar("nochat","0") && LibVar("max_messages","4"),"complete native configuration owners");
    handle=BotAllocChatState();NativeState(handle);SetupAttempt();botimport.FS_FOpenFile=SetupOpen;botimport.HunkAlloc=SetupHunk;return handle;
}
static void SetupValues(int handle)
{
    bot_match_t match;char text[32]="hello";
    Check(synonyms && synonyms->context==1 && synonyms->firstsynonym && !strcmp(synonyms->firstsynonym->string,"hi") &&
          synonyms->firstsynonym->next && !strcmp(synonyms->firstsynonym->next->string,"hello") && synonyms->totalweight==3,
          "complete native synonym contexts/weights/order remain");
    BotReplaceSynonyms(text,1);Check(!strcmp(text,"hi"),"actual ABI-safe public synonym replacement remains native");
    Check(randomstrings && !strcmp(RandomString("ONE"),"Native"),"actual native random dictionary lookup remains");
    Check(matchtemplates && BotFindMatch("hello Native",&match,1) && match.type==2 && match.subtype==3 &&
          match.variables[0].offset==6 && match.variables[0].length==6,"actual native public template lookup/capture remains");
    Check(replychats && BotReplyChat(handle,"hello Native",0,0,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL) &&
          !strcmp(botchatstates[handle]->chatmessage,"reply") && replychats->firstchatmessage->time==145.5f,
          "actual native public reply selection/construction/timing remains");replychats->firstchatmessage->time=-2*CHATMESSAGE_RECENTTIME;
}
static void GoldenSetup(void)
{
    int handle=SetupBegin();Check(BotSetupChatAI()==BLERR_NOERROR && !errors && opens==6 && closes==6 && !numtokens,
          "native complete configured library setup succeeds and releases every source");SetupValues(handle);
    Check(consolemessageheap && consolemessageheapcount==4 && freeconsolemessages==consolemessageheap,"native complete console pool remains configured");ChatEnd();
    handle=SetupBegin();{int i;for(i=0;i<4;i++)setupFiles[i]="";}
    Check(BotSetupChatAI()==BLERR_NOERROR && !errors && !synonyms && !randomstrings && !matchtemplates && !replychats &&
          consolemessageheap && opens==closes && !numtokens,"valid empty optional files remain distinct from failed setup");ChatEnd();
    handle=SetupBegin();LibVarSet("nochat","1");SetupAttempt();missingFile=3;
    Check(BotSetupChatAI()==BLERR_NOERROR && !errors && synonyms && randomstrings && matchtemplates && !replychats && opens==5 && closes==5,
          "native disabled replies do not request an optional reply file");ChatEnd();
}
static void QueueValues(int handle)
{
    bot_consolemessage_t message;
    Check(BotNumConsoleMessages(handle)==2 && BotNextConsoleMessage(handle,&message)==1 && message.time==125.5f && message.type==7 &&
          !strcmp(message.message,"first"),"live native first queue values survive complete setup");
}
static int PriorSetup(void)
{
    int handle=SetupBegin();Check(BotSetupChatAI()==BLERR_NOERROR,"prior actual complete library setup");SetupValues(handle);
    BotQueueConsoleMessage(handle,7,"first");BotQueueConsoleMessage(handle,8,"second");QueueValues(handle);return handle;
}
typedef struct {
    bot_synonymlist_t *syn;bot_randomlist_t *random;bot_matchtemplate_t *matches;bot_replychat_t *replies;
    bot_synonymlist_t synHeader;bot_synonym_t synonym;
    bot_randomlist_t randomHeader;bot_randomstring_t randomString;
    bot_matchtemplate_t matchHeader;bot_matchpiece_t piece;bot_matchstring_t matchString;
    bot_replychat_t replyHeader;bot_replychatkey_t key;bot_chatmessage_t line;
    bot_consolemessage_t *pool,*freeRoot,poolBytes[4];bot_chatstate_t state;int owners,hunks;
} setup_snapshot_t;
static void SaveSetup(setup_snapshot_t *saved,int handle)
{
    saved->syn=synonyms;saved->random=randomstrings;saved->matches=matchtemplates;saved->replies=replychats;
    saved->synHeader=*synonyms;saved->synonym=*synonyms->firstsynonym;
    saved->randomHeader=*randomstrings;saved->randomString=*randomstrings->firstrandomstring;
    saved->matchHeader=*matchtemplates;saved->piece=*matchtemplates->first;saved->matchString=*matchtemplates->first->firststring;
    saved->replyHeader=*replychats;saved->key=*replychats->keys;saved->line=*replychats->firstchatmessage;
    saved->pool=consolemessageheap;saved->freeRoot=freeconsolemessages;memcpy(saved->poolBytes,consolemessageheap,sizeof(saved->poolBytes));
    saved->state=*botchatstates[handle];saved->owners=heapLive;saved->hunks=hunkLive;
}
static void SameSetup(setup_snapshot_t *saved,int handle)
{
    Check(synonyms==saved->syn && randomstrings==saved->random && matchtemplates==saved->matches && replychats==saved->replies &&
          consolemessageheap==saved->pool && freeconsolemessages==saved->freeRoot && heapLive==saved->owners && hunkLive==saved->hunks &&
          !numtokens && !memcmp(saved->poolBytes,consolemessageheap,sizeof(saved->poolBytes)) &&
          !memcmp(&saved->state,botchatstates[handle],sizeof(saved->state)),"failed setup preserves every prior dictionary/pool/queue root, state/pool byte and physical owner count");
    Check(!memcmp(&saved->synHeader,synonyms,sizeof(saved->synHeader)) && !memcmp(&saved->synonym,synonyms->firstsynonym,sizeof(saved->synonym)) &&
          !memcmp(&saved->randomHeader,randomstrings,sizeof(saved->randomHeader)) && !memcmp(&saved->randomString,randomstrings->firstrandomstring,sizeof(saved->randomString)) &&
          !memcmp(&saved->matchHeader,matchtemplates,sizeof(saved->matchHeader)) && !memcmp(&saved->piece,matchtemplates->first,sizeof(saved->piece)) &&
          !memcmp(&saved->matchString,matchtemplates->first->firststring,sizeof(saved->matchString)) && !memcmp(&saved->replyHeader,replychats,sizeof(saved->replyHeader)) &&
          !memcmp(&saved->key,replychats->keys,sizeof(saved->key)) && !memcmp(&saved->line,replychats->firstchatmessage,sizeof(saved->line)),
          "all prior dictionary record/pointer/timing bytes remain");QueueValues(handle);SetupValues(handle);
}
static void BadSetup(int kind)
{
    int handle=PriorSetup(),index=kind%4;setup_snapshot_t saved;SaveSetup(&saved,handle);SetupAttempt();
    if(kind<4)setupFiles[index]=index==0?"1 { [(\"hi\",1)] }":index==1?"ONE = { \"Native\";":index==2?"1 { \"hello \",0 = (2,3);":"[\"Native\"] = 2 { \"reply\";";
    else missingFile=index;
    Check(BotSetupChatAI()==BLERR_LIBRARYNOTSETUP && errors>=1,"each malformed/missing dictionary propagates failed complete setup");SameSetup(&saved,handle);
    SetupAttempt();Check(BotSetupChatAI()==BLERR_NOERROR && !errors,"failed setup retries as a complete group");QueueValues(handle);SetupValues(handle);ChatEnd();
}
static void NullableSetup(int position,int priorMode)
{
    int handle=priorMode?PriorSetup():SetupBegin(),owners=heapLive,hunks=hunkLive;setup_snapshot_t saved;
    if(priorMode)SaveSetup(&saved,handle);SetupAttempt();failAt=position;
    Check(BotSetupChatAI()==BLERR_LIBRARYNOTSETUP && errors>=1 && requests>=failAt && heapLive==owners && hunkLive==hunks && !numtokens,
          "every nullable dictionary/source/final-pool import rejects, physically releases candidates and spends no replacement hunk");
    if(priorMode)SameSetup(&saved,handle);else Check(!synonyms && !randomstrings && !matchtemplates && !replychats && !consolemessageheap &&
          !freeconsolemessages && !consolemessageheapcount,"fresh failure publishes no dictionary/pool roots");
    SetupAttempt();Check(BotSetupChatAI()==BLERR_NOERROR && !errors,"nullable complete setup retries");SetupValues(handle);if(priorMode)QueueValues(handle);ChatEnd();
}
static void FinalPoolFailure(void)
{
    int handle=PriorSetup();setup_snapshot_t saved;SaveSetup(&saved,handle);SetupAttempt();failPool=1;
    Check(BotSetupChatAI()==BLERR_LIBRARYNOTSETUP && !failPool && errors>=1 && opens==6 && closes==6,
          "actual final NULL pool import rejects after dictionary sources complete");SameSetup(&saved,handle);ChatEnd();
}
int main(int argc,char **argv)
{
    int handle,counts[2],i,mode;
    if(argc>1){i=atoi(argv[1]);if(i<8)BadSetup(i);else GoldenSetup();return 0;}
    GoldenSetup();FinalPoolFailure();for(i=0;i<8;i++)BadSetup(i);
    for(mode=0;mode<2;mode++){
        handle=mode?PriorSetup():SetupBegin();SetupAttempt();Check(BotSetupChatAI()==BLERR_NOERROR,"actual complete setup nullable baseline");
        counts[mode]=requests;SetupValues(handle);ChatEnd();
    }
    for(mode=0;mode<2;mode++)for(i=1;i<=counts[mode];i++)NullableSetup(i,mode);
    printf("Actual complete chat library, %d nullable imports, dictionary/source/final-pool rollback, live queues, native empty/disabled data and public behavior (issue #48)\n",counts[0]+counts[1]);return 0;
}
