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
 * Each entry is "old=new:time@", and retail 1.32c's CG_ShaderStateChanged
 * copies up to the first '=', then up to the next ':', then up to the next
 * '@' into 64-, 64- and 16-byte stack buffers with no bound. Names that fit
 * still overflowed those when the old name held '=' (the new name then
 * starts inside it) or the new name held ':' (the time then starts inside
 * it), and map entity keys can hold either. The string also travels to the
 * client as the quoted argument of a "cs"/"bcs" server command, which the
 * client rebuilds with Cmd_TokenizeString and Cmd_ArgsFrom(2): a '"' in a
 * name ends that quoted argument, and the unquoted remainder has C-style
 * comments stripped and tokens joined, so a name with '"' lets a map drop or
 * glue parts of other entries into one over-long field.
 *
 * This test links the real g_main.c, g_utils.c, g_spawn.c and g_mem.c. It
 * registers and changes the team name cvars through the real G_RegisterCvars
 * and G_UpdateCvars, calls G_RemapTeamShaders with team names of 48, 49, 50,
 * 63 and 255 characters and with separators, and fires remaps from map
 * entity strings, parsed by the real G_ParseSpawnVars and G_ParseField,
 * through G_UseTargets, with names of 1 to 1000 characters and with
 * separators. The long names go into the last slots of the table, so
 * master's strcpy runs off its end under AddressSanitizer. Every case checks
 * the exact CS_SHADERSTATE string the game sets: a name too long for a shader
 * name, an old name with '=', '@' or '"' and a new name with ':', '@' or '"'
 * are skipped with a warning, and other names give the string master gives,
 * byte for byte wherever master's entry was not cut short (the two names and
 * the time in at most 132 characters). Every CS_SHADERSTATE string the game
 * sets also goes through copies of retail's "cs"/"bcs" transport
 * (Cmd_TokenizeString, Cmd_ArgsFrom) and of its CG_ShaderStateChanged parser
 * under AddressSanitizer, which must deliver and read each expected entry
 * exactly as it was sent. TestQuoteGluing is the reviewer's worst case.
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

/*
 * Retail 1.32c's CG_ShaderStateChanged (git show dbe4ddb:code/cgame/cg_servercmds.c),
 * with its unbounded strncpys into stack buffers, so that AddressSanitizer
 * reports a CS_SHADERSTATE string that would overflow a retail client. It
 * returns the number of remaps the client would make, and the last one.
 */
static int RetailShaderStateChanged( const char *configstring, char *lastOld, char *lastNew, char *lastTime ) {
	char originalShader[MAX_QPATH];
	char newShader[MAX_QPATH];
	char timeOffset[16];
	const char *o;
	char *n,*t;
	int remaps = 0;

	o = configstring;
	while (o && *o) {
		n = strstr(o, "=");
		if (n && *n) {
			strncpy(originalShader, o, n-o);
			originalShader[n-o] = 0;
			n++;
			t = strstr(n, ":");
			if (t && *t) {
				strncpy(newShader, n, t-n);
				newShader[t-n] = 0;
			} else {
				break;
			}
			t++;
			o = strstr(t, "@");
			if (o) {
				strncpy(timeOffset, t, o-t);
				timeOffset[o-t] = 0;
				o++;
				// trap_R_RemapShader( originalShader, newShader, timeOffset );
				strcpy( lastOld, originalShader );
				strcpy( lastNew, newShader );
				strcpy( lastTime, timeOffset );
				remaps++;
			}
		} else {
			break;
		}
	}
	return remaps;
}

/*
 * Retail 1.32c's Cmd_TokenizeString and Cmd_ArgsFrom (git show
 * dbe4ddb:code/qcommon/cmd.c), which a retail client runs on each "cs" and
 * "bcs0/1/2" server command.
 */
static int		cmd_argc;
static char		*cmd_argv[MAX_STRING_TOKENS];		// points into cmd_tokenized
static char		cmd_tokenized[BIG_INFO_STRING+MAX_STRING_TOKENS];	// will have 0 bytes inserted

static char *Cmd_Argv( int arg ) {
	if ( (unsigned)arg >= cmd_argc ) {
		return "";
	}
	return cmd_argv[arg];
}

