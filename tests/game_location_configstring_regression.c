/*
 * Issue #461: target_location_linkup (code/game/g_target.c) gives the n-th
 * target_location of a map, n from 1, configstring CS_LOCATIONS + n and
 * sets its health to n, the location index the team overlay gets in
 * "tinfo". CS_LOCATIONS is 608 and there are 1024 configstrings, so a map
 * with 416 or more target_locations asked for configstring 1024, and the
 * server's SV_SetConfigstring ended the map with ERR_DROP ("bad index").
 * Any map can hold that many, a downloaded one too.
 *
 * Retail 1.32c's cgame, and this repository's, read CS_LOCATIONS + location
 * through CG_ConfigString, which bounds the index to the configstring
 * array but not to MAX_LOCATIONS, and show "unknown" for an empty string. So
 * a map with 64 to 415 locations, whose names run on into CS_PARTICLES
 * (which nothing reads) and the unused configstrings after it, shows every
 * name today, and must keep doing so. The linkup now stops naming
 * locations at the last configstring: the rest keep their place in the
 * location list, so Team_GetLocation still finds the nearest location, but
 * get location 0, CS_LOCATIONS itself, which reads "unknown". A location
 * index past the array would make a retail client's CG_ConfigString end
 * the game with CG_Error, so the linkup also clears the "health" a map may
 * give such a location. With "developer" set it prints one warning.
 *
 * This test links the real g_spawn.c, g_target.c, g_team.c, g_utils.c,
 * g_mem.c and bg_misc.c, and loads maps from entity strings through the
 * real G_SpawnEntitiesFromString (G_ParseSpawnVars, G_SpawnGEntityFromSpawnVars,
 * G_CallSpawn with the real spawns[] table, SP_target_location), then runs
 * the target_locations' think 200 ms later, as G_RunFrame does. The maps
 * hold 63, 64, 100, 300, 415, 416, 600 and 958 (as many as G_Spawn allows)
 * target_locations, each with its own name (some empty), color and a
 * "health" key, with target_speakers between them. trap_SetConfigstring
 * ends the test where SV_SetConfigstring would drop the map, so master
 * fails on the 416-location map. Every map is checked against a copy of
 * master's loop (retail's, git show dbe4ddb:code/game/g_target.c) run on
 * the same entities: up to 415 locations every configstring, location
 * index and the location list are master's, byte for byte; from 416 on,
 * master drops the map, and every configstring it set before that is the
 * same here. Then a player stands at each location in turn: the real
 * CheckTeamStatus and TeamplayInfoMessage send "tinfo", a copy of retail's
 * CG_ParseTeamInfo and CG_ConfigString read it, and the team overlay must
 * show the location's name, "unknown" for the empty names and for the
 * locations past the last configstring, while Team_GetLocationMsg still
 * names every location for say_team. Built for base Quake III and Team
 * Arena (MISSIONPACK).
 */
#include "../code/game/g_local.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* the locations that fit the configstring array: CS_LOCATIONS + 1 to 1023 */
#define MAX_NAMED_LOCATIONS	( MAX_CONFIGSTRINGS - 1 - CS_LOCATIONS )
/* as many entities as G_Spawn gives a map */
#define MAX_MAP_ENTITIES	( ENTITYNUM_MAX_NORMAL - MAX_CLIENTS )

/* --- the game globals g_main.c would define --- */
level_locals_t	level;
gentity_t		g_entities[MAX_GENTITIES];
static gclient_t	g_clients[MAX_CLIENTS];
vmCvar_t		g_gametype;
vmCvar_t		g_maxclients;
vmCvar_t		g_motd;
vmCvar_t		g_restarted;
vmCvar_t		g_doWarmup;
vmCvar_t		g_debugAlloc;

static const char	*testCase = "setup";

static void Fail( const char *message ) {
	fprintf( stderr, "location configstring regression failed: %s: %s\n", testCase, message );
	exit( 1 );
}
static void Unexpected( const char *name ) {
	fprintf( stderr, "location configstring regression failed: %s: unexpected call to %s\n", testCase, name );
	exit( 1 );
}

/* --- prints and errors --- */
static int	developer;
static int	locationWarnings;
static char	locationWarning[1024];

