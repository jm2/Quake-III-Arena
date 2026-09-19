/* Actual two-pass random dictionary, rebasing and physical ownership. */
#define Q3_CHAT_CONSUMER_NO_MAIN
#include "bot_chat_consumer_regression.c"

static const char *dictionary="Alpha07 = { \"First07\"; \"Last007\"; } Beta007 = { \"Only007\"; }";
static const char *secondText;
static int hunkBytes, failArena;
static void *DictionaryHunk(int size)
{
    hunkBytes=size;
    if (failArena) { failAt=requests+1;failArena=0; }
    return HunkAlloc(size);
}
static int DictionaryOpen(const char *path,fileHandle_t *file,fsMode_t mode)
{
    if (opens==1 && secondText) fileText=secondText;
    return Open(path,file,mode);
}
static void RandomBegin(void)
{
    ChatBegin();Attempt(dictionary);secondText=NULL;hunkBytes=failArena=0;
    botimport.HunkAlloc=DictionaryHunk;botimport.FS_FOpenFile=DictionaryOpen;
}
static void Values(bot_randomlist_t *root)
{
    bot_randomlist_t *second;
    Check(root && !strcmp(root->string,"Alpha07") && root->numstrings==2,"native first random group and count");
    Check(root->firstrandomstring && !strcmp(root->firstrandomstring->string,"Last007") &&
          root->firstrandomstring->next && !strcmp(root->firstrandomstring->next->string,"First07") &&
          !root->firstrandomstring->next->next,"native reverse message order remains");
    second=root->next;
    Check(second && !strcmp(second->string,"Beta007") && second->numstrings==1 && !second->next &&
          second->firstrandomstring && !strcmp(second->firstrandomstring->string,"Only007") &&
          !second->firstrandomstring->next,"native forward group order and second complete message");
}
static void GoldenRandom(void)
{
    RandomBegin();randomstrings=BotLoadRandomStrings("native.c");Values(randomstrings);
    Check(!heapLive && hunkLive==1 && opens==2 && closes==2 && !errors && messages==1 &&
          !strcmp(RandomString("Beta007"),"Only007"),"actual native two-pass lookup and complete source release");
    ChatEnd();
}
static void EmptyRandom(void)
{
    RandomBegin();Attempt("");Check(!BotLoadRandomStrings("native.c") && !errors && !heapLive && !hunkLive && opens==2 && closes==2,
          "native empty optional dictionary consumes no persistent memory");ChatEnd();
    RandomBegin();Attempt("Empty07 = { }");randomstrings=BotLoadRandomStrings("native.c");
    Check(randomstrings && !randomstrings->numstrings && !randomstrings->firstrandomstring && !randomstrings->next &&
          !RandomString("Empty07") && !errors && !heapLive && hunkLive==1,"native zero-message group remains complete");ChatEnd();
}
static void ShortAlignment(void)
{
    RandomBegin();Attempt("A = { \"one\"; \"three\"; } B = { \"two\"; }");randomstrings=BotLoadRandomStrings("native.c");
    { volatile uintptr_t address=(uintptr_t)randomstrings->firstrandomstring;
      Check(address % __alignof__(bot_randomstring_t)==0,"actual pointer-bearing message storage is aligned before client dereference"); }
    Check(randomstrings && randomstrings->numstrings==2 && randomstrings->next && randomstrings->next->numstrings==1 &&
          !strcmp(randomstrings->firstrandomstring->string,"three") &&
          !strcmp(RandomString("B"),"two") && !errors && !heapLive,"short native strings keep aligned pointer-bearing records and rebased lookup");ChatEnd();
}
static void RandomNullable(int position,int priorMode)
{
    bot_randomlist_t *prior=NULL,*candidate;
    unsigned char saved[2048];int bytes=0;
    RandomBegin();
    if(priorMode){prior=BotLoadRandomStrings("native.c");Values(prior);randomstrings=prior;
        bytes=hunkBytes-(int)((char *)prior-(char *)hunk[0]);Check(bytes>0&&bytes<(int)sizeof(saved),"complete prior physical dictionary payload");memcpy(saved,prior,bytes);}
    Attempt(dictionary);failAt=position;candidate=BotLoadRandomStrings("native.c");
    Check(!candidate && errors>=1 && requests>=position && !heapLive && hunkLive==priorMode && randomstrings==prior,
          "every nullable stage/source/publish import rejects without extra persistent memory or roots");
    if(priorMode)Check(!memcmp(saved,prior,bytes) && !strcmp(RandomString("Beta007"),"Only007"),"nullable replacement preserves all prior pointers/payload bytes and lookup");
    Attempt(dictionary);candidate=BotLoadRandomStrings("native.c");Values(candidate);
    if(prior)FreeMemory(prior);randomstrings=candidate;ChatEnd();
}
static void BadSource(int kind)
{
    bot_randomlist_t *prior,*retry;
    unsigned char saved[2048];int bytes;
    RandomBegin();prior=BotLoadRandomStrings("native.c");Values(prior);randomstrings=prior;
    bytes=hunkBytes-(int)((char *)prior-(char *)hunk[0]);Check(bytes>0&&bytes<(int)sizeof(saved),"complete native prior payload snapshot");memcpy(saved,prior,bytes);
    Attempt(dictionary);
    if(kind==0)secondText="Alpha07 = { \"bad\";";
    else if(kind==1)secondText="Alpha07 = { \"changed dictionary message much larger than both original short lines combined\"; \"another much larger message\"; } Beta007 = { \"Only007\"; }";
    else if(kind==2)secondText="";
    else if(kind==3)secondText="Alpha07 = { \"First07\"; } #unknown";
    else if(kind==4)fileText="Alpha07 = { \"First07\"; } #unknown";
    else fileText="Alpha07 = { \"First07\"; } \"\\q\"";
    Check(!BotLoadRandomStrings("native.c") && errors>=1 && !heapLive && hunkLive==1 && randomstrings==prior &&
          !memcmp(saved,prior,bytes),"malformed/changed/suffix/lexical dictionaries keep every prior byte and spend no persistent replacement arena");
    secondText=NULL;Attempt(dictionary);retry=BotLoadRandomStrings("native.c");Values(retry);FreeMemory(retry);
    ChatEnd();
}
static void FailedArena(void)
{
    RandomBegin();failArena=1;Check(!BotLoadRandomStrings("native.c") && errors>=1 && !heapLive && !hunkLive,
          "failed persistent import rejects after complete staging and frees every private owner");ChatEnd();
}
static void InvalidPath(int kind)
{
    char path[MAX_PATH+1];RandomBegin();memset(path,'x',sizeof(path));path[sizeof(path)-1]=0;
    Check(!BotLoadRandomStrings(kind==0?NULL:kind==1?"":path) && errors==1 && !requests && !opens && !heapLive && !hunkLive,
          "invalid full filename rejects before imports");ChatEnd();
}
int main(int argc,char **argv)
{
    int proof,count,mode,i;
    if(argc>1){proof=atoi(argv[1]);if(proof<6)BadSource(proof);else if(proof==6)FailedArena();else if(proof==7)ShortAlignment();else if(proof==8)InvalidPath(0);else{GoldenRandom();EmptyRandom();InvalidPath(0);}return 0;}
    GoldenRandom();EmptyRandom();ShortAlignment();FailedArena();for(i=0;i<3;i++)InvalidPath(i);for(i=0;i<6;i++)BadSource(i);
    RandomBegin();randomstrings=BotLoadRandomStrings("native.c");Values(randomstrings);count=requests;ChatEnd();
    for(mode=0;mode<2;mode++)for(i=1;i<=count;i++)RandomNullable(i,mode);
    printf("Actual complete aligned random dictionaries, %d nullable imports, second-pass/source rollback, native order/lookup and physical ownership (issue #48)\n",count*2);
    return 0;
}