static char *Cmd_ArgsFrom( int arg ) {
	static	char		cmd_args[BIG_INFO_STRING];
	int		i;

	cmd_args[0] = 0;
	if (arg < 0)
		arg = 0;
	for ( i = arg ; i < cmd_argc ; i++ ) {
		strcat( cmd_args, cmd_argv[i] );
		if ( i != cmd_argc-1 ) {
			strcat( cmd_args, " " );
		}
	}

	return cmd_args;
}

static void Cmd_TokenizeString( const char *text_in ) {
	const char	*text;
	char	*textOut;

	// clear previous args
	cmd_argc = 0;

	if ( !text_in ) {
		return;
	}

	text = text_in;
	textOut = cmd_tokenized;

	while ( 1 ) {
		if ( cmd_argc == MAX_STRING_TOKENS ) {
			return;			// this is usually something malicious
		}

		while ( 1 ) {
			// skip whitespace
			while ( *text && *text <= ' ' ) {
				text++;
			}
			if ( !*text ) {
				return;			// all tokens parsed
			}

			// skip // comments
			if ( text[0] == '/' && text[1] == '/' ) {
				return;			// all tokens parsed
			}

			// skip /* */ comments
			if ( text[0] == '/' && text[1] =='*' ) {
				while ( *text && ( text[0] != '*' || text[1] != '/' ) ) {
					text++;
				}
				if ( !*text ) {
					return;		// all tokens parsed
				}
				text += 2;
			} else {
				break;			// we are ready to parse a token
			}
		}

		// handle quoted strings
		if ( *text == '"' ) {
			cmd_argv[cmd_argc] = textOut;
			cmd_argc++;
			text++;
			while ( *text && *text != '"' ) {
				*textOut++ = *text++;
			}
			*textOut++ = 0;
			if ( !*text ) {
				return;		// all tokens parsed
			}
			text++;
			continue;
		}

		// regular token
		cmd_argv[cmd_argc] = textOut;
		cmd_argc++;

		// skip until whitespace, quote, or command
		while ( *text > ' ' ) {
			if ( text[0] == '"' ) {
				break;
			}

			if ( text[0] == '/' && text[1] == '/' ) {
				break;
			}

			// skip /* */ comments
			if ( text[0] == '/' && text[1] =='*' ) {
				break;
			}

			*textOut++ = *text++;
		}

		*textOut++ = 0;

		if ( !*text ) {
			return;		// all tokens parsed
		}
	}
}

/**
 * What a retail client's CL_ConfigstringModified gets for a configstring
 * the server sends: SV_SetConfigstring sends "cs <index> "<string>"", or
 * "bcs0", "bcs1" and "bcs2" chunks of 999 characters from 1000 on, which
 * CL_GetServerCommand joins into "cs <index> "<string>"" again.
 */
static const char *RetailReceivedConfigstring( int index, const char *string ) {
	static char	bigConfigString[BIG_INFO_STRING];
	char		command[MAX_STRING_CHARS * 2], chunk[MAX_STRING_CHARS];
	const int	maxChunkSize = MAX_STRING_CHARS - 24;
	int			sent, remaining;
	const char	*cmd;

	remaining = strlen( string );
	if ( remaining < maxChunkSize ) {
		Com_sprintf( command, sizeof( command ), "cs %i \"%s\"\n", index, string );
		Cmd_TokenizeString( command );
	} else {
		for ( sent = 0 ; remaining > 0 ; sent += maxChunkSize - 1, remaining -= maxChunkSize - 1 ) {
			cmd = sent == 0 ? "bcs0" : remaining < maxChunkSize ? "bcs2" : "bcs1";
			Q_strncpyz( chunk, string + sent, maxChunkSize );
			Com_sprintf( command, sizeof( command ), "%s %i \"%s\"\n", cmd, index, chunk );
			Cmd_TokenizeString( command );
			if ( !strcmp( Cmd_Argv( 0 ), "bcs0" ) ) {
				Com_sprintf( bigConfigString, BIG_INFO_STRING, "cs %s \"%s", Cmd_Argv(1), Cmd_Argv(2) );
			} else {
				if ( strlen( bigConfigString ) + strlen( Cmd_Argv( 2 ) ) + 1 >= BIG_INFO_STRING ) {
					Fail( "bcs exceeded BIG_INFO_STRING" );
				}
				strcat( bigConfigString, Cmd_Argv( 2 ) );
			}
		}
		strcat( bigConfigString, "\"" );
		Cmd_TokenizeString( bigConfigString );
	}
	if ( strcmp( Cmd_Argv( 0 ), "cs" ) || atoi( Cmd_Argv( 1 ) ) != index ) {
		Fail( "the configstring command did not arrive" );
	}
	// get everything after "cs <num>"
	return Cmd_ArgsFrom( 2 );
}

