/*
 * q3dm0 has five deathmatch spawn points and marks one "nobots". Selection
 * skipped occupied spots, and ClientSpawn rejected a "nobots" result and tried
 * again. With the other four taken, the fifth bot was offered that one spot
 * every time and the server never left the loop. Five bots carried into q3dm0
 * do this as the map starts, before the human has spawned.
 *
 * The real g_client.c, with the entities and traps it touches stubbed. Selection
 * has to apply the flags itself, so a spot a client may not use is never returned.
 */
#include "../code/game/g_client.c"
#include <stdio.h>
#include <stdlib.h>

#define SPOTS	5
#define PLAYER	( SPOTS + 1 )

gentity_t g_entities[MAX_GENTITIES];
static gclient_t player;
static qboolean occupied[SPOTS];

static void Check( int condition, const char *message ) {
	if ( !condition ) {
		fprintf( stderr, "Spawn regression failed: %s\n", message );
		exit( 1 );
	}
}

void QDECL G_Error( const char *fmt, ... ) {
	(void)fmt;
	Check( 0, "G_Error" );
}

gentity_t *G_Find( gentity_t *from, int fieldofs, const char *match ) {
	int i = from ? from - g_entities + 1 : 0;

	(void)fieldofs; (void)match;
	return i < SPOTS ? &g_entities[i] : NULL;
}

int trap_EntitiesInBox( const vec3_t mins, const vec3_t maxs, int *list, int maxcount ) {
	int i;

	(void)maxs; (void)maxcount;
	for ( i = 0; i < SPOTS; i++ ) {
		if ( occupied[i] && g_entities[i].s.origin[0] == mins[0] - playerMins[0] ) {
			list[0] = PLAYER;
			return 1;
		}
	}
	return 0;
}

/** Five spots, like q3dm0, with flags on the one at index. */
static void Map( int index, int flags ) {
	int i;

	memset( g_entities, 0, sizeof( g_entities ) );
	memset( occupied, 0, sizeof( occupied ) );
	g_entities[PLAYER].client = &player;
	for ( i = 0; i < SPOTS; i++ ) {
		g_entities[i].s.origin[0] = 100 * ( i + 1 );
	}
	g_entities[index].flags = flags;
	g_entities[index].spawnflags = 1;
}

static void TakeAllBut( int index ) {
	int i;

	for ( i = 0; i < SPOTS; i++ ) {
		occupied[i] = i != index;
	}
}

static int Select( qboolean isBot ) {
	vec3_t avoid = { 0, 0, 0 }, origin, angles;

	return SelectSpawnPoint( avoid, origin, angles, isBot ) - g_entities;
}

int main( void ) {
	vec3_t origin, angles;
	int i;

	/* Free spots: a client gets one it may use. */
	Map( 4, FL_NO_BOTS );
	for ( i = 0; i < 500; i++ ) {
		Check( Select( qtrue ) != 4, "a bot was given a nobots spot" );
	}
	Map( 4, FL_NO_HUMANS );
	for ( i = 0; i < 500; i++ ) {
		Check( Select( qfalse ) != 4, "a human was given a nohumans spot" );
	}

	/* The hang: every spot taken but the nobots one. A bot shares a spot instead. */
	Map( 4, FL_NO_BOTS );
	TakeAllBut( 4 );
	Check( Select( qtrue ) == 0, "the fifth bot was offered the nobots spot" );
	Check( Select( qfalse ) == 4, "a human can use the nobots spot" );

	/* Everything taken, and the spot listed first is not for this client. */
	Map( 0, FL_NO_BOTS );
	TakeAllBut( -1 );
	Check( Select( qtrue ) == 1, "a full map gave a bot the nobots spot" );
	Check( Select( qfalse ) == 0, "a full map moved the human" );

	/* A map with no spot for this client still spawns it. */
	Map( 0, FL_NO_BOTS );
	for ( i = 1; i < SPOTS; i++ ) {
		g_entities[i].flags = FL_NO_BOTS;
	}
	Check( Select( qtrue ) == 0, "no spot for bots" );

	/* The initial spot follows the same rule. */
	Map( 4, FL_NO_BOTS );
	Check( SelectInitialSpawnPoint( origin, angles, qfalse ) == &g_entities[4], "the human starts on the initial spot" );
	Check( SelectInitialSpawnPoint( origin, angles, qtrue ) != &g_entities[4], "a bot started on the nobots initial spot" );

	puts( "Spawn selection keeps every client off spots it may not use, and always ends" );
	return 0;
}
