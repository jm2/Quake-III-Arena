/* Actual compressed-file EOF and exhausted root/include conditional ownership. */
#define Q3_FILE_COMMENT_ENTRY FileCommentFixtureMain
#include "bot_file_comment_regression.c"
extern int PC_Directive_if(source_t *);
static void EofScript(int empty){
    script_t *script;token_t token;
    FileReset(empty?"/*closed*/\n// tail":"native /*closed*/ tail");script=LoadScriptFile("native.c");
    Check(script&&script->end_p==script->buffer+script->length&&EndOfScript(script)==empty&&liveOwners==2&&!errors&&!warnings,"compressed file owns the complete actual EOF interval");
    if(!empty)Check(PS_ReadToken(script,&token)&&!strcmp(token.string,"native")&&!EndOfScript(script)&&PS_ReadToken(script,&token)&&!strcmp(token.string,"tail")&&EndOfScript(script),"native file tokens advance to complete compressed EOF");
    Check(!PS_ReadToken(script,&token)&&EndOfScript(script)&&!errors&&!warnings,"empty/nonempty compressed files report actual EOF");
    FreeScript(script);Check(!liveOwners&&!numtokens,"compressed script owners physically release");
}
static void EofRoot(void){
    source_t *source;token_t token;
    FileReset("#if 0\nignored\n/*closed*/");source=LoadSourceFile("root.c");
    Check(source&&liveOwners==4&&!PC_ReadToken(source,&token)&&warnings==1&&!errors&&!source->indentstack&&!source->skip&&liveOwners==4&&!numtokens&&EndOfScript(source->scriptstack)&&!PC_SourceHasError(source),"root compressed EOF warns/unwinds every missing conditional exactly once");
    Check(!PC_ReadToken(source,&token)&&warnings==1&&!errors&&liveOwners==4,"root EOF remains synchronized on repeated reads");
    FreeSource(source);Check(!liveOwners&&!numtokens,"root conditional owners physically release");
}
static void EofInclude(int skipped){
    const char *body="1\n#include <child.c>\nbody\n#endif\ntail";source_t *source;script_t *parent;indent_t *prior;unsigned char saved[sizeof(indent_t)];token_t token;
    FileReset(skipped?"#if 0\nignored\n/*closed*/":"#if 1\ninside\n/*closed*/");
    source=LoadSourceMemory((char *)body,(int)strlen(body),"parent");Check(source&&PC_Directive_if(source),"actual parent conditional/imports prepare");
    parent=source->scriptstack;prior=source->indentstack;memcpy(saved,prior,sizeof(*prior));
    if(!skipped)Check(PC_ReadToken(source,&token)&&!strcmp(token.string,"inside")&&liveOwners==8&&!errors&&!warnings&&source->scriptstack!=parent&&source->indentstack!=prior,"native child active token owns complete nested frames");
    Check(PC_ReadToken(source,&token)&&!strcmp(token.string,"body")&&source->scriptstack==parent&&source->indentstack==prior&&!memcmp(saved,prior,sizeof(*prior))&&!source->skip&&liveOwners==5&&!numtokens&&warnings==1&&!errors&&opens==1&&closes==1&&!PC_SourceHasError(source),"compressed child EOF physically releases child frame/scripts and preserves parent frame/skip bytes");
    Check(PC_ReadToken(source,&token)&&!strcmp(token.string,"tail")&&!source->indentstack&&!source->skip&&liveOwners==4&&!PC_ReadToken(source,&token)&&warnings==1&&!errors,"parent endif/token order recover after exhausted compressed child");
    FreeSource(source);Check(!liveOwners&&!numtokens,"all nested source/frame owners physically release");
}
static void EofGolden(void){
    source_t *source;token_t token;FileGoldens();
    FileReset("native tail");source=LoadSourceFile("native.c");
    Check(source&&PC_ReadToken(source,&token)&&!strcmp(token.string,"native")&&!EndOfScript(source->scriptstack)&&PC_ReadToken(source,&token)&&!strcmp(token.string,"tail")&&EndOfScript(source->scriptstack)&&!PC_ReadToken(source,&token)&&!errors&&!warnings&&liveOwners==4,"unchanged native noncompacting source tokens/EOF remain");
    FreeSource(source);Check(!liveOwners&&!numtokens,"native source golden owners release");
}
int main(int argc,char **argv){int i;if(argc>1){i=atoi(argv[1]);if(i<2)EofScript(i);else if(i==2)EofRoot();else if(i==3)EofInclude(0);else if(i==4)EofInclude(1);else EofGolden();return 0;}EofScript(0);EofScript(1);EofRoot();EofInclude(0);EofInclude(1);EofGolden();puts("Real compressed file EOF, nested conditional/skip ownership, parent recovery and native byte/token goldens passed (issue #48)");return 0;}