/* The server keeps a configstring of any length; BuildShaderStateConfig's is at most 4095. */
static char	shaderState[MAX_STRING_CHARS * 4];

void trap_SetConfigstring( int num, const char *string ) {
	char		oldShader[MAX_QPATH], newShader[MAX_QPATH], timeText[16];
	const char	*received;

	if ( num != CS_SHADERSTATE ) {
		Fail( "unexpected configstring" );
	}
	if ( strlen( string ) >= sizeof( shaderState ) ) {
		Fail( "CS_SHADERSTATE is longer than BuildShaderStateConfig's buffer" );
	}
	strcpy( shaderState, string );
	// a retail client receives it through the "cs" command, then parses it;
	// no case here reaches BuildShaderStateConfig's cap, so it reads every remap
	received = RetailReceivedConfigstring( num, string );
	if ( RetailShaderStateChanged( received, oldShader, newShader, timeText ) != remapCount ) {
		Fail( "a retail client would read a different number of remaps" );
	}
	if ( strcmp( received, string ) ) {
		Fail( "a retail client would receive a different CS_SHADERSTATE" );
	}
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

/**
 * The CS_SHADERSTATE entry of one remap, as BuildShaderStateConfig writes it,
 * which a retail client must read back as this remap.
 */
static void AppendEntry( char *expected, int size, const char *oldShader, const char *newShader, const char *timeText ) {
	char	entry[MAX_QPATH * 4];
	char	readOld[MAX_QPATH], readNew[MAX_QPATH], readTime[16];

	Com_sprintf( entry, sizeof( entry ), "%s=%s:%s@", oldShader, newShader, timeText );
	if ( RetailShaderStateChanged( entry, readOld, readNew, readTime ) != 1
		|| strcmp( readOld, oldShader ) || strcmp( readNew, newShader ) || strcmp( readTime, timeText ) ) {
		fprintf( stderr, "entry \"%s\"\n", entry );
		Fail( "a retail client would misread a remap" );
	}
	Q_strcat( expected, size, entry );
}

/** AddRemap's rule: a name that fits a shader name and holds no separator that moves a retail client's split. */
static qboolean OldNameFits( const char *name ) {
	return strlen( name ) < MAX_QPATH && !strchr( name, '=' ) && !strchr( name, '@' ) && !strchr( name, '"' );
}
static qboolean NewNameFits( const char *name ) {
	return strlen( name ) < MAX_QPATH && !strchr( name, ':' ) && !strchr( name, '@' ) && !strchr( name, '"' );
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

	// a value with a '"' is written as a bare word, which COM_Parse reads up to whitespace
	Com_sprintf( entityString, sizeof( entityString ),
		"{\n\"classname\" \"trigger_multiple\"\n\"model\" \"*1\"\n\"targetShaderName\" %s%s%s\n\"targetShaderNewName\" %s%s%s\n}\n",
		strchr( oldShader, '"' ) ? "" : "\"", oldShader, strchr( oldShader, '"' ) ? "" : "\"",
		strchr( newShader, '"' ) ? "" : "\"", newShader, strchr( newShader, '"' ) ? "" : "\"" );

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
	int			oldLength;
	int			newLength;
	const char	*oldPrefix;		// NULL for "textures/"
	const char	*newPrefix;		// NULL for "textures/new/"
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
	// separators: retail reads the new name from the old one's '=', 125
	// characters into 64, and the time from the new name's ':', 67 into 16
	{ 63, 63, "a=" },
	{ 63, 63, NULL, "b:" },
	{ 1, 12, "=" },
	{ 12, 1, NULL, ":" },
	{ 12, 12, "textures/o=d", "textures/n:w" },
	{ 12, 12, "textures/o@d" },
	{ 12, 12, NULL, "textures/n@w" },
	{ 63, 63, "textures/o@d=", "textures/n@w:" },
	// the other name's separators move no split, so retail reads these as sent
	{ 12, 12, "textures/o:d", "textures/n=w" },
	{ 63, 63, "textures/o:d", "textures/n=w" },
	// an interior '"' ends the "cs" argument, so a retail tokenizer would drop
	// or glue parts of the string; TestQuoteGluing below is the worst case. A
	// map value starting with '"' is a quoted token to COM_Parse and carries
	// no payload, so the reachable case is a '"' inside a bare word.
	{ 12, 12, "textures/o\"d", "textures/n\"w" },
	{ 63, 63, "textures/o\"d", "textures/n\"w" },
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
		MakeName( oldShader, entityCases[c].oldLength, entityCases[c].oldPrefix ? entityCases[c].oldPrefix : "textures/" );
		MakeName( newShader, entityCases[c].newLength, entityCases[c].newPrefix ? entityCases[c].newPrefix : "textures/new/" );
		oldFits = OldNameFits( oldShader );
		newFits = NewNameFits( newShader );

		// the remap takes the last slot of the table, or the one before it
		for ( fill = TEST_MAX_SHADER_REMAPS - 1 ; fill >= TEST_MAX_SHADER_REMAPS - 2 ; fill-- ) {
			snprintf( name, sizeof( name ), "entity remap of %d characters from %s to %d from %s after %d remaps",
				entityCases[c].oldLength, entityCases[c].oldPrefix ? entityCases[c].oldPrefix : "textures/",
				entityCases[c].newLength, entityCases[c].newPrefix ? entityCases[c].newPrefix : "textures/new/", fill );
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
				FireEntityRemap( oldShader, "x:" );
				ExpectShaderState( expected, count, warnings + 2 );
				FireEntityRemap( oldShader, "x=" );
				expected[strlen( expected ) - strlen( newShader ) - strlen( ": 0.00@" )] = 0;
				Q_strcat( expected, sizeof( expected ), "x=:12.75@" );
				ExpectShaderState( expected, count, warnings + 2 );
			}
		}
	}
}

