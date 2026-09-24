/* Issue #303: pak lists that fill CS_SYSTEMINFO must not push sv_serverid and the other short keys out,
   names never go without their checksums, and referenced checksums never go without their names. */
#include "q_shared.h"
#include "qcommon.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern cvar_t *cvar_vars;	/* cvar.c */

cvar_t *sv_serverid;	/* assigned by SV_Init's systeminfo block */
cvar_t *sv_pure;

/* The lists SV_SpawnServer fills from the filesystem, as the checksum/name pairs clients read.
   Names without checksums leak on the client; referenced checksums without names crash a retail
   client (strict), while the loaded names are never read, so sv_paks may go alone. */
static const struct {
	const char *sums, *names;
	qboolean strict;
} pakListPairs[] = {
	{ "sv_paks", "sv_pakNames", qfalse },
	{ "sv_referencedPaks", "sv_referencedPakNames", qtrue }
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

void Com_Memset( void *dest, const int val, const size_t count ) {
	memset( dest, val, count );
}

/* True if a WARNING line of the last build names key as a whole word. */
static qboolean Warned( const char *key ) {
	const char *line, *end, *p;
	size_t keyLength = strlen( key );

	for ( line = printed ; *line ; line = *end ? end + 1 : end ) {
		end = strchr( line, '\n' );
		if ( !end ) {
			end = line + strlen( line );
		}
		if ( strncmp( line, "WARNING: ", 9 ) ) {
			continue;
		}
		for ( p = strstr( line, key ) ; p && p < end ; p = strstr( p + keyLength, key ) ) {
			if ( p[-1] == ' ' && strchr( " ,\n", p[keyLength] ) ) {
				return qtrue;
			}
		}
	}
	return qfalse;
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

static qboolean Sent( const char *info, const char *key ) {
	return Info_ValueForKey( info, key )[0] != 0;
}

/* True if info carries every short key that has a value. */
static qboolean ShortKeysSent( const char *info ) {
	int i;

	for ( i = 0 ; i < ARRAY_LEN( shortKeys ) ; i++ ) {
		if ( Cvar_VariableString( shortKeys[i] )[0] && !Sent( info, shortKeys[i] ) ) {
			return qfalse;
		}
	}
	return qtrue;
}

/* 202cda0: short values first, then each long value on its own, for the pure-mode check. */
static const char *UnpairedTwoPassInfo( void ) {
	static char info[BIG_INFO_STRING];
	cvar_t *var;
	int pass;

	info[0] = 0;
	for ( pass = 0 ; pass < 2 ; pass++ ) {
		for ( var = cvar_vars ; var ; var = var->next ) {
			if ( ( var->flags & CVAR_SYSTEMINFO ) && ( strlen( var->string ) >= MAX_INFO_VALUE ) == pass ) {
				Info_SetValueForKey_Big( info, var->name, var->string );
			}
		}
	}
	return info;
}

/* The old single newest-first pass (master and retail 1.32c), for the unchanged-output and pure-mode checks. */
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
	const char *built, *keys[2], *value;
	int i, j, infoLength, length[2], firstDrop = -1;
	qboolean allShort, roomGiven, in[2], out[2];

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
		for ( i = 0 ; i < ARRAY_LEN( pakListPairs ) ; i++ ) {
			keys[0] = pakListPairs[i].sums;
			keys[1] = pakListPairs[i].names;
			roomGiven = qfalse;
			for ( j = 0 ; j < 2 ; j++ ) {
				value = Cvar_VariableString( keys[j] );
				if ( strlen( value ) >= MAX_INFO_VALUE ) {
					allShort = qfalse;
				}
				Q_strncpyz( sent, Info_ValueForKey( info, keys[j] ), sizeof( sent ) );
				length[j] = value[0] ? (int)( strlen( keys[j] ) + strlen( value ) + 2 ) : 0;
				in[j] = sent[0] != 0;
				out[j] = value[0] && !sent[0];
				if ( !out[j] ) {
					Com_sprintf( message, sizeof( message ), "%s is sent whole and not reported", keys[j] );
					Check( !strcmp( sent, value ) && !Warned( keys[j] ), message );
					continue;
				}
				Com_sprintf( message, sizeof( message ), "left-out %s is reported", keys[j] );
				Check( Warned( keys[j] ), message );
				Com_sprintf( warning, sizeof( warning ), "WARNING: no room for %s (%i chars)", keys[j], (int)strlen( value ) );
				roomGiven |= strstr( printed, warning ) != NULL;
				if ( firstDrop < 0 ) {
					firstDrop = numPaks;
				}
				if ( firstDrop == numPaks ) {
					Q_strcat( dropped, droppedSize, dropped[0] ? va( " %s", keys[j] ) : keys[j] );
				}
			}

			/* names never go without their checksums; referenced checksums never without their names */
			Com_sprintf( message, sizeof( message ), "%s is never sent without %s", keys[1], keys[0] );
			Check( !in[1] || in[0], message );
			if ( pakListPairs[i].strict ) {
				Com_sprintf( message, sizeof( message ), "%s is never sent without %s", keys[0], keys[1] );
				Check( !in[0] || in[1], message );
			}

			/* and a list is only left out when it can't fit next to everything else */
			if ( out[0] ) {
				Com_sprintf( message, sizeof( message ), "%s is left out only when it cannot fit", keys[0] );
				Check( infoLength + length[0] + ( pakListPairs[i].strict ? length[1] : 0 ) >= BIG_INFO_STRING
					&& roomGiven, message );
			} else if ( out[1] ) {
				Com_sprintf( message, sizeof( message ), "%s is left out only when it cannot fit", keys[1] );
				Check( infoLength + length[1] >= BIG_INFO_STRING && roomGiven, message );
			}
		}

		/* full pure mode stays wherever master or 202cda0 kept sv_paks next to every short key */
		if ( ( Sent( SinglePassInfo(), "sv_paks" ) && ShortKeysSent( SinglePassInfo() ) )
			|| ( Sent( UnpairedTwoPassInfo(), "sv_paks" ) && ShortKeysSent( UnpairedTwoPassInfo() ) ) ) {
			Check( Sent( info, "sv_paks" ), "sv_paks is kept wherever master or 202cda0 kept it" );
		}

		/* while every value would fit a normal info string the output is the old one, byte for byte */
		if ( allShort ) {
			Check( !strcmp( info, SinglePassInfo() ), "short-list systeminfo keeps the retail key order" );
		}
	}
	return firstDrop;
}

