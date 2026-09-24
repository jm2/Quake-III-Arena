/* Issue #426: FS_SetRestrictions decodes the full game's productid.txt with
 * Q_rand's 69069 LCG, and multiplied in signed int, which overflows from the
 * second byte on: undefined behaviour at every native startup and FS_Restart
 * with the full game.  The real FS_SetRestrictions, files.c and unzip.c (the
 * fs_zip fixture) mount the pk3s built by tests/run_fs_product_id_tests.sh,
 * with -fsanitize=signed-integer-overflow -fno-sanitize-recover=all:
 *   retail.pk3     the full game's productid.txt: no restrictions
 *   badfirst.pk3   its first byte changed: "Invalid product identification"
 *   badlast.pk3    its 152nd byte changed: the same fatal error
 * The retail text must also be fs_scrambledProductId decoded with the LCG in
 * uint32_t, byte for byte. */
#include <setjmp.h>
#include <stdint.h>
#define Q3_ZIP_ERROR_HOOK
#define main FixtureZipMain
#ifndef Q3_ZIP_NATIVE_FIXTURE
#define Q3_ZIP_NATIVE_FIXTURE "fs_zip_regression.c"
#endif
#include Q3_ZIP_NATIVE_FIXTURE
#undef main

static jmp_buf errorTarget;
static int expectError, errorLevel;
static char packPath[MAX_OSPATH * 2];

static void FixtureErrorHook(int level) {
    Check(expectError, "unexpected engine error");
    errorLevel = level;
    longjmp(errorTarget, 1);
}

/* Restricted demo mode (Cvar_Set, then FS_Startup( DEMOGAME )) must not run:
 * every product id here is either accepted or fatal. */
void Cvar_Set(const char *name, const char *value) {
    (void)name; (void)value;
    Check(0, "unexpected restricted demo mode");
}
cvar_t *Cvar_Get(const char *name, const char *value, int flags) {
    (void)name; (void)value; (void)flags;
    Check(0, "unexpected filesystem startup");
    return NULL;
}
void Cmd_AddCommand(const char *name, xcommand_t function) { (void)name; (void)function; Check(0, "unexpected startup"); }
void Cmd_RemoveCommand(const char *name) { (void)name; Check(0, "unexpected shutdown"); }
int Cmd_Argc(void) { Check(0, "unexpected console command"); return 0; }
char *Cmd_Argv(int arg) { (void)arg; Check(0, "unexpected console command"); return ""; }
void Cmd_TokenizeString(const char *text) { (void)text; Check(0, "unexpected pure list"); }
char *CopyString(const char *in) { (void)in; Check(0, "unexpected file list"); return NULL; }
int Com_FilterPath(char *filter, char *name, int casesensitive) {
    (void)filter; (void)name; (void)casesensitive;
    Check(0, "unexpected filtered file list");
    return 0;
}
void Com_ReadCDKey(const char *filename) { (void)filename; Check(0, "unexpected startup"); }
void Com_AppendCDKey(const char *filename) { (void)filename; Check(0, "unexpected startup"); }
void QDECL Com_FlightRecord(const char *fmt, ...) { (void)fmt; }
void S_ClearSoundBuffer(void) {}
char *Sys_DefaultCDPath(void) { Check(0, "unexpected startup"); return ""; }
char *Sys_DefaultInstallPath(void) { Check(0, "unexpected startup"); return ""; }
char *Sys_DefaultHomePath(void) { Check(0, "unexpected startup"); return ""; }
char **Sys_ListFiles(const char *directory, const char *extension, char *filter,
                     int *numfiles, qboolean wantsubs) {
    (void)directory; (void)extension; (void)filter; (void)wantsubs;
    Check(0, "unexpected directory scan");
    *numfiles = 0;
    return NULL;
}
void Sys_FreeFileList(char **list) { (void)list; Check(0, "unexpected directory scan"); }
void Sys_BeginStreamedFile(fileHandle_t f, int readahead) {
    (void)f; (void)readahead;
    Check(0, "unexpected streamed read");
}
void Sys_StreamSeek(fileHandle_t f, int offset, int origin) {
    (void)f; (void)offset; (void)origin;
    Check(0, "unexpected streamed seek");
}

/* pak0.pk3 is the only search path, as for a full install's product id. */
static void Mount(const char *dir, const char *name) {
    Com_sprintf(packPath, sizeof(packPath), "%s/%s", dir, name);
    Begin();
    search.pack = FS_LoadZipFile(packPath, "pak0");
    Check(search.pack != NULL, "product id pk3 mounts");
}

/** The retail text is the scrambled id decoded with the wrapped LCG. */
static void RetailText(const char *dir) {
    uint32_t seed = 5000;
    void *text = NULL;
    int i, n;

    Mount(dir, "retail.pk3");
    n = FS_ReadFile("productid.txt", &text);
    Check(n == (int)sizeof(fs_scrambledProductId) && text != NULL, "retail productid.txt length");
    for (i = 0; i < n; i++) {
        Check((byte)(fs_scrambledProductId[i] ^ (seed & 255)) == ((byte *)text)[i],
              "retail productid.txt is the uint32_t decode of fs_scrambledProductId");
        seed = 69069u * seed + 1u;
    }
    FS_FreeFile(text);
    End();
}

/** The full game's product id lifts the restrictions without a signed overflow. */
static void Accepted(const char *dir) {
    Mount(dir, "retail.pk3");
    restrictVar.integer = 0;
    FS_SetRestrictions();
    Check(!restrictVar.integer, "retail product id leaves fs_restrict clear");
    End();
}

/** Every one of the 152 bytes is still compared. */
static void Rejected(const char *dir, const char *name) {
    Mount(dir, name);
    restrictVar.integer = 0;
    expectError = 1;
    errorLevel = -1;
    if (!setjmp(errorTarget)) {
        FS_SetRestrictions();
        Check(0, "a changed product id byte was accepted");
    }
    expectError = 0;
    Check(errorLevel == ERR_FATAL, "a changed product id byte is fatal");
    End();
}

int main(int argc, char **argv) {
    Check(argc == 2, "usage: fs_product_id_regression pk3-dir");
    RetailText(argv[1]);
    Accepted(argv[1]);
    Rejected(argv[1], "badfirst.pk3");
    Rejected(argv[1], "badlast.pk3");
    Accepted(argv[1]);
    puts("FS_SetRestrictions decodes the retail product id without a signed overflow (issue #426)");
    return 0;
}
