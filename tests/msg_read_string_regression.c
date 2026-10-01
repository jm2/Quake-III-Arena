/* Issue #435: MSG_ReadString, MSG_ReadBigString and MSG_ReadStringLine
 * (msg.c) read on to a string's terminator, or to the end of the message,
 * even when the string fills their buffer, and keep only what fits, so the
 * next read starts after the terminator. They used to stop at a full buffer:
 * the terminator of a string of exactly buffer - 1 chars was read next, as
 * the "bad command byte" a client drops a gamestate with. Every string that
 * retail 1.32c reads whole still reads as it did, with the same '%' and
 * high-ASCII mapping and the same read position: checked against dbe4ddb's
 * readers over random messages, in-band (Huffman) and out-of-band. Drives the
 * real msg.c and huffman.c. */
#include "../code/game/q_shared.h"
#include "../code/qcommon/qcommon.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

cvar_t *cl_shownet;	/* msg.c's delta readers; none runs here */

static const char *scenario = "setup";
static unsigned int seed = 435;

/** Fail with the reader contract that broke. */
static void Check( int ok, const char *message ) {
	if ( !ok ) { fprintf( stderr, "MSG read string regression failed (%s): %s\n", scenario, message ); exit( 1 ); }
}
void QDECL Com_Error( int level, const char *format, ... ) { (void)level; Check( 0, format ); }
void QDECL Com_Printf( const char *format, ... ) { (void)format; }
void QDECL Com_DPrintf( const char *format, ... ) { (void)format; }
void Com_Memcpy( void *out, const void *in, const size_t size ) { memcpy( out, in, size ); }
void Com_Memset( void *out, const int value, const size_t size ) { memset( out, value, size ); }

/** Deterministic LCG, so every failure reproduces. */
static int Random( int range ) {
	seed = seed * 1103515245u + 12345u;
	return (int)( ( seed >> 8 ) % (unsigned int)range );
}

/* dbe4ddb msg.c: retail 1.32c's readers, verbatim but for their names. */
static char *RetailReadString( msg_t *msg ) {
	static char	string[MAX_STRING_CHARS];
	int		l,c;

	l = 0;
	do {
		c = MSG_ReadByte(msg);		// use ReadByte so -1 is out of bounds
		if ( c == -1 || c == 0 ) {
			break;
		}
		// translate all fmt spec to avoid crash bugs
		if ( c == '%' ) {
			c = '.';
		}
		// don't allow higher ascii values
		if ( c > 127 ) {
			c = '.';
		}

		string[l] = c;
		l++;
	} while (l < sizeof(string)-1);

	string[l] = 0;

	return string;
}

static char *RetailReadBigString( msg_t *msg ) {
	static char	string[BIG_INFO_STRING];
	int		l,c;

	l = 0;
	do {
		c = MSG_ReadByte(msg);		// use ReadByte so -1 is out of bounds
		if ( c == -1 || c == 0 ) {
			break;
		}
		// translate all fmt spec to avoid crash bugs
		if ( c == '%' ) {
			c = '.';
		}

		string[l] = c;
		l++;
	} while (l < sizeof(string)-1);

	string[l] = 0;

	return string;
}

static char *RetailReadStringLine( msg_t *msg ) {
	static char	string[MAX_STRING_CHARS];
	int		l,c;

	l = 0;
	do {
		c = MSG_ReadByte(msg);		// use ReadByte so -1 is out of bounds
		if (c == -1 || c == 0 || c == '\n') {
			break;
		}
		// translate all fmt spec to avoid crash bugs
		if ( c == '%' ) {
			c = '.';
		}
		string[l] = c;
		l++;
	} while (l < sizeof(string)-1);

	string[l] = 0;

	return string;
}

typedef struct {
	const char	*name;
	char		*(*read)( msg_t *msg );
	char		*(*retail)( msg_t *msg );
	int			size;		/* its buffer: size - 1 chars fit */
	qboolean	line;		/* '\n' ends a string too */
	qboolean	highDots;	/* chars over 127 read as '.' */
} reader_t;

