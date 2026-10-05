/* Issue #325: each executable links in one game's modules, so a client of the
 * other game used to run them against a server they do not match.  Retail
 * 1.32c ran whatever vm/<module>.qvm the search path provided once a server's
 * systeminfo had set fs_game, and #490 (issue #13) runs the linked-in module
 * only where that QVM is this executable's own stock one, interpreting any
 * other.  This walks the connect path with the real code end to end:
 *   CL_ParseGamestate -> CL_SystemInfoChanged (cl_parse.c) sets fs_game
 *   -> FS_ConditionalRestart -> FS_Restart (files.c) remounts the game
 *   -> VM_Create (vm.c) for the UI that CL_FlushMemory restarts and the
 *      cgame CL_InitCGame starts, with the vm_ui/vm_cgame they pass,
 * then a disconnect (Com_Error's CL_Disconnect + CL_FlushMemory, which keep
 * fs_game exactly as retail's did) and a UI restart, for
 *   (a) Quake3_TeamArena (fs_game missionpack by default, #487) joining a
 *       baseq3 server, (b) Quake3 joining a Team Arena server, and
 *   (c) each executable joining a server of its own game, and a baseq3 mod.
 * At every step it checks fs_game and which QVM the search path gives (the
 * one retail ran), and that the executable runs its linked-in module exactly
 * when that QVM is its own stock one, otherwise the QVM itself.
 *
 * run_vm_static_qvm_tests.sh builds this over the same generated install as
 * vm_static_qvm_regression.c, whose engine imports, linked-in stand-ins and
 * helpers this file reuses by including it (its main renamed away).
 */
#define main VmStaticQvmMain
#include "vm_static_qvm_regression.c"
#undef main
#include "../code/client/cl_parse.c"

#ifndef DEFAULT_FS_GAME_EXPECTED
#error "the runner passes the DEFAULT_FS_GAME CMakeLists.txt gives this executable"
#endif

/* cl_main.c's state, which CL_ParseGamestate fills. */
clientActive_t cl;
clientConnection_t clc;
clientStatic_t cls;
cvar_t *cl_shownet;
int cl_connectedToPureServer;
void Con_Close(void) {}
void CL_ClearState(void) { memset(&cl, 0, sizeof(cl)); }

/* What CL_DownloadsComplete starts: CL_FlushMemory restarts the UI (vm_ui)
 * and CL_InitCGame creates the cgame (vm_cgame). */
static int startedQVM[2], startedNative[2];
static int connects;
static void StartModule(int index, const char *module, const char *cvar) {
    vm_t *vm = VM_Create(module, SystemCall, (int)Cvar_VariableValue(cvar));
    int result;

    Check(vm != NULL, "module created");
    result = VM_Call(vm, 0);
    startedNative[index] = VM_IsNative(vm);
    startedQVM[index] = startedNative[index] ? 0 : result;
    VM_Free(vm);
}
void CL_InitDownloads(void) {
    connects++;
    StartModule(0, "ui", "vm_ui");
    StartModule(1, "cgame", "vm_cgame");
}

static const char *FsGame(void) {
    return Cvar_VariableString("fs_game");
}

/* The QVM the search path provides for module: its pk3's game, and the number
 * its vmMain returns. */
static int SearchPathQVM(const char *module, const char **game) {
    char filename[MAX_QPATH];
    int checksum, length, value;
    int *header;

    Com_sprintf(filename, sizeof(filename), "vm/%s.qvm", module);
    Check(FS_FilePakChecksum(filename, &checksum) == 1, "the search path's QVM is in a pk3");
    *game = FS_IdPakGame(checksum);
    Check(*game != NULL, "the search path's QVM is a retail one");
    length = FS_ReadFile(filename, (void **)&header);
    Check(length > 32 + 6, "QVM read");
    /* the fixture's code segment is ENTER 8; CONST value; LEAVE 8 */
    memcpy(&value, (byte *)header + 32 + 5 + 1, 4);
    value = LittleLong(value);
    FS_FreeFile(header);
    return value;
}

