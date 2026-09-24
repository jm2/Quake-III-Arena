/* Issue #342: FS_PureServerSetLoadedPaks and FS_PureServerSetReferencedPaks
 * copied every pak name a gamestate sent but freed only as many as it sent
 * checksums, so a name list longer than its checksum list leaked names from
 * the fixed 512 KB small zone on every gamestate.  The strings live inside
 * the zone arena, where LeakSanitizer cannot see them, so this checks the
 * real zone's own accounting instead: the real functions run with the real
 * tokenizer and zone allocator (CopyString -> S_Malloc, Z_Free), and after
 * every gamestate the small zone must hold exactly the names still stored,
 * and the stored lists must be what ioquake3 stores.  Com_Error calls
 * FS_PureServerSetLoadedPaks( "", "" ) before its recursion guard, so a
 * stored name whose zone block another bug damaged must fail once. */
#include Q3_PURE_COMMON_SOURCE
#include "../code/qcommon/files.c"
#include "../code/qcommon/unzip.c"

#include <setjmp.h>
#include <stdarg.h>

#define MAX_WORDS		MAX_STRING_TOKENS
#define ROUNDS			64
#define HOSTILE_NAMES	MAX_STRING_TOKENS

typedef struct {
	int		count;
	char	word[MAX_WORDS][16];
} words_t;

static cvar_t homepath = { .string = "/nonexistent/q3-fs-pure-paks" };
static char hostileNames[HOSTILE_NAMES * 6 + 1], hostileSums[HOSTILE_NAMES * 8 + 1];
static int smallBase, mainBase, gamestates;
static jmp_buf errorFrame;
static int expectingError, errorDepth, errors, errorLevel;
static char errorText[1024];

static void Check( int ok, const char *message ) {
	if ( !ok ) {
		fprintf( stderr, "FS pure paks regression failed (gamestate %d): %s\n", gamestates, message );
		exit( 1 );
	}
}

/* Engine imports; a zone failure is ERR_FATAL on the client.  An expected
 * error takes the real Com_Error's first step, which comes before its
 * com_errorEntered guard, then leaves as the real one does. */
void QDECL Com_Error( int level, const char *fmt, ... ) {
	va_list ap;

	va_start( ap, fmt );
	vsnprintf( errorText, sizeof( errorText ), fmt, ap );
	va_end( ap );
	if ( !expectingError ) {
		fprintf( stderr, "Com_Error(%d): %s\n", level, errorText );
	}
	Check( expectingError, "unexpected engine error" );
	Check( ++errorDepth == 1, "Com_Error re-entered before its recursion guard (unbounded on the target)" );
	FS_PureServerSetLoadedPaks( "", "" );
	errorLevel = level;
	errors++;
	errorDepth--;
	longjmp( errorFrame, 1 );
}
void QDECL Com_Printf( const char *fmt, ... ) { (void)fmt; }
void QDECL Com_DPrintf( const char *fmt, ... ) { (void)fmt; }
void Com_Memset( void *out, const int value, const size_t size ) { memset( out, value, size ); }
void Com_Memcpy( void *out, const void *in, const size_t size ) { memcpy( out, in, size ); }
void Z_LogHeap( void ) {}
void Hunk_Log( void ) {}
void Hunk_SmallLog( void ) {}

/* Only a search path restart or pak I/O reaches these; none happens here. */
static void Unreached( void ) { Check( 0, "no filesystem restart or pak I/O" ); }
cvar_t *Cvar_Get( const char *name, const char *value, int flags ) {
	(void)name; (void)value; (void)flags; Unreached(); return NULL;
}
void Cvar_Set( const char *name, const char *value ) { (void)name; (void)value; Unreached(); }
qboolean CL_CDKeyValidate( const char *key, const char *checksum ) {
	(void)key; (void)checksum; Unreached(); return qfalse;
}
void S_ClearSoundBuffer( void ) { Unreached(); }
void Sys_Mkdir( const char *path ) { (void)path; Unreached(); }
void Sys_EndStreamedFile( fileHandle_t f ) { (void)f; Unreached(); }
char **Sys_ListFiles( const char *directory, const char *extension, char *filter, int *numfiles, qboolean wantsubs ) {
	(void)directory; (void)extension; (void)filter; (void)numfiles; (void)wantsubs; Unreached(); return NULL;
}
void Sys_FreeFileList( char **list ) { (void)list; Unreached(); }
char *Sys_DefaultCDPath( void ) { Unreached(); return ""; }
char *Sys_DefaultInstallPath( void ) { Unreached(); return ""; }
char *Sys_DefaultHomePath( void ) { Unreached(); return ""; }