static const reader_t readers[] = {
	{ "MSG_ReadString", MSG_ReadString, RetailReadString, MAX_STRING_CHARS, qfalse, qtrue },
	{ "MSG_ReadBigString", MSG_ReadBigString, RetailReadBigString, BIG_INFO_STRING, qfalse, qfalse },
	{ "MSG_ReadStringLine", MSG_ReadStringLine, RetailReadStringLine, MAX_STRING_CHARS, qtrue, qfalse }
};
#define NUM_READERS ( (int)( sizeof( readers ) / sizeof( readers[0] ) ) )

static byte written[MAX_MSGLEN];
static msg_t writer;

/** Start a message: in-band ones are Huffman coded, as the netchan sends them. */
static void Begin( qboolean oob ) {
	if ( oob ) {
		MSG_InitOOB( &writer, written, sizeof( written ) );
	} else {
		MSG_Init( &writer, written, sizeof( written ) );
	}
}

/** Read the message from a heap copy sized to it, so a read past its end is an ASan error. */
static void Open( msg_t *msg, qboolean oob ) {
	byte *data;

	Check( !writer.overflowed && writer.cursize > 0, "test message fits" );
	data = malloc( writer.cursize );
	Check( data != NULL, "allocation" );
	memcpy( data, writer.data, writer.cursize );
	memset( msg, 0, sizeof( *msg ) );
	msg->data = data;
	msg->maxsize = msg->cursize = writer.cursize;
	if ( oob ) {
		MSG_BeginReadingOOB( msg );
	} else {
		MSG_BeginReading( msg );
	}
}

/** The char a reader keeps for c. */
static char Mapped( const reader_t *r, int c ) {
	if ( c == '%' || ( c > 127 && r->highDots ) ) {
		return '.';
	}
	return (char)c;
}

/** Lengths around the buffer (size - 1 fills it), each followed by its terminator, a long and another string. */
static void TestBoundaries( void ) {
	static const int offsets[] = { -3, -2, -1, 0, 1, 40 };
	static char text[BIG_INFO_STRING + 64], expected[BIG_INFO_STRING];
	const reader_t *r;
	const char *s;
	char name[128];
	msg_t msg;
	int i, j, c, oob, terminator, length, kept;

	for ( i = 0; i < NUM_READERS; i++ ) {
		r = &readers[i];
		for ( oob = 0; oob < 2; oob++ ) {
			for ( terminator = 0; terminator <= ( r->line ? '\n' : 0 ); terminator += '\n' ) {
				for ( j = 0; j < (int)( sizeof( offsets ) / sizeof( offsets[0] ) ); j++ ) {
					length = r->size + offsets[j];
					Com_sprintf( name, sizeof( name ), "%s, %s, %i chars ended by %s", r->name, oob ? "out-of-band" : "in-band",
						length, terminator ? "'\\n'" : "0" );
					scenario = name;
					/* letters with '%' and a high char; '\n' is only a char to the NUL-ended readers */
					for ( c = 0; c < length; c++ ) {
						text[c] = c % 97 == 5 ? '%' : c % 89 == 7 ? (char)0xe9
							: ( c % 83 == 11 && !r->line ) ? '\n' : 'a' + c % 26;
					}
					kept = length < r->size - 1 ? length : r->size - 1;
					for ( c = 0; c < kept; c++ ) {
						expected[c] = Mapped( r, (byte)text[c] );
					}
					expected[kept] = 0;

					Begin( oob );
					MSG_WriteData( &writer, text, length );
					MSG_WriteByte( &writer, terminator );
					MSG_WriteLong( &writer, 0x435435 );
					MSG_WriteData( &writer, "next", 5 );
					Open( &msg, oob );
					s = r->read( &msg );
					Check( strlen( s ) == (size_t)kept && !strcmp( s, expected ), "keeps what fits, mapped" );
					Check( MSG_ReadLong( &msg ) == 0x435435, "the next read starts after the terminator" );
					Check( !strcmp( r->read( &msg ), "next" ), "the string after it reads whole" );
					Check( oob ? msg.readcount == msg.cursize : msg.readcount <= msg.cursize, "the message is read to its end" );
					free( msg.data );
				}
			}
		}
	}
	puts( "Strings of buffer - 3 to buffer + 40 chars end at their terminator (1021 to 1064, 8189 to 8232)" );
}

