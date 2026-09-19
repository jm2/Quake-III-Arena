/* Aggregate native preprocessor memory, depth and rollback limits. */
#define Q3_CHARACTER_MAX_OWNERS 4096
#define Q3_CHARACTER_MAX_REQUESTS 8192
#define Q3_SOURCE_ENTRY SourceFixtureMain
#include "bot_source_regression.c"
#undef Q3_SOURCE_ENTRY

extern unsigned long sourceparsememory;
extern token_t *PC_CopyToken(token_t *token);
extern void PC_FreeToken(token_t *token);
extern int PC_PushIndent(source_t *source, int type, int skip);
extern void PC_PopIndent(source_t *source, int *type, int *skip);
extern int PC_PushScript(source_t *source, script_t *script);

static void BudgetReset(void)
{
    Check(!sourceparsememory && !globaldefines, "previous parser budget and globals release");
    SourceReset("");
}

static void ScriptLimit(void)
{
    unsigned long punctuationBytes = 256UL * sizeof(punctuation_t *);
    unsigned long overhead = sizeof(script_t) + 1UL + punctuationBytes;
    int fit;
    char *text;
    script_t *script;

    Check(MAX_SCRIPT_PARSER_MEMORY > overhead, "fixture script ceiling fits native metadata");
    fit = (int)(MAX_SCRIPT_PARSER_MEMORY - overhead);
    text = malloc((size_t)fit + 2);
    Check(text != NULL, "fixture script bytes allocate");
    memset(text, 'x', (size_t)fit + 1);
    text[fit + 1] = '\0';

    BudgetReset();
    script = LoadScriptMemory(text, fit, "last-fitting");
    Check(script && script->memorysize == MAX_SCRIPT_PARSER_MEMORY &&
          liveOwners == 2 && !sourceparsememory,
          "last-fitting script owns its complete buffer and punctuation table");
    FreeScript(script);
    Check(!liveOwners && !sourceparsememory, "standalone fitting script releases");

    BudgetReset();
    Check(!LoadScriptMemory(text, fit + 1, "first-oversized") &&
          !requests && !liveOwners && !sourceparsememory,
          "first oversized memory script rejects before allocation");

    BudgetReset();
    reportLength = fit + 1;
    Check(!LoadScriptFile("scripts/oversized.c") && opens == 1 && closes == 1 &&
          !fileReads && !requests && !liveOwners && !sourceparsememory,
          "oversized file script closes before allocation or read");
    free(text);
}

static void ConcurrentSources(void)
{
    source_t *sources[64];
    source_t *source;
    int count = 0, beforeOwners;
    unsigned long beforeMemory;
    char name[32];

    BudgetReset();
    for (;;)
    {
        Check(count < (int)(sizeof(sources) / sizeof(sources[0])),
              "aggregate source limit rejects before fixture capacity");
        snprintf(name, sizeof(name), "source-%d", count);
        beforeOwners = liveOwners;
        beforeMemory = sourceparsememory;
        source = LoadSourceMemory(NULL, 0, name);
        if (!source)
        {
            Check(liveOwners == beforeOwners && sourceparsememory == beforeMemory,
                  "rejected concurrent source releases its unreserved script");
            break;
        }
        sources[count++] = source;
        Check(sourceparsememory <= MAX_SOURCE_PARSE_MEMORY,
              "concurrent source accounting stays under the ceiling");
    }
    Check(count > 1 && count < 64 && !errors && !fatalErrors,
          "multiple complete sources fit before the aggregate rejection");
    while (count) FreeSource(sources[--count]);
    Check(!liveOwners && !numtokens && !sourceparsememory,
          "all concurrent source owners and reservations release");

    BudgetReset();
    source = LoadSourceMemory(NULL, 0, "source-retry");
    Check(source != NULL && sourceparsememory > 0,
          "source allocation retries after aggregate cleanup");
    FreeSource(source);
    Check(!liveOwners && !sourceparsememory, "retried source releases");
}

static void QueuedTokens(void)
{
    source_t *source;
    token_t token, *copy;
    unsigned long base, expected;
    int count = 0, baseOwners;

    BudgetReset();
    source = LoadSourceMemory(NULL, 0, "token-budget");
    Check(source != NULL, "token-budget source prepares");
    base = sourceparsememory;
    baseOwners = liveOwners;
    expected = (MAX_SOURCE_PARSE_MEMORY - base) / sizeof(token_t);
    memset(&token, 0, sizeof(token));
    strcpy(token.string, "native");
    token.type = TT_NAME;

    while ((copy = PC_CopyToken(&token)) != NULL)
    {
        copy->next = source->tokens;
        source->tokens = copy;
        count++;
    }
    Check((unsigned long)count == expected && numtokens == count &&
          sourceparsememory == base + expected * sizeof(token_t) &&
          liveOwners == baseOwners + count && !errors && !fatalErrors,
          "queued tokens stop exactly at the aggregate ceiling without fatal allocation");
    FreeSource(source);
    Check(!liveOwners && !numtokens && !sourceparsememory,
          "queued token owners and source budget release");

    BudgetReset();
    source = LoadSourceMemory(NULL, 0, "token-retry");
    Check(source != NULL && (copy = PC_CopyToken(&token)) != NULL,
          "token allocation retries after complete release");
    source->tokens = copy;
    FreeSource(source);
    Check(!liveOwners && !numtokens && !sourceparsememory,
          "retried token owner releases");
}

