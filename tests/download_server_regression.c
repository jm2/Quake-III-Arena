/* Server download authorization through the production private helper. */
#include "../code/server/server.h"
#include "../code/server/sv_download.h"

static const char *referencedNames;

static void Check( int condition, const char *message ) {
	if ( !condition ) {
		fprintf( stderr, "Server download regression failed: %s\n", message );
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

void Com_Memcpy( void *dest, const void *src, size_t count ) {
	memcpy( dest, src, count );
}

const char *FS_ReferencedPakNames( void ) {
	return referencedNames;
}

qboolean FS_idPak( char *pak, char *base ) {
	int i;

	for ( i = 0; i < NUM_ID_PAKS; i++ ) {
		char expected[MAX_QPATH];
		Com_sprintf( expected, sizeof(expected), "%s/pak%d", base, i );
		if ( !Q_stricmp( pak, expected ) ) {
			return qtrue;
		}
	}
	return qfalse;
}

static void Reject( const char *name, const char *message ) {
	qboolean idPack = qtrue;
	qboolean missionPack = qtrue;

	Check( !SV_ReferencedDownload( name, &idPack, &missionPack ), message );
	Check( !idPack && !missionPack, "rejection resets retail-pak flags" );
}

int main( void ) {
	qboolean idPack;
	qboolean missionPack;
	char longName[MAX_QPATH + 8];

	referencedNames = "  mod/pak\tbaseq3/pak0\nmissionpack/pak1 MIXED/Case  ";
	Check( SV_ReferencedDownload( "mod/pak.pk3", &idPack, &missionPack )
		&& !idPack && !missionPack, "referenced mod pak" );
	Check( SV_ReferencedDownload( "mixed/case.PK3", &idPack, &missionPack )
		&& !idPack && !missionPack, "case-insensitive canonical spelling" );
	Check( SV_ReferencedDownload( "baseq3/pak0.pk3", &idPack, &missionPack )
		&& idPack && !missionPack, "retail base pak classification" );
	Check( SV_ReferencedDownload( "missionpack/pak1.pk3", &idPack, &missionPack )
		&& idPack && missionPack, "retail mission pack classification" );

	Reject( NULL, "null name" );
	Reject( "", "empty name" );
	Reject( ".pk3", "empty pak stem" );
	Reject( "mod/pak", "missing extension" );
	Reject( "mod/pak.pk3.txt", "non-pk3 extension" );
	Reject( "other/pak.pk3", "unreferenced pak" );
	Reject( "../mod/pak.pk3", "forward traversal" );
	Reject( "..\\mod\\pak.pk3", "backslash traversal" );
	Reject( "mod::pak.pk3", "classic Mac traversal" );
	Reject( "/mod/pak.pk3", "absolute slash path" );
	Reject( "\\mod\\pak.pk3", "absolute backslash path" );
	Reject( "mod\\pak.pk3", "backslash separator alias" );
	Reject( "mod:pak.pk3", "colon separator alias" );
	Reject( "mod@pak.pk3", "pair delimiter" );
	Reject( "mod/pak;quit.pk3", "command delimiter" );
	Reject( "mod/pak\nnext.pk3", "line delimiter" );

	memset( longName, 'a', sizeof(longName) );
	memcpy( longName + sizeof(longName) - 5, ".pk3", 5 );
	Reject( longName, "name truncated by the legacy client field" );

	puts( "Server referenced-pak authorization and path spelling pass" );
	return 0;
}
