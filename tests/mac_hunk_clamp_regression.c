/* Issue #230: on the classic Mac a com_hunkMegs archived from a PC
 * q3config.cfg (128 is common) made the hunk calloc fail with "Hunk data
 * failed to allocate", or took the whole partition so that the sound pool and
 * AGL failed later.  Com_InitHunkMemory now asks MaxBlock() (Retro68's calloc
 * is NewPtrClear in the application heap) and takes only what fits beside
 * MAC_HUNK_RESERVE_KB, with a message, never below the 56 MB floor; when even
 * the floor does not fit it stops with a Sys_Error that asks for more memory.
 * The cvar keeps the value the user set.
 *
 * The runner builds the real common.c for __MACOS__ (with Com_Error and
 * Com_Printf renamed out of the way, and the cacheline alignment's (int)
 * cast widened to intptr_t for a 64-bit host) and points <MacTypes.h> and
 * <MacMemory.h> at a fake whose MaxBlock() reports fakeHeap, and calloc at
 * FakeCalloc, which fails beyond fakeHeap and takes what it gives from it. */
#include <setjmp.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include Q3_HUNK_COMMON_SOURCE
#undef calloc
void *calloc( size_t count, size_t size );	/* <stdlib.h> declared FakeCalloc */
#ifndef MAC_HUNK_RESERVE_KB		/* before the fix, so that the checks run and fail */
#define MAC_HUNK_RESERVE_KB	( 12368 + 8 * 1024 )
#endif

#define MB	( 1024L * 1024L )
#define KB	1024L

static long		fakeHeap;
static int		fakeCallocs;
static void		*fakeBlock;
static cvar_t	hunkCvar;
static char		hunkString[16];
static char		output[4096];
static char		fatal[1024];
static jmp_buf	abortCase;

static void Fail( const char *name, const char *message ) {
	fprintf( stderr, "mac_hunk_clamp %s: %s\noutput: %s\nfatal: %s\n", name, message, output, fatal );
	exit( 1 );
}

long MaxBlock( void ) { return fakeHeap; }

void *FakeCalloc( size_t count, size_t size ) {
	fakeCallocs++;
	if ( size && count > (size_t)fakeHeap / size ) {
		return NULL;
	}
	fakeHeap -= (long)( count * size );
	fakeBlock = calloc( count, size );
	return fakeBlock;
}

void QDECL Com_Printf( const char *fmt, ... ) {
	va_list	argptr;
	size_t	used = strlen( output );

	va_start( argptr, fmt );
	vsnprintf( output + used, sizeof( output ) - used, fmt, argptr );
	va_end( argptr );
}

void QDECL Com_Error( int code, const char *fmt, ... ) {
	va_list	argptr;

	va_start( argptr, fmt );
	snprintf( fatal, sizeof( fatal ), "Com_Error %d: ", code );
	vsnprintf( fatal + strlen( fatal ), sizeof( fatal ) - strlen( fatal ), fmt, argptr );
	va_end( argptr );
	longjmp( abortCase, 1 );
}

void QDECL Sys_Error( const char *fmt, ... ) {
	va_list	argptr;

	va_start( argptr, fmt );
	strcpy( fatal, "Sys_Error: " );
	vsnprintf( fatal + strlen( fatal ), sizeof( fatal ) - strlen( fatal ), fmt, argptr );
	va_end( argptr );
	longjmp( abortCase, 1 );
}

cvar_t *Cvar_Get( const char *name, const char *value, int flags ) {
	(void)value;
	if ( strcmp( name, "com_hunkMegs" ) || flags != ( CVAR_LATCH | CVAR_ARCHIVE ) ) {
		Fail( name, "unexpected Cvar_Get" );
	}
	return &hunkCvar;
}

int FS_LoadStack( void ) { return 0; }
void Cmd_AddCommand( const char *name, xcommand_t function ) { (void)name; (void)function; }
int Cmd_Argc( void ) { return 0; }
void CL_ShutdownCGame( void ) {}
void CL_ShutdownUI( void ) {}
void SV_ShutdownGameProgs( void ) {}
void CIN_CloseAllVideos( void ) {}
void VM_Clear( void ) {}
void Hunk_Log( void ) {}
void Hunk_SmallLog( void ) {}

/* Runs Com_InitHunkMemory with com_hunkMegs set to megs and heap bytes free.
 * Returns the megs allocated, or -1 after a fatal error. */
