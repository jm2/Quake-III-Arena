/* Issue #40: the base q3_ui server browser prints netnames[nettype] for every
 * listed server. CL_ServerInfoPacket replaces the first nettype of a server's
 * infoResponse with its own, but when that reply fills MAX_INFO_STRING the
 * engine's value does not fit and a second nettype from the server remains,
 * so ArenaServers_Insert must bound it. */
#include "../code/q3_ui/ui_servers2.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** Fail with a description of the first mismatch. */
static void Check( int ok, const char *what ) {
	if ( !ok ) {
		fprintf( stderr, "q3_ui server browser regression failed: %s\n", what );
		exit( 1 );
	}
}

/** cl_maxPing is unset, so ArenaServers_MaxPing uses its floor. */
float trap_Cvar_VariableValue( const char *var_name ) {
	Check( !Q_stricmp( var_name, "cl_maxPing" ), "only cl_maxPing is read" );
	return 0;
}

/** Fail on engine errors. */
void QDECL Com_Error( int level, const char *error, ... ) {
	(void)level;
	fprintf( stderr, "Unexpected Com_Error: %s\n", error );
	exit( 1 );
}

/** Nothing is printed on this path. */
void QDECL Com_Printf( const char *msg, ... ) {
	(void)msg;
}

/** List one local server whose infoResponse says nettype; return its row. */
static const char *ServerRow( const char *nettype ) {
	char info[MAX_INFO_STRING];

	memset( &g_arenaservers, 0, sizeof( g_arenaservers ) );
	memset( g_localserverlist, 0, sizeof( g_localserverlist ) );
	g_numlocalservers = 0;
	g_arenaservers.numservers = &g_numlocalservers;
	g_arenaservers.serverlist = g_localserverlist;
	g_arenaservers.maxservers = MAX_LOCALSERVERS;
	g_arenaservers.numqueriedservers = 1;
	g_servertype = AS_LOCAL;
	g_gametype = GAMES_ALL;
	g_emptyservers = g_fullservers = 1;

	Com_sprintf( info, sizeof( info ),
		"\\hostname\\lan arena\\mapname\\q3dm17\\clients\\2\\sv_maxclients\\8\\gametype\\0\\nettype\\%s", nettype );
	ArenaServers_Insert( "192.168.0.2:27960", info, 25 );
	Check( g_numlocalservers == 1, "server listed" );
	ArenaServers_UpdateMenu();
	Check( g_arenaservers.list.numitems == 1 && g_arenaservers.table[0].servernode == g_localserverlist,
		"server row built" );
	return g_arenaservers.table[0].buff;
}

/** The row names the network the index selects. */
static void CheckRow( const char *nettype, const char *label ) {
	char expected[MAX_LISTBOXWIDTH];

	Com_sprintf( expected, sizeof( expected ), " %s " S_COLOR_GREEN " 25", label );
	Check( strstr( ServerRow( nettype ), expected ) != NULL, nettype );
	Check( g_localserverlist[0].nettype >= 0 && g_localserverlist[0].nettype <= 2, "stored nettype" );
}

int main( void ) {
	static const char *hostile[] = { "3", "4", "-1", "1000000", "-1000000", "2147483647", "-2147483648", NULL };
	int i;

	// the engine's own values keep their labels
	CheckRow( "0", "???" );
	CheckRow( "1", "UDP" );
	CheckRow( "2", "IPX" );
	CheckRow( "", "???" );
	// the NULL terminator and everything past netnames read as unknown
	for ( i = 0; hostile[i]; i++ ) {
		CheckRow( hostile[i], "???" );
	}
	puts( "q3_ui server browser nettype stays inside netnames (issue #40)" );
	return 0;
}
