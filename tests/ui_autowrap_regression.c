/* Issue #411: the base q3_ui and Team Arena menus wrap a server's text (the
 * connect screen's message, a disconnect reason) from a 1024-byte copy. When
 * the text ended with a word too wide for a line and no space before it to cut
 * at, the retail loops read the byte after the copy's terminator; when one space
 * followed such a word, they scanned on from after the terminator and briefly
 * wrote there. For a 1023-byte copy both are past the buffer.
 * The runner extracts the real UI_DrawProportionalString_AutoWrapped (q3_ui
 * ui_atoms.c) and Text_PaintCenter_AutoWrapped (ui_main.c) verbatim; the width
 * and draw functions they call are replaced here: every byte is CHAR_WIDTH
 * wide, and each line drawn is recorded as "y text|". */
#include "../code/game/q_shared.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHAR_WIDTH 8
/* where and how the q3_ui connect and error screens draw the text */
#define DRAW_X 320
#define DRAW_Y 192
#define DRAW_STEP 20
#define STYLE ( UI_CENTER | UI_SMALLFONT | UI_DROPSHADOW )
#define SCALE 0.25f
#define COPY_LENGTH 1023	/* the most the helpers' 1024-byte copy holds */

static vec4_t textColor = { 1, 1, 1, 1 };
static char drawn[32768];
static int drawnLength;
static char expected[32768];
static int expectedLength;
static char text[4096];

/** Fail with a description of the first mismatch. */
static void Check( int ok, const char *what ) {
	if ( !ok ) {
		fprintf( stderr, "UI autowrap regression failed: %s\n", what );
		exit( 1 );
	}
}

void QDECL Com_Error( int level, const char *error, ... ) {
	(void)level;
	fprintf( stderr, "Unexpected Com_Error: %s\n", error );
	exit( 1 );
}

void QDECL Com_Printf( const char *msg, ... ) {
	(void)msg;
}

/** Append one line to a log as "y text|". */
static void Append( char *log, int *length, int size, float y, const char *line ) {
	int n = snprintf( log + *length, size - *length, "%g %s|", y, line );

	Check( n >= 0 && n < size - *length, "the lines fit the log" );
	*length += n;
}

static void Record( float x, float y, const char *line ) {
	Check( x == DRAW_X, "every line is drawn at the caller's x" );
	Append( drawn, &drawnLength, sizeof( drawn ), y, line );
}

/* q3_ui */
float UI_ProportionalSizeScale( int style ) {
	Check( style == STYLE, "q3_ui measures in the caller's style" );
	return 1.0f;
}

int UI_ProportionalStringWidth( const char *str ) {
	return (int)strlen( str ) * CHAR_WIDTH;
}

void UI_DrawProportionalString( int x, int y, const char *str, int style, vec4_t color ) {
	Check( style == STYLE && color == textColor, "q3_ui draws in the caller's style and color" );
	Record( x, y, str );
}

/* Team Arena */
int Text_Width( const char *text, float scale, int limit ) {
	Check( scale == SCALE && limit == 0, "Team Arena measures the whole line at the caller's scale" );
	return (int)strlen( text ) * CHAR_WIDTH;
}

void Text_PaintCenter( float x, float y, float scale, vec4_t color, const char *text, float adjust ) {
	Check( scale == SCALE && color == textColor && adjust == 0, "Team Arena draws at the caller's scale and color" );
	Record( x, y, text );
}

#include "q3ui_autowrapped.c"
#include "ui_autowrapped.c"

static qboolean teamArena;	/* which helper draws: q3_ui's or Team Arena's */