/* Pak lists are plain space separated words. */
static void Split( const char *text, words_t *words ) {
	int n;

	words->count = 0;
	while ( *text ) {
		while ( *text == ' ' ) {
			text++;
		}
		if ( !*text ) {
			break;
		}
		Check( words->count < MAX_WORDS, "bounded fixture list" );
		if ( *text == '"' ) {		/* a quoted empty name */
			Check( text[1] == '"', "fixture quotes only empty names" );
			words->word[words->count++][0] = 0;
			text += 2;
			continue;
		}
		for ( n = 0; text[n] && text[n] != ' '; n++ ) {
		}
		Check( n < (int)sizeof( words->word[0] ), "fixture word fits" );
		memcpy( words->word[words->count], text, n );
		words->word[words->count++][n] = 0;
		text += n;
	}
}

/* Small-zone bytes one stored name holds, from its zone block header (a block
 * may absorb a tiny free fragment); "" and "0".."9" are static blocks. */
static int NameCost( const char *name ) {
	memblock_t *block = (memblock_t *)( (byte *)name - sizeof( memblock_t ) );

	Check( block->id == ZONEID, "stored name is a zone block" );
	if ( block->tag == TAG_STATIC ) {
		Check( !name[0] || ( !name[1] && name[0] >= '0' && name[0] <= '9' ), "only CopyString's static names" );
		return 0;
	}
	Check( block->tag == TAG_SMALL && block->size >= Z_AllocationSize( (int)strlen( name ) + 1 ),
		"stored name is a live small-zone block" );
	return block->size;
}

/* Every name either list still points at, in small-zone bytes. */
static int StoredCost( char **stored ) {
	int i, cost = 0;

	for ( i = 0; i < MAX_SEARCH_PATHS; i++ ) {
		if ( stored[i] ) {
			cost += NameCost( stored[i] );
		}
	}
	return cost;
}

/* ioquake3 stores every checksum for both lists, every name for the loaded
 * list, and for the referenced list only as many pairs as it has names and
 * checksums.  Nothing may stay stored past those. */
static void CheckStored( const words_t *sums, const words_t *names, char **stored,
		const int *checksums, int storedNames ) {
	int i;

	for ( i = 0; i < sums->count; i++ ) {
		Check( checksums[i] == atoi( sums->word[i] ), "stored checksum" );
	}
	for ( i = 0; i < storedNames; i++ ) {
		Check( stored[i] && !strcmp( stored[i], names->word[i] ), "stored name" );
	}
	for ( ; i < MAX_SEARCH_PATHS; i++ ) {
		Check( !stored[i], "no name stored past the copied names" );
	}
}

/* One CS_SYSTEMINFO change, in CL_SystemInfoChanged's order. */
static void Gamestate( const char *pakSums, const char *pakNames ) {
	words_t *sums = calloc( 2, sizeof( *sums ) ), *names = sums + 1;
	int pairs;

	Check( sums != NULL, "host allocation" );
	gamestates++;
	Split( pakSums, sums );
	Split( pakNames, names );
	pairs = sums->count < names->count ? sums->count : names->count;

	FS_PureServerSetLoadedPaks( pakSums, pakNames );
	FS_PureServerSetReferencedPaks( pakSums, pakNames );

	Check( smallzone->used == smallBase + StoredCost( fs_serverPakNames )
		+ StoredCost( fs_serverReferencedPakNames ), "small zone holds only names still stored (leak)" );
	Check( mainzone->used == mainBase, "main zone untouched" );
	Check( fs_numServerPaks == sums->count, "loaded checksum count" );
	CheckStored( sums, names, fs_serverPakNames, fs_serverPaks, names->count );
	Check( fs_numServerReferencedPaks == pairs, "referenced count is the checksum/name pairs" );
	CheckStored( sums, names, fs_serverReferencedPakNames, fs_serverReferencedPaks, pairs );
	free( sums );
}

/* Missing paks as FS_ComparePaks lists them for the connect screen. */
static void Missing( const char *expected ) {
	char needed[MAX_STRING_CHARS];
	qboolean missing;

	memset( needed, 0x55, sizeof( needed ) );
	missing = FS_ComparePaks( needed, sizeof( needed ), qfalse );
	if ( !expected ) {
		Check( !missing, "no referenced pak is missing" );
		return;
	}
	Check( missing && !strcmp( needed, expected ), "missing referenced paks" );
	Check( FS_ComparePaks( needed, sizeof( needed ), qtrue ), "download list" );
}

