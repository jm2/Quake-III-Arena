/* Actual EA setup/input operations and physical native heap/hunk owners. */
#define main AASSetupFixtureMain
#define EA_Setup AASFixtureEASetup
#include "aas_setup_regression.c"
#undef EA_Setup
#undef main
#include "../code/game/be_ea.h"
extern int EA_Setup(void);
extern void EA_Shutdown(void);
extern qboolean EA_ClientValid(int client);
extern bot_input_t *botinputs;
#ifdef MEMORYMANEGER
extern int numblocks,allocatedmemory,totalmemorysize;
#endif

static void FinishActions(void)
{
    EA_Shutdown();
    Check(!botinputs&&!EA_ClientValid(0),"actual shutdown resets input pointer/capacity");
    End();
#ifdef MEMORYMANEGER
    Check(!numblocks&&!allocatedmemory&&!totalmemorysize,"tracked native adapter has no logical replacement owners");
#endif
}

static void PrepareActions(int occupied)
{
    Begin();
    Check(!botinputs&&!EA_ClientValid(0),"prior native input state shut down");
    if(occupied) {
        botlibglobals.maxclients=2;
        Check(EA_Setup()==BLERR_NOERROR,"complete native prior input setup");
        EA_Action(1,ACTION_ATTACK);
        EA_SelectWeapon(1,7);
        botinputs[1].speed=91;
        Check(EA_ClientValid(1)&&!EA_ClientValid(2),"native prior actual capacity");
    }
}

static void Nullable(int occupied)
{
    bot_input_t *prior,saved[2],out;
    int imports,arena;
    PrepareActions(occupied);prior=botinputs;
    if(occupied)memcpy(saved,prior,sizeof(saved));
    imports=requests;arena=hunkLive;botlibglobals.maxclients=4;failAt=imports+1;
    Check(EA_Setup()==BLERR_LIBRARYNOTSETUP&&requests==failAt&&botinputs==prior&&hunkLive==arena,"nullable input import preserves prior pointer and physical arena");
    Check(EA_ClientValid(0)==occupied&&EA_ClientValid(1)==occupied&&!EA_ClientValid(2),"nullable replacement retains actual prior capacity");
    if(occupied) {
        Check(!memcmp(saved,prior,sizeof(saved)),"failed input setup retains every prior payload byte");
        EA_GetInput(1,0.25f,&out);
        Check(out.actionflags==ACTION_ATTACK&&out.weapon==7&&out.speed==91&&out.thinktime==0.25f,"actual prior inputs remain usable after failed replacement");
    }
    failAt=0;
    Check(EA_Setup()==BLERR_NOERROR&&botinputs&&botinputs!=prior&&EA_ClientValid(3)&&!EA_ClientValid(4)&&hunkLive==arena+1,"nullable failure retries with complete larger native inputs");
    memset(saved,0,sizeof(saved));Check(!memcmp(saved,botinputs,sizeof(saved)),"successful replacement initializes actual native input bytes");
    FinishActions();
}

static void InvalidCount(int occupied,int count)
{
    bot_input_t *prior,saved[2];int imports,arena;
    PrepareActions(occupied);prior=botinputs;if(occupied)memcpy(saved,prior,sizeof(saved));imports=requests;arena=hunkLive;botlibglobals.maxclients=count;
    Check(EA_Setup()==BLERR_LIBRARYNOTSETUP&&requests==imports&&botinputs==prior&&hunkLive==arena&&EA_ClientValid(1)==occupied,"existing count/cost rejection precedes imports and keeps prior inputs");
    if(occupied)Check(!memcmp(saved,prior,sizeof(saved)),"invalid count retains prior payload");
    FinishActions();
}

static void ActionsGolden(void)
{
    bot_input_t input,zero={0};int imports;
    PrepareActions(0);botlibglobals.maxclients=128;
    Check(EA_Setup()==BLERR_NOERROR&&EA_ClientValid(127)&&!EA_ClientValid(128)&&hunkLive==1,"native default complete input capacity");
    Check(!memcmp(&zero,&botinputs[127],sizeof(zero)),"native default cleared payload");
    imports=requests;
    EA_Action(127,ACTION_ATTACK);EA_SelectWeapon(127,5);EA_GetInput(127,0.125f,&input);
    Check(input.actionflags==ACTION_ATTACK&&input.weapon==5&&input.thinktime==0.125f&&requests==imports,"native actual action and input export golden");
    FinishActions();
}

static void Replacement(void)
{
    bot_input_t *prior,zero={0};
    PrepareActions(1);prior=botinputs;botlibglobals.maxclients=4;
    Check(EA_Setup()==BLERR_NOERROR&&botinputs!=prior&&hunkLive==2&&EA_ClientValid(3)&&!EA_ClientValid(4)&&!memcmp(&zero,&botinputs[1],sizeof(zero)),"successful complete replacement publishes cleared payload and new actual capacity");
    FinishActions();
}

int main(int argc,char **argv)
{
    int occupied,i,bad[]={0,-1,INT_MAX,INT_MIN};
    if(argc>1) {
        i=atoi(argv[1]);if(i<2)Nullable(i);else if(i==2)Replacement();else ActionsGolden();return 0;
    }
    for(occupied=0;occupied<2;occupied++) {
        Nullable(occupied);
        for(i=0;i<4;i++)InvalidCount(occupied,bad[i]);
    }
    Replacement();ActionsGolden();
    puts("Real action setup rollback, capacity, native payload and physical heap/hunk lifetime passed (issues #35/#48)");
    return 0;
}
