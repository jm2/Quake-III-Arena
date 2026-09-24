/* Issue #377: the Team Arena movie and demo lists strip the file extension only
 * from names at least as long as it. The engine lists every name that ends in the
 * requested extension, so a pk3 entry named video/roq or demos/dm_68 is listed as
 * "roq" or "dm_68", one byte shorter than ".roq" or ".dm_68". Comparing such a
 * name against the extension read one byte before the stack list, and wrote a NUL
 * there when that byte was '.'. */
#include "../code/ui/ui_main.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *servedDirectory;
static const char *servedExtension;
static const char *const *servedNames;
static int servedCount;
static int protocol;

/** Fail with a description of the first mismatch. */
static void Check( int ok, const char *what ) {
	if ( !ok ) {
		fprintf( stderr, "UI file list regression failed: %s\n", what );
		exit( 1 );
	}
}

/** FS_GetFileList: the served names, each NUL-terminated, packed from the start of listbuf. */
int trap_FS_GetFileList( const char *path, const char *extension, char *listbuf, int bufsize ) {
	int i, len, used;

	Check( !strcmp( path, servedDirectory ), "listed directory" );
	Check( !strcmp( extension, servedExtension ), "listed extension" );
	used = 0;
	for ( i = 0; i < servedCount; i++ ) {
		len = strlen( servedNames[i] ) + 1;
		Check( used + len < bufsize, "served list fits" );
		memcpy( listbuf + used, servedNames[i], len );
		used += len;
	}
	return servedCount;
}
/** Only the demo list reads a cvar: the protocol its extension is built from. */
float trap_Cvar_VariableValue( const char *var_name ) {
	Check( !strcmp( var_name, "protocol" ), "protocol cvar" );
	return protocol;
}
/** Fail on engine errors. */
void QDECL Com_Error( int level, const char *error, ... ) {
	(void)level;
	fprintf( stderr, "Unexpected Com_Error: %s\n", error );
	exit( 1 );
}
/** Nothing is printed on this path. */
void QDECL Com_Printf( const char *msg, ... ) {
	(void)msg;
}
/** Menu script parsing is kept by the linked ui_shared.c but never runs here. */
int trap_PC_ReadToken( int handle, pc_token_t *pc_token ) {
	(void)handle; (void)pc_token;
	Check( 0, "no menu script parsing" );
	return 0;
}
/** Menu script parsing is kept by the linked ui_shared.c but never runs here. */
int trap_PC_SourceFileAndLine( int handle, char *filename, int *line ) {
	(void)handle; (void)filename; (void)line;
	Check( 0, "no menu script parsing" );
	return 0;
}

/** Check that a list shows expected[0..count). */
static void CheckList( const char **list, int listed, const char *const *expected, int count, const char *what ) {
	int i;
	Check( listed == count, what );
	for ( i = 0; i < count; i++ ) {
		if ( !list[i] || strcmp( list[i], expected[i] ) ) {
			fprintf( stderr, "%s %d: \"%s\", expected \"%s\"\n", what, i, list[i] ? list[i] : "(null)", expected[i] );
			Check( 0, what );
		}
	}
}

/** List the served names as the video directory's roq files through the real UI_LoadMovies. */
static void CheckMovies( const char *const *names, const char *const *expected, int count ) {
	servedDirectory = "video";
	servedExtension = "roq";
	servedNames = names;
	servedCount = count;
	memset( uiInfo.movieList, 0, sizeof( uiInfo.movieList ) );
	UI_LoadMovies();
	CheckList( uiInfo.movieList, uiInfo.movieCount, expected, count, "movie" );
}

/** List the served names as the demos directory's dm_<protocol> files through the real UI_LoadDemos. */
static void CheckDemos( int demoProtocol, const char *const *names, const char *const *expected, int count ) {
	char extension[32];

	Com_sprintf( extension, sizeof( extension ), "dm_%d", demoProtocol );
	protocol = demoProtocol;
	servedDirectory = "demos";
	servedExtension = extension;
	servedNames = names;
	servedCount = count;
	memset( uiInfo.demoList, 0, sizeof( uiInfo.demoList ) );
	UI_LoadDemos();
	CheckList( uiInfo.demoList, uiInfo.demoCount, expected, count, "demo" );
}

#define COUNT( a ) ( (int)( sizeof( a ) / sizeof( ( a )[0] ) ) )

int main( void ) {
	/* The first name starts the stack list: "roq" is shorter than ".roq", ".roq"
	 * as long, the others longer (idlogo.RoQ and demoEnd.RoQ are the retail
	 * pak0.pk3 movies). A name that ends in the extension without its dot is
	 * listed whole, as before. */
	static const char *const movies[] = { "roq", "idlogo.RoQ", "demoEnd.RoQ", ".roq", "fooroq", "sub/end.roq", "a.roq" };
	static const char *const movieNames[] = { "ROQ", "IDLOGO", "DEMOEND", "", "FOOROQ", "SUB/END", "A" };
	/* Normal names list as before when the short name is not first. */
	static const char *const laterMovies[] = { "intro.roq", "roq", "tier1.roq" };
	static const char *const laterMovieNames[] = { "INTRO", "ROQ", "TIER1" };
	static const char *const demos[] = { "dm_68", "four.dm_68", ".dm_68", "xdm_68", "Match.DM_68", "sub/x.dm_68" };
	static const char *const demoNames[] = { "DM_68", "FOUR", "", "XDM_68", "MATCH", "SUB/X" };
	static const char *const laterDemos[] = { "demo1.dm_68", "dm_68" };
	static const char *const laterDemoNames[] = { "DEMO1", "DM_68" };
	/* A one-digit protocol makes the extension one byte shorter. */
	static const char *const shortDemos[] = { "dm_5", ".dm_5", "a.dm_5" };
	static const char *const shortDemoNames[] = { "DM_5", "", "A" };

	CheckMovies( movies, movieNames, COUNT( movies ) );
	CheckMovies( laterMovies, laterMovieNames, COUNT( laterMovies ) );
	CheckDemos( 68, demos, demoNames, COUNT( demos ) );
	CheckDemos( 68, laterDemos, laterDemoNames, COUNT( laterDemos ) );
	CheckDemos( 5, shortDemos, shortDemoNames, COUNT( shortDemos ) );
	puts( "UI movie and demo list extension regressions passed (issue #377)" );
	return 0;
}
