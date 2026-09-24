/* Issue #39: the cgame and UI cvar and command traps, and a server's
 * systeminfo, must not move the filesystem paths or remove engine commands,
 * while module cvars and commands and the systeminfo cvars keep working.
 * Runs the real cl_cgame.c, cl_ui.c (CL_UISystemCalls), cl_parse.c
 * (CL_SystemInfoChanged), cvar.c and cmd.c. */
#include "../code/client/cl_cgame.c"
#include "protected_cvar_harness.h"

clientActive_t cl;
clientConnection_t clc;
clientStatic_t cls;
refexport_t re;
vm_t *cgvm;
char cl_cdkey[34];
botlib_export_t *botlib_export;
int CL_UISystemCalls( int *args );	/* cl_ui.c */

void FS_PureServerSetLoadedPaks( const char *sums, const char *names ) { (void)sums; (void)names; }
void FS_PureServerSetReferencedPaks( const char *sums, const char *names ) { (void)sums; (void)names; }

/* Engine services the cvar and command traps never reach. */
qboolean SV_GameCommand( void ) { Unexpected( __func__ ); return qfalse; }
void CL_ForwardCommandToServer( const char *string ) { Unexpected( __func__ ); }
void CIN_DrawCinematic( int handle ) { Unexpected( __func__ ); }
int CIN_PlayCinematic( const char *arg0, int xpos, int ypos, int width, int height, int bits ) { Unexpected( __func__ ); return 0; }
e_status CIN_RunCinematic( int handle ) { Unexpected( __func__ ); return FMV_EOF; }
void CIN_SetExtents( int handle, int x, int y, int w, int h ) { Unexpected( __func__ ); }
e_status CIN_StopCinematic( int handle ) { Unexpected( __func__ ); return FMV_EOF; }
void CL_AddReliableCommand( const char *cmd ) { Unexpected( __func__ ); }
qboolean CL_CDKeyValidate( const char *key, const char *checksum ) { Unexpected( __func__ ); return qfalse; }
void CL_ClearPing( int n ) { Unexpected( __func__ ); }
void CL_GetPing( int n, char *buf, int buflen, int *pingtime ) { Unexpected( __func__ ); }
void CL_GetPingInfo( int n, char *buf, int buflen ) { Unexpected( __func__ ); }
int CL_GetPingQueueCount( void ) { Unexpected( __func__ ); return 0; }
int CL_ServerStatus( char *serverAddress, char *serverStatusString, int maxLen ) { Unexpected( __func__ ); return 0; }
qboolean CL_UpdateVisiblePings_f( int source ) { Unexpected( __func__ ); return qfalse; }
void CM_BoxTrace( trace_t *results, const vec3_t start, const vec3_t end, vec3_t mins, vec3_t maxs,
	clipHandle_t model, int brushmask, int capsule ) { Unexpected( __func__ ); }
void CM_TransformedBoxTrace( trace_t *results, const vec3_t start, const vec3_t end, vec3_t mins, vec3_t maxs,
	clipHandle_t model, int brushmask, const vec3_t origin, const vec3_t angles, int capsule ) { Unexpected( __func__ ); }
