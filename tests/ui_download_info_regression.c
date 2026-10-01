/* Issue #429: while the client downloads, the connect screen shows the file's
 * progress, transfer rate and time left (UI_DisplayDownloadInfo, in the Team
 * Arena ui_main.c and the base q3_ui ui_connect.c, both linked natively). They
 * are worked out from cl_downloadSize, the size the server announces in its
 * first block, cl_downloadCount, the bytes it has sent since, and
 * cl_downloadTime, the client's clock when the download began. Retail
 * converted the cvars' floats to int even past an int's range (3 GB); divided
 * by downloadSize/1024, which is 0 for a size of 1-1023 bytes once 4096 bytes
 * have arrived and a second has passed; and multiplied the count by 100, which
 * overflows an int past 21 MB, as did UI_ReadableSize's GB hundredths, the
 * time left's K times seconds and the time left in msec.
 * Built with -DMISSIONPACK the fixture includes the real ui_main.c, else the
 * real ui_connect.c, and draws the real display with the cvars set as the
 * client sets them, under UBSan with no recovery. Each line of text drawn is
 * recorded; both UIs draw the same text, which must be the text master draws
 * wherever master's arithmetic is defined (captured from master's build). */
#ifdef MISSIONPACK
#include "../code/ui/ui_main.c"
#else
#include "../code/q3_ui/ui_connect.c"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DOWNLOAD_NAME "baseq3/arena.pk3"
#define LABELS "Downloading:|Estimated time left:|Transfer rate:|"
#define MAX_LINES 16

typedef struct {
	const char	*size, *count, *time;	/* the cvars, as the client sets them */
	int			realtime;				/* the UI's clock, in msec */
	const char	*expected;				/* the lines drawn after the labels */
} downloadCase_t;

/* the counts: just past the 4096 bytes before an estimate, 22 MB, 3 GB */
#define COUNT_4K "4097"
#define COUNT_22MB "23068672"
#define COUNT_3GB "3221225472"

/* cl_downloadTime and the UI's clock: the client's clock 23 days in, 10
 * seconds after the download began; a download begun 1 second in, 24.8 days
 * ago (the longest the clock allows); and a start past an int's range */
#define TIME_LATE "2000000000", 2000010000
#define TIME_SLOW "1000", 2147483000
#define TIME_FUTURE "3000000000", 10000