/** An unterminated string at the end of an out-of-band message: the reader stops there with what fits. */
static void TestEndOfMessage( void ) {
	static const int offsets[] = { -2, -1, 0, 40 };
	static char text[BIG_INFO_STRING + 64];
	const reader_t *r;
	const char *s;
	char name[128];
	msg_t msg;
	int i, j, length, kept;

	for ( i = 0; i < NUM_READERS; i++ ) {
		r = &readers[i];
		for ( j = 0; j < (int)( sizeof( offsets ) / sizeof( offsets[0] ) ); j++ ) {
			length = r->size + offsets[j];
			Com_sprintf( name, sizeof( name ), "%s, %i chars and no terminator", r->name, length );
			scenario = name;
			memset( text, 'q', length );
			kept = length < r->size - 1 ? length : r->size - 1;
			Begin( qtrue );
			MSG_WriteData( &writer, text, length );
			Open( &msg, qtrue );
			s = r->read( &msg );
			Check( strlen( s ) == (size_t)kept && strspn( s, "q" ) == (size_t)kept, "keeps what fits" );
			Check( msg.readcount > msg.cursize && MSG_ReadByte( &msg ) == -1, "reads to the end of the message" );
			free( msg.data );
		}
	}
	puts( "Unterminated strings stop at the end of the message" );
}

/** The mapping retail applies, spelt out: '%' always, chars over 127 only in MSG_ReadString. */
static void TestMapping( void ) {
	static const char text[] = "100% \xff\x80~ done";
	static const char *expected[NUM_READERS] = { "100. ..~ done", "100. \xff\x80~ done", "100. \xff\x80~ done" };
	msg_t msg;
	int i, oob;

	scenario = "mapping";
	for ( i = 0; i < NUM_READERS; i++ ) {
		for ( oob = 0; oob < 2; oob++ ) {
			Begin( oob );
			MSG_WriteData( &writer, text, sizeof( text ) );
			Open( &msg, oob );
			Check( !strcmp( readers[i].read( &msg ), expected[i] ), readers[i].name );
			free( msg.data );
		}
	}
}

/**
 * Random messages of strings retail reads whole (buffer - 2 chars at most, any
 * char but 0, ended by 0 or, for lines, '\n'): each reader returns what retail's
 * does and leaves the message where retail's does, to its end.
 */
static void TestRetailStrings( void ) {
	static char text[BIG_INFO_STRING], expected[BIG_INFO_STRING];
	const reader_t *r;
	msg_t msg, retail;
	int iteration, oob, count, i, c, length, reads, full = 0;

	scenario = "retail strings";
	for ( iteration = 0; iteration < 3000; iteration++ ) {
		r = &readers[iteration % NUM_READERS];
		oob = ( iteration / NUM_READERS ) % 2;
		Begin( oob );
		count = 1 + Random( 6 );
		for ( i = 0; i < count; i++ ) {
			/* one string near BIG_INFO_STRING at most, to fit the message */
			length = Random( 3 ) || ( i && r->size > MAX_STRING_CHARS ) ? Random( 80 ) : r->size - 2 - Random( 3 );
			for ( c = 0; c < length; c++ ) {
				text[c] = (char)( 1 + Random( 255 ) );
			}
			if ( length == r->size - 2 ) {
				memset( text, 'z', length );	/* no '\n' to split the line */
				full++;
			}
			MSG_WriteData( &writer, text, length );
			MSG_WriteByte( &writer, r->line && Random( 2 ) ? '\n' : 0 );
		}
		Open( &msg, oob );
		Open( &retail, oob );
		for ( reads = 0; reads < 1 + count * 60 && retail.readcount <= retail.cursize; reads++ ) {
			Q_strncpyz( expected, r->retail( &retail ), sizeof( expected ) );
			Check( !strcmp( r->read( &msg ), expected ), r->name );
			Check( msg.readcount == retail.readcount && msg.bit == retail.bit, "read position as retail's" );
		}
		Check( retail.readcount > retail.cursize, "message read to its end" );
		free( msg.data );
		free( retail.data );
	}
	Check( full > 100, "strings retail just reads whole were tested" );
	printf( "Random retail strings: %d messages read as retail 1.32c reads them, %d strings of buffer - 2 chars\n",
		iteration, full );
}

int main( void ) {
	TestBoundaries();
	TestEndOfMessage();
	TestMapping();
	TestRetailStrings();
	puts( "MSG read string regressions passed (issue #435)" );
	return 0;
}