static int Run( int megs, long heap ) {
	int		allocated = -1;

	snprintf( hunkString, sizeof( hunkString ), "%d", megs );
	memset( &hunkCvar, 0, sizeof( hunkCvar ) );
	hunkCvar.name = "com_hunkMegs";
	hunkCvar.string = hunkString;
	hunkCvar.flags = CVAR_LATCH | CVAR_ARCHIVE;
	hunkCvar.integer = megs;
	hunkCvar.value = megs;
	fakeHeap = heap;
	fakeCallocs = 0;
	fakeBlock = NULL;
	output[0] = fatal[0] = 0;
	s_hunkTotal = 0;
	s_hunkData = NULL;
	if ( !setjmp( abortCase ) ) {
		Com_InitHunkMemory();
		allocated = s_hunkTotal / MB;
		if ( s_hunkTotal % MB || !s_hunkData ) {
			Fail( "run", "the hunk is not whole megabytes" );
		}
	}
	free( fakeBlock );
	if ( strcmp( hunkCvar.string, hunkString ) || hunkCvar.integer != megs || hunkCvar.modified
		|| hunkCvar.modificationCount || hunkCvar.latchedString ) {
		Fail( "run", "com_hunkMegs was changed" );
	}
	return allocated;
}

static void Expect( const char *name, int condition, const char *message ) {
	if ( !condition ) {
		Fail( name, message );
	}
}

int main( void ) {
	const long	reserve = MAC_HUNK_RESERVE_KB * KB;
	/* About what the 120,000 KB SIZE minimum leaves once the zone, small
	 * zone, stack and image are in: cmake/mac_partition.py budgets 83,715 KB. */
	const long	minimumHeap = 82 * MB;
	int			megs;

	/* A PC config's 128 at the minimum partition: clamped with a message,
	 * and the sound pool and AGL still have their reserve. */
	megs = Run( 128, minimumHeap );
	Expect( "oversized", megs == ( minimumHeap - 31 - reserve ) / MB, "128 is not clamped to what fits" );
	Expect( "oversized", megs >= 56 && fakeHeap >= reserve, "the clamp leaves less than the reserve" );
	Expect( "oversized", strstr( output, "com_hunkMegs 128 does not fit" ) && strstr( output, "allocating 61 megs" )
		&& strstr( output, "Get Info" ), "no clamp message" );
	Expect( "oversized", !strstr( output, "WARNING" ), "a warning for a clamp above the floor" );

	/* The default and a value that fits are unchanged, with no message. */
	megs = Run( 56, minimumHeap );
	Expect( "default", megs == 56 && !strstr( output, "does not fit" ) && !strstr( output, "WARNING" ),
		"the default 56 is changed at the minimum partition" );
	megs = Run( 60, minimumHeap );
	Expect( "fits", megs == 60 && !strstr( output, "does not fit" ), "a value that fits is changed" );
	megs = Run( 128, 128 * MB + 31 + reserve );
	Expect( "fits", megs == 128 && !strstr( output, "does not fit" ), "128 is changed in a large partition" );

	/* Below the floor still allocates the floor, as retail. */
	megs = Run( 16, minimumHeap );
	Expect( "floor", megs == 56 && strstr( output, "Minimum com_hunkMegs is 56" ), "the 56 MB floor is not kept" );

	/* The floor fits, but not beside the whole reserve: the floor, with a warning. */
	megs = Run( 128, 60 * MB );
	Expect( "tight", megs == 56 && strstr( output, "WARNING: the minimum 56 MB hunk" )
		&& strstr( output, "allocating 56 megs" ), "a tight partition does not get the floor with a warning" );

	/* Not even the floor fits: a clean Sys_Error asking for more memory,
	 * before any allocation. */
	megs = Run( 128, 50 * MB );
	Expect( "too small", megs == -1 && !strncmp( fatal, "Sys_Error: Not enough memory for the 56 MB hunk", 47 )
		&& strstr( fatal, "Get Info" ) && !fakeCallocs, "no clean error when the floor does not fit" );
	megs = Run( 56, 56 * MB + 30 );
	Expect( "too small", megs == -1 && !strncmp( fatal, "Sys_Error:", 10 ) && !fakeCallocs,
		"no clean error one byte short of the floor" );
	megs = Run( 56, 56 * MB + 31 );
	Expect( "exact", megs == 56 && fakeHeap == 0, "the floor does not fit exactly" );

	printf( "Mac com_hunkMegs clamp checks passed (issue #230).\n" );
	return 0;
}