clipHandle_t CM_InlineModel( int index ) { Unexpected( __func__ ); return 0; }
void CM_LoadMap( const char *name, qboolean clientload, int *checksum ) { Unexpected( __func__ ); }
int CM_NumInlineModels( void ) { Unexpected( __func__ ); return 0; }
int CM_PointContents( const vec3_t p, clipHandle_t model ) { Unexpected( __func__ ); return 0; }
clipHandle_t CM_TempBoxModel( const vec3_t mins, const vec3_t maxs, int capsule ) { Unexpected( __func__ ); return 0; }
int CM_TransformedPointContents( const vec3_t p, clipHandle_t model, const vec3_t origin, const vec3_t angles ) { Unexpected( __func__ ); return 0; }
int Com_RealTime( qtime_t *qtime ) { Unexpected( __func__ ); return 0; }
void Con_ClearNotify( void ) { Unexpected( __func__ ); }
void Con_Close( void ) { Unexpected( __func__ ); }
void FS_FCloseFile( fileHandle_t f ) { Unexpected( __func__ ); }
int FS_FOpenFileByMode( const char *qpath, fileHandle_t *f, fsMode_t mode ) { Unexpected( __func__ ); return 0; }
int FS_GetFileList( const char *path, const char *extension, char *listbuf, int bufsize ) { Unexpected( __func__ ); return 0; }
int FS_Read( void *buffer, int len, fileHandle_t f ) { Unexpected( __func__ ); return 0; }
int FS_Read2( void *buffer, int len, fileHandle_t f ) { Unexpected( __func__ ); return 0; }
int FS_Seek( fileHandle_t f, long offset, int origin ) { Unexpected( __func__ ); return 0; }
int FS_SV_FOpenFileRead( const char *filename, fileHandle_t *fp ) { Unexpected( __func__ ); return 0; }
fileHandle_t FS_SV_FOpenFileWrite( const char *filename ) { Unexpected( __func__ ); return 0; }
int FS_Write( const void *buffer, int len, fileHandle_t f ) { Unexpected( __func__ ); return 0; }
int Hunk_MemoryRemaining( void ) { Unexpected( __func__ ); return 0; }
void Key_ClearStates( void ) { Unexpected( __func__ ); }
char *Key_GetBinding( int keynum ) { Unexpected( __func__ ); return NULL; }
int Key_GetKey( const char *binding ) { Unexpected( __func__ ); return -1; }
qboolean Key_GetOverstrikeMode( void ) { Unexpected( __func__ ); return qfalse; }
qboolean Key_IsDown( int keynum ) { Unexpected( __func__ ); return qfalse; }
char *Key_KeynumToString( int keynum ) { Unexpected( __func__ ); return NULL; }
void Key_SetBinding( int keynum, const char *binding ) { Unexpected( __func__ ); }
void Key_SetOverstrikeMode( qboolean state ) { Unexpected( __func__ ); }
const char *NET_AdrToString( netadr_t a ) { Unexpected( __func__ ); return NULL; }
qboolean NET_CompareAdr( netadr_t a, netadr_t b ) { Unexpected( __func__ ); return qfalse; }
qboolean NET_StringToAdr( const char *s, netadr_t *a ) { Unexpected( __func__ ); return qfalse; }
float Q_acos( float c ) { Unexpected( __func__ ); return 0; }
void SCR_UpdateScreen( void ) { Unexpected( __func__ ); }
void S_AddLoopingSound( int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfx ) { Unexpected( __func__ ); }
void S_AddRealLoopingSound( int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfx ) { Unexpected( __func__ ); }
void S_ClearLoopingSounds( qboolean killall ) { Unexpected( __func__ ); }
sfxHandle_t S_RegisterSound( const char *sample, qboolean compressed ) { Unexpected( __func__ ); return 0; }
void S_Respatialize( int entityNum, const vec3_t origin, vec3_t axis[3], int inwater ) { Unexpected( __func__ ); }
void S_StartBackgroundTrack( const char *intro, const char *loop ) { Unexpected( __func__ ); }
void S_StartLocalSound( sfxHandle_t sfx, int channelNum ) { Unexpected( __func__ ); }
void S_StartSound( vec3_t origin, int entnum, int entchannel, sfxHandle_t sfx ) { Unexpected( __func__ ); }
void S_StopBackgroundTrack( void ) { Unexpected( __func__ ); }
void S_StopLoopingSound( int entityNum ) { Unexpected( __func__ ); }
void S_UpdateEntityPosition( int entityNum, const vec3_t origin ) { Unexpected( __func__ ); }
char *Sys_GetClipboardData( void ) { Unexpected( __func__ ); return NULL; }
int Sys_Milliseconds( void ) { Unexpected( __func__ ); return 0; }
void Sys_SnapVector( float *v ) { Unexpected( __func__ ); }
int VM_CallCompiled( vm_t *target, int *args ) { Unexpected( __func__ ); return 0; }
int VM_CallInterpreted( vm_t *target, int *args ) { Unexpected( __func__ ); return 0; }
void *Z_Malloc( int size ) { Unexpected( __func__ ); return NULL; }

/** A module's trap_Cvar_Register into a vmCvar_t in its image; returns the
 * vmCvar_t argument. */
static int Register( int trap, const char *name, const char *value, int flags ) {
	int handle = Arg( Alloc( sizeof( vmCvar_t ) ) );

	Trap( trap, handle, Str( name ), Str( value ), flags );
	return handle;
}
static vmCvar_t *VMCvar( int handle ) {
	return Image( handle );
}
static qboolean Is( const char *name, const char *value ) {
	return Value( name ) && !strcmp( Value( name ), value );
}
/** A float trap argument, as PASSFLOAT passes it. */
static int FloatArg( float value ) {
	int bits;

	memcpy( &bits, &value, sizeof( bits ) );
	return bits;
}
static int foundCommand;
static const char *wantedCommand;
static void MatchCommand( const char *name ) {
	if ( !strcmp( name, wantedCommand ) ) {
		foundCommand++;
	}
}
/** How many commands are named exactly name. */
static int CommandCount( const char *name ) {
	foundCommand = 0;
	wantedCommand = name;
	Cmd_CommandCompletion( MatchCommand );
	return foundCommand;
}
/** A registration of a protected path, with flags that would publish it: it
 * must neither change the path nor send it anywhere. */
