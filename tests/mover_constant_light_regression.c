/*
 * Issue #344: InitMover (code/game/g_mover.c) packs a mover's "color" and
 * "light" entity keys into entityState_t.constantLight as
 * r | ( g << 8 ) | ( b << 16 ) | ( i << 24 ) in signed int arithmetic. The
 * keys are map data, so i reaches the sign bit for any light of 512 or more,
 * and r, g, b and i go negative for negative keys; each of those is an
 * undefined left shift in C. The game module is native code here, so the
 * compiler decides what happens. Retro68 GCC at -O0 simply shifts the bits.
 *
 * This test links the real g_mover.c, g_spawn.c and g_utils.c and spawns
 * every mover class that calls InitMover (func_door, func_plat, func_button,
 * func_train, func_static, func_rotating, func_bobbing, func_pendulum) from
 * an entity string. The string goes through the real G_ParseSpawnVars (fed by
 * a trap_GetEntityToken that parses like the server's), G_Spawn and
 * G_ParseField, as G_SpawnGEntityFromSpawnVars does, and then the real SP_
 * function for the class. The light and color keys cover the stock range, the
 * sign-bit range, the clamp and negative values. Under -fsanitize=shift master
 * aborts on the first light of 512. Every expected value below is what master
 * built with -fwrapv (the -O0 bits) stores, so the fix must not change one.
 *
 * Issue #373: r, g, b and i come from float-to-int conversions of the keys,
 * which are undefined in C when the value is beyond the int range ("light"
 * "1e10", inf or NaN). PowerPC's fctiwz saturates there, and x86 gives
 * INT_MIN. The last cases are such keys, run under
 * -fsanitize=float-cast-overflow; each expects what the PowerPC build of
 * master stores, so the Mac's movers keep their light.
 */
#include "../code/game/g_local.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

level_locals_t	level;
gentity_t		g_entities[MAX_GENTITIES];
gclient_t		g_clients[MAX_CLIENTS];
vmCvar_t		g_gravity;

static const char	*testCase = "setup";

static void Fail( const char *message ) {
	fprintf( stderr, "mover constantLight regression failed: %s: %s\n", testCase, message );
	exit( 1 );
}
static void Unexpected( const char *name ) {
	fprintf( stderr, "mover constantLight regression failed: %s: unexpected call to %s\n", testCase, name );
	exit( 1 );
}

/* --- engine traps the spawn path reaches --- */
static const char	*entityParsePoint;

/** As the server's G_GET_ENTITY_TOKEN: the next COM_Parse token, false at the end of the string. */
qboolean trap_GetEntityToken( char *buffer, int bufferSize ) {
	const char *s = COM_Parse( (char **)&entityParsePoint );

	Q_strncpyz( buffer, s, bufferSize );
	return entityParsePoint || s[0];
}
static char	configstrings[MAX_CONFIGSTRINGS][MAX_QPATH];

void trap_GetConfigstring( int num, char *buffer, int bufferSize ) {
	if ( num < 0 || num >= MAX_CONFIGSTRINGS ) {
		Fail( "configstring index out of range" );
	}
	Q_strncpyz( buffer, configstrings[num], bufferSize );
}
void trap_SetConfigstring( int num, const char *string ) {
	if ( num < 0 || num >= MAX_CONFIGSTRINGS ) {
		Fail( "configstring index out of range" );
	}
	Q_strncpyz( configstrings[num], string, sizeof( configstrings[num] ) );
}
/** A 64-unit brush model, as the server's clip model would set it. */
void trap_SetBrushModel( gentity_t *ent, const char *name ) {
	if ( !name || name[0] != '*' ) {
		Fail( "mover without an inline brush model" );
	}
	VectorSet( ent->r.mins, -32, -32, -32 );
	VectorSet( ent->r.maxs, 32, 32, 32 );
	ent->r.bmodel = qtrue;
	ent->r.contents = CONTENTS_SOLID;
}
void trap_LinkEntity( gentity_t *ent ) { ent->r.linked = qtrue; }
void trap_UnlinkEntity( gentity_t *ent ) { ent->r.linked = qfalse; }
void trap_LocateGameData( gentity_t *gEnts, int numGEntities, int sizeofGEntity_t, playerState_t *clients, int sizeofGClient ) {
	(void)gEnts; (void)numGEntities; (void)sizeofGEntity_t; (void)clients; (void)sizeofGClient;
}
/** G_ParseField's strings; the pool is emptied for every level, as G_InitMemory does. */
static char	allocPool[16 * 1024];
static int	allocUsed;

