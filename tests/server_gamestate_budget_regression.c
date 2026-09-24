/* Issue #345: the pure pak lists must not make the gamestate too big for a
 * client. Drives the real SV_Init, SV_SpawnServer and SV_SendClientGameState
 * (sv_init.c, sv_client.c) with the real cvar, message and Huffman code over
 * levels of every size, from a few configstrings to more than a client
 * takes, and pk3 counts from none to more than BIG_INFO_STRING holds.
 * A level whose gamestate is roomy keeps the systeminfo master builds, byte
 * for byte; one whose pure lists would not fit loses sv_pakNames and then
 * sv_paks, each with a warning; the server never sends a gamestate over
 * MAX_GAMESTATE_CHARS or one message, or with a configstring retail's
 * MSG_ReadBigString can't end, and refuses one cleanly instead, at the
 * first char and byte over. Every gamestate sent is written to argv[1]
 * for the real client parser (client_gamestate_budget_regression.c). */
#include "../code/server/server.h"
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RESERVE		512		/* SV_RemainingGameState keeps this much back */

serverStatic_t svs;
server_t sv;
vm_t *gvm;
cvar_t *sv_fps, *sv_timeout, *sv_zombietime, *sv_rconPassword, *sv_privatePassword, *sv_allowDownload;
cvar_t *sv_maxclients, *sv_privateClients, *sv_hostname, *sv_master[MAX_MASTER_SERVERS], *sv_reconnectlimit;
cvar_t *sv_showloss, *sv_padPackets, *sv_killserver, *sv_mapname, *sv_mapChecksum, *sv_serverid, *sv_maxRate;
cvar_t *sv_minPing, *sv_maxPing, *sv_gametype, *sv_pure, *sv_floodProtect, *sv_lanForceRate, *sv_strictAuth;
cvar_t *com_dedicated;
int com_frameTime;
void SV_SendClientGameState( client_t *client );	/* sv_client.c, no header prototype */

/* The level SV_SpawnServer loads: its pk3s and what the game puts in the gamestate. */
typedef struct {
	const char	*game;		/* BASEGAME, or a mod whose pk3s are all referenced */
	const char	*family;
	int			paks, fill, entities;
	char		pad;		/* configstring text: '\0' for model paths, else this one char */
} level_t;

static level_t level;
static sharedEntity_t gentities[MAX_GENTITIES];
static char printed[1 << 16], oob[MAX_STRING_CHARS], disconnect[MAX_STRING_CHARS];
static char loadedSums[BIG_INFO_STRING], loadedNames[BIG_INFO_STRING];
static char referencedSums[BIG_INFO_STRING], referencedNames[BIG_INFO_STRING];
static size_t printedLength;
static int sends, sentBytes, expectDrop, nameExtra;
static void *snapshotEntities;
static jmp_buf dropJump;
static FILE *out;

