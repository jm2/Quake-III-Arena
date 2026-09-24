/* Issue #414: FS_FOpenFileWrite, FS_FOpenFileAppend, FS_Rename and the
 * FS_SV_* write and rename paths checked the extension of the name they were
 * given, then built the OS path with FS_BuildOSPath, which silently cuts it at
 * MAX_OSPATH (256 on the Retro68 target).  A name whose OS path is 256 or 257
 * characters long, such as "<pad>.pk3x", passed the check and was created as
 * "<pad>.pk3"; a server-chosen download name could turn the .tmp file or the
 * trusted final rename into a loose .qvm.
 *
 * run_fs_write_ospath_tests.sh builds this file around the real files.c three
 * ways:
 *   (default)             MAX_OSPATH 256, as on Retro68, and a '/' separator
 *   -DQ3_TEST_HFS         MAX_OSPATH 256 and the Classic Mac OS PATH_SEP ':'
 *   -DQ3_TEST_HOST_OSPATH MAX_OSPATH = the host PATH_MAX
 * FS_InitFilesystem takes fs_basepath and a padded fs_homepath from the
 * command line (Com_StartupVariable -> Cvar_Set -> FS_Startup's Cvar_Get), so
 * a name directly in the game directory reaches MAX_OSPATH.  VM writes and
 * appends (FS_FOpenFileByMode), FS_WriteFile, FS_Rename, FS_SV_FOpenFileWrite
 * and FS_SV_Rename (safe, and trusted as in the download finalisation) get
 * names whose OS path is MAX_OSPATH - 1, MAX_OSPATH and MAX_OSPATH + 1
 * characters and ends in .pk3 / .qvm, or in .pk3x / .qvmx that the cut would
 * turn into .pk3 / .qvm.  Each is refused before any fopen, mkdir, rename or
 * remove and before Com_sprintf ever cuts a path; a planted pk3 of the cut
 * name stays intact.  Ordinary writes, appends and renames, including names
 * that fill exactly MAX_OSPATH - 1 characters, still reach the same OS paths
 * as before. */
#include <limits.h>
#ifndef Q3_TEST_HOST_OSPATH
/* The Retro68 headers have no PATH_MAX, so q_shared.h picks 256. */
#undef PATH_MAX
#endif
#include "../code/game/q_shared.h"
#ifdef Q3_TEST_HFS
#undef PATH_SEP
#define PATH_SEP ':'
#endif
#if !defined(Q3_TEST_HOST_OSPATH) && MAX_OSPATH != 256
#error "the target build must have MAX_OSPATH 256"
#endif
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static FILE *FixtureOpen(const char *path, const char *mode);
static int FixtureRename(const char *from, const char *to);
static int FixtureRemove(const char *path);
#define fopen FixtureOpen
#define rename FixtureRename
#define remove FixtureRemove
#include "../code/qcommon/files.c"
#undef fopen
#undef rename
#undef remove

#define BASE_DIR "base"
#define M MAX_OSPATH

static char home[MAX_OSPATH];
static char lastOpen[MAX_OSPATH * 2], lastFrom[MAX_OSPATH * 2], lastTo[MAX_OSPATH * 2];
static int opens, renames, removes, mkdirs, warnings, overflows, zoneLive;
qboolean com_fullyInitialized;
cvar_t *com_journal;
fileHandle_t com_journalDataFile;
static cvar_t *cvars[32];
static int numCvars;

static void Check(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "FS write OS path regression failed (%s, MAX_OSPATH %d): %s\n",
#ifdef Q3_TEST_HFS
                "HFS PATH_SEP ':'",
#else
                "host PATH_SEP",
#endif
                M, message);
        exit(1);
    }
}

static void Copy(char *out, size_t size, const char *in) {
    Check(strlen(in) < size, "bounded fixture copy");
    strcpy(out, in);
}

