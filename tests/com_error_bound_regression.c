/* Issue #408: Com_Error formatted its message into com_errorMessage
 * (MAXPRINTMSG, 4096 bytes) with an unbounded vsprintf, and modules and
 * servers control text that reaches it: the cgame's and game's trap_Error
 * (VM_Error from the CG_ERROR and G_ERROR traps, and the CL_CgameError and
 * SV_GameError wrappers), a game's brush model name, and a cvar value of any
 * length through the CG_CVAR_UPDATE and G_CVAR_UPDATE traps' Cvar_Update.
 * This drives the real Com_Error (common.c) through each of those, with the
 * real cvar.c, Com_Printf and zone behind it, with messages of 1 to 10000
 * bytes around the 4095 bytes com_errorMessage holds.  Each message must
 * reach com_errorMessage, the com_errorMessage cvar, the printed ERROR banner,
 * SV_Shutdown's final message and (for ERR_FATAL) Sys_Error byte-identical to
 * the unbounded format when it fits, and cut to its first MAXPRINTMSG - 1
 * bytes when it does not, with no sanitizer report.  On master the first
 * 4096-byte message is a global-buffer-overflow on com_errorMessage. */
#include "../code/qcommon/common.c"
#include "../code/qcommon/vm_local.h"
#include "../code/game/g_public.h"

#include <setjmp.h>
#include <stdarg.h>

/* cl_cgame.c and sv_game.c define these without a header prototype */
void CL_CgameError( const char *string );
void SV_GameError( const char *string );
void SV_SetBrushModel( sharedEntity_t *ent, const char *name );

#define IMAGE_SIZE		16384	/* the QVM image the trap strings live in */
#define TEXT_OFFSET		64
#define LONG_CVAR		"q3_test_long_cvar"
#define BANNER_HEAD		"********************\nERROR: "
#define BANNER_TAIL		"\n********************\n"
#define COUNT(array)	((int)(sizeof(array) / sizeof((array)[0])))

typedef enum {
	VIA_CGAME_WRAPPER,	/* CL_CgameError */
	VIA_GAME_WRAPPER,	/* SV_GameError */
	VIA_CGAME_TRAP,		/* CG_ERROR: VM_Error( VMAS(1) ) on a QVM */
	VIA_GAME_TRAP,		/* G_ERROR: VM_Error( VMAS(1) ) on a QVM */
	VIA_BRUSH_MODEL,	/* G_SET_BRUSH_MODEL: SV_SetBrushModel */
	VIA_CVAR_UPDATE,	/* CG_CVAR_UPDATE, G_CVAR_UPDATE: Cvar_Update */
	VIA_FATAL,			/* ERR_FATAL: Sys_Error gets the message */
	VIA_COUNT
} via_t;

static const char *viaNames[VIA_COUNT] = {
	"CL_CgameError", "SV_GameError", "CG_ERROR trap", "G_ERROR trap",
	"SV_SetBrushModel", "Cvar_Update", "Com_Error ERR_FATAL"
};

static const char *currentCase = "setup";
static int currentLength;
static char printed[MAXPRINTMSG * 2];
static char shutdownMessage[MAXPRINTMSG * 2];
static char fatalMessage[MAXPRINTMSG * 2];
static int purePaksCleared, disconnects, flushes, clientShutdowns, serverShutdowns, fatals;
static int fakeTime;
static vm_t qvm;
static vmCvar_t longCvar;
static sharedEntity_t brushEntity;

static void Check( int ok, const char *message ) {
	if ( !ok ) {
		fprintf( stderr, "Com_Error bound regression failed (%s, %d-byte message): %s\n",
			currentCase, currentLength, message );
		exit( 1 );
	}
}

static void Unexpected( const char *name ) {
	fprintf( stderr, "Com_Error bound regression reached %s (%s)\n", name, currentCase );
	exit( 1 );
}

/* Engine services Com_Error and Com_Printf call on the ERR_DROP and
 * ERR_FATAL paths. */