/** Stop with the level that broke a contract. */
static void Check( int ok, const char *message ) {
	if ( !ok ) {
		fprintf( stderr, "Server gamestate budget regression failed (%s, %d pk3s, %d chars, %d entities): %s\n",
			level.family, level.paks, level.fill, level.entities, message );
		exit( 1 );
	}
}
/** ERR_DROP returns to the caller, as Com_Error does to Com_Frame; nothing else is expected. */
void QDECL Com_Error( int code, const char *fmt, ... ) {
	char text[1024]; va_list ap;
	va_start( ap, fmt ); vsnprintf( text, sizeof(text), fmt, ap ); va_end( ap );
	if ( !expectDrop || code != ERR_DROP ) { fprintf( stderr, "Com_Error(%d): %s\n", code, text ); Check( 0, "engine error" ); }
	Q_strncpyz( disconnect, text, sizeof(disconnect) );
	longjmp( dropJump, 1 );
}
/** Keep the console, for the warnings. */
void QDECL Com_Printf( const char *fmt, ... ) {
	va_list ap; int n;
	va_start( ap, fmt ); n = vsnprintf( printed + printedLength, sizeof(printed) - printedLength, fmt, ap ); va_end( ap );
	Check( n >= 0 && (size_t)n < sizeof(printed) - printedLength, "console capture capacity" );
	printedLength += n;
}
void QDECL Com_DPrintf( const char *fmt, ... ) { (void)fmt; }
void Com_Memset( void *dest, const int val, const size_t count ) { memset( dest, val, count ); }
void Com_Memcpy( void *dest, const void *src, const size_t count ) { memcpy( dest, src, count ); }
int Com_Milliseconds( void ) { return 345000; }
void *Z_Malloc( int size ) { void *p = calloc( 1, size ); Check( p != NULL, "allocation" ); return p; }
void Z_Free( void *ptr ) { free( ptr ); }
char *CopyString( const char *in ) { return strcpy( Z_Malloc( strlen( in ) + 1 ), in ); }
/** Only the snapshot entities come from the hunk; keep one block for every level. */
void *Hunk_Alloc( int size, ha_pref preference ) {
	static int allocated;
	(void)preference;
	if ( size > allocated ) { free( snapshotEntities ); snapshotEntities = Z_Malloc( size ); allocated = size; }
	return snapshotEntities;
}
void *Hunk_AllocateTempMemory( int size ) { return Z_Malloc( size ); }
void Hunk_FreeTempMemory( void *buf ) { free( buf ); }
void Hunk_Clear( void ) {}
void Hunk_SetMark( void ) {}
void CL_MapLoading( void ) {}
void CL_ShutdownAll( void ) {}
void CM_ClearMap( void ) {}
void CM_LoadMap( const char *name, qboolean clientload, int *checksum ) { (void)name; (void)clientload; *checksum = 0x345; }
void FS_Restart( int checksumFeed ) { (void)checksumFeed; }
void FS_ClearPakReferences( int flags ) { (void)flags; }
int FS_FOpenFileRead( const char *qpath, fileHandle_t *file, qboolean uniqueFILE ) { (void)qpath; (void)uniqueFILE; *file = 0; return -1; }
void FS_FCloseFile( fileHandle_t f ) { (void)f; }
void SV_AddOperatorCommands( void ) {}
void SV_BotInitCvars( void ) {}
void SV_BotInitBotLib( void ) {}
void SV_BotFrame( int time ) { (void)time; }
void SV_BotFreeClient( int clientNum ) { (void)clientNum; }
void SV_ClearWorld( void ) {}
void SV_ShutdownGameProgs( void ) {}
void SV_Heartbeat_f( void ) {}
void SV_Netchan_FreeQueue( client_t *client ) { (void)client; }
sharedEntity_t *SV_GentityNum( int num ) { return &gentities[num]; }
int VM_CallArgs( vm_t *vm, int callNum, const int *args, int argCount ) { (void)vm; (void)callNum; (void)args; (void)argCount; return 0; }
char *VM_CheckedExplicitString( vm_t *vm, int value, qboolean nullable ) { (void)vm; (void)value; (void)nullable; return NULL; }
qboolean NET_CompareAdr( netadr_t a, netadr_t b ) { return a.type == b.type && !memcmp( a.ip, b.ip, 4 ) && a.port == b.port; }
/* The client commands (sv_client.c's ucmds table) stay linked under ASan; none runs here. */
static void Unreached( void ) { Check( 0, "no client command runs" ); }
int Cmd_Argc( void ) { Unreached(); return 0; }
char *Cmd_Argv( int arg ) { (void)arg; Unreached(); return ""; }
void Cmd_TokenizeString( const char *text ) { (void)text; Unreached(); }
int FS_FileIsInPAK( const char *name, int *sum ) { (void)name; (void)sum; Unreached(); return -1; }
const char *FS_LoadedPakPureChecksums( void ) { Unreached(); return ""; }
const char *NET_AdrToString( netadr_t a ) { (void)a; Unreached(); return ""; }
qboolean NET_IsLocalAddress( netadr_t adr ) { (void)adr; Unreached(); return qfalse; }
qboolean Sys_IsLANAddress( netadr_t adr ) { (void)adr; Unreached(); return qfalse; }
void SV_SendClientSnapshot( client_t *client ) { (void)client; Unreached(); }
/** The out-of-band reason a refused client is shown. */
void QDECL NET_OutOfBandPrint( netsrc_t sock, netadr_t adr, const char *fmt, ... ) {
	va_list ap; (void)sock; (void)adr;
	va_start( ap, fmt ); vsnprintf( oob, sizeof(oob), fmt, ap ); va_end( ap );
}
/** The disconnect command a dropped client gets. */
void QDECL SV_SendServerCommand( client_t *cl, const char *fmt, ... ) {
	char text[MAX_STRING_CHARS]; va_list ap;
	va_start( ap, fmt ); vsnprintf( text, sizeof(text), fmt, ap ); va_end( ap );
	if ( cl && !strncmp( text, "disconnect ", 11 ) ) Q_strncpyz( disconnect, text + 11, sizeof(disconnect) );
}
/** No server command is waiting when the gamestate goes out. */
void SV_UpdateServerCommandsToClient( client_t *client, msg_t *msg ) { (void)client; (void)msg; }
/** The netchan adds svc_EOF, then the message has to fit MAX_MSGLEN; keep it for the client parser. */
void SV_SendMessageToClient( msg_t *msg, client_t *client ) {
	const char *systemInfo = sv.configstrings[CS_SYSTEMINFO];
	int length;
	(void)client;
	sentBytes = msg->cursize;
	MSG_WriteByte( msg, svc_EOF );
	length = msg->cursize;
	fwrite( &length, sizeof(length), 1, out );
	fwrite( msg->data, 1, length, out );
	length = strlen( systemInfo );
	fwrite( &length, sizeof(length), 1, out );
	fwrite( systemInfo, 1, length, out );
	fflush( out );
	sends++;
	Check( !msg->overflowed && msg->cursize <= MAX_MSGLEN, "gamestate sent that does not fit one message with its svc_EOF" );
}

