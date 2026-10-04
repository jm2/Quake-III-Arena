/* Bot preprocessor bounds (issue #48): adjacent-string concatenation no longer
   recurses once per string, macro cycles stop at a bounded expansion budget, and
   distinct-file include chains stop at a bounded depth.  Crafted files are loaded
   through the real LoadSourceFile/PC_LoadSourceHandle paths of l_precomp.c and
   l_script.c.  Every case runs on a 1 MiB thread stack (the Mac OS 9 application
   stack has no guard page) under an alarm, so the original code fails by stack
   overflow, by a hang, or by accepting an unbounded include chain.  The
   allocator fails past 64 MiB of live memory, so a case that would exhaust the
   Mac's 16 MiB zone ends the original code in a fatal error instead of using
   gigabytes of host memory. */
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../code/game/q_shared.h"
#include "../code/game/botlib.h"
#include "../code/botlib/l_script.h"
#include "../code/botlib/l_precomp.h"

#define FIXTURE_STACK (1024 * 1024)
#define MAX_FILES 160
#define MAX_TEXT (512 * 1024)
#define MEMORY_CAP (64UL * 1024 * 1024)

botlib_import_t botimport;
extern int numtokens;
static int failures, errors, warnings, liveOwners;
static const char *caseName = "";
static char lastError[1024];
static char *fileNames[MAX_FILES], *fileTexts[MAX_FILES];
static int numFiles, openText[64];
static char text[MAX_TEXT];
static int onlyCase = -1;
static size_t used, liveBytes, peakBytes;

static void Check(int condition, const char *message)
{
	if (!condition)
	{
		fprintf(stderr, "FAIL [%s]: %s (errors=%d last=\"%s\")\n", caseName, message, errors, lastError);
		failures++;
	}
}

/* each block carries its size so live and peak usage can be measured */
void *GetMemory(unsigned long size)
{
	size_t *p;
	if (size > MEMORY_CAP - liveBytes) return NULL;
	p = malloc(size + 16);
	if (!p) return NULL;
	*p = size;
	liveBytes += size;
	if (liveBytes > peakBytes) peakBytes = liveBytes;
	liveOwners++;
	return (char *)p + 16;
}
void *GetClearedMemory(unsigned long size) {void *p = GetMemory(size); if (p) memset(p, 0, size); return p;}
void FreeMemory(void *p)
{
	size_t *block;
	if (!p) return;
	block = (size_t *)((char *)p - 16);
	liveBytes -= *block;
	liveOwners--;
	free(block);
}
void *GetHunkMemory(unsigned long size) {return GetMemory(size);}
void *GetClearedHunkMemory(unsigned long size) {return GetClearedMemory(size);}
#ifndef Com_Memcpy
void Com_Memcpy(void *out, const void *in, size_t size) {memcpy(out, in, size);}
#endif
#ifndef Com_Memset
void Com_Memset(void *out, int value, size_t size) {memset(out, value, size);}
#endif
void QDECL Log_Write(char *format, ...) {(void)format;}
void QDECL Com_Error(int level, const char *format, ...)
{
	va_list args;
	(void)level;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	abort();
}
void QDECL Com_Printf(const char *format, ...) {(void)format;}
static void QDECL Print(int level, char *format, ...)
{
	va_list args;
	if (level == PRT_ERROR || level == PRT_FATAL)
	{
		errors++;
		va_start(args, format);
		vsnprintf(lastError, sizeof(lastError), format, args);
		va_end(args);
	}
	else warnings++;
}

static int FileOpen(const char *path, fileHandle_t *file, fsMode_t mode)
{
	int i, handle;
	(void)mode;
	*file = 0;
	for (i = 0; i < numFiles; i++)
	{
		if (strcmp(fileNames[i], path)) continue;
		for (handle = 1; handle < 64 && openText[handle]; handle++);
		if (handle >= 64) return -1;
		openText[handle] = i + 1;
		*file = handle;
		return (int)strlen(fileTexts[i]);
	}
	return -1;
}
static int FileRead(void *out, int length, fileHandle_t file)
{
	memcpy(out, fileTexts[openText[file] - 1], length);
	return length;
}
static void FileClose(fileHandle_t file) {openText[file] = 0;}

