/* Actual FS startup/restart search paths, local-config precedence and the
 * CVE-2017-6903 write/rename/remove extension guards against real ZIPs. */
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
/* The retail product-ID gate in FS_SetRestrictions is unrelated to config and
 * pak precedence (tests/run_fs_product_id_tests.sh covers it). */
#define PRE_RELEASE_DEMO
#define Q3_ZIP_ZONE_CAPACITY 64
#define main FixtureZipMain
#ifndef Q3_ZIP_NATIVE_FIXTURE
#define Q3_ZIP_NATIVE_FIXTURE "fs_zip_regression.c"
#endif
#include Q3_ZIP_NATIVE_FIXTURE
#undef main

typedef struct {
    cvar_t var;
    char name[32];
    char string[MAX_OSPATH];
} fixtureCvar_t;

static fixtureCvar_t cvars[24];
static int cvarCount;
static char root[MAX_OSPATH], execText[256];

static fixtureCvar_t *FixtureCvar(const char *name, const char *value) {
    int i;
    fixtureCvar_t *c;
    for (i = 0; i < cvarCount; i++) if (!strcmp(cvars[i].name, name)) return &cvars[i];
    Check(cvarCount < (int)(sizeof(cvars) / sizeof(cvars[0])) &&
          strlen(name) < sizeof(cvars[0].name), "bounded fixture cvars");
    c = &cvars[cvarCount++];
    strcpy(c->name, name);
    c->var.name = c->name;
    c->var.string = c->string;
    Check(strlen(value) < sizeof(c->string), "bounded fixture cvar value");
    strcpy(c->string, value);
    c->var.integer = atoi(value);
    c->var.value = atof(value);
    return c;
}

cvar_t *Cvar_Get(const char *name, const char *value, int flags) {
    fixtureCvar_t *c = FixtureCvar(name, value);
    c->var.flags |= flags;
    return &c->var;
}

void Cvar_Set(const char *name, const char *value) {
    fixtureCvar_t *c = FixtureCvar(name, value);
    Check(strlen(value) < sizeof(c->string), "bounded fixture cvar value");
    strcpy(c->string, value);
    c->var.integer = atoi(value);
    c->var.value = atof(value);
    c->var.modified = qtrue;
    c->var.modificationCount++;
}

void Com_StartupVariable(const char *match) { (void)match; }
qboolean Com_SafeMode(void) { return qfalse; }
void QDECL Com_FlightRecord(const char *fmt, ...) { (void)fmt; }
void Com_ReadCDKey(const char *filename) { (void)filename; }
void Com_AppendCDKey(const char *filename) { (void)filename; }
void Cmd_AddCommand(const char *name, xcommand_t function) { (void)name; (void)function; }
void Cmd_RemoveCommand(const char *name) { (void)name; }
void S_ClearSoundBuffer(void) {}
/* Console commands and pure-server lists are registered but never run here. */
int Cmd_Argc(void) { Check(0, "unexpected console command"); return 0; }
char *Cmd_Argv(int arg) { (void)arg; Check(0, "unexpected console command"); return ""; }
void Cmd_TokenizeString(const char *text) { (void)text; Check(0, "unexpected pure list"); }
char *CopyString(const char *in) { (void)in; Check(0, "unexpected file list"); return NULL; }
int Com_FilterPath(char *filter, char *name, int casesensitive) {
    (void)filter; (void)name; (void)casesensitive;
    Check(0, "unexpected filtered file list");
    return 0;
}
char *Sys_DefaultCDPath(void) { return ""; }
char *Sys_DefaultInstallPath(void) { return ""; }
char *Sys_DefaultHomePath(void) { return ""; }

void Cbuf_AddText(const char *text) {
    Check(strlen(execText) + strlen(text) < sizeof(execText), "bounded command buffer");
    strcat(execText, text);
}

void Sys_BeginStreamedFile(fileHandle_t f, int readahead) {
    (void)f; (void)readahead;
    Check(0, "unexpected streamed VM read");
}

void Sys_StreamSeek(fileHandle_t f, int offset, int origin) {
    (void)f; (void)offset; (void)origin;
    Check(0, "unexpected platform streaming seek");
}

/* Real directory listing; FS_AddGameDirectory sorts with a 4-byte element
 * width (32-bit PowerPC pointers), so every fixture directory holds at most
 * one pk3 and the LP64 host never sorts. */