/*
 * The reviewer's worst case: two old names that fit MAX_QPATH and hold no
 * '=', ':' or '@', but whose tail and head are a quote and a C-comment
 * delimiter. The first old name ends with a quote then a comment-open, the
 * second starts with a comment-close then a quote, and each has a short new
 * name so its entry still carries an '='. The server string is, in effect,
 * firstOld"<open>=n: 0.00@<close>"secondOld=m: 0.00@. A retail client runs it
 * through Cmd_TokenizeString: the first quote ends the "cs" argument's quoted
 * token at firstOld, the comment pair removes =n: 0.00@ between them, and the
 * tail secondOld=m: 0.00@ is one bare token. Cmd_ArgsFrom(2) rejoins it as
 * firstOld + ' ' + secondOld=m: 0.00@, whose part before the surviving '=' is
 * 121 characters, which retail's CG_ShaderStateChanged copies into
 * originalShader[64]. AddRemap refuses both old names, so the server never
 * sends it; the test's trap_SetConfigstring runs the real transport and
 * parser and would report that overflow under AddressSanitizer without the
 * refusal, and otherwise sees only the empty CS_SHADERSTATE.
 */
static void TestQuoteGluing( void ) {
	char	firstOld[MAX_QPATH], secondOld[MAX_QPATH];
	int		i;

	testCase = "quote-and-comment gluing";
	ResetRemaps();
	// a...a"/* : 60 of 'a', then a quote and a comment-open
	for ( i = 0 ; i < 60 ; i++ ) {
		firstOld[i] = 'a';
	}
	strcpy( firstOld + 60, "\"/*" );
	// */"b...b : a comment-close and a quote, then 60 of 'b'
	strcpy( secondOld, "*/\"" );
	for ( i = 0 ; i < 60 ; i++ ) {
		secondOld[3 + i] = 'b';
	}
	secondOld[63] = 0;
	if ( strlen( firstOld ) != 63 || strlen( secondOld ) != 63 ) {
		Fail( "the worst-case names are not 63 characters" );
	}
	if ( OldNameFits( firstOld ) || OldNameFits( secondOld ) ) {
		Fail( "the worst-case names must be refused" );
	}

	// both AddRemap calls are refused on the old name, so the table and
	// CS_SHADERSTATE stay empty and one warning is printed for each
	FireEntityRemap( firstOld, "textures/n" );
	FireEntityRemap( secondOld, "textures/m" );
	if ( remapCount != 0 || shaderState[0] != 0 || remapWarnings != 2 ) {
		Fail( "the worst-case chain was not refused" );
	}

	// a short remap whose names each hold a lone '/' or '*' (no quote) is
	// accepted and still arrives and parses as sent (checked in trap_SetConfigstring)
	FireEntityRemap( "textures/a/b", "textures/c*d" );
	FireEntityRemap( "textures/e*f", "textures/g/h" );
	if ( remapCount != 2 ) {
		Fail( "a '/' or '*' alone must be accepted" );
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

/** Whether G_RemapTeamShaders's icon name for this team fits AddRemap. */
static qboolean TeamIconFits( const char *team, const char *suffix ) {
	char	shader[MAX_CVAR_VALUE_STRING + 32];

	Com_sprintf( shader, sizeof( shader ), "team_icon/%s%s", team, suffix );
	return NewNameFits( shader );
}

/** The four team icon entries G_RemapTeamShaders sets for these names, or none for a name that does not fit. */
static void AppendTeamEntries( char *expected, int size, const char *redTeam, const char *blueTeam, const char *timeText ) {
	char	shader[MAX_CVAR_VALUE_STRING + 32];

	Com_sprintf( shader, sizeof( shader ), "team_icon/%s_red", redTeam );
	if ( TeamIconFits( redTeam, "_red" ) ) {
		AppendEntry( expected, size, "textures/ctf2/redteam01", shader, timeText );
		AppendEntry( expected, size, "textures/ctf2/redteam02", shader, timeText );
	}
	Com_sprintf( shader, sizeof( shader ), "team_icon/%s_blue", blueTeam );
	if ( TeamIconFits( blueTeam, "_blue" ) ) {
		AppendEntry( expected, size, "textures/ctf2/blueteam01", shader, timeText );
		AppendEntry( expected, size, "textures/ctf2/blueteam02", shader, timeText );
	}
}

static const int teamNameLengths[] = { 1, 6, 48, 49, 50, 63, 255 };
#define NUM_TEAM_NAME_LENGTHS	( sizeof( teamNameLengths ) / sizeof( teamNameLengths[0] ) )

/* A ':' or '@' in the icon name is skipped; an '=' moves no split of a new name. */
static const char *separatorTeamNames[] = { "Team: Alpha", ":", "Team@Home", "@", "A=B", "=" };
#define NUM_SEPARATOR_TEAM_NAMES	( sizeof( separatorTeamNames ) / sizeof( separatorTeamNames[0] ) )

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

	// team names with separators, as red and as blue
	for ( r = 0 ; r < NUM_SEPARATOR_TEAM_NAMES ; r++ ) {
		for ( b = 0 ; b < 2 ; b++ ) {
			snprintf( name, sizeof( name ), "%s team name \"%s\"", b ? "blue" : "red", separatorTeamNames[r] );
			testCase = name;
			ResetRemaps();
			Q_strncpyz( g_redteam.string, b ? "Stroggs" : separatorTeamNames[r], sizeof( g_redteam.string ) );
			Q_strncpyz( g_blueteam.string, b ? separatorTeamNames[r] : "Pagans", sizeof( g_blueteam.string ) );
			G_RemapTeamShaders();

			expected[0] = 0;
			AppendTeamEntries( expected, sizeof( expected ), g_redteam.string, g_blueteam.string, " 0.00" );
			if ( TeamIconFits( separatorTeamNames[r], b ? "_blue" : "_red" ) ) {
				ExpectShaderState( expected, 4, 0 );
			} else {
				ExpectShaderState( expected, 2, 2 );
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
	TestQuoteGluing();
	TestRemapCount();
#ifdef MISSIONPACK
	printf( "AddRemap keeps map entity and team icon shader names in its table, or skips the remap (issue #448, Team Arena)\n" );
#else
	printf( "AddRemap keeps map entity shader names in its table, or skips the remap (issue #448)\n" );
#endif
	return 0;
}