/* Build systeminfo for three pk3s with one list rejected for a ';'. */
static const char *BuildWithRejected( const char *key, const char *value ) {
	static char info[BIG_INFO_STRING];

	SetPakLists( BASEGAME, numPaks );
	Cvar_Set( key, value );
	printedLength = 0;
	printed[0] = 0;
	Q_strncpyz( info, Cvar_InfoString_Big( CVAR_SYSTEMINFO ), sizeof( info ) );
	Check( Sent( info, "sv_serverid" ), "sv_serverid is still sent" );
	Check( !strstr( printed, "no room for" ), "a rejected value is not reported as too big" );
	Check( strstr( printed, va( "WARNING: %s has a \\, \" or ; in it", key ) ) != NULL, "the rejection reason is given" );
	return info;
}

/* A value the info string rejects is reported for that reason, and the pairing rules still hold. */
static void TestRejectedValue( void ) {
	const char *info;

	scenario = "rejected value";
	numPaks = 3;
	Cvar_Set( "fs_game", "" );

	info = BuildWithRejected( "sv_pakNames", "mappack-000 map;pack-001 mappack-002" );
	Check( !Sent( info, "sv_pakNames" ) && Sent( info, "sv_paks" ), "sv_paks stays when only its names are rejected" );
	Check( Sent( info, "sv_referencedPaks" ) && Sent( info, "sv_referencedPakNames" ), "the referenced pair is still sent" );
	Check( !Warned( "sv_paks" ), "sv_paks is not reported" );

	info = BuildWithRejected( "sv_paks", "12 3;4 56" );
	Check( !Sent( info, "sv_paks" ) && !Sent( info, "sv_pakNames" ), "rejected checksums take their names out" );
	Check( Warned( "sv_pakNames" ), "the names left out with them are reported" );

	info = BuildWithRejected( "sv_referencedPakNames", "baseq3/mappack-000 baseq3/map;pack-001" );
	Check( !Sent( info, "sv_referencedPakNames" ) && !Sent( info, "sv_referencedPaks" ),
		"rejected referenced names take their checksums out" );
	Check( Warned( "sv_referencedPaks" ), "the referenced checksums left out with them are reported" );
	Check( Sent( info, "sv_paks" ) && Sent( info, "sv_pakNames" ), "the loaded pair is still sent" );
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

	TestRejectedValue();

	printf( "Cvar systeminfo regression passed\n" );
	return 0;
}
