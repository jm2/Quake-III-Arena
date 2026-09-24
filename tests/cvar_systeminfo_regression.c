/* Issue #303: pak lists that fill CS_SYSTEMINFO must not push sv_serverid and the other short keys out. */
#include "q_shared.h"
#include "qcommon.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern cvar_t *cvar_vars;	/* cvar.c */

cvar_t *sv_serverid;	/* assigned by SV_Init's systeminfo block */
cvar_t *sv_pure;

/* The lists SV_SpawnServer fills from the filesystem. */
static const char *pakListKeys[] = {
	"sv_paks", "sv_pakNames", "sv_referencedPaks", "sv_referencedPakNames"
};

/* Short keys clients act on; without sv_serverid a client reloads the gamestate forever. */
static const char *shortKeys[] = {
	"sv_serverid", "sv_pure", "sv_cheats", "timescale", "fs_game",
	"cl_anonymous", "g_synchronousClients", "pmove_fixed", "pmove_msec"
};

#define ARRAY_LEN( a ) ( (int)( sizeof( a ) / sizeof( ( a )[0] ) ) )

static char printed[65536];
static size_t printedLength;
static const char *scenario = "setup";
static int numPaks;

static void Check( int condition, const char *message ) {
	if ( !condition ) {
		fprintf( stderr, "Cvar systeminfo regression failed (%s, %i pk3s): %s\n", scenario, numPaks, message );
		exit( 1 );
	}
}

void QDECL Com_Error( int level, const char *error, ... ) {
	va_list args;

	(void)level;
	va_start( args, error );
	vfprintf( stderr, error, args );
	va_end( args );
	fputc( '\n', stderr );
	exit( 2 );
}

/* Capture console prints so the dropped-key warnings can be checked. */
void QDECL Com_Printf( const char *msg, ... ) {
	va_list args;
	int length;

	va_start( args, msg );
	length = vsnprintf( printed + printedLength, sizeof( printed ) - printedLength, msg, args );
	va_end( args );
	Check( length >= 0 && (size_t)length < sizeof( printed ) - printedLength, "print capture capacity" );
	printedLength += (size_t)length;
}

void QDECL Com_DPrintf( const char *msg, ... ) {
	(void)msg;
}

char *CopyString( const char *in ) {
	char *out = malloc( strlen( in ) + 1 );

	Check( out != NULL, "CopyString allocation" );
	strcpy( out, in );
	return out;
}

void Z_Free( void *ptr ) {
	free( ptr );
}

/* Create the CVAR_SYSTEMINFO cvars in engine start-up order. */
static void CreateSystemInfoCvars( void ) {
	Cvar_Get( "sv_cheats", "1", CVAR_ROM | CVAR_SYSTEMINFO );	/* Cvar_Init */
	Cvar_Get( "fs_game", "", CVAR_INIT|CVAR_SYSTEMINFO );	/* FS_Startup */
	Cvar_Get( "timescale", "1", CVAR_CHEAT | CVAR_SYSTEMINFO );	/* Com_Init */
#include Q3_SV_SYSTEMINFO_CVARS
	Cvar_Get( "cl_anonymous", "0", CVAR_INIT|CVAR_SYSTEMINFO );	/* CL_Init */
	Cvar_Get( "g_synchronousClients", "0", CVAR_SYSTEMINFO );	/* game module */
	Cvar_Get( "pmove_fixed", "0", CVAR_SYSTEMINFO );
	Cvar_Get( "pmove_msec", "8", CVAR_SYSTEMINFO );
	Check( sv_serverid && sv_pure, "SV_Init block assigns sv_serverid and sv_pure" );
}

/*
Publish the pak lists as SV_SpawnServer does for count pk3s, in the
FS_LoadedPakChecksums, FS_LoadedPakNames, FS_ReferencedPakChecksums and
FS_ReferencedPakNames formats (search order, BIG_INFO_STRING buffers).
Every pk3 outside baseq3 is referenced; in baseq3 only the first three are.
*/
static void SetPakLists( const char *gamename, int count ) {
	static char sums[BIG_INFO_STRING], names[BIG_INFO_STRING];
	static char refSums[BIG_INFO_STRING], refNames[BIG_INFO_STRING];
	char name[MAX_QPATH];
	unsigned int seed = 303;
	int i, checksum;
	qboolean referenced;

	sums[0] = names[0] = refSums[0] = refNames[0] = 0;
	for ( i = 0 ; i < count ; i++ ) {
		seed = seed * 1103515245u + 12345u;
		checksum = (int)seed;
		Com_sprintf( name, sizeof( name ), "mappack-%03i", i );
		referenced = Q_stricmp( gamename, BASEGAME ) || i < 3;

		Q_strcat( sums, sizeof( sums ), va( "%i ", checksum ) );
		if ( names[0] ) {
			Q_strcat( names, sizeof( names ), " " );
		}
		Q_strcat( names, sizeof( names ), name );
		if ( referenced ) {
			Q_strcat( refSums, sizeof( refSums ), va( "%i ", checksum ) );
		}
		if ( refNames[0] ) {
			Q_strcat( refNames, sizeof( refNames ), " " );
		}
		if ( referenced ) {
			Q_strcat( refNames, sizeof( refNames ), va( "%s/%s", gamename, name ) );
		}
	}
	Cvar_Set( "sv_paks", sums );
	Cvar_Set( "sv_pakNames", names );
	Cvar_Set( "sv_referencedPaks", refSums );
	Cvar_Set( "sv_referencedPakNames", refNames );
}