static void CheckNotPublished( const char *name ) {
	Check( !strstr( Cvar_InfoString( CVAR_USERINFO ), name ), "a module put a protected path in the userinfo" );
	Check( !strstr( Cvar_InfoString( CVAR_SERVERINFO ), name ), "a module put a protected path in the serverinfo" );
	Check( !strstr( Cvar_InfoString_Big( CVAR_SYSTEMINFO ), name ), "a module put a protected path in the systeminfo" );
}

/** The cgame keeps its own cvars and commands, and nothing else. */
static void TestCgame( void ) {
	int handle, i;

	handle = Register( CG_CVAR_REGISTER, "cg_modCvar", "1", CVAR_ARCHIVE );
	Trap( CG_CVAR_SET, Str( "cg_modCvar" ), Str( "7" ), 0, 0 );
	Trap( CG_CVAR_UPDATE, handle, 0, 0, 0 );
	Check( Is( "cg_modCvar", "7" ) && VMCvar( handle )->integer == 7, "the cgame sets its own cvar" );
	Trap( CG_CVAR_SET, Str( "cg_modNew" ), Str( "3" ), 0, 0 );
	Check( Is( "cg_modNew", "3" ), "the cgame creates a cvar by setting it" );
	Trap( CG_CVAR_SET, Str( "cg_modNew" ), 0, 0, 0 );
	Check( Is( "cg_modNew", "3" ), "the cgame resets its own cvar" );
	Trap( CG_ADDCOMMAND, Str( "+modcmd" ), 0, 0, 0 );
	Check( CommandCount( "+modcmd" ) == 1, "the cgame adds a command" );
	Trap( CG_REMOVECOMMAND, Str( "+modcmd" ), 0, 0, 0 );
	Check( CommandCount( "+modcmd" ) == 0, "the cgame removes its own command" );
	Trap( CG_REMOVECOMMAND, Str( "nosuchcmd" ), 0, 0, 0 );

	for ( i = 0; i < COUNT( protectedCvars ); i++ ) {
		Refuse( CG_CVAR_SET, Str( protectedCvars[i] ), Str( HOSTILE ), 0, 0, "the cgame set a protected path" );
		Refuse( CG_CVAR_SET, Str( protectedCvars[i] ), 0, 0, 0, "the cgame reset a protected path" );
		handle = Register( CG_CVAR_REGISTER, protectedCvars[i], HOSTILE,
			CVAR_USERINFO | CVAR_SERVERINFO | CVAR_SYSTEMINFO | CVAR_USER_CREATED | CVAR_ROM );
		Check( !strcmp( VMCvar( handle )->string, protectedValues[i] ), "the cgame reads a protected path" );
		CheckNotPublished( protectedCvars[i] );
	}
	Refuse( CG_CVAR_SET, Str( "FS_BASEPATH" ), Str( HOSTILE ), 0, 0, "the cgame set a protected path by another case" );
	CheckPaths( "the cgame moved a protected path" );

	// engine commands (Cmd_Init's and Cvar_Init's) stay
	Refuse( CG_REMOVECOMMAND, Str( "exec" ), 0, 0, 0, "the cgame removed exec" );
	Refuse( CG_REMOVECOMMAND, Str( "cvar_restart" ), 0, 0, 0, "the cgame removed cvar_restart" );
	Refuse( CG_REMOVECOMMAND, Str( "VSTR" ), 0, 0, 0, "the cgame removed vstr by another case" );
	// a command that differs only in case can't be added to stand in for one
	Trap( CG_ADDCOMMAND, Str( "SET" ), 0, 0, 0 );
	Check( CommandCount( "SET" ) == 0, "the cgame added a case variant of an engine command" );
	Refuse( CG_REMOVECOMMAND, Str( "set" ), 0, 0, 0, "the cgame removed set" );
	Check( CommandCount( "exec" ) == 1 && CommandCount( "cvar_restart" ) == 1 && CommandCount( "vstr" ) == 1 &&
		CommandCount( "set" ) == 1, "an engine command went missing" );
}

