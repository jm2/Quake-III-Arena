/* Issue #39: the game's cvar traps must not move the filesystem paths, while
 * its own cvars keep working. Runs the real sv_game.c, cvar.c and cmd.c. */
#include "../code/server/sv_game.c"
#include "protected_cvar_harness.h"

server_t sv;
serverStatic_t svs;
cvar_t *sv_maxclients;
vm_t *gvm = &vm;

/* Engine services the cvar traps never reach. */
qboolean CL_GameCommand( void ) { Unexpected( __func__ ); return qfalse; }
qboolean UI_GameCommand( void ) { Unexpected( __func__ ); return qfalse; }
void CL_ForwardCommandToServer( const char *string ) { Unexpected( __func__ ); }
int BotImport_DebugPolygonCreate( int color, int numPoints, vec3_t *points ) { Unexpected( __func__ ); return 0; }
void BotImport_DebugPolygonDelete( int id ) { Unexpected( __func__ ); }
void CM_AdjustAreaPortalState( int area1, int area2, qboolean open ) { Unexpected( __func__ ); }
qboolean CM_AreasConnected( int area1, int area2 ) { Unexpected( __func__ ); return qfalse; }
byte *CM_ClusterPVS( int cluster ) { Unexpected( __func__ ); return NULL; }
clipHandle_t CM_InlineModel( int index ) { Unexpected( __func__ ); return 0; }
int CM_LeafArea( int leafnum ) { Unexpected( __func__ ); return 0; }
int CM_LeafCluster( int leafnum ) { Unexpected( __func__ ); return 0; }
void CM_ModelBounds( clipHandle_t model, vec3_t mins, vec3_t maxs ) { Unexpected( __func__ ); }
int CM_PointLeafnum( const vec3_t p ) { Unexpected( __func__ ); return 0; }
void CM_TransformedBoxTrace( trace_t *results, const vec3_t start, const vec3_t end, vec3_t mins, vec3_t maxs,
	clipHandle_t model, int brushmask, const vec3_t origin, const vec3_t angles, int capsule ) { Unexpected( __func__ ); }
int Com_RealTime( qtime_t *qtime ) { Unexpected( __func__ ); return 0; }
qboolean EA_ClientValid( int client ) { Unexpected( __func__ ); return qfalse; }
void FS_FCloseFile( fileHandle_t f ) { Unexpected( __func__ ); }
int FS_FOpenFileByMode( const char *qpath, fileHandle_t *f, fsMode_t mode ) { Unexpected( __func__ ); return 0; }
int FS_GetFileList( const char *path, const char *extension, char *listbuf, int bufsize ) { Unexpected( __func__ ); return 0; }
int FS_Read2( void *buffer, int len, fileHandle_t f ) { Unexpected( __func__ ); return 0; }
int FS_Seek( fileHandle_t f, long offset, int origin ) { Unexpected( __func__ ); return 0; }
int FS_Write( const void *buffer, int len, fileHandle_t f ) { Unexpected( __func__ ); return 0; }
int SV_AreaEntities( const vec3_t mins, const vec3_t maxs, int *entityList, int maxcount ) { Unexpected( __func__ ); return 0; }
int SV_BotAllocateClient( void ) { Unexpected( __func__ ); return -1; }
void SV_BotFreeClient( int clientNum ) { Unexpected( __func__ ); }
int SV_BotGetConsoleMessage( int client, char *buf, int size ) { Unexpected( __func__ ); return 0; }
int SV_BotGetSnapshotEntity( int client, int ent ) { Unexpected( __func__ ); return -1; }
int SV_BotLibSetup( void ) { Unexpected( __func__ ); return 0; }
int SV_BotLibShutdown( void ) { Unexpected( __func__ ); return 0; }
void SV_ClientThink( client_t *cl, usercmd_t *cmd ) { Unexpected( __func__ ); }
clipHandle_t SV_ClipHandleForEntity( const sharedEntity_t *ent ) { Unexpected( __func__ ); return 0; }
void SV_DropClient( client_t *drop, const char *reason ) { Unexpected( __func__ ); }
void SV_GetConfigstring( int index, char *buffer, int bufferSize ) { Unexpected( __func__ ); }
void SV_GetUserinfo( int index, char *buffer, int bufferSize ) { Unexpected( __func__ ); }
void SV_LinkEntity( sharedEntity_t *ent ) { Unexpected( __func__ ); }
int SV_PointContents( const vec3_t p, int passEntityNum ) { Unexpected( __func__ ); return 0; }
void QDECL SV_SendServerCommand( client_t *cl, const char *fmt, ... ) { Unexpected( __func__ ); }
void SV_SetConfigstring( int index, const char *val ) { Unexpected( __func__ ); }
void SV_SetUserinfo( int index, const char *val ) { Unexpected( __func__ ); }
void SV_Trace( trace_t *results, const vec3_t start, vec3_t mins, vec3_t maxs, const vec3_t end,
	int passEntityNum, int contentmask, int capsule ) { Unexpected( __func__ ); }
