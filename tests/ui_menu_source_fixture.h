/* Issue #49: a stand-in for the engine's precompiler (botlib l_precomp) that
 * serves menu sources to the Team Arena UI's menu parser. It reads quoted
 * strings, names, unsigned numbers and one-character punctuation separated by
 * spaces, which is all the fixtures use. Each handle reads its own text;
 * trap_PC_LoadSource opens the texts named with MenuSource_File.
 * Include it after the module source; the test defines Check. */
#include <stdlib.h>
#include <string.h>

#define MENU_SOURCE_FILES 8

static const char *menuSourceCursor[MENU_SOURCE_FILES + 1];
static const char *menuSourceName[MENU_SOURCE_FILES];
static const char *menuSourceText[MENU_SOURCE_FILES];

/** Read text from handle (1..MENU_SOURCE_FILES) from its start. */
static void MenuSource_Serve( int handle, const char *text ) {
	Check( handle > 0 && handle <= MENU_SOURCE_FILES, "source handle" );
	menuSourceCursor[handle] = text;
}

/** Serve text as the file name for trap_PC_LoadSource. */
static void MenuSource_File( int index, const char *name, const char *text ) {
	Check( index >= 0 && index < MENU_SOURCE_FILES, "source file slot" );
	menuSourceName[index] = name;
	menuSourceText[index] = text;
}

/** Open a served file; 0 when it is not served, as for a missing file. */
int trap_PC_LoadSource( const char *filename ) {
	int i;
	for ( i = 0; i < MENU_SOURCE_FILES; i++ ) {
		if ( menuSourceName[i] && !strcmp( menuSourceName[i], filename ) ) {
			MenuSource_Serve( i + 1, menuSourceText[i] );
			return i + 1;
		}
	}
	return 0;
}

/** Close a served file. */
int trap_PC_FreeSource( int handle ) {
	Check( handle > 0 && handle <= MENU_SOURCE_FILES && menuSourceCursor[handle], "open source freed" );
	menuSourceCursor[handle] = NULL;
	return 1;
}

static int MenuSource_IsName( char c ) {
	return c == '_' || c == '/' || c == '.' || ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' )
		|| ( c >= '0' && c <= '9' );
}

/** The precompiler's tokens: quoted strings, names, numbers and punctuation. */
int trap_PC_ReadToken( int handle, pc_token_t *token ) {
	const char *cursor;
	int length = 0;

	Check( handle > 0 && handle <= MENU_SOURCE_FILES && menuSourceCursor[handle], "read from an open source" );
	cursor = menuSourceCursor[handle];
	memset( token, 0, sizeof( *token ) );
	while ( *cursor == ' ' ) {
		cursor++;
	}
	if ( !*cursor ) {
		menuSourceCursor[handle] = cursor;
		return 0;
	}
	if ( *cursor == '"' ) {
		token->type = TT_STRING;
		for ( cursor++; *cursor && *cursor != '"'; cursor++ ) {
			Check( length < MAX_TOKENLENGTH - 1, "token fits" );
			token->string[length++] = *cursor;
		}
		if ( *cursor ) {
			cursor++;
		}
	} else if ( *cursor >= '0' && *cursor <= '9' ) {
		token->type = TT_NUMBER;
		while ( ( *cursor >= '0' && *cursor <= '9' ) || *cursor == '.' ) {
			Check( length < MAX_TOKENLENGTH - 1, "token fits" );
			token->string[length++] = *cursor++;
		}
		token->floatvalue = atof( token->string );
		token->intvalue = atoi( token->string );
	} else if ( MenuSource_IsName( *cursor ) ) {
		token->type = TT_NAME;
		while ( MenuSource_IsName( *cursor ) ) {
			Check( length < MAX_TOKENLENGTH - 1, "token fits" );
			token->string[length++] = *cursor++;
		}
	} else {
		token->type = TT_PUNCTUATION;
		token->string[length++] = *cursor++;
	}
	menuSourceCursor[handle] = cursor;
	return 1;
}

/** Parse errors are reported with the source position. */
int trap_PC_SourceFileAndLine( int handle, char *filename, int *line ) {
	(void)handle;
	strcpy( filename, "fixture.menu" );
	*line = 1;
	return 1;
}