void *G_Alloc( int size ) {
	char *p;

	size = ( size + 31 ) & ~31;
	if ( size <= 0 || allocUsed + size > (int)sizeof( allocPool ) ) {
		Fail( "G_Alloc pool exhausted" );
	}
	p = allocPool + allocUsed;
	allocUsed += size;
	return p;
}
void QDECL G_Printf( const char *fmt, ... ) { (void)fmt; Unexpected( "G_Printf" ); }
void QDECL G_Error( const char *fmt, ... ) { (void)fmt; Unexpected( "G_Error" ); }
void QDECL Com_Error( int level, const char *error, ... ) { (void)level; (void)error; Unexpected( "Com_Error" ); }
void QDECL Com_Printf( const char *msg, ... ) { (void)msg; Unexpected( "Com_Printf" ); }

/* --- symbols of mover callbacks that are assigned but never run here --- */
void G_Damage( gentity_t *targ, gentity_t *inflictor, gentity_t *attacker, vec3_t dir, vec3_t point, int damage, int dflags, int mod ) {
	(void)targ; (void)inflictor; (void)attacker; (void)dir; (void)point; (void)damage; (void)dflags; (void)mod;
	Unexpected( "G_Damage" );
}
void BG_EvaluateTrajectory( const trajectory_t *tr, int atTime, vec3_t result ) {
	(void)tr; (void)atTime; (void)result;
	Unexpected( "BG_EvaluateTrajectory" );
}
void G_ExplodeMissile( gentity_t *ent ) { (void)ent; Unexpected( "G_ExplodeMissile" ); }
void G_RunThink( gentity_t *ent ) { (void)ent; Unexpected( "G_RunThink" ); }
void Team_DroppedFlagThink( gentity_t *ent ) { (void)ent; Unexpected( "Team_DroppedFlagThink" ); }
void trap_AdjustAreaPortalState( gentity_t *ent, qboolean open ) { (void)ent; (void)open; Unexpected( "trap_AdjustAreaPortalState" ); }
int trap_EntitiesInBox( const vec3_t mins, const vec3_t maxs, int *list, int maxcount ) {
	(void)mins; (void)maxs; (void)list; (void)maxcount;
	Unexpected( "trap_EntitiesInBox" );
	return 0;
}
void trap_Trace( trace_t *results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentmask ) {
	(void)results; (void)start; (void)mins; (void)maxs; (void)end; (void)passEntityNum; (void)contentmask;
	Unexpected( "trap_Trace" );
}
qboolean trap_EntityContact( const vec3_t mins, const vec3_t maxs, const gentity_t *ent ) {
	(void)mins; (void)maxs; (void)ent;
	Unexpected( "trap_EntityContact" );
	return qfalse;
}
int trap_PointContents( const vec3_t point, int passEntityNum ) { (void)point; (void)passEntityNum; Unexpected( "trap_PointContents" ); return 0; }
void TeleportPlayer( gentity_t *player, vec3_t origin, vec3_t angles ) {
	(void)player; (void)origin; (void)angles;
	Unexpected( "TeleportPlayer" );
}
void trap_SendServerCommand( int clientNum, const char *text ) { (void)clientNum; (void)text; Unexpected( "trap_SendServerCommand" ); }

/* --- the spawn path --- */
/* g_spawn.c and g_mover.c do not export these through g_local.h. */
qboolean G_ParseSpawnVars( void );
void G_ParseField( const char *key, const char *value, gentity_t *ent );
void SP_func_door( gentity_t *ent );
void SP_func_plat( gentity_t *ent );
void SP_func_button( gentity_t *ent );
void SP_func_train( gentity_t *ent );
void SP_func_static( gentity_t *ent );
void SP_func_rotating( gentity_t *ent );
void SP_func_bobbing( gentity_t *ent );
void SP_func_pendulum( gentity_t *ent );

