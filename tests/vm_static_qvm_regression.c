/* Issue #13: a static build's linked-in game modules must not override a mod's
 * QVMs.  Retail 1.32c ran whatever vm/<module>.qvm the search path provided
 * (vm_cgame, vm_game and vm_ui default to 2); the Mac port links the stock
 * game's modules into the application and Sys_LoadDll returned them for every
 * load, so a QVM-only mod (fs_game, or a pure server's pk3) never ran.
 *
 * VM_Create now runs the linked-in module only where retail would have run
 * the stock QVM it replaces: the one in a retail id pk3 of STATIC_MODULES_GAME
 * (baseq3, or missionpack for Quake3_TeamArena).  Any other QVM runs in the
 * interpreter.
 *
 * run_vm_static_qvm_tests.sh builds this file around the real files.c,
 * unzip.c, vm.c, vm_interpreted.c, vm_static.c, cvar.c and cmd.c, once as
 * Quake3 and once with the STATIC_MODULES_GAME that CMakeLists.txt gives
 * Quake3_TeamArena.  Its install has
 *   baseq3/pak0.pk3       stock baseq3 QVMs (header checksum of retail pak0),
 *                         and default.cfg and productid.txt outside it
 *   missionpack/pak3.pk3  stock missionpack QVMs (retail missionpack pak3)
 *   mymod/zmod.pk3        a mod's cgame.qvm only
 *   loosemod/vm/cgame.qvm a mod's cgame.qvm outside any pk3
 * Each QVM's vmMain returns its own number; the linked-in stand-ins (Sys_LoadDll
 * below, restoring their images with vm_static.c as mac_main.c does) return a
 * count that shows whether their image was fresh.
 */
#include <dirent.h>
#include "../code/qcommon/files.c"
#include "../code/qcommon/vm_local.h"
#include "../code/qcommon/vm_static.h"

#ifndef STATIC_MODULES_GAME
#error "the runner passes STATIC_MODULES_GAME through qcommon.h's default"
#endif

qboolean com_fullyInitialized;
qboolean com_errorEntered;
static cvar_t developer;
cvar_t *com_developer = &developer;
cvar_t *com_journal;
fileHandle_t com_journalDataFile;
static char install[MAX_OSPATH];
static const char *scenario = "start";

