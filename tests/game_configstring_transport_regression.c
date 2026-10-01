/*
 * Issue #453: a configstring change reaches a client already in the game as
 * the quoted argument of a "cs <index> "<string>"" server command, or, from
 * 1000 characters on, of "bcs0", "bcs1" and "bcs2" chunks (SV_SetConfigstring,
 * code/server/sv_init.c), and a retail 1.32c client rebuilds the string with
 * Cmd_TokenizeString and Cmd_ArgsFrom(2). A '"' in the string ends the quoted
 * argument, and the client reads the rest outside the quotes, where "//" ends
 * the line, a C comment is cut and the words are joined by single spaces. A
 * client that connects later gets the string from the gamestate as it was
 * set (only '%' and bytes above 127 become '.').
 *
 * Cmd_CallVote_f (code/game/g_cmds.c) put quotes in CS_VOTE_STRING itself:
 * a map_restart, kick, clientkick, g_doWarmup, timelimit or fraglimit vote
 * shows the command it executes, whose argument it quotes, and a map vote
 * the nextmap it sets, in quotes. So a retail client in the game showed
 * kick "Bob" as "kick  Bob ", and a "//" or C comment in the argument, which
 * the caller chooses, cut the rest of the vote it showed: kick "Bob // x"
 * read "kick  Bob". The game now shows the vote without those quotes; the
 * command it executes keeps them.
 *
 * No other configstring the game sets from map or client text is misread,
 * and none overflows a retail consumer:
 * - CS_MUSIC, CS_MESSAGE, CS_MOTD, CS_LOCATIONS and the CS_MODELS and
 *   CS_SOUNDS names from map keys are set while the level loads, when
 *   SV_SetConfigstring sends no "cs", and a map_restart sets them to the
 *   strings they hold, which it does not send again; clients read them from
 *   the gamestate.
 * - ClientUserinfoChanged replaces a userinfo that holds a '"' with
 *   "\name\badinfo", so CS_PLAYERS holds none.
 * - CS_TEAMVOTE_STRING is "leader <client number>".
 * The "cs" transport makes a string at most two characters longer ("bcs"
 * never longer), and every retail consumer copies with a bound or draws the
 * string in place.
 *
 * This test links the real g_cmds.c, g_client.c, g_target.c, g_spawn.c,
 * g_utils.c and g_mem.c and sets these configstrings as the server does:
 * worldspawn music and message, g_motd, target_location messages, and
 * target_speaker and mover model and sound names while maps load, from map
 * entity strings parsed by the real G_ParseSpawnVars and G_ParseField; then
 * votes, team votes and userinfo changes in the game; then the same map again
 * on a map_restart. Values hold '"', "//", C comments, tabs and up to 1023
 * characters. Every configstring the game sets goes through copies of
 * retail's "cs"/"bcs" transport and of the retail code that reads it
 * (CG_ConfigStringModified, CG_StartMusic with COM_Parse, CG_NewClientInfo
 * with Info_ValueForKey, CG_DrawVote's and CG_DrawTeamVote's va, CG_DrawStrlen
 * for text drawn in place, and the engine's model and sound name checks), in
 * exact-size buffers under AddressSanitizer. A client must read the string the
 * server set: from "cs" for every string set in the game or on the
 * map_restart, from the gamestate for those set while the level loads. On
 * master a retail client reads every vote with quotes as a different string.
 */
#include "../code/game/g_local.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* the game files do not export these through g_local.h */
void SP_worldspawn( void );
void SP_target_location( gentity_t *self );
void SP_target_speaker( gentity_t *ent );
qboolean G_ParseSpawnVars( void );
void G_ParseField( const char *key, const char *value, gentity_t *ent );
void Cmd_CallVote_f( gentity_t *ent );
void Cmd_CallTeamVote_f( gentity_t *ent );

#define TEST_MAXCLIENTS	4

/* --- the game globals g_main.c would define --- */
level_locals_t	level;
gentity_t		g_entities[MAX_GENTITIES];
static gclient_t	g_clients[MAX_CLIENTS];
vmCvar_t		g_gametype;
vmCvar_t		g_allowVote;
vmCvar_t		g_motd;
vmCvar_t		g_restarted;
vmCvar_t		g_doWarmup;
vmCvar_t		g_debugAlloc;

static const char	*testCase = "setup";

static void Fail( const char *message ) {
	fprintf( stderr, "configstring transport regression failed: %s: %s\n", testCase, message );
	exit( 1 );
}

void QDECL G_Printf( const char *fmt, ... ) {
	(void)fmt;
}
void QDECL G_Error( const char *fmt, ... ) {
	va_list	argptr;

	va_start( argptr, fmt );
	vfprintf( stderr, fmt, argptr );
	va_end( argptr );
	fprintf( stderr, "\n" );
	Fail( "G_Error" );
}
void QDECL G_LogPrintf( const char *fmt, ... ) {
	(void)fmt;
}
void QDECL Com_Error( int level, const char *error, ... ) {
	(void)level;
	fprintf( stderr, "%s\n", error );
	Fail( "Com_Error" );
}
void QDECL Com_Printf( const char *msg, ... ) {
	(void)msg;
}

/*
 * Retail 1.32c's Cmd_TokenizeString and Cmd_ArgsFrom (git show
 * dbe4ddb:code/qcommon/cmd.c), which a retail client runs on each "cs" and
 * "bcs0/1/2" server command.
 */
static int		cmd_argc;
static char		*cmd_argv[MAX_STRING_TOKENS];		// points into cmd_tokenized
static char		cmd_tokenized[BIG_INFO_STRING+MAX_STRING_TOKENS];	// will have 0 bytes inserted

static char *Cmd_Argv( int arg ) {
	if ( (unsigned)arg >= cmd_argc ) {
		return "";
	}
	return cmd_argv[arg];
}

static char *Cmd_ArgsFrom( int arg ) {
	static	char		cmd_args[BIG_INFO_STRING];
	int		i;

	cmd_args[0] = 0;
	if (arg < 0)
		arg = 0;
	for ( i = arg ; i < cmd_argc ; i++ ) {
		strcat( cmd_args, cmd_argv[i] );
		if ( i != cmd_argc-1 ) {
			strcat( cmd_args, " " );
		}
	}

	return cmd_args;
}

