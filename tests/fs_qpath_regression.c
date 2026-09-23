/* Issue #262: HFS ':' separators must not let a qpath or a game directory
 * walk out of the game directory.  The real files.c (and unzip.c for pk3
 * lookups) is built once with the Classic Mac OS PATH_SEP ':' (Q3_TEST_HFS)
 * and once with the host separator.  Every OS path the filesystem opens,
 * lists or creates is recorded, so a refused name must never reach one. */
#include "../code/game/q_shared.h"
#ifdef Q3_TEST_HFS
#undef PATH_SEP
#define PATH_SEP ':'
#endif
#include <sys/stat.h>
#include <sys/types.h>

static FILE *FixtureOpen(const char *path, const char *mode);
#define fopen FixtureOpen
#include "../code/qcommon/files.c"
#include "../code/qcommon/unzip.c"
#undef fopen

#define MAX_RECORDED 64
typedef struct {
    char path[MAX_RECORDED][MAX_OSPATH * 2];
    int count;
} record_t;

static record_t opened, listed, made;
static char base[MAX_OSPATH], pk3Path[MAX_OSPATH];
static int zoneLive;
qboolean com_fullyInitialized;
cvar_t *com_journal;
fileHandle_t com_journalDataFile;
static cvar_t *cvars[32];
static int numCvars, appendedKeys;
static searchpath_t packPath, dirPath;
static directory_t dirEntry;

static void Check(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "qpath regression failed (%s): %s\n",
#ifdef Q3_TEST_HFS
                "HFS PATH_SEP ':'",
#else
                "host PATH_SEP",
#endif
                message);
        exit(1);
    }
}

static void Record(record_t *record, const char *path) {
    Check(record->count < MAX_RECORDED, "bounded path record");
    Q_strncpyz(record->path[record->count++], path, sizeof(record->path[0]));
}

static FILE *FixtureOpen(const char *path, const char *mode) {
    if (strcmp(path, pk3Path)) {
        Record(&opened, path);
    }
    return fopen(path, mode);
}

/* Nothing after the base path may walk up: a doubled ':' on HFS, a ".."
 * component or a stray ':' elsewhere. */
static int Inside(const char *ospath) {
    size_t n = strlen(base);
    if (strncmp(ospath, base, n)) {
        return 0;
    }
#ifdef Q3_TEST_HFS
    return ospath[n] == ':' && !strstr(ospath + n, "::");
#else
    return ospath[n] == '/' && !strstr(ospath + n, "..") && !strchr(ospath + n, ':');
#endif
}

static void CheckRecorded(const record_t *record, const char *message) {
    int i;
    for (i = 0; i < record->count; i++) {
        if (!Inside(record->path[i])) {
            fprintf(stderr, "escaping OS path: %s\n", record->path[i]);
            Check(0, message);
        }
    }
}