static void ClearFiles(void)
{
	int i;
	for (i = 0; i < numFiles; i++)
	{
		free(fileNames[i]);
		free(fileTexts[i]);
	}
	numFiles = 0;
}
static void AddFile(const char *name, const char *contents)
{
	if (numFiles >= MAX_FILES) {Check(0, "fixture file table"); return;}
	fileNames[numFiles] = strdup(name);
	fileTexts[numFiles] = strdup(contents);
	numFiles++;
}
static void Begin(void) {used = 0; text[0] = '\0';}
static void Append(const char *format, ...)
{
	va_list args;
	int length;
	va_start(args, format);
	length = vsnprintf(text + used, sizeof(text) - used, format, args);
	va_end(args);
	if (length < 0 || (size_t)length >= sizeof(text) - used) {Check(0, "fixture text fits"); return;}
	used += length;
}

static source_t *Load(const char *name)
{
	source_t *source;
	errors = warnings = 0;
	lastError[0] = '\0';
	peakBytes = liveBytes;
	PC_SetBaseFolder("");
	source = LoadSourceFile(name);
	Check(source != NULL, "crafted source loads");
	return source;
}
static void Expect(source_t *source, int type, const char *string)
{
	token_t token;
	if (!PC_ReadToken(source, &token)) {Check(0, "expected token is read"); return;}
	if (token.type != type || strcmp(token.string, string))
	{
		fprintf(stderr, "[%s] got type %d \"%.60s\", want type %d \"%.60s\"\n", caseName, token.type, token.string, type, string);
		Check(0, "token matches the native golden");
	}
}
static void ExpectEnd(source_t *source)
{
	token_t token;
	Check(!PC_ReadToken(source, &token), "source ends after the golden tokens");
	Check(!errors, "golden source reports no error");
}
static void Finish(source_t *source)
{
	if (source) FreeSource(source);
	Check(!liveOwners && !numtokens, "every script, source and token owner is released");
	ClearFiles();
}
/* reads until the first failure; returns the number of tokens published first */
static int ReadUntilFailure(source_t *source, int limit)
{
	token_t token;
	int count = 0;
	while (count < limit && PC_ReadToken(source, &token)) count++;
	return count;
}
static void ExpectFailure(source_t *source, int published, int limit, const char *message, const char *diagnostic)
{
	Check(published < limit, message);
	Check(errors == 1 && strstr(lastError, diagnostic) != NULL, "one bounded diagnostic");
	Check(PC_SourceHasError(source), "source keeps its error state");
	Check(!source->tokens, "queued expansion candidates are released on failure");
}
static void ExpectBoundedFailure(source_t *source, int published, int limit, const char *message)
{
	ExpectFailure(source, published, limit, message, "macro expansion exceeds");
}

/* --- adjacent strings ----------------------------------------------------- */

