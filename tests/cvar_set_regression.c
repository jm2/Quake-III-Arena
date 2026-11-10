/* Cvar_Set_f (code/qcommon/cvar.c) joins "set NAME WORD..." into a
 * MAX_STRING_TOKENS buffer, whole words only while they fit.  Its length
 * check counted each word one short and no joining space, so one-letter
 * words counted nothing and a long enough line (a module's EXEC_NOW can pass
 * up to BIG_INFO_STRING) overran the stack.  Found by the ETL-OS9 review. */
#include "../code/game/q_shared.h"
#include "../code/qcommon/qcommon.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char		lastName[64], lastValue[4096];
static int		sets;

void QDECL Com_Printf( const char *fmt, ... ) { (void)fmt; }
void QDECL Com_Error( int level, const char *fmt, ... ) { (void)level; fprintf( stderr, "Com_Error: %s\n", fmt ); exit( 1 ); }
cvar_t *Cvar_Set2( const char *name, const char *value, qboolean force ) {
	(void)force;
	Q_strncpyz( lastName, name, sizeof( lastName ) );
	Q_strncpyz( lastValue, value, sizeof( lastValue ) );
	sets++;
	return NULL;
}

#include "cvar_set_extracted.c"

static int failures;
static void Check( int ok, const char *what ) {
	if ( !ok ) {
		fprintf( stderr, "cvar set regression failed: %s (value of %d bytes)\n", what, (int)strlen( lastValue ) );
		failures++;
	}
}

static void Set( const char *line ) {
	sets = 0;
	lastValue[0] = 0;
	Cmd_TokenizeString( line );
	Cvar_Set_f();
}

int main( void ) {
	static char	line[BIG_INFO_STRING];
	int			i;

	Set( "set name some words here" );
	Check( sets == 1 && !strcmp( lastName, "name" ) && !strcmp( lastValue, "some words here" ), "words are joined" );
	Set( "set name \"quoted value\" tail" );
	Check( !strcmp( lastValue, "quoted value tail" ), "a quoted word stays one word" );

	/* 3000 one-letter words: far past the buffer */
	Q_strncpyz( line, "set name", sizeof( line ) );
	for ( i = 0 ; i < 3000 ; i++ ) {
		Q_strcat( line, sizeof( line ), " a" );
	}
	Set( line );
	Check( sets == 1 && strlen( lastValue ) < MAX_STRING_TOKENS, "one-letter words stop at the buffer" );
	Check( strlen( lastValue ) >= MAX_STRING_TOKENS - 3, "and fill it" );

	/* long words: whole words only */
	Q_strncpyz( line, "set name", sizeof( line ) );
	for ( i = 0 ; i < 40 ; i++ ) {
		Q_strcat( line, sizeof( line ), " abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ" );
	}
	Set( line );
	/* as before, words that did not fit leave the space after the last one */
	{
		size_t	n = strlen( lastValue );
		if ( n && lastValue[n - 1] == ' ' ) {
			n--;
		}
		Check( strlen( lastValue ) < MAX_STRING_TOKENS && n % 63 == 62, "long words are kept whole" );
	}

	/* a value that fits exactly is unchanged */
	Q_strncpyz( line, "set name ", sizeof( line ) );
	for ( i = 0 ; i < MAX_STRING_TOKENS - 1 ; i++ ) {
		line[9 + i] = 'x';
	}
	line[9 + MAX_STRING_TOKENS - 1] = 0;
	Set( line );
	Check( strlen( lastValue ) == MAX_STRING_TOKENS - 1, "a 1023-byte word fits" );
	if ( failures ) {
		return 1;
	}
	puts( "cvar set regression passed" );
	return 0;
}