/** Draw the text with the helper, and require the expected lines. */
static void Wrap( const char *name, const char *str, int xmax, const char *lines ) {
	drawnLength = 0;
	drawn[0] = '\0';
	if ( teamArena ) {
		Text_PaintCenter_AutoWrapped( DRAW_X, DRAW_Y, xmax, DRAW_STEP, SCALE, textColor, str, 0 );
	} else {
		UI_DrawProportionalString_AutoWrapped( DRAW_X, DRAW_Y, xmax, DRAW_STEP, str, STYLE, textColor );
	}
	if ( strcmp( drawn, lines ) ) {
		fprintf( stderr, "%s %s (xmax %d) expected:\n%s\ndrew:\n%s\n",
			teamArena ? "Team Arena" : "q3_ui", name, xmax, lines, drawn );
		Check( 0, "the helper drew other lines" );
	}
}

/** Start a list of expected lines. */
static void Expect( void ) {
	expectedLength = 0;
	expected[0] = '\0';
}

static void ExpectLine( int line, const char *str ) {
	Append( expected, &expectedLength, sizeof( expected ), DRAW_Y + line * DRAW_STEP, str );
}

/** Fill text with count copies of c after prefix; return the end of the text. */
static char *Fill( const char *prefix, char c, int count ) {
	int length = strlen( prefix );

	Check( length + count < (int)sizeof( text ), "the text fits" );
	memcpy( text, prefix, length );
	memset( text + length, c, count );
	text[length + count] = '\0';
	return text + length + count;
}

typedef struct {
	const char	*name;
	const char	*text;
	int			xmax;
	const char	*lines;
} wrapCase_t;

/* What master drew for each: text that ends inside the copy, including a word
 * wider than the line, a word exactly as wide as it, leading and repeated
 * spaces, and a single character wider than the line. The real callers pass
 * xmax 600 (errors) and 630 (connect messages). */
static const wrapCase_t lineCases[] = {
	{ "kicked", "Server disconnected - was kicked", 600,
		"192 Server disconnected - was kicked|" },
	{ "kicked narrow", "Server disconnected - was kicked", 80,
		"192 Server|"
		"212 disconnected|"
		"232 - was|"
		"252 kicked|" },
	{ "unpure", "Server disconnected - Unpure client detected. Invalid .PK3 files referenced!", 600,
		"192 Server disconnected - Unpure client detected. Invalid .PK3 files|"
		"212 referenced!|" },
	{ "unpure connect width", "Server disconnected - Unpure client detected. Invalid .PK3 files referenced!", 630,
		"192 Server disconnected - Unpure client detected. Invalid .PK3 files referenced!|" },
	{ "unknown reason", "Server disconnected for unknown reason\n", 80,
		"192 Server|"
		"212 disconnected|"
		"232 for|"
		"252 unknown|"
		"272 reason\n|" },
	{ "userinfo", "Userinfo string length exceeded.  Try removing setu cvars from your config.\n", 600,
		"192 Userinfo string length exceeded.  Try removing setu cvars from your|"
		"212 config.\n|" },
	{ "userinfo narrow", "Userinfo string length exceeded.  Try removing setu cvars from your config.\n", 80,
		"192 Userinfo|"
		"212 string|"
		"232 length|"
		"252 exceeded. |"
		"272 Try|"
		"292 removing|"
		"312 setu cvars|"
		"332 from your|"
		"352 config.\n|" },
	{ "protocol", "Server uses protocol version 68.\n", 630,
		"192 Server uses protocol version 68.\n|" },
	{ "cd key", "Awaiting CD key authorization\n", 80,
		"192 Awaiting|"
		"212 CD key|"
		"232 authorization\n|" },
	{ "exact words", "0123456789 abcdefghij klmnopqrst", 80,
		"192 0123456789|"
		"212 abcdefghij|"
		"232 klmnopqrst|" },
	{ "exact line", "01234 6789", 80,
		"192 01234 6789|" },
	{ "exact word and space", "0123456789 ", 80,
		"192 0123456789|" },
	{ "leading space", " Server is full.\n", 80,
		"192  Server is|"
		"212 full.\n|" },
	{ "repeated spaces", "a  b   c    d", 16,
		"192 a |"
		"212 b |"
		"232  c|"
		"252   |"
		"272 d|" },
	{ "narrow characters", "a b c", 4,
		"192 a|"
		"212 b|"
		"232 c|" },
	{ "narrow spaces", "   ", 4,
		"192  |"
		"212  |" },
	{ "wide last word", "ab cdefghijklmnop", 80,
		"192 ab|"
		"212 cdefghijklmnop|" },
	{ "wide word", "abcdefghijkl", 80,
		"192 abcdefghijkl|" },
};