void QDECL G_Printf( const char *fmt, ... ) {
	va_list	argptr;
	char	text[1024];

	va_start( argptr, fmt );
	vsnprintf( text, sizeof( text ), fmt, argptr );
	va_end( argptr );
	if ( !strstr( text, "target_location" ) ) {
		fprintf( stderr, "%s", text );
		Fail( "unexpected print" );
	}
	locationWarnings++;
	Q_strncpyz( locationWarning, text, sizeof( locationWarning ) );
}
void QDECL G_Error( const char *fmt, ... ) {
	va_list	argptr;

	va_start( argptr, fmt );
	vfprintf( stderr, fmt, argptr );
	va_end( argptr );
	fprintf( stderr, "\n" );
	Fail( "G_Error" );
}
void QDECL G_LogPrintf( const char *fmt, ... ) {
	(void)fmt;
}
void QDECL Com_Error( int level, const char *error, ... ) {
	(void)level;
	fprintf( stderr, "%s\n", error );
	Fail( "Com_Error" );
}
void QDECL Com_Printf( const char *msg, ... ) {
	(void)msg;
	Unexpected( "Com_Printf" );
}
int trap_Cvar_VariableIntegerValue( const char *var_name ) {
	if ( strcmp( var_name, "developer" ) ) {
		Fail( "unexpected cvar read" );
	}
	return developer;
}

/* --- the server's configstrings --- */
static char	*configstrings[MAX_CONFIGSTRINGS];

static void SetString( char **slot, const char *string ) {
	free( *slot );
	*slot = malloc( strlen( string ) + 1 );
	if ( !*slot ) {
		Fail( "out of memory" );
	}
	strcpy( *slot, string );
}

static const char *Configstring( char **strings, int num ) {
	return strings[num] ? strings[num] : "";
}

/** SV_SetConfigstring, which ends the map with ERR_DROP for an index past the array. */
void trap_SetConfigstring( int num, const char *string ) {
	if ( num < 0 || num >= MAX_CONFIGSTRINGS ) {
		fprintf( stderr, "ERR_DROP: SV_SetConfigstring: bad index %i\n", num );
		Fail( "the server would end the map" );
	}
	SetString( &configstrings[num], string ? string : "" );
}
void trap_GetConfigstring( int num, char *buffer, int bufferSize ) {
	if ( num < 0 || num >= MAX_CONFIGSTRINGS ) {
		Fail( "bad configstring index" );
	}
	Q_strncpyz( buffer, Configstring( configstrings, num ), bufferSize );
}

/* --- other engine traps the level load and team status reach --- */
static const char	*entityParsePoint;

/** As the server's G_GET_ENTITY_TOKEN: the next COM_Parse token, false at the end of the string. */
qboolean trap_GetEntityToken( char *buffer, int bufferSize ) {
	const char *s = COM_Parse( (char **)&entityParsePoint );

	Q_strncpyz( buffer, s, bufferSize );
	return entityParsePoint || s[0];
}
void trap_Cvar_Set( const char *name, const char *value ) {
	(void)name; (void)value;
}
void trap_LinkEntity( gentity_t *ent ) { ent->r.linked = qtrue; }
void trap_UnlinkEntity( gentity_t *ent ) { ent->r.linked = qfalse; }
void trap_LocateGameData( gentity_t *gEnts, int numGEntities, int sizeofGEntity_t, playerState_t *clients, int sizeofGClient ) {
	(void)gEnts; (void)numGEntities; (void)sizeofGEntity_t; (void)clients; (void)sizeofGClient;
}
/** Every location is in sight, so Team_GetLocation takes the nearest. */
qboolean trap_InPVS( const vec3_t p1, const vec3_t p2 ) {
	(void)p1; (void)p2;
	return qtrue;
}

static int		tinfoCommands;
static char		tinfoCommand[MAX_STRING_CHARS];

void trap_SendServerCommand( int clientNum, const char *text ) {
	if ( clientNum != 0 || strncmp( text, "tinfo ", 6 ) ) {
		fprintf( stderr, "%d: %s\n", clientNum, text );
		Fail( "unexpected server command" );
	}
	tinfoCommands++;
	Q_strncpyz( tinfoCommand, text, sizeof( tinfoCommand ) );
}