static void AdjacentStrings(void)
{
	source_t *source;
	int i;

	caseName = "2000 adjacent strings";
	Begin();
	Append("first ");
	for (i = 0; i < 2000; i++) Append("\"\" ");
	Append("last\n");
	AddFile("strings.txt", text);
	source = Load("strings.txt");
	Expect(source, TT_NAME, "first");
	Expect(source, TT_STRING, "\"\"");
	Expect(source, TT_NAME, "last");
	ExpectEnd(source);
	Finish(source);

	caseName = "2000 macro-generated strings";
	Begin();
	Append("#define E \"\"\n#define S \"s\"\n");
	for (i = 0; i < 2000; i++) Append("E ");
	Append("S \"t\" S end\n");
	AddFile("macro-strings.txt", text);
	source = Load("macro-strings.txt");
	Expect(source, TT_STRING, "\"sts\"");
	Expect(source, TT_NAME, "end");
	ExpectEnd(source);
	Finish(source);

	caseName = "2000 strings in a skipped block";
	/* retail concatenates strings through a skipped block and publishes the
	   result once the skip ends; keep that, without one frame per string */
	Begin();
	Append("#if 0\n");
	for (i = 0; i < 2000; i++) Append("\"\" x ");
	Append("\n#endif\ntail \"d\" \"e\" end\n#ifdef NOPE\n\"f\"\n#else\n\"g\"\n#endif\n\"h\" last\n");
	AddFile("skipped.txt", text);
	source = Load("skipped.txt");
	Expect(source, TT_STRING, "\"\"");
	Expect(source, TT_NAME, "tail");
	Expect(source, TT_STRING, "\"de\"");
	Expect(source, TT_NAME, "end");
	Expect(source, TT_STRING, "\"fgh\"");
	Expect(source, TT_NAME, "last");
	ExpectEnd(source);
	Finish(source);

	caseName = "2000 strings longer than MAX_TOKEN";
	Begin();
	for (i = 0; i < 2000; i++) Append("\"x\" ");
	AddFile("long.txt", text);
	source = Load("long.txt");
	Check(!ReadUntilFailure(source, 10), "overlong concatenation fails");
	Check(errors == 1 && strstr(lastError, "MAX_TOKEN") != NULL, "native MAX_TOKEN diagnostic");
	Finish(source);
}

/* --- macro cycles --------------------------------------------------------- */

static int Want(int which) {return onlyCase < 0 || onlyCase == which;}

static void MacroCycles(void)
{
	source_t *source;
	pc_token_t pctoken;
	int handle, i;

	if (Want(0))
	{
		caseName = "mutual macro pair";
		AddFile("pair.txt", "#define a b\n#define b a\nmarker a tail\n");
		source = Load("pair.txt");
		Expect(source, TT_NAME, "marker");
		ExpectBoundedFailure(source, ReadUntilFailure(source, 1), 1, "mutual pair fails the token read");
		Finish(source);
	}
	if (Want(1))
	{
		caseName = "mutual macro pair in $evalint";
		AddFile("pair-eval.txt", "#define a b\n#define b a\nmarker $evalint(a) tail\n");
		source = Load("pair-eval.txt");
		Expect(source, TT_NAME, "marker");
		ExpectBoundedFailure(source, ReadUntilFailure(source, 1), 1, "cyclic $evalint operand fails");
		Finish(source);
	}
	if (Want(2))
	{
		caseName = "mutual macro pair in #if";
		AddFile("pair-hash-if.txt", "#define a b\n#define b a\nmarker\n#if a\nyes\n#endif\ntail\n");
		source = Load("pair-hash-if.txt");
		Expect(source, TT_NAME, "marker");
		ExpectBoundedFailure(source, ReadUntilFailure(source, 1), 1, "cyclic #if operand fails");
		Finish(source);
	}
	if (Want(3))
	{
		caseName = "five-macro cycle with growing tails";
		AddFile("cycle.txt", "#define c0 c1 t0\n#define c1 c2 t1\n#define c2 c3 t2\n"
			"#define c3 c4 t3\n#define c4 c0 t4\nmarker c0 tail\n");
		source = Load("cycle.txt");
		Expect(source, TT_NAME, "marker");
		ExpectBoundedFailure(source, ReadUntilFailure(source, 1), 1, "longer cycle fails the token read");
		Finish(source);
	}
	if (Want(4))
	{
		caseName = "function-like macro cycle";
		AddFile("fcycle.txt", "#define f(x) g(x x)\n#define g(x) f(x)\nmarker f(1) tail\n");
		source = Load("fcycle.txt");
		Expect(source, TT_NAME, "marker");
		ExpectBoundedFailure(source, ReadUntilFailure(source, 1), 1, "function-like cycle fails");
		Finish(source);
	}
	if (Want(5))
	{
		caseName = "cycle publishing tokens forever";
		AddFile("stream.txt", "#define a b\n#define b x a\nmarker a tail\n");
		source = Load("stream.txt");
		Expect(source, TT_NAME, "marker");
		ExpectBoundedFailure(source, ReadUntilFailure(source, 100000), 4096,
			"an endless expansion stream stops within one budget");
		Finish(source);
	}
	if (Want(6))
	{
		caseName = "exponential expansion";
		Begin();
		Append("#define e0 x\n");
		for (i = 1; i <= 24; i++) Append("#define e%d e%d e%d\n", i, i - 1, i - 1);
		Append("marker e24 tail\n");
		AddFile("exp.txt", text);
		source = Load("exp.txt");
		Expect(source, TT_NAME, "marker");
		ExpectBoundedFailure(source, ReadUntilFailure(source, 1 << 25), 4096,
			"a 2^24-token expansion stops within one budget");
		Finish(source);
	}
	if (Want(7))
	{
		caseName = "mutual macro pair through the UI handle API";
		AddFile("ui/cycle.menu", "#define a b\n#define b a\n{ menuDef { name a } }\n");
		PS_SetBaseFolder("");
		errors = 0;
		lastError[0] = '\0';
		handle = PC_LoadSourceHandle("ui/cycle.menu", PC_OWNER_UI);
		Check(handle > 0, "menu handle loads");
		for (i = 0; i < 4; i++) Check(PC_ReadTokenHandle(handle, &pctoken), "menu prefix tokens");
		Check(!PC_ReadTokenHandle(handle, &pctoken) && errors == 1 && strstr(lastError, "macro expansion exceeds"),
			"menu macro cycle fails the trap read");
		PC_FreeSourceHandle(handle);
		Finish(NULL);
	}
}

