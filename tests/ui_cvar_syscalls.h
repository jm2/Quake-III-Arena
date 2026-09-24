/* Issue #379: cl_ui.c's syscalls on the paths the Team Arena UI and q3_ui cvar
 * index fixtures drive. Cvars go to the real cvar.c behind the real
 * CL_SystemInfoChanged (tests/systeminfo_cvar_harness.c); everything else is
 * an empty install: no files, empty menu scripts, and a new handle for every
 * shader. Include it after the UI source; the fixture defines Check. */
#ifndef Q3_UI_CVAR_SYSCALLS_H
#define Q3_UI_CVAR_SYSCALLS_H
#include "systeminfo_cvar_harness.h"
#include <stdarg.h>
#include <string.h>

#define LOCAL_CLIENT 2

void dllEntry( int (QDECL *syscallptr)( int arg, ... ) );	/* ui_syscalls.c */
static void Check( int ok, const char *what );

static char configStrings[MAX_CONFIGSTRINGS][MAX_INFO_STRING];
static char executed[MAX_STRING_CHARS];	/* EXEC_APPEND text */
static char printed[MAX_STRING_CHARS];	/* the last UI_PRINT text */
static qhandle_t lastShader;	/* the last registered shader handle */
static qhandle_t drawnShader;	/* the last shader drawn */

/** The UI's syscalls; cvars go to cvar.c. */
static int QDECL FakeSyscall( int command, ... ) {
	va_list ap;
	int result = 0;

	va_start( ap, command );
	switch ( command ) {
	case UI_ERROR:
		Check( 0, va_arg( ap, const char * ) );
		break;
	case UI_PRINT:
		Q_strncpyz( printed, va_arg( ap, const char * ), sizeof( printed ) );
		break;
	case UI_MILLISECONDS:
	case UI_R_SETCOLOR:
	case UI_KEY_SETCATCHER:
	case UI_KEY_CLEARSTATES:
	case UI_LAN_LOADCACHEDSERVERS:
	case UI_FS_GETFILELIST:	/* no files */
	case UI_PC_READ_TOKEN:	/* empty menu scripts */
	case UI_PC_FREE_SOURCE:
		break;
	case UI_CVAR_REGISTER: {
		vmCvar_t *vmCvar = va_arg( ap, vmCvar_t * );
		const char *name = va_arg( ap, const char * );
		const char *value = va_arg( ap, const char * );
		Cvar_Register( vmCvar, name, value, va_arg( ap, int ) );
		break;
	}
	case UI_CVAR_UPDATE:
		Cvar_Update( va_arg( ap, vmCvar_t * ) );
		break;
	case UI_CVAR_SET: {
		const char *name = va_arg( ap, const char * );
		Cvar_Set( name, va_arg( ap, const char * ) );
		break;
	}
	case UI_CVAR_SETVALUE: {
		const char *name = va_arg( ap, const char * );
		int bits = va_arg( ap, int );	// PASSFLOAT
		float value;
		memcpy( &value, &bits, sizeof( value ) );
		Cvar_SetValue( name, value );
		break;
	}
	case UI_CVAR_VARIABLEVALUE: {
		float value = Cvar_VariableValue( va_arg( ap, const char * ) );
		memcpy( &result, &value, sizeof( result ) );	// FloatAsInt
		break;
	}
	case UI_CVAR_VARIABLESTRINGBUFFER: {
		const char *name = va_arg( ap, const char * );
		char *buffer = va_arg( ap, char * );
		Cvar_VariableStringBuffer( name, buffer, va_arg( ap, int ) );
		break;
	}
	case UI_CMD_EXECUTETEXT:
		Check( va_arg( ap, int ) == EXEC_APPEND, "commands are appended" );
		Q_strcat( executed, sizeof( executed ), va_arg( ap, const char * ) );
		break;
	case UI_FS_FOPENFILE: {
		fileHandle_t *f;
		va_arg( ap, const char * );
		f = va_arg( ap, fileHandle_t * );
		if ( f ) {
			*f = 0;
		}
		result = -1;
		break;
	}
	case UI_PC_LOAD_SOURCE:
		result = 1;
		break;
	case UI_R_REGISTERSHADERNOMIP:
		result = ++lastShader;
		break;
	case UI_R_DRAWSTRETCHPIC: {
		int i;
		for ( i = 0; i < 8; i++ ) {
			va_arg( ap, int );	// PASSFLOAT x, y, w, h, s1, t1, s2, t2
		}
		drawnShader = va_arg( ap, qhandle_t );
		break;
	}
	case UI_S_REGISTERSOUND:
		result = 1;
		break;
	case UI_R_REGISTERMODEL:	/* no player models */
	case UI_R_REGISTERSKIN:
		break;
	case UI_KEY_GETBINDINGBUF: {
		char *buffer;
		va_arg( ap, int );
		buffer = va_arg( ap, char * );
		if ( va_arg( ap, int ) > 0 ) {
			buffer[0] = 0;
		}
		break;
	}
	case UI_KEY_GETCATCHER:
		result = KEYCATCH_UI;
		break;
	case UI_GETCLIENTSTATE: {
		uiClientState_t *state = va_arg( ap, uiClientState_t * );
		memset( state, 0, sizeof( *state ) );
		state->connState = CA_ACTIVE;
		state->clientNum = LOCAL_CLIENT;
		break;
	}
	case UI_GETCONFIGSTRING: {
		int index = va_arg( ap, int );
		char *buffer = va_arg( ap, char * );
		int size = va_arg( ap, int );
		Check( index >= 0 && index < MAX_CONFIGSTRINGS, "configstring index" );
		Q_strncpyz( buffer, configStrings[index], size );
		result = 1;
		break;
	}
	case UI_GETGLCONFIG: {
		glconfig_t *config = va_arg( ap, glconfig_t * );
		memset( config, 0, sizeof( *config ) );
		config->vidWidth = 640;
		config->vidHeight = 480;
		break;
	}
	default:
		Check( 0, va( "unexpected UI syscall %d", command ) );
	}
	va_end( ap );
	return result;
}

#endif
