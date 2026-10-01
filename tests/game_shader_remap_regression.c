/*
 * Issue #448: AddRemap (code/game/g_utils.c) strcpy'd both shader names of a
 * remap into the 64-byte (MAX_QPATH) oldShader and newShader of its static
 * remappedShaders table, and its callers pass names that need not fit:
 *
 * - G_UseTargets passes a map entity's "targetShaderName" and
 *   "targetShaderNewName" keys, in the base game and Team Arena. They are
 *   BSP entity strings, so a downloaded map sets them, up to the 1022
 *   characters of an entity token.
 * - Team Arena's G_RemapTeamShaders (g_main.c) passes
 *   "team_icon/<g_redteam>_red" and "team_icon/<g_blueteam>_blue", which
 *   overflow for a red team name of 50 or more characters and a blue one of
 *   49 or more (a cvar reaches the game with up to 255).
 *
 * BuildShaderStateConfig then formatted each entry into a 133-byte buffer,
 * which two 63-character names and the time do not fit, so such an entry of
 * CS_SHADERSTATE lost its time and "@" terminator.
 *
 * This test links the real g_main.c, g_utils.c, g_spawn.c and g_mem.c. It
 * registers and changes the team name cvars through the real G_RegisterCvars
 * and G_UpdateCvars, calls G_RemapTeamShaders with team names of 48, 49, 50,
 * 63 and 255 characters, and fires remaps from map entity strings, parsed by
 * the real G_ParseSpawnVars and G_ParseField, through G_UseTargets, with
 * names of 1 to 1000 characters. The long names go into the last slots of the
 * table, so master's strcpy runs off its end under AddressSanitizer. Every
 * case checks the exact CS_SHADERSTATE string the game sets: a name too long
 * for a shader name is skipped with a warning, and names that fit give the
 * string master gives, byte for byte.
 */
#include "../code/game/g_local.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* g_utils.c keeps its table and MAX_SHADER_REMAPS to itself. */
#define TEST_MAX_SHADER_REMAPS	128
extern int	remapCount;

/* g_main.c does not export its client array through g_local.h. */
extern gclient_t	g_clients[MAX_CLIENTS];

/* g_main.c and g_spawn.c do not export these through g_local.h. */
void G_RegisterCvars( void );
void G_UpdateCvars( void );
void G_RemapTeamShaders( void );
qboolean G_ParseSpawnVars( void );
void G_ParseField( const char *key, const char *value, gentity_t *ent );

static const char	*testCase = "setup";

static void Fail( const char *message ) {
	fprintf( stderr, "shader remap regression failed: %s: %s\n", testCase, message );
	exit( 1 );
}

/* --- engine traps --- */
static int	remapWarnings;

void trap_Printf( const char *text ) {
	if ( strncmp( text, "AddRemap: ", 10 ) ) {
		fprintf( stderr, "%s", text );
		Fail( "unexpected print" );
	}
	remapWarnings++;
}
void trap_Error( const char *text ) {
	fprintf( stderr, "%s\n", text );
	Fail( "unexpected error" );
}

/* The server keeps a configstring of any length; BuildShaderStateConfig's is at most 4095. */
static char	shaderState[MAX_STRING_CHARS * 4];

void trap_SetConfigstring( int num, const char *string ) {
	if ( num != CS_SHADERSTATE ) {
		Fail( "unexpected configstring" );
	}
	if ( strlen( string ) >= sizeof( shaderState ) ) {
		Fail( "CS_SHADERSTATE is longer than BuildShaderStateConfig's buffer" );
	}
	strcpy( shaderState, string );
}

/* A cvar store that behaves as the engine's Cvar_Register, Cvar_Update and Cvar_Set. */
typedef struct {
	char	name[MAX_QPATH];
	char	string[MAX_CVAR_VALUE_STRING];
	int		modificationCount;
} testCvar_t;

static testCvar_t	cvars[128];
static int			numCvars;