static const downloadCase_t cases[] = {
	/* downloads drawn byte for byte as master draws them */
	{ "2097152", "1048576", "0", 10000, DOWNLOAD_NAME " (50%)|estimating|(1024 KB of 2.00 MB copied)" },
	{ "2097152", "1024", "5000", 6000, DOWNLOAD_NAME " (0%)|estimating|(1024 bytes of 2.00 MB copied)" },
	{ "10485760", "5242880", "5000", 15000, DOWNLOAD_NAME " (50%)|10 sec|(5.00 MB of 10.00 MB copied)|512 KB/Sec" },
	{ "31457280", "20971520", "5000", 130000, DOWNLOAD_NAME " (66%)|1 min 3 sec|(20.00 MB of 30.00 MB copied)|163 KB/Sec" },
	{ "104857600", "5242880", "5000", 1005000, DOWNLOAD_NAME " (5%)|5 hr 16 min|(5.00 MB of 100.00 MB copied)|5 KB/Sec" },
	{ "2097152", "8192", "5000", 5999, DOWNLOAD_NAME " (0%)|estimating|(8 KB of 2.00 MB copied)" },
	{ "0", "8192", "5000", 7000, DOWNLOAD_NAME "|estimating|(8 KB copied)|4 KB/Sec" },
	{ "1074790400", "20971520", "5000", 105000, DOWNLOAD_NAME " (1%)|1 hr 23 min|(20.00 MB of 1.00 GB copied)|204 KB/Sec" },
	{ "2048", "4097", "5000", 15000, DOWNLOAD_NAME " (200%)|-5 sec|(4 KB of 2 KB copied)|409 bytes/Sec" }, /* more than announced */

	/* big or slow downloads, on which master overflowed and drew what the
	 * note says: a 1.5 GB file's GB hundredths; 20 MB of 1 GB at 10 KB/s, its
	 * K times seconds left; 100 MB at 40 bytes/s, its msec left */
	{ "1610612736", "1048576", "0", 10000, DOWNLOAD_NAME " (0%)|estimating|(1024 KB of 1.50 GB copied)" }, /* master: 1.-2 GB */
	{ "1073741824", "20971520", "5000", 2102000, DOWNLOAD_NAME " (1%)|29 hr 14 min|(20.00 MB of 1024.00 MB copied)|9 KB/Sec" }, /* master: 30 hr 22 min */
	{ "104857600", "4097", "5000", 107000, DOWNLOAD_NAME " (0%)|728 hr 8 min|(4 KB of 100.00 MB copied)|40 bytes/Sec" }, /* master: -1673629 sec */

	/* the sizes, counts and times of issue #429: rows without a note draw
	 * the text master draws; a note says what master's arithmetic did */
	{ "0", COUNT_4K, "0", 10000, DOWNLOAD_NAME "|estimating|(4 KB of 0 bytes copied)" },
	{ "0", COUNT_4K, TIME_LATE, DOWNLOAD_NAME "|estimating|(4 KB copied)|409 bytes/Sec" },
	{ "0", COUNT_4K, TIME_SLOW, DOWNLOAD_NAME "|estimating|(4 KB copied)" },
	{ "0", COUNT_4K, TIME_FUTURE, DOWNLOAD_NAME "|estimating|(4 KB copied)" }, /* master: float to int, overflows */
	{ "0", COUNT_22MB, "0", 10000, DOWNLOAD_NAME "|estimating|(22.00 MB of 0 bytes copied)" },
	{ "0", COUNT_22MB, TIME_LATE, DOWNLOAD_NAME "|estimating|(22.00 MB copied)|2.19 MB/Sec" },
	{ "0", COUNT_22MB, TIME_SLOW, DOWNLOAD_NAME "|estimating|(22.00 MB copied)|10 bytes/Sec" },
	{ "0", COUNT_22MB, TIME_FUTURE, DOWNLOAD_NAME "|estimating|(22.00 MB copied)|-10 bytes/Sec" }, /* master: float to int, overflows */
	{ "0", COUNT_3GB, "0", 10000, DOWNLOAD_NAME "|estimating|(1.99 GB of 0 bytes copied)" }, /* master: float to int */
	{ "0", COUNT_3GB, TIME_LATE, DOWNLOAD_NAME "|estimating|(1.99 GB copied)|204.79 MB/Sec" }, /* master: float to int */
	{ "0", COUNT_3GB, TIME_SLOW, DOWNLOAD_NAME "|estimating|(1.99 GB copied)|1000 bytes/Sec" }, /* master: float to int */
	{ "0", COUNT_3GB, TIME_FUTURE, DOWNLOAD_NAME "|estimating|(1.99 GB copied)|-1000 bytes/Sec" }, /* master: float to int */
	{ "1", COUNT_4K, "0", 10000, DOWNLOAD_NAME " (409700%)|estimating|(4 KB of 1 bytes copied)" },
	{ "1", COUNT_4K, TIME_LATE, DOWNLOAD_NAME " (409700%)|estimating|(4 KB of 1 bytes copied)|409 bytes/Sec" }, /* master: divides by zero */
	{ "1", COUNT_4K, TIME_SLOW, DOWNLOAD_NAME " (409700%)|estimating|(4 KB of 1 bytes copied)" },
	{ "1", COUNT_4K, TIME_FUTURE, DOWNLOAD_NAME " (409700%)|estimating|(4 KB of 1 bytes copied)" }, /* master: float to int, overflows */
	{ "1", COUNT_22MB, "0", 10000, DOWNLOAD_NAME " (2147483647%)|estimating|(22.00 MB of 1 bytes copied)" }, /* master: overflows */
	{ "1", COUNT_22MB, TIME_LATE, DOWNLOAD_NAME " (2147483647%)|estimating|(22.00 MB of 1 bytes copied)|2.19 MB/Sec" }, /* master: divides by zero, overflows */
	{ "1", COUNT_22MB, TIME_SLOW, DOWNLOAD_NAME " (2147483647%)|estimating|(22.00 MB of 1 bytes copied)|10 bytes/Sec" }, /* master: divides by zero, overflows */
	{ "1", COUNT_22MB, TIME_FUTURE, DOWNLOAD_NAME " (2147483647%)|estimating|(22.00 MB of 1 bytes copied)|-10 bytes/Sec" }, /* master: float to int, divides by zero, overflows */
	{ "1", COUNT_3GB, "0", 10000, DOWNLOAD_NAME " (2147483647%)|estimating|(1.99 GB of 1 bytes copied)" }, /* master: float to int, overflows */
	{ "1", COUNT_3GB, TIME_LATE, DOWNLOAD_NAME " (2147483647%)|estimating|(1.99 GB of 1 bytes copied)|204.79 MB/Sec" }, /* master: float to int, overflows */
	{ "1", COUNT_3GB, TIME_SLOW, DOWNLOAD_NAME " (2147483647%)|estimating|(1.99 GB of 1 bytes copied)|1000 bytes/Sec" }, /* master: float to int, overflows */
	{ "1", COUNT_3GB, TIME_FUTURE, DOWNLOAD_NAME " (2147483647%)|estimating|(1.99 GB of 1 bytes copied)|-1000 bytes/Sec" }, /* master: float to int, overflows */
	{ "1023", COUNT_4K, "0", 10000, DOWNLOAD_NAME " (400%)|estimating|(4 KB of 1023 bytes copied)" },
	{ "1023", COUNT_4K, TIME_LATE, DOWNLOAD_NAME " (400%)|estimating|(4 KB of 1023 bytes copied)|409 bytes/Sec" }, /* master: divides by zero */
	{ "1023", COUNT_4K, TIME_SLOW, DOWNLOAD_NAME " (400%)|estimating|(4 KB of 1023 bytes copied)" },
	{ "1023", COUNT_4K, TIME_FUTURE, DOWNLOAD_NAME " (400%)|estimating|(4 KB of 1023 bytes copied)" }, /* master: float to int, overflows */
	{ "1023", COUNT_22MB, "0", 10000, DOWNLOAD_NAME " (2255002%)|estimating|(22.00 MB of 1023 bytes copied)" }, /* master: overflows */
	{ "1023", COUNT_22MB, TIME_LATE, DOWNLOAD_NAME " (2255002%)|estimating|(22.00 MB of 1023 bytes copied)|2.19 MB/Sec" }, /* master: divides by zero, overflows */
	{ "1023", COUNT_22MB, TIME_SLOW, DOWNLOAD_NAME " (2255002%)|estimating|(22.00 MB of 1023 bytes copied)|10 bytes/Sec" }, /* master: divides by zero, overflows */
	{ "1023", COUNT_22MB, TIME_FUTURE, DOWNLOAD_NAME " (2255002%)|estimating|(22.00 MB of 1023 bytes copied)|-10 bytes/Sec" }, /* master: float to int, divides by zero, overflows */
	{ "1023", COUNT_3GB, "0", 10000, DOWNLOAD_NAME " (209920200%)|estimating|(1.99 GB of 1023 bytes copied)" }, /* master: float to int, overflows */
	{ "1023", COUNT_3GB, TIME_LATE, DOWNLOAD_NAME " (209920200%)|estimating|(1.99 GB of 1023 bytes copied)|204.79 MB/Sec" }, /* master: float to int, overflows */
	{ "1023", COUNT_3GB, TIME_SLOW, DOWNLOAD_NAME " (209920200%)|estimating|(1.99 GB of 1023 bytes copied)|1000 bytes/Sec" }, /* master: float to int, overflows */
	{ "1023", COUNT_3GB, TIME_FUTURE, DOWNLOAD_NAME " (209920200%)|estimating|(1.99 GB of 1023 bytes copied)|-1000 bytes/Sec" }, /* master: float to int, overflows */
	{ "1024", COUNT_4K, "0", 10000, DOWNLOAD_NAME " (400%)|estimating|(4 KB of 1024 bytes copied)" },
	{ "1024", COUNT_4K, TIME_LATE, DOWNLOAD_NAME " (400%)|-6 sec|(4 KB of 1024 bytes copied)|409 bytes/Sec" },
	{ "1024", COUNT_4K, TIME_SLOW, DOWNLOAD_NAME " (400%)|estimating|(4 KB of 1024 bytes copied)" },
	{ "1024", COUNT_4K, TIME_FUTURE, DOWNLOAD_NAME " (400%)|estimating|(4 KB of 1024 bytes copied)" }, /* master: float to int, overflows */
	{ "1024", COUNT_22MB, "0", 10000, DOWNLOAD_NAME " (2252800%)|estimating|(22.00 MB of 1024 bytes copied)" }, /* master: overflows */
	{ "1024", COUNT_22MB, TIME_LATE, DOWNLOAD_NAME " (2252800%)|0 sec|(22.00 MB of 1024 bytes copied)|2.19 MB/Sec" }, /* master: overflows */
	{ "1024", COUNT_22MB, TIME_SLOW, DOWNLOAD_NAME " (2252800%)|-2297754 sec|(22.00 MB of 1024 bytes copied)|10 bytes/Sec" }, /* master: overflows */
	{ "1024", COUNT_22MB, TIME_FUTURE, DOWNLOAD_NAME " (2252800%)|638 hr 15 min|(22.00 MB of 1024 bytes copied)|-10 bytes/Sec" }, /* master: float to int, overflows */
	{ "1024", COUNT_3GB, "0", 10000, DOWNLOAD_NAME " (209715199%)|estimating|(1.99 GB of 1024 bytes copied)" }, /* master: float to int, overflows */
	{ "1024", COUNT_3GB, TIME_LATE, DOWNLOAD_NAME " (209715199%)|0 sec|(1.99 GB of 1024 bytes copied)|204.79 MB/Sec" }, /* master: float to int, overflows */
	{ "1024", COUNT_3GB, TIME_SLOW, DOWNLOAD_NAME " (209715199%)|-2097150 sec|(1.99 GB of 1024 bytes copied)|1000 bytes/Sec" }, /* master: float to int, overflows */
	{ "1024", COUNT_3GB, TIME_FUTURE, DOWNLOAD_NAME " (209715199%)|582 hr 32 min|(1.99 GB of 1024 bytes copied)|-1000 bytes/Sec" }, /* master: float to int, overflows */
};

