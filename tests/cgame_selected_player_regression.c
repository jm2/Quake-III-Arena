/* Issue #379: a server can set cg_currentSelectedPlayer to anything through
 * systeminfo, so the Team Arena cgame must bound it before it indexes the team
 * overlay's sortedTeamPlayers. */
#include "../code/cgame/cg_local.h"
#include "../ui/menudef.h"
#include "systeminfo_cvar_harness.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOCAL_CLIENT 5
#define LOCAL_INDEX 1	/* our overlay slot */
#define NO_SELECTION -1
#define GESTURE "+button7; wait; -button7"	/* CG_CheckOrderPending, TEAMTASK_OFFENSE */

void dllEntry( int (QDECL *syscallptr)( int arg, ... ) );	/* cg_syscalls.c */
void CG_RegisterCvars( void );	/* cg_main.c */

static char commandText[MAX_STRING_CHARS];
static char *commandArgs[MAX_STRING_TOKENS];
static int commandArgc;
static char sent[MAX_STRING_CHARS];
static snapshot_t serverSnap;
static const char *selection;
static int teamSize;

/** Fail with the case under test and the contract that broke. */
static void Check( int ok, const char *what ) {
	if ( !ok ) {
		fprintf( stderr, "Cgame selected player regression failed (%i teammates, cg_currentSelectedPlayer %s): %s\n",
			teamSize, selection, what );
		exit( 1 );
	}
}

/** cl_cgame.c's syscalls on the tested paths; cvars go to the real cvar.c. */
static int QDECL FakeSyscall( int command, ... ) {
	va_list ap;
	int result = 0;

	va_start( ap, command );
	switch ( command ) {
	case CG_ERROR:
		Check( 0, va_arg( ap, const char * ) );
		break;
	case CG_PRINT:
		break;
	case CG_CVAR_REGISTER: {
		vmCvar_t *vmCvar = va_arg( ap, vmCvar_t * );
		const char *name = va_arg( ap, const char * );
		const char *value = va_arg( ap, const char * );
		Cvar_Register( vmCvar, name, value, va_arg( ap, int ) );
		break;
	}
	case CG_CVAR_UPDATE:
		Cvar_Update( va_arg( ap, vmCvar_t * ) );
		break;
	case CG_CVAR_SET: {
		const char *name = va_arg( ap, const char * );
		Cvar_Set( name, va_arg( ap, const char * ) );
		break;
	}
	case CG_CVAR_VARIABLESTRINGBUFFER: {
		const char *name = va_arg( ap, const char * );
		char *buffer = va_arg( ap, char * );
		Cvar_VariableStringBuffer( name, buffer, va_arg( ap, int ) );
		break;
	}
	case CG_ARGC:
		result = commandArgc;
		break;
	case CG_ARGV: {
		int n = va_arg( ap, int );
		char *buffer = va_arg( ap, char * );
		int length = va_arg( ap, int );
		Q_strncpyz( buffer, n >= 0 && n < commandArgc ? commandArgs[n] : "", length );
		break;
	}
	case CG_GETSERVERCOMMAND:
		result = 1;
		break;
	case CG_SENDCONSOLECOMMAND:
		Q_strcat( sent, sizeof( sent ), va_arg( ap, const char * ) );
		break;
	default:
		Check( 0, va( "unexpected cgame syscall %d", command ) );
	}
	va_end( ap );
	return result;
}

/** Tokenize a command for CG_Argv. */
static void Tokenize( const char *text ) {
	char *token;

	Q_strncpyz( commandText, text, sizeof( commandText ) );
	commandArgc = 0;
	for ( token = strtok( commandText, " " ); token; token = strtok( NULL, " " ) ) {
		commandArgs[commandArgc++] = token;
	}
}

/** The client in overlay slot i: us in LOCAL_INDEX, other clients elsewhere. */
static int SlotClient( int i ) {
	return i == LOCAL_INDEX ? LOCAL_CLIENT : i < LOCAL_CLIENT ? i : i + 1;
}

/** A frame's worth of state: our snapshot, a CTF game, and the server's team
 * overlay ("tinfo") for teamSize teammates. */
static void Serve( void ) {
	char text[MAX_STRING_CHARS];
	int i;

	memset( &cg, 0, sizeof( cg ) );
	memset( &cgs, 0, sizeof( cgs ) );
	memset( &serverSnap, 0, sizeof( serverSnap ) );
	serverSnap.ps.clientNum = LOCAL_CLIENT;
	cg.snap = &serverSnap;
	cg.time = 1000;
	cgs.gametype = GT_CTF;

	Com_sprintf( text, sizeof( text ), "tinfo %i", teamSize );
	for ( i = 0; i < teamSize; i++ ) {
		Q_strcat( text, sizeof( text ), va( " %i 0 %i 0 0 0", SlotClient( i ), 10 + i ) );
	}
	Tokenize( text );
	cgs.serverCommandSequence = 0;
	CG_ExecuteNewServerCommands( 1 );
	Check( numSortedTeamPlayers == teamSize && sortedTeamPlayers[LOCAL_INDEX] == LOCAL_CLIENT, "team overlay" );
}

