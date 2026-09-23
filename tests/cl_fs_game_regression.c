/* Issue #262: a server-supplied fs_game containing ':' walks out of the
 * install folder on HFS, so CL_SystemInfoChanged must refuse it like the
 * other path and shell metacharacters.  Runs the real cl_parse.c code. */
#include "../code/client/cl_parse.c"

clientActive_t cl;
clientConnection_t clc;
clientStatic_t cls;

static char fsGame[MAX_CVAR_VALUE_STRING];
static int gameSets;

static void Check(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "fs_game systeminfo regression failed: %s\n", message);
        exit(1);
    }
}

void QDECL Com_Error(int level, const char *format, ...) {
    (void)level; (void)format;
    Check(0, "unexpected engine error");
}
void QDECL Com_Printf(const char *format, ...) { (void)format; }
void QDECL Com_DPrintf(const char *format, ...) { (void)format; }
void Cvar_SetCheatState(void) {}
void FS_PureServerSetLoadedPaks(const char *sums, const char *names) { (void)sums; (void)names; }
void FS_PureServerSetReferencedPaks(const char *sums, const char *names) { (void)sums; (void)names; }
float Cvar_VariableValue(const char *name) { (void)name; return 0; }
char *Cvar_VariableString(const char *name) {
    Check(!strcmp(name, "fs_game"), "only fs_game is read back");
    return fsGame;
}
void Cvar_Set(const char *name, const char *value) {
    if (!strcmp(name, "fs_game")) {
        Q_strncpyz(fsGame, value, sizeof(fsGame));
        gameSets++;
    }
}

/* Deliver one systeminfo configstring with the client's fs_game preset. */
static void SystemInfo(const char *current, const char *serverGame) {
    memset(&cl, 0, sizeof(cl));
    Com_sprintf(cl.gameState.stringData, sizeof(cl.gameState.stringData),
                "\\sv_serverid\\7\\sv_pure\\0\\fs_game\\%s", serverGame);
    cl.gameState.stringOffsets[CS_SYSTEMINFO] = 0;
    Q_strncpyz(fsGame, current, sizeof(fsGame));
    gameSets = 0;
    CL_SystemInfoChanged();
}

int main(void) {
    static const char *const hostile[] = {
        "::", ":x", "x:", "a:b", ":", "Macintosh HD:System Folder", "../x", "x/y", "x;quit", NULL
    };
    int i;

    for (i = 0; hostile[i]; i++) {
        SystemInfo("", hostile[i]);
        Check(gameSets == 0 && !fsGame[0], "hostile server fs_game refused");
        SystemInfo("osp", hostile[i]);
        Check(gameSets == 1 && !fsGame[0], "hostile server fs_game clears the previous mod");
    }
    SystemInfo("", "osp");
    Check(gameSets == 1 && !strcmp(fsGame, "osp"), "retail mod fs_game accepted");
    SystemInfo("", "missionpack");
    Check(gameSets == 1 && !strcmp(fsGame, "missionpack"), "Team Arena fs_game accepted");
    return 0;
}