int main( int argc, char **argv ) {
	const char *mode = argc > 2 ? argv[2] : "";
	char *end;
	int i;

	if ( argc != 3 || ( strcmp( argv[1], "q3_ui" ) && strcmp( argv[1], "ui" ) ) ) {
		fprintf( stderr, "usage: %s q3_ui|ui lines|word|truncated|space|ending|narrow\n", argv[0] );
		return 2;
	}
	teamArena = !strcmp( argv[1], "ui" );
	if ( !strcmp( mode, "lines" ) ) {
		for ( i = 0; i < (int)( sizeof( lineCases ) / sizeof( lineCases[0] ) ); i++ ) {
			Wrap( lineCases[i].name, lineCases[i].text, lineCases[i].xmax, lineCases[i].lines );
		}
	} else if ( !strcmp( mode, "word" ) ) {
		/* one word fills the copy: it is drawn whole, and nothing after it */
		Fill( "", 'w', COPY_LENGTH );
		Expect();
		ExpectLine( 0, text );
		Wrap( "1023-byte word", text, 80, expected );
	} else if ( !strcmp( mode, "truncated" ) ) {
		/* a disconnect reason is up to 4095 bytes; the copy keeps 1023 */
		Fill( "", 'w', COPY_LENGTH );
		Expect();
		ExpectLine( 0, text );
		Fill( "", 'w', sizeof( text ) - 1 );
		Wrap( "4095-byte word", text, 80, expected );
	} else if ( !strcmp( mode, "space" ) ) {
		/* one space ends the copy after a word too wide for the line */
		end = Fill( "", 'w', COPY_LENGTH - 1 );
		Expect();
		ExpectLine( 0, text );
		strcpy( end, " " );
		Wrap( "1022-byte word and a space", text, 80, expected );
		/* the same inside the copy: master also drew the empty rest a line
		 * below, which draws no glyph */
		Expect();
		ExpectLine( 0, "abcdefghijkl" );
		Wrap( "wide word and a space", "abcdefghijkl ", 80, expected );
	} else if ( !strcmp( mode, "ending" ) ) {
		/* a longer disconnect reason whose 1023rd byte is a space */
		Fill( "", 'w', COPY_LENGTH - 23 );
		Expect();
		ExpectLine( 0, "Server" );
		ExpectLine( 1, "disconnected" );
		ExpectLine( 2, "-" );
		ExpectLine( 3, text );
		end = Fill( "Server disconnected - ", 'w', COPY_LENGTH - 23 );
		strcpy( end, " and more of the reason" );
		Wrap( "1023 bytes ending in a space", text, 80, expected );
	} else if ( !strcmp( mode, "narrow" ) ) {
		/* 512 one-byte words, each wider than the line, fill the copy */
		Expect();
		for ( i = 0; i < ( COPY_LENGTH + 1 ) / 2; i++ ) {
			ExpectLine( i, "x" );
			text[i * 2] = 'x';
			text[i * 2 + 1] = ' ';
		}
		text[COPY_LENGTH] = '\0';
		Wrap( "1023 bytes of narrow words", text, 4, expected );
	} else {
		fprintf( stderr, "unknown case %s\n", mode );
		return 2;
	}
	printf( "%s wraps the %s case inside its copy (issue #411)\n", teamArena ? "Team Arena" : "q3_ui", mode );
	return 0;
}