/* --- the spawn functions of game files the test does not link --- */
#define UNEXPECTED_SPAWN( name ) void name( gentity_t *ent ) { (void)ent; Unexpected( #name ); }
UNEXPECTED_SPAWN( SP_info_player_start )
UNEXPECTED_SPAWN( SP_info_player_deathmatch )
UNEXPECTED_SPAWN( SP_info_player_intermission )
UNEXPECTED_SPAWN( SP_info_null )
UNEXPECTED_SPAWN( SP_info_notnull )
UNEXPECTED_SPAWN( SP_info_camp )
UNEXPECTED_SPAWN( SP_func_plat )
UNEXPECTED_SPAWN( SP_func_button )
UNEXPECTED_SPAWN( SP_func_door )
UNEXPECTED_SPAWN( SP_func_static )
UNEXPECTED_SPAWN( SP_func_rotating )
UNEXPECTED_SPAWN( SP_func_bobbing )
UNEXPECTED_SPAWN( SP_func_pendulum )
UNEXPECTED_SPAWN( SP_func_train )
UNEXPECTED_SPAWN( SP_func_timer )
UNEXPECTED_SPAWN( SP_trigger_always )
UNEXPECTED_SPAWN( SP_trigger_multiple )
UNEXPECTED_SPAWN( SP_trigger_push )
UNEXPECTED_SPAWN( SP_trigger_teleport )
UNEXPECTED_SPAWN( SP_trigger_hurt )
UNEXPECTED_SPAWN( SP_target_push )
UNEXPECTED_SPAWN( SP_light )
UNEXPECTED_SPAWN( SP_path_corner )
UNEXPECTED_SPAWN( SP_misc_teleporter_dest )
UNEXPECTED_SPAWN( SP_misc_model )
UNEXPECTED_SPAWN( SP_misc_portal_surface )
UNEXPECTED_SPAWN( SP_misc_portal_camera )
UNEXPECTED_SPAWN( SP_shooter_rocket )
UNEXPECTED_SPAWN( SP_shooter_grenade )
UNEXPECTED_SPAWN( SP_shooter_plasma )

void G_SpawnItem( gentity_t *ent, gitem_t *item ) {
	(void)ent; (void)item;
	Unexpected( "G_SpawnItem" );
}

/* --- what the linked files' target and team entities do in the game --- */
void trap_Trace( trace_t *results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentmask ) {
	(void)results; (void)start; (void)mins; (void)maxs; (void)end; (void)passEntityNum; (void)contentmask;
	Unexpected( "trap_Trace" );
}
void G_Damage( gentity_t *targ, gentity_t *inflictor, gentity_t *attacker, vec3_t dir, vec3_t point, int damage, int dflags, int mod ) {
	(void)targ; (void)inflictor; (void)attacker; (void)dir; (void)point; (void)damage; (void)dflags; (void)mod;
	Unexpected( "G_Damage" );
}
void AddScore( gentity_t *ent, vec3_t origin, int score ) {
	(void)ent; (void)origin; (void)score;
	Unexpected( "AddScore" );
}
void TeleportPlayer( gentity_t *player, vec3_t origin, vec3_t angles ) {
	(void)player; (void)origin; (void)angles;
	Unexpected( "TeleportPlayer" );
}
void Touch_Item( gentity_t *ent, gentity_t *other, trace_t *trace ) {
	(void)ent; (void)other; (void)trace;
	Unexpected( "Touch_Item" );
}
void RespawnItem( gentity_t *ent ) {
	(void)ent;
	Unexpected( "RespawnItem" );
}
#ifdef MISSIONPACK
vmCvar_t		g_obeliskHealth;
vmCvar_t		g_obeliskRegenPeriod;
vmCvar_t		g_obeliskRegenAmount;
vmCvar_t		g_obeliskRespawnDelay;

void CalculateRanks( void ) {
	Unexpected( "CalculateRanks" );
}
#endif

/* --- master's linkup, for comparison --- */
static char			*masterConfigstrings[MAX_CONFIGSTRINGS];
static int			masterHealth[MAX_GENTITIES];
static gentity_t	*masterNextTrain[MAX_GENTITIES];
static gentity_t	*masterLocationHead;
static qboolean		masterDropped;

/** SV_SetConfigstring for master's loop: false where it ends the map with ERR_DROP. */
static qboolean MasterSetConfigstring( int num, const char *string ) {
	if ( num < 0 || num >= MAX_CONFIGSTRINGS ) {
		return qfalse;
	}
	SetString( &masterConfigstrings[num], string ? string : "" );
	return qtrue;
}

