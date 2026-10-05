/* Issue #235: retail started Team Arena with "+set fs_game missionpack", but a
 * Finder launch passes no command line (Retro68 calls main(1, ...)), so the
 * Quake3_TeamArena executable mounted only baseq3 and its UI could not find
 * ui/menus.txt.  CMake now compiles that executable with
 * DEFAULT_FS_GAME="missionpack"; Quake3 keeps the empty default.
 *
 * run_fs_default_game_tests.sh builds this file around the real files.c,
 * cvar.c, cmd.c, unzip.c and common.c's Com_StartupVariable twice: with no
 * DEFAULT_FS_GAME (Quake3) and with the one CMakeLists.txt gives
 * Quake3_TeamArena.  Each run starts the filesystem once, from argv[2..] as
 * the command line ("set fs_game mymod", as Com_ParseCommandLine splits
 * "+set fs_game mymod"), over an install of baseq3, missionpack and mymod
 * pk3s, and checks the search path FS_InitFilesystem builds against
 * argv[1]'s expected game directory:
 *   baseq3       baseq3 alone, and no ui/menus.txt
 *   missionpack  missionpack's directory and pk3 over baseq3's
 *   mymod        the command line's game over baseq3, never missionpack
 * FS_Restart must then keep the same search path. */
#include <dirent.h>
#include "../code/qcommon/files.c"

#define MAX_CONSOLE_LINES 32	/* common.c */

qboolean com_fullyInitialized;
cvar_t *com_journal;
fileHandle_t com_journalDataFile;
static char install[MAX_OSPATH];
static const char *expectGame;

static void Check(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "FS default game regression failed (DEFAULT_FS_GAME \"%s\", expecting %s): %s\n",
                DEFAULT_FS_GAME, expectGame ? expectGame : "?", message);
        exit(1);
    }
}

/* Engine imports. */
void QDECL Com_Error(int level, const char *format, ...) {
    va_list args;
    (void)level;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    Check(0, "unexpected engine error");
}
void QDECL Com_Printf(const char *format, ...) { (void)format; }
void QDECL Com_DPrintf(const char *format, ...) { (void)format; }
void QDECL Com_FlightRecord(const char *format, ...) { (void)format; }
void Com_Memset(void *out, const int value, const size_t size) { memset(out, value, size); }
void Com_Memcpy(void *out, const void *in, const size_t size) { memcpy(out, in, size); }
void *Z_Malloc(int size) {
    void *p = calloc(1, size ? size : 1);
    Check(p != NULL, "zone allocation");
    return p;
}
void *S_Malloc(int size) { return Z_Malloc(size); }
void Z_Free(void *p) { free(p); }
char *CopyString(const char *in) {
    char *out = Z_Malloc((int)strlen(in) + 1);
    strcpy(out, in);
    return out;
}
void *Hunk_AllocateTempMemory(int size) { return Z_Malloc(size); }
void Hunk_FreeTempMemory(void *p) { free(p); }
void Hunk_ClearTempMemory(void) {}
int Com_Filter(char *filter, char *name, int casesensitive) {
    (void)filter; (void)name; (void)casesensitive;
    return 0;
}
int Com_FilterPath(char *filter, char *name, int casesensitive) {
    (void)filter; (void)name; (void)casesensitive;
    return 0;
}
qboolean Com_SafeMode(void) { return qfalse; }
void Com_ReadCDKey(const char *filename) { (void)filename; }
void Com_AppendCDKey(const char *filename) { (void)filename; }
void S_ClearSoundBuffer(void) {}
void Sys_BeginStreamedFile(fileHandle_t f, int readAhead) { (void)f; (void)readAhead; }
void Sys_EndStreamedFile(fileHandle_t f) { (void)f; }
int Sys_StreamedRead(void *buffer, int size, int count, fileHandle_t f) {
    return FS_Read(buffer, size * count, f);
}
void Sys_Mkdir(const char *path) { (void)path; }
/* mac_main.c: the application's folder, no CD and no separate home. */
char *Sys_DefaultCDPath(void) { return ""; }
char *Sys_DefaultInstallPath(void) { return install; }
char *Sys_DefaultHomePath(void) { return ""; }
/* The pk3s in one game directory (one each, so paksort's order is moot). */
char **Sys_ListFiles(const char *directory, const char *extension, char *filter, int *numfiles, qboolean wantsubs) {
    char **list = Z_Malloc(4 * sizeof(*list));
    struct dirent *entry;
    DIR *dir = opendir(directory);
    size_t n;

    Check(extension && !strcmp(extension, ".pk3") && !filter && !wantsubs, "only pk3 scans");
    *numfiles = 0;
    while (dir && (entry = readdir(dir)) != NULL) {
        n = strlen(entry->d_name);
        if (n > 4 && !strcmp(entry->d_name + n - 4, ".pk3")) {
            Check(*numfiles < 3, "bounded pk3 list");
            list[(*numfiles)++] = CopyString(entry->d_name);
        }
    }
    if (dir) {
        closedir(dir);
    }
    return list;
}
void Sys_FreeFileList(char **list) {
    int i;
    for (i = 0; list && list[i]; i++) {
        free(list[i]);
    }
    free(list);
}
/* cmd.c's other imports: no command is ever executed here. */
void CL_ForwardCommandToServer(const char *string) { (void)string; Check(0, "unexpected forward"); }
qboolean SV_GameCommand(void) { Check(0, "unexpected game command"); return qfalse; }
qboolean UI_GameCommand(void) { Check(0, "unexpected ui command"); return qfalse; }
qboolean CL_GameCommand(void) { Check(0, "unexpected cgame command"); return qfalse; }

