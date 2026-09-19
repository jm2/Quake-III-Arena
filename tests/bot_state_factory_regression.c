/* Actual goal/movement factories, native handle values and physical owners. */
#define Q3_ITEM_OWNER_ONLY
#include "bot_item_config_regression.c"
#ifdef MEMORYMANEGER
extern int numblocks, allocatedmemory, totalmemorysize;
#endif
#ifdef Q3_GOAL_STATE
#define state_t bot_goalstate_t
#define states botgoalstates
#define StateFromHandle BotGoalStateFromHandle
#define StateFree BotFreeGoalState
#define StateShutdown BotShutdownGoalAI
static int StateAlloc(void) { return BotAllocGoalState(17); }
#else
#define state_t bot_movestate_t
#define states botmovestates
#define StateFromHandle BotMoveStateFromHandle
#define StateFree BotFreeMoveState
#define StateShutdown BotShutdownMoveAI
static int StateAlloc(void) { return BotAllocMoveState(); }
#endif

static void StateBegin(void)
{
    int i;
    Begin();
    for (i = 0; i <= MAX_CLIENTS; i++) Check(!states[i], "prior native state roots release");
}

static void StateEnd(void)
{
    int i;
    StateShutdown();
    for (i = 0; i <= MAX_CLIENTS; i++) Check(!states[i], "shutdown clears every native state root");
    Check(!heapLive && !hunkLive, "all native state owners physically release before engine reset");
    End();
#ifdef MEMORYMANEGER
    Check(!numblocks && !allocatedmemory && !totalmemorysize, "all tracked state owners release logically");
#endif
}

static void Cleared(int handle)
{
    state_t expected;
    memset(&expected, 0, sizeof(expected));
#ifdef Q3_GOAL_STATE
    expected.client = 17;
#endif
    Check(StateFromHandle(handle) == states[handle] && !memcmp(states[handle], &expected, sizeof(expected)),
          "complete native state bytes and handle lookup retain cleared values and goal client");
}

static void NativeState(int handle)
{
#ifdef Q3_GOAL_STATE
    bot_goal_t goal;
    memset(&goal, 0, sizeof(goal));
    goal.number = 42; goal.areanum = 7; goal.origin[0] = 12.5f;
    BotPushGoal(handle, &goal);
    Check(states[handle]->client == 17 && states[handle]->goalstacktop == 1 &&
          !memcmp(&states[handle]->goalstack[1], &goal, sizeof(goal)), "native public goal stack and client values");
#else
    bot_initmove_t init;
    memset(&init, 0, sizeof(init));
    init.origin[0] = 12.5f; init.velocity[1] = -7.25f; init.viewoffset[2] = 24;
    init.entitynum = 8; init.client = 17; init.thinktime = 0.125f; init.presencetype = 2;
    init.viewangles[1] = 90; init.or_moveflags = MFL_ONGROUND | MFL_WALK;
    BotInitMoveState(handle, &init);
    Check(!memcmp(states[handle]->origin, init.origin, sizeof(init.origin)) &&
          !memcmp(states[handle]->velocity, init.velocity, sizeof(init.velocity)) &&
          !memcmp(states[handle]->viewoffset, init.viewoffset, sizeof(init.viewoffset)) &&
          !memcmp(states[handle]->viewangles, init.viewangles, sizeof(init.viewangles)) &&
          states[handle]->entitynum == 8 && states[handle]->client == 17 && states[handle]->thinktime == 0.125f &&
          states[handle]->presencetype == 2 && states[handle]->moveflags == (MFL_ONGROUND | MFL_WALK),
          "actual native public movement initialization values and flags");
#endif
}

static void Golden(void)
{
    int handle;
    StateBegin(); handle = StateAlloc();
    Check(handle == 1 && requests == 1 && heapLive == 1 && !errors, "native first handle and one physical owner");
    Cleared(handle); NativeState(handle); StateFree(handle);
    Check(!states[handle] && !heapLive, "actual public free releases native owner and root");
    StateEnd();
}

static void Nullable(int occupied)
{
    state_t saved;
    int prior = 0, baseline, target = occupied ? 2 : 1;
    StateBegin();
    if (occupied) { prior = StateAlloc(); Check(prior == 1, "complete prior native state"); NativeState(prior); saved = *states[prior]; }
    baseline = heapLive; Attempt(nativeText); failAt = 1;
    Check(StateAlloc() == 0 && requests == 1 && errors == 1 && !states[target] && heapLive == baseline && !hunkLive,
          "nullable factory returns zero, publishes no state and consumes no physical owner");
    if (prior) Check(!memcmp(&saved, states[prior], sizeof(saved)), "every prior native state byte and root survives failure");
    Attempt(nativeText);
    Check(StateAlloc() == target && requests == 1 && !errors && heapLive == baseline + 1,
          "nullable factory retries at the same first free native handle");
    Cleared(target);
    if (prior) Check(!memcmp(&saved, states[prior], sizeof(saved)), "successful distinct retry preserves prior bytes and roots");
    StateEnd();
}

static void AllSlots(void)
{
    int i, imports;
    StateBegin();
    for (i = 1; i <= MAX_CLIENTS; i++) { Check(StateAlloc() == i, "all 64 native handle slots allocate in order"); Cleared(i); }
    imports = requests;
    Check(StateAlloc() == 0 && requests == imports && heapLive == MAX_CLIENTS && !errors,
          "native handle exhaustion returns zero without an import");
    NativeState(MAX_CLIENTS); StateFree(MAX_CLIENTS);
    Check(StateAlloc() == MAX_CLIENTS && heapLive == MAX_CLIENTS, "last native slot frees and reuses exactly its handle");
    Cleared(MAX_CLIENTS); StateEnd();
    StateBegin(); Check(StateAlloc() == 1 && requests == 1, "complete shutdown permits native first slot reuse"); StateEnd();
}

int main(int argc, char **argv)
{
    if (argc > 1) { int proof = atoi(argv[1]); if (proof < 2) Nullable(proof); else Golden(); return 0; }
    Golden(); Nullable(0); Nullable(1); AllSlots();
    puts("Actual goal/movement nullable handles, prior byte preservation, retry, 64 slots and physical shutdown pass (issue #48)");
    return 0;
}
