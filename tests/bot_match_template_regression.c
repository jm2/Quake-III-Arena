/* Actual complete template factory, physical ownership and public lookup. */
#define Q3_MATCH_PIECE_NO_MAIN
#include "bot_match_piece_regression.c"

static const char *templates="1 { \"hello \",0,\"!\",7 = (2,3); \"plain\" = (4,5); } 2 { 0 = (6,7); }";
static void TemplateValues(bot_matchtemplate_t *root)
{
    bot_matchtemplate_t *second,*third;bot_match_t match;
    Check(root && root->context==1 && root->type==2 && root->subtype==3 && root->first &&
          root->first->type==MT_STRING && !strcmp(root->first->firststring->string,"hello "),
          "native first complete context/type/subtype/piece values");
    second=root->next;third=second?second->next:NULL;
    Check(second && second->context==1 && second->type==4 && second->subtype==5 && second->first &&
          !strcmp(second->first->firststring->string,"plain") && third && third->context==2 && third->type==6 &&
          third->subtype==7 && third->first && third->first->variable==0 && !third->next,
          "native forward template order across contexts remains");
    Check(BotFindMatch("hello Native!Tail",&match,1) && match.type==2 && match.subtype==3 &&
          match.variables[0].offset==6 && match.variables[0].length==6 && match.variables[7].offset==13 &&
          match.variables[7].length==4,"actual first-context lookup preserves native captures/type/subtype");
    Check(BotFindMatch("plain",&match,1) && match.type==4 && match.subtype==5,
          "actual second template lookup remains native");
    Check(BotFindMatch("Other",&match,2) && match.type==6 && match.subtype==7 && match.variables[0].offset==0 &&
          match.variables[0].length==5 && !BotFindMatch("Other",&match,4),"native context masks and trailing variable capture remain");
}
static void GoldenTemplates(void)
{
    ChatBegin();Attempt(templates);matchtemplates=BotLoadMatchTemplates("native.c");TemplateValues(matchtemplates);
    Check(!errors && messages==1 && opens==closes && !numtokens,"successful whole source releases before normal public lookup");ChatEnd();
    ChatBegin();Attempt("");Check(!BotLoadMatchTemplates("native.c") && !errors && messages==1 && !heapLive && !hunkLive,
          "native empty optional template file remains distinct from error");ChatEnd();
    ChatBegin();Attempt("1 { } 2 { }");Check(!BotLoadMatchTemplates("native.c") && !errors && messages==1 && !heapLive && !hunkLive,
          "native empty context blocks remain accepted without storage");ChatEnd();
}
static void BadTemplates(int kind)
{
    char text[1024];const char *bad[]={"1 { \"native\" = (2,3);","1 {","1 { \"native\" = (2,3)","1 { \"native\" = (2,3); 8 = (4,5); }","1 { \"native\" = (2,3); } bad","1 { \"native\" = (2,3); } #unknown","1 { \"native\" = (2,3); } \"\\q\""};
    bot_matchtemplate_t *prior,header,second,third;int owners,spent;
    ChatBegin();Attempt(templates);prior=BotLoadMatchTemplates("native.c");matchtemplates=prior;TemplateValues(prior);
    header=*prior;second=*prior->next;third=*prior->next->next;owners=heapLive;spent=hunkLive;
    snprintf(text,sizeof(text),"%s",bad[kind]);Attempt(text);
    Check(!BotLoadMatchTemplates("native.c") && errors>=1 && !messages && heapLive==owners && !numtokens && hunkLive==spent &&
          opens==closes && matchtemplates==prior && !memcmp(&header,prior,sizeof(header)) &&
          !memcmp(&second,prior->next,sizeof(second)) && !memcmp(&third,prior->next->next,sizeof(third)),
          "incomplete/source-invalid templates reject, physically release candidates and preserve all prior header/pointer bytes");
    TemplateValues(prior);Attempt(templates);{bot_matchtemplate_t *retry=BotLoadMatchTemplates("native.c");Check(retry!=NULL,"malformed factory retries");BotFreeMatchTemplates(retry);}ChatEnd();
}
static void NullableTemplates(int position,int priorMode)
{
    bot_matchtemplate_t *prior=NULL,*candidate,header,second,third;int owners;
    ChatBegin();
    if(priorMode){Attempt(templates);prior=BotLoadMatchTemplates("native.c");matchtemplates=prior;TemplateValues(prior);
        header=*prior;second=*prior->next;third=*prior->next->next;}
    owners=heapLive;Attempt(templates);failAt=position;candidate=BotLoadMatchTemplates("native.c");
    Check(!candidate && errors>=1 && requests>=failAt && !messages && heapLive==owners && !numtokens && !hunkLive && opens==closes &&
          matchtemplates==prior,"every nullable source/container/piece/string import rejects with complete physical cleanup and no root mutation");
    if(prior){Check(!memcmp(&header,prior,sizeof(header)) && !memcmp(&second,prior->next,sizeof(second)) &&
          !memcmp(&third,prior->next->next,sizeof(third)),"nullable factory preserves prior headers/pointers");TemplateValues(prior);}
    Attempt(templates);candidate=BotLoadMatchTemplates("native.c");Check(candidate!=NULL,"nullable factory retries");
    if(prior)BotFreeMatchTemplates(prior);matchtemplates=candidate;TemplateValues(candidate);ChatEnd();
}
static int failedHeaderSize,armHeader;
static void *NullableHeaderHeap(int size)
{
    if(armHeader && size==failedHeaderSize){failAt=requests+1;armHeader=0;}return HeapAlloc(size);
}
static void *NullableHeaderHunk(int size)
{
    if(armHeader && size==failedHeaderSize){failAt=requests+1;armHeader=0;}return HunkAlloc(size);
}
static void FailedHeader(void)
{
    void *probe;
    ChatBegin();probe=GetMemory(0);Check(probe!=NULL,"actual template allocator prefix probe");
    failedHeaderSize=(int)sizeof(bot_matchtemplate_t)+(int)((char *)probe-(char *)heap[0]);FreeMemory(probe);
    Attempt(templates);armHeader=1;botimport.GetMemory=NullableHeaderHeap;botimport.HunkAlloc=NullableHeaderHunk;
    Check(!BotLoadMatchTemplates("native.c") && !armHeader && failAt && requests>=failAt && errors>=1 && !messages &&
          !heapLive && !hunkLive && !numtokens && opens==closes,"actual nullable template header rejects before dereference and frees source");ChatEnd();
}
static void InvalidFilename(int kind)
{
    char path[MAX_PATH+1];ChatBegin();memset(path,'x',sizeof(path));path[sizeof(path)-1]=0;
    Check(!BotLoadMatchTemplates(kind==0?NULL:kind==1?"":path) && errors==1 && !requests && !opens && !heapLive && !hunkLive,
          "invalid full template filename rejects before imports");ChatEnd();
}
int main(int argc,char **argv)
{
    int count,i,mode;
    if(argc>1){i=atoi(argv[1]);if(i<7)BadTemplates(i);else if(i==7)FailedHeader();else GoldenTemplates();return 0;}
    GoldenTemplates();FailedHeader();for(i=0;i<7;i++)BadTemplates(i);for(i=0;i<3;i++)InvalidFilename(i);
    ChatBegin();Attempt(templates);matchtemplates=BotLoadMatchTemplates("native.c");TemplateValues(matchtemplates);count=requests;ChatEnd();
    for(mode=0;mode<2;mode++)for(i=1;i<=count;i++)NullableTemplates(i,mode);
    printf("Actual complete template contexts, %d nullable imports, source rollback, native public lookup and physical ownership (issue #48)\n",count*2);
    return 0;
}