/** The game: its cvars, the level's entities and configstrings up to level.fill chars. */
void SV_InitGameProgs( void ) {
	static const char *dirs[] = { "models/mapobjects/", "sound/world/", "models/players/", "textures/base_wall/" };
	char text[MAX_STRING_CHARS];
	int i, chars, len;
	Cvar_Get( "g_synchronousClients", "0", CVAR_SYSTEMINFO );
	Cvar_Get( "pmove_fixed", "0", CVAR_SYSTEMINFO );
	Cvar_Get( "pmove_msec", "8", CVAR_SYSTEMINFO );
	memset( gentities, 0, sizeof(gentities) );
	sv.num_entities = level.entities + MAX_CLIENTS;
	for ( i = MAX_CLIENTS; i < sv.num_entities; i++ ) {
		gentities[i].r.linked = qtrue;
		gentities[i].s.eType = ET_GENERAL + i % 4;
		gentities[i].s.modelindex = 1 + i % 200;
		gentities[i].s.origin[0] = (float)( i * 37 % 4096 - 2048 );
		gentities[i].s.origin[1] = (float)( i * 91 % 4096 - 2048 );
		gentities[i].s.origin[2] = (float)( i * 13 % 512 );
		gentities[i].s.angles[1] = (float)( i * 45 % 360 );
	}
	for ( i = CS_MODELS + 1, chars = 0; chars < level.fill && i < MAX_CONFIGSTRINGS; i++ ) {
		len = level.fill - chars - 1 < 60 ? level.fill - chars - 1 : 60;
		if ( len < 1 ) break;
		if ( level.pad ) {
			memset( text, level.pad, len ); text[len] = 0;
		} else {
			Com_sprintf( text, sizeof(text), "%sarea%03d/object_%d_%s", dirs[i % 4], i, i * 7919 % 100000, "tessellated_md3" );
			text[len] = 0;
		}
		SV_SetConfigstring( i, text );
		chars += strlen( text ) + 1;
	}
}

