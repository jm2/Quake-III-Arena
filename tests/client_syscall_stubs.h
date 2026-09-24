/* Issue #238: engine stand-ins for fixtures that drive the real UI or cgame dispatcher. */
#ifndef Q3_CLIENT_SYSCALL_STUBS_H
#define Q3_CLIENT_SYSCALL_STUBS_H
#include "../code/qcommon/vm_local.h"
#include <limits.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IMAGE_SIZE 4096
clientActive_t cl;
clientConnection_t clc;
clientStatic_t cls;
refexport_t re;
char cl_cdkey[34];
int cvar_modifiedFlags;
botlib_export_t *botlib_export;
cvar_t *com_sv_running;
static vm_t vm;
static byte before[IMAGE_SIZE];
static int expectError, nativeCalls, lastIndex;
static jmp_buf errorJump;

/* Native-shaped tables: an unchecked index reaching them is also an ASan error. */
static char bindings[MAX_KEYS][32];
static qboolean keyDown[MAX_KEYS];
static ping_t pings[MAX_PINGREQUESTS];
static vec3_t soundOrigins[MAX_GENTITIES];
static qboolean soundActive[MAX_GENTITIES];
static int soundListener;
static float soundRead;

/** Fail when dispatch, fault state, or native index use differs. */
static void Check( int ok, const char *message ) {
	if ( !ok ) { fprintf( stderr, "Client syscall index regression failed: %s\n", message ); exit( 1 ); }
}
/** Catch controlled drops and prove the faulting VM kept its complete image. */
void QDECL Com_Error( int level, const char *format, ... ) {
	(void)format;
	Check( expectError && level == ERR_DROP, "unexpected engine error" );
	Check( vm.interpretFaulted && !vm.currentlyInterpreting, "fault state" );
	Check( !memcmp( before, vm.dataBase, IMAGE_SIZE ), "rejection changed VM data" );
	longjmp( errorJump, 1 );
}
/** Restore a live interpreted VM and snapshot its data image. */
static void Reset( void ) {
	vm.interpretFaulted = qfalse;
	vm.currentlyInterpreting = qtrue;
	memcpy( before, vm.dataBase, IMAGE_SIZE );
}
/** Allocate the interpreted VM image the dispatcher checks pointers against. */
static void SetupVM( void ) {
	vm.dataBase = calloc( 1, IMAGE_SIZE );
	Check( vm.dataBase != NULL, "allocation" );
	vm.dataMask = IMAGE_SIZE - 1;
	currentVM = &vm;
}
/** Require one trap to drop its VM before any native table sees the index. */
static void Reject( int (*dispatch)( int * ), int *args ) {
	int calls = nativeCalls;
	Reset(); expectError = 1;
	if ( setjmp( errorJump ) == 0 ) {
		dispatch( args );
		Check( 0, "invalid index accepted" );
	}
	expectError = 0;
	Check( nativeCalls == calls, "rejected index dispatched" );
}
/** Require one trap to reach native code exactly once with the unchanged index. */
static int Accept( int (*dispatch)( int * ), int *args, int index ) {
	int calls = nativeCalls, result;
	Reset();
	result = dispatch( args );
	Check( nativeCalls == calls + 1 && lastIndex == index, "valid index dispatch" );
	Check( !vm.interpretFaulted, "valid index faulted" );
	return result;
}
/** Record native use; a rejected index must never get here. */
static void Native( int index ) {
	Check( !expectError, "rejected index reached native code" );
	nativeCalls++;
	lastIndex = index;
}