static void ConditionalDepth(void)
{
    source_t *source;
    unsigned long base;
    int baseOwners, i, type, skip;

    BudgetReset();
    source = LoadSourceMemory(NULL, 0, "conditional-depth");
    Check(source != NULL, "conditional-depth source prepares");
    base = sourceparsememory;
    baseOwners = liveOwners;
    for (i = 0; i < 128; i++)
        Check(PC_PushIndent(source, INDENT_IF, 0),
              "permitted conditional frame publishes");
    Check(source->indentdepth == 128 &&
          sourceparsememory == base + 128UL * sizeof(indent_t) &&
          liveOwners == baseOwners + 128,
          "conditional frame depth and bytes are accounted exactly");
    Check(!PC_PushIndent(source, INDENT_IF, 0) && errors == 1 &&
          source->indentdepth == 128 &&
          sourceparsememory == base + 128UL * sizeof(indent_t) &&
          liveOwners == baseOwners + 128,
          "next conditional frame rejects without changing owners");
    for (i = 0; i < 128; i++)
    {
        PC_PopIndent(source, &type, &skip);
        Check(type == INDENT_IF && !skip, "conditional frame pops with native metadata");
    }
    Check(!source->indentstack && !source->indentdepth &&
          sourceparsememory == base && liveOwners == baseOwners,
          "all conditional frames release exact reservations");
    FreeSource(source);
    Check(!liveOwners && !sourceparsememory, "conditional source releases");
}

static void IncludedScript(void)
{
    source_t *source;
    script_t *child;
    token_t token;
    unsigned long base, childBytes;
    int baseOwners;

    BudgetReset();
    source = LoadSourceMemory(NULL, 0, "include-root");
    Check(source != NULL, "include root prepares");
    base = sourceparsememory;
    baseOwners = liveOwners;
    child = LoadScriptMemory(NULL, 0, "include-child");
    Check(child != NULL && sourceparsememory == base && liveOwners == baseOwners + 2,
          "detached include owns bytes before parser publication");
    childBytes = child->memorysize;
    Check(PC_PushScript(source, child) &&
          sourceparsememory == base + childBytes &&
          liveOwners == baseOwners + 2,
          "published include reserves its complete script cost");
    Check(!PC_ReadToken(source, &token) && source->scriptstack &&
          !source->scriptstack->next && sourceparsememory == base &&
          liveOwners == baseOwners && !errors,
          "include EOF releases its reservation before root EOF");
    FreeSource(source);
    Check(!liveOwners && !sourceparsememory, "include root releases");
}

static void SourceDefinitions(void)
{
    source_t *source;
    unsigned long beforeMemory;
    int count = 0, beforeOwners;
    char definition[64];

    BudgetReset();
    source = LoadSourceMemory(NULL, 0, "source-definitions");
    Check(source != NULL, "definition source prepares");
    for (;;)
    {
        snprintf(definition, sizeof(definition), "D%d %d", count, count);
        beforeMemory = sourceparsememory;
        beforeOwners = liveOwners;
        if (!PC_AddDefine(source, definition))
        {
            Check(sourceparsememory == beforeMemory && liveOwners == beforeOwners,
                  "failed source definition leaves prior dictionary complete");
            break;
        }
        count++;
        Check(sourceparsememory <= MAX_SOURCE_PARSE_MEMORY,
              "source definitions remain inside aggregate ceiling");
    }
    Check(count > 0 && count < 256 && !fatalErrors,
          "persistent source definitions reach the aggregate ceiling");
    FreeSource(source);
    Check(!liveOwners && !numtokens && !sourceparsememory,
          "source definitions and table release exact reservations");

    BudgetReset();
    source = LoadSourceMemory(NULL, 0, "definition-retry");
    Check(source && PC_AddDefine(source, "RETRY 1"),
          "source definition retries after complete cleanup");
    FreeSource(source);
    Check(!liveOwners && !numtokens && !sourceparsememory,
          "retried source definition releases");
}

static void GlobalDefinitions(void)
{
    unsigned long beforeMemory;
    int count = 0, beforeOwners;
    char definition[64];

    BudgetReset();
    for (;;)
    {
        snprintf(definition, sizeof(definition), "GLOBAL_%d %d", count, count);
        beforeMemory = sourceparsememory;
        beforeOwners = liveOwners;
        if (!PC_AddGlobalDefine(definition))
        {
            Check(sourceparsememory == beforeMemory && liveOwners == beforeOwners,
                  "failed global definition leaves prior registry complete");
            break;
        }
        count++;
        Check(sourceparsememory <= MAX_SOURCE_PARSE_MEMORY,
              "global definitions remain inside aggregate ceiling");
    }
    Check(count > 0 && count < 256 && globaldefines && !fatalErrors,
          "persistent global definitions reach the aggregate ceiling");
    PC_RemoveAllGlobalDefines();
    Check(!globaldefines && !liveOwners && !numtokens && !sourceparsememory,
          "global registry releases exact reservations");

    BudgetReset();
    Check(PC_AddGlobalDefine("GLOBAL_RETRY 1"),
          "global definition retries after aggregate cleanup");
    PC_RemoveAllGlobalDefines();
    Check(!globaldefines && !liveOwners && !numtokens && !sourceparsememory,
          "retried global definition releases");
}

int main(void)
{
    ScriptLimit();
    ConcurrentSources();
    QueuedTokens();
    ConditionalDepth();
    IncludedScript();
    SourceDefinitions();
    GlobalDefinitions();
    puts("Aggregate parser memory, script, token, include, conditional and definition budgets passed (issue #48)");
    return 0;
}