/* --- include chains ------------------------------------------------------- */

static void IncludeChain(int files, int accepted)
{
	char name[64], body[128];
	source_t *source;
	int i;

	for (i = 0; i < files; i++)
	{
		snprintf(name, sizeof(name), "inc%d.h", i);
		if (i + 1 < files) snprintf(body, sizeof(body), "#include \"inc%d.h\"\n", i + 1);
		else snprintf(body, sizeof(body), "leaf\n");
		AddFile(name, body);
	}
	AddFile("chain.txt", "first\n#include \"inc0.h\"\nafter\n");
	source = Load("chain.txt");
	Expect(source, TT_NAME, "first");
	if (accepted)
	{
		Expect(source, TT_NAME, "leaf");
		Expect(source, TT_NAME, "after");
		ExpectEnd(source);
	}
	else
	{
		Check(!ReadUntilFailure(source, 1), "chain beyond the include depth fails");
		Check(errors == 1 && strstr(lastError, "active source files") != NULL, "one include-depth diagnostic");
	}
	Finish(source);
}

static void Includes(void)
{
	caseName = "100-file include chain";
	IncludeChain(100, 0);
	caseName = "63-file include chain";
	IncludeChain(63, 1);
	caseName = "64-file include chain";
	IncludeChain(64, 0);
}

/* --- legitimate input that must stay unbounded ---------------------------- */