/** FS lists for level.paks pk3s, as the real ones build them (BIG_INFO_STRING buffers); in baseq3 three are referenced. */
static void BuildPakLists( void ) {
	char name[MAX_QPATH]; unsigned int seed = 345; int i; qboolean referenced;
	loadedSums[0] = loadedNames[0] = referencedSums[0] = referencedNames[0] = 0;
	for ( i = 0; i < level.paks; i++ ) {
		seed = seed * 1103515245u + 12345u;
		Com_sprintf( name, sizeof(name), level.pad ? "hth%03i" : "mappack-%03i", i );
		referenced = Q_stricmp( level.game, BASEGAME ) || i < 3;
		Q_strcat( loadedSums, sizeof(loadedSums), va( "%i ", (int)seed ) );
		if ( loadedNames[0] ) Q_strcat( loadedNames, sizeof(loadedNames), " " );
		Q_strcat( loadedNames, sizeof(loadedNames), name );
		if ( referenced ) {
			Q_strcat( referencedSums, sizeof(referencedSums), va( "%i ", (int)seed ) );
			if ( referencedNames[0] ) Q_strcat( referencedNames, sizeof(referencedNames), " " );
			Q_strcat( referencedNames, sizeof(referencedNames), va( "%s/%s", level.game, name ) );
		}
	}
	/* a longer last pk3 name, to make sv_pakNames end where a test needs it */
	i = strlen( loadedNames );
	Check( i + nameExtra < (int)sizeof(loadedNames), "pk3 name padding" );
	memset( loadedNames + i, 'x', nameExtra );
	loadedNames[i + nameExtra] = 0;
}
const char *FS_LoadedPakChecksums( void ) { return loadedSums; }
const char *FS_LoadedPakNames( void ) { return loadedNames; }
const char *FS_ReferencedPakChecksums( void ) { return referencedSums; }
const char *FS_ReferencedPakNames( void ) { return referencedNames; }

/* A gamestate's size: the chars a client stores, the bytes of its message before the netchan's svc_EOF. */
typedef struct { int chars, bytes; } gamestateSize_t;

