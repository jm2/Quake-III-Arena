/* Actual native character cache hits must still resolve once all 64 handles are occupied. */
#define main CharacterFixtureMain
#include "bot_character_regression.c"
#undef main
#define CHARACTER_BYTES (sizeof(bot_character_t)+MAX_CHARACTERISTICS*sizeof(bot_characteristic_t))
static const char *cacheText="skill 4 { 0 42 1 1.25 79 \"native\" }";
static bot_character_t *cacheRoots[MAX_CLIENTS+1];
static unsigned char cacheBytes[MAX_CLIENTS+1][CHARACTER_BYTES];
static void CacheDone(void){BotShutdownCharacters();LibVarDeAllocAll();Check(!liveOwners&&!numtokens,"shutdown physically releases every cached character/string owner");}
static void CacheValues(int handle){char text[16];Check(handle>0&&handle<=MAX_CLIENTS&&botcharacters[handle]&&botcharacters[handle]->skill==4&&Characteristic_Integer(handle,0)==42&&Characteristic_Float(handle,1)==1.25f,"cached native skill/integer/float fields");Characteristic_String(handle,79,text,sizeof(text));Check(!strcmp(text,"native"),"cached native characteristic 79 string");}
static void CacheSnapshot(void){int i;memcpy(cacheRoots,botcharacters,sizeof(cacheRoots));for(i=1;i<=MAX_CLIENTS;i++){Check(botcharacters[i]!=NULL,"every native handle is occupied");memcpy(cacheBytes[i],botcharacters[i],CHARACTER_BYTES);}}
static void CacheUnchanged(void){int i;Check(!memcmp(cacheRoots,botcharacters,sizeof(cacheRoots)),"full-cache lookup keeps every cached root");for(i=1;i<=MAX_CLIENTS;i++)Check(!memcmp(cacheBytes[i],botcharacters[i],CHARACTER_BYTES)&&!strcmp(botcharacters[i]->c[79].value.string,"native"),"full-cache lookup keeps every header/field/string byte");}
/* Fill all 64 handles through the internal cached loader with distinct files. */
static void FillCached(void){int i;char path[MAX_QPATH];Reset(cacheText);for(i=1;i<=MAX_CLIENTS;i++){snprintf(path,sizeof(path),"bots/cache%d.c",i);requests=0;Check(BotLoadCachedCharacter(path,4,0)==i&&!errors,"cold cached loads fill native handles in order");CacheValues(i);}CacheSnapshot();}
static void CachedHit(int kind){
    int handle,before,beforeOpens,beforeOwners;char path[MAX_QPATH];float skill;
    FillCached();handle=kind==0?1:kind==1?MAX_CLIENTS:17;skill=kind==2?-1:kind==3?4.005f:4;snprintf(path,sizeof(path),"bots/cache%d.c",handle);
    requests=0;before=requests;beforeOpens=opens;beforeOwners=liveOwners;
    Check(BotLoadCachedCharacter(path,skill,0)==handle,"full cache still returns an existing exact/any-skill/tolerance hit");
    Check(requests==before&&opens==beforeOpens&&liveOwners==beforeOwners&&!errors,"full-cache hit performs no file or heap import");CacheValues(handle);CacheUnchanged();
    Check(!BotLoadCachedCharacter(path,4,1)&&!BotLoadCachedCharacter("bots/missing.c",4,0)&&requests==before&&opens==beforeOpens&&liveOwners==beforeOwners,"full cache still rejects reload and new files before imports");CacheUnchanged();
    BotFreeCharacter2(MAX_CLIENTS);requests=0;Check(BotLoadCachedCharacter("bots/retry.c",4,0)==MAX_CLIENTS&&!errors,"a freed final handle is reused by a new file");CacheValues(MAX_CLIENTS);CacheDone();
}
/* Retail game path: trap_BotLoadCharacter loads bots/default_c.c then the bot file through the cache. */
static void PublicHit(void){
    int i,handle,before,beforeOpens,beforeOwners;char path[MAX_QPATH];Reset(cacheText);
    for(i=1;i<MAX_CLIENTS;i++){snprintf(path,sizeof(path),"bots/public%d.c",i);requests=0;Check(BotLoadCharacter(path,4)==i+1&&!errors,"public loads cache the default once and one character per file");CacheValues(i+1);}
    Check(!strcmp(botcharacters[1]->filename,"bots/default_c.c"),"default character occupies the first native handle");CacheSnapshot();
    requests=0;before=requests;beforeOpens=opens;beforeOwners=liveOwners;handle=BotLoadCharacter("bots/public10.c",4);
    Check(handle==11,"re-adding an already cached bot succeeds with a full cache");
    Check(requests==before&&opens==beforeOpens&&liveOwners==beforeOwners&&!errors,"public full-cache hit performs no file or heap import");CacheValues(handle);CacheUnchanged();
    Check(!BotLoadCharacter("bots/new.c",4)&&opens==beforeOpens&&liveOwners==beforeOwners,"a new bot file still fails when every handle is occupied");CacheUnchanged();CacheDone();
}
static void Ordinary(void){
    int handle,before;Reset(cacheText);handle=BotLoadCachedCharacter("bots/native.c",4,0);Check(handle==1&&opens==1&&closes==1&&liveOwners==2&&!errors&&!numtokens,"native cold cached load");CacheValues(handle);
    requests=0;before=opens;Check(BotLoadCachedCharacter("bots/native.c",4,0)==handle&&!requests&&opens==before,"native cached hit reuses its handle without imports");
    Check(BotLoadCachedCharacter("bots/native.c",4,1)==2&&opens==before+1&&liveOwners==4,"native reload bypasses the cache into the next free handle");CacheValues(2);CacheDone();
}
int main(int argc,char **argv){
    int i;
    if(argc>1){i=atoi(argv[1]);if(i<4)CachedHit(i);else if(i==4)PublicHit();else Ordinary();return 0;}
    Ordinary();for(i=0;i<4;i++)CachedHit(i);PublicHit();
    puts("Native character cache hits resolve with all 64 handles occupied; reload/new-file exhaustion unchanged (issue #48)");
    return 0;
}