static void Goldens(void)
{
	source_t *source;
	int i;

	caseName = "large ordinary source";
	Begin();
	for (i = 0; i < 20000; i++) Append("x%d ", i % 7);
	AddFile("plain.txt", text);
	source = Load("plain.txt");
	Check(ReadUntilFailure(source, 30000) == 20000 && !errors, "every ordinary token is published");
	Finish(source);

	/* about 1000 live token copies at most on retail data (342 measured);
	   4000 fit the live token budget */
	caseName = "large definitions, skipped block and header";
	Begin();
	Append("#define BIG");
	for (i = 0; i < 1000; i++) Append(" b");
	Append("\n#define HUGE");
	for (i = 0; i < 1000; i++) Append(" h");
	Append("\n#if 0\n");
	for (i = 0; i < 5000; i++) Append("itemDef { name skipped rect 0 0 1 1 }\n");
	Append("#endif\n");
	for (i = 0; i < 1000; i++) Append("#define LOCAL_%d %d\n", i, i);
	Append("LOCAL_999 BIG end\n");
	AddFile("large.txt", text);
	source = Load("large.txt");
	Expect(source, TT_NUMBER, "999");
	for (i = 0; i < 1000; i++) Expect(source, TT_NAME, "b");
	Expect(source, TT_NAME, "end");
	ExpectEnd(source);
	Finish(source);

	caseName = "single definition beyond the live token budget";
	Begin();
	Append("#define HUGE");
	for (i = 0; i < 5000; i++) Append(" h");
	Append("\nmarker HUGE\n");
	AddFile("huge.txt", text);
	source = Load("huge.txt");
	ExpectFailure(source, ReadUntilFailure(source, 1), 1, "a 5000-token definition is rejected",
		"preprocessor holds more than 4096 tokens");
	Finish(source);

	caseName = "single expansion beyond the live token budget";
	Begin();
	Append("#define HUGE");
	for (i = 0; i < 3000; i++) Append(" h");
	Append("\nmarker HUGE\n");
	AddFile("huge.txt", text);
	source = Load("huge.txt");
	Expect(source, TT_NAME, "marker");
	ExpectFailure(source, ReadUntilFailure(source, 1), 1, "a 3000-token expansion of a 3000-token definition is rejected",
		"preprocessor holds more than 4096 tokens");
	Finish(source);
}

/* --- resource budgets (follow-up to #469) --------------------------------- */

static void ExpectBudgetFailure(source_t *source, int published, int limit, const char *message, const char *diagnostic)
{
	ExpectFailure(source, published, limit, message, diagnostic);
	Check(peakBytes < 8 * 1024 * 1024, "the failing source stays far below the Mac zone");
}

/* the directive fails after its expression, which may leave the next
   line's first token unread; FreeSource releases it */
static void ExpectNestingFailure(source_t *source, const char *message)
{
	Check(!ReadUntilFailure(source, 10), message);
	Check(errors == 1 && strstr(lastError, "more than 4096 nested conditionals") != NULL,
		"one nesting diagnostic");
	Check(PC_SourceHasError(source), "source keeps its error state");
}