/** Size of the gamestate SV_SendClientGameState writes for client 0 with this systeminfo, measured whole. */
static gamestateSize_t Measure( const char *systemInfo ) {
	static byte buffer[4 * MAX_MSGLEN];
	gamestateSize_t size; entityState_t nullstate; msg_t msg; const char *s; int i;
	client_t *cl = &svs.clients[0];
	MSG_Init( &msg, buffer, sizeof(buffer) );
	MSG_WriteLong( &msg, cl->lastClientCommand );
	MSG_WriteByte( &msg, svc_gamestate );
	MSG_WriteLong( &msg, cl->reliableSequence );
	size.chars = 1;
	for ( i = 0; i < MAX_CONFIGSTRINGS; i++ ) {
		s = i == CS_SYSTEMINFO ? systemInfo : sv.configstrings[i];
		if ( !s[0] ) continue;
		MSG_WriteByte( &msg, svc_configstring ); MSG_WriteShort( &msg, i ); MSG_WriteBigString( &msg, s );
		/* retail's MSG_ReadBigString, and this client's, leave a BIG_INFO_STRING - 1 char string's terminator unread */
		size.chars += strlen( s ) == BIG_INFO_STRING - 1 ? MAX_GAMESTATE_CHARS : (int)strlen( s ) + 1;
	}
	memset( &nullstate, 0, sizeof(nullstate) );
	for ( i = 0; i < MAX_GENTITIES; i++ ) {
		if ( !sv.svEntities[i].baseline.number ) continue;
		MSG_WriteByte( &msg, svc_baseline ); MSG_WriteDeltaEntity( &msg, &nullstate, &sv.svEntities[i].baseline, qtrue );
	}
	MSG_WriteByte( &msg, svc_EOF ); MSG_WriteLong( &msg, 0 ); MSG_WriteLong( &msg, sv.checksumFeed );
	Check( !msg.overflowed, "measure buffer" );
	size.bytes = msg.cursize;
	return size;
}
/** Room for the reserve and the queued commands twice over: the lists must stay. */
static qboolean Roomy( gamestateSize_t size ) {
	return size.chars + 2 * RESERVE <= MAX_GAMESTATE_CHARS && size.bytes + 2 * RESERVE <= MAX_MSGLEN;
}
/** Inside the reserve even with no queued command: a list here must go. */
static qboolean Tight( gamestateSize_t size ) {
	return size.chars + RESERVE > MAX_GAMESTATE_CHARS || size.bytes + RESERVE > MAX_MSGLEN;
}
/** Fits what a client takes: MAX_GAMESTATE_CHARS, and one message with the netchan's svc_EOF. */
static qboolean Fits( gamestateSize_t size ) {
	return size.chars <= MAX_GAMESTATE_CHARS && size.bytes < MAX_MSGLEN;
}
/** The systeminfo master's SV_SpawnServer builds with or without each pure list, from the same cvars. */
static const char *Candidate( qboolean sums, qboolean names, char *info ) {
	char keepSums[BIG_INFO_STRING], keepNames[BIG_INFO_STRING];
	size_t mark = printedLength;
	Q_strncpyz( keepSums, Cvar_VariableString( "sv_paks" ), sizeof(keepSums) );
	Q_strncpyz( keepNames, Cvar_VariableString( "sv_pakNames" ), sizeof(keepNames) );
	Cvar_Set( "sv_paks", sums && sv_pure->integer ? loadedSums : "" );
	Cvar_Set( "sv_pakNames", names && sv_pure->integer ? loadedNames : "" );
	Q_strncpyz( info, Cvar_InfoString_Big( CVAR_SYSTEMINFO ), BIG_INFO_STRING );
	Cvar_Set( "sv_paks", keepSums );
	Cvar_Set( "sv_pakNames", keepNames );
	printedLength = mark; printed[mark] = 0;
	return info;
}
/** Spawn the level on a fresh map (sv_serverid keeps seven digits). */
static void Spawn( void ) {
	com_frameTime += 3601;
	Check( com_frameTime < 10000000, "sv_serverid length" );
	BuildPakLists();
	printedLength = 0; printed[0] = 0;
	SV_SpawnServer( "budget", qfalse );
	Check( sv.state == SS_GAME, "level spawned" );
}
/** Connect client 0 over the network (or loopback) and send it the gamestate; return whether it went out. */
static qboolean SendGameState( netadrtype_t type ) {
	client_t *cl = &svs.clients[0];
	int before = sends;
	size_t mark = printedLength;
	memset( cl, 0, sizeof(*cl) );
	cl->state = CS_CONNECTED;
	cl->netchan.remoteAddress.type = type;
	Q_strncpyz( cl->name, "budget", sizeof(cl->name) );
	oob[0] = disconnect[0] = 0;
	SV_SendClientGameState( cl );
	if ( sends == before ) {
		Check( cl->state == CS_ZOMBIE && !strcmp( disconnect, "\"gamestate overflow\"" ), "refused client dropped with the reason" );
		Check( !strcmp( oob, "print\nSERVER ERROR: gamestate overflow\n" ), "refused client told out of band" );
		Check( strstr( printed + mark, "WARNING: gamestate for budget is too big (" ) != NULL, "refusal reported on the server" );
	} else {
		Check( sends == before + 1 && cl->state == CS_PRIMED, "gamestate sent once" );
	}
	cl->state = CS_FREE;
	return sends != before;
}
/** True if the warning for a pure list left out of the gamestate was printed. */
static qboolean Warned( const char *key ) {
	char prefix[128];
	Com_sprintf( prefix, sizeof(prefix), "WARNING: no room in the gamestate for %s (", key );
	return strstr( printed, prefix ) != NULL;
}

static int unchanged, namesOut, degraded, refused, rescued, charBound, unreadable;