char **Sys_ListFiles(const char *directory, const char *extension, char *filter,
                     int *numfiles, qboolean wantsubs) {
    DIR *dir;
    struct dirent *entry;
    char **list;
    int count = 0;
    size_t length, extensionLength = strlen(extension);
    Check(!filter && !wantsubs, "startup lists plain pk3 files only");
    list = calloc(2, sizeof(*list));
    Check(list != NULL, "native file list owner");
    dir = opendir(directory);
    while (dir && (entry = readdir(dir)) != NULL) {
        length = strlen(entry->d_name);
        if (length <= extensionLength ||
            Q_stricmp(entry->d_name + length - extensionLength, extension)) continue;
        Check(count == 0, "one pk3 per fixture directory");
        list[count] = strdup(entry->d_name);
        Check(list[count] != NULL, "native file list entry");
        count++;
    }
    if (dir) closedir(dir);
    *numfiles = count;
    return list;
}

void Sys_FreeFileList(char **list) {
    int i;
    if (!list) return;
    for (i = 0; list[i]; i++) free(list[i]);
    free(list);
}

static const char *Path(const char *relative) {
    static char paths[2][MAX_OSPATH * 2];
    static int toggle;
    toggle ^= 1;
    Check(snprintf(paths[toggle], sizeof(paths[0]), "%s/%s", root, relative) <
          (int)sizeof(paths[0]), "bounded fixture path");
    return paths[toggle];
}

static int Exists(const char *relative) {
    return access(Path(relative), F_OK) == 0;
}

static long Size(const char *relative) {
    struct stat st;
    Check(!stat(Path(relative), &st), "fixture file exists");
    return (long)st.st_size;
}

static void Expect(const char *name, const char *expected, const char *message) {
    void *buffer = NULL;
    int length = FS_ReadFile(name, &buffer);
    if (!expected) {
        Check(length == -1 && !buffer, message);
        return;
    }
    Check(length == (int)strlen(expected) && buffer &&
          !memcmp(buffer, expected, length), message);
    FS_FreeFile(buffer);
}

static void Probe(const char *name, int expected, const char *message) {
    Check(FS_FOpenFileByMode(name, NULL, FS_READ) == expected, message);
}

static void Configs(const char *q3config, const char *autoexec) {
    /* Com_Init and FS_Restart exec these names through FS_ReadFile. */
    Expect("default.cfg", "// retail default\n", "retail default.cfg still loads from pak0.pk3");
    Expect("q3config.cfg", q3config, "q3config.cfg never comes from a pk3");
    Expect("autoexec.cfg", autoexec, "autoexec.cfg never comes from a pk3");
    Expect("/q3config.cfg", q3config, "leading-slash q3config.cfg never comes from a pk3");
    Expect("/autoexec.cfg", autoexec, "leading-slash autoexec.cfg never comes from a pk3");
    Expect("Q3CONFIG.CFG", NULL, "case-folded q3config.cfg never comes from a pk3");
    Expect("AutoExec.Cfg", NULL, "case-folded autoexec.cfg never comes from a pk3");
    Probe("q3config.cfg", q3config != NULL, "existence probe ignores a pk3 q3config.cfg");
    Probe("autoexec.cfg", autoexec != NULL, "existence probe ignores a pk3 autoexec.cfg");
    Probe("default.cfg", 1, "existence probe still finds pak0 default.cfg");
}

static void RefusedOpen(const char *name, long existing) {
    static const fsMode_t modes[] = {FS_WRITE, FS_APPEND, FS_APPEND_SYNC};
    char relative[MAX_QPATH * 2];
    fileHandle_t f;
    int i;
    snprintf(relative, sizeof(relative), "home/evilmod/%s", name);
    for (i = 0; i < 3; i++) {
        f = -1;
        Check(FS_FOpenFileByMode(name, &f, modes[i]) == -1 && f == 0,
              "VM write/append to a code or pak extension is refused");
        if (existing < 0) Check(!Exists(relative), "refused VM open creates no file");
        else Check(Size(relative) == existing, "refused VM open leaves the pak intact");
    }
    Check(!FS_FOpenFileWrite(name) && !FS_FOpenFileAppend(name),
          "command-reachable write/append to a code or pak extension is refused");
    if (existing < 0) Check(!Exists(relative), "refused command open creates no file");
    else Check(Size(relative) == existing, "refused command open leaves the pak intact");
}