static void Check(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "static module QVM regression failed (STATIC_MODULES_GAME \"%s\", %s): %s\n",
                STATIC_MODULES_GAME, scenario, message);
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
    fputc('\n', stderr);
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
/* The VMs' hunk: freed when the test ends, as a map change clears the hunk. */
static void *hunk[1024];
static int hunkCount;
void *Hunk_Alloc(int size, ha_pref preference) {
    (void)preference;
    Check(size > 0 && size <= 4 * 1024 * 1024 && hunkCount < 1024, "bounded hunk allocation");
    hunk[hunkCount] = calloc(1, size);
    Check(hunk[hunkCount] != NULL, "hunk allocation");
    return hunk[hunkCount++];
}
int Hunk_MemoryRemaining(void) { return 64 * 1024 * 1024; }
int Com_Filter(char *filter, char *name, int casesensitive) {
    (void)filter; (void)name; (void)casesensitive;
    return 0;
}
int Com_FilterPath(char *filter, char *name, int casesensitive) {
    (void)filter; (void)name; (void)casesensitive;
    return 0;
}
qboolean Com_SafeMode(void) { return qtrue; }
void Com_StartupVariable(const char *match) { (void)match; }
void Com_ReadCDKey(const char *filename) { (void)filename; }
void Com_AppendCDKey(const char *filename) { (void)filename; }
void S_ClearSoundBuffer(void) {}
void Sys_BeginStreamedFile(fileHandle_t f, int readAhead) { (void)f; (void)readAhead; }
void Sys_EndStreamedFile(fileHandle_t f) { (void)f; }
int Sys_StreamedRead(void *buffer, int size, int count, fileHandle_t f) {
    return FS_Read(buffer, size * count, f);
}
void Sys_Mkdir(const char *path) { (void)path; }
char *Sys_DefaultCDPath(void) { return ""; }
char *Sys_DefaultInstallPath(void) { return install; }
char *Sys_DefaultHomePath(void) { return ""; }
char **Sys_ListFiles(const char *directory, const char *extension, char *filter, int *numfiles, qboolean wantsubs) {
    char **list = Z_Malloc(8 * sizeof(*list));
    struct dirent *entry;
    DIR *dir = opendir(directory);
    size_t n;

    Check(extension && !strcmp(extension, ".pk3") && !filter && !wantsubs, "only pk3 scans");
    *numfiles = 0;
    while (dir && (entry = readdir(dir)) != NULL) {
        n = strlen(entry->d_name);
        if (n > 4 && !strcmp(entry->d_name + n - 4, ".pk3")) {
            Check(*numfiles < 7, "bounded pk3 list");
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
void CL_ForwardCommandToServer(const char *string) { (void)string; Check(0, "unexpected forward"); }
qboolean SV_GameCommand(void) { Check(0, "unexpected game command"); return qfalse; }
qboolean UI_GameCommand(void) { Check(0, "unexpected ui command"); return qfalse; }
qboolean CL_GameCommand(void) { Check(0, "unexpected cgame command"); return qfalse; }
int VM_CallCompiled(vm_t *vm, int *args) {
    (void)vm; (void)args;
    Check(0, "a static build has no QVM compiler");
    return 0;
}
void VM_Compile(vm_t *vm, vmHeader_t *header) {
    (void)vm; (void)header;
    Check(0, "a static build has no QVM compiler");
}

/* The linked-in modules, as mac_main.c's Sys_LoadDll gives them out: every
 * load restores the module's fresh image (#462).  Each vmMain returns its
 * base plus the calls since that image was fresh, so a first call after a
 * fresh load returns base + 1. */
#define GAME_NATIVE		9000
#define CGAME_NATIVE	7000
#define UI_NATIVE		8000
static int gameData[2] = { GAME_NATIVE, 0 }, gameBss[2];
static int cgameData[2] = { CGAME_NATIVE, 0 }, cgameBss[2];
static int uiData[2] = { UI_NATIVE, 0 }, uiBss[2];
#define BRACKET(a) (unsigned char *)(a), (unsigned char *)((a) + 2)
static vmStaticModule_t staticModules[] = {
    { "qagame", BRACKET(gameData), BRACKET(gameBss) },
    { "cgame", BRACKET(cgameData), BRACKET(cgameBss) },
    { "ui", BRACKET(uiData), BRACKET(uiBss) },
};
static int nativeLoads, nativeUnloads;

#define NATIVE_MAIN(name, data, bss) \
static int QDECL name(int command, int a0, int a1, int a2, int a3, int a4, int a5, \
                      int a6, int a7, int a8, int a9, int a10, int a11) { \
    (void)command; (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; \
    (void)a6; (void)a7; (void)a8; (void)a9; (void)a10; (void)a11; \
    return data[0] + ++bss[0]; \
}
NATIVE_MAIN(Game_vmMain, gameData, gameBss)
NATIVE_MAIN(CGame_vmMain, cgameData, cgameBss)
NATIVE_MAIN(UI_vmMain, uiData, uiBss)

void *QDECL Sys_LoadDll(const char *name, char *fqpath, int (QDECL **entryPoint)(int, ...),
                        int (QDECL *systemcalls)(int, ...)) {
    vmStaticModule_t *module;
    (void)fqpath; (void)systemcalls;

    module = VM_FindStaticModule(staticModules, 3, name);
    if (!module) {
        return NULL;
    }
    Check(VM_LoadStaticModule(module) == NULL, "a static module was loaded twice");
    nativeLoads++;
    if (!Q_stricmp(name, "ui")) {
        *entryPoint = (int (QDECL *)(int, ...))UI_vmMain;
    } else if (!Q_stricmp(name, "cgame")) {
        *entryPoint = (int (QDECL *)(int, ...))CGame_vmMain;
    } else {
        *entryPoint = (int (QDECL *)(int, ...))Game_vmMain;
    }
    return module;
}
void Sys_UnloadDll(void *handle) {
    nativeUnloads++;
    VM_UnloadStaticModule((vmStaticModule_t *)handle);
}

static int SystemCall(int *args) {
    (void)args;
    Check(0, "the fixture QVMs make no system calls");
    return 0;
}

/* ------------------------------------------------------------------ */

static const char *Interpret(int interpret) {
    return interpret == VMI_NATIVE ? "native" : interpret == VMI_BYTECODE ? "bytecode" : "compiled";
}

/** Mounts game (BASEGAME for none) as a client or server would after a change. */
static void Mount(const char *game) {
    Cvar_Set("fs_game", Q_stricmp(game, BASEGAME) ? game : "");
    FS_Restart(0);
    Check(!strcmp(fs_gamedir, game), "mounted the requested game");
}

static pack_t *Pak(const char *game, const char *base) {
    searchpath_t *s;
    for (s = fs_searchpaths; s; s = s->next) {
        if (s->pack && !Q_stricmp(s->pack->pakGamename, game) && !Q_stricmp(s->pack->pakBasename, base)) {
            return s->pack;
        }
    }
    Check(0, "fixture pk3 is mounted");
    return NULL;
}

/** Creates module as its call site does, calls vmMain twice and frees it.
 * qvm is the number the QVM the search path provides returns, or 0 when the
 * linked-in module must run. */
static void Expect(const char *module, int interpret, int qvm) {
    char message[256];
    int native = !Q_stricmp(module, "cgame") ? CGAME_NATIVE : !Q_stricmp(module, "ui") ? UI_NATIVE : GAME_NATIVE;
    int loads = nativeLoads;
    vm_t *vm = VM_Create(module, SystemCall, interpret);

    Com_sprintf(message, sizeof(message), "%s with %s: %s", module, Interpret(interpret),
                qvm ? "the search path's QVM runs in the interpreter" : "the linked-in module runs");
    Check(vm != NULL, message);
    Check(VM_IsNative(vm) == !qvm && nativeLoads == loads + !qvm, message);
    if (qvm) {
        Check(VM_Call(vm, 0) == qvm && VM_Call(vm, 1, 2, 3) == qvm, message);
    } else {
        /* a fresh image, whatever ran before */
        Check(VM_Call(vm, 0) == native + 1 && VM_Call(vm, 1, 2, 3) == native + 2, message);
    }
    VM_Free(vm);
}

static int StockFor(const char *game) {
    return !Q_stricmp(game, STATIC_MODULES_GAME);
}

static void TestStockPaks(void) {
    char sums[64];
    pack_t *pak;
    int checksum;

    scenario = "stock baseq3";
    Mount(BASEGAME);
    pak = Pak(BASEGAME, "pak0");
    if (!FS_IdPakGame(pak->checksum)) {
        fprintf(stderr, "baseq3/pak0.pk3 header checksum %u: the runner's forged entry is stale\n",
                (unsigned)pak->checksum);
    }
    Check(FS_IdPakGame(pak->checksum) && !strcmp(FS_IdPakGame(pak->checksum), BASEGAME),
          "the fixture pak has retail baseq3/pak0.pk3's header checksum");
    Check(FS_FilePakChecksum("vm/cgame.qvm", &checksum) == 1 && checksum == pak->checksum,
          "vm/cgame.qvm comes from baseq3/pak0.pk3");
    Check(FS_FilePakChecksum("vm/none.qvm", &checksum) == -1, "a missing file");
    pak->referenced = 0;

    /* The defaults (VM_Init registers 2), as CL_InitCGame, CL_InitUI and
     * SV_InitGameProgs pass them.  Quake3_TeamArena interprets baseq3's
     * stock QVMs: its own modules are missionpack's (#325). */
    Expect("cgame", (int)Cvar_VariableValue("vm_cgame"), StockFor(BASEGAME) ? 0 : 101);
    Expect("qagame", (int)Cvar_VariableValue("vm_game"), StockFor(BASEGAME) ? 0 : 102);
    Expect("ui", (int)Cvar_VariableValue("vm_ui"), StockFor(BASEGAME) ? 0 : 103);
    /* Like reading the QVM, standing in for it references its pk3, so a pure
     * server gets the cgame and ui paks it expects first in "cp". */
    Check((pak->referenced & (FS_CGAME_REF | FS_UI_REF | FS_QAGAME_REF)) ==
          (FS_CGAME_REF | FS_UI_REF | FS_QAGAME_REF), "the stock QVMs' pk3 is referenced");
    Com_sprintf(sums, sizeof(sums), "%d %d @ ", pak->pure_checksum, pak->pure_checksum);
    Check(!strncmp(FS_ReferencedPakPureChecksums(), sums, strlen(sums)),
          "pure checksums start with the cgame and ui pk3");

    /* vm_cgame 0 always takes the linked-in module, 1 always interprets. */
    Expect("cgame", VMI_NATIVE, 0);
    Expect("cgame", VMI_BYTECODE, 101);
    /* The demo never runs a linked-in module, as retail never loaded a DLL. */
    Cvar_Set("fs_restrict", "1");
    Expect("cgame", VMI_NATIVE, 101);
    Expect("cgame", VMI_COMPILED, 101);
    Cvar_Set("fs_restrict", "0");

    scenario = "stock missionpack";
    Mount("missionpack");
    pak = Pak("missionpack", "pak3");
    if (!FS_IdPakGame(pak->checksum)) {
        fprintf(stderr, "missionpack/pak3.pk3 header checksum %u: the runner's forged entry is stale\n",
                (unsigned)pak->checksum);
    }
    Check(FS_IdPakGame(pak->checksum) && !strcmp(FS_IdPakGame(pak->checksum), "missionpack"),
          "the fixture pak has retail missionpack/pak3.pk3's header checksum");
    /* Quake3 interprets Team Arena's stock QVMs (#325). */
    Expect("cgame", VMI_COMPILED, StockFor("missionpack") ? 0 : 301);
    Expect("qagame", VMI_COMPILED, StockFor("missionpack") ? 0 : 302);
    Expect("ui", VMI_COMPILED, StockFor("missionpack") ? 0 : 303);
    Expect("ui", VMI_NATIVE, 0);
}

static void TestMods(void) {
    char sums[64];
    int checksum;

    /* A QVM-only mod: its cgame runs, and the stock ui and qagame it does
     * not replace are baseq3's. */
    scenario = "mymod over baseq3";
    Mount("mymod");
    Check(FS_FilePakChecksum("vm/cgame.qvm", &checksum) == 1 && checksum == Pak("mymod", "zmod")->checksum,
          "the mod's pk3 provides vm/cgame.qvm");
    Check(!FS_IdPakGame(checksum), "other pk3s are not id paks");
    Expect("cgame", VMI_COMPILED, 201);
    Expect("cgame", VMI_BYTECODE, 201);
    Expect("cgame", VMI_NATIVE, 0);
    Expect("ui", VMI_COMPILED, StockFor(BASEGAME) ? 0 : 103);
    Expect("qagame", VMI_COMPILED, StockFor(BASEGAME) ? 0 : 102);

    /* A pure server: a client loads only what the server's pk3s provide,
     * and is forced to "compiled" whatever vm_cgame says. */
    scenario = "mymod, pure server with the mod's pk3";
    Com_sprintf(sums, sizeof(sums), "%d %d", Pak(BASEGAME, "pak0")->checksum, Pak("mymod", "zmod")->checksum);
    FS_PureServerSetLoadedPaks(sums, "");
    Expect("cgame", VMI_COMPILED, 201);
    scenario = "mymod, pure server without the mod's pk3";
    Com_sprintf(sums, sizeof(sums), "%d", Pak(BASEGAME, "pak0")->checksum);
    FS_PureServerSetLoadedPaks(sums, "");
    Expect("cgame", VMI_COMPILED, StockFor(BASEGAME) ? 0 : 101);
    scenario = "pure server with no QVM at all";
    FS_PureServerSetLoadedPaks("1", "");	/* a pk3 this client lacks */
    Check(FS_FilePakChecksum("vm/cgame.qvm", &checksum) == -1, "no QVM is visible");
    Expect("cgame", VMI_COMPILED, 0);		/* the linked-in fallback */
    FS_PureServerSetLoadedPaks("", "");

    /* A QVM outside any pk3 is a mod's, never stock; pure servers refuse it. */
    scenario = "loosemod";
    Mount("loosemod");
    Check(FS_FilePakChecksum("vm/cgame.qvm", &checksum) == 0, "loose vm/cgame.qvm");
    Expect("cgame", VMI_COMPILED, 401);
    Com_sprintf(sums, sizeof(sums), "%d", Pak(BASEGAME, "pak0")->checksum);
    FS_PureServerSetLoadedPaks(sums, "");
    Expect("cgame", VMI_COMPILED, StockFor(BASEGAME) ? 0 : 101);
    FS_PureServerSetLoadedPaks("", "");
}

/* Missionpack and a mod: Team Arena's stock QVMs, then a mod's over them,
 * then back, with each executable's own modules where they are stock. */
static void TestMissionpackAndMod(void) {
    char sums[96];

    scenario = "missionpack, then a pure mod server";
    Mount("missionpack");
    Expect("cgame", VMI_COMPILED, StockFor("missionpack") ? 0 : 301);
    Mount("mymod");
    Com_sprintf(sums, sizeof(sums), "%d %d", Pak(BASEGAME, "pak0")->checksum, Pak("mymod", "zmod")->checksum);
    FS_PureServerSetLoadedPaks(sums, "");
    Expect("cgame", VMI_COMPILED, 201);
    Expect("ui", VMI_COMPILED, StockFor(BASEGAME) ? 0 : 103);
    FS_PureServerSetLoadedPaks("", "");
    Mount("missionpack");
    Expect("cgame", VMI_COMPILED, StockFor("missionpack") ? 0 : 301);
    Expect("ui", VMI_COMPILED, StockFor("missionpack") ? 0 : 303);
}

/* Map changes, map_restart (VM_Restart) and disconnects that switch one
 * module between its linked-in and interpreted forms: each linked-in load
 * starts from a fresh image (#462) and pairs with one unload. */
static void TestSwitching(void) {
    const char *stock = STATIC_MODULES_GAME;
    vm_t *vm;
    int i;

    scenario = "switching";
    for (i = 0; i < 3; i++) {
        Mount(stock);
        Expect("cgame", VMI_COMPILED, 0);
        Mount("mymod");
        Expect("cgame", VMI_COMPILED, 201);
        Mount(i == 1 ? "loosemod" : stock);
        Expect("cgame", VMI_COMPILED, i == 1 ? 401 : 0);
    }

    /* map_restart of a linked-in qagame frees and creates it again. */
    Mount(stock);
    vm = VM_Create("qagame", SystemCall, VMI_COMPILED);
    Check(VM_IsNative(vm) && VM_Call(vm, 0) == GAME_NATIVE + 1, "linked-in qagame");
    vm = VM_Restart(vm);
    Check(vm && VM_IsNative(vm) && VM_Call(vm, 0) == GAME_NATIVE + 1, "map_restart restarts the linked-in qagame fresh");
    VM_Free(vm);

    /* map_restart of a mod's interpreted qagame reloads its data in place;
     * a mod with only a cgame keeps the stock qagame. */
    Mount("mymod");
    vm = VM_Create("qagame", SystemCall, VMI_COMPILED);
    Check(VM_IsNative(vm) == StockFor(BASEGAME), "a cgame-only mod keeps the stock qagame");
    VM_Free(vm);
    vm = VM_Create("cgame", SystemCall, VMI_COMPILED);
    Check(!VM_IsNative(vm) && VM_Call(vm, 0) == 201, "mod cgame");
    vm = VM_Restart(vm);
    Check(vm && !VM_IsNative(vm) && VM_Call(vm, 0) == 201, "an interpreted restart keeps the mod's QVM");
    VM_Free(vm);

    /* Back to the stock game: the linked-in module, fresh. */
    Mount(stock);
    Expect("cgame", VMI_COMPILED, 0);
    Expect("ui", VMI_COMPILED, 0);
    Check(nativeLoads == nativeUnloads, "every linked-in load was unloaded");
}

int main(int argc, char **argv) {
    int i;

    Check(argc == 2, "usage: vm_static_qvm_regression install");
    Q_strncpyz(install, argv[1], sizeof(install));
    Check(VM_InitStaticModules(staticModules, 3) == NULL, "static module brackets");

    Cvar_Init();
    Cmd_Init();
    FS_InitFilesystem();
    VM_Init();
    Check(!Cvar_VariableValue("fs_restrict"), "the fixture is a full install");

    TestStockPaks();
    TestMods();
    TestMissionpackAndMod();
    TestSwitching();

    FS_Shutdown(qtrue);
    for (i = 0; i < hunkCount; i++) {
        free(hunk[i]);
    }
    for (i = 0; i < 3; i++) {
        free(staticModules[i].image);
    }
    printf("static module QVM selection (STATIC_MODULES_GAME \"%s\"): %d linked-in loads\n",
           STATIC_MODULES_GAME, nativeLoads);
    return 0;
}
