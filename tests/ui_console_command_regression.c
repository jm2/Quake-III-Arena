/* Issue #319: UI_CMD_EXECUTETEXT through the real UI dispatcher and command buffer. */
#include "../code/client/cl_ui.c"
#include "console_command_trap_harness.h"

clientActive_t cl;
clientConnection_t clc;
clientStatic_t cls;
refexport_t re;
char cl_cdkey[34];
int cvar_modifiedFlags;

qboolean SV_GameCommand( void ) { Unexpected( __func__ ); return qfalse; }
void CIN_DrawCinematic( int handle ) { Unexpected( __func__ ); }
int CIN_PlayCinematic( const char *arg0, int xpos, int ypos, int width, int height, int bits ) { Unexpected( __func__ ); return 0; }
e_status CIN_RunCinematic( int handle ) { Unexpected( __func__ ); return FMV_EOF; }
void CIN_SetExtents( int handle, int x, int y, int w, int h ) { Unexpected( __func__ ); }
e_status CIN_StopCinematic( int handle ) { Unexpected( __func__ ); return FMV_EOF; }
qboolean CL_CDKeyValidate( const char *key, const char *checksum ) { Unexpected( __func__ ); return qfalse; }
void CL_ClearPing( int n ) { Unexpected( __func__ ); }
void CL_GetPing( int n, char *buf, int buflen, int *pingtime ) { Unexpected( __func__ ); }
void CL_GetPingInfo( int n, char *buf, int buflen ) { Unexpected( __func__ ); }
int CL_GetPingQueueCount( void ) { Unexpected( __func__ ); return 0; }
int CL_ServerStatus( char *serverAddress, char *serverStatusString, int maxLen ) { Unexpected( __func__ ); return 0; }
qboolean CL_UpdateVisiblePings_f( int source ) { Unexpected( __func__ ); return qfalse; }
int Com_RealTime( qtime_t *qtime ) { Unexpected( __func__ ); return 0; }
cvar_t *Cvar_Get( const char *var_name, const char *value, int flags ) { Unexpected( __func__ ); return NULL; }
void Cvar_InfoStringBuffer( int bit, char *buff, int buffsize ) { Unexpected( __func__ ); }
void Cvar_Register( vmCvar_t *vmCvar, const char *varName, const char *defaultValue, int flags ) { Unexpected( __func__ ); }
void Cvar_Reset( const char *var_name ) { Unexpected( __func__ ); }
void Cvar_Set( const char *var_name, const char *value ) { Unexpected( __func__ ); }
void Cvar_SetSafe( const char *var_name, const char *value ) { Unexpected( __func__ ); }
void Cvar_SetValue( const char *var_name, float value ) { Unexpected( __func__ ); }
void Cvar_SetValueSafe( const char *var_name, float value ) { Unexpected( __func__ ); }
void Cvar_Update( vmCvar_t *vmCvar ) { Unexpected( __func__ ); }
void Cvar_VariableStringBuffer( const char *var_name, char *buffer, int bufsize ) { Unexpected( __func__ ); }
float Cvar_VariableValue( const char *var_name ) { Unexpected( __func__ ); return 0; }
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
qboolean Key_GetOverstrikeMode( void ) { Unexpected( __func__ ); return qfalse; }
qboolean Key_IsDown( int keynum ) { Unexpected( __func__ ); return qfalse; }
char *Key_KeynumToString( int keynum ) { Unexpected( __func__ ); return NULL; }
void Key_SetBinding( int keynum, const char *binding ) { Unexpected( __func__ ); }
void Key_SetOverstrikeMode( qboolean state ) { Unexpected( __func__ ); }
const char *NET_AdrToString( netadr_t a ) { Unexpected( __func__ ); return NULL; }
qboolean NET_CompareAdr( netadr_t a, netadr_t b ) { Unexpected( __func__ ); return qfalse; }
qboolean NET_StringToAdr( const char *s, netadr_t *a ) { Unexpected( __func__ ); return qfalse; }
void SCR_UpdateScreen( void ) { Unexpected( __func__ ); }
sfxHandle_t S_RegisterSound( const char *sample, qboolean compressed ) { Unexpected( __func__ ); return 0; }
void S_StartBackgroundTrack( const char *intro, const char *loop ) { Unexpected( __func__ ); }
void S_StartLocalSound( sfxHandle_t sfx, int channelNum ) { Unexpected( __func__ ); }
void S_StopBackgroundTrack( void ) { Unexpected( __func__ ); }
char *Sys_GetClipboardData( void ) { Unexpected( __func__ ); return NULL; }
int Sys_Milliseconds( void ) { Unexpected( __func__ ); return 0; }
void Z_Free( void *ptr ) { Unexpected( __func__ ); }
void *Z_Malloc( int size ) { Unexpected( __func__ ); return NULL; }

int main( void ) {
	RunConsoleCommandTrap( CL_UISystemCalls, UI_CMD_EXECUTETEXT, "UI" );
	return 0;
}