/** One level: the pure lists stay, or go names first, exactly when the budget says so, and the gamestate fits. */
static void RunLevel( void ) {
	static char full[BIG_INFO_STRING], sumsOnly[BIG_INFO_STRING], none[BIG_INFO_STRING];
	const char *built;
	qboolean pure, namesDropped, sumsDropped, sent;
	gamestateSize_t fullSize, sumsSize, noneSize, builtSize;

	Spawn();
	sent = SendGameState( NA_IP );	/* first, so the client parser gets it even if a check below fails */
	built = sv.configstrings[CS_SYSTEMINFO];
	pure = sv_pure->integer != 0;
	namesDropped = pure && loadedNames[0] && !Cvar_VariableString( "sv_pakNames" )[0];
	sumsDropped = pure && loadedSums[0] && !Cvar_VariableString( "sv_paks" )[0];
	Candidate( qtrue, qtrue, full );
	Candidate( qtrue, qfalse, sumsOnly );
	Candidate( qfalse, qfalse, none );

	Check( namesDropped == Warned( "sv_pakNames" ), "sv_pakNames is left out exactly when a warning says so" );
	Check( sumsDropped == Warned( "sv_paks" ), "sv_paks is left out exactly when a warning says so" );
	Check( !sumsDropped || strstr( printed, "(degraded pure)" ), "leaving sv_paks out names degraded pure" );
	Check( !sumsDropped || namesDropped || !loadedNames[0], "sv_pakNames goes before sv_paks" );
	Check( !strcmp( built, sumsDropped ? none : namesDropped ? sumsOnly : full ), "systeminfo is master's with the lists kept" );
	fullSize = Measure( full );
	Check( !Roomy( fullSize ) || !namesDropped, "a roomy gamestate keeps the systeminfo master builds" );
	Check( !Tight( fullSize ) || !pure || !loadedNames[0] || namesDropped, "sv_pakNames left out when it would fill the gamestate" );
	/* sv_paks alone is smaller than both lists, so it only needs checking once sv_pakNames is out */
	sumsSize = noneSize = fullSize;
	if ( namesDropped ) {
		sumsSize = Measure( sumsOnly );
		Check( !Roomy( sumsSize ) || !sumsDropped, "sv_paks kept when it fits without sv_pakNames" );
		Check( !Tight( sumsSize ) || !loadedSums[0] || sumsDropped, "sv_paks left out when it would fill the gamestate" );
	}
	if ( sumsDropped ) {
		noneSize = Measure( none );
	}

	builtSize = sumsDropped ? noneSize : namesDropped ? sumsSize : fullSize;
	Check( sent == Fits( builtSize ), "the gamestate is sent exactly when a client can take it" );
	Check( !sent || sentBytes == builtSize.bytes, "measured gamestate matches the one sent" );
	if ( !sent && !sumsDropped ) {
		noneSize = Measure( none );
	}
	Check( sent || Tight( noneSize ), "a level that fits without the pure lists is never refused" );

	if ( !sent ) refused++;
	else if ( sumsDropped ) degraded++;
	else if ( namesDropped ) namesOut++;
	else unchanged++;
	if ( strlen( full ) == BIG_INFO_STRING - 1 ) unreadable++;
	else if ( sent && fullSize.chars > MAX_GAMESTATE_CHARS && fullSize.bytes < MAX_MSGLEN ) charBound++;
	if ( sent && !Fits( fullSize ) ) rescued++;
}

/** Pk3 counts and fills from a few configstrings to past the budget, for each kind of level. */
static void Sweep( void ) {
	static const level_t families[] = {
		{ BASEGAME, "baseq3, model paths", 0, 0, 40, '\0' },
		{ BASEGAME, "baseq3, many baselines", 0, 0, 250, '\0' },
		{ BASEGAME, "baseq3, short-code text", 0, 0, 10, 'h' },
		{ "mymod", "mod, every pk3 referenced", 0, 0, 40, '\0' },
	};
	static const int paks[] = { 0, 1, 9, 50, 100, 150, 200, 300, 400, 550, 700 };
	static const int fills[] = { 1000, 5000, 9000, 11000, 12500, 14000, 15500 };
	int f, p, c;
	for ( f = 0; f < (int)( sizeof(families) / sizeof(families[0]) ); f++ ) {
		Cvar_Set( "fs_game", Q_stricmp( families[f].game, BASEGAME ) ? families[f].game : "" );
		for ( p = 0; p < (int)( sizeof(paks) / sizeof(paks[0]) ); p++ ) {
			for ( c = 0; c < (int)( sizeof(fills) / sizeof(fills[0]) ); c++ ) {
				level = families[f];
				level.paks = paks[p];
				level.fill = fills[c];
				RunLevel();
			}
		}
	}
	Cvar_Set( "fs_game", "" );
	printf( "Levels: %d unchanged, %d without sv_pakNames, %d degraded pure, %d refused; %d that master could "
		"not send (%d over MAX_GAMESTATE_CHARS only, %d with a BIG_INFO_STRING - 1 char systeminfo)\n",
		unchanged, namesOut, degraded, refused, rescued, charBound, unreadable );
	Check( unchanged > 0 && namesOut > 0 && degraded > 0 && refused > 0 && rescued > 0 && charBound > 0,
		"the sweep reaches every outcome" );
}