/*
 * Master's target_location_linkup loop, retail 1.32c's (git show
 * dbe4ddb:code/game/g_target.c), on the spawned entities. The health,
 * nextTrain and configstrings it sets go into the master* copies, and it
 * returns where SV_SetConfigstring's ERR_DROP would end the map.
 */
static void MasterLocationLinkup( void ) {
	gentity_t	*ent;
	int			i, n;

	masterDropped = qfalse;
	masterLocationHead = NULL;

	MasterSetConfigstring( CS_LOCATIONS, "unknown" );

	for (i = 0, ent = g_entities, n = 1;
			i < level.num_entities;
			i++, ent++) {
		if (ent->classname && !Q_stricmp(ent->classname, "target_location")) {
			// lets overload some variables!
			masterHealth[i] = n; // use for location marking
			if ( !MasterSetConfigstring( CS_LOCATIONS + n, ent->message ) ) {
				masterDropped = qtrue;
				return;
			}
			n++;
			masterNextTrain[i] = masterLocationHead;
			masterLocationHead = ent;
		}
	}
}

/* --- a retail client reading the team overlay --- */
static int		cg_argc;
static char		cg_argv[MAX_STRING_TOKENS][MAX_TOKEN_CHARS];

/** The client's Cmd_TokenizeString, for "tinfo", which holds numbers only. */
static void TokenizeTeamInfo( const char *text ) {
	const char	*s = text;
	int			l;

	cg_argc = 0;
	while ( 1 ) {
		while ( *s == ' ' ) {
			s++;
		}
		if ( !*s ) {
			return;
		}
		if ( cg_argc == MAX_STRING_TOKENS ) {
			Fail( "tinfo holds too many tokens" );
		}
		for ( l = 0 ; s[l] && s[l] != ' ' ; l++ ) {
		}
		if ( l >= MAX_TOKEN_CHARS ) {
			Fail( "tinfo holds a long token" );
		}
		memcpy( cg_argv[cg_argc], s, l );
		cg_argv[cg_argc][l] = 0;
		cg_argc++;
		s += l;
	}
}
static const char *CG_Argv( int arg ) {
	return arg < cg_argc ? cg_argv[arg] : "";
}

/* the cgs fields retail's CG_ParseTeamInfo fills in */
static int	numSortedTeamPlayers;
static int	sortedTeamPlayers[TEAM_MAXOVERLAY];
static int	clientLocation[MAX_CLIENTS];

/* Retail 1.32c's CG_ParseTeamInfo (git show dbe4ddb:code/cgame/cg_servercmds.c), for the location. */
static void CG_ParseTeamInfo( void ) {
	int		i;
	int		client;

	numSortedTeamPlayers = atoi( CG_Argv( 1 ) );
	if ( numSortedTeamPlayers != 1 ) {
		Fail( "tinfo does not hold the one player" );
	}

	for ( i = 0 ; i < numSortedTeamPlayers ; i++ ) {
		client = atoi( CG_Argv( i * 6 + 2 ) );
		if ( client != 0 ) {
			Fail( "tinfo names another client" );
		}

		sortedTeamPlayers[i] = client;

		clientLocation[ client ] = atoi( CG_Argv( i * 6 + 3 ) );
	}
}

/* Retail's CG_ConfigString (git show dbe4ddb:code/cgame/cg_main.c), on the configstrings the client got. */
static const char *CG_ConfigString( int index ) {
	if ( index < 0 || index >= MAX_CONFIGSTRINGS ) {
		fprintf( stderr, "CG_Error: CG_ConfigString: bad index: %i\n", index );
		Fail( "a retail client would end the game" );
	}
	return Configstring( configstrings, index );
}

/* What retail's CG_DrawTeamOverlay (cg_draw.c) shows as the player's location. */
static const char *OverlayLocation( int client ) {
	const char	*p;

	p = CG_ConfigString(CS_LOCATIONS + clientLocation[client]);
	if (!p || !*p)
		p = "unknown";
	return p;
}

/* --- maps --- */
#define MAX_TEST_LOCATIONS	MAX_MAP_ENTITIES

typedef struct {
	int			numLocations;
	qboolean	speakers;	// a target_speaker after every 50th location
} testMap_t;