static void Budgets(void)
{
	char name[64], body[128];
	source_t *source;
	size_t loaded;
	int i, j, hash;

	if (Want(10))
	{
		/* each file includes the next one twice: 2^40 includes, depth 42 */
		caseName = "include fan-out";
		for (i = 0; i <= 40; i++)
		{
			snprintf(name, sizeof(name), "fan%d.h", i);
			if (i < 40) snprintf(body, sizeof(body), "#include \"fan%d.h\"\n#include \"fan%d.h\"\n", i + 1, i + 1);
			else snprintf(body, sizeof(body), "leaf\n");
			AddFile(name, body);
		}
		AddFile("fan.txt", "first\n#include \"fan0.h\"\nafter\n");
		source = Load("fan.txt");
		Expect(source, TT_NAME, "first");
		ExpectBudgetFailure(source, ReadUntilFailure(source, 1 << 20), 256, "fan-out stops after 256 includes",
			"more than 256 included files");
		Finish(source);
	}
	if (Want(11))
	{
		caseName = "256 sequential includes";
		AddFile("one.h", "leaf\n");
		Begin();
		for (i = 0; i < 256; i++) Append("#include \"one.h\"\n");
		Append("end\n");
		AddFile("seq.txt", text);
		source = Load("seq.txt");
		for (i = 0; i < 256; i++) Expect(source, TT_NAME, "leaf");
		Expect(source, TT_NAME, "end");
		ExpectEnd(source);
		Finish(source);

		caseName = "257 sequential includes";
		AddFile("one.h", "leaf\n");
		Begin();
		for (i = 0; i < 257; i++) Append("#include \"one.h\"\n");
		Append("end\n");
		AddFile("seq.txt", text);
		source = Load("seq.txt");
		ExpectFailure(source, ReadUntilFailure(source, 1000), 257, "the 257th include fails",
			"more than 256 included files");
		Finish(source);
	}
	for (hash = 1; hash >= 0; hash--)
	{
		/* 600 copies of a 199-token macro: 120,000 copied operands in one
		   expression, where the evaluator can use 64 values */
		if (!Want(hash ? 12 : 13)) continue;
		caseName = hash ? "#if operand list" : "$evalint operand list";
		Begin();
		Append("#define A 1");
		for (i = 0; i < 99; i++) Append(" + 1");
		Append(hash ? "\nmarker\n#if A" : "\nmarker $evalint(A");
		for (i = 1; i < 600; i++) Append(" + A");
		Append(hash ? "\nyes\n#endif\ntail\n" : ") tail\n");
		AddFile("operands.txt", text);
		source = Load("operands.txt");
		Expect(source, TT_NAME, "marker");
		ExpectBudgetFailure(source, ReadUntilFailure(source, 1), 1, "the operand list stops at the expression budget",
			"expression longer than 1024 tokens");
		Finish(source);
	}
	if (Want(14))
	{
		/* (((... defined X ...))) with 1024 and 1025 tokens */
		caseName = "1024-token expression";
		Begin();
		Append("#if ");
		for (i = 0; i < 511; i++) Append("(");
		Append("defined X");
		for (i = 0; i < 511; i++) Append(")");
		Append("\nno\n#endif\nend\n");
		AddFile("expr.txt", text);
		source = Load("expr.txt");
		Expect(source, TT_NAME, "end");
		ExpectEnd(source);
		Finish(source);

		caseName = "1025-token expression";
		Begin();
		Append("#if ");
		for (i = 0; i < 511; i++) Append("(");
		Append("!defined X");
		for (i = 0; i < 511; i++) Append(")");
		Append("\nyes\n#endif\nend\n");
		AddFile("expr.txt", text);
		source = Load("expr.txt");
		ExpectFailure(source, ReadUntilFailure(source, 1), 1, "a 1025-token expression is rejected",
			"expression longer than 1024 tokens");
		Finish(source);
	}
	if (Want(15))
	{
		/* 1000 definitions of 100 tokens: 100,000 retained copies (about
		   110 MB on the Mac, where the zone has 16 MB) */
		caseName = "retained definitions";
		Begin();
		for (i = 0; i < 1000; i++)
		{
			Append("#define D%d", i);
			for (j = 0; j < 100; j++) Append(" 1");
			Append("\n");
		}
		Append("marker\n");
		AddFile("defines.txt", text);
		source = Load("defines.txt");
		ExpectBudgetFailure(source, ReadUntilFailure(source, 1), 1, "definitions stop at the live token budget",
			"preprocessor holds more than 4096 tokens");
		Finish(source);
	}
	if (Want(16))
	{
		caseName = "100000-token macro argument";
		Begin();
		Append("#define F(x) x\nmarker F(");
		for (i = 0; i < 100000; i++) Append(" 1");
		Append(") tail\n");
		AddFile("argument.txt", text);
		source = Load("argument.txt");
		Expect(source, TT_NAME, "marker");
		ExpectBudgetFailure(source, ReadUntilFailure(source, 1), 1, "the argument stops at the live token budget",
			"preprocessor holds more than 4096 tokens");
		Finish(source);
	}
	if (Want(17))
	{
		/* conditionals nest through a heap list and never recurse, but each
		   open one costs a zone block (44 bytes on the Mac) for 6 bytes of
		   text: 4096 may be open in one source, retail nests 1 deep */
		caseName = "4096 nested conditionals";
		Begin();
		for (i = 0; i < 4096; i++) Append(i & 1 ? "#ifndef NOPE\n" : "#if 1\n");
		Append("x\n");
		for (i = 0; i < 4096; i++) Append("#endif\n");
		Append("end\n");
		AddFile("nest.txt", text);
		source = Load("nest.txt");
		Expect(source, TT_NAME, "x");
		Expect(source, TT_NAME, "end");
		ExpectEnd(source);
		Finish(source);

		caseName = "4097 nested conditionals";
		Begin();
		for (i = 0; i < 4097; i++) Append("#if 1\n");
		Append("x\n");
		for (i = 0; i < 4097; i++) Append("#endif\n");
		Append("end\n");
		AddFile("nest.txt", text);
		source = Load("nest.txt");
		ExpectNestingFailure(source, "the 4097th open conditional fails");
		Finish(source);

		/* the count follows the stack: an included file's unclosed
		   conditionals are closed at its end, so 2 x 3000 never reach it */
		caseName = "unclosed conditionals closed by the end of each include";
		Begin();
		for (i = 0; i < 3000; i++) Append("#if 1\n");
		Append("leaf\n");
		AddFile("open.h", text);
		AddFile("twice.txt", "#if 1\n#include \"open.h\"\n#include \"open.h\"\n#endif\nend\n");
		source = Load("twice.txt");
		Expect(source, TT_NAME, "leaf");
		Expect(source, TT_NAME, "leaf");
		Expect(source, TT_NAME, "end");
		ExpectEnd(source);
		Finish(source);
	}
	if (Want(18))
	{
		/* 80,000 nested conditionals (480 KB, skipped) took about 3.5 MB of
		   the Mac zone with no cap, so a .menu of about 2 MB filled its 16 MB */
		caseName = "80000 nested conditionals";
		Begin();
		for (i = 0; i < 80000; i++) Append("#if 0\n");
		Append("x\n");
		AddFile("deep.txt", text);
		source = Load("deep.txt");
		loaded = liveBytes;
		peakBytes = liveBytes;
		ExpectNestingFailure(source, "deep nesting stops at the cap");
		Check(peakBytes - loaded < 256 * 1024, "open conditionals stay far below the Mac zone");
		Finish(source);
	}
}