static void Cmd_TokenizeString( const char *text_in ) {
	const char	*text;
	char	*textOut;

	// clear previous args
	cmd_argc = 0;

	if ( !text_in ) {
		return;
	}

	text = text_in;
	textOut = cmd_tokenized;

	while ( 1 ) {
		if ( cmd_argc == MAX_STRING_TOKENS ) {
			return;			// this is usually something malicious
		}

		while ( 1 ) {
			// skip whitespace
			while ( *text && *text <= ' ' ) {
				text++;
			}
			if ( !*text ) {
				return;			// all tokens parsed
			}

			// skip // comments
			if ( text[0] == '/' && text[1] == '/' ) {
				return;			// all tokens parsed
			}

			// skip /* */ comments
			if ( text[0] == '/' && text[1] =='*' ) {
				while ( *text && ( text[0] != '*' || text[1] != '/' ) ) {
					text++;
				}
				if ( !*text ) {
					return;		// all tokens parsed
				}
				text += 2;
			} else {
				break;			// we are ready to parse a token
			}
		}

		// handle quoted strings
		if ( *text == '"' ) {
			cmd_argv[cmd_argc] = textOut;
			cmd_argc++;
			text++;
			while ( *text && *text != '"' ) {
				*textOut++ = *text++;
			}
			*textOut++ = 0;
			if ( !*text ) {
				return;		// all tokens parsed
			}
			text++;
			continue;
		}

		// regular token
		cmd_argv[cmd_argc] = textOut;
		cmd_argc++;

		// skip until whitespace, quote, or command
		while ( *text > ' ' ) {
			if ( text[0] == '"' ) {
				break;
			}

			if ( text[0] == '/' && text[1] == '/' ) {
				break;
			}

			// skip /* */ comments
			if ( text[0] == '/' && text[1] =='*' ) {
				break;
			}

			*textOut++ = *text++;
		}

		*textOut++ = 0;

		if ( !*text ) {
			return;		// all tokens parsed
		}
	}
}

/**
 * What a retail client's CL_ConfigstringModified gets for a configstring
 * the server sends: SV_SetConfigstring sends "cs <index> "<string>"", or
 * "bcs0", "bcs1" and "bcs2" chunks of 999 characters from 1000 on, which
 * CL_GetServerCommand joins into "cs <index> "<string>"" again.
 */
static const char *RetailReceivedConfigstring( int index, const char *string ) {
	static char	bigConfigString[BIG_INFO_STRING];
	char		command[MAX_STRING_CHARS * 2], chunk[MAX_STRING_CHARS];
	const int	maxChunkSize = MAX_STRING_CHARS - 24;
	int			sent, remaining;
	const char	*cmd;

	remaining = strlen( string );
	if ( remaining < maxChunkSize ) {
		Com_sprintf( command, sizeof( command ), "cs %i \"%s\"\n", index, string );
		Cmd_TokenizeString( command );
	} else {
		for ( sent = 0 ; remaining > 0 ; sent += maxChunkSize - 1, remaining -= maxChunkSize - 1 ) {
			cmd = sent == 0 ? "bcs0" : remaining < maxChunkSize ? "bcs2" : "bcs1";
			Q_strncpyz( chunk, string + sent, maxChunkSize );
			Com_sprintf( command, sizeof( command ), "%s %i \"%s\"\n", cmd, index, chunk );
			Cmd_TokenizeString( command );
			if ( !strcmp( Cmd_Argv( 0 ), "bcs0" ) ) {
				Com_sprintf( bigConfigString, BIG_INFO_STRING, "cs %s \"%s", Cmd_Argv(1), Cmd_Argv(2) );
			} else {
				if ( strlen( bigConfigString ) + strlen( Cmd_Argv( 2 ) ) + 1 >= BIG_INFO_STRING ) {
					Fail( "bcs exceeded BIG_INFO_STRING" );
				}
				strcat( bigConfigString, Cmd_Argv( 2 ) );
			}
		}
		strcat( bigConfigString, "\"" );
		Cmd_TokenizeString( bigConfigString );
	}
	if ( strcmp( Cmd_Argv( 0 ), "cs" ) || atoi( Cmd_Argv( 1 ) ) != index ) {
		Fail( "the configstring command did not arrive" );
	}
	// get everything after "cs <num>"
	return Cmd_ArgsFrom( 2 );
}

/**
 * What a retail client's CL_ParseGamestate stores for a configstring:
 * SV_SendClientGameState writes it with MSG_WriteBigString, which turns
 * bytes above 127 into '.', and MSG_ReadBigString turns '%' into '.'.
 */
static const char *RetailGamestateConfigstring( const char *string ) {
	static char	received[BIG_INFO_STRING];
	int			i;

	Q_strncpyz( received, string, sizeof( received ) );
	for ( i = 0 ; received[i] ; i++ ) {
		if ( (byte)received[i] > 127 || received[i] == '%' ) {
			received[i] = '.';
		}
	}
	return received;
}

/*
 * Retail 1.32c's COM_ParseExt (git show dbe4ddb:code/game/q_shared.c), which
 * the retail cgame's COM_Parse runs with its own com_token. A quoted token of
 * MAX_TOKEN_CHARS characters writes its terminator past com_token, and an
 * unterminated one leaves the cursor past the string's end.
 */
static	char	retail_com_token[MAX_TOKEN_CHARS];
static	int		retail_com_lines;

static char *RetailSkipWhitespace( char *data, qboolean *hasNewLines ) {
	int c;

	while( (c = *data) <= ' ') {
		if( !c ) {
			return NULL;
		}
		if( c == '\n' ) {
			retail_com_lines++;
			*hasNewLines = qtrue;
		}
		data++;
	}

	return data;
}

static char *RetailCOM_ParseExt( char **data_p, qboolean allowLineBreaks )
{
	int c = 0, len;
	qboolean hasNewLines = qfalse;
	char *data;

	data = *data_p;
	len = 0;
	retail_com_token[0] = 0;

	// make sure incoming data is valid
	if ( !data )
	{
		*data_p = NULL;
		return retail_com_token;
	}

	while ( 1 )
	{
		// skip whitespace
		data = RetailSkipWhitespace( data, &hasNewLines );
		if ( !data )
		{
			*data_p = NULL;
			return retail_com_token;
		}
		if ( hasNewLines && !allowLineBreaks )
		{
			*data_p = data;
			return retail_com_token;
		}

		c = *data;

		// skip double slash comments
		if ( c == '/' && data[1] == '/' )
		{
			data += 2;
			while (*data && *data != '\n') {
				data++;
			}
		}
		// skip /* */ comments
		else if ( c=='/' && data[1] == '*' )
		{
			data += 2;
			while ( *data && ( *data != '*' || data[1] != '/' ) )
			{
				data++;
			}
			if ( *data )
			{
				data += 2;
			}
		}
		else
		{
			break;
		}
	}

	// handle quoted strings
	if (c == '\"')
	{
		data++;
		while (1)
		{
			c = *data++;
			if (c=='\"' || !c)
			{
				retail_com_token[len] = 0;
				*data_p = ( char * ) data;
				return retail_com_token;
			}
			if (len < MAX_TOKEN_CHARS)
			{
				retail_com_token[len] = c;
				len++;
			}
		}
	}

	// parse a regular word
	do
	{
		if (len < MAX_TOKEN_CHARS)
		{
			retail_com_token[len] = c;
			len++;
		}
		data++;
		c = *data;
		if ( c == '\n' )
			retail_com_lines++;
	} while (c>32);

	if (len == MAX_TOKEN_CHARS)
	{
//		Com_Printf ("Token exceeded %i chars, discarded.\n", MAX_TOKEN_CHARS);
		len = 0;
	}
	retail_com_token[len] = 0;

	*data_p = ( char * ) data;
	return retail_com_token;
}

