/* Issue #259: FS_SV_Rename and FS_Rename fall back to FS_CopyFile and then
 * FS_Remove when rename() fails, as it always did on the Mac while
 * libretro's _rename_r was a stub.  FS_CopyFile read the whole file into an
 * unchecked malloc, so a large download (a pk3) that did not fit read into
 * NULL, and the caller then removed the source anyway.  Now that
 * code/mac/mac_syscalls.c gives the Mac a real rename() and remove(), that
 * removal would really delete the only copy.
 *
 * Built around the real files.c with rename, remove and malloc wrapped:
 *   - the download finalisation (CL_ParseDownload's FS_SV_Rename of
 *     <name>.pk3.tmp, trusted) and FS_Rename with a working rename(): one
 *     rename, no copy, the .tmp is gone;
 *   - the same with rename() refused (the old Retro68 stub): the copy
 *     fallback still produces the pk3 and removes the .tmp;
 *   - a rename the mutable-name checks refuse stays refused on that path:
 *     no rename, copy or remove;
 *   - rename() refused and the copy's malloc failing: no crash, a warning,
 *     no destination, and the source stays where it was;
 *   - rename() refused and the copy's destination refused by FS_CreatePath
 *     or not openable: the copy buffer is freed (malloc and free are
 *     counted) and the source stays. */
#include "../code/game/q_shared.h"
#include <stdarg.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static FILE *FixtureOpen(const char *path, const char *mode);
static int FixtureRename(const char *from, const char *to);
static int FixtureRemove(const char *path);
static void *FixtureMalloc(size_t size);
static void FixtureFree(void *p);
#define fopen FixtureOpen
#define rename FixtureRename
#define remove FixtureRemove
#define malloc FixtureMalloc
#define free FixtureFree
#include "../code/qcommon/files.c"
#undef fopen
#undef rename
#undef remove
#undef malloc
#undef free

#define BASE_DIR "base"
#define HOME_DIR "home"

static int writeOpens, renames, removes, mallocs, mallocLive, warnings, zoneLive;
static int renameRefused, mallocFails;
qboolean com_fullyInitialized;
cvar_t *com_journal;
fileHandle_t com_journalDataFile;
static cvar_t *cvars[32];
static int numCvars;

static void Check(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "FS copy/rename regression failed: %s (write opens %d renames %d removes %d "
                "mallocs %d warnings %d)\n", message, writeOpens, renames, removes, mallocs, warnings);
        exit(1);
    }
}