/** The UI keeps its own cvars (and the retail mods menu fs_game), and nothing else. */
static void TestUI( void ) {
	const char *name = native ? "ui_modNative" : "ui_modCvar";
	int i;

	Trap( UI_CVAR_CREATE, Str( name ), Str( "2" ), CVAR_ARCHIVE, 0 );
	Check( Is( name, "2" ), "the UI creates a cvar" );
	Trap( UI_CVAR_SET, Str( name ), Str( "5" ), 0, 0 );
	Check( Is( name, "5" ), "the UI sets its own cvar" );
	Trap( UI_CVAR_SETVALUE, Str( name ), FloatArg( 9 ), 0, 0 );
	Check( Is( name, "9" ), "the UI sets its own cvar to a whole number" );
	Trap( UI_CVAR_SETVALUE, Str( name ), FloatArg( 0.5f ), 0, 0 );
	Check( Is( name, "0.500000" ), "the UI sets its own cvar to a fraction" );
	Trap( UI_CVAR_SET, Str( "fs_game" ), Str( "missionpack" ), 0, 0 );
	Check( Is( "fs_game", "missionpack" ), "the mods menu sets fs_game" );
	Trap( UI_CVAR_SET, Str( "fs_game" ), Str( "" ), 0, 0 );

	for ( i = 0; i < COUNT( protectedCvars ); i++ ) {
		Refuse( UI_CVAR_SET, Str( protectedCvars[i] ), Str( HOSTILE ), 0, 0, "the UI set a protected path" );
		Refuse( UI_CVAR_SET, Str( protectedCvars[i] ), 0, 0, 0, "the UI reset a protected path" );
		Refuse( UI_CVAR_SETVALUE, Str( protectedCvars[i] ), FloatArg( 1 ), 0, 0, "the UI set a protected path to a number" );
		Trap( UI_CVAR_CREATE, Str( protectedCvars[i] ), Str( HOSTILE ),
			CVAR_USERINFO | CVAR_SERVERINFO | CVAR_SYSTEMINFO | CVAR_USER_CREATED | CVAR_ROM, 0 );
		Trap( UI_CVAR_RESET, Str( protectedCvars[i] ), 0, 0, 0 );
		CheckNotPublished( protectedCvars[i] );
	}
	CheckPaths( "the UI moved a protected path" );
}

/** Deliver a gamestate's CS_SYSTEMINFO through the real CL_SystemInfoChanged;
 * true if it dropped the connection. */
static qboolean SystemInfo( const char *info, qboolean mayDrop ) {
	int before = drops;

	memset( &cl, 0, sizeof( cl ) );
	Q_strncpyz( cl.gameState.stringData, info, sizeof( cl.gameState.stringData ) );
	cl.gameState.stringOffsets[CS_SYSTEMINFO] = 0;
	printed[0] = 0;
	expectDrop = mayDrop;
	if ( setjmp( dropJump ) == 0 ) {
		CL_SystemInfoChanged();
	}
	expectDrop = 0;
	return drops != before;
}
/** The client said it would not take key from the server. */
static qboolean Warned( const char *key ) {
	return strstr( printed, va( "server is not allowed to set %s=", key ) ) != NULL;
}

/** A server sets the systeminfo cvars and the ones it created or the player
 * made, but no engine cvar, and never a protected path. */