/* Mirrors of the retail natives, which only treat -1 specially. */
qboolean Key_IsDown( int keynum ) { Native( keynum ); return keynum == -1 ? qfalse : keyDown[keynum]; }
char *Key_GetBinding( int keynum ) {
	Native( keynum );
	if ( keynum == -1 ) return "";
	return bindings[keynum][0] ? bindings[keynum] : NULL;
}
void Key_SetBinding( int keynum, const char *binding ) {
	Native( keynum );
	if ( keynum == -1 ) return;
	Q_strncpyz( bindings[keynum], binding, sizeof(bindings[keynum]) );
}
void CL_GetPing( int n, char *buf, int buflen, int *pingtime ) {
	Native( n );
	Q_strncpyz( buf, pings[n].info, buflen );
	*pingtime = pings[n].time;
}
void CL_GetPingInfo( int n, char *buf, int buflen ) { Native( n ); Q_strncpyz( buf, pings[n].info, buflen ); }
void S_StartSound( vec3_t origin, int entnum, int entchannel, sfxHandle_t sfx ) {
	Native( entnum );
	if ( !origin ) soundRead = soundOrigins[entnum][0];
}
void S_AddLoopingSound( int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfx ) {
	Native( entityNum ); VectorCopy( origin, soundOrigins[entityNum] ); soundActive[entityNum] = qtrue;
}
void S_AddRealLoopingSound( int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfx ) {
	Native( entityNum ); VectorCopy( origin, soundOrigins[entityNum] ); soundActive[entityNum] = qtrue;
}
void S_StopLoopingSound( int entityNum ) { Native( entityNum ); soundActive[entityNum] = qfalse; }
void S_UpdateEntityPosition( int entityNum, const vec3_t origin ) {
	Native( entityNum ); VectorCopy( origin, soundOrigins[entityNum] );
}
void S_Respatialize( int entityNum, const vec3_t origin, vec3_t axis[3], int inwater ) {
	Native( entityNum ); soundListener = entityNum; soundRead = soundOrigins[soundListener][0];
}