/* Retail 1.32c's Info_ValueForKey (git show dbe4ddb:code/game/q_shared.c). */
static char *RetailInfo_ValueForKey( const char *s, const char *key ) {
	char	pkey[BIG_INFO_KEY];
	static	char value[2][BIG_INFO_VALUE];	// use two buffers so compares
											// work without stomping on each other
	static	int	valueindex = 0;
	char	*o;

	if ( !s || !key ) {
		return "";
	}

	if ( strlen( s ) >= BIG_INFO_STRING ) {
		Com_Error( ERR_DROP, "Info_ValueForKey: oversize infostring" );
	}

	valueindex ^= 1;
	if (*s == '\\')
		s++;
	while (1)
	{
		o = pkey;
		while (*s != '\\')
		{
			if (!*s)
				return "";
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value[valueindex];

		while (*s != '\\' && *s)
		{
			*o++ = *s++;
		}
		*o = 0;

		if (!Q_stricmp (key, pkey) )
			return value[valueindex];

		if (!*s)
			break;
		s++;
	}

	return "";
}

/* Retail 1.32c's va (git show dbe4ddb:code/game/q_shared.c). */
static char * QDECL RetailVa( char *format, ... ) {
	va_list		argptr;
	static char		string[2][32000];	// in case va is called by nested functions
	static int		index = 0;
	char	*buf;

	buf = string[index & 1];
	index++;

	va_start (argptr, format);
	vsprintf (buf, format,argptr);
	va_end (argptr);

	return buf;
}

/* Retail 1.32c's CG_DrawStrlen (git show dbe4ddb:code/cgame/cg_drawtools.c) and its Q_IsColorString. */
#define Retail_IsColorString(p)	( p && *(p) == Q_COLOR_ESCAPE && *((p)+1) && *((p)+1) != Q_COLOR_ESCAPE )

static int CG_DrawStrlen( const char *str ) {
	const char *s = str;
	int count = 0;

	while ( *s ) {
		if ( Retail_IsColorString( s ) ) {
			s += 2;
		} else {
			count++;
			s++;
		}
	}

	return count;
}

/* --- the retail client code that reads each configstring --- */

/* CG_StartMusic (cg_main.c), and S_StartBackgroundTrack's copy of the loop name. */
static void RetailStartMusic( const char *configstring ) {
	char	*s;
	char	parm1[MAX_QPATH], parm2[MAX_QPATH];
	char	backgroundLoop[MAX_QPATH];

	s = (char *)configstring;
	Q_strncpyz( parm1, RetailCOM_ParseExt( &s, qtrue ), sizeof( parm1 ) );
	Q_strncpyz( parm2, RetailCOM_ParseExt( &s, qtrue ), sizeof( parm2 ) );

	Q_strncpyz( backgroundLoop, parm2[0] ? parm2 : parm1, sizeof( backgroundLoop ) );
}

/* The copies of CG_NewClientInfo (cg_players.c) with cg_forceModel 0. */
static void RetailNewClientInfo( const char *configstring ) {
	char		name[MAX_QPATH], redTeam[MAX_TEAMNAME], blueTeam[MAX_TEAMNAME];
	char		modelName[MAX_QPATH], skinName[MAX_QPATH];
	char		headModelName[MAX_QPATH], headSkinName[MAX_QPATH];
	const char	*v;
	char		*slash;

	if ( !configstring[0] ) {
		return;		// player just left
	}

	v = RetailInfo_ValueForKey( configstring, "n" );
	Q_strncpyz( name, v, sizeof( name ) );
	(void)atoi( RetailInfo_ValueForKey( configstring, "c1" ) );
	(void)atoi( RetailInfo_ValueForKey( configstring, "c2" ) );
	(void)atoi( RetailInfo_ValueForKey( configstring, "skill" ) );
	(void)atoi( RetailInfo_ValueForKey( configstring, "hc" ) );
	(void)atoi( RetailInfo_ValueForKey( configstring, "t" ) );

	v = RetailInfo_ValueForKey( configstring, "g_redteam" );
	Q_strncpyz( redTeam, v, MAX_TEAMNAME );
	v = RetailInfo_ValueForKey( configstring, "g_blueteam" );
	Q_strncpyz( blueTeam, v, MAX_TEAMNAME );

	v = RetailInfo_ValueForKey( configstring, "model" );
	Q_strncpyz( modelName, v, sizeof( modelName ) );
	slash = strchr( modelName, '/' );
	if ( !slash ) {
		Q_strncpyz( skinName, "default", sizeof( skinName ) );
	} else {
		Q_strncpyz( skinName, slash + 1, sizeof( skinName ) );
		*slash = 0;
	}

	v = RetailInfo_ValueForKey( configstring, "hmodel" );
	Q_strncpyz( headModelName, v, sizeof( headModelName ) );
	slash = strchr( headModelName, '/' );
	if ( !slash ) {
		Q_strncpyz( headSkinName, "default", sizeof( headSkinName ) );
	} else {
		Q_strncpyz( headSkinName, slash + 1, sizeof( headSkinName ) );
		*slash = 0;
	}
}

/*
 * RE_RegisterModel (tr_model.c) and S_RegisterSound (snd_dma.c) refuse a
 * name of MAX_QPATH characters or more, then copy it into a MAX_QPATH name.
 */
static void RetailRegisterName( const char *name ) {
	char	registered[MAX_QPATH];

	if ( !name[0] || strlen( name ) >= MAX_QPATH ) {
		return;
	}
	strcpy( registered, name );
}

/* the cgs fields CG_ConfigStringModified copies into */
static char	cgsVoteString[MAX_STRING_TOKENS];
static char	cgsTeamVoteString[2][MAX_STRING_TOKENS];

/**
 * A retail client reading one configstring, as CG_Init and
 * CG_ConfigStringModified do. The string is in an exact-size buffer, so
 * AddressSanitizer reports a read past its end.
 */
static void RetailReadConfigstring( int num, const char *received ) {
	char	*str;
	int		n;

	str = malloc( strlen( received ) + 1 );
	if ( !str ) {
		Fail( "out of memory" );
	}
	strcpy( str, received );

	if ( num == CS_MUSIC ) {
		RetailStartMusic( str );
	} else if ( num == CS_MESSAGE || num == CS_MOTD ) {
		// CG_DrawInformation (and CG_DrawOldTourneyScoreboard) draw it in place
		(void)CG_DrawStrlen( str );
	} else if ( num == CS_VOTE_STRING ) {
		Q_strncpyz( cgsVoteString, str, sizeof( cgsVoteString ) );
		(void)RetailVa( "VOTE(%i):%s yes:%i no:%i", 30, cgsVoteString, 1, 0 );
	} else if ( num >= CS_TEAMVOTE_STRING && num <= CS_TEAMVOTE_STRING + 1 ) {
		// retail passes sizeof( cgs.teamVoteString ), the size of both
		// strings, and its Q_strncpyz (strncpy) zero-fills that many bytes,
		// so a blue team vote overruns into the cgs fields that follow
		// whatever its string; this repository's cgame passes the size of
		// one. The string itself must fit the one it goes in.
		n = num - CS_TEAMVOTE_STRING;
		if ( strlen( str ) >= sizeof( cgsTeamVoteString[n] ) ) {
			Fail( "CS_TEAMVOTE_STRING overflows cgs.teamVoteString" );
		}
		Q_strncpyz( cgsTeamVoteString[n], str, sizeof( cgsTeamVoteString[n] ) );
		(void)RetailVa( "TEAMVOTE(%i):%s yes:%i no:%i", 30, cgsTeamVoteString[n], 1, 0 );
	} else if ( num >= CS_PLAYERS && num < CS_PLAYERS + MAX_CLIENTS ) {
		RetailNewClientInfo( str );
	} else if ( num >= CS_LOCATIONS && num < CS_LOCATIONS + MAX_LOCATIONS ) {
		// CG_DrawTeamOverlay measures it, and draws it in place
		(void)CG_DrawStrlen( str );
	} else if ( num >= CS_MODELS && num < CS_MODELS + MAX_MODELS ) {
		RetailRegisterName( str );
	} else if ( num >= CS_SOUNDS && num < CS_SOUNDS + MAX_SOUNDS ) {
		if ( str[0] != '*' ) {
			RetailRegisterName( str );
		}
	}
	// the others set here are numbers, read with atoi, and GAME_VERSION

	free( str );
}

/* --- the server's configstrings, and what a retail client holds --- */

typedef enum {
	SERVER_LOADING,		// SV_SpawnServer: sv.state SS_LOADING, nothing sent
	SERVER_RESTARTING,	// SV_MapRestart_f: sv.restarting, changes sent
	SERVER_GAME			// SS_GAME: changes sent
} serverState_t;

static serverState_t	serverState;
static char		*serverConfigstrings[MAX_CONFIGSTRINGS];
static char		*clientConfigstrings[MAX_CONFIGSTRINGS];
static int		sentConfigstrings[MAX_CONFIGSTRINGS];
static int		unsentMisreads;		// strings set while loading that "cs" would change

static void SetString( char **slot, const char *string ) {
	free( *slot );
	*slot = malloc( strlen( string ) + 1 );
	if ( !*slot ) {
		Fail( "out of memory" );
	}
	strcpy( *slot, string );
}

static const char *ServerConfigstring( int num ) {
	return serverConfigstrings[num] ? serverConfigstrings[num] : "";
}

static void ShowStrings( const char *sent, const char *received ) {
	fprintf( stderr, "set      \"%s\"\nreceived \"%s\"\n", sent, received );
}

/**
 * SV_SetConfigstring. Whatever the server state, the string must reach a
 * retail client through "cs" or "bcs" at most two characters longer and be
 * read safely; while the level loads no "cs" is sent, so it need not arrive
 * as set, but otherwise it must.
 */
void trap_SetConfigstring( int num, const char *string ) {
	const char	*received;

	if ( num < 0 || num >= MAX_CONFIGSTRINGS ) {
		Fail( "bad configstring index" );
	}
	if ( !string ) {
		string = "";
	}
	// don't bother broadcasting an update if no change
	if ( !strcmp( string, ServerConfigstring( num ) ) ) {
		return;
	}
	SetString( &serverConfigstrings[num], string );

	received = RetailReceivedConfigstring( num, string );
	if ( strlen( received ) > strlen( string ) + 2 ) {
		ShowStrings( string, received );
		Fail( "the cs transport made a configstring more than two characters longer" );
	}
	RetailReadConfigstring( num, received );
	if ( serverState == SERVER_LOADING ) {
		if ( strcmp( received, string ) ) {
			unsentMisreads++;
		}
		return;
	}
	if ( strcmp( received, string ) ) {
		fprintf( stderr, "configstring %d\n", num );
		ShowStrings( string, received );
		Fail( "a retail client in the game would receive a different configstring" );
	}
	SetString( &clientConfigstrings[num], received );
	sentConfigstrings[num]++;
}

void trap_GetConfigstring( int num, char *buffer, int bufferSize ) {
	if ( num < 0 || num >= MAX_CONFIGSTRINGS ) {
		Fail( "bad configstring index" );
	}
	Q_strncpyz( buffer, ServerConfigstring( num ), bufferSize );
}

/** SV_SpawnServer: a new level clears every configstring. */
static void ClearConfigstrings( void ) {
	int i;

	for ( i = 0 ; i < MAX_CONFIGSTRINGS ; i++ ) {
		free( serverConfigstrings[i] );
		serverConfigstrings[i] = NULL;
		free( clientConfigstrings[i] );
		clientConfigstrings[i] = NULL;
		sentConfigstrings[i] = 0;
	}
}

/** A retail client connecting: the gamestate gives it every configstring, which it reads. */
static void SendGamestate( void ) {
	const char	*received;
	int			i;

	for ( i = 0 ; i < MAX_CONFIGSTRINGS ; i++ ) {
		if ( !serverConfigstrings[i] ) {
			continue;
		}
		received = RetailGamestateConfigstring( serverConfigstrings[i] );
		if ( strcmp( received, serverConfigstrings[i] ) ) {
			fprintf( stderr, "configstring %d\n", i );
			ShowStrings( serverConfigstrings[i], received );
			Fail( "a retail client would read a different configstring from the gamestate" );
		}
		RetailReadConfigstring( i, received );
		SetString( &clientConfigstrings[i], received );
	}
}

/* --- engine traps --- */

void trap_SendServerCommand( int clientNum, const char *text ) {
	(void)clientNum;
	if ( strncmp( text, "print \"", 7 ) ) {
		fprintf( stderr, "%s\n", text );
		Fail( "unexpected server command" );
	}
}
void trap_Cvar_Set( const char *name, const char *value ) {
	(void)name; (void)value;
}
void trap_LinkEntity( gentity_t *ent ) { ent->r.linked = qtrue; }
void trap_UnlinkEntity( gentity_t *ent ) { ent->r.linked = qfalse; }
void trap_LocateGameData( gentity_t *gEnts, int numGEntities, int sizeofGEntity_t, playerState_t *clients, int sizeofGClient ) {
	(void)gEnts; (void)numGEntities; (void)sizeofGEntity_t; (void)clients; (void)sizeofGClient;
}
void trap_SendConsoleCommand( int exec_when, const char *text ) {
	(void)exec_when; (void)text;
	Fail( "unexpected console command" );
}

static const char	*entityParsePoint;

/** As the server's G_GET_ENTITY_TOKEN: the next COM_Parse token, false at the end of the string. */
qboolean trap_GetEntityToken( char *buffer, int bufferSize ) {
	const char *s = COM_Parse( (char **)&entityParsePoint );

	Q_strncpyz( buffer, s, bufferSize );
	return entityParsePoint || s[0];
}

/* the client command being executed */
static int			testArgc;
static const char	*testArgv[8];

int trap_Argc( void ) {
	return testArgc;
}
void trap_Argv( int n, char *buffer, int bufferLength ) {
	if ( n < 0 || n >= testArgc ) {
		buffer[0] = 0;
		return;
	}
	Q_strncpyz( buffer, testArgv[n], bufferLength );
}

static const char	*nextmapCvar = "";

void trap_Cvar_VariableStringBuffer( const char *var_name, char *buffer, int bufsize ) {
	if ( strcmp( var_name, "nextmap" ) ) {
		Fail( "unexpected cvar read" );
	}
	Q_strncpyz( buffer, nextmapCvar, bufsize );
}

/* target_location_linkup reads "developer" only for more locations than configstrings (issue #461) */
int trap_Cvar_VariableIntegerValue( const char *var_name ) {
	if ( strcmp( var_name, "developer" ) ) {
		Fail( "unexpected cvar read" );
	}
	return 0;
}

static const char	*testUserinfo = "";

void trap_GetUserinfo( int num, char *buffer, int bufferSize ) {
	(void)num;
	Q_strncpyz( buffer, testUserinfo, bufferSize );
}

/* --- helpers --- */

/** Text of the given length: a repeating pattern that starts with prefix. */
static const char *MakeText( char *buffer, int length, const char *prefix ) {
	static const char	pattern[] = "abcdefghij klmnopqrstuvwxyz0123456789";
	int					i, prefixLength = strlen( prefix );

	for ( i = 0 ; i < length ; i++ ) {
		buffer[i] = i < prefixLength ? prefix[i] : pattern[i % ( sizeof( pattern ) - 1 )];
	}
	buffer[length] = 0;
	return buffer;
}

/** Text of the given length with no whitespace, which a map can only give as a bare word. */
static const char *MakeWord( char *buffer, int length, const char *prefix ) {
	int i;

	MakeText( buffer, length, prefix );
	for ( i = 0 ; buffer[i] ; i++ ) {
		if ( buffer[i] == ' ' ) {
			buffer[i] = '_';
		}
	}
	return buffer;
}

/**
 * A key and value of a map entity string. COM_Parse reads a quoted value
 * up to the next '"', and a bare word up to whitespace, so a value with a
 * '"' must be a bare word, without whitespace, that does not start with a
 * comment.
 */
static void AppendPair( char *entityString, int size, const char *key, const char *value ) {
	if ( strchr( value, '"' ) ) {
		if ( strpbrk( value, " \t\n" ) || value[0] == '"' || ( value[0] == '/' && ( value[1] == '/' || value[1] == '*' ) ) ) {
			Fail( "a map value with a quote must be a bare word that COM_Parse reads whole" );
		}
		Q_strcat( entityString, size, va( "\"%s\" %s\n", key, value ) );
	} else {
		Q_strcat( entityString, size, va( "\"%s\" \"%s\"\n", key, value ) );
	}
}

/** G_InitGame's fresh level, at the given time. */
static void ResetLevel( int levelTime ) {
	memset( &level, 0, sizeof( level ) );
	memset( g_entities, 0, sizeof( g_entities ) );
	memset( g_clients, 0, sizeof( g_clients ) );
	G_InitMemory();
	level.gentities = g_entities;
	level.clients = g_clients;
	level.maxclients = TEST_MAXCLIENTS;
	level.num_entities = MAX_CLIENTS;
	level.time = levelTime;
	level.startTime = levelTime;
}

/* --- maps: set while the level loads --- */

#define MAX_TEST_ENTITIES	8

typedef struct {
	const char	*music;
	const char	*message;
	const char	*motd;
	const char	*locations[MAX_TEST_ENTITIES];	// target_location "message"
	const char	*speakers[MAX_TEST_ENTITIES];	// target_speaker "noise"
	const char	*model2s[MAX_TEST_ENTITIES];	// a mover's "model2"
	const char	*moverNoises[MAX_TEST_ENTITIES];	// a mover's "noise"
} testMap_t;

static char	longQuoted[1100], longWord[1100], longWord2[1100], longMotd[300];
static char	boundaryWord[1100], longLocation[1100], longModel[1100];

static testMap_t	testMaps[8];
static int			numTestMaps;

static void BuildTestMaps( void ) {
	testMap_t	*m;

	// the longest entity token G_GET_ENTITY_TOKEN gives is 1023 characters
	MakeText( longQuoted, 1023, "Welcome to " );
	MakeWord( longWord, 1023, "music/" );
	longWord[500] = '"';
	MakeWord( longWord2, 1000, "message" );
	longWord2[10] = '"';
	longWord2[600] = '"';
	MakeText( longMotd, 255, "motd // " );
	// a quote at the end of the first "bcs" chunk and at the start of the second
	MakeWord( boundaryWord, 1010, "x" );
	boundaryWord[998] = '"';
	boundaryWord[999] = '"';
	MakeWord( longLocation, 1000, "Base" );
	longLocation[50] = '"';
	MakeWord( longModel, 1000, "models/mapobjects/" );
	longModel[100] = '"';

	m = &testMaps[numTestMaps++];
	m->music = "music/fla22k_02 music/fla22k_02_loop";
	m->message = "The Longest Yard";
	m->motd = "Welcome";
	m->locations[0] = "Red Base";
	m->locations[1] = "Mega Health";
	m->speakers[0] = "sound/world/klaxon1";
	m->model2s[0] = "models/mapobjects/flag.md3";
	m->moverNoises[0] = "sound/movers/doors/dr1_strt.wav";

	m = &testMaps[numTestMaps++];
	m->music = "music/a\"b.wav";
	m->message = "Bob's\"Arena\"";
	m->motd = "Play // fair /* or */ else";
	m->locations[0] = "Red\"Base";
	m->locations[1] = "Red\\n\"Base\"";		// G_NewString makes the \n a linefeed
	m->locations[2] = "Yellow/*Armor";
	m->speakers[0] = "sound/a\"b";
	m->model2s[0] = "models/a\"b.md3";
	m->moverNoises[0] = "sound/m\"v.wav";

	m = &testMaps[numTestMaps++];
	m->music = "music/x.wav // music/y.wav";
	m->message = "Go // here /* not */ there";
	m->motd = "/* motd */ text";
	m->locations[0] = "Rail // Gun";
	m->locations[1] = "/* Quad */ Damage";
	m->locations[2] = "Tab\there";
	m->speakers[0] = "sound/x // y";
	m->model2s[0] = "models/x /* y */.md3";
	m->moverNoises[0] = "sound/x//y.wav";

	m = &testMaps[numTestMaps++];
	m->music = "a//b\"c/*d";
	m->message = "a/*\"*/\"//";
	m->motd = "";
	m->locations[0] = "a\"/*b";
	m->locations[1] = "*/\"//c";
	m->speakers[0] = "*/\"/*";
	m->model2s[0] = "x/*\"";
	m->moverNoises[0] = "\\\"//";

	m = &testMaps[numTestMaps++];
	m->music = longWord;
	m->message = longQuoted;
	m->motd = longMotd;
	m->locations[0] = longLocation;
	m->locations[1] = longQuoted;
	m->speakers[0] = longWord2;
	m->model2s[0] = longModel;
	m->moverNoises[0] = boundaryWord;

	m = &testMaps[numTestMaps++];
	m->music = boundaryWord;
	m->message = longWord2;
	m->motd = "x";
	m->locations[0] = boundaryWord;
	m->model2s[0] = boundaryWord;
}

/**
 * Load a map as SV_SpawnServer does: G_InitGame spawns the entities from
 * the entity string, then three frames settle, in which the
 * target_locations link up 200 ms after they spawn. On a map_restart the
 * server keeps its configstrings, and sends the ones that change.
 */
static void LoadMap( const testMap_t *m, qboolean restart ) {
	static char	entityString[16384];
	gentity_t	*ent;
	char		*s;
	int			i, j;

	entityString[0] = 0;
	Q_strcat( entityString, sizeof( entityString ), "{\n" );
	AppendPair( entityString, sizeof( entityString ), "classname", "worldspawn" );
	AppendPair( entityString, sizeof( entityString ), "music", m->music );
	AppendPair( entityString, sizeof( entityString ), "message", m->message );
	Q_strcat( entityString, sizeof( entityString ), "}\n" );
	for ( i = 0 ; i < MAX_TEST_ENTITIES ; i++ ) {
		if ( m->locations[i] ) {
			Q_strcat( entityString, sizeof( entityString ), "{\n" );
			AppendPair( entityString, sizeof( entityString ), "classname", "target_location" );
			AppendPair( entityString, sizeof( entityString ), "message", m->locations[i] );
			AppendPair( entityString, sizeof( entityString ), "origin", "0 0 0" );
			Q_strcat( entityString, sizeof( entityString ), "}\n" );
		}
		if ( m->speakers[i] ) {
			Q_strcat( entityString, sizeof( entityString ), "{\n" );
			AppendPair( entityString, sizeof( entityString ), "classname", "target_speaker" );
			AppendPair( entityString, sizeof( entityString ), "noise", m->speakers[i] );
			Q_strcat( entityString, sizeof( entityString ), "}\n" );
		}
		if ( m->model2s[i] || m->moverNoises[i] ) {
			Q_strcat( entityString, sizeof( entityString ), "{\n" );
			AppendPair( entityString, sizeof( entityString ), "classname", "func_static" );
			if ( m->model2s[i] ) {
				AppendPair( entityString, sizeof( entityString ), "model2", m->model2s[i] );
			}
			if ( m->moverNoises[i] ) {
				AppendPair( entityString, sizeof( entityString ), "noise", m->moverNoises[i] );
			}
			Q_strcat( entityString, sizeof( entityString ), "}\n" );
		}
	}

	serverState = restart ? SERVER_RESTARTING : SERVER_LOADING;
	if ( !restart ) {
		ClearConfigstrings();
	}
	ResetLevel( restart ? 7000 : 1000 );
	Q_strncpyz( g_motd.string, m->motd, sizeof( g_motd.string ) );

	// G_SpawnEntitiesFromString
	entityParsePoint = entityString;
	level.spawning = qtrue;
	if ( !G_ParseSpawnVars() ) {
		Fail( "no worldspawn" );
	}
	SP_worldspawn();
	if ( strcmp( ServerConfigstring( CS_MUSIC ), m->music ) || strcmp( ServerConfigstring( CS_MESSAGE ), m->message )
		|| strcmp( ServerConfigstring( CS_MOTD ), m->motd ) ) {
		Fail( "SP_worldspawn did not set the map's music, message and motd" );
	}
	while ( G_ParseSpawnVars() ) {
		ent = G_Spawn();
		for ( j = 0 ; j < level.numSpawnVars ; j++ ) {
			G_ParseField( level.spawnVars[j][0], level.spawnVars[j][1], ent );
		}
		if ( !strcmp( ent->classname, "target_location" ) ) {
			SP_target_location( ent );
		} else if ( !strcmp( ent->classname, "target_speaker" ) ) {
			SP_target_speaker( ent );
		} else {
			// InitMover (g_mover.c)
			if ( ent->model2 ) {
				ent->s.modelindex2 = G_ModelIndex( ent->model2 );
			}
			if ( G_SpawnString( "noise", "100", &s ) ) {
				ent->s.loopSound = G_SoundIndex( s );
			}
		}
	}
	level.spawning = qfalse;

	// the settling frames: G_RunThink at level.time + 200
	level.time += 200;
	for ( i = MAX_CLIENTS ; i < level.num_entities ; i++ ) {
		ent = &g_entities[i];
		if ( ent->inuse && ent->think && ent->nextthink && ent->nextthink <= level.time ) {
			ent->nextthink = 0;
			ent->think( ent );
		}
	}

	if ( !restart ) {
		serverState = SERVER_GAME;
		SendGamestate();
	}
	serverState = SERVER_GAME;
}

static void TestMaps( void ) {
	static char	name[64];
	int			i, j;

	for ( i = 0 ; i < numTestMaps ; i++ ) {
		Com_sprintf( name, sizeof( name ), "map %d", i );
		testCase = name;
		LoadMap( &testMaps[i], qfalse );

		// a map_restart sets the same strings, so it sends none of them
		Com_sprintf( name, sizeof( name ), "map_restart of map %d", i );
		for ( j = 0 ; j < MAX_CONFIGSTRINGS ; j++ ) {
			sentConfigstrings[j] = 0;
		}
		LoadMap( &testMaps[i], qtrue );
		for ( j = 0 ; j < MAX_CONFIGSTRINGS ; j++ ) {
			if ( sentConfigstrings[j] && j != CS_LEVEL_START_TIME ) {
				fprintf( stderr, "configstring %d \"%s\"\n", j, ServerConfigstring( j ) );
				Fail( "a map_restart sent a map's configstring" );
			}
		}
		if ( !sentConfigstrings[CS_LEVEL_START_TIME] ) {
			Fail( "the map_restart did not send the new level start time" );
		}
	}
	// the maps' strings with a quote would be misread if they were sent by "cs"
	if ( unsentMisreads < 20 ) {
		Fail( "too few map strings exercise the cs misparse" );
	}
}

/* --- in the game: votes, team votes and userinfo --- */

/** A level in the game with every slot connected, even slots red and odd slots blue in a team game. */
static void SetupGame( int gametype ) {
	int i;

	g_gametype.integer = gametype;
	g_allowVote.integer = 1;
	for ( i = 0 ; i < TEST_MAXCLIENTS ; i++ ) {
		g_entities[i].s.number = i;
		g_entities[i].inuse = qtrue;
		g_entities[i].client = &g_clients[i];
		g_clients[i].ps.clientNum = i;
		g_clients[i].pers.connected = CON_CONNECTED;
		g_clients[i].sess.sessionTeam = gametype >= GT_TEAM ? ( ( i & 1 ) ? TEAM_BLUE : TEAM_RED ) : TEAM_FREE;
	}
	Q_strncpyz( g_clients[0].pers.netname, "Player", sizeof( g_clients[0].pers.netname ) );
	Q_strncpyz( g_clients[1].pers.netname, "Bob // x", sizeof( g_clients[1].pers.netname ) );
	Q_strncpyz( g_clients[2].pers.netname, "Ann /* y", sizeof( g_clients[2].pers.netname ) );
	Q_strncpyz( g_clients[3].pers.netname, "Tab\tName", sizeof( g_clients[3].pers.netname ) );
}

typedef struct {
	const char	*arg1;
	const char	*arg2;
	const char	*nextmap;
	const char	*executed;	// level.voteString, as master builds it
	const char	*shown;		// CS_VOTE_STRING
} voteCase_t;

static char	longArg[1100], longArgExecuted[1100], longArgShown[1100];
static char	commentArg[1100], commentArgExecuted[1100], commentArgShown[1100];
static char	nearArg[1100], nearArgExecuted[1100], nearArgShown[1100];

static voteCase_t	voteCases[] = {
	// the votes whose argument the game puts in quotes
	{ "map_restart", "", "", "map_restart \"\"", "map_restart " },
	{ "map_restart", "5", "", "map_restart \"5\"", "map_restart 5" },
	{ "kick", "Player", "", "kick \"Player\"", "kick Player" },
	{ "kick", "Bob // x", "", "kick \"Bob // x\"", "kick Bob // x" },
	{ "kick", "Ann /* y", "", "kick \"Ann /* y\"", "kick Ann /* y" },
	{ "kick", "a /* b */ c", "", "kick \"a /* b */ c\"", "kick a /* b */ c" },
	{ "kick", "*/", "", "kick \"*/\"", "kick */" },
	{ "kick", "Tab\tName", "", "kick \"Tab\tName\"", "kick Tab\tName" },
	{ "kick", "two  spaces", "", "kick \"two  spaces\"", "kick two  spaces" },
	{ "clientkick", "3", "", "clientkick \"3\"", "clientkick 3" },
	{ "g_doWarmup", "1", "", "g_doWarmup \"1\"", "g_doWarmup 1" },
	{ "timelimit", "20", "", "timelimit \"20\"", "timelimit 20" },
	{ "fraglimit", "5//0", "", "fraglimit \"5//0\"", "fraglimit 5//0" },
	// a map vote quotes the nextmap it sets
	{ "map", "q3dm17", "vstr d2", "map q3dm17; set nextmap \"vstr d2\"", "map q3dm17; set nextmap vstr d2" },
	{ "map", "q3dm17 // x", "map q3dm1 /* y */", "map q3dm17 // x; set nextmap \"map q3dm1 /* y */\"",
		"map q3dm17 // x; set nextmap map q3dm1 /* y */" },
	// the votes without quotes are shown as before
	{ "map", "q3dm17", "", "map q3dm17", "map q3dm17" },
	{ "map", "q3dm17 // x", "", "map q3dm17 // x", "map q3dm17 // x" },
	{ "nextmap", "", "vstr d2", "vstr nextmap", "vstr nextmap" },
	{ "g_gametype", "3", "", "g_gametype 3", "g_gametype Team Deathmatch" },
	// long arguments: filled in by BuildVoteCases
	{ "kick", longArg, "", longArgExecuted, longArgShown },
	{ "kick", commentArg, "", commentArgExecuted, commentArgShown },
	{ "kick", nearArg, "", nearArgExecuted, nearArgShown },
};
#define NUM_VOTE_CASES	( sizeof( voteCases ) / sizeof( voteCases[0] ) )

static void BuildVoteCases( void ) {
	// "kick <1000>" goes in "bcs" chunks; "kick <992>" just fits a "cs"
	// without the quotes, and with them
	MakeText( longArg, 1000, "Long" );
	// "kick " and the argument's characters 0 to 993 fill the first chunk
	MakeText( commentArg, 1000, "a // b /* c" );
	commentArg[993] = '/';
	commentArg[994] = '/';
	commentArg[995] = '/';
	commentArg[996] = '*';
	MakeText( nearArg, 992, "Near // " );
	Com_sprintf( longArgExecuted, sizeof( longArgExecuted ), "kick \"%s\"", longArg );
	Com_sprintf( longArgShown, sizeof( longArgShown ), "kick %s", longArg );
	Com_sprintf( commentArgExecuted, sizeof( commentArgExecuted ), "kick \"%s\"", commentArg );
	Com_sprintf( commentArgShown, sizeof( commentArgShown ), "kick %s", commentArg );
	Com_sprintf( nearArgExecuted, sizeof( nearArgExecuted ), "kick \"%s\"", nearArg );
	Com_sprintf( nearArgShown, sizeof( nearArgShown ), "kick %s", nearArg );
}

static void TestVotes( int gametype ) {
	static char	name[128];
	gentity_t	*ent;
	size_t		c;

	for ( c = 0 ; c < NUM_VOTE_CASES ; c++ ) {
		Com_sprintf( name, sizeof( name ), "gametype %d callvote %s with a %d-character argument (case %d)",
			gametype, voteCases[c].arg1, (int)strlen( voteCases[c].arg2 ), (int)c );
		testCase = name;
		SetupGame( gametype );
		ent = &g_entities[c % TEST_MAXCLIENTS];
		ent->client->pers.voteCount = 0;
		level.voteTime = 0;
		level.voteExecuteTime = 0;
		nextmapCvar = voteCases[c].nextmap;
		testArgv[0] = "callvote";
		testArgv[1] = voteCases[c].arg1;
		testArgv[2] = voteCases[c].arg2;
		testArgc = 3;

		Cmd_CallVote_f( ent );

		if ( level.voteTime != level.time ) {
			Fail( "the vote was not called" );
		}
		if ( strcmp( level.voteString, voteCases[c].executed ) ) {
			fprintf( stderr, "executes \"%s\"\nexpected \"%s\"\n", level.voteString, voteCases[c].executed );
			Fail( "the vote executes a different command" );
		}
		// trap_SetConfigstring checked that a retail client in the game reads it as set
		if ( strcmp( ServerConfigstring( CS_VOTE_STRING ), voteCases[c].shown ) ) {
			ShowStrings( ServerConfigstring( CS_VOTE_STRING ), voteCases[c].shown );
			Fail( "CS_VOTE_STRING is not the vote shown" );
		}
		// and one that connects now reads it from the gamestate the same way
		SendGamestate();
		if ( strcmp( clientConfigstrings[CS_VOTE_STRING], voteCases[c].shown ) ) {
			Fail( "a connecting client reads a different CS_VOTE_STRING" );
		}
	}
}

typedef struct {
	int			caller;
	const char	*arg1;
	const char	*args[3];
	int			leader;
} teamVoteCase_t;

static const teamVoteCase_t	teamVoteCases[] = {
	{ 0, "leader", { NULL }, 0 },
	{ 1, "leader", { NULL }, 1 },
	{ 0, "LeAdEr", { "2" }, 2 },
	{ 1, "leader", { "3" }, 3 },
	{ 1, "leader", { "Bob", "//", "x" }, 1 },
	{ 0, "leader", { "Ann /* y" }, 2 },
	{ 1, "leader", { "Tab\tName" }, 3 },
};
#define NUM_TEAM_VOTE_CASES	( sizeof( teamVoteCases ) / sizeof( teamVoteCases[0] ) )

static void TestTeamVotes( void ) {
	static char	name[128];
	char		expected[32];
	gentity_t	*ent;
	size_t		c;
	int			i, cs_offset;

	for ( c = 0 ; c < NUM_TEAM_VOTE_CASES ; c++ ) {
		Com_sprintf( name, sizeof( name ), "callteamvote case %d", (int)c );
		testCase = name;
		SetupGame( GT_TEAM );
		ent = &g_entities[teamVoteCases[c].caller];
		cs_offset = ent->client->sess.sessionTeam == TEAM_RED ? 0 : 1;
		ent->client->pers.teamVoteCount = 0;
		level.teamVoteTime[cs_offset] = 0;
		testArgv[0] = "callteamvote";
		testArgv[1] = teamVoteCases[c].arg1;
		testArgc = 2;
		for ( i = 0 ; i < 3 && teamVoteCases[c].args[i] ; i++ ) {
			testArgv[testArgc++] = teamVoteCases[c].args[i];
		}

		Cmd_CallTeamVote_f( ent );

		Com_sprintf( expected, sizeof( expected ), "%s %d", teamVoteCases[c].arg1, teamVoteCases[c].leader );
		if ( strcmp( ServerConfigstring( CS_TEAMVOTE_STRING + cs_offset ), expected ) ) {
			ShowStrings( ServerConfigstring( CS_TEAMVOTE_STRING + cs_offset ), expected );
			Fail( "CS_TEAMVOTE_STRING is not the team vote" );
		}
	}
}

static char	longColorInfo[1100], longTeamInfo[1100];

static const char	*userinfos[] = {
	"\\name\\Player\\model\\sarge/default\\headmodel\\sarge/default\\team_model\\james\\team_headmodel\\*james"
		"\\color1\\4\\color2\\5\\handicap\\100\\teamtask\\0\\g_redteam\\Stroggs\\g_blueteam\\Pagans",
	"\\name\\Bob // x\\model\\sarge/*red\\headmodel\\//\\team_model\\a/*b\\team_headmodel\\*/"
		"\\color1\\/*4*/\\color2\\//5\\g_redteam\\//red\\g_blueteam\\/*blue",
	"\\name\\Tab\tName\\model\\visor\\color1\\  4  \\g_redteam\\Red  Team",
	// a userinfo with a '"', which ClientUserinfoChanged replaces
	"\\name\\Bob\"x\\model\\sarge\"/*\\color1\\\"//",
	longColorInfo,
	longTeamInfo,
};
#define NUM_USERINFOS	( sizeof( userinfos ) / sizeof( userinfos[0] ) )

static void TestUserinfo( int gametype ) {
	static char	name[128];
	char		text[1100];
	size_t		u;
	int			clientNum;

	// a CS_PLAYERS string of 1000 characters or more goes in "bcs" chunks
	Com_sprintf( longColorInfo, sizeof( longColorInfo ), "\\name\\Long\\model\\sarge\\color1\\%s\\color2\\%s",
		MakeWord( text, 600, "1/*" ), "//" );
	Com_sprintf( longTeamInfo, sizeof( longTeamInfo ), "\\name\\Long\\g_redteam\\%s", MakeText( text, 990, "red // " ) );

	for ( u = 0 ; u < NUM_USERINFOS ; u++ ) {
		for ( clientNum = 0 ; clientNum < TEST_MAXCLIENTS ; clientNum++ ) {
			Com_sprintf( name, sizeof( name ), "gametype %d userinfo %d of client %d", gametype, (int)u, clientNum );
			testCase = name;
			SetupGame( gametype );
			testUserinfo = userinfos[u];

			ClientUserinfoChanged( clientNum );

			if ( strchr( ServerConfigstring( CS_PLAYERS + clientNum ), '"' ) ) {
				Fail( "CS_PLAYERS holds a quote" );
			}
			if ( strchr( userinfos[u], '"' ) && strncmp( ServerConfigstring( CS_PLAYERS + clientNum ), "n\\badinfo\\", 10 ) ) {
				Fail( "a userinfo with a quote was not replaced" );
			}
		}
	}
}

int main( void ) {
	BuildTestMaps();
	BuildVoteCases();

	TestMaps();
	// in the game, on the last map
	TestVotes( GT_FFA );
	TestVotes( GT_TEAM );
	TestTeamVotes();
	TestUserinfo( GT_FFA );
	TestUserinfo( GT_TEAM );

#ifdef MISSIONPACK
	printf( "configstrings the game sets from map and client text reach a retail client as set (issue #453, Team Arena)\n" );
#else
	printf( "configstrings the game sets from map and client text reach a retail client as set (issue #453)\n" );
#endif
	return 0;
}
