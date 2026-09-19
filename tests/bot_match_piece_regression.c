/* Actual match parser, borrowed source lifetime, nullable owners and matching. */
#define Q3_CHAT_CONSUMER_NO_MAIN
#include "bot_chat_consumer_regression.c"
int bot_developer;

static const char *pattern="\"hello \" | \"hi \",0,\"!\",7 = tail";
static source_t *PieceSource(const char *text)
{
    source_t *source;
    Attempt(text); source=LoadSourceFile("native.c");Check(source!=NULL,"actual borrowed source opens");return source;
}
static void MatchValues(bot_matchpiece_t *pieces)
{
    bot_match_t match;
    Check(pieces && pieces->type==MT_STRING && pieces->firststring &&
          !strcmp(pieces->firststring->string,"hello ") && pieces->firststring->next &&
          !strcmp(pieces->firststring->next->string,"hi ") && !pieces->firststring->next->next &&
          pieces->next && pieces->next->type==MT_VARIABLE && pieces->next->variable==0 &&
          pieces->next->next && pieces->next->next->type==MT_STRING &&
          !strcmp(pieces->next->next->firststring->string,"!") && pieces->next->next->next &&
          pieces->next->next->next->variable==7 && !pieces->next->next->next->next,
          "native piece/alternative order and variable slots remain");
    memset(&match,0,sizeof(match));strcpy(match.string,"hello Native!Tail");
    Check(StringsMatch(pieces,&match) && match.variables[0].offset==6 && match.variables[0].length==6 &&
          match.variables[7].offset==13 && match.variables[7].length==4,"actual native matcher keeps captured offsets and lengths");
    memset(&match,0,sizeof(match));strcpy(match.string,"hi Other!End");
    Check(StringsMatch(pieces,&match) && match.variables[0].offset==3 && match.variables[0].length==5 &&
          match.variables[7].offset==9 && match.variables[7].length==3,"actual alternative matching remains native");
}
static void GoldenCallers(void)
{
    bot_match_t match;int handle;
    ChatBegin();Attempt("1 { \"hello \",0,\"!\",7 = (2,3); }");matchtemplates=BotLoadMatchTemplates("native.c");
    Check(matchtemplates && matchtemplates->context==1 && matchtemplates->type==2 && matchtemplates->subtype==3 &&
          !matchtemplates->next && !errors && opens==closes && !numtokens,"native complete template caller fields and source cleanup");
    Check(BotFindMatch("hello Native!Tail",&match,1) && match.type==2 && match.subtype==3 &&
          match.variables[0].offset==6 && match.variables[0].length==6 && match.variables[7].offset==13 &&
          match.variables[7].length==4,"actual public template lookup preserves native capture/type values");ChatEnd();
    ChatBegin();handle=BotAllocChatState();NativeState(handle);
    Attempt("[(\"hello \",0,\"!\",7)] = 2 { \"reply \",0; }");replychats=BotLoadReplyChat("native.c");
    Check(replychats && replychats->priority==2 && replychats->keys && replychats->keys->flags==RCKFL_VARIABLES &&
          replychats->keys->match && replychats->numchatmessages==1 && !errors && opens==closes && !numtokens,
          "native reply match-key caller publishes full dictionary and releases its source");
    Check(BotReplyChat(handle,"hello Native!Tail",0,0,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL) &&
          !strcmp(botchatstates[handle]->chatmessage,"reply Native") && replychats->firstchatmessage->time==145.5f,
          "actual native reply match-key selection expands captured text and keeps timing");ChatEnd();
}
static void GoldenPieces(void)
{
    source_t *source;bot_matchpiece_t *pieces;token_t token;int i;char text[64];
    ChatBegin();source=PieceSource(pattern);pieces=BotLoadMatchPieces(source,"=");MatchValues(pieces);
    Check(!errors && PC_ReadToken(source,&token) && !strcmp(token.string,"tail"),"delimiter leaves borrowed source usable at native next token");
    FreeSource(source);BotFreeMatchPieces(pieces);ChatEnd();
    for(i=0;i<MAX_MATCHVARIABLES;i++) {
        ChatBegin();snprintf(text,sizeof(text),"%d ) tail",i);source=PieceSource(text);pieces=BotLoadMatchPieces(source,")");
        Check(pieces && pieces->type==MT_VARIABLE && pieces->variable==i && !pieces->next && !errors,
              "all native slots and reply delimiter remain accepted");FreeSource(source);BotFreeMatchPieces(pieces);ChatEnd();
    }
    ChatBegin();source=PieceSource("0,\"x\"|\"\" =");pieces=BotLoadMatchPieces(source,"=");
    Check(pieces && pieces->next && pieces->next->firststring && pieces->next->firststring->next &&
          !pieces->next->firststring->next->string[0] && !errors,"native empty alternative remains accepted");
    FreeSource(source);BotFreeMatchPieces(pieces);ChatEnd();
    GoldenCallers();
}
static void BadPieces(int kind)
{
    const char *texts[]={"\"hello \",","8 =","0,1 =","0,\"\"|\"x\",1 =","\"hello \"|1 =","unknown =","\"hello \",\"\\q\" =","\"hello \",#unknown"};
    source_t *source;bot_matchpiece_t *pieces;int owners;
    ChatBegin();source=PieceSource(texts[kind]);owners=heapLive;
    pieces=BotLoadMatchPieces(source,"=");
    Check(!pieces && errors>=1 && heapLive<=owners && !hunkLive,"bad/unterminated pieces reject, free private records and spend no persistent memory");
    Check(source->scriptstack!=NULL && PC_SourceHasError(source),"failure retains caller source ownership and its error flag");
    FreeSource(source);Check(!heapLive && !numtokens,"caller releases its source exactly once after helper failure");ChatEnd();
}
static void MissingInputs(int kind)
{
    source_t *source;
    ChatBegin();source=PieceSource(pattern);
    Check(!BotLoadMatchPieces(kind==0?NULL:source,kind==1?NULL:kind==2?"":"=") && errors==1 && !hunkLive,
          "missing borrowed source or delimiter rejects before imports");FreeSource(source);ChatEnd();
}
static void NullablePieces(int position,int priorMode)
{
    source_t *source;bot_matchpiece_t *prior=NULL,*candidate;bot_matchpiece_t header;bot_matchstring_t alternative;
    int owners,base,spent;
    ChatBegin();
    if(priorMode){source=PieceSource(pattern);prior=BotLoadMatchPieces(source,"=");MatchValues(prior);FreeSource(source);header=*prior;alternative=*prior->firststring;}
    owners=heapLive;source=PieceSource(pattern);base=requests;spent=hunkLive;failAt=base+position;
    candidate=BotLoadMatchPieces(source,"=");
    Check(!candidate && errors>=1 && requests>=failAt && hunkLive==spent,
          "every nullable parser/piece/string import rejects and frees partial private owners");
    Check(source->scriptstack && PC_SourceHasError(source),"nullable helper failure keeps borrowed source alive and marked");
    FreeSource(source);Check(heapLive==owners && !numtokens,"caller releases buffered source and every partial piece physically");
    if(prior){Check(!memcmp(&header,prior,sizeof(header)) && !memcmp(&alternative,prior->firststring,sizeof(alternative)),
          "nullable parse preserves prior record/pointer bytes");MatchValues(prior);}
    source=PieceSource(pattern);candidate=BotLoadMatchPieces(source,"=");MatchValues(candidate);FreeSource(source);
    BotFreeMatchPieces(candidate);BotFreeMatchPieces(prior);ChatEnd();
}
static void CallerFailures(void)
{
    ChatBegin();Attempt("1 { 8 = (2,3); }");Check(!BotLoadMatchTemplates("native.c") && errors>=1 && !heapLive && !numtokens && opens==closes,
          "template caller releases source once after piece failure");ChatEnd();
    ChatBegin();Attempt("[(8)] = 1 { \"native\"; }");Check(!BotLoadReplyChat("native.c") && errors>=1 && !heapLive && !numtokens && opens==closes,
          "reply caller releases source once after piece failure");ChatEnd();
}
int main(int argc,char **argv)
{
    source_t *source;bot_matchpiece_t *pieces;int base,count,i,mode;
    if(argc>1){i=atoi(argv[1]);if(i<8)BadPieces(i);else if(i<11)MissingInputs(i-8);else GoldenPieces();return 0;}
    GoldenPieces();for(i=0;i<8;i++)BadPieces(i);for(i=0;i<3;i++)MissingInputs(i);CallerFailures();
    ChatBegin();source=PieceSource(pattern);base=requests;pieces=BotLoadMatchPieces(source,"=");MatchValues(pieces);count=requests-base;
    FreeSource(source);BotFreeMatchPieces(pieces);ChatEnd();
    for(mode=0;mode<2;mode++)for(i=1;i<=count;i++)NullablePieces(i,mode);
    printf("Actual complete match pieces, %d nullable imports, borrowed-source cleanup, both callers and native matching/physical ownership (issue #48)\n",count*2);
    return 0;
}
