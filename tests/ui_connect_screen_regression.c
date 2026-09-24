/* Issue #419: the Team Arena connect screen (UI_DrawConnectScreen) is drawn by
 * the native UI. It copied "Connecting to " and the server name into a 256-byte
 * stack buffer with strcpy. cls.servername holds up to MAX_OSPATH - 1 (255)
 * bytes of whatever connect was given, by the console, a config or a downloaded
 * QVM's trap_SendConsoleCommand, so a name of 242 bytes or more overflowed it.
 * The fixture includes the real ui_main.c and draws the real screen in a font
 * whose glyph handles are the characters, so each line of text is read back
 * from the draws it makes (its drop shadow is skipped). */
#include "../code/ui/ui_main.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PREFIX "Connecting to "
#define TEXT_SIZE 256	/* UI_DrawConnectScreen's text[] */
/* where the screen draws each line: yStart is 130 */
#define LOADING_Y 130
#define CONNECTING_Y 178
#define STATE_Y 210
#define MESSAGE_Y 306
#define MOTD_Y 600
#define DOWNLOAD_NAME_Y 266
#define MAX_LINES 32

static uiClientState_t clientState;
static char serverInfo[MAX_INFO_STRING];
static char downloadName[MAX_INFO_VALUE];
static struct {
	float y;
	char text[MAX_INFO_VALUE * 2];
	int length;
} lines[MAX_LINES];
static int numLines;
static qboolean shadow;
static const char *testCase;

/** Fail with the case under test and the first mismatch. */
static void Check( int ok, const char *what ) {
	if ( !ok ) {
		fprintf( stderr, "UI connect screen regression failed (%s): %s\n", testCase, what );
		exit( 1 );
	}
}

/** Fail on engine errors. */
void QDECL Com_Error( int level, const char *error, ... ) {
	(void)level;
	fprintf( stderr, "Unexpected Com_Error: %s\n", error );
	exit( 1 );
}

/** Com_sprintf warns when it cuts a string short; nothing else prints here. */
void QDECL Com_Printf( const char *msg, ... ) {
	(void)msg;
}

/** No menus are loaded, so the screen draws only its own text. */
menuDef_t *Menus_FindByName( const char *p ) {
	Check( !strcmp( p, "Connect" ), "only the Connect menu is looked up" );
	return NULL;
}
void Menu_Paint( menuDef_t *menu, qboolean forcePaint ) {
	(void)menu; (void)forcePaint;
	Check( 0, "no menu is painted" );
}

/** cl_ui.c GetClientState and GetConfigString. */
void trap_GetClientState( uiClientState_t *state ) {
	*state = clientState;
}
int trap_GetConfigString( int index, char *buff, int buffsize ) {
	Check( index == CS_SERVERINFO, "only the serverinfo is read" );
	Q_strncpyz( buff, serverInfo, buffsize );
	return 1;
}

/** The download cvars CL_ParseDownload sets: half of a 2 MB file. */
void trap_Cvar_VariableStringBuffer( const char *var_name, char *buffer, int bufsize ) {
	Check( !strcmp( var_name, "cl_downloadName" ), "only the download name is read as a string" );
	Q_strncpyz( buffer, downloadName, bufsize );
}
float trap_Cvar_VariableValue( const char *var_name ) {
	if ( !strcmp( var_name, "cl_downloadSize" ) ) {
		return 2 * 1024 * 1024;
	}
	if ( !strcmp( var_name, "cl_downloadCount" ) ) {
		return 1024 * 1024;
	}
	Check( !strcmp( var_name, "cl_downloadTime" ), "only the download cvars are read as values" );
	return 0;
}

/** Text_Paint draws a glyph's drop shadow in colorBlack, then the glyph. */
void trap_R_SetColor( const float *rgba ) {
	shadow = rgba == colorBlack;
}
void UI_SetColor( const float *rgba ) {
	(void)rgba;
}
void UI_AdjustFrom640( float *x, float *y, float *w, float *h ) {
	(void)x; (void)y; (void)w; (void)h;
}

/** Each glyph's handle is its character: append it to the line drawn at y. */
void trap_R_DrawStretchPic( float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader ) {
	int i;

	(void)x; (void)w; (void)h; (void)s1; (void)t1; (void)s2; (void)t2;
	if ( shadow ) {
		return;
	}
	Check( hShader > 0 && hShader <= GLYPH_END, "only glyphs are drawn" );
	for ( i = 0; i < numLines && lines[i].y != y; i++ ) {
	}
	if ( i == numLines ) {
		Check( numLines < MAX_LINES, "lines drawn" );
		lines[numLines++].y = y;
	}
	Check( lines[i].length < (int)sizeof( lines[i].text ) - 1, "line length" );
	lines[i].text[lines[i].length++] = (char)hShader;
}

/** The text drawn at y, or NULL. */
static const char *Line( float y ) {
	int i;

	for ( i = 0; i < numLines; i++ ) {
		if ( lines[i].y == y ) {
			return lines[i].text;
		}
	}
	return NULL;
}

/** Draw the screen as the client does while it connects, with each glyph one
 * unit wide at the top of its line. */
static void Draw( void ) {
	fontInfo_t *fonts[] = { &uiInfo.uiDC.Assets.textFont, &uiInfo.uiDC.Assets.smallFont, &uiInfo.uiDC.Assets.bigFont };
	int f, c;

	for ( f = 0; f < 3; f++ ) {
		memset( fonts[f], 0, sizeof( *fonts[f] ) );
		fonts[f]->glyphScale = 1;
		for ( c = 1; c <= GLYPH_END; c++ ) {
			fonts[f]->glyphs[c].xSkip = fonts[f]->glyphs[c].imageWidth = fonts[f]->glyphs[c].imageHeight = 1;
			fonts[f]->glyphs[c].glyph = c;
		}
	}
	memset( lines, 0, sizeof( lines ) );
	numLines = 0;
	UI_DrawConnectScreen( qfalse );
}