typedef struct {
	const char	*classname;
	void		(*spawn)( gentity_t *ent );
} moverClass_t;

/* Every class whose spawn function calls InitMover, as the game's spawns[] table maps them. */
static const moverClass_t moverClasses[] = {
	{ "func_door", SP_func_door },
	{ "func_plat", SP_func_plat },
	{ "func_button", SP_func_button },
	{ "func_train", SP_func_train },
	{ "func_static", SP_func_static },
	{ "func_rotating", SP_func_rotating },
	{ "func_bobbing", SP_func_bobbing },
	{ "func_pendulum", SP_func_pendulum },
};
#define NUM_MOVER_CLASSES	( sizeof( moverClasses ) / sizeof( moverClasses[0] ) )

typedef struct {
	const char	*light;		// NULL leaves the key out
	const char	*color;		// NULL leaves the key out
	int			expected;	// master's constantLight with -fwrapv, or PowerPC's (#373)
} lightCase_t;

/*
 * r, g and b are color * 255 and i is light / 4, each truncated toward zero
 * and clamped to 255 from above only; an absent key reads as "1 1 1" or "100".
 */
static const lightCase_t lightCases[] = {
	// the stock range: no sign bit, no negative value
	{ "100", NULL, 0x19ffffff },
	{ NULL, "1 0.5 0.25", 0x193f7fff },
	{ "0", "0 0 0", 0x00000000 },
	{ "300", "0.2 0.4 0.6", 0x4b996633 },
	{ "511", NULL, 0x7fffffff },
	// i in the sign bit: 512 <= light < 1024, then the clamp to 255
	{ "512", NULL, (int)0x80ffffff },
	{ "600", "0.2 0.4 0.6", (int)0x96996633 },
	{ "1020", "0 0 0", (int)0xff000000 },
	{ "1023", NULL, (int)0xffffffff },
	{ "1024", NULL, (int)0xffffffff },	// i == 256: the clamp
	{ "100000", "1 1 1", (int)0xffffffff },
	// a negative light: the top byte is i's low byte in two's complement
	{ "-3", NULL, 0x00ffffff },
	{ "-4", "0 0 0", (int)0xff000000 },
	{ "-100", NULL, (int)0xe7ffffff },
	{ "-1020", "0 0 0", 0x01000000 },
	{ "-8589934592", NULL, 0x00ffffff },	// i == INT_MIN
	// negative colors: each component's sign bits cover the bytes above it
	{ "0", "-1 0 0", (int)0xffffff01 },
	{ "0", "0 -0.5 0", (int)0xffff8100 },
	{ "40", "0 0 -1", (int)0xff010000 },
	{ "100", "-1 -1 -1", (int)0xffffff01 },
	{ "0", "0 -8421504 0", 0x00008000 },
	{ "0", "0 0 -8421504", 0x00800000 },
	{ "-8", "0.5 -0.25 2", (int)0xffffc17f },
	// the ends of the int range
	{ "0", "-8421505 0 0", (int)0x80000000 },	// color * 255 rounds to INT_MIN
	{ "8589934080", NULL, (int)0xffffffff },	// light / 4 is the last float below 2^31
	// issue #373: keys beyond the int range, which were undefined. They
	// saturate, as PowerPC's fctiwz does: above INT_MAX gives 255 through the
	// clamp, and below INT_MIN, -inf and NaN give INT_MIN, whose bits shift
	// out of g, b and i
	{ "1e10", NULL, (int)0xffffffff },
	{ "-1e10", NULL, 0x00ffffff },
	{ "8589934592", NULL, (int)0xffffffff },	// light / 4 == 2^31
	{ "-8589935616", NULL, 0x00ffffff },	// light / 4 is the first float below INT_MIN
	{ "1e40", NULL, (int)0xffffffff },	// atof's float is +inf
	{ "-1e40", NULL, 0x00ffffff },
	{ "inf", "0 0 0", (int)0xff000000 },
	{ "-inf", "0 0 0", 0x00000000 },
	{ "nan", NULL, 0x00ffffff },
	{ "0", "1e10 0 0", 0x000000ff },
	{ "0", "-1e10 0 0", (int)0x80000000 },
	{ "0", "-8421506 0 0", (int)0x80000000 },	// color * 255 is the first float below INT_MIN
	{ "0", "0 -1e10 0", 0x00000000 },
	{ "0", "0 0 -1e10", 0x00000000 },
	{ "0", "nan 0.5 1", (int)0x80ff7f00 },
	{ "40", "1e39 -1e39 nan", 0x0a0000ff },	// sscanf's floats are +inf, -inf and NaN
	{ "1e10", "-1e10 0.5 1e10", (int)0xffff7f00 },
	{ "-1e10", "-1e10 -1e10 -1e10", (int)0x80000000 },
};
#define NUM_LIGHT_CASES	( sizeof( lightCases ) / sizeof( lightCases[0] ) )