/** Let the server select the value, then let the cgame pick it up as a frame does. */
static void Select( void ) {
	SystemInfo_Set( "cg_currentSelectedPlayer", selection );
	CG_UpdateCvars();
	Check( !strcmp( cg_currentSelectedPlayer.string, selection ), "systeminfo reached cg_currentSelectedPlayer" );
}

/** "nextOrder" from the console: whether it queued a new order. */
static qboolean NextOrder( qboolean leader ) {
	Select();
	cgs.clientinfo[LOCAL_CLIENT].teamLeader = leader;
	cgs.currentOrder = TEAMTASK_OFFENSE;
	cgs.orderPending = qfalse;
	Tokenize( "nextOrder" );
	Check( CG_ConsoleCommand(), "nextOrder is a console command" );
	Check( cgs.orderPending == ( cgs.currentOrder != TEAMTASK_OFFENSE ), "nextOrder queues what it picks" );
	return cgs.orderPending;
}

/** The selection after next or previous team member. */
static int Step( qboolean next ) {
	Select();
	if ( next ) {
		CG_SelectNextPlayer();
	} else {
		CG_SelectPrevPlayer();
	}
	return cg_currentSelectedPlayer.integer;
}

/** Every consumer of cg_currentSelectedPlayer, with the overlay slot it selects
 * (teamSize for Everyone, NO_SELECTION for values that name nobody). */
static void TestSelection( int selected ) {
	const char *expected;
	char name[MAX_CVAR_VALUE_STRING];

	// someone who is not the leader may only order themselves
	Check( NextOrder( qfalse ) == ( selected == LOCAL_INDEX ), "nextOrder for a non-leader" );
	Check( NextOrder( qtrue ), "nextOrder for the leader" );

	// a pending order goes to the selected teammate, ourselves, or everyone
	Select();
	cgs.currentOrder = TEAMTASK_OFFENSE;
	cgs.orderPending = qtrue;
	sent[0] = 0;
	CG_CheckOrderPending();
	if ( selected == teamSize ) {
		expected = va( "cmd vsay_team %s\n" GESTURE, VOICECHAT_OFFENSE );
	} else if ( selected == LOCAL_INDEX ) {
		expected = va( "teamtask %i\ncmd vsay_team %s\n" GESTURE, TEAMTASK_OFFENSE, VOICECHAT_ONOFFENSE );
	} else if ( selected >= 0 ) {
		expected = va( "cmd vtell %i %s\n" GESTURE, SlotClient( selected ), VOICECHAT_OFFENSE );
	} else {
		expected = GESTURE;
	}
	Check( !strcmp( sent, expected ), "pending order" );
	Check( !cgs.orderPending, "pending order sent" );

	// next and previous team member step through the overlay and Everyone
	Check( Step( qtrue ) == ( selected >= 0 && selected < teamSize ? selected + 1 : 0 ),
		"next team member" );
	Check( Step( qfalse ) == ( selected > 0 && selected < teamSize ? selected - 1 : teamSize ),
		"previous team member" );
	Cvar_VariableStringBuffer( "cg_selectedPlayerName", name, sizeof( name ) );
	Check( !strcmp( name, selected > 0 && selected < teamSize ? "" : "Everyone" ), "previous team member name" );

	// the HUD shows the team overlay for Everyone, else the selected teammate, the
	// first one when the value names nobody
	Select();
	Check( CG_OwnerDrawVisible( CG_SHOW_TEAMINFO ) == ( selected == teamSize ), "CG_SHOW_TEAMINFO" );
	Check( CG_GetValue( CG_SELECTEDPLAYER_HEALTH ) == 10 + ( selected >= 0 && selected < teamSize ? selected : 0 ),
		"selected player health" );
}

/** One team overlay size and selection per process. */
int main( int argc, char **argv ) {
	char *end;
	long value;
	int selected;

	if ( argc != 3 ) {
		fprintf( stderr, "usage: %s <teammates> <cg_currentSelectedPlayer>\n", argv[0] );
		return 2;
	}
	teamSize = atoi( argv[1] );
	selection = argv[2];
	value = strtol( selection, &end, 10 );
	Check( teamSize > LOCAL_INDEX && teamSize <= TEAM_MAXOVERLAY, "an overlay with our slot" );
	Check( *selection && !*end, "a decimal selection" );
	selected = value >= 0 && value <= teamSize ? (int)value : NO_SELECTION;

	dllEntry( FakeSyscall );
	CG_RegisterCvars();
	Serve();
	TestSelection( selected );
	printf( "Team Arena cgame bounds cg_currentSelectedPlayer %s from systeminfo with %i teammates (issue #379)\n",
		selection, teamSize );
	return 0;
}
