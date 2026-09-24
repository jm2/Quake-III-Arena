/* Issue #379: the engine half the UI and cgame cvar index fixtures link.
 * qcommon.h cannot share a unit with the module headers, so this names the
 * real cvar.c entry points the fake syscall tables forward to. */
#ifndef Q3_SYSTEMINFO_CVAR_HARNESS_H
#define Q3_SYSTEMINFO_CVAR_HARNESS_H

void Cvar_Register( vmCvar_t *vmCvar, const char *varName, const char *defaultValue, int flags );
void Cvar_Update( vmCvar_t *vmCvar );
void Cvar_Set( const char *var_name, const char *value );
void Cvar_SetValue( const char *var_name, float value );
float Cvar_VariableValue( const char *var_name );
void Cvar_VariableStringBuffer( const char *var_name, char *buffer, int bufsize );

/* Deliver a CS_SYSTEMINFO string that sets name to value through the real
 * CL_SystemInfoChanged, as a server's gamestate does. */
void SystemInfo_Set( const char *name, const char *value );

#endif