void FS_PureServerSetLoadedPaks( const char *pakSums, const char *pakNames ) {
	Check( !pakSums[0] && !pakNames[0], "Com_Error clears the pure pak list" );
	purePaksCleared++;
}
int Sys_Milliseconds( void ) {
	return fakeTime += 1000;	/* never a "solid stream of ERR_DROP" */
}
void Sys_Print( const char *msg ) {
	Q_strcat( printed, sizeof( printed ), msg );
}
void CL_Disconnect( qboolean showMainMenu ) { disconnects++; }
void CL_FlushMemory( void ) { flushes++; }
void CL_Shutdown( void ) { clientShutdowns++; }
void SV_Shutdown( char *finalmsg ) {
	Check( strlen( finalmsg ) < sizeof( shutdownMessage ), "final message fits the fixture" );
	Q_strncpyz( shutdownMessage, finalmsg, sizeof( shutdownMessage ) );
	serverShutdowns++;
}
/* The platform's Sys_Error does not return; leave through the frame. */
void QDECL Sys_Error( const char *error, ... ) {
	va_list ap;

	va_start( ap, error );
	vsnprintf( fatalMessage, sizeof( fatalMessage ), error, ap );
	va_end( ap );
	fatals++;
	longjmp( abortframe, 2 );
}
void CL_CDDialog( void ) { Unexpected( __func__ ); }
void CL_ConsolePrint( char *text ) { Unexpected( __func__ ); }
qboolean FS_Initialized( void ) { Unexpected( __func__ ); return qfalse; }
fileHandle_t FS_FOpenFileWrite( const char *qpath ) { Unexpected( __func__ ); return 0; }
void FS_ForceFlush( fileHandle_t f ) { Unexpected( __func__ ); }
int FS_Write( const void *buffer, int len, fileHandle_t f ) { Unexpected( __func__ ); return 0; }
void FS_FCloseFile( fileHandle_t f ) { Unexpected( __func__ ); }
/* SV_SetBrushModel past its name check */
clipHandle_t CM_InlineModel( int index ) { Unexpected( __func__ ); return 0; }
void CM_ModelBounds( clipHandle_t model, vec3_t mins, vec3_t maxs ) { Unexpected( __func__ ); }
void SV_LinkEntity( sharedEntity_t *ent ) { Unexpected( __func__ ); }
#ifndef Com_Memset
void Com_Memset( void *dest, const int val, const size_t count ) { memset( dest, val, count ); }
void Com_Memcpy( void *dest, const void *src, const size_t count ) { memcpy( dest, src, count ); }
#endif

/* Com_Error's format for each path, as the engine calls it. */
static const char *Format( via_t via ) {
	switch ( via ) {
	case VIA_BRUSH_MODEL:
		return "SV_SetBrushModel: %s isn't a brush model";
	case VIA_CVAR_UPDATE:
		return "Cvar_Update: src %s length %d exceeds MAX_CVAR_VALUE_STRING";
	default:
		return "%s";
	}
}

/* The whole message a path formats for an argument of length n. */
static int MessageLength( via_t via, int n ) {
	if ( via == VIA_CVAR_UPDATE ) {
		return n + snprintf( NULL, 0, Format( via ), "", n );
	}
	return n + snprintf( NULL, 0, Format( via ), "" );
}

/* The argument length that makes the message exactly length bytes, or -1. */
static int ArgumentLength( via_t via, int length ) {
	int n, minimum = via == VIA_CVAR_UPDATE ? MAX_CVAR_VALUE_STRING : 1;

	for ( n = length; n >= minimum; n-- ) {
		if ( MessageLength( via, n ) == length ) {
			return n;
		}
	}
	return -1;
}

/* Printable text that differs from byte to byte, so a cut in the wrong
 * place or a copy from the wrong offset shows. */
static void Fill( char *text, int n ) {
	int i;

	for ( i = 0; i < n; i++ ) {
		text[i] = 'a' + ( i * 7 + i / 26 ) % 26;
	}
	text[n] = 0;
}

/* Enter the engine through one path; Com_Error must not return. */
static void Raise( via_t via, const char *text ) {
	char *trapString;

	switch ( via ) {
	case VIA_CGAME_WRAPPER:
		CL_CgameError( text );
		break;
	case VIA_GAME_WRAPPER:
		SV_GameError( text );
		break;
	case VIA_CGAME_TRAP:
	case VIA_GAME_TRAP:
		/* the trap's string lives in the interpreted module's image */
		Check( strlen( text ) < IMAGE_SIZE - TEXT_OFFSET, "trap text fits the QVM image" );
		memset( qvm.dataBase, 0, IMAGE_SIZE );
		strcpy( (char *)qvm.dataBase + TEXT_OFFSET, text );
		qvm.interpretFaulted = qfalse;
		qvm.currentlyInterpreting = qtrue;
		currentVM = &qvm;
		trapString = VM_CheckedArgString( TEXT_OFFSET, qfalse );
		Check( trapString != NULL, "valid trap string" );
		VM_Error( trapString );
		break;
	case VIA_BRUSH_MODEL:
		SV_SetBrushModel( &brushEntity, text );
		break;
	case VIA_CVAR_UPDATE:
		Cvar_Set( LONG_CVAR, text );
		Cvar_Update( &longCvar );
		break;
	case VIA_FATAL:
		Com_Error( ERR_FATAL, "%s", text );
		break;
	default:
		Check( 0, "known path" );
	}
	Check( 0, "Com_Error returned" );
}

/* Require the source prefix of a result and nothing else. */
static void CheckCut( const char *result, const char *source, int length, const char *what ) {
	int expected = length < MAXPRINTMSG - 1 ? length : MAXPRINTMSG - 1;

	Check( (int)strlen( result ) == expected, what );
	Check( !memcmp( result, source, expected ), what );
}