/* The old single newest-first pass (retail 1.32c's key order), for the unchanged-output check. */
static const char *SinglePassInfo( void ) {
	static char info[BIG_INFO_STRING];
	cvar_t *var;

	info[0] = 0;
	for ( var = cvar_vars ; var ; var = var->next ) {
		if ( var->flags & CVAR_SYSTEMINFO ) {
			Info_SetValueForKey_Big( info, var->name, var->string );
		}
	}
	return info;
}

/* Build systeminfo for 0..maxPaks pk3s; return the first count that leaves a list out. */
static int SweepPakCounts( const char *name, const char *gamename, int maxPaks, char *dropped, int droppedSize ) {
	char info[BIG_INFO_STRING];
	char sent[BIG_INFO_VALUE];
	char message[256];
	char warning[128];
	const char *built, *value;
	int i, infoLength, pairLength, firstDrop = -1;
	qboolean allShort;

	scenario = name;
	Cvar_Set( "fs_game", Q_stricmp( gamename, BASEGAME ) ? gamename : "" );
	dropped[0] = 0;
	for ( numPaks = 0 ; numPaks <= maxPaks ; numPaks++ ) {
		SetPakLists( gamename, numPaks );
		printedLength = 0;
		printed[0] = 0;
		built = Cvar_InfoString_Big( CVAR_SYSTEMINFO );
		Check( strlen( built ) < BIG_INFO_STRING, "systeminfo stays inside BIG_INFO_STRING" );
		Q_strncpyz( info, built, sizeof( info ) );
		infoLength = strlen( info );

		for ( i = 0 ; i < ARRAY_LEN( shortKeys ) ; i++ ) {
			value = Cvar_VariableString( shortKeys[i] );
			if ( !value[0] ) {
				continue;
			}
			Q_strncpyz( sent, Info_ValueForKey( info, shortKeys[i] ), sizeof( sent ) );
			Com_sprintf( message, sizeof( message ), "%s is present with its value", shortKeys[i] );
			Check( !strcmp( sent, value ), message );
		}

		allShort = qtrue;
		for ( i = 0 ; i < ARRAY_LEN( pakListKeys ) ; i++ ) {
			value = Cvar_VariableString( pakListKeys[i] );
			if ( strlen( value ) >= MAX_INFO_VALUE ) {
				allShort = qfalse;
			}
			Q_strncpyz( sent, Info_ValueForKey( info, pakListKeys[i] ), sizeof( sent ) );
			Com_sprintf( warning, sizeof( warning ), "WARNING: no room for %s (%i chars)",
				pakListKeys[i], (int)strlen( value ) );
			if ( !value[0] || sent[0] ) {
				Com_sprintf( message, sizeof( message ), "%s is sent whole and not reported", pakListKeys[i] );
				Check( !strcmp( sent, value ) && !strstr( printed, warning ), message );
				continue;
			}
			/* a list is only left out when it can't fit next to everything else, and never silently */
			pairLength = strlen( pakListKeys[i] ) + strlen( value ) + 2;
			Com_sprintf( message, sizeof( message ), "%s is left out only when it cannot fit", pakListKeys[i] );
			Check( infoLength + pairLength >= BIG_INFO_STRING, message );
			Com_sprintf( message, sizeof( message ), "left-out %s is reported", pakListKeys[i] );
			Check( strstr( printed, warning ) != NULL, message );
			if ( firstDrop < 0 ) {
				firstDrop = numPaks;
			}
			if ( firstDrop == numPaks ) {
				Q_strcat( dropped, droppedSize, dropped[0] ? va( " %s", pakListKeys[i] ) : pakListKeys[i] );
			}
		}

		/* while every value would fit a normal info string the output is the old one, byte for byte */
		if ( allShort ) {
			Check( !strcmp( info, SinglePassInfo() ), "short-list systeminfo keeps the retail key order" );
		}
	}
	return firstDrop;
}

int main( void ) {
	char dropped[256];
	int baseDrop, modDrop;

	CreateSystemInfoCvars();
	Cvar_Set( "sv_serverid", va( "%i", 3601234 ) );	/* com_frameTime an hour after start-up */

	baseDrop = SweepPakCounts( "baseq3", BASEGAME, 800, dropped, sizeof( dropped ) );
	Check( baseDrop > 0, "baseq3 sweep reaches a count that fills systeminfo" );
	printf( "baseq3: every key fits up to %i pk3s; from %i, left out: %s\n", baseDrop - 1, baseDrop, dropped );

	modDrop = SweepPakCounts( "mod", "mymod", 800, dropped, sizeof( dropped ) );
	Check( modDrop > 0, "mod sweep reaches a count that fills systeminfo" );
	printf( "mymod: every key fits up to %i pk3s; from %i, left out: %s\n", modDrop - 1, modDrop, dropped );

	printf( "Cvar systeminfo regression passed\n" );
	return 0;
}