static void Round( void ) {
	/* Retail: one checksum per name. */
	Gamestate( "1566731103 -1397871237 908855077", "baseq3/pak0 baseq3/pak1 baseq3/mapA" );
	Missing( "baseq3/mapA.pk3\n" );
	/* Issue #342: more names than checksums. */
	Gamestate( "1566731103 908855077", "baseq3/pak0 baseq3/mapA baseq3/mapB osp/zz baseq3/mapC" );
	Missing( "baseq3/mapA.pk3\n" );
	Gamestate( "", "baseq3/pak0 baseq3/mapA baseq3/mapB osp/zz" );
	Missing( NULL );
	Gamestate( "-7", "baseq3/mapQ 7 \"\" baseq3/mapR" );
	Missing( "baseq3/mapQ.pk3\n" );
	/* Fewer names than checksums: nothing reads past the copied names. */
	Gamestate( "11 -22 33 44", "baseq3/mapD baseq3/mapE" );
	Missing( "baseq3/mapD.pk3\nbaseq3/mapE.pk3\n" );
	Gamestate( "11 -22 33", "" );
	Missing( NULL );
	Gamestate( "11 -22 33", "7 \"\"" );
	Missing( "7.pk3\n" );
	/* A hostile server: the most names a list can tokenize, few checksums. */
	Gamestate( "", hostileNames );
	Gamestate( "1 2", hostileNames );
	Gamestate( hostileSums, hostileNames );
	Gamestate( "1566731103 -1397871237 908855077", "baseq3/pak0 baseq3/pak1 baseq3/mapA" );
	/* Disconnect: no pure server. */
	Gamestate( "", "" );
	Missing( NULL );
	Check( smallzone->used == smallBase, "small zone back at its baseline" );
}

/* Another bug overwrote the header id (loaded) or the trailer mark
 * (referenced) of a stored name's zone block. */
static int *DamageMark( char *name, qboolean trailer ) {
	memblock_t *block = (memblock_t *)( (byte *)name - sizeof( memblock_t ) );

	return trailer ? (int *)( (byte *)block + block->size - 4 ) : &block->id;
}

static void DamagedName( qboolean referenced ) {
	char **stored = referenced ? fs_serverReferencedPakNames : fs_serverPakNames;
	static char *name;
	static int *mark;
	int i;

	Gamestate( "11 -22 33", "baseq3/mapD baseq3/mapE baseq3/mapF" );
	name = stored[1];
	mark = DamageMark( name, referenced );
	Check( *mark == ZONEID, "zone mark" );
	*mark = 0;
	errors = 0;
	expectingError = 1;
	if ( !setjmp( errorFrame ) ) {
		if ( referenced ) {
			FS_PureServerSetReferencedPaks( "44", "baseq3/mapG" );
		} else {
			FS_PureServerSetLoadedPaks( "44", "baseq3/mapG" );
		}
		Check( 0, "a damaged name is freed without an error" );
	}
	expectingError = 0;
	Check( errors == 1 && errorLevel == ERR_FATAL && !errorDepth, "one fatal error" );
	Check( strstr( errorText, referenced ? "wrote past end" : "without ZONEID" ) != NULL, "Z_Free names the damage" );
	Check( !stored[1], "the damaged name is no longer stored" );
	for ( i = 0; i < MAX_SEARCH_PATHS; i++ ) {
		Check( !fs_serverPakNames[i], "Com_Error released the loaded names" );
	}
	/* Mend the block so the zone can be checked again. */
	*mark = ZONEID;
	Z_Free( name );
	Check( smallzone->used == smallBase + StoredCost( fs_serverReferencedPakNames ), "zone after the error" );
	Gamestate( "", "" );
	Check( smallzone->used == smallBase, "small zone back at its baseline after the error" );
}

int main( void ) {
	char *s;
	int i;

	Com_InitSmallZoneMemory();
	mainzone = calloc( 1, 1 << 20 );
	Check( mainzone != NULL, "host allocation" );
	Z_ClearZone( mainzone, 1 << 20 );
	fs_homepath = &homepath;
	for ( i = 0, s = hostileNames; i < HOSTILE_NAMES; i++ ) {
		s += sprintf( s, "%s%04x", i ? " " : "", i );
	}
	for ( i = 0, s = hostileSums; i < HOSTILE_NAMES; i++ ) {
		s += sprintf( s, "%s%d", i ? " " : "", i * 7 - 3000 );
	}
	smallBase = smallzone->used;
	mainBase = mainzone->used;
	for ( i = 0; i < ROUNDS; i++ ) {
		Round();
	}
	DamagedName( qfalse );
	DamagedName( qtrue );
	free( smallzone );
	free( mainzone );
	printf( "FS pure pak lists release every name over %d gamestates (issue #342)\n", gamestates );
	return 0;
}
