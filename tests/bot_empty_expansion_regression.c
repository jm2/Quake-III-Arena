/* Real source continuation and publication after complete empty macro expansion. */
#define Q3_EVAL_ENTRY EvalFixtureMain
#include "bot_eval_regression.c"
extern int PC_ExpandDefineIntoSource(source_t *,token_t *,define_t *);
extern define_t *PC_FindHashedDefine(define_t **,char *);
extern int PC_UnreadSourceToken(source_t *,token_t *);
static void EmptyRead(const char *text,const char **expected,int count,int nativeWarnings){
    source_t *source=EvalStart(text);token_t token;int i;
    for(i=0;i<count;i++)Check(PC_ReadToken(source,&token)&&!strcmp(token.string,expected[i]),"complete empty expansion preserves subsequent token order");
    Check(!PC_ReadToken(source,&token)&&!errors&&!fatals&&!PC_SourceHasError(source)&&EndOfScript(source->scriptstack)&&warnings==nativeWarnings,"empty expansions reach actual EOF with native warnings");
    EvalEnd(source);
}
static void EmptyCase(int entry){
    const char *pair[]={"before","after"},*number[]={"7"},*tail[]={"tail"},*joined[]={"\"firstsecond\"","tail"};
    if(entry==0)EmptyRead("#define EMPTY\nbefore EMPTY after",pair,2,0);
    else if(entry==1)EmptyRead("#define EMPTY\nEMPTY EMPTY 7",number,1,0);
    else if(entry==2)EmptyRead("#define DROP(a)\nDROP(7) tail",tail,1,0);
    else if(entry==3)EmptyRead("#define ID(a) a\nID() tail",tail,1,1);
    else EmptyRead("#define EMPTY\n\"first\" EMPTY \"second\" tail",joined,2,0);
}
static void EmptyLexical(void){
    source_t *source=EvalStart("#define EMPTY\nEMPTY \"unterminated");token_t token;
    Check(!PC_ReadToken(source,&token)&&errors==1&&!fatals&&PC_SourceHasError(source)&&!source->tokens&&!numtokens&&liveOwners==5,"empty macro cannot hide the following lexical error as clean EOF");
    Check(!PC_ReadToken(source,&token)&&errors==1&&liveOwners==5,"lexical failure remains sticky after empty expansion");
    EvalEnd(source);
}
static void EmptyInclude(void){
    source_t *source=EvalStart("tail");script_t *parent=source->scriptstack,*child;token_t token;
    Check(PC_AddDefine(source,"EMPTY"),"actual empty object definition prepares");
    child=LoadScriptMemory("EMPTY",5,"empty-child");Check(child!=NULL,"actual included script prepares");
    child->next=parent;source->scriptstack=child;
    Check(PC_ReadToken(source,&token)&&!strcmp(token.string,"tail")&&source->scriptstack==parent&&liveOwners==5&&!numtokens&&!errors&&!warnings&&!fatals,"empty included script returns to parent and physically releases child owners");
    Check(!PC_ReadToken(source,&token)&&!PC_SourceHasError(source),"parent reaches actual EOF");
    EvalEnd(source);
}
static void EmptyQueue(void){
    source_t *source=EvalStart("tail");token_t token,macro;token_t *prior;define_t *define;unsigned char saved[sizeof(token_t)];int imports,owners;
    Check(PC_AddDefine(source,"EMPTY")&&PC_ReadToken(source,&token)&&PC_UnreadSourceToken(source,&token),"actual empty define and queued native token prepare");
    define=PC_FindHashedDefine(source->definehash,"EMPTY");prior=source->tokens;memcpy(saved,prior,sizeof(*prior));
    memset(&macro,0,sizeof(macro));strcpy(macro.string,"EMPTY");macro.type=TT_NAME;macro.subtype=5;imports=requests;owners=liveOwners;
    Check(PC_ExpandDefineIntoSource(source,&macro,define)&&source->tokens==prior&&!memcmp(saved,prior,sizeof(*prior))&&requests==imports&&liveOwners==owners&&!errors&&!fatals&&numtokens==1,"empty expansion succeeds with no import and preserves complete prior queued bytes");
    Check(PC_ReadToken(source,&token)&&!strcmp(token.string,"tail")&&!PC_ReadToken(source,&token)&&!errors&&!numtokens,"native queued continuation remains readable");
    EvalEnd(source);
}
static void EmptyCharacter(void){
    bot_character_t *character;
    Reset("#define EMPTY\n#define DROP(a)\nskill 4 { EMPTY 0 42 DROP(7) 1 1.25 EMPTY 79 \"native\" EMPTY }");fatals=0;
    character=BotLoadCharacterFromFile("bots/native.c",4);
    Check(character&&character->c[0].type==CT_INTEGER&&character->c[0].value.integer==42&&character->c[1].type==CT_FLOAT&&character->c[1].value._float==1.25&&character->c[79].type==CT_STRING&&!strcmp(character->c[79].value.string,"native"),"empty expansions inside actual character fields publish complete native values");
    Check(opens==1&&closes==1&&!errors&&!warnings&&!fatals&&liveOwners==2&&!numtokens,"actual source owners release before character publication");
    Release(character);
}
static void EmptyGolden(void){
    const char *tokens[]={"42","7","+","8","tail"};
    EmptyRead("#define VALUE 42\n#define ADD(a,b) a + b\nVALUE ADD(7,8) tail",tokens,5,0);
}
int main(int argc,char **argv){
    int i;
    if(argc>1){i=atoi(argv[1]);if(i<5)EmptyCase(i);else if(i==5)EmptyLexical();else if(i==6)EmptyInclude();else if(i==7)EmptyQueue();else if(i==8)EmptyCharacter();else if(i==9)EmptyRead("#define EMPTY\nEMPTY EMPTY",NULL,0,0);else EmptyGolden();return 0;}
    for(i=0;i<5;i++)EmptyCase(i);
    EmptyLexical();EmptyInclude();EmptyQueue();EmptyCharacter();EmptyRead("#define EMPTY\nEMPTY EMPTY",NULL,0,0);EmptyGolden();
    puts("Real empty macro continuation, queued bytes, included owners, lexical errors and character publication passed (issue #48)");
    return 0;
}