static const testMap_t	testMaps[] = {
	{ 63, qtrue },
	{ MAX_LOCATIONS, qtrue },
	{ 100, qtrue },
	{ 300, qtrue },
	{ MAX_NAMED_LOCATIONS, qtrue },
	{ MAX_NAMED_LOCATIONS + 1, qtrue },
	{ 600, qtrue },
	{ MAX_MAP_ENTITIES, qfalse },
};
#define NUM_TEST_MAPS	( sizeof( testMaps ) / sizeof( testMaps[0] ) )

static char			entityString[MAX_TEST_LOCATIONS * 160 + 1024];
static gentity_t	*locations[MAX_TEST_LOCATIONS + 1];	// from 1, in entity order
static int			numLocations;

/** The k-th location's name: every 97th is empty, every 10th long. */
static const char *LocationName( int k ) {
	static char	name[MAX_QPATH];

	if ( k % 97 == 0 ) {
		return "";
	}
	Com_sprintf( name, sizeof( name ), "Location %d%s", k, k % 10 ? "" : " by the long corridor" );
	return name;
}
/** The k-th location's origin: 32 to a row, 128 units apart. */
static void LocationOrigin( int k, vec3_t origin ) {
	VectorSet( origin, ( k % 32 ) * 128, ( k / 32 ) * 128, 64 );
}

static void BuildMap( const testMap_t *m ) {
	vec3_t	origin;
	int		k;

	entityString[0] = 0;
	Q_strcat( entityString, sizeof( entityString ),
		"{\n\"classname\" \"worldspawn\"\n\"message\" \"Too Many Places\"\n\"music\" \"music/fla22k_02\"\n}\n" );
	for ( k = 1 ; k <= m->numLocations ; k++ ) {
		LocationOrigin( k, origin );
		// a "health" key, which the linkup must not leave as a location index
		Q_strcat( entityString, sizeof( entityString ),
			va( "{\n\"classname\" \"target_location\"\n\"message\" \"%s\"\n\"count\" \"%d\"\n\"health\" \"%d\"\n\"origin\" \"%d %d %d\"\n}\n",
			LocationName( k ), k % 8, 100000 + k, (int)origin[0], (int)origin[1], (int)origin[2] ) );
		if ( m->speakers && k % 50 == 0 ) {
			Q_strcat( entityString, sizeof( entityString ),
				va( "{\n\"classname\" \"target_speaker\"\n\"noise\" \"sound/world/speaker%d\"\n\"origin\" \"-64 -64 0\"\n}\n", k ) );
		}
	}
}

static void ClearConfigstrings( void ) {
	int i;

	for ( i = 0 ; i < MAX_CONFIGSTRINGS ; i++ ) {
		free( configstrings[i] );
		configstrings[i] = NULL;
		free( masterConfigstrings[i] );
		masterConfigstrings[i] = NULL;
	}
}

/** SV_SpawnServer and G_InitGame: a fresh level, one red player in client slot 0. */
static void ResetLevel( void ) {
	ClearConfigstrings();
	memset( &level, 0, sizeof( level ) );
	memset( g_entities, 0, sizeof( g_entities ) );
	memset( g_clients, 0, sizeof( g_clients ) );
	G_InitMemory();
	level.gentities = g_entities;
	level.clients = g_clients;
	level.maxclients = 1;
	level.num_entities = MAX_CLIENTS;
	level.time = 1000;
	level.startTime = 1000;
	level.sortedClients[0] = 0;
	g_gametype.integer = GT_CTF;
	g_maxclients.integer = 1;
	Q_strncpyz( g_motd.string, "Welcome", sizeof( g_motd.string ) );

	g_entities[0].s.number = 0;
	g_entities[0].inuse = qtrue;
	g_entities[0].client = &g_clients[0];
	g_clients[0].ps.clientNum = 0;
	g_clients[0].pers.connected = CON_CONNECTED;
	g_clients[0].pers.teamInfo = qtrue;
	g_clients[0].sess.sessionTeam = TEAM_RED;
}

/**
 * Load the map as SV_SpawnServer does: G_InitGame spawns the entities from
 * the entity string, then the settling frames run the target_locations'
 * think 200 ms after they spawned; the first one links them all.
 */