/* Checks the started module against retail: retail ran the search path's QVM,
 * which is game's; this executable runs that QVM, or its own module where the
 * QVM is its own stock one. */
static void ExpectStarted(int index, const char *module, const char *game, int qvm) {
    const char *pathGame;
    int pathQVM = SearchPathQVM(module, &pathGame);
    int stock = !Q_stricmp(game, STATIC_MODULES_GAME);
    char message[256];

    Com_sprintf(message, sizeof(message), "%s: retail ran %s's QVM %d (search path: %s's %d); "
                "this executable runs %s", module, game, qvm, pathGame, pathQVM,
                stock ? "its linked-in module" : "that QVM in the interpreter");
    Check(!Q_stricmp(pathGame, game) && pathQVM == qvm, message);
    if (stock) {
        Check(startedNative[index] && !startedQVM[index], message);
    } else {
        Check(!startedNative[index] && startedQVM[index] == qvm, message);
    }
}

/* Each step's expected state: fs_game, and the game whose QVMs run. */
static void ExpectState(const char *fsGame, const char *game) {
    char message[160];

    Com_sprintf(message, sizeof(message), "fs_game is \"%s\" (got \"%s\")", fsGame, FsGame());
    Check(!strcmp(FsGame(), fsGame), message);
    Check(!strcmp(fs_gamedir, fsGame[0] ? fsGame : BASEGAME), "fs_gamedir follows fs_game");
    ExpectStarted(0, "ui", game, !Q_stricmp(game, BASEGAME) ? 103 : 303);
}

/* The client's own start: CL_StartHunkUsers creates the UI. */
static void Launch(void) {
    scenario = "launch";
    StartModule(0, "ui", "vm_ui");
}

/* One gamestate as SV_SendClientGameState writes it; serverGame "" is a
 * baseq3 server, whose systeminfo has no fs_game. */
static void Connect(const char *serverGame, int checksumFeed) {
    static byte buffer[MAX_MSGLEN];
    char systemInfo[128];
    msg_t msg;
    int before = connects;

    Com_sprintf(systemInfo, sizeof(systemInfo), "\\sv_serverid\\7\\sv_pure\\0%s%s",
                serverGame[0] ? "\\fs_game\\" : "", serverGame);
    MSG_Init(&msg, buffer, sizeof(buffer));
    MSG_WriteLong(&msg, 1);
    MSG_WriteByte(&msg, svc_configstring);
    MSG_WriteShort(&msg, CS_SYSTEMINFO);
    MSG_WriteBigString(&msg, systemInfo);
    MSG_WriteByte(&msg, svc_EOF);
    MSG_WriteLong(&msg, 0);					// clientNum
    MSG_WriteLong(&msg, checksumFeed);
    Check(!msg.overflowed, "gamestate written");
    MSG_BeginReading(&msg);
    memset(&clc, 0, sizeof(clc));
    cls.state = CA_CONNECTED;
    CL_ParseGamestate(&msg);
    Check(connects == before + 1 && clc.checksumFeed == checksumFeed, "gamestate parsed");
}

/* Com_Error( ERR_DISCONNECT ): CL_Disconnect leaves fs_game and the search
 * path alone (retail's and this port's alike; the runner checks no engine
 * code but CL_SystemInfoChanged and FS_Startup sets fs_game), Com_Error
 * clears the pure lists, and CL_FlushMemory restarts the UI. */
static void Disconnect(void) {
    cls.state = CA_DISCONNECTED;
    FS_PureServerSetLoadedPaks("", "");
    StartModule(0, "ui", "vm_ui");
}