static FILE *FixtureOpen(const char *path, const char *mode) {
    if (strchr(mode, 'w') || strchr(mode, 'a')) {
        writeOpens++;
    }
    return fopen(path, mode);
}
/* With renameRefused, rename() fails as Retro68's libretro stub did. */
static int FixtureRename(const char *from, const char *to) {
    renames++;
    if (renameRefused) {
        errno = EACCES;
        return -1;
    }
    return rename(from, to);
}
static int FixtureRemove(const char *path) {
    removes++;
    return remove(path);
}
/* files.c's only malloc and free are FS_CopyFile's whole-file buffer. */
static void *FixtureMalloc(size_t size) {
    void *p;
    mallocs++;
    p = mallocFails ? NULL : malloc(size);
    mallocLive += p != NULL;
    return p;
}
static void FixtureFree(void *p) {
    mallocLive -= p != NULL;
    free(p);
}
static void Reset(int refuse, int failMalloc) {
    writeOpens = renames = removes = mallocs = warnings = 0;
    renameRefused = refuse;
    mallocFails = failMalloc;
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
void *Hunk_AllocateTempMemory(int size) { return calloc(1, size ? size : 1); }
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
void Sys_Mkdir(const char *path) { mkdir(path, 0777); }
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
void Com_StartupVariable(const char *match) {
    if (!strcmp(match, "fs_basepath")) {
        Cvar_Set(match, BASE_DIR);
    } else if (!strcmp(match, "fs_homepath")) {
        Cvar_Set(match, HOME_DIR);
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

/* The OS path of a name under the home game directory. */
static const char *OSName(const char *name) {
    static char paths[4][MAX_OSPATH];
    static int toggle;
    char *out = paths[toggle++ & 3];
    char *s;
    Check(snprintf(out, MAX_OSPATH, "%s/%s/%s", HOME_DIR, BASEGAME, name) < MAX_OSPATH, "bounded fixture path");
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

static void Plant(const char *ospath, const char *payload) {
    char path[MAX_OSPATH];
    FILE *out;
    Q_strncpyz(path, ospath, sizeof(path));
    FS_CreatePath(path);
    out = fopen(ospath, "wb");
    Check(out != NULL, "plant fixture file");
    fputs(payload, out);
    fclose(out);
}

static void Contains(const char *ospath, const char *payload, const char *message) {
    char text[64];
    FILE *in = fopen(ospath, "rb");
    int n = -1;
    if (in) {
        n = (int)fread(text, 1, sizeof(text), in);
        fclose(in);
    }
    Check(n == (int)strlen(payload) && !memcmp(text, payload, n), message);
}

static void Startup(void) {
    char path[64];

    snprintf(path, sizeof(path), "%s%c%s%cdefault.cfg", BASE_DIR, PATH_SEP, BASEGAME, PATH_SEP);
    Plant(path, "// default\n");
    snprintf(path, sizeof(path), "%s%c%s%cproductid.txt", BASE_DIR, PATH_SEP, BASEGAME, PATH_SEP);
    Plant(path, "This file is copyright 1999 Id Software, and may not be duplicated except "
          "during a licensed installation of the full commercial version of Quake 3:Arena");
    FS_InitFilesystem();
    Check(!strcmp(fs_gamedir, BASEGAME), "full game directory");
}

#define PAYLOAD "PK\003\004downloaded"

/* CL_ParseDownload: FS_SV_FOpenFileWrite( <name>.pk3.tmp ), write, close,
 * then FS_SV_Rename( tmp, final, qfalse ). */
static void Download(const char *stem) {
    char tmp[MAX_QPATH];
    fileHandle_t f;

    Com_sprintf(tmp, sizeof(tmp), "%s/%s.pk3.tmp", BASEGAME, stem);
    f = FS_SV_FOpenFileWrite(tmp);
    Check(f > 0 && FS_Write(PAYLOAD, (int)strlen(PAYLOAD), f) == (int)strlen(PAYLOAD), "download temp file");
    FS_FCloseFile(f);
}

static void FinishDownload(const char *stem) {
    char tmp[MAX_QPATH], final[MAX_QPATH];

    Com_sprintf(tmp, sizeof(tmp), "%s/%s.pk3.tmp", BASEGAME, stem);
    Com_sprintf(final, sizeof(final), "%s/%s.pk3", BASEGAME, stem);
    FS_SV_Rename(tmp, final, qfalse);
}

int main(int argc, char **argv) {
    int i;

    Check(argc == 2 && !chdir(argv[1]), "usage: fs_copy_rename_regression workdir");
    Startup();

    /* A working rename(): one rename, no copy. */
    Download("renamed");
    Reset(0, 0);
    FinishDownload("renamed");
    Check(renames == 1 && !writeOpens && !removes && !mallocs, "a working rename is used alone");
    Contains(OSName("renamed.pk3"), PAYLOAD, "renamed download holds its payload");
    Check(!Exists(OSName("renamed.pk3.tmp")), "renamed download leaves no .tmp");
    Plant(OSName("notes.txt"), "notes");
    Reset(0, 0);
    FS_Rename("notes.txt", "notes2.txt");
    Check(renames == 1 && !writeOpens && !removes && !mallocs, "FS_Rename uses a working rename alone");
    Contains(OSName("notes2.txt"), "notes", "FS_Rename moved the file");
    Check(!Exists(OSName("notes.txt")), "FS_Rename left the old name");

    /* rename() refused: the copy fallback still produces the pk3. */
    Download("copied");
    Reset(1, 0);
    FinishDownload("copied");
    Check(renames == 1 && mallocs == 1 && writeOpens == 1 && removes == 1, "refused rename falls back to copy and remove");
    Contains(OSName("copied.pk3"), PAYLOAD, "copied download holds its payload");
    Check(!Exists(OSName("copied.pk3.tmp")), "copied download removes its .tmp");
    Reset(1, 0);
    FS_Rename("notes2.txt", "notes3.txt");
    Check(mallocs == 1 && removes == 1, "FS_Rename falls back to copy and remove");
    Contains(OSName("notes3.txt"), "notes", "FS_Rename copied the file");
    Check(!Exists(OSName("notes2.txt")), "FS_Rename's fallback left the old name");

    /* A rename the mutable-name checks refuse stays refused, whatever
     * rename() does. */
    for (i = 0; i < 2; i++) {
        Reset(i, 0);
        FS_SV_Rename(BASEGAME "/notes3.txt", BASEGAME "/evil.pk3", qtrue);
        FS_Rename("notes3.txt", "vm/qagame.qvm");
        Check(!renames && !mallocs && !writeOpens && !removes && warnings == 2, "refused renames reach no OS call");
        Check(!Exists(OSName("evil.pk3")) && !Exists(OSName("vm/qagame.qvm")), "refused renames create nothing");
        Contains(OSName("notes3.txt"), "notes", "refused renames keep the source");
    }

    /* rename() refused and no memory for the copy: the source stays. */
    Download("nomem");
    Reset(1, 1);
    FinishDownload("nomem");
    Check(renames == 1 && mallocs == 1 && !writeOpens && !removes && warnings == 1,
          "a failed copy is reported and removes nothing");
    Contains(OSName("nomem.pk3.tmp"), PAYLOAD, "the download stays in its .tmp");
    Check(!Exists(OSName("nomem.pk3")), "a failed copy creates no pk3");
    Reset(1, 1);
    FS_Rename("notes3.txt", "notes4.txt");
    Check(mallocs == 1 && !writeOpens && !removes && warnings == 1, "FS_Rename's failed copy removes nothing");
    Contains(OSName("notes3.txt"), "notes", "FS_Rename's source stays");
    Check(!Exists(OSName("notes4.txt")), "FS_Rename's failed copy creates nothing");
    /* and the next attempt, with memory, still finishes it */
    Reset(1, 0);
    FinishDownload("nomem");
    Contains(OSName("nomem.pk3"), PAYLOAD, "a retried download holds its payload");
    Check(!Exists(OSName("nomem.pk3.tmp")), "a retried download removes its .tmp");

    /* An empty file copies too (malloc( 0 ) may return NULL). */
    Plant(OSName("empty.txt"), "");
    Reset(1, 0);
    FS_Rename("empty.txt", "empty2.txt");
    Check(mallocs == 1 && removes == 1 && !warnings, "an empty file copies");
    Check(Exists(OSName("empty2.txt")) && !Exists(OSName("empty.txt")), "an empty file moved");

    /* The copy's destination refused: FS_CreatePath will not make a path
     * with "..", and fopen cannot create a file under a plain file. */
    Reset(1, 0);
    FS_Rename("empty2.txt", "a..txt");
    Check(mallocs == 1 && !writeOpens && !removes, "FS_CreatePath refused the copy");
    Check(Exists(OSName("empty2.txt")) && !Exists(OSName("a..txt")), "a refused copy keeps the source only");
    Check(!mallocLive, "FS_CopyFile leaked its buffer when FS_CreatePath refused");
    Plant(OSName("blocker"), "file");
    Reset(1, 0);
    FS_Rename("empty2.txt", "blocker/empty3.txt");
    Check(mallocs == 1 && writeOpens == 1 && !removes, "the copy's fopen failed");
    Check(Exists(OSName("empty2.txt")), "a failed copy keeps the source");
    Check(!mallocLive, "FS_CopyFile leaked its buffer when fopen failed");

    FS_Shutdown(qtrue);
    FreeCvars();
    Check(!zoneLive, "zone released");
    printf("FS copy/rename fallback passes\n");
    return 0;
}