/** Pure lists that make systeminfo exactly BIG_INFO_STRING - 1 chars: every retail client would drop the gamestate. */
static void ExactFit( void ) {
	static char full[BIG_INFO_STRING];
	level_t exact = { BASEGAME, "exact BIG_INFO_STRING - 1 systeminfo", 300, 3000, 40, '\0' };
	int before = unreadable;

	level = exact;
	nameExtra = 0;
	Spawn();
	nameExtra = BIG_INFO_STRING - 1 - (int)strlen( Candidate( qtrue, qtrue, full ) );
	Check( nameExtra > 0 && !strcmp( Info_ValueForKey( full, "sv_pakNames" ), loadedNames ), "exact fit padding" );
	RunLevel();
	nameExtra = 0;
	Check( unreadable == before + 1, "master's systeminfo is BIG_INFO_STRING - 1 chars" );
	Check( !Cvar_VariableString( "sv_pakNames" )[0] && !strcmp( Cvar_VariableString( "sv_paks" ), loadedSums ),
		"sv_pakNames left out of it, sv_paks kept" );
	puts( "a systeminfo no retail client can read loses sv_pakNames" );
}

/** A small retail-like pure server: nine baseq3 pk3s, the systeminfo is master's and no warning is printed. */
static void Retail( void ) {
	static char full[BIG_INFO_STRING];
	level_t retail = { BASEGAME, "retail-like", 9, 3000, 60, '\0' };
	level = retail;
	Spawn();
	Candidate( qtrue, qtrue, full );
	Check( !strcmp( sv.configstrings[CS_SYSTEMINFO], full ), "retail-like systeminfo unchanged" );
	Check( !strcmp( Info_ValueForKey( full, "sv_paks" ), loadedSums ) && !strcmp( Info_ValueForKey( full, "sv_pakNames" ), loadedNames ),
		"both pure lists sent whole" );
	Check( !strstr( printed, "gamestate" ), "no gamestate warning" );
	Check( SendGameState( NA_IP ), "retail-like gamestate sent" );
}

/** Set the padding configstring to len longChar and shorts 'h'; return the gamestate size in chars or bytes. */
static int Pad( int len, int shorts, char longChar, qboolean inChars ) {
	char text[BIG_INFO_STRING];
	gamestateSize_t size;
	memset( text, longChar, len );
	memset( text + len, 'h', shorts );
	text[len + shorts] = 0;
	SV_SetConfigstring( MAX_CONFIGSTRINGS - 1, text );
	size = Measure( sv.configstrings[CS_SYSTEMINFO] );
	return inChars ? size.chars : size.bytes;
}
/** Pad one configstring until the whole gamestate is exactly target chars or bytes. */
static void PadTo( int target, qboolean inChars, char longChar ) {
	int shorts, low, high, mid;
	for ( shorts = 0; shorts < 16; shorts++ ) {
		low = 1; high = BIG_INFO_STRING - 32;
		Check( Pad( high, shorts, longChar, inChars ) >= target, "padding can reach the target size" );
		while ( low < high ) {	/* the shortest padding that reaches the target */
			mid = ( low + high ) / 2;
			if ( Pad( mid, shorts, longChar, inChars ) >= target ) high = mid; else low = mid + 1;
		}
		if ( Pad( low, shorts, longChar, inChars ) == target ) return;
	}
	Check( 0, "padding reaches the target size exactly" );
}