void SV_UnlinkEntity( sharedEntity_t *ent ) { Unexpected( __func__ ); }
int Sys_Milliseconds( void ) { Unexpected( __func__ ); return 0; }
void Sys_SnapVector( float *v ) { Unexpected( __func__ ); }
int VM_CallCompiled( vm_t *target, int *args ) { Unexpected( __func__ ); return 0; }
int VM_CallInterpreted( vm_t *target, int *args ) { Unexpected( __func__ ); return 0; }

static qboolean Is( const char *name, const char *value ) {
	return Value( name ) && !strcmp( Value( name ), value );
}

/** The game's trap_Cvar_Register. */
static void RegisterGameCvar( const char *name, int flags ) {
	Trap( G_CVAR_REGISTER, Arg( Alloc( sizeof( vmCvar_t ) ) ), Str( name ), Str( "1" ), flags );
}

/** The game keeps its own cvars, and nothing else. */
static void TestGame( void ) {
	vmCvar_t *vmCvar;
	int handle, offset, i;

	handle = Arg( Alloc( sizeof( vmCvar_t ) ) );
	Trap( G_CVAR_REGISTER, handle, Str( "g_modCvar" ), Str( "1" ), CVAR_SERVERINFO | CVAR_ARCHIVE );
	Trap( G_CVAR_SET, Str( "g_modCvar" ), Str( "4" ), 0, 0 );
	Trap( G_CVAR_UPDATE, handle, 0, 0, 0 );
	vmCvar = Image( handle );
	Check( Is( "g_modCvar", "4" ) && vmCvar->integer == 4, "the game sets its own cvar" );
	Check( Trap( G_CVAR_VARIABLE_INTEGER_VALUE, Str( "g_modCvar" ), 0, 0, 0 ) == 4, "the game reads its own cvar" );
	Trap( G_CVAR_SET, Str( "g_modNew" ), Str( "3" ), 0, 0 );
	Check( Is( "g_modNew", "3" ), "the game creates a cvar by setting it" );
	CheckEngineOnlyFlags( RegisterGameCvar, G_CVAR_SET );

	for ( i = 0; i < COUNT( protectedCvars ); i++ ) {
		Refuse( G_CVAR_SET, Str( protectedCvars[i] ), Str( HOSTILE ), 0, 0, "the game set a protected path" );
		Refuse( G_CVAR_SET, Str( protectedCvars[i] ), 0, 0, 0, "the game reset a protected path" );
		handle = Arg( Alloc( sizeof( vmCvar_t ) ) );
		Trap( G_CVAR_REGISTER, handle, Str( protectedCvars[i] ), Str( HOSTILE ),
			CVAR_SERVERINFO | CVAR_SYSTEMINFO | CVAR_USER_CREATED | CVAR_ROM );
		vmCvar = Image( handle );
		Check( !strcmp( vmCvar->string, protectedValues[i] ), "the game reads a protected path" );
		// server browsers and every client would see it
		Check( !strstr( Cvar_InfoString( CVAR_SERVERINFO ), protectedCvars[i] ),
			"the game put a protected path in the serverinfo" );
		Check( !strstr( Cvar_InfoString_Big( CVAR_SYSTEMINFO ), protectedCvars[i] ),
			"the game put a protected path in the systeminfo" );
	}
	// a long value must not overrun the drop message
	offset = Alloc( MAXPRINTMSG );
	memset( vm.dataBase + offset, 'x', MAXPRINTMSG - 1 );
	Refuse( G_CVAR_SET, Str( "fs_basegame" ), Arg( offset ), 0, 0, "the game set a protected path to a long value" );
	CheckPaths( "the game moved a protected path" );
}

int main( void ) {
	int pass;

	StartEngine();
	for ( pass = 0; pass < 2; pass++ ) {
		if ( LoadModule( "game", SV_GameSystemCalls, pass ) ) {
			TestGame();
		}
	}
	munmap( vm.dataBase, IMAGE_SIZE );
	puts( "the game keeps the filesystem paths (issue #39)" );
	return 0;
}