static void *Run(void *argument)
{
	int which = *(int *)argument;
	if (which == 0 || which == 1) AdjacentStrings();
	if (which == 0 || which == 2) MacroCycles();
	if (which == 0 || which == 3) Includes();
	if (which == 0 || which == 4) Goldens();
	if (which == 0 || which == 5) Budgets();
	return NULL;
}

static void Hang(int signal)
{
	static const char message[] = "FAIL: preprocessor case did not finish (hang)\n";
	(void)signal;
	if (write(2, message, sizeof(message) - 1) < 0) _exit(2);
	_exit(1);
}

int main(int argc, char **argv)
{
	pthread_attr_t attributes;
	pthread_t thread;
	int which = argc > 1 ? atoi(argv[1]) : 0;

	if (argc > 2) onlyCase = atoi(argv[2]);

	botimport.Print = Print;
	botimport.FS_FOpenFile = FileOpen;
	botimport.FS_Read = FileRead;
	botimport.FS_FCloseFile = FileClose;
	signal(SIGALRM, Hang);
	alarm(30);
	pthread_attr_init(&attributes);
	pthread_attr_setstacksize(&attributes, FIXTURE_STACK);
	if (pthread_create(&thread, &attributes, Run, &which)) {fprintf(stderr, "thread\n"); return 2;}
	pthread_join(thread, NULL);
	if (failures)
	{
		fprintf(stderr, "%d bot preprocessor bounds check(s) failed\n", failures);
		return 1;
	}
	puts("Bot preprocessor adjacent strings, macro cycles, includes, expressions, conditionals and live tokens are bounded (issue #48)");
	return 0;
}