static void RunCase( via_t via, int length ) {
	static char text[IMAGE_SIZE], expected[IMAGE_SIZE], banner[IMAGE_SIZE], final[IMAGE_SIZE];
	int n, fatal = via == VIA_FATAL, jumped;

	n = ArgumentLength( via, length );
	if ( n < 0 ) {
		/* only short messages can be shorter than a path's fixed text */
		Check( length < 1000, "every path reaches this length" );
		return;
	}
	currentCase = viaNames[via];
	currentLength = length;
	Fill( text, n );
	if ( via == VIA_BRUSH_MODEL ) {
		Check( text[0] != '*', "the name is not an inline model" );
	}
	/* What the unbounded vsprintf wrote on master. */
	if ( via == VIA_CVAR_UPDATE ) {
		snprintf( expected, sizeof( expected ), Format( via ), text, n );
	} else {
		snprintf( expected, sizeof( expected ), Format( via ), text );
	}
	Check( (int)strlen( expected ) == length, "fixture message length" );

	printed[0] = shutdownMessage[0] = fatalMessage[0] = 0;
	purePaksCleared = disconnects = flushes = clientShutdowns = serverShutdowns = fatals = 0;
	/* a byte past the old message must not survive into the new one */
	memset( com_errorMessage, 'X', sizeof( com_errorMessage ) );
	jumped = setjmp( abortframe );
	if ( !jumped ) {
		Raise( via, text );
	}
	Check( jumped == ( fatal ? 2 : -1 ), fatal ? "ERR_FATAL reaches Sys_Error" : "ERR_DROP leaves through abortframe" );
	Check( purePaksCleared == 1, "pure paks cleared once" );

	/* com_errorMessage: exact when it fits, else its first MAXPRINTMSG - 1 bytes */
	CheckCut( com_errorMessage, expected, length, "com_errorMessage" );
	if ( length < MAXPRINTMSG ) {
		Check( !strcmp( com_errorMessage, expected ), "a message that fits is unchanged" );
	}
	CheckCut( Cvar_VariableString( "com_errorMessage" ), expected, length, "com_errorMessage cvar" );

	if ( fatal ) {
		/* SV_Shutdown( va( "Server fatal crashed: %s\n", ... ) ), then Sys_Error( "%s", ... ) */
		Check( clientShutdowns == 1 && serverShutdowns == 1 && fatals == 1, "fatal shutdown order" );
		Com_sprintf( final, sizeof( final ), "Server fatal crashed: %s\n", com_errorMessage );
		Check( !strcmp( shutdownMessage, final ), "fatal SV_Shutdown message" );
		CheckCut( fatalMessage, expected, length, "Sys_Error message" );
		Check( printed[0] == 0, "ERR_FATAL prints nothing itself" );
		com_errorEntered = qfalse;	/* Sys_Error would have exited */
		return;
	}

	Check( !com_errorEntered, "ERR_DROP clears com_errorEntered" );
	Check( disconnects == 1 && flushes == 1 && serverShutdowns == 1 && !fatals, "drop shutdown order" );
	/* SV_Shutdown( va( "Server crashed: %s\n", ... ) ) */
	Com_sprintf( final, sizeof( final ), "Server crashed: %s\n", com_errorMessage );
	Check( !strcmp( shutdownMessage, final ), "drop SV_Shutdown message" );
	/* Com_Printf's MAXPRINTMSG buffer cuts the banner cleanly */
	Com_sprintf( banner, sizeof( banner ), BANNER_HEAD "%s" BANNER_TAIL, com_errorMessage );
	CheckCut( printed, banner, (int)strlen( banner ), "printed ERROR banner" );
	if ( via == VIA_CGAME_TRAP || via == VIA_GAME_TRAP ) {
		Check( qvm.interpretFaulted && !qvm.currentlyInterpreting, "trap_Error faults its QVM" );
	}
	if ( via == VIA_CVAR_UPDATE ) {
		Check( !strcmp( Cvar_VariableString( LONG_CVAR ), text ), "the long cvar keeps its value" );
	}
}

int main( void ) {
	/* 4095 is the longest message com_errorMessage holds; 4096 is the first
	 * that overflowed it by its terminator on master. */
	static const int lengths[] = { 1, 1000, MAXPRINTMSG - 2, MAXPRINTMSG - 1,
		MAXPRINTMSG, MAXPRINTMSG + 1, 10000 };
	int i, via;

	Com_InitSmallZoneMemory();
	Com_InitZoneMemory();
	qvm.dataBase = calloc( 1, IMAGE_SIZE );
	Check( qvm.dataBase != NULL, "QVM image allocation" );
	qvm.dataMask = IMAGE_SIZE - 1;
	/* registered short, as a module would; Cvar_Set then makes it long */
	Cvar_Register( &longCvar, LONG_CVAR, "short", 0 );
	Check( !strcmp( longCvar.string, "short" ), "cvar registration" );

	for ( i = 0; i < COUNT( lengths ); i++ ) {
		for ( via = 0; via < VIA_COUNT; via++ ) {
			RunCase( (via_t)via, lengths[i] );
		}
	}
	free( qvm.dataBase );
	puts( "Com_Error bound regressions passed (issue #408)" );
	return 0;
}