static void Writes(void) {
    static const char *refused[] = {
        "x.pk3", "zz.PK3", "vm/cgame.qvm", "vm/ui.QVM", "vm/qagame.qvm", "x.dll", "x.so"
    };
    fileHandle_t f;
    int i;
    long evilSize = Size("home/evilmod/evil.pk3");
    for (i = 0; i < (int)(sizeof(refused) / sizeof(refused[0])); i++) RefusedOpen(refused[i], -1);
    RefusedOpen("evil.pk3", evilSize);
    Check(!FS_SV_FOpenFileWrite("evilmod/x.pk3") && !Exists("home/evilmod/x.pk3") &&
          !FS_SV_FOpenFileWrite("baseq3/vm/cgame.qvm") && !Exists("home/baseq3/vm/cgame.qvm"),
          "home-path writes of code or pak extensions are refused");

    Check(FS_FOpenFileByMode("ui_saved.txt", &f, FS_WRITE) == 0 && f > 0 &&
          FS_Write("saved\n", 6, f) == 6, "VM .txt write still works");
    FS_FCloseFile(f);
    Check(FS_FOpenFileByMode("ui_saved.txt", &f, FS_APPEND) == 0 && f > 0 &&
          FS_Write("more\n", 5, f) == 5, "VM .txt append still works");
    FS_FCloseFile(f);
    Expect("ui_saved.txt", "saved\nmore\n", "VM .txt write/append payload");
    f = FS_FOpenFileWrite("q3config.cfg");
    Check(f > 0 && FS_Write("// written\n", 11, f) == 11, "writeconfig/q3config.cfg write still works");
    FS_FCloseFile(f);
    Expect("q3config.cfg", "// written\n", "written local q3config.cfg is read back");
}

static void Renames(void) {
    fileHandle_t f;
    FS_Rename("ui_saved.txt", "renamed.pk3");
    Check(Exists("home/evilmod/ui_saved.txt") && !Exists("home/evilmod/renamed.pk3"),
          "rename onto a pak extension is refused");
    FS_Rename("evil.pk3", "evil.txt");
    Check(Exists("home/evilmod/evil.pk3") && !Exists("home/evilmod/evil.txt"),
          "rename of a pak away is refused");
    FS_Rename("ui_saved.txt", "ui_moved.txt");
    Check(!Exists("home/evilmod/ui_saved.txt") && Exists("home/evilmod/ui_moved.txt"),
          "ordinary rename still works");
    FS_SV_Rename("evilmod/ui_moved.txt", "evilmod/vm/cgame.qvm", qtrue);
    Check(Exists("home/evilmod/ui_moved.txt") && !Exists("home/evilmod/vm/cgame.qvm"),
          "safe home-path rename onto a qvm is refused");
    FS_Remove(Path("home/evilmod/evil.pk3"));
    Check(Exists("home/evilmod/evil.pk3"), "remove of a pak is refused");
    FS_Remove(Path("home/evilmod/ui_moved.txt"));
    Check(!Exists("home/evilmod/ui_moved.txt"), "ordinary remove still works");

    /* CL_ParseDownload: FS_SV_FOpenFileWrite(<name>.pk3.tmp), then trusted rename. */
    f = FS_SV_FOpenFileWrite("evilmod/newmap.pk3.tmp");
    Check(f > 0 && FS_Write("PK\003\004", 4, f) == 4, "download temp file write");
    FS_FCloseFile(f);
    FS_SV_Rename("evilmod/newmap.pk3.tmp", "evilmod/newmap.pk3", qtrue);
    Check(Exists("home/evilmod/newmap.pk3.tmp") && !Exists("home/evilmod/newmap.pk3"),
          "untrusted home-path rename onto a pk3 is refused");
    FS_SV_Rename("evilmod/newmap.pk3.tmp", "evilmod/newmap.pk3", qfalse);
    Check(!Exists("home/evilmod/newmap.pk3.tmp") && Size("home/evilmod/newmap.pk3") == 4,
          "trusted download finalisation still produces the pk3");
}

int main(int argc, char **argv) {
    Check(argc == 2 && strlen(argv[1]) < sizeof(root), "fixture root");
    strcpy(root, argv[1]);
    Cvar_Set("fs_basepath", Path("base"));
    Cvar_Set("fs_homepath", Path("home"));
    Cvar_Set("fs_cdpath", "");
    Cvar_Set("fs_game", "");

    /* Startup: [zz_dl.pk3, home/baseq3, pak0.pk3, base/baseq3]. */
    FS_InitFilesystem();
    Expect("dl.txt", "downloaded pk3 data\n", "downloaded pk3 still serves ordinary names");
    Configs("// local q3config\n", "// local autoexec\n");
    Check(!unlink(Path("home/baseq3/autoexec.cfg")), "remove local autoexec.cfg");
    Configs("// local q3config\n", NULL);

    /* Server-driven game dir change: FS_Restart re-execs q3config.cfg. */
    Cvar_Set("fs_game", "evilmod");
    execText[0] = 0;
    FS_Restart(0);
    Check(!strcmp(execText, "exec q3config.cfg\n"), "FS_Restart re-execs q3config.cfg");
    Check(!strcmp(fs_gamedir, "evilmod"), "restart selects the downloaded game dir");
    Expect("evil.txt", "evil pk3 data\n", "mod pk3 still serves ordinary names");
    Configs("// local q3config\n", NULL);

    Writes();
    Renames();
    FS_Shutdown(qtrue);
    Check(!zoneLive && !temporaryLive && !fs_loadStack, "filesystem owners release physically");
    puts("Actual FS local-config precedence and mutable-extension guards pass");
    return 0;
}
