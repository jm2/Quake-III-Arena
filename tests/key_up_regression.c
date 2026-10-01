/*
 * Issue #394 follow-up: a key release sends "-button <key> <time>" so that
 * IN_KeyUp can count the part of the frame the button was held. Retail's
 * CL_AddKeyUpCommands had no time parameter, so its "%i" read the C library's
 * time() function instead: a function pointer passed for an int, which made
 * the up time an address and the held time garbage.
 *
 * This test includes the real cl_keys.c and drives CL_KeyEvent with a press
 * and a release of a bound key. The declarations below give the engine's
 * print functions printf format checking in this file only, and the runner
 * builds with -Werror=format. The button commands are compared at run time.
 */
#include <stdarg.h>
#include <stdio.h>

void Com_Printf( const char *fmt, ... ) __attribute__(( format( printf, 1, 2 ) ));
void Com_DPrintf( const char *fmt, ... ) __attribute__(( format( printf, 1, 2 ) ));
void Com_Error( int code, const char *fmt, ... ) __attribute__(( format( printf, 2, 3 ) ));
void Com_sprintf( char *dest, int size, const char *fmt, ... ) __attribute__(( format( printf, 3, 4 ) ));

#include "../code/client/cl_keys.c"
#include <stdlib.h>
#include <string.h>

int cvar_modifiedFlags;
clientConnection_t clc;
clientStatic_t cls;
vm_t *cgvm;
vm_t *uivm;
static char added[1024];

/** Fail with the commands added so far. */
static void Check( int ok, const char *message ) {
	if ( !ok ) {
		fprintf( stderr, "Key up regression failed: %s (added \"%s\")\n", message, added );
		exit( 1 );
	}
}
/** Keep every command a key event adds. */
void Cbuf_AddText( const char *text ) {
	Check( strlen( added ) + strlen( text ) < sizeof( added ), "command buffer overflow" );
	strcat( added, text );
}
void QDECL Com_Printf( const char *format, ... ) { (void)format; }
void QDECL Com_Error( int level, const char *format, ... ) { (void)level; (void)format; Check( 0, "unexpected Com_Error" ); }
char *CopyString( const char *in ) { return strcpy( malloc( strlen( in ) + 1 ), in ); }
void Z_Free( void *ptr ) { free( ptr ); }
/* CL_KeyEvent's console, menu and message paths: a bound key in game takes none of them. */
int g_console_field_width;
static void Unexpected( void ) { Check( 0, "unexpected console, menu or message call" ); }
void Cbuf_ExecuteText( int exec_when, const char *text ) { (void)exec_when; (void)text; Unexpected(); }
void CL_AddReliableCommand( const char *cmd ) { (void)cmd; Unexpected(); }
void CL_Disconnect_f( void ) { Unexpected(); }
void Con_ToggleConsole_f( void ) { Unexpected(); }
void Con_PageUp( void ) { Unexpected(); }
void Con_PageDown( void ) { Unexpected(); }
void Con_Top( void ) { Unexpected(); }
void Con_Bottom( void ) { Unexpected(); }
void SCR_UpdateScreen( void ) { Unexpected(); }
void S_StopAllSounds( void ) { Unexpected(); }
int VM_CallArgs( vm_t *vm, int callNum, const int *args, int argCount ) { (void)vm; (void)callNum; (void)args; (void)argCount; Unexpected(); return 0; }
void Cvar_Set( const char *var_name, const char *value ) { (void)var_name; (void)value; Unexpected(); }
float Cvar_VariableValue( const char *var_name ) { (void)var_name; Unexpected(); return 0; }
void Field_Clear( field_t *edit ) { (void)edit; Unexpected(); }
void Field_CompleteCommand( field_t *edit ) { (void)edit; Unexpected(); }
char *Sys_GetClipboardData( void ) { Unexpected(); return NULL; }

int main( void ) {
	cls.state = CA_ACTIVE;
	Key_SetBinding( 'f', "+attack;+speed" );

	CL_KeyEvent( 'f', qtrue, 1000 );
	Check( !strcmp( added, "+attack 102 1000\n+speed 102 1000\n" ), "key press commands" );
	added[0] = 0;
	CL_KeyEvent( 'f', qfalse, 1234 );
	Check( !strcmp( added, "-attack 102 1234\n-speed 102 1234\n" ), "key release commands lost the event time" );

	puts( "Key release button commands carry the event time (issue #394)" );
	return 0;
}