/**
 * Spawn one mover from its entity string as G_SpawnEntitiesFromString and
 * G_SpawnGEntityFromSpawnVars do, with the class's real spawn function.
 */
static gentity_t *SpawnMover( const moverClass_t *mover, const lightCase_t *lightCase ) {
	char		entityString[512];
	gentity_t	*ent;
	int			i;

	Com_sprintf( entityString, sizeof( entityString ), "{\n\"classname\" \"%s\"\n\"model\" \"*1\"\n\"origin\" \"0 0 64\"\n",
		mover->classname );
	if ( !strcmp( mover->classname, "func_train" ) ) {
		Q_strcat( entityString, sizeof( entityString ), "\"target\" \"t1\"\n" );
	}
	if ( lightCase->light ) {
		Q_strcat( entityString, sizeof( entityString ), va( "\"light\" \"%s\"\n", lightCase->light ) );
	}
	if ( lightCase->color ) {
		Q_strcat( entityString, sizeof( entityString ), va( "\"color\" \"%s\"\n", lightCase->color ) );
	}
	Q_strcat( entityString, sizeof( entityString ), "}\n" );

	// a fresh level for every mover, as after G_InitGame
	memset( g_entities, 0, sizeof( g_entities ) );
	memset( configstrings, 0, sizeof( configstrings ) );
	allocUsed = 0;
	level.gentities = g_entities;
	level.clients = g_clients;
	level.num_entities = MAX_CLIENTS;
	level.time = level.startTime = 0;

	entityParsePoint = entityString;
	if ( !G_ParseSpawnVars() ) {
		Fail( "G_ParseSpawnVars found no entity" );
	}
	ent = G_Spawn();
	for ( i = 0 ; i < level.numSpawnVars ; i++ ) {
		G_ParseField( level.spawnVars[i][0], level.spawnVars[i][1], ent );
	}
	VectorCopy( ent->s.origin, ent->s.pos.trBase );
	VectorCopy( ent->s.origin, ent->r.currentOrigin );
	if ( strcmp( ent->classname, mover->classname ) ) {
		Fail( "the classname did not parse" );
	}
	mover->spawn( ent );
	if ( G_ParseSpawnVars() ) {
		Fail( "the entity string held more than one entity" );
	}
	if ( !ent->inuse || ent->s.eType != ET_MOVER ) {
		Fail( "the mover did not spawn" );
	}
	return ent;
}

int main( void ) {
	char		name[128];
	gentity_t	*ent;
	size_t		c, l;

	g_gravity.value = 800;
	for ( c = 0 ; c < NUM_MOVER_CLASSES ; c++ ) {
		for ( l = 0 ; l < NUM_LIGHT_CASES ; l++ ) {
			snprintf( name, sizeof( name ), "%s light %s color %s", moverClasses[c].classname,
				lightCases[l].light ? lightCases[l].light : "(none)", lightCases[l].color ? lightCases[l].color : "(none)" );
			testCase = name;
			ent = SpawnMover( &moverClasses[c], &lightCases[l] );
			if ( ent->s.constantLight != lightCases[l].expected ) {
				fprintf( stderr, "constantLight 0x%08x, expected 0x%08x\n",
					(unsigned)ent->s.constantLight, (unsigned)lightCases[l].expected );
				Fail( "constantLight changed" );
			}
		}
	}
	printf( "Mover constantLight packs %d light/color keys as the PowerPC build does for %d mover classes (issues #344, #373)\n",
		(int)NUM_LIGHT_CASES, (int)NUM_MOVER_CLASSES );
	return 0;
}
