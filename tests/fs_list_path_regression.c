/* Issue #384: FS_ListFilteredFiles read path[pathLength-1] before checking
 * that the list path had a character.  The base Load Config menu and Team
 * Arena's UI_LoadTeams list "" (trap_FS_GetFileList( "", "cfg" / "team" )),
 * and so does fdir, so each read the byte before the caller's string.  When
 * that byte was a separator, as for a "" literal merged into the tail of a
 * string such as "levelshots/", pathLength became -1 and every pk3 entry was
 * dropped from the list.  The real files.c and unzip.c list a pk3 laid out
 * like pak0.pk3 (default.cfg and other configs at the root) plus one loose
 * config.  Every path lives in an exact-sized heap block, so ASan reports
 * any read before it. */
#include "../code/game/q_shared.h"
#include "../code/qcommon/files.c"
#include "../code/qcommon/unzip.c"

static char base[MAX_OSPATH], rootDir[MAX_OSPATH * 2];
static int zoneLive;
qboolean com_fullyInitialized;
cvar_t *com_journal;
fileHandle_t com_journalDataFile;
static cvar_t *cvars[16];
static int numCvars;
static searchpath_t packPath, dirPath;
static directory_t dirEntry;

static void Check(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "FS list path regression failed: %s\n", message);
        exit(1);
    }
}

/* Engine imports. */
void QDECL Com_Error(int level, const char *format, ...) {
    (void)level; (void)format;
    Check(0, "unexpected engine error");
}
void QDECL Com_Printf(const char *format, ...) { (void)format; }
void QDECL Com_DPrintf(const char *format, ...) { (void)format; }
void QDECL Com_FlightRecord(const char *format, ...) { (void)format; }
void Com_Memset(void *out, int value, size_t size) { memset(out, value, size); }
void Com_Memcpy(void *out, const void *in, size_t size) { memcpy(out, in, size); }

void *Z_Malloc(int size) {
    void *p = calloc(1, size ? size : 1);
    Check(p != NULL, "zone allocation");
    zoneLive++;
    return p;
}
void Z_Free(void *p) {
    Check(p != NULL, "zone free of NULL");
    zoneLive--;
    free(p);
}
char *CopyString(const char *in) {
    char *out = Z_Malloc(strlen(in) + 1);
    strcpy(out, in);
    return out;
}
void *Hunk_AllocateTempMemory(int size) { return malloc(size ? size : 1); }
void Hunk_FreeTempMemory(void *p) { free(p); }
/* Kept by the linker for file reads; listing never opens a file. */
void S_ClearSoundBuffer(void) { Check(0, "S_ClearSoundBuffer"); }
void Sys_EndStreamedFile(fileHandle_t f) { (void)f; Check(0, "Sys_EndStreamedFile"); }

/* fdir filters are "*suffix" here, which is all the tests need. */
static int FilterMatch(const char *filter, const char *name) {
    size_t f, n = strlen(name);
    Check(filter[0] == '*', "fixture filter form");
    f = strlen(filter + 1);
    return n >= f && !Q_stricmp(name + n - f, filter + 1);
}
int Com_FilterPath(char *filter, char *name, int casesensitive) {
    (void)casesensitive;
    return FilterMatch(filter, name);
}

/* The loose game directory holds one config at its root: "<base>/baseq3/"
 * however many separators follow it, as for a real directory scan. */
char **Sys_ListFiles(const char *directory, const char *extension, char *filter, int *numfiles, qboolean wantsubs) {
    static const char loose[] = "q3config.cfg";
    size_t n = strlen(rootDir);
    char **list;
    (void)wantsubs;

    *numfiles = 0;
    if (strncmp(directory, rootDir, n) || strspn(directory + n, "/") != strlen(directory + n)) {
        return NULL;
    }
    if (filter ? !FilterMatch(filter, loose) :
        strlen(loose) < strlen(extension) || Q_stricmp(loose + strlen(loose) - strlen(extension), extension)) {
        return NULL;
    }
    list = malloc(2 * sizeof(*list));
    Check(list != NULL, "loose list allocation");
    list[0] = strdup(loose);
    list[1] = NULL;
    *numfiles = 1;
    return list;
}
void Sys_FreeFileList(char **list) {
    int i;
    for (i = 0; list && list[i]; i++) {
        free(list[i]);
    }
    free(list);
}

static cvar_t *FindCvar(const char *name, const char *value) {
    int i;
    cvar_t *var;
    for (i = 0; i < numCvars; i++) {
        if (!strcmp(cvars[i]->name, name)) {
            return cvars[i];
        }
    }
    Check(numCvars < (int)(sizeof(cvars) / sizeof(cvars[0])), "bounded cvars");
    var = calloc(1, sizeof(*var));
    Check(var != NULL, "cvar allocation");
    var->name = strdup(name);
    var->string = strdup(value);
    var->integer = atoi(value);
    cvars[numCvars++] = var;
    return var;
}
static void FreeCvars(void) {
    while (numCvars) {
        cvar_t *var = cvars[--numCvars];
        free(var->name);
        free(var->string);
        free(var);
    }
}