static void LoadMap( const testMap_t *m ) {
	gentity_t	*ent;
	vec3_t		origin;
	int			i, k;

	ResetLevel();
	BuildMap( m );
	entityParsePoint = entityString;
	G_SpawnEntitiesFromString();

	numLocations = 0;
	for ( i = 0 ; i < level.num_entities ; i++ ) {
		ent = &g_entities[i];
		if ( ent->inuse && ent->classname && !strcmp( ent->classname, "target_location" ) ) {
			k = ++numLocations;
			if ( k > m->numLocations ) {
				Fail( "the map spawned more target_locations than it holds" );
			}
			LocationOrigin( k, origin );
			if ( !VectorCompare( ent->r.currentOrigin, origin ) || strcmp( ent->message, LocationName( k ) )
				|| ent->count != k % 8 || ent->health != 100000 + k || ent->think == NULL ) {
				Fail( "a target_location did not spawn from the entity string" );
			}
			locations[k] = ent;
		}
	}
	if ( numLocations != m->numLocations ) {
		Fail( "the map did not spawn every target_location" );
	}

	// master's loop sets its strings on the configstrings as they are now
	for ( i = 0 ; i < MAX_CONFIGSTRINGS ; i++ ) {
		if ( configstrings[i] ) {
			SetString( &masterConfigstrings[i], configstrings[i] );
		}
	}

	// G_RunFrame's G_RunThink, 200 ms on
	locationWarnings = 0;
	level.time += 200;
	for ( i = 0 ; i < level.num_entities ; i++ ) {
		ent = &g_entities[i];
		if ( ent->inuse && ent->nextthink > 0 && ent->nextthink <= level.time ) {
			ent->nextthink = 0;
			ent->think( ent );
		}
	}
	if ( !level.locationLinked ) {
		Fail( "the target_locations did not link" );
	}
	MasterLocationLinkup();
}

/** Every configstring, location index and the location list, against master's. */
static void CheckLinkup( const testMap_t *m ) {
	char		expected[256];
	gentity_t	*ent, *masterEnt;
	int			i, k, named;

	named = m->numLocations < MAX_NAMED_LOCATIONS ? m->numLocations : MAX_NAMED_LOCATIONS;

	// master ends the map with ERR_DROP from 416 locations on
	if ( masterDropped != ( m->numLocations > MAX_NAMED_LOCATIONS ) ) {
		Fail( "master's loop did not drop the map where expected" );
	}
	// every configstring master sets, before it drops the map if it does
	for ( i = 0 ; i < MAX_CONFIGSTRINGS ; i++ ) {
		if ( strcmp( Configstring( configstrings, i ), Configstring( masterConfigstrings, i ) ) ) {
			fprintf( stderr, "configstring %d \"%s\", master \"%s\"\n", i,
				Configstring( configstrings, i ), Configstring( masterConfigstrings, i ) );
			Fail( "a configstring differs from master's" );
		}
	}
	if ( strcmp( Configstring( configstrings, CS_LOCATIONS ), "unknown" ) ) {
		Fail( "CS_LOCATIONS is not \"unknown\"" );
	}
	for ( k = 1 ; k <= named ; k++ ) {
		if ( strcmp( Configstring( configstrings, CS_LOCATIONS + k ), LocationName( k ) ) ) {
			Fail( "a location's configstring does not hold its name" );
		}
	}
	// the names run on past CS_PARTICLES as they always have
	if ( named > MAX_LOCATIONS && strcmp( Configstring( configstrings, CS_PARTICLES ), LocationName( MAX_LOCATIONS ) ) ) {
		Fail( "location 64 is not in CS_PARTICLES as on master" );
	}

	// the location index of each, master's where master sets one, else 0
	for ( k = 1 ; k <= m->numLocations ; k++ ) {
		i = locations[k] - g_entities;
		if ( locations[k]->health != ( k <= named ? masterHealth[i] : 0 ) || ( k <= named && masterHealth[i] != k ) ) {
			fprintf( stderr, "location %d: index %d\n", k, locations[k]->health );
			Fail( "a location index is not master's" );
		}
	}

	// every location is in the list, last spawned first; up to 415, master's list
	for ( ent = level.locationHead, masterEnt = masterLocationHead, k = m->numLocations ; ent ; ent = ent->nextTrain, k-- ) {
		if ( k < 1 || ent != locations[k] ) {
			Fail( "the location list does not hold every location in order" );
		}
		if ( !masterDropped ) {
			if ( ent != masterEnt ) {
				Fail( "the location list is not master's" );
			}
			masterEnt = masterNextTrain[masterEnt - g_entities];
		}
	}
	if ( k != 0 || ( !masterDropped && masterEnt ) ) {
		Fail( "the location list misses locations" );
	}

	// one warning, with "developer" set, for the locations without a name
	if ( m->numLocations > MAX_NAMED_LOCATIONS && developer ) {
		Com_sprintf( expected, sizeof( expected ), "%i of %i target_locations", m->numLocations - named, m->numLocations );
		if ( locationWarnings != 1 || !strstr( locationWarning, expected ) ) {
			Fail( "the linkup did not warn of the locations without a configstring" );
		}
	} else if ( locationWarnings ) {
		Fail( "the linkup warned without cause, or without \"developer\"" );
	}
}

