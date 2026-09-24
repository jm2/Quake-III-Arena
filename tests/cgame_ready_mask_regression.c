/* Issue #301: the intermission READY marker for clients 0-63 must match the retail PowerPC cgame.
 *
 * Built three ways by tests/run_cgame_ready_mask_tests.sh:
 *   plain                     real CG_DrawClientScore (Quake3 scoreboard)
 *   -DMISSIONPACK             real CG_DrawClientScore (Team Arena build of the same scoreboard)
 *   -DMISSIONPACK -DREADY_FEEDER
 *                             real CG_FeederItemText column 2 (Team Arena menu scoreboard),
 *                             linked with the real cg_scoreboard.c
 */
#ifdef READY_FEEDER
#include "../code/cgame/cg_main.c"
#else
#include "../code/cgame/cg_scoreboard.c"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static snapshot_t snap;
static int readyDrawn;

/** Fail when a READY marker differs from the retail cgame. */
static void Check( int ok, const char *message, int stats, int client ) {
	if ( !ok ) {
		fprintf( stderr, "Cgame ready mask regression failed: %s (stats 0x%08x, client %i)\n",
			message, (unsigned)stats, client );
		exit( 1 );
	}
}

#ifndef READY_FEEDER
cg_t cg;
cgs_t cgs;
vmCvar_t cg_drawIcons;

/** Count the READY markers the scoreboard draws. */
void CG_DrawBigStringColor( int x, int y, const char *s, vec4_t color ) {
	(void)x; (void)y; (void)color;
	if ( !strcmp( s, "READY" ) ) {
		readyDrawn++;
	}
}
void CG_DrawBigString( int x, int y, const char *s, float alpha ) { (void)x; (void)y; (void)s; (void)alpha; }
void CG_DrawSmallStringColor( int x, int y, const char *s, vec4_t color ) { (void)x; (void)y; (void)s; (void)color; }
void CG_FillRect( float x, float y, float width, float height, const float *color ) {
	(void)x; (void)y; (void)width; (void)height; (void)color;
}
void CG_DrawPic( float x, float y, float width, float height, qhandle_t hShader ) {
	(void)x; (void)y; (void)width; (void)height; (void)hShader;
}
void CG_DrawHead( float x, float y, float w, float h, int clientNum, vec3_t headAngles ) {
	(void)x; (void)y; (void)w; (void)h; (void)clientNum; (void)headAngles;
}
void CG_DrawFlagModel( float x, float y, float w, float h, int team, qboolean force2D ) {
	(void)x; (void)y; (void)w; (void)h; (void)team; (void)force2D;
}
void QDECL Com_Printf( const char *msg, ... ) { (void)msg; Check( 0, "unexpected Com_Printf", 0, -1 ); }
void QDECL Com_Error( int level, const char *error, ... ) { (void)level; (void)error; Check( 0, "engine error", 0, -1 ); }

/** Return 1 when the real scoreboard line of client draws READY. */
static int ShownReady( int client ) {
	score_t score;
	float color[4] = { 1, 1, 1, 1 };

	memset( &score, 0, sizeof(score) );
	score.client = client;
	readyDrawn = 0;
	CG_DrawClientScore( 100, &score, color, 1.0f, ( client & 1 ) ? qtrue : qfalse );
	Check( readyDrawn <= 1, "READY drawn twice", snap.ps.stats[STAT_CLIENTS_READY], client );
	return readyDrawn;
}
#else
gitem_t bg_itemlist[1];
qhandle_t CG_StatusHandle( int task ) { (void)task; return 0; }
gitem_t *BG_FindItemForPowerup( powerup_t pw ) { (void)pw; return NULL; }

/** Return 1 when the real Team Arena scoreboard feeder shows client as Ready. */
static int ShownReady( int client ) {
	qhandle_t handle;
	const char *text;

	memset( cg.scores, 0, sizeof(cg.scores) );
	cg.scores[0].client = client;
	cg.numScores = 1;
	text = CG_FeederItemText( FEEDER_SCOREBOARD, 0, 2, &handle );
	return text != NULL && !strcmp( text, "Ready" );
}
#endif