/* Every write-mode open, rename, remove and directory the filesystem makes. */
static FILE *FixtureOpen(const char *path, const char *mode) {
    if (strchr(mode, 'w') || strchr(mode, 'a')) {
        opens++;
        Copy(lastOpen, sizeof(lastOpen), path);
    }
    return fopen(path, mode);
}
static int FixtureRename(const char *from, const char *to) {
    renames++;
    Copy(lastFrom, sizeof(lastFrom), from);
    Copy(lastTo, sizeof(lastTo), to);
    return rename(from, to);
}
static int FixtureRemove(const char *path) {
    removes++;
    return remove(path);
}
static void Reset(void) {
    opens = renames = removes = mkdirs = warnings = overflows = 0;
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
void QDECL Com_Printf(const char *format, ...) {
    char text[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    warnings += strstr(text, "WARNING") != NULL;
    overflows += strstr(text, "Com_sprintf: overflow") != NULL;
}
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
    mkdirs++;
    mkdir(path, 0777);
}
char **Sys_ListFiles(const char *directory, const char *extension, char *filter, int *numfiles, qboolean wantsubs) {
    (void)directory; (void)extension; (void)filter; (void)wantsubs;
    *numfiles = 0;
    return NULL;
}
void Sys_FreeFileList(char **list) { Check(list == NULL, "no platform list to free"); }
char *Sys_DefaultCDPath(void) { return ""; }
char *Sys_DefaultInstallPath(void) { return ""; }
char *Sys_DefaultHomePath(void) { return ""; }
void Cmd_AddCommand(const char *name, xcommand_t function) { (void)name; (void)function; }
void Cmd_RemoveCommand(const char *name) { (void)name; }
int Cmd_Argc(void) { return 0; }
char *Cmd_Argv(int arg) { (void)arg; return ""; }
void Com_ReadCDKey(const char *filename) { (void)filename; }
void Com_AppendCDKey(const char *filename) { (void)filename; }

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
cvar_t *Cvar_Get(const char *name, const char *value, int flags) {
    cvar_t *var = FindCvar(name, value);
    var->flags |= flags;
    return var;
}
void Cvar_Set(const char *name, const char *value) {
    cvar_t *var = FindCvar(name, value);
    free(var->string);
    var->string = strdup(value);
    var->integer = atoi(value);
    var->modified = qtrue;
}
/* "+set fs_basepath base +set fs_homepath <padded>" on the command line. */
void Com_StartupVariable(const char *match) {
    if (!strcmp(match, "fs_basepath")) {
        Cvar_Set(match, BASE_DIR);
    } else if (!strcmp(match, "fs_homepath")) {
        Cvar_Set(match, home);
    }
}
static void FreeCvars(void) {
    while (numCvars) {
        cvar_t *var = cvars[--numCvars];
        free(var->name);
        free(var->string);
        free(var);
    }
}

/* A relative home path of MAX_OSPATH - 25 characters, in components short
 * enough for the host, so "<home>/baseq3/" is MAX_OSPATH - 17 long. */
static void BuildHome(void) {
    int i, n = M - 25;
    for (i = 0; i < n; i++) {
        home[i] = (i && i % 50 == 0 && i != n - 1) ? PATH_SEP : 'a' + i % 26;
    }
    home[n] = 0;
}

/* dir + stem + ext whose OS path is length characters.  An FS_SV_* name
 * starts with "baseq3/" and its length counts the trailing separator
 * FS_BuildOSPath( home, name, "" ) adds.  The stem depends only on its
 * length, so "<stem>.pk3x" at MAX_OSPATH and "<stem>.pk3" at
 * MAX_OSPATH - 1 share it. */
static const char *Name(int length, int sv, const char *dir, const char *ext) {
    static char names[4][MAX_OSPATH * 2];
    static int toggle;
    char *out = names[toggle++ & 3];
    int i, prefix = (int)strlen(home) + (sv ? 9 : 8);
    int stem = length - prefix - (int)strlen(dir) - (int)strlen(ext);
    int n = snprintf(out, sizeof(names[0]), "%s%s", sv ? BASEGAME "/" : "", dir);
    Check(stem > 0 && length < (int)sizeof(names[0]), "fixture name fits");
    for (i = 0; i < stem; i++) {
        out[n + i] = 'a' + i % 26;
    }
    strcpy(out + n + stem, ext);
    return out;
}

/* The OS path the engine uses for a game directory qpath or an FS_SV_* name. */
static const char *OSName(int sv, const char *name) {
    static char paths[4][MAX_OSPATH * 2];
    static int toggle;
    char *out = paths[toggle++ & 3];
    char *s;
    Check(snprintf(out, sizeof(paths[0]), "%s/%s%s", home, sv ? "" : BASEGAME "/", name) <
          (int)sizeof(paths[0]), "bounded fixture path");
    for (s = out; *s; s++) {
        if (*s == '/') {
            *s = PATH_SEP;
        }
    }
    return out;
}

static int Exists(const char *ospath) {
    return access(ospath, F_OK) == 0;
}

static int ReadAll(const char *ospath, char *buffer, int size) {
    FILE *in = fopen(ospath, "rb");
    int n;
    if (!in) {
        return -1;
    }
    n = (int)fread(buffer, 1, size, in);
    fclose(in);
    return n;
}

static void Plant(const char *ospath, const char *payload) {
    char path[MAX_OSPATH * 2];
    FILE *out;
    Copy(path, sizeof(path), ospath);
    FS_CreatePath(path);
    out = fopen(ospath, "wb");
    Check(out != NULL, "plant fixture file");
    fputs(payload, out);
    fclose(out);
}

static void Contains(const char *ospath, const char *payload, const char *message) {
    char text[64];
    int n = ReadAll(ospath, text, sizeof(text));
    Check(n == (int)strlen(payload) && !memcmp(text, payload, n), message);
}

static void ReadBack(const char *qpath, const char *payload, const char *message) {
    void *buffer = NULL;
    int n = FS_ReadFile(qpath, &buffer);
    Check(n == (int)strlen(payload) && buffer && !memcmp(buffer, payload, n), message);
    FS_FreeFile(buffer);
}

/* Nothing reached the OS, Com_sprintf never cut a path, and it was reported. */
static void Untouched(const char *message) {
    if (opens || renames || removes || mkdirs || overflows || !warnings) {
        fprintf(stderr, "opens %d renames %d removes %d mkdirs %d overflows %d warnings %d (last %s)\n",
                opens, renames, removes, mkdirs, overflows, warnings, opens ? lastOpen : lastTo);
        Check(0, message);
    }
}

static void Startup(void) {
    char path[64];

    BuildHome();
    snprintf(path, sizeof(path), "%s%c%s%cdefault.cfg", BASE_DIR, PATH_SEP, BASEGAME, PATH_SEP);
    Plant(path, "// default\n");
    /* The full game's productid.txt, so FS_SetRestrictions keeps baseq3. */
    snprintf(path, sizeof(path), "%s%c%s%cproductid.txt", BASE_DIR, PATH_SEP, BASEGAME, PATH_SEP);
    Plant(path, "This file is copyright 1999 Id Software, and may not be duplicated except "
          "during a licensed installation of the full commercial version of Quake 3:Arena");
    FS_InitFilesystem();
    Check(fs_homepath && !strcmp(fs_homepath->string, home), "fs_homepath comes from the cvar");
    Check(!strcmp(fs_gamedir, BASEGAME), "full game directory");
    Check((int)strlen(OSName(0, "")) == M - 17, "game directory OS path is MAX_OSPATH - 17 long");
}

static void Normal(void) {
    const char *edge = Name(M - 1, 0, "", ".pk3x");
    const char *edgeMoved = Name(M - 1, 0, "", ".txtx");
    const char *svEdge = Name(M - 1, 1, "", ".pk3x");
    const char *svMoved = Name(M - 1, 1, "", ".datx");
    fileHandle_t f;

    Reset();
    Check(FS_FOpenFileByMode("ui_saved.txt", &f, FS_WRITE) == 0 && f > 0 &&
          FS_Write("saved\n", 6, f) == 6, "VM write still works");
    FS_FCloseFile(f);
    Check(opens == 1 && !strcmp(lastOpen, OSName(0, "ui_saved.txt")), "VM write OS path unchanged");
    Check(FS_FOpenFileByMode("ui_saved.txt", &f, FS_APPEND) == 0 && f > 0 &&
          FS_Write("more\n", 5, f) == 5, "VM append still works");
    FS_FCloseFile(f);
    Check(opens == 2 && !strcmp(lastOpen, OSName(0, "ui_saved.txt")), "VM append OS path unchanged");
    ReadBack("ui_saved.txt", "saved\nmore\n", "VM write/append payload");
    FS_WriteFile("demos/test.dm_68", "demo", 4);
    Check(opens == 3 && !strcmp(lastOpen, OSName(0, "demos/test.dm_68")), "FS_WriteFile OS path unchanged");
    ReadBack("demos/test.dm_68", "demo", "FS_WriteFile payload");
    FS_WriteFile("src.txt", "src\n", 4);
    Check(!warnings && !overflows, "ordinary writes print no warning");

    /* A name that fills exactly MAX_OSPATH - 1 characters is kept whole. */
    Reset();
    Check((int)strlen(OSName(0, edge)) == M - 1, "edge name is MAX_OSPATH - 1 long");
    Check(FS_FOpenFileByMode(edge, &f, FS_WRITE) == 0 && f > 0 && FS_Write("edge", 4, f) == 4,
          "VM write of an exact-fit name still works");
    FS_FCloseFile(f);
    Check(FS_FOpenFileByMode(edge, &f, FS_APPEND_SYNC) == 0 && f > 0 && FS_Write("+", 1, f) == 1,
          "VM append of an exact-fit name still works");
    FS_FCloseFile(f);
    Check(opens == 2 && !strcmp(lastOpen, OSName(0, edge)), "exact-fit OS path is not cut");
    Contains(OSName(0, edge), "edge+", "exact-fit file keeps its full name");
    FS_Rename(edge, edgeMoved);
    Check(renames == 1 && !strcmp(lastFrom, OSName(0, edge)) && !strcmp(lastTo, OSName(0, edgeMoved)) &&
          !Exists(OSName(0, edge)), "exact-fit rename still works");
    Contains(OSName(0, edgeMoved), "edge+", "exact-fit rename payload");

    Check((int)strlen(OSName(1, svEdge)) == M - 2, "FS_SV_* edge name builds a MAX_OSPATH - 1 path");
    f = FS_SV_FOpenFileWrite(svEdge);
    Check(f > 0 && FS_Write("sv", 2, f) == 2, "home-path write of an exact-fit name still works");
    FS_FCloseFile(f);
    Check(opens == 3 && !strcmp(lastOpen, OSName(1, svEdge)), "home-path exact-fit OS path is not cut");
    FS_SV_Rename(svEdge, svMoved, qtrue);
    Check(renames == 2 && !strcmp(lastTo, OSName(1, svMoved)) && !Exists(OSName(1, svEdge)),
          "home-path exact-fit rename still works");
    Contains(OSName(1, svMoved), "sv", "home-path exact-fit rename payload");

    /* CL_ParseDownload: FS_SV_FOpenFileWrite(<name>.pk3.tmp), then trusted rename. */
    f = FS_SV_FOpenFileWrite(BASEGAME "/newmap.pk3.tmp");
    Check(f > 0 && FS_Write("PK\003\004", 4, f) == 4, "download temp file write");
    FS_FCloseFile(f);
    FS_SV_Rename(BASEGAME "/newmap.pk3.tmp", BASEGAME "/newmap.pk3", qfalse);
    Check(!strcmp(lastTo, OSName(0, "newmap.pk3")) && !Exists(OSName(0, "newmap.pk3.tmp")),
          "trusted download finalisation still produces the pk3");
    Contains(OSName(0, "newmap.pk3"), "PK\003\004", "download payload");
    Check(!warnings && !overflows, "ordinary edge writes print no warning");
}

typedef struct {
    int delta;              /* OS path length - MAX_OSPATH */
    const char *dir, *ext;
    const char *cutExt;     /* what FS_BuildOSPath's cut would leave */
} refusedName_t;

static const refusedName_t refusedNames[] = {
    { -1, "", ".pk3", ".pk3" },     /* fits, and already a pak name */
    { 0, "", ".pk3x", ".pk3" },
    { 1, "", ".pk3xx", ".pk3" },
    { -1, "vm/", ".qvm", ".qvm" },
    { 0, "vm/", ".qvmx", ".qvm" },
    { 1, "vm/", ".qvmxx", ".qvm" },
};

static void Refused(const refusedName_t *r, const char *planted) {
    static const fsMode_t modes[] = { FS_WRITE, FS_APPEND, FS_APPEND_SYNC };
    char q[MAX_OSPATH * 2], sv[MAX_OSPATH * 2], cut[MAX_OSPATH * 2], svCut[MAX_OSPATH * 2];
    fileHandle_t f;
    int i;

    Copy(q, sizeof(q), Name(M + r->delta, 0, r->dir, r->ext));
    Copy(sv, sizeof(sv), Name(M + r->delta, 1, r->dir, r->ext));
    Copy(cut, sizeof(cut), OSName(0, Name(M - 1, 0, r->dir, r->cutExt)));
    Copy(svCut, sizeof(svCut), OSName(1, Name(M - 1, 1, r->dir, r->cutExt)));
    Check((int)strlen(OSName(0, q)) == M + r->delta, "qpath OS path length");
    Check((int)strlen(OSName(1, sv)) + 1 == M + r->delta, "FS_SV_* built OS path length");
    for (i = 0; i < 3; i++) {
        Reset();
        f = -1;
        Check(FS_FOpenFileByMode(q, &f, modes[i]) == -1 && f == 0, "VM write/append is refused");
        Untouched("refused VM open reaches no OS path");
    }
    Reset();
    FS_WriteFile(q, "evil", 4);
    Untouched("refused FS_WriteFile reaches no OS path");
    Reset();
    FS_Rename("src.txt", q);
    Untouched("rename onto the name is refused");
    Reset();
    FS_Rename(q, "moved.txt");
    Untouched("rename of the name away is refused");

    Reset();
    Check(FS_SV_FOpenFileWrite(sv) == 0, "home-path write is refused");
    Untouched("refused home-path write reaches no OS path");
    Reset();
    FS_SV_Rename(BASEGAME "/src.txt", sv, qtrue);
    Untouched("home-path rename onto the name is refused");
    Reset();
    FS_SV_Rename(sv, BASEGAME "/moved.txt", qtrue);
    Untouched("home-path rename of the name away is refused");
    if (r->delta >= 0) {
        /* The download finalisation skips the extension check, not the cut. */
        Reset();
        FS_SV_Rename(BASEGAME "/src.txt", sv, qfalse);
        Untouched("trusted rename onto a cut name is refused");
        Reset();
        FS_SV_Rename(sv, BASEGAME "/moved.txt", qfalse);
        Untouched("trusted rename of a cut name away is refused");
    }

    Check(Exists(OSName(0, "src.txt")) && !Exists(OSName(0, "moved.txt")), "rename source stays put");
    if (planted) {
        Contains(cut, planted, "planted pak of the cut name stays intact");
        Contains(svCut, planted, "planted home-path pak of the cut name stays intact");
    } else {
        Check(!Exists(cut) && !Exists(svCut), "no file is created under the cut name");
    }
    if (r->delta >= 0) {
        Check(!Exists(OSName(0, q)) && !Exists(OSName(1, sv)), "no file is created under the full name");
    }
}

/* A server picks the download pair; CL_BeginDownload only asks for a relative
 * "<game>/<name>.pk3" shorter than MAX_OSPATH - 4. */
static void Downloads(const char *planted) {
    char qvm[MAX_OSPATH * 2], tmp[MAX_OSPATH * 2], local[MAX_OSPATH * 2];
    fileHandle_t f;

    Copy(qvm, sizeof(qvm), OSName(1, Name(M - 1, 1, "vm/", ".qvm")));
    /* Cut at MAX_OSPATH - 1 and stripped of its last character, each ends in ".qvm". */
    Copy(tmp, sizeof(tmp), Name(M + 7, 1, "vm/", ".qvm.pk3.tmp"));
    Copy(local, sizeof(local), Name(M + 3, 1, "vm/", ".qvm.pk3"));
    Check(strlen(tmp) - 4 < MAX_OSPATH - 4 && strlen(local) < MAX_OSPATH - 4,
          "download names pass CL_BeginDownload's length check");
    Reset();
    Check(FS_SV_FOpenFileWrite(tmp) == 0, "download temp file whose cut path is a qvm is refused");
    Untouched("refused download temp file reaches no OS path");

    f = FS_SV_FOpenFileWrite(BASEGAME "/dl.pk3.tmp");
    Check(f > 0 && FS_Write("PK\003\004", 4, f) == 4, "short download temp file");
    FS_FCloseFile(f);
    Reset();
    FS_SV_Rename(BASEGAME "/dl.pk3.tmp", local, qfalse);
    Untouched("trusted rename whose cut path is a qvm is refused");
    Check(Exists(OSName(0, "dl.pk3.tmp")), "refused download stays in its temp file");
    if (planted) {
        Contains(qvm, planted, "planted qvm stays intact");
    } else {
        Check(!Exists(qvm), "no qvm is created from a download");
    }
}

#ifdef Q3_TEST_HFS
/* HFS drops a doubled separator, so the name used ends in ".pk3" although
 * the one given ends in ".pk3/". */
static void FinalComponent(void) {
    Reset();
    Check(FS_SV_FOpenFileWrite(BASEGAME "/zz.pk3/") == 0, "HFS home-path write of zz.pk3/ is refused");
    Check(!opens && !Exists(OSName(1, BASEGAME "/zz.pk3")), "HFS zz.pk3/ never creates zz.pk3");
}
#endif

int main(int argc, char **argv) {
    int i, n = (int)(sizeof(refusedNames) / sizeof(refusedNames[0]));

    Check(argc == 2 && !chdir(argv[1]), "usage: fs_write_ospath_regression workdir");
    Startup();
    Normal();
    for (i = 0; i < n; i++) {
        Refused(&refusedNames[i], NULL);
    }
    Downloads(NULL);
    /* Now with paks already under the cut names, as a download leaves them. */
    for (i = 0; i < n; i++) {
        Plant(OSName(0, Name(M - 1, 0, refusedNames[i].dir, refusedNames[i].cutExt)), "PK\003\004planted");
        Plant(OSName(1, Name(M - 1, 1, refusedNames[i].dir, refusedNames[i].cutExt)), "PK\003\004planted");
    }
    for (i = 0; i < n; i++) {
        Refused(&refusedNames[i], "PK\003\004planted");
    }
    Downloads("PK\003\004planted");
#ifdef Q3_TEST_HFS
    FinalComponent();
#endif
    FS_Shutdown(qtrue);
    for (i = 0; i < MAX_FILE_HANDLES; i++) {
        Check(!fsh[i].handleFiles.file.o && !fsh[i].buffer, "all handles closed");
    }
    FreeCvars();
    Check(!zoneLive, "zone released");
    printf("FS write OS path guards pass (MAX_OSPATH %d)\n", M);
    return 0;
}