static const downloadCase_t *test;
static struct {
	float y;
	char text[256];
	int length;
} lines[MAX_LINES];
static int numLines;

/** Fail with the case under test and the first mismatch. */
static void Check( int ok, const char *what ) {
	if ( !ok ) {
		fprintf( stderr, "UI download info regression failed (size %s, count %s, time %s, realtime %d): %s\n",
			test->size, test->count, test->time, test->realtime, what );
		exit( 1 );
	}
}

/** Fail on engine errors. */
void QDECL Com_Error( int level, const char *error, ... ) {
	(void)level;
	fprintf( stderr, "Unexpected Com_Error: %s\n", error );
	exit( 1 );
}

void QDECL Com_Printf( const char *msg, ... ) {
	Check( 0, va( "nothing is printed: %s", msg ) );
}

/** Cvar_VariableValue: the value Cvar_Set parsed from the cvar's string. */
float trap_Cvar_VariableValue( const char *var_name ) {
	if ( !strcmp( var_name, "cl_downloadSize" ) ) {
		return atof( test->size );
	}
	if ( !strcmp( var_name, "cl_downloadCount" ) ) {
		return atof( test->count );
	}
	Check( !strcmp( var_name, "cl_downloadTime" ), "only the download cvars are read" );
	return atof( test->time );
}