static void Reset(void) {
    opened.count = listed.count = made.count = 0;
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
void Hunk_ClearTempMemory(void) {}
int Com_FilterPath(char *filter, char *name, int casesensitive) {
    (void)filter; (void)name; (void)casesensitive;
    return 0;
}
void S_ClearSoundBuffer(void) {}
void Sys_BeginStreamedFile(fileHandle_t f, int readAhead) { (void)f; (void)readAhead; }
void Sys_EndStreamedFile(fileHandle_t f) { (void)f; }
int Sys_StreamedRead(void *buffer, int size, int count, fileHandle_t f) {
    return FS_Read(buffer, size * count, f);
}
void Sys_Mkdir(const char *path) {
    Record(&made, path);
    mkdir(path, 0777);
}
char **Sys_ListFiles(const char *directory, const char *extension, char *filter, int *numfiles, qboolean wantsubs) {
    (void)extension; (void)filter; (void)wantsubs;
    Record(&listed, directory);
    *numfiles = 0;
    return NULL;
}
void Sys_FreeFileList(char **list) { Check(list == NULL, "no platform list to free"); }
char *Sys_DefaultCDPath(void) { return ""; }
char *Sys_DefaultInstallPath(void) { return base; }
char *Sys_DefaultHomePath(void) { return ""; }
void Cmd_AddCommand(const char *name, xcommand_t function) { (void)name; (void)function; }
void Cmd_RemoveCommand(const char *name) { (void)name; }
int Cmd_Argc(void) { return 0; }
char *Cmd_Argv(int arg) { (void)arg; return ""; }
void Com_ReadCDKey(const char *filename) { (void)filename; }
void Com_AppendCDKey(const char *filename) {
    (void)filename;
    appendedKeys++;
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
    var->name = strdup(name);
    var->string = strdup(value);
    var->integer = atoi(value);
    cvars[numCvars++] = var;
    return var;
}
cvar_t *Cvar_Get(const char *name, const char *value, int flags) {
    (void)flags;
    return FindCvar(name, value);
}
void Cvar_Set(const char *name, const char *value) {
    cvar_t *var = FindCvar(name, value);
    free(var->string);
    var->string = strdup(value);
    var->integer = atoi(value);
    var->modified = qtrue;
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
static void Mount(void) {
    memset(fsh, 0, sizeof(fsh));
    fs_debug = FindCvar("fs_debug", "0");
    fs_restrict = FindCvar("fs_restrict", "0");
    fs_copyfiles = FindCvar("fs_copyfiles", "0");
    fs_homepath = FindCvar("fs_homepath", base);
    fs_basepath = FindCvar("fs_basepath", base);
    fs_cdpath = FindCvar("fs_cdpath", "");
    Q_strncpyz(fs_gamedir, BASEGAME, sizeof(fs_gamedir));
    Q_strncpyz(dirEntry.path, base, sizeof(dirEntry.path));
    Q_strncpyz(dirEntry.gamedir, BASEGAME, sizeof(dirEntry.gamedir));
    dirPath.dir = &dirEntry;
    packPath.pack = FS_LoadZipFile(pk3Path, "retail");
    Check(packPath.pack != NULL, "retail-layout pk3 mounts");
    packPath.next = &dirPath;
    fs_searchpaths = &packPath;
}

static void Unmount(void) {
    int i;
    for (i = 0; i < MAX_FILE_HANDLES; i++) {
        Check(!fsh[i].handleFiles.file.o && !fsh[i].buffer, "all handles closed");
    }
    unzClose(packPath.pack->handle);
    Z_Free(packPath.pack->buildBuffer);
    Z_Free(packPath.pack);
    packPath.pack = NULL;
    fs_searchpaths = NULL;
}

static const char *const hostileNames[] = {
    ":/:/System Folder/Finder",     /* issue #262: ":baseq3:::::System Folder:Finder" */
    "/:x",
    "a/:/b",
    ":x",
    "::",
    "::x",
    "a:b",
    "maps/q3dm1.bsp:",
    "c:/autoexec.bat",
    "..",
    "../x",
    "a/../../b",
    NULL
};

/* Harmless on the host, but every doubled or leading separator walks up
 * one HFS directory once FS_ReplaceSeparators turns it into ':'. */
static const char *const hfsHostileNames[] = {
    "a//b",
    "//x",
    "\\\\x",
    "a\\\\b",
    "a/\\b",
    "models//players/sarge/head.md3",
    "textures//base_wall/bluemetal1light.tga", /* retail base_wall.shader */
    NULL
};

static void Refused(const char *name) {
    fileHandle_t f = 123;
    char **list;
    int n = 77;

    Reset();
    Check(FS_FOpenFileRead(name, &f, qtrue) == -1 && f == 0, "hostile qpath read refused");
    Check(FS_FOpenFileRead(name, &f, qfalse) == -1 && f == 0, "hostile shared qpath read refused");
    Check(FS_FOpenFileRead(name, NULL, qfalse) == qfalse, "hostile qpath existence check refused");
    Check(FS_ReadFile(name, NULL) == -1, "hostile qpath length refused");
    list = FS_ListFiles(name, ".md3", &n);
    Check(list == NULL && n == 0, "hostile list path refused");
    list = FS_ListFilteredFiles(name, "", NULL, &n);
    Check(list == NULL && n == 0, "hostile filtered list path refused");
    Check(FS_FOpenFileWrite(name) == 0, "hostile qpath write refused");
    Check(FS_FOpenFileAppend(name) == 0, "hostile qpath append refused");
    Check(FS_FOpenFileByMode(name, &f, FS_WRITE) == -1 && f == 0, "hostile VM write refused");
    if (opened.count || listed.count || made.count) {
        fprintf(stderr, "refused name reached the OS: \"%s\" (%s)\n", name,
                opened.count ? opened.path[0] : listed.count ? listed.path[0] : made.path[0]);
        Check(0, "refused qpath never builds an OS path");
    }
}

static void ReadRetail(const char *name, const char *payload) {
    void *buffer = NULL;
    fileHandle_t f;
    char text[64];
    int len = (int)strlen(payload);

    Reset();
    Check(FS_ReadFile(name, &buffer) == len && !memcmp(buffer, payload, len), "retail qpath loads");
    FS_FreeFile(buffer);
    Check(FS_FOpenFileRead(name, &f, qtrue) == len && f > 0, "retail qpath opens");
    Check(FS_Read(text, sizeof(text), f) == len && !memcmp(text, payload, len), "retail qpath reads");
    FS_FCloseFile(f);
    /* Only full opens strip a leading slash, as in 1.32c. */
    Check(name[0] == '/' || FS_FOpenFileRead(name, NULL, qfalse) == qtrue, "retail qpath exists");
    CheckRecorded(&opened, "retail qpath stays inside the game directory");
}

/* Create a loose file where the platform's separator puts it, so HFS
 * mode reads "<base>:baseq3:scripts:loose.txt" through real stdio. */
static void BuildLoose(const char *qpath, const char *payload) {
    char ospath[MAX_OSPATH * 2];
    const char *s;
    char *d;
    FILE *out;

    mkdir(base, 0777);
    Com_sprintf(ospath, sizeof(ospath), "%s%c%s", base, PATH_SEP, BASEGAME);
    mkdir(ospath, 0777);
    d = ospath + strlen(ospath);
    *d++ = PATH_SEP;
    for (s = qpath; *s; s++) {
        if (*s == '/') {
            *d = 0;
            mkdir(ospath, 0777);
            *d++ = PATH_SEP;
        } else {
            *d++ = *s;
        }
    }
    *d = 0;
    out = fopen(ospath, "wb");
    Check(out != NULL, "loose fixture file");
    fputs(payload, out);
    fclose(out);
}

static void OSPaths(void) {
#ifdef Q3_TEST_HFS
    Check(!strcmp(FS_BuildOSPath("Macintosh HD:Quake3", BASEGAME, "models/players/sarge/head.md3"),
                  "Macintosh HD:Quake3:baseq3:models:players:sarge:head.md3"), "HFS retail OS path");
    Check(!strcmp(FS_BuildOSPath("Macintosh HD:Quake3", BASEGAME, ":/:/System Folder/Finder"),
                  "Macintosh HD:Quake3:baseq3:System Folder:Finder"), "HFS doubled separators never walk up");
    Check(!strcmp(FS_BuildOSPath("Macintosh HD:Quake3", BASEGAME, "a//b"),
                  "Macintosh HD:Quake3:baseq3:a:b"), "HFS doubled slash collapses");
    Check(!strcmp(FS_BuildOSPath("Macintosh HD:Quake3", "::", "x"),
                  "Macintosh HD:Quake3:x"), "HFS game directory cannot walk up");
    Check(!strcmp(FS_BuildOSPath("", BASEGAME, "/:x"), ":baseq3:x"), "HFS relative base keeps one leading ':'");
    Check(!strcmp(FS_BuildOSPath("::Quake3", BASEGAME, "x"), "::Quake3:baseq3:x"), "user base path is not rewritten");
#else
    Check(!strcmp(FS_BuildOSPath("/q3", BASEGAME, "models/players/sarge/head.md3"),
                  "/q3/baseq3/models/players/sarge/head.md3"), "host retail OS path");
    Check(!strcmp(FS_BuildOSPath("/q3", BASEGAME, "a//b\\c"), "/q3/baseq3/a//b/c"), "host separators unchanged");
#endif
}

static void QPaths(void) {
    static const char *const retailEntries[][2] = {
        { "models/players/sarge/head.md3", "head\n" },
        { "sound/player/sarge/death1.wav", "death\n" },
        { "scripts/base_wall.shader", "shader\n" },
        { "textures/base_wall/bluemetal1light.tga", "tga\n" },
        { "maps/q3dm1.bsp", "bsp\n" },
        { "botfiles/bots/sarge_c.c", "bot\n" },
        { "levelshots/q3dm1.jpg", "shot\n" },
        { "video/intro.roq", "roq\n" },
        { NULL, NULL }
    };
    fileHandle_t f;
    char **list;
    char listbuf[256];
    int i, n;

    Mount();
    for (i = 0; hostileNames[i]; i++) {
        Refused(hostileNames[i]);
    }
#ifdef Q3_TEST_HFS
    for (i = 0; hfsHostileNames[i]; i++) {
        Refused(hfsHostileNames[i]);
    }
#else
    /* The host keeps accepting doubled separators; nothing walks up. */
    Reset();
    Check(FS_FOpenFileRead("textures//base_wall/bluemetal1light.tga", &f, qtrue) == -1 && f == 0,
          "doubled-slash retail reference never matches the pk3, as in 1.32c");
    Check(opened.count == 1, "host still looks for doubled-slash loose files");
    CheckRecorded(&opened, "host doubled slash stays inside");
#endif

    for (i = 0; retailEntries[i][0]; i++) {
        ReadRetail(retailEntries[i][0], retailEntries[i][1]);
    }
    BuildLoose("scripts/loose.txt", "loose\n");
    ReadRetail("scripts/loose.txt", "loose\n");
    ReadRetail("/scripts/loose.txt", "loose\n");
    BuildLoose("q3config.cfg", "cfg\n");
    ReadRetail("q3config.cfg", "cfg\n");

    Reset();
    list = FS_ListFiles("scripts", ".shader", &n);
    Check(n == 1 && list && !strcmp(list[0], "base_wall.shader"), "retail list path");
    FS_FreeFileList(list);
    Check(listed.count == 1, "retail list path scans the game directory");
    CheckRecorded(&listed, "retail list path stays inside");
    Reset();
    Check(FS_GetFileList("models/players/sarge", "md3", listbuf, sizeof(listbuf)) == 1 &&
          !strcmp(listbuf, "head.md3"), "retail VM file list");
    CheckRecorded(&listed, "retail VM list stays inside");

    Reset();
    f = FS_FOpenFileWrite("demos/test.dm_68");
    Check(f > 0, "retail demo record path");
    Check(FS_Write("demo", 4, f) == 4, "retail demo write");
    FS_FCloseFile(f);
    CheckRecorded(&opened, "retail write stays inside");
    f = FS_FOpenFileAppend("demos/test.dm_68");
    Check(f > 0, "retail append path");
    FS_FCloseFile(f);
    ReadRetail("demos/test.dm_68", "demo");
    Unmount();
}

/* fs_game and fs_basegame come from servers (systeminfo), VMs and the
 * command line; FS_Startup is shared by FS_InitFilesystem and FS_Restart. */
static void Startup(const char *cvarName, const char *value, int accepted) {
    char expected[MAX_OSPATH * 2];
    cvar_t *var;
    int i, found = 0;

    Reset();
    appendedKeys = 0;
    FreeCvars();
    var = FindCvar(cvarName, value);
    FS_Startup(BASEGAME);
    CheckRecorded(&listed, "game directory scan stays inside the install folder");
    Com_sprintf(expected, sizeof(expected), "%s%c%s", base, PATH_SEP, value);
    for (i = 0; i < listed.count; i++) {
        found |= !strcmp(listed.path[i], expected);
    }
    if (accepted) {
        Check(found && !strcmp(var->string, value), "legal game directory is searched");
    } else {
        Check(!found && !var->string[0], "illegal game directory is dropped");
        Check(!strcmp(fs_gamedir, BASEGAME) && !appendedKeys, "illegal game directory is never used");
    }
    FS_Shutdown(qfalse);
    Check(!zoneLive, "search paths released");
}

static void GameDirs(void) {
    static const char *const hostileDirs[] = {
        "::", ":x", "x:", "a:b", "Macintosh HD:System Folder", "..", ".", "../x", "x/y", "/x", "x\\y", NULL
    };
    int i;

    for (i = 0; hostileDirs[i]; i++) {
        Startup("fs_game", hostileDirs[i], 0);
        Startup("fs_basegame", hostileDirs[i], 0);
    }
    Startup("fs_game", "osp", 1);
    Startup("fs_game", "missionpack", 1);
    Startup("fs_basegame", "missionpack", 1);
}

int main(int argc, char **argv) {
    Check(argc == 3, "usage: fs_qpath_regression retail.pk3 workdir");
    Q_strncpyz(pk3Path, argv[1], sizeof(pk3Path));
    Com_sprintf(base, sizeof(base), "%s/q3", argv[2]);
    OSPaths();
    QPaths();
    GameDirs();
    FreeCvars();
    Check(!zoneLive, "zone released");
    return 0;
}