/** A server's connect state: challenging, with a map, motd and message. */
static void Serve( const char *servername ) {
	memset( &clientState, 0, sizeof( clientState ) );
	clientState.connState = CA_CHALLENGING;
	clientState.connectPacketCount = 3;
	Q_strncpyz( clientState.servername, servername, sizeof( clientState.servername ) );
	Q_strncpyz( clientState.updateInfoString, "\\motd\\Welcome to the arena", sizeof( clientState.updateInfoString ) );
	Q_strncpyz( clientState.messageString, "Server is full.", sizeof( clientState.messageString ) );
	Q_strncpyz( serverInfo, "\\mapname\\q3dm17\\sv_hostname\\Arena", sizeof( serverInfo ) );
	downloadName[0] = '\0';
}

/** The lines every connecting server shows, besides its name. */
static void CheckOtherLines( int expectedLines, const char *state ) {
	Check( numLines == expectedLines, "the lines drawn" );
	Check( Line( LOADING_Y ) && !strcmp( Line( LOADING_Y ), "Loading q3dm17" ), "the map line" );
	Check( Line( MOTD_Y ) && !strcmp( Line( MOTD_Y ), "Welcome to the arena" ), "the motd line" );
	if ( state ) {
		Check( Line( STATE_Y ) && !strcmp( Line( STATE_Y ), state ), "the state line" );
	}
}

/** A server name of length bytes, such as a long DNS name (63-byte labels)
 * with a port, is drawn after the prefix, cut to the 255 bytes text[] holds. */
static void TestServerName( int length ) {
	char name[sizeof( clientState.servername )];
	char expected[TEXT_SIZE];
	int i;

	Check( length >= 0 && length < (int)sizeof( name ), "a server name length" );
	for ( i = 0; i < length; i++ ) {
		name[i] = i % 64 == 63 ? '.' : 'a' + i % 26;
	}
	name[length] = '\0';
	if ( length > 6 ) {
		memcpy( name + length - 6, ":27960", 6 );
	}
	Serve( name );
	Draw();

	// the retail text whole while it fits, else as much of it as fits
	Q_strncpyz( expected, PREFIX, sizeof( expected ) );
	Q_strncpyz( expected + strlen( PREFIX ), name, sizeof( expected ) - strlen( PREFIX ) );
	Check( strlen( expected ) == ( length + strlen( PREFIX ) < TEXT_SIZE ? length + strlen( PREFIX ) : TEXT_SIZE - 1 ),
		"the expected text" );
	Check( Line( CONNECTING_Y ) && !strcmp( Line( CONNECTING_Y ), expected ), "the server name line" );
	CheckOtherLines( 5, "Awaiting challenge...3" );
	Check( Line( MESSAGE_Y ) && !strcmp( Line( MESSAGE_Y ), "Server is full." ), "the message line" );
}

/** Normal names are drawn byte for byte as retail drew them. */
static void TestNormal( void ) {
	static const char *names[] = { "192.0.2.1:27960", "q3.example.org:27961", NULL };
	int i;

	for ( i = 0; names[i]; i++ ) {
		Serve( names[i] );
		Draw();
		Check( Line( CONNECTING_Y ) && !strcmp( Line( CONNECTING_Y ), va( PREFIX "%s", names[i] ) ), names[i] );
		CheckOtherLines( 5, "Awaiting challenge...3" );
		Check( Line( MESSAGE_Y ) && !strcmp( Line( MESSAGE_Y ), "Server is full." ), "the message line" );
	}

	// a local game starts up, and shows no connect state
	Serve( "localhost" );
	Draw();
	Check( Line( CONNECTING_Y ) && !strcmp( Line( CONNECTING_Y ), "Starting up..." ), "localhost" );
	CheckOtherLines( 4, NULL );
	Check( !Line( STATE_Y ), "no state line for localhost" );
}

/** A download's name, as long as the screen's copy of cl_downloadName holds,
 * is drawn whole with its progress. */
static void TestDownload( void ) {
	int i;

	Serve( "192.0.2.1:27960" );
	clientState.connState = CA_CONNECTED;
	for ( i = 0; i < (int)sizeof( downloadName ) - 1; i++ ) {
		downloadName[i] = i % 64 == 63 ? '/' : 'a' + i % 26;
	}
	downloadName[i] = '\0';
	Draw();
	Check( Line( CONNECTING_Y ) && !strcmp( Line( CONNECTING_Y ), PREFIX "192.0.2.1:27960" ), "the server name line" );
	Check( Line( DOWNLOAD_NAME_Y ) && !strcmp( Line( DOWNLOAD_NAME_Y ), va( "%s (50%%)", downloadName ) ),
		"the download name line" );
}

/** One case per process: ASan stops at the first write past text[]. */
int main( int argc, char **argv ) {
	if ( argc == 3 && !strcmp( argv[1], "servername" ) ) {
		testCase = argv[2];
		TestServerName( atoi( argv[2] ) );
	} else if ( argc == 2 && ( !strcmp( argv[1], "normal" ) || !strcmp( argv[1], "download" ) ) ) {
		testCase = argv[1];
		if ( !strcmp( argv[1], "normal" ) ) {
			TestNormal();
		} else {
			TestDownload();
		}
	} else {
		fprintf( stderr, "usage: %s servername <length> | normal | download\n", argv[0] );
		return 2;
	}
	printf( "Team Arena connect screen draws %s %s inside its text buffer (issue #419)\n", argv[1], argc == 3 ? argv[2] : "" );
	return 0;
}