/* The command line, as Com_ParseCommandLine leaves it, and Com_StartupVariable
 * from common.c itself (the runner extracts it). */
int com_numConsoleLines;
char *com_consoleLines[MAX_CONSOLE_LINES];
#include Q3_COM_STARTUP_VARIABLE

/** The game directory of each search path, highest priority first. */
static void SearchPath(char *out, size_t size) {
    searchpath_t *s;

    out[0] = 0;
    for (s = fs_searchpaths; s; s = s->next) {
        if (out[0]) {
            Q_strcat(out, size, " ");
        }
        if (s->pack) {
            Q_strcat(out, size, s->pack->pakGamename);
            Q_strcat(out, size, "/");
            Q_strcat(out, size, s->pack->pakBasename);
            Q_strcat(out, size, ".pk3");
        } else {
            Q_strcat(out, size, s->dir->gamedir);
            Q_strcat(out, size, "/");
        }
    }
}

static int Readable(const char *qpath) {
    void *buffer = NULL;
    int n = FS_ReadFile(qpath, &buffer);
    if (buffer) {
        FS_FreeFile(buffer);
    }
    return n > 0;
}

static void CheckMounted(const char *when) {
    char path[1024], expect[1024];

    SearchPath(path, sizeof(path));
    if (!strcmp(expectGame, BASEGAME)) {
        Q_strncpyz(expect, BASEGAME "/pak0.pk3 " BASEGAME "/", sizeof(expect));
    } else {
        Com_sprintf(expect, sizeof(expect), "%s/pak0.pk3 %s/ " BASEGAME "/pak0.pk3 " BASEGAME "/",
                    expectGame, expectGame);
    }
    if (strcmp(path, expect)) {
        fprintf(stderr, "%s search path: \"%s\", expected \"%s\"\n", when, path, expect);
        Check(0, "search path");
    }
    Check(!strcmp(fs_gamedir, expectGame), "fs_gamedir");
    Check(!strcmp(Cvar_VariableString("fs_game"), strcmp(expectGame, BASEGAME) ? expectGame : ""),
          "fs_game value");
    Check(Readable("default.cfg"), "baseq3's default.cfg is readable");
    Check(Readable("ui/menus.txt") == !strcmp(expectGame, "missionpack"),
          "missionpack's ui/menus.txt is readable exactly when missionpack is mounted");
    Check(Readable("mymod.txt") == !strcmp(expectGame, "mymod"),
          "mymod's file is readable exactly when mymod is mounted");
}

int main(int argc, char **argv) {
    int i;

    Check(argc >= 3 && argc - 3 < MAX_CONSOLE_LINES,
          "usage: fs_default_game_regression install expected-game [command-line-set ...]");
    Q_strncpyz(install, argv[1], sizeof(install));
    expectGame = argv[2];
    for (i = 3; i < argc; i++) {
        com_consoleLines[com_numConsoleLines++] = argv[i];
    }

    Cvar_Init();
    Cmd_Init();
    /* Com_Init: the whole command line, then the filesystem. */
    Com_StartupVariable(NULL);
    FS_InitFilesystem();
    CheckMounted("FS_InitFilesystem");
    Check((Cvar_Flags("fs_game") & (CVAR_INIT | CVAR_SYSTEMINFO)) == (CVAR_INIT | CVAR_SYSTEMINFO),
          "fs_game keeps its flags");

    /* A server's or a mod's pak change restarts the filesystem; the game stays. */
    FS_Restart(fs_checksumFeed);
    CheckMounted("FS_Restart");
    FS_Shutdown(qtrue);
    printf("FS default game: \"%s\" -> %s (%d command line set%s)\n", DEFAULT_FS_GAME, expectGame,
           argc - 3, argc - 3 == 1 ? "" : "s");
    return 0;
}