/** Start a new line of text. */
static void NewLine( float y ) {
	Check( numLines < MAX_LINES, "the lines drawn" );
	lines[numLines++].y = y;
}

/** Append one character to the last line. */
static void Append( char c ) {
	Check( numLines > 0 && lines[numLines - 1].length < (int)sizeof( lines[0].text ) - 1, "the line's length" );
	lines[numLines - 1].text[lines[numLines - 1].length++] = c;
}

#ifdef MISSIONPACK
static qboolean shadow;

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

/** Each glyph's handle is its character, and every line has its own y. */
void trap_R_DrawStretchPic( float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader ) {
	(void)x; (void)w; (void)h; (void)s1; (void)t1; (void)s2; (void)t2;
	if ( shadow ) {
		return;
	}
	Check( hShader > 0 && hShader <= GLYPH_END, "only glyphs are drawn" );
	if ( !numLines || lines[numLines - 1].y != y ) {
		NewLine( y );
	}
	Append( (char)hShader );
}

/** Draw the display as the connect screen does, in a font whose glyphs are one
 * unit wide at the top of their line. */
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
	uiInfo.uiDC.realTime = test->realtime;
	UI_DisplayDownloadInfo( DOWNLOAD_NAME, 320, 130, 0.5f );
}
#else
vec4_t color_white = { 1, 1, 1, 1 };
uiStatic_t uis;

