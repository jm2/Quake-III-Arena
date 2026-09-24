/* Issue #319: G_SEND_CONSOLE_COMMAND through the real game dispatcher and command buffer. */
#include "../code/server/sv_game.c"
#include "console_command_trap_harness.h"

server_t sv;
serverStatic_t svs;
cvar_t *sv_maxclients;
vm_t *gvm = &vm;

qboolean UI_GameCommand( void ) { Unexpected( __func__ ); return qfalse; }
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
char *Cvar_InfoString( int bit ) { Unexpected( __func__ ); return NULL; }
void Cvar_Register( vmCvar_t *vmCvar, const char *varName, const char *defaultValue, int flags ) { Unexpected( __func__ ); }
void Cvar_Set( const char *var_name, const char *value ) { Unexpected( __func__ ); }
void Cvar_Update( vmCvar_t *vmCvar ) { Unexpected( __func__ ); }
int Cvar_VariableIntegerValue( const char *var_name ) { Unexpected( __func__ ); return 0; }
void Cvar_VariableStringBuffer( const char *var_name, char *buffer, int bufsize ) { Unexpected( __func__ ); }
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

int main( void ) {
	RunConsoleCommandTrap( SV_GameSystemCalls, G_SEND_CONSOLE_COMMAND, "Game" );
	return 0;
}