static testCvar_t *FindCvar( const char *name ) {
	int i;

	for ( i = 0 ; i < numCvars ; i++ ) {
		if ( !Q_stricmp( cvars[i].name, name ) ) {
			return &cvars[i];
		}
	}
	return NULL;
}
void trap_Cvar_Set( const char *name, const char *value ) {
	testCvar_t *cv = FindCvar( name );

	if ( !cv ) {
		Fail( "trap_Cvar_Set of an unregistered cvar" );
	}
	// Cvar_Update drops the game with ERR_DROP for 256 characters or more
	if ( strlen( value ) >= sizeof( cv->string ) ) {
		Fail( "cvar value too long for a vmCvar_t" );
	}
	strcpy( cv->string, value );
	cv->modificationCount++;
}
void trap_Cvar_Update( vmCvar_t *vmCvar ) {
	testCvar_t *cv;

	if ( vmCvar->handle < 0 || vmCvar->handle >= numCvars ) {
		Fail( "trap_Cvar_Update of a bad handle" );
	}
	cv = &cvars[vmCvar->handle];
	if ( cv->modificationCount == vmCvar->modificationCount ) {
		return;
	}
	vmCvar->modificationCount = cv->modificationCount;
	Q_strncpyz( vmCvar->string, cv->string, sizeof( vmCvar->string ) );
	vmCvar->value = atof( cv->string );
	vmCvar->integer = atoi( cv->string );
}
void trap_Cvar_Register( vmCvar_t *vmCvar, const char *name, const char *value, int flags ) {
	testCvar_t *cv = FindCvar( name );

	(void)flags;
	if ( !cv ) {
		if ( numCvars == (int)( sizeof( cvars ) / sizeof( cvars[0] ) ) ) {
			Fail( "too many cvars" );
		}
		cv = &cvars[numCvars++];
		Q_strncpyz( cv->name, name, sizeof( cv->name ) );
		Q_strncpyz( cv->string, value, sizeof( cv->string ) );
		cv->modificationCount = 1;
	}
	if ( vmCvar ) {
		vmCvar->handle = cv - cvars;
		vmCvar->modificationCount = -1;
		trap_Cvar_Update( vmCvar );
	}
}
/* G_UpdateCvars announces a change of g_redteam and g_blueteam. */
void trap_SendServerCommand( int clientNum, const char *text ) {
	if ( clientNum != -1 || strncmp( text, "print \"Server: g_", 17 ) ) {
		Fail( "unexpected server command" );
	}
}

static const char	*entityParsePoint;

/** As the server's G_GET_ENTITY_TOKEN: the next COM_Parse token, false at the end of the string. */
qboolean trap_GetEntityToken( char *buffer, int bufferSize ) {
	const char *s = COM_Parse( (char **)&entityParsePoint );

	Q_strncpyz( buffer, s, bufferSize );
	return entityParsePoint || s[0];
}
void trap_LocateGameData( gentity_t *gEnts, int numGEntities, int sizeofGEntity_t, playerState_t *clients, int sizeofGClient ) {
	(void)gEnts; (void)numGEntities; (void)sizeofGEntity_t; (void)clients; (void)sizeofGClient;
}
void trap_UnlinkEntity( gentity_t *ent ) { ent->r.linked = qfalse; }

/* --- helpers --- */

/** A name of the given length, from a repeating pattern that starts with prefix. */
static const char *MakeName( char *buffer, int length, const char *prefix ) {
	static const char	pattern[] = "abcdefghijklmnopqrstuvwxyz0123456789";
	int					i, prefixLength = strlen( prefix );

	for ( i = 0 ; i < length ; i++ ) {
		buffer[i] = i < prefixLength ? prefix[i] : pattern[i % ( sizeof( pattern ) - 1 )];
	}
	buffer[length] = 0;
	return buffer;
}

/** An empty remap table and configstring, as for a new level. */
static void ResetRemaps( void ) {
	remapCount = 0;
	shaderState[0] = 0;
	remapWarnings = 0;
	level.time = 0;
}

/** The CS_SHADERSTATE entry of one remap, as BuildShaderStateConfig writes it. */
static void AppendEntry( char *expected, int size, const char *oldShader, const char *newShader, const char *timeText ) {
	Q_strcat( expected, size, oldShader );
	Q_strcat( expected, size, "=" );
	Q_strcat( expected, size, newShader );
	Q_strcat( expected, size, ":" );
	Q_strcat( expected, size, timeText );
	Q_strcat( expected, size, "@" );
}

