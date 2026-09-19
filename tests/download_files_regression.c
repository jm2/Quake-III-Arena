/* Filesystem download-list construction through the production files.c body. */
#define main Q3_UnusedZipRegressionMain
#include "fs_zip_regression.c"
#undef main

#include <errno.h>
#include <sys/stat.h>

static cvar_t downloadHome;
static searchpath_t localSearch;
static pack_t localPack;

static void DownloadReset( const char *home ) {
	int i;

	for ( i = 0; i < fs_numServerReferencedPaks; i++ ) {
		fs_serverReferencedPakNames[i] = NULL;
		fs_serverReferencedPaks[i] = 0;
	}
	fs_numServerReferencedPaks = 0;
	fs_searchpaths = NULL;
	memset( &localSearch, 0, sizeof(localSearch) );
	memset( &localPack, 0, sizeof(localPack) );
	memset( &downloadHome, 0, sizeof(downloadHome) );
	downloadHome.string = (char *)home;
	fs_homepath = &downloadHome;
}

static void SetReference( int index, char *name, int checksum ) {
	Check( index >= 0 && index < MAX_SEARCH_PATHS, "reference fixture index" );
	fs_serverReferencedPakNames[index] = name;
	fs_serverReferencedPaks[index] = checksum;
	if ( index >= fs_numServerReferencedPaks ) {
		fs_numServerReferencedPaks = index + 1;
	}
}

static void GoldenLists( const char *home ) {
	char output[256];
	char path[MAX_OSPATH * 2];
	FILE *file;

	DownloadReset( home );
	SetReference( 0, "mod/missing", 0x12345678 );
	Check( FS_ComparePaks( output, sizeof(output), qtrue )
		&& !strcmp( output, "@mod/missing.pk3@mod/missing.pk3" ),
		"missing referenced pak pair" );

	Com_sprintf( path, sizeof(path), "%s/mod", home );
	Check( mkdir( path, 0700 ) == 0 || errno == EEXIST,
		"fixture mod directory" );
	Com_sprintf( path, sizeof(path), "%s/mod/existing.pk3", home );
	file = fopen( path, "wb" );
	Check( file != NULL, "fixture existing pak" );
	Check( fwrite( "pk3", 1, 3, file ) == 3 && fclose( file ) == 0,
		"fixture existing pak contents" );

	DownloadReset( home );
	SetReference( 0, "mod/existing", 0x12345678 );
	Check( FS_ComparePaks( output, sizeof(output), qtrue )
		&& !strcmp( output,
			"@mod/existing.pk3@mod/existing.12345678.pk3" ),
		"wrong-checksum local pak gets a distinct complete name" );

	localPack.checksum = 0x12345678;
	localSearch.pack = &localPack;
	fs_searchpaths = &localSearch;
	memset( output, 0xa5, sizeof(output) );
	Check( !FS_ComparePaks( output, sizeof(output), qtrue ) && !output[0],
		"matching checksum requires no download" );
}

static void AtomicCapacity( const char *home ) {
	const char *first = "@mod/a.pk3@mod/a.pk3";
	char output[128];

	DownloadReset( home );
	SetReference( 0, "mod/a", 1 );
	Check( FS_ComparePaks( output, strlen(first) + 1, qtrue )
		&& !strcmp( output, first ), "exact pair capacity" );
	memset( output, 0xa5, sizeof(output) );
	Check( !FS_ComparePaks( output, strlen(first), qtrue ) && !output[0],
		"one-byte-short capacity publishes no partial pair" );

	SetReference( 1, "mod/second", 2 );
	memset( output, 0xa5, sizeof(output) );
	Check( FS_ComparePaks( output, strlen(first) + 1, qtrue )
		&& !strcmp( output, first ),
		"later overflow retains only earlier complete pairs" );
}

static void RejectedNames( const char *home ) {
	char output[256];
	char longName[MAX_ZPATH + 8];
	char *names[] = {
		"", "../pak", "..\\pak", "mod::pak", "/absolute/pak",
		"\\absolute\\pak", "drive:pak", "mod\\pak", "mod@pak",
		"mod/pak;quit", "mod/pak\nnext"
	};
	int i;

	DownloadReset( home );
	for ( i = 0; i < sizeof(names) / sizeof(names[0]); i++ ) {
		SetReference( i, names[i], i + 1 );
	}
	memset( longName, 'a', sizeof(longName) );
	longName[sizeof(longName) - 1] = 0;
	SetReference( i++, longName, 100 );
	SetReference( i, "mod/good", 101 );
	Check( FS_ComparePaks( output, sizeof(output), qtrue )
		&& !strcmp( output, "@mod/good.pk3@mod/good.pk3" ),
		"all unsafe names skipped while a canonical pair remains" );

	memset( output, 0xa5, sizeof(output) );
	Check( !FS_ComparePaks( NULL, sizeof(output), qtrue ),
		"null output rejected" );
	Check( !FS_ComparePaks( output, 0, qtrue )
		&& (unsigned char)output[0] == 0xa5,
		"zero capacity rejected without access" );
}

int main( int argc, char **argv ) {
	Check( argc == 2, "scratch home path argument" );
	GoldenLists( argv[1] );
	AtomicCapacity( argv[1] );
	RejectedNames( argv[1] );
	puts( "Filesystem download pairs, rejection spellings and atomic capacity pass" );
	return 0;
}
