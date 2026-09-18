/* Issue #35: indirect native bot commands must use the actual server client allocation. */
#include "../code/server/sv_bot.c"
#include "../code/qcommon/vm_local.h"
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

serverStatic_t svs;
static cvar_t maxclients;
cvar_t *sv_maxclients=&maxclients;
static vm_t gameVM, otherVM;
vm_t *gvm=&gameVM;
static client_t before[2];
static int expectError, commands, expectedClient;
static jmp_buf errorJump;
/** Stop on any unexpected command or client mutation. */
static void Check( int ok, const char *message ) {
	if(!ok) { fprintf(stderr,"Server bot client regression failed: %s\n",message); exit(1); }
}
/** Attribute an indirect bot error to the game even with another cached current VM. */
void QDECL Com_Error( int level, const char *format, ... ) {
	(void)format;
	Check(expectError && level==ERR_DROP && gameVM.interpretFaulted && !gameVM.currentlyInterpreting,"owner error state");
	Check(!otherVM.interpretFaulted && currentVM==&otherVM,"other VM changed");
	Check(!svs.clients || !memcmp(before,svs.clients,sizeof(before)),"native clients changed on error"); longjmp(errorJump,1);
}
/** Ignore unused VM diagnostics. */
void QDECL Com_Printf( const char *format, ... ) { (void)format; }
/** Check the exact validated native client passed to command execution. */
void SV_ExecuteClientCommand( client_t *client, const char *command, qboolean clientOK ) {
	Check(!expectError && client==svs.clients+expectedClient && clientOK && !strcmp(command,"test"),"native command dispatch"); commands++;
}
/** Require each native bot entry point to reject an invalid client before access. */
static void Reject( int client, int operation ) {
	int oldCommands=commands; char output[8]; gameVM.interpretFaulted=qfalse; gameVM.currentlyInterpreting=qtrue;
	if(svs.clients) memcpy(before,svs.clients,sizeof(before)); expectError=1;
	if(setjmp(errorJump)==0) {
		if(operation==0) BotClientCommand(client,"test");
		else if(operation==1) SV_BotGetConsoleMessage(client,output,sizeof(output));
		else SV_BotGetSnapshotEntity(client,-1);
		Check(0,"invalid native client accepted");
	}
	expectError=0; Check(commands==oldCommands,"invalid command dispatched");
}
/** Cover indirect commands, console/snapshot access, immutable bounds, and shutdown absence. */
int main( void ) {
	int i,j,bad[]={-1,2,INT_MAX,INT_MIN};
	svs.clients=calloc(2,sizeof(*svs.clients)); Check(svs.clients!=NULL,"allocation");
	svs.clientCapacity=2; maxclients.integer=MAX_CLIENTS; currentVM=&otherVM;
	for(i=0;i<2;i++) { expectedClient=i; BotClientCommand(i,"test"); Check(SV_BotGetSnapshotEntity(i,-1)==-1,"valid snapshot client"); }
	Check(commands==2,"valid commands");
	for(i=0;i<4;i++) for(j=0;j<3;j++) Reject(bad[i],j);
	free(svs.clients); svs.clients=NULL;
	for(j=0;j<3;j++) Reject(0,j);
	puts("Server indirect bot client regressions passed (issue #35)"); return 0;
}