/** Fill the first count slots of the table with "sNNN" -> "nNNN" remaps; each CS_SHADERSTATE entry is 16 characters. */
static void FillTable( char *expected, int size, int count ) {
	char	oldShader[8], newShader[8];
	int		i;

	expected[0] = 0;
	for ( i = 0 ; i < count ; i++ ) {
		Com_sprintf( oldShader, sizeof( oldShader ), "s%03d", i );
		Com_sprintf( newShader, sizeof( newShader ), "n%03d", i );
		AddRemap( oldShader, newShader, level.time * 0.001 );
		AppendEntry( expected, size, oldShader, newShader, " 0.00" );
	}
	if ( remapCount != count ) {
		Fail( "the table did not fill" );
	}
}

static void ExpectShaderState( const char *expected, int count, int warnings ) {
	if ( strcmp( shaderState, expected ) ) {
		fprintf( stderr, "CS_SHADERSTATE \"%s\"\nexpected       \"%s\"\n", shaderState, expected );
		Fail( "CS_SHADERSTATE changed" );
	}
	if ( remapCount != count ) {
		fprintf( stderr, "remapCount %d, expected %d\n", remapCount, count );
		Fail( "the remap count changed" );
	}
	if ( remapWarnings != warnings ) {
		fprintf( stderr, "%d warnings, expected %d\n", remapWarnings, warnings );
		Fail( "the skipped remaps were not reported" );
	}
}

/**
 * Spawn an entity from a map entity string with targetShaderName and
 * targetShaderNewName keys, as G_SpawnEntitiesFromString and
 * G_SpawnGEntityFromSpawnVars do, and fire its targets as a trigger does.
 */
static void FireEntityRemap( const char *oldShader, const char *newShader ) {
	char		entityString[4096];
	gentity_t	*ent;
	int			i;

	Com_sprintf( entityString, sizeof( entityString ),
		"{\n\"classname\" \"trigger_multiple\"\n\"model\" \"*1\"\n\"targetShaderName\" \"%s\"\n\"targetShaderNewName\" \"%s\"\n}\n",
		oldShader, newShader );

	memset( g_entities, 0, sizeof( g_entities ) );
	G_InitMemory();
	level.gentities = g_entities;
	level.clients = g_clients;
	level.num_entities = MAX_CLIENTS;

	entityParsePoint = entityString;
	if ( !G_ParseSpawnVars() ) {
		Fail( "G_ParseSpawnVars found no entity" );
	}
	ent = G_Spawn();
	for ( i = 0 ; i < level.numSpawnVars ; i++ ) {
		G_ParseField( level.spawnVars[i][0], level.spawnVars[i][1], ent );
	}
	if ( !ent->targetShaderName || strcmp( ent->targetShaderName, oldShader )
		|| !ent->targetShaderNewName || strcmp( ent->targetShaderNewName, newShader ) ) {
		Fail( "the shader keys did not parse" );
	}
	G_UseTargets( ent, ent );
	G_FreeEntity( ent );
}

/* --- map entity remaps: the base game and Team Arena --- */

typedef struct {
	int		oldLength;
	int		newLength;
} entityCase_t;

static const entityCase_t entityCases[] = {
	{ 1, 1 },
	{ 23, 31 },
	{ 48, 48 },
	{ 49, 50 },
	{ 63, 63 },		// the longest that fit: master's entry lost its time and "@"
	{ 63, 64 },
	{ 64, 63 },
	{ 10, 64 },
	{ 64, 10 },
	{ 10, 255 },
	{ 255, 10 },
	{ 255, 255 },
	{ 1000, 1000 },
};
#define NUM_ENTITY_CASES	( sizeof( entityCases ) / sizeof( entityCases[0] ) )