static void TestSystemInfo( void ) {
	int handle, i;

	module = "systeminfo";
	native = 0;
	for ( i = 0; i < COUNT( protectedCvars ); i++ ) {
		SystemInfo( va( "\\sv_serverid\\7\\%s\\%s", protectedCvars[i], HOSTILE ), qfalse );
		Check( Warned( protectedCvars[i] ), "a server change to a protected path was not refused" );
		CheckPaths( "a server moved a protected path" );
	}

	// Engine cvars are not the server's, even when the player's command line
	// or config set them first (activeAction runs as commands at the first
	// snapshot) or a module registers them as user-created.
	Check( LoadModule( "cgame", CL_CgameSystemCalls, 0 ), "cgame" );
	Register( CG_CVAR_REGISTER, "cl_allowDownload", "0", CVAR_ARCHIVE | CVAR_USER_CREATED );
	module = "systeminfo";
	SystemInfo( "\\fs_copyfiles\\1\\rconPassword\\x\\activeAction\\quit\\sv_master2\\evil\\cl_allowDownload\\1", qfalse );
	Check( Is( "fs_copyfiles", "0" ) && Warned( "fs_copyfiles" ), "a server set a +set engine cvar" );
	Check( Is( "rconPassword", "secret" ) && Warned( "rconPassword" ) && Is( "activeAction", "demo intro" ) &&
		Warned( "activeAction" ), "a server set a +set cvar the engine registered later" );
	Check( Is( "sv_master2", "master.example.com" ) && Warned( "sv_master2" ),
		"a server set a config cvar the engine registered later" );
	Check( Is( "cl_allowDownload", "0" ) && Warned( "cl_allowDownload" ), "a server set an engine cvar" );

	// the systeminfo cvars still follow the server
	SystemInfo( "\\sv_serverid\\8\\sv_cheats\\0\\fs_game\\osp", qfalse );
	Check( Is( "sv_cheats", "0" ) && Is( "fs_game", "osp" ), "the systeminfo cvars follow the server" );
	SystemInfo( "\\sv_serverid\\9\\sv_cheats\\1", qfalse );
	Check( Is( "sv_cheats", "1" ) && Is( "fs_game", "" ), "a server without fs_game clears it" );

	// a cvar the player made with set is still the server's to set
	Cmd_ExecuteString( "set mymod_option 1" );
	SystemInfo( "\\mymod_option\\2", qfalse );
	Check( Is( "mymod_option", "2" ), "a server sets a cvar the player made" );

	// A cvar nothing has registered yet is the server's: it keeps the value
	// when the Team Arena cgame registers it at its first gamestate (as in
	// ioquake3), and the cgame bounds what it reads (#379); after that the
	// server may not change it.
	SystemInfo( "\\cg_currentSelectedPlayer\\999", qfalse );
	Check( Is( "cg_currentSelectedPlayer", "999" ), "a server creates a cvar" );
	SystemInfo( "\\cg_currentSelectedPlayer\\998", qfalse );
	Check( Is( "cg_currentSelectedPlayer", "998" ), "a server sets a cvar it created" );
	Check( LoadModule( "cgame", CL_CgameSystemCalls, 0 ), "cgame" );
	handle = Register( CG_CVAR_REGISTER, "cg_currentSelectedPlayer", "0", CVAR_ARCHIVE );
	Check( VMCvar( handle )->integer == 998, "the server's value survives the cgame's registration" );
	module = "systeminfo";
	SystemInfo( "\\cg_currentSelectedPlayer\\997", qfalse );
	Check( Is( "cg_currentSelectedPlayer", "998" ) && Warned( "cg_currentSelectedPlayer" ),
		"a server set a cvar the cgame registered" );
	Check( LoadModule( "cgame", CL_CgameSystemCalls, 0 ), "cgame" );
	Trap( CG_CVAR_SET, Str( "cg_currentSelectedPlayer" ), Str( "1" ), 0, 0 );
	Check( Is( "cg_currentSelectedPlayer", "1" ), "the cgame sets the cvar it registered" );

	// the retail cgame registers the prediction cvars without CVAR_SYSTEMINFO
	Register( CG_CVAR_REGISTER, "g_synchronousClients", "0", 0 );
	Register( CG_CVAR_REGISTER, "pmove_fixed", "0", 0 );
	Register( CG_CVAR_REGISTER, "pmove_msec", "8", 0 );
	module = "systeminfo";
	SystemInfo( "\\g_synchronousClients\\1\\pmove_fixed\\1\\pmove_msec\\11", qfalse );
	Check( Is( "g_synchronousClients", "1" ) && Is( "pmove_fixed", "1" ) && Is( "pmove_msec", "11" ),
		"the cgame predicts with the server's movement cvars" );

	// behind the filter, Cvar_SetSafe: a protected path the server could
	// otherwise set (were one ever a systeminfo cvar too) drops the connection
	Cvar_Get( "fs_basegame", "", CVAR_SYSTEMINFO );
	Check( SystemInfo( "\\fs_basegame\\" HOSTILE, qtrue ), "a server change to a systeminfo protected path did not drop" );
	CheckPaths( "a server moved a systeminfo protected path" );
}

int main( void ) {
	int pass;

	StartEngine();
	for ( pass = 0; pass < 2; pass++ ) {
		if ( LoadModule( "cgame", CL_CgameSystemCalls, pass ) ) {
			TestCgame();
		}
		if ( LoadModule( "UI", CL_UISystemCalls, pass ) ) {
			TestUI();
		}
	}
	TestSystemInfo();
	munmap( vm.dataBase, IMAGE_SIZE );
	puts( "cgame, UI and systeminfo keep the filesystem paths and engine commands (issue #39)" );
	return 0;
}
