/* Actual bot script base-folder copy must treat percent sequences as data. */
#include Q3_FORMAT_SCRIPT_SOURCE

static void Check( int condition, const char *message ) {
	if ( !condition ) {
		fprintf( stderr, "Script format regression failed: %s\n", message );
		exit( 1 );
	}
}

void QDECL Com_Error( int level, const char *format, ... ) {
	(void)level;
	(void)format;
	Check( qfalse, "unexpected shared fatal error" );
}

void QDECL Com_Printf( const char *format, ... ) {
	(void)format;
}

int main( void ) {
	char longPath[sizeof(basefolder) + 32];
	const char *hostile = "scripts/%n/%08x/%%";

	PS_SetBaseFolder( (char *)hostile );
	Check( !strcmp( basefolder, hostile ), "percent-bearing base folder copied literally" );

	memset( longPath, 'p', sizeof(longPath) );
	longPath[sizeof(longPath) - 1] = 0;
	PS_SetBaseFolder( longPath );
	Check( strlen(basefolder) == sizeof(basefolder) - 1
		&& !memcmp(basefolder, longPath, sizeof(basefolder) - 1),
		"base folder is bounded and terminated" );

	puts( "Bot script base-folder literal copy and capacity pass" );
	return 0;
}
