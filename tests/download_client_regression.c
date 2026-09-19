/* Client download-pair parsing and validation through the production bodies. */
#ifndef Q3_CLIENT_MAIN_SOURCE
#define Q3_CLIENT_MAIN_SOURCE "../code/client/cl_main.c"
#endif
#include Q3_CLIENT_MAIN_SOURCE

static char cvarName[64];
static char cvarValue[MAX_OSPATH];
static int cvarSets;

static void Check( int condition, const char *message ) {
	if ( !condition ) {
		fprintf( stderr, "Client download regression failed: %s\n", message );
		exit( 1 );
	}
}

void QDECL Com_Error( int level, const char *format, ... ) {
	(void)level;
	(void)format;
	Check( qfalse, "unexpected engine error" );
}

void QDECL Com_Printf( const char *format, ... ) {
	(void)format;
}

void QDECL Com_DPrintf( const char *format, ... ) {
	(void)format;
}

void Cvar_Set( const char *name, const char *value ) {
	Q_strncpyz( cvarName, name, sizeof(cvarName) );
	Q_strncpyz( cvarValue, value, sizeof(cvarValue) );
	cvarSets++;
}

void Cvar_SetValue( const char *name, float value ) {
	Q_strncpyz( cvarName, name, sizeof(cvarName) );
	Com_sprintf( cvarValue, sizeof(cvarValue), "%g", value );
	cvarSets++;
}

static void Reset( void ) {
	memset( &clc, 0, sizeof(clc) );
	memset( &cls, 0, sizeof(cls) );
	memset( cvarName, 0, sizeof(cvarName) );
	memset( cvarValue, 0, sizeof(cvarValue) );
	cvarSets = 0;
}

static void RejectPair( const char *localName, const char *remoteName,
	const char *message ) {
	int sequence = clc.reliableSequence;

	Q_strncpyz( clc.downloadName, "sentinel", sizeof(clc.downloadName) );
	Q_strncpyz( clc.downloadTempName, "sentinel", sizeof(clc.downloadTempName) );
	Check( !CL_BeginDownload( localName, remoteName ), message );
	Check( !clc.downloadName[0] && !clc.downloadTempName[0],
		"rejection clears filesystem names" );
	Check( clc.reliableSequence == sequence,
		"rejection sends no reliable download command" );
	Check( !strcmp( cvarName, "cl_downloadName" ) && !cvarValue[0],
		"rejection clears the UI download name" );
}

static void NameBoundaries( void ) {
	char longName[MAX_OSPATH + 8];

	Reset();
	Check( CL_BeginDownload( "mod/pak.pk3", "MOD/PAK.PK3" ),
		"canonical relative pair" );
	Check( !strcmp( clc.downloadName, "mod/pak.pk3" )
		&& !strcmp( clc.downloadTempName, "mod/pak.pk3.tmp" ),
		"complete local and temporary names" );
	Check( clc.reliableSequence == 1
		&& !strcmp( clc.reliableCommands[1], "download MOD/PAK.PK3" ),
		"complete remote command" );
	Check( !strcmp( cvarName, "cl_downloadTime" ) && cvarSets == 4,
		"successful UI publication" );

	RejectPair( "", "mod/pak.pk3", "empty local name" );
	RejectPair( "mod/pak.pk3", "", "empty remote name" );
	RejectPair( "mod/pak.txt", "mod/pak.pk3", "non-pk3 local name" );
	RejectPair( "mod/pak.pk3", "mod/pak.txt", "non-pk3 remote name" );
	RejectPair( "../pak.pk3", "mod/pak.pk3", "forward traversal" );
	RejectPair( "mod/pak.pk3", "..\\pak.pk3", "backslash traversal" );
	RejectPair( "mod::pak.pk3", "mod/pak.pk3", "classic Mac traversal" );
	RejectPair( "/mod/pak.pk3", "mod/pak.pk3", "absolute local path" );
	RejectPair( "mod/pak.pk3", "\\mod\\pak.pk3", "absolute remote path" );
	RejectPair( "C:/pak.pk3", "mod/pak.pk3", "drive-qualified path" );
	RejectPair( "mod/pak.pk3", "mod\\pak.pk3", "separator alias" );
	RejectPair( "mod@pak.pk3", "mod/pak.pk3", "pair delimiter injection" );
	RejectPair( "mod/pak.pk3", "mod;quit.pk3", "command delimiter injection" );
	RejectPair( "mod/new\nline.pk3", "mod/pak.pk3", "line injection" );

	memset( longName, 'a', sizeof(longName) );
	memcpy( longName + sizeof(longName) - 5, ".pk3", 5 );
	RejectPair( "mod/pak.pk3", longName, "oversized remote name" );
	longName[MAX_OSPATH - 4] = '.';
	memcpy( longName + MAX_OSPATH - 3, "pk3", 4 );
	RejectPair( longName, "mod/pak.pk3", "oversized local name" );
}

static void PairListBoundaries( void ) {
	int i;

	Reset();
	Q_strncpyz( clc.downloadList,
		"@bad.txt@bad.txt@mod/pak.pk3@mod/pak.pk3",
		sizeof(clc.downloadList) );
	Check( CL_StartNextDownload(), "rejected pair followed by valid pair" );
	Check( clc.downloadRestart && !clc.downloadList[0]
		&& !strcmp( clc.downloadName, "mod/pak.pk3" )
		&& clc.reliableSequence == 1,
		"only the valid complete pair starts" );

	Reset();
	Q_strncpyz( clc.downloadList, "@mod/pak.pk3", sizeof(clc.downloadList) );
	Check( !CL_StartNextDownload() && !clc.downloadList[0]
		&& clc.reliableSequence == 0,
		"truncated pair is consumed without a command" );

	Reset();
	Q_strncpyz( clc.downloadList, "@mod/pak.pk3@", sizeof(clc.downloadList) );
	Check( !CL_StartNextDownload() && !clc.downloadList[0]
		&& clc.reliableSequence == 0,
		"empty local half is rejected" );

	Reset();
	for ( i = 0; i < sizeof(clc.downloadList) - 1; i++ ) {
		clc.downloadList[i] = '@';
	}
	clc.downloadList[i] = 0;
	Check( !CL_StartNextDownload() && !clc.downloadList[0]
		&& clc.reliableSequence == 0,
		"maximum invalid list is consumed iteratively" );
}

int main( void ) {
	NameBoundaries();
	PairListBoundaries();
	puts( "Client download names, complete pairs and bounded list iteration pass" );
	return 0;
}