/** Fail when a dispatcher reaches an engine service these fixtures never exercise. */
static void Unexpected( const char *name ) {
	fprintf( stderr, "Client syscall index regression reached %s\n", name ); exit( 1 );
}
void QDECL Com_Printf( const char *fmt, ... ) { (void)fmt; }
void QDECL Com_DPrintf( const char *fmt, ... ) { (void)fmt; }
void Com_Memcpy( void *dest, const void *src, const size_t count ) { memcpy( dest, src, count ); }
void Com_Memset( void *dest, const int val, const size_t count ) { memset( dest, val, count ); }
void Cbuf_AddText( const char *text ) { Unexpected( __func__ ); }
void Cbuf_ExecuteText( int exec_when, const char *text ) { Unexpected( __func__ ); }
void CIN_DrawCinematic( int handle ) { Unexpected( __func__ ); }
int CIN_PlayCinematic( const char *arg0, int xpos, int ypos, int width, int height, int bits ) { Unexpected( __func__ ); return 0; }
e_status CIN_RunCinematic( int handle ) { Unexpected( __func__ ); return FMV_EOF; }
void CIN_SetExtents( int handle, int x, int y, int w, int h ) { Unexpected( __func__ ); }
e_status CIN_StopCinematic( int handle ) { Unexpected( __func__ ); return FMV_EOF; }
void CL_AddReliableCommand( const char *cmd ) { Unexpected( __func__ ); }
qboolean CL_CDKeyValidate( const char *key, const char *checksum ) { Unexpected( __func__ ); return qfalse; }
void CL_ClearPing( int n ) { Unexpected( __func__ ); }
int CL_GetPingQueueCount( void ) { Unexpected( __func__ ); return 0; }
int CL_ServerStatus( char *serverAddress, char *serverStatusString, int maxLen ) { Unexpected( __func__ ); return 0; }
void CL_SystemInfoChanged( void ) { Unexpected( __func__ ); }
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
void Cmd_AddCommand( const char *cmd_name, xcommand_t function ) { Unexpected( __func__ ); }
int Cmd_Argc( void ) { Unexpected( __func__ ); return 0; }
void Cmd_ArgsBuffer( char *buffer, int bufferLength ) { Unexpected( __func__ ); }
char *Cmd_ArgsFrom( int arg ) { Unexpected( __func__ ); return NULL; }
char *Cmd_Argv( int arg ) { Unexpected( __func__ ); return NULL; }
void Cmd_ArgvBuffer( int arg, char *buffer, int bufferLength ) { Unexpected( __func__ ); }
void Cmd_RemoveCommand( const char *cmd_name ) { Unexpected( __func__ ); }
void Cmd_RemoveCommandSafe( const char *cmd_name ) { Unexpected( __func__ ); }
void Cmd_TokenizeString( const char *text ) { Unexpected( __func__ ); }
int Com_RealTime( qtime_t *qtime ) { Unexpected( __func__ ); return 0; }
void Con_ClearNotify( void ) { Unexpected( __func__ ); }
void Con_Close( void ) { Unexpected( __func__ ); }
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
#ifndef Q3_CLIENT_SYSCALL_REAL_FS	/* fixtures linking the real files.c bring these */
void FS_FCloseFile( fileHandle_t f ) { Unexpected( __func__ ); }
int FS_FOpenFileByMode( const char *qpath, fileHandle_t *f, fsMode_t mode ) { Unexpected( __func__ ); return 0; }
int FS_GetFileList( const char *path, const char *extension, char *listbuf, int bufsize ) { Unexpected( __func__ ); return 0; }
int FS_Read( void *buffer, int len, fileHandle_t f ) { Unexpected( __func__ ); return 0; }
int FS_Read2( void *buffer, int len, fileHandle_t f ) { Unexpected( __func__ ); return 0; }
int FS_Seek( fileHandle_t f, long offset, int origin ) { Unexpected( __func__ ); return 0; }
int FS_SV_FOpenFileRead( const char *filename, fileHandle_t *fp ) { Unexpected( __func__ ); return 0; }
fileHandle_t FS_SV_FOpenFileWrite( const char *filename ) { Unexpected( __func__ ); return 0; }
int FS_Write( const void *buffer, int len, fileHandle_t f ) { Unexpected( __func__ ); return 0; }
#endif
int Hunk_MemoryRemaining( void ) { Unexpected( __func__ ); return 0; }
void Key_ClearStates( void ) { Unexpected( __func__ ); }
int Key_GetKey( const char *binding ) { Unexpected( __func__ ); return -1; }
qboolean Key_GetOverstrikeMode( void ) { Unexpected( __func__ ); return qfalse; }
char *Key_KeynumToString( int keynum ) { Unexpected( __func__ ); return NULL; }
void Key_SetOverstrikeMode( qboolean state ) { Unexpected( __func__ ); }
const char *NET_AdrToString( netadr_t a ) { Unexpected( __func__ ); return NULL; }
qboolean NET_CompareAdr( netadr_t a, netadr_t b ) { Unexpected( __func__ ); return qfalse; }
qboolean NET_StringToAdr( const char *s, netadr_t *a ) { Unexpected( __func__ ); return qfalse; }
float Q_acos( float c ) { Unexpected( __func__ ); return 0; }
void SCR_UpdateScreen( void ) { Unexpected( __func__ ); }
void S_ClearLoopingSounds( qboolean killall ) { Unexpected( __func__ ); }
sfxHandle_t S_RegisterSound( const char *sample, qboolean compressed ) { Unexpected( __func__ ); return 0; }
void S_StartBackgroundTrack( const char *intro, const char *loop ) { Unexpected( __func__ ); }
void S_StartLocalSound( sfxHandle_t sfx, int channelNum ) { Unexpected( __func__ ); }
void S_StopBackgroundTrack( void ) { Unexpected( __func__ ); }
char *Sys_GetClipboardData( void ) { Unexpected( __func__ ); return NULL; }
int Sys_Milliseconds( void ) { Unexpected( __func__ ); return 0; }
void Sys_SnapVector( float *v ) { Unexpected( __func__ ); }
int VM_CallCompiled( vm_t *target, int *args ) { Unexpected( __func__ ); return 0; }
int VM_CallInterpreted( vm_t *target, int *args ) { Unexpected( __func__ ); return 0; }
#ifndef Q3_CLIENT_SYSCALL_REAL_FS
void Z_Free( void *ptr ) { Unexpected( __func__ ); }
void *Z_Malloc( int size ) { Unexpected( __func__ ); return NULL; }
#endif
#endif
