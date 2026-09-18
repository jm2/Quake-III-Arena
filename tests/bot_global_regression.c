/* Actual global registry deletion with real source/global copy ownership. */
#define Q3_SOURCE_ENTRY SourceFixtureMain
#include "bot_source_regression.c"
#undef Q3_SOURCE_ENTRY
static void Prepare(void) {
    SourceReset("");Check(PC_AddGlobalDefine("first 1")&&PC_AddGlobalDefine("middle(value) value")&&PC_AddGlobalDefine("last 3"),"real native global registry prepares");
    Check(liveOwners==7&&numtokens==4&&!errors,"known name/body/parameter registry owners");
}
static void RemovePosition(int position) {
    const char *names[]={"last","middle","first"};source_t *before,*after;define_t *head,*middle,*tail;int ownersBefore,tokensBefore,imports;
    Prepare();head=globaldefines;middle=head->next;tail=middle->next;before=LoadSourceMemory("first middle(42) last",21,"before-delete");
    Check(before&&liveOwners==18&&numtokens==8,"existing source owns independent copied globals");ownersBefore=liveOwners;tokensBefore=numtokens;imports=requests;
    Check(PC_RemoveGlobalDefine((char *)names[position]),"selected native global removal succeeds");
    Check(requests==imports,"deletion requires no prospective allocation");
    after=LoadSourceMemory("first last",10,"after-delete");Check(after!=NULL,"new source creates from remaining globals without a dangling registry node");
    if(position==0)Check(globaldefines==middle&&middle->next==tail&&!tail->next,"head removal preserves remaining native chain");
    else if(position==1)Check(globaldefines==head&&head->next==tail&&!tail->next,"middle removal preserves remaining native chain");
    else Check(globaldefines==head&&head->next==middle&&!middle->next,"tail removal terminates remaining native chain");
    ReadGolden(before,"1",TT_NUMBER);ReadGolden(before,"42",TT_NUMBER);ReadGolden(before,"3",TT_NUMBER);
    ReadGolden(after,position==2?"first":"1",position==2?TT_NAME:TT_NUMBER);ReadGolden(after,position==0?"last":"3",position==0?TT_NAME:TT_NUMBER);
    FreeSource(after);Check(liveOwners==ownersBefore-(position==1?3:2)&&numtokens==tokensBefore-(position==1?2:1),"deletion frees exactly removed name/body/parameter owners while prior source copies survive");
    FreeSource(before);Check(liveOwners==(position==1?4:5)&&numtokens==(position==1?2:3),"source cleanup retains only remaining original registry owners");
    PC_RemoveAllGlobalDefines();Check(!globaldefines&&!liveOwners&&!numtokens,"remaining native registry cleanup frees each physical owner once");
}
static void NoMatch(void) {
    define_t *head;int imports;Prepare();head=globaldefines;imports=requests;
    Check(!PC_RemoveGlobalDefine("absent")&&!PC_RemoveGlobalDefine("FIRST")&&!PC_RemoveGlobalDefine(NULL)&&globaldefines==head&&liveOwners==7&&numtokens==4&&requests==imports,"missing/case-distinct/null removal preserves registry and performs no imports");
    PC_RemoveAllGlobalDefines();Check(!liveOwners&&!numtokens,"unmodified registry cleanup physically releases");
}
static void Duplicate(void) {
    define_t *older;source_t *source;SourceReset("");Check(PC_AddGlobalDefine("same 1")&&PC_AddGlobalDefine("same 2"),"native duplicate registry definitions prepare");older=globaldefines->next;
    Check(PC_RemoveGlobalDefine("same")&&globaldefines==older&&!older->next&&liveOwners==2&&numtokens==1,"first matching duplicate alone unlinks/frees");
    source=LoadSourceMemory("same",4,"duplicate-delete");Check(source!=NULL,"remaining duplicate source creates");ReadGolden(source,"1",TT_NUMBER);FreeSource(source);
    Check(PC_RemoveGlobalDefine("same")&&!globaldefines&&!liveOwners&&!numtokens&&!PC_RemoveGlobalDefine("same"),"last duplicate deletion and repeated missing deletion retain native result");
}
int main(int argc,char **argv) {
    if(argc>1){int proof=atoi(argv[1]);if(proof<3)RemovePosition(proof);else if(proof==3)NoMatch();else Duplicate();}
    else {int position;for(position=0;position<3;position++)RemovePosition(position);NoMatch();Duplicate();puts("Native global head/middle/tail deletion and independent source-copy ownership passed (issue #48)");}
    return 0;
}