float UI_ProportionalSizeScale( int style ) {
	Check( style == ( UI_LEFT | UI_SMALLFONT | UI_DROPSHADOW ), "the display's style" );
	return 0.75f;
}
int UI_ProportionalStringWidth( const char *str ) {
	return (int)strlen( str ) * 16;
}

/** Each call draws one line. */
void UI_DrawProportionalString( int x, int y, const char *str, int style, vec4_t color ) {
	(void)x;
	Check( style == ( UI_LEFT | UI_SMALLFONT | UI_DROPSHADOW ) && color == color_white, "the display's style" );
	NewLine( y );
	for ( ; *str; str++ ) {
		Append( *str );
	}
}

/** Draw the display as the connect screen does. */
static void Draw( void ) {
	uis.realtime = test->realtime;
	UI_DisplayDownloadInfo( DOWNLOAD_NAME );
}
#endif

/** Draw one case and compare its lines with the expected ones. */
static void Run( const downloadCase_t *c ) {
	char drawn[MAX_LINES * 256 + 1];
	int i;

	test = c;
	memset( lines, 0, sizeof( lines ) );
	numLines = 0;
	Draw();
	drawn[0] = '\0';
	for ( i = 0; i < numLines; i++ ) {
		Q_strcat( drawn, sizeof( drawn ), va( "%s%s", i ? "|" : "", lines[i].text ) );
	}
	Check( !strcmp( drawn, va( LABELS "%s", c->expected ) ), va( "drew \"%s\"", drawn ) );
}

/** One case per process: UBSan stops at the first undefined operation. */
int main( int argc, char **argv ) {
	int i;

	if ( argc == 2 && !strcmp( argv[1], "count" ) ) {
		printf( "%d\n", (int)( sizeof( cases ) / sizeof( cases[0] ) ) );
		return 0;
	}
	i = argc == 2 ? atoi( argv[1] ) : -1;
	if ( i < 0 || i >= (int)( sizeof( cases ) / sizeof( cases[0] ) ) ) {
		fprintf( stderr, "usage: %s count | <case>\n", argv[0] );
		return 2;
	}
	Run( &cases[i] );
	return 0;
}