int main(int argc, char **argv) {
    const char *own = STATIC_MODULES_GAME;
    const char *other = !Q_stricmp(own, BASEGAME) ? "missionpack" : BASEGAME;
    const char *ownFsGame = !Q_stricmp(own, BASEGAME) ? "" : own;
    int i;

    Check(argc == 2, "usage: cross_game_qvm_regression install");
    Q_strncpyz(install, argv[1], sizeof(install));
    Check(VM_InitStaticModules(staticModules, 3) == NULL, "static module brackets");
    Check(!strcmp(DEFAULT_FS_GAME, DEFAULT_FS_GAME_EXPECTED), "the executable's DEFAULT_FS_GAME");
    Check(!strcmp(DEFAULT_FS_GAME, ownFsGame), "each executable starts on its own game");

    Cvar_Init();
    Cmd_Init();
    FS_InitFilesystem();
    VM_Init();

    /* A Finder launch: each executable starts on its own game and runs its
     * own UI, as retail Quake3 and "+set fs_game missionpack" Team Arena did. */
    Launch();
    ExpectState(ownFsGame, own);

    /* (a)/(b): a server of the other game.  Retail set fs_game from its
     * systeminfo ("" for baseq3, which drops missionpack) and ran that
     * game's ui and cgame QVMs; so does this executable, interpreting them. */
    for (i = 0; i < 2; i++) {
        scenario = i ? "reconnect to the other game's server" : "connect to the other game's server";
        Connect(!Q_stricmp(other, BASEGAME) ? "" : other, 0x1234 + i);
        ExpectState(!Q_stricmp(other, BASEGAME) ? "" : other, other);
        ExpectStarted(1, "cgame", other, !Q_stricmp(other, BASEGAME) ? 101 : 301);

        /* Retail kept that fs_game after the disconnect: nothing restores
         * the launch value, so the UI stays the other game's (interpreted)
         * until a server, the Mods menu or a relaunch changes fs_game. */
        scenario = "disconnect from the other game's server";
        Disconnect();
        ExpectState(!Q_stricmp(other, BASEGAME) ? "" : other, other);
    }

    /* (c): back on a server of this executable's own game: its own modules. */
    scenario = "connect to a server of the executable's own game";
    Connect(ownFsGame, 0x2000);
    ExpectState(ownFsGame, own);
    ExpectStarted(1, "cgame", own, !Q_stricmp(own, BASEGAME) ? 101 : 301);
    scenario = "disconnect from the own game's server";
    Disconnect();
    ExpectState(ownFsGame, own);
    scenario = "the same server again";
    Connect(ownFsGame, 0x2000);
    ExpectState(ownFsGame, own);
    ExpectStarted(1, "cgame", own, !Q_stricmp(own, BASEGAME) ? 101 : 301);
    Disconnect();

    /* A baseq3 mod server: its own cgame runs, and the ui is baseq3's, which
     * the base executable runs natively; then a baseq3 server clears it. */
    scenario = "baseq3 mod server";
    Connect("mymod", 0x3000);
    Check(!strcmp(FsGame(), "mymod"), "fs_game is the mod");
    ExpectStarted(0, "ui", BASEGAME, 103);
    Check(!startedNative[1] && startedQVM[1] == 201, "the mod's cgame runs in the interpreter");
    Disconnect();
    Check(!strcmp(FsGame(), "mymod"), "the mod's fs_game stays after the disconnect");
    scenario = "baseq3 server after the mod";
    Connect("", 0x3001);
    ExpectState("", BASEGAME);
    ExpectStarted(1, "cgame", BASEGAME, 101);
    Disconnect();

    Check(nativeLoads == nativeUnloads, "every linked-in load was unloaded");
    FS_Shutdown(qtrue);
    for (i = 0; i < hunkCount; i++) {
        free(hunk[i]);
    }
    for (i = 0; i < 3; i++) {
        free(staticModules[i].image);
    }
    printf("cross-game QVM selection (STATIC_MODULES_GAME \"%s\", DEFAULT_FS_GAME \"%s\"): %d connects\n",
           STATIC_MODULES_GAME, DEFAULT_FS_GAME, connects);
    return 0;
}
