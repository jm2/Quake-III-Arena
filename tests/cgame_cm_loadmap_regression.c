/* Issue #35: a cgame QVM must not replace a listen server's collision map through CG_CM_LOADMAP. */
#include "../code/client/cl_cgame.c"
/* Rename the shared stub so this fixture can record the real call. */
#define CM_LoadMap Unused_CM_LoadMap
#include "client_syscall_stubs.h"
#undef CM_LoadMap

static char loadedName[MAX_QPATH];
static int loads;

void CM_LoadMap( const char *name, qboolean clientload, int *checksum ) {
	Check( clientload && checksum != NULL, "cgame loads are client loads" );
	Q_strncpyz( loadedName, name, sizeof( loadedName ) );
	*checksum = 0;
	loads++;
}

/* cl_ui.c owns the catcher accessors in the engine. */
int Key_GetCatcher( void ) { Unexpected( __func__ ); return 0; }
void Key_SetCatcher( int catcher ) { Unexpected( __func__ ); }

/* Run CG_CM_LOADMAP with a name in the VM image and report whether CM_LoadMap ran. */
static int LoadMap( int serverRunning, netadrtype_t remote, qboolean demo, const char *name ) {
	static cvar_t running;
	int args[2];

	running.integer = serverRunning;
	com_sv_running = &running;
	clc.netchan.remoteAddress.type = remote;
	clc.demoplaying = demo;
	Q_strncpyz( (char *)vm.dataBase + 64, name, MAX_QPATH );
	args[0] = CG_CM_LOADMAP;
	args[1] = 64;
	loads = 0;
	loadedName[0] = 0;
	Reset();
	Check( CL_CgameSystemCalls( args ) == 0, "CG_CM_LOADMAP returns 0" );
	Check( !vm.interpretFaulted, "CG_CM_LOADMAP never faults" );
	Check( !loads || !strcmp( loadedName, name ), "CM_LoadMap gets the cgame's name" );
	return loads;
}

int main( void ) {
	SetupVM();

	/* Listen server: the server's map stays, whatever name the cgame passes */
	Check( LoadMap( 1, NA_LOOPBACK, qfalse, "maps/other.bsp" ) == 0, "listen server map replaced by the cgame" );
	Check( LoadMap( 1, NA_LOOPBACK, qfalse, "maps/q3dm1.bsp" ) == 0, "listen server map reloaded by the cgame" );

	/* Without a local server, or when not playing on it, retail behaviour is unchanged */
	Check( LoadMap( 0, NA_IP, qfalse, "maps/q3dm1.bsp" ) == 1, "remote client loads its map" );
	Check( LoadMap( 0, NA_LOOPBACK, qfalse, "maps/q3dm1.bsp" ) == 1, "client without a server loads its map" );
	Check( LoadMap( 0, NA_BOT, qtrue, "maps/q3dm1.bsp" ) == 1, "demo playback loads its map" );
	Check( LoadMap( 1, NA_IP, qfalse, "maps/q3dm2.bsp" ) == 1, "remote server with a local server keeps retail behaviour" );
	Check( LoadMap( 1, NA_BOT, qtrue, "maps/q3dm2.bsp" ) == 1, "demo with a local server keeps retail behaviour" );

	free( vm.dataBase );
	puts( "Cgame CM_LoadMap regressions passed (issue #35)" );
	return 0;
}
