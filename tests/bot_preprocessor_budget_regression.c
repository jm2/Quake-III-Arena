/* Actual preprocessor expansion work, recursion and source-owner budgets. */
#define Q3_CHARACTER_MAX_REQUESTS 16384
#define Q3_CHARACTER_MAX_OWNERS 8192
#define Q3_EVAL_COLLECTION_ENTRY EvalCollectionMain
#include "bot_eval_collection_regression.c"

static void Append(char *text, size_t capacity, size_t *used, const char *part)
{
    size_t length = strlen(part);
    Check(*used < capacity && length < capacity - *used, "budget fixture text fits its private buffer");
    memcpy(text + *used, part, length + 1);
    *used += length;
}

static void MacroCycle(int expression)
{
    source_t *source = EvalStart("#define LEFT RIGHT tail\n#define RIGHT LEFT\nmarker LEFT\n");
    token_t token;
    int owners, tokens;
    long integer = 99;
    double floating = 99;

    Check(PC_ReadToken(source, &token) && !strcmp(token.string, "marker") && !errors,
          "mutual macro fixture reaches its native expression cursor");
    owners = liveOwners;
    tokens = numtokens;
    if (expression)
    {
        Check(!PC_Evaluate(source, &integer, &floating, qtrue) && !integer && !floating,
              "direct expression collection rejects cyclic macro work");
    }
    else
    {
        Check(!PC_ReadToken(source, &token), "public token reading rejects cyclic macro work");
    }
    Check(errors == 1 && !fatals && PC_SourceHasError(source) &&
          !source->tokenworkdepth && !source->tokens && numtokens == tokens && liveOwners == owners,
          "cyclic expansion stops at one bounded diagnostic without queued or physical owner growth");
    EvalEnd(source);
}

static void OversizedDefinition(int external)
{
    char text[16384];
    size_t used = 0;
    source_t *source;
    token_t token;
    int i, owners, tokens;

    Append(text, sizeof(text), &used, external ? "HUGE " : "#define HUGE ");
    for (i = 0; i < 5000; i++) Append(text, sizeof(text), &used, "x ");
    if (!external) Append(text, sizeof(text), &used, "\ntail");
    source = EvalStart(external ? "tail" : text);
    owners = liveOwners;
    tokens = numtokens;
    if (external)
    {
        Check(!PC_AddDefine(source, text), "external oversized definition rejects within one work budget");
        Check(PC_ReadToken(source, &token) && !strcmp(token.string, "tail"),
              "failed external definition preserves the existing source cursor");
    }
    else
    {
        Check(!PC_ReadToken(source, &token) && PC_SourceHasError(source),
              "oversized in-file definition rejects before cache publication");
    }
    Check(errors == 1 && !fatals && !source->tokenworkdepth && !source->tokens &&
          numtokens == tokens && liveOwners == owners,
          "definition work failure releases every candidate name/body token owner");
    EvalEnd(source);
}

static void StringDepth(void)
{
    char text[2048];
    size_t used = 0;
    source_t *source;
    token_t token;
    int i, result, owners, tokens;

    Append(text, sizeof(text), &used, "#define S \"x\"\nmarker ");
    for (i = 0; i < 160; i++) Append(text, sizeof(text), &used, "S ");
    source = EvalStart(text);
    Check(PC_ReadToken(source, &token) && !strcmp(token.string, "marker") && !errors,
          "string recursion fixture publishes its marker before expansion");
    owners = liveOwners;
    tokens = numtokens;
    result = PC_ReadToken(source, &token);
    if (result || errors != 1 || fatals || !PC_SourceHasError(source) ||
        source->tokenworkdepth || source->tokens || numtokens != tokens || liveOwners != owners)
        fprintf(stderr, "String depth state: result=%d errors=%d fatals=%d sourceError=%d depth=%u queue=%p tokens=%d owners=%d\n",
                result, errors, fatals, PC_SourceHasError(source), source->tokenworkdepth,
                (void *)source->tokens, numtokens, liveOwners);
    Check(!result && errors == 1 && !fatals && PC_SourceHasError(source) &&
          !source->tokenworkdepth && !source->tokens && numtokens == tokens && liveOwners == owners,
          "adjacent-string recursion rejects before exhausting the native C stack");
    EvalEnd(source);
}

static void Goldens(void)
{
    char text[32768], expected[64];
    size_t used = 0;
    source_t *source;
    token_t token;
    int i;

    Append(text, sizeof(text), &used, "#define S \"x\"\n");
    for (i = 0; i < 16; i++) Append(text, sizeof(text), &used, "S ");
    source = EvalStart(text);
    memset(expected, 'x', 16);
    expected[0] = '"';
    memset(expected + 1, 'x', 16);
    expected[17] = '"';
    expected[18] = '\0';
    Check(PC_ReadToken(source, &token) && token.type == TT_STRING && !strcmp(token.string, expected) &&
          !PC_ReadToken(source, &token) && !errors && !fatals && !source->tokenworkdepth,
          "ordinary adjacent strings retain native concatenation and EOF behavior");
    EvalEnd(source);

    used = 0;
    for (i = 0; i < 5000; i++) Append(text, sizeof(text), &used, "x ");
    source = EvalStart(text);
    for (i = 0; i < 5000; i++)
        Check(PC_ReadToken(source, &token) && !strcmp(token.string, "x"),
              "work budget resets for each successfully published source token");
    Check(!PC_ReadToken(source, &token) && !errors && !fatals && !source->tokenworkdepth && !numtokens,
          "large ordinary source retains native complete token order");
    EvalEnd(source);
}

int main(int argc, char **argv)
{
    if (argc > 1)
    {
        int proof = atoi(argv[1]);
        if (proof == 0) MacroCycle(0);
        else if (proof == 1) MacroCycle(1);
        else if (proof == 2) OversizedDefinition(0);
        else if (proof == 3) OversizedDefinition(1);
        else StringDepth();
        return 0;
    }
    MacroCycle(0);
    MacroCycle(1);
    OversizedDefinition(0);
    OversizedDefinition(1);
    StringDepth();
    Goldens();
    puts("Actual preprocessor macro/expression work, string recursion and owner budgets pass (issue #48)");
    return 0;
}