/** The retail cgame computed stats & ( 1 << client ) with PowerPC slw, which uses the low six
    count bits and clears the word for counts 32-63 (the QVM interpreter matches it since #248). */
static int RetailReady( int stats, int client ) {
	unsigned count = (unsigned)client & 63;
	unsigned bit = count < 32 ? 1u << count : 0u;

	return ( (unsigned)stats & bit ) != 0;
}

/** Check every client number against the retail result for one STAT_CLIENTS_READY value. */
static void CheckStats( int stats ) {
	int client;

	snap.ps.stats[STAT_CLIENTS_READY] = stats;
	for ( client = 0; client < MAX_CLIENTS; client++ ) {
		Check( ShownReady( client ) == RetailReady( stats, client ), "READY differs from retail", stats, client );
	}
}

/** Pin one client's marker for one stats value. */
static void Expect( int stats, int client, int ready ) {
	snap.ps.stats[STAT_CLIENTS_READY] = stats;
	Check( ShownReady( client ) == ready, ready ? "client not shown ready" : "client shown ready", stats, client );
}

int main( void ) {
	static const int rawStats[] = {
		0x7fffffff, (int)0x80000000, 0x00010000, 0x55555555, (int)0xaaaaaaaa, (int)0xdeadbeef, 0x40000001
	};
	int bit, mask, client;
	unsigned i;

	cg.snap = &snap;
	cg.time = 1000;
	cgs.maxclients = MAX_CLIENTS;
	cgs.gametype = GT_FFA;
	snap.ps.clientNum = 0;
	for ( client = 0; client < MAX_CLIENTS; client++ ) {
		cgs.clientinfo[client].infoValid = qtrue;
		cgs.clientinfo[client].team = TEAM_FREE;
		cgs.clientinfo[client].handicap = 100;
	}

	/* stats as MSG_ReadShort delivers them: the 16-bit wire value sign-extended */
	CheckStats( 0 );
	for ( bit = 0; bit < 16; bit++ ) {
		CheckStats( (short)( 1 << bit ) );
	}
	CheckStats( (short)0xffff );
	CheckStats( (short)0x7fff );
	CheckStats( (short)0x5555 );
	CheckStats( (short)0xaaaa );
	CheckStats( (short)0x8001 );
	for ( mask = 0; mask < 0x10000; mask += 0x0fff ) {
		CheckStats( (short)mask );
	}
	/* any other value the int stat can hold */
	for ( i = 0; i < sizeof(rawStats) / sizeof(rawStats[0]); i++ ) {
		CheckStats( rawStats[i] );
	}

	/* client 15 ready: retail also shows 16-31 (sign extension), never 32-63 */
	for ( client = 0; client < MAX_CLIENTS; client++ ) {
		Expect( (short)0x8000, client, client >= 15 && client < 32 );
	}
	/* x86 would alias client 32 to bit 0 and client 63 to bit 31 */
	Expect( 1, 0, 1 );
	Expect( 1, 32, 0 );
	Expect( -1, 31, 1 );
	Expect( -1, 32, 0 );
	Expect( -1, 63, 0 );
	/* PowerPC sraw (the Retro68 code for the old expression) would copy the sign bit into 32-63 */
	Expect( (int)0x80000000, 31, 1 );
	Expect( (int)0x80000000, 47, 0 );

#ifdef READY_FEEDER
	printf( "Cgame ready mask regressions passed: Team Arena feeder, clients 0-%i (issue #301)\n", MAX_CLIENTS - 1 );
#elif defined( MISSIONPACK )
	printf( "Cgame ready mask regressions passed: Team Arena scoreboard, clients 0-%i (issue #301)\n", MAX_CLIENTS - 1 );
#else
	printf( "Cgame ready mask regressions passed: scoreboard, clients 0-%i (issue #301)\n", MAX_CLIENTS - 1 );
#endif
	return 0;
}