/* Search order pk3 -> base/baseq3 directory, as FS_Startup builds it. */
static void Mount(const char *pk3Path) {
    fs_debug = FindCvar("fs_debug", "0");
    fs_restrict = FindCvar("fs_restrict", "0");
    Q_strncpyz(fs_gamedir, BASEGAME, sizeof(fs_gamedir));
    Q_strncpyz(dirEntry.path, base, sizeof(dirEntry.path));
    Q_strncpyz(dirEntry.gamedir, BASEGAME, sizeof(dirEntry.gamedir));
    Q_strncpyz(rootDir, FS_BuildOSPath(base, BASEGAME, ""), sizeof(rootDir));
    dirPath.dir = &dirEntry;
    packPath.pack = FS_LoadZipFile((char *)pk3Path, "pak0");
    Check(packPath.pack != NULL, "pak0-layout pk3 mounts");
    packPath.next = &dirPath;
    fs_searchpaths = &packPath;
}

static void Unmount(void) {
    unzClose(packPath.pack->handle);
    Z_Free(packPath.pack->buildBuffer);
    Z_Free(packPath.pack);
    packPath.pack = NULL;
    fs_searchpaths = NULL;
}

/* A caller's path in a block of exactly strlen + 1 bytes. */
static char *Exact(const char *text) {
    char *copy = malloc(strlen(text) + 1);
    Check(copy != NULL, "path allocation");
    strcpy(copy, text);
    return copy;
}

/* want is the expected names, each followed by '\n'. */
static void Same(char **list, int n, const char *want, const char *message) {
    char got[512];
    int i;

    got[0] = 0;
    for (i = 0; i < n; i++) {
        Check(list && list[i] != NULL, "list entry");
        Q_strcat(got, sizeof(got), list[i]);
        Q_strcat(got, sizeof(got), "\n");
    }
    Check(!list || !list[n], "list terminator");
    if (strcmp(got, want)) {
        fprintf(stderr, "want:\n%sgot:\n%s", want, got);
        Check(0, message);
    }
}

/* trap_FS_GetFileList, the way the UI and cgame reach the list. */
static void GetFileList(const char *path, const char *extension, const char *want, const char *message) {
    char buffer[512], got[512];
    const char *s;
    int i, n;

    n = FS_GetFileList(path, extension, buffer, sizeof(buffer));
    got[0] = 0;
    for (i = 0, s = buffer; i < n; i++, s += strlen(s) + 1) {
        Q_strcat(got, sizeof(got), s);
        Q_strcat(got, sizeof(got), "\n");
    }
    if (strcmp(got, want)) {
        fprintf(stderr, "path \"%s\" extension \"%s\"\nwant:\n%sgot:\n%s", path, extension, want, got);
        Check(0, message);
    }
}

static const char rootCfgs[] = "ares.cfg\ndefault.cfg\nq3config.cfg\n";

static void EmptyPath(void) {
    char *path = Exact("");
    char **list;
    int n = -1;

    GetFileList(path, "cfg", rootCfgs, "Load Config menu lists every root config");
    GetFileList(path, "team", "", "UI_LoadTeams finds no team files in this layout");
    list = FS_ListFilteredFiles(path, "", "*.cfg", &n);
    Same(list, n, rootCfgs, "fdir *.cfg lists every root config");
    FS_FreeFileList(list);
    list = FS_ListFiles(path, ".cfg", &n);
    Same(list, n, rootCfgs, "FS_ListFiles( \"\", \".cfg\" ) lists every root config");
    FS_FreeFileList(list);
    free(path);
}

/* A "" that follows a separator, as a literal merged into the tail of
 * another string may.  The separator belongs to that other string. */
static void EmptyPathAfterSeparator(void) {
    static const char *const tails[] = { "levelshots/", "botfiles\\", "x", NULL };
    char *owner;
    int i;

    for (i = 0; tails[i]; i++) {
        owner = Exact(tails[i]);
        GetFileList(owner + strlen(owner), "cfg", rootCfgs,
                    "a separator before \"\" does not drop the pk3 configs");
        free(owner);
    }
}

/* One- and few-character paths list exactly as before. */
static void ShortPaths(void) {
    static const char *const cases[][3] = {
        { "/", "cfg", rootCfgs },
        { "\\", "cfg", rootCfgs },
        { "a", "cfg", "" },
        { "scripts", "shader", "base_wall.shader\n" },
        { "scripts/", "shader", "base_wall.shader\n" },
        { "levelshots", "jpg", "q3dm1.jpg\n" },
        { NULL, NULL, NULL }
    };
    char *path;
    int i;

    for (i = 0; cases[i][0]; i++) {
        path = Exact(cases[i][0]);
        GetFileList(path, cases[i][1], cases[i][2], "short list path");
        free(path);
    }
}

int main(int argc, char **argv) {
    Check(argc == 3, "usage: fs_list_path_regression pak0.pk3 workdir");
    Com_sprintf(base, sizeof(base), "%s/q3", argv[2]);
    Mount(argv[1]);
    EmptyPath();
    EmptyPathAfterSeparator();
    ShortPaths();
    Unmount();
    FreeCvars();
    Check(!zoneLive, "zone released");
    puts("FS list paths never read before \"\" and list as before (issue #384)");
    return 0;
}