static void TestEntityRemaps( void ) {
	static char	expected[MAX_STRING_CHARS * 4], updated[MAX_STRING_CHARS * 4];
	char		name[128];
	char		oldShader[1024], newShader[1024], longShader[1024];
	size_t		c;
	int			fill, count, warnings;
	qboolean	oldFits, newFits;

	for ( c = 0 ; c < NUM_ENTITY_CASES ; c++ ) {
		MakeName( oldShader, entityCases[c].oldLength, "textures/" );
		MakeName( newShader, entityCases[c].newLength, "textures/new/" );
		oldFits = entityCases[c].oldLength < MAX_QPATH;
		newFits = entityCases[c].newLength < MAX_QPATH;

		// the remap takes the last slot of the table, or the one before it
		for ( fill = TEST_MAX_SHADER_REMAPS - 1 ; fill >= TEST_MAX_SHADER_REMAPS - 2 ; fill-- ) {
			snprintf( name, sizeof( name ), "entity remap of %d to %d characters after %d remaps",
				entityCases[c].oldLength, entityCases[c].newLength, fill );
			testCase = name;
			ResetRemaps();
			FillTable( expected, sizeof( expected ), fill );
			count = fill;
			warnings = 0;

			FireEntityRemap( oldShader, newShader );
			if ( oldFits && newFits ) {
				AppendEntry( expected, sizeof( expected ), oldShader, newShader, " 0.00" );
				count++;
			} else {
				warnings++;
			}
			ExpectShaderState( expected, count, warnings );

			// a later remap of a shader in the table replaces its new name in place
			level.time = 12500;
			FireEntityRemap( "s000", newShader );
			if ( newFits ) {
				updated[0] = 0;
				AppendEntry( updated, sizeof( updated ), "s000", newShader, "12.50" );
				Q_strcat( updated, sizeof( updated ), expected + strlen( "s000=n000: 0.00@" ) );
				Q_strncpyz( expected, updated, sizeof( expected ) );
			} else {
				warnings++;
			}
			ExpectShaderState( expected, count, warnings );

			// and so does a remap of the shader that ends the table, where master's
			// strcpy of a 255-character name runs off the end of remappedShaders
			if ( oldFits && newFits ) {
				level.time = 12750;
				FireEntityRemap( oldShader, MakeName( longShader, 255, "textures/new/" ) );
				ExpectShaderState( expected, count, warnings + 1 );
				FireEntityRemap( oldShader, "x" );
				expected[strlen( expected ) - strlen( newShader ) - strlen( ": 0.00@" )] = 0;
				Q_strcat( expected, sizeof( expected ), "x:12.75@" );
				ExpectShaderState( expected, count, warnings + 1 );
			}
		}
	}
}

/** The table holds MAX_SHADER_REMAPS shaders; later ones are dropped, as on master. */
static void TestRemapCount( void ) {
	static char	expected[MAX_STRING_CHARS * 4];

	testCase = "remap count";
	ResetRemaps();
	FillTable( expected, sizeof( expected ), TEST_MAX_SHADER_REMAPS );
	FireEntityRemap( "textures/one/more", "textures/one/new" );
	FireEntityRemap( "textures/two/more", "textures/two/new" );
	if ( strlen( shaderState ) != TEST_MAX_SHADER_REMAPS * 16 ) {
		Fail( "the full table did not give MAX_SHADER_REMAPS entries" );
	}
	ExpectShaderState( expected, TEST_MAX_SHADER_REMAPS, 0 );
}

#ifdef MISSIONPACK
/* --- Team Arena's team icon remaps --- */

/** The four team icon entries G_RemapTeamShaders sets for these names, or none for a name that does not fit. */
static void AppendTeamEntries( char *expected, int size, const char *redTeam, const char *blueTeam, const char *timeText ) {
	char	shader[MAX_CVAR_VALUE_STRING + 32];

	Com_sprintf( shader, sizeof( shader ), "team_icon/%s_red", redTeam );
	if ( strlen( shader ) < MAX_QPATH ) {
		AppendEntry( expected, size, "textures/ctf2/redteam01", shader, timeText );
		AppendEntry( expected, size, "textures/ctf2/redteam02", shader, timeText );
	}
	Com_sprintf( shader, sizeof( shader ), "team_icon/%s_blue", blueTeam );
	if ( strlen( shader ) < MAX_QPATH ) {
		AppendEntry( expected, size, "textures/ctf2/blueteam01", shader, timeText );
		AppendEntry( expected, size, "textures/ctf2/blueteam02", shader, timeText );
	}
}

static const int teamNameLengths[] = { 1, 6, 48, 49, 50, 63, 255 };
#define NUM_TEAM_NAME_LENGTHS	( sizeof( teamNameLengths ) / sizeof( teamNameLengths[0] ) )