/**
 * A player at each location in turn: CheckTeamStatus finds it and sends
 * "tinfo", and a retail client's team overlay shows it.
 */
static void CheckOverlay( const testMap_t *m ) {
	char		location[64], expected[128];
	const char	*shown;
	gentity_t	*player = &g_entities[0];
	int			k, named;

	named = m->numLocations < MAX_NAMED_LOCATIONS ? m->numLocations : MAX_NAMED_LOCATIONS;

	for ( k = 1 ; k <= m->numLocations ; k++ ) {
		VectorCopy( locations[k]->r.currentOrigin, player->r.currentOrigin );
		level.time += TEAM_LOCATION_UPDATE_TIME + 50;
		tinfoCommands = 0;
		CheckTeamStatus();
		if ( tinfoCommands != 1 ) {
			Fail( "CheckTeamStatus did not send the player's tinfo" );
		}
		if ( player->client->pers.teamState.location != ( k <= named ? k : 0 ) ) {
			Fail( "CheckTeamStatus did not find the nearest location" );
		}

		TokenizeTeamInfo( tinfoCommand );
		CG_ParseTeamInfo();
		shown = OverlayLocation( sortedTeamPlayers[0] );
		if ( k > named || !LocationName( k )[0] ) {
			Q_strncpyz( expected, "unknown", sizeof( expected ) );
		} else {
			Q_strncpyz( expected, LocationName( k ), sizeof( expected ) );
		}
		if ( strcmp( shown, expected ) ) {
			fprintf( stderr, "location %d: overlay \"%s\", expected \"%s\"\n", k, shown, expected );
			Fail( "the team overlay does not show the player's location" );
		}

		// say_team names the location from the entity, configstring or not
		if ( k % 8 ) {
			Com_sprintf( expected, sizeof( expected ), "%c%c%s" S_COLOR_WHITE, Q_COLOR_ESCAPE, k % 8 + '0', LocationName( k ) );
		} else {
			Q_strncpyz( expected, LocationName( k ), sizeof( expected ) );
		}
		if ( !Team_GetLocationMsg( player, location, sizeof( location ) ) || strcmp( location, expected ) ) {
			Fail( "say_team does not name the player's location" );
		}
	}
}

int main( void ) {
	static const testMap_t	silentMap = { MAX_NAMED_LOCATIONS + 1, qtrue };
	static char	name[128];
	size_t		i;

	if ( MAX_NAMED_LOCATIONS != 415 || MAX_MAP_ENTITIES <= 600 ) {
		Fail( "unexpected configstring or entity layout" );
	}

	for ( i = 0 ; i < NUM_TEST_MAPS ; i++ ) {
		Com_sprintf( name, sizeof( name ), "map with %d target_locations", testMaps[i].numLocations );
		testCase = name;
		developer = 1;
		LoadMap( &testMaps[i] );
		CheckLinkup( &testMaps[i] );
		CheckOverlay( &testMaps[i] );
	}

	// without "developer", the linkup is silent
	testCase = "map with 416 target_locations, developer 0";
	developer = 0;
	LoadMap( &silentMap );
	CheckLinkup( &silentMap );

	ClearConfigstrings();
	printf( "target_location_linkup names up to %d locations as master does and loads maps with up to %d without ERR_DROP; the rest show as unknown (issue #461)\n",
		MAX_NAMED_LOCATIONS, MAX_MAP_ENTITIES );
	return 0;
}
