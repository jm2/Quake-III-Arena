/* Issue #379: the engine half of the cvar index fixtures. A server's systeminfo
 * reaches the client cvars through the real CL_SystemInfoChanged (cl_parse.c)
 * and cvar.c, where the UI and cgame read them back. */
#include "../code/client/cl_parse.c"

clientActive_t cl;
clientConnection_t clc;
clientStatic_t cls;

void QDECL Com_DPrintf( const char *format, ... ) { (void)format; }
void FS_PureServerSetLoadedPaks( const char *sums, const char *names ) { (void)sums; (void)names; }
void FS_PureServerSetReferencedPaks( const char *sums, const char *names ) { (void)sums; (void)names; }

/** Z_Malloc'd copies in the engine; cvar.c frees them with Z_Free. */
char *CopyString( const char *in ) {
	char *out = malloc( strlen( in ) + 1 );

	if ( !out ) {
		abort();
	}
	strcpy( out, in );
	return out;
}

void Z_Free( void *ptr ) {
	free( ptr );
}

void SystemInfo_Set( const char *name, const char *value ) {
	// clear the cvar first, so an unchanged value is still a modification the
	// modules' next cvar update copies
	Cvar_Set( name, "" );
	memset( &cl, 0, sizeof( cl ) );
	Com_sprintf( cl.gameState.stringData, sizeof( cl.gameState.stringData ),
		"\\sv_serverid\\7\\sv_pure\\0\\%s\\%s", name, value );
	cl.gameState.stringOffsets[CS_SYSTEMINFO] = 0;
	CL_SystemInfoChanged();
}