static void TestTeamShaders( void ) {
	static const char	retail[] =
		"textures/ctf2/redteam01=team_icon/Stroggs_red: 0.00@"
		"textures/ctf2/redteam02=team_icon/Stroggs_red: 0.00@"
		"textures/ctf2/blueteam01=team_icon/Pagans_blue: 0.00@"
		"textures/ctf2/blueteam02=team_icon/Pagans_blue: 0.00@";
	static char	expected[MAX_STRING_CHARS * 4];
	char		name[128];
	char		redTeam[MAX_CVAR_VALUE_STRING], blueTeam[MAX_CVAR_VALUE_STRING];
	size_t		r, b;
	int			fill, count, warnings;

	// the default names, registered as G_RegisterCvars does when the game loads
	testCase = "default team names";
	ResetRemaps();
	G_RegisterCvars();
	ExpectShaderState( retail, 4, 0 );

	// a server operator's change, through G_UpdateCvars
	testCase = "g_redteam changed to 255 characters";
	level.time = 61500;
	trap_Cvar_Set( "g_redteam", MakeName( redTeam, 255, "Red" ) );
	G_UpdateCvars();
	// the red icons keep the remap they had
	expected[0] = 0;
	AppendEntry( expected, sizeof( expected ), "textures/ctf2/redteam01", "team_icon/Stroggs_red", " 0.00" );
	AppendEntry( expected, sizeof( expected ), "textures/ctf2/redteam02", "team_icon/Stroggs_red", " 0.00" );
	AppendEntry( expected, sizeof( expected ), "textures/ctf2/blueteam01", "team_icon/Pagans_blue", "61.50" );
	AppendEntry( expected, sizeof( expected ), "textures/ctf2/blueteam02", "team_icon/Pagans_blue", "61.50" );
	ExpectShaderState( expected, 4, 2 );

	testCase = "g_redteam changed back to a short name";
	level.time = 62000;
	trap_Cvar_Set( "g_redteam", "Intruders" );
	G_UpdateCvars();
	expected[0] = 0;
	AppendTeamEntries( expected, sizeof( expected ), "Intruders", "Pagans", "62.00" );
	ExpectShaderState( expected, 4, 2 );

	// every pair of name lengths, into a new table and into the last slots of a full one
	for ( r = 0 ; r < NUM_TEAM_NAME_LENGTHS ; r++ ) {
		for ( b = 0 ; b < NUM_TEAM_NAME_LENGTHS ; b++ ) {
			for ( fill = 0 ; fill <= TEST_MAX_SHADER_REMAPS - 4 ; fill += TEST_MAX_SHADER_REMAPS - 4 ) {
				snprintf( name, sizeof( name ), "team names of %d and %d characters after %d remaps",
					teamNameLengths[r], teamNameLengths[b], fill );
				testCase = name;
				ResetRemaps();
				FillTable( expected, sizeof( expected ), fill );
				Q_strncpyz( g_redteam.string, MakeName( redTeam, teamNameLengths[r], "Red" ), sizeof( g_redteam.string ) );
				Q_strncpyz( g_blueteam.string, MakeName( blueTeam, teamNameLengths[b], "Blue" ), sizeof( g_blueteam.string ) );
				G_RemapTeamShaders();

				AppendTeamEntries( expected, sizeof( expected ), redTeam, blueTeam, " 0.00" );
				count = fill;
				warnings = 0;
				// "team_icon/" + name + "_red" fits in 63 characters up to a 49-character name, "_blue" up to 48
				if ( teamNameLengths[r] <= 49 ) {
					count += 2;
				} else {
					warnings += 2;
				}
				if ( teamNameLengths[b] <= 48 ) {
					count += 2;
				} else {
					warnings += 2;
				}
				ExpectShaderState( expected, count, warnings );
			}
		}
	}
}
#endif

int main( void ) {
#ifdef MISSIONPACK
	TestTeamShaders();
#endif
	TestEntityRemaps();
	TestRemapCount();
#ifdef MISSIONPACK
	printf( "AddRemap keeps map entity and team icon shader names in its table, or skips the remap (issue #448, Team Arena)\n" );
#else
	printf( "AddRemap keeps map entity shader names in its table, or skips the remap (issue #448)\n" );
#endif
	return 0;
}