/** A level too big for other reasons: refused cleanly at the first char and byte over, sent just under. */
static void Overflow( void ) {
	level_t big = { BASEGAME, "no pure lists, edges", 0, 11000, 40, 'h' };
	gamestateSize_t size;

	Cvar_Set( "sv_pure", "0" );
	level = big;
	Spawn();
	PadTo( MAX_GAMESTATE_CHARS, qtrue, 'h' );
	size = Measure( sv.configstrings[CS_SYSTEMINFO] );
	Check( size.bytes < MAX_MSGLEN - 64, "chars edge is under the message size" );
	Check( SendGameState( NA_IP ), "gamestate of exactly MAX_GAMESTATE_CHARS sent" );
	PadTo( MAX_GAMESTATE_CHARS + 1, qtrue, 'h' );
	Check( !SendGameState( NA_IP ), "gamestate one char over MAX_GAMESTATE_CHARS refused" );

	/* retail's MSG_ReadBigString leaves the terminator of a BIG_INFO_STRING - 1 char string unread */
	level.fill = 1000;
	Spawn();
	Pad( BIG_INFO_STRING - 2, 0, 'h', qtrue );
	Check( SendGameState( NA_IP ), "configstring of BIG_INFO_STRING - 2 chars sent" );
	Pad( BIG_INFO_STRING - 1, 0, 'h', qtrue );
	Check( !SendGameState( NA_IP ), "configstring of BIG_INFO_STRING - 1 chars, which no client reads, refused" );

	level.pad = 'K';	/* ten bits a char: the message fills before the chars do */
	level.fill = 9000;
	Spawn();
	PadTo( MAX_MSGLEN - 1, qfalse, 'K' );
	size = Measure( sv.configstrings[CS_SYSTEMINFO] );
	Check( size.chars < MAX_GAMESTATE_CHARS, "message edge is under MAX_GAMESTATE_CHARS" );
	Check( SendGameState( NA_IP ), "gamestate with room for svc_EOF sent" );
	PadTo( MAX_MSGLEN, qfalse, 'K' );
	Check( !SendGameState( NA_IP ), "gamestate that fills the message refused" );
	level.fill = 15500; level.entities = 900;
	Spawn();
	Check( !SendGameState( NA_IP ), "gamestate that overflows the message refused" );

	/* the local client of a listen server can only drop the whole game */
	expectDrop = 1;
	if ( !setjmp( dropJump ) ) {
		SendGameState( NA_LOOPBACK );
		Check( 0, "loopback gamestate overflow drops the game" );
	}
	expectDrop = 0;
	Check( !strcmp( disconnect, "gamestate overflow" ), "loopback drop gives the reason" );
	svs.clients[0].state = CS_FREE;
	Cvar_Set( "sv_pure", "1" );
	puts( "oversized gamestates are refused at the first char and byte over" );
}

int main( int argc, char **argv ) {
	Check( argc == 2, "usage: server-gamestate-budget <gamestates>" );
	out = fopen( argv[1], "wb" );
	Check( out != NULL, "open the gamestates" );
	/* start-up order of the systeminfo cvars: Cvar_Init, FS_Startup, Com_Init, SV_Init, CL_Init */
	Cvar_Get( "sv_cheats", "1", CVAR_ROM | CVAR_SYSTEMINFO );
	Cvar_Get( "fs_game", "", CVAR_INIT | CVAR_SYSTEMINFO );
	Cvar_Get( "timescale", "1", CVAR_CHEAT | CVAR_SYSTEMINFO );
	com_dedicated = Cvar_Get( "dedicated", "1", CVAR_ROM );
	SV_Init();
	Cvar_Get( "cl_anonymous", "0", CVAR_INIT | CVAR_SYSTEMINFO );
	Cvar_Set( "sv_hostname", "Issue 345 budget server" );

	com_frameTime = 1000000;
	Retail();
	Sweep();
	ExactFit();
	Overflow();
	fclose( out );
	puts( "Server gamestate budget regression passed" );
	return 0;
}
