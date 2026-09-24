/* Issue #340: a player's chat line reaches the real game dispatcher and the real botlib reply chat. */
#ifdef Q3_CHAT_SOURCE
/* Botlib object: the real chat AI, linked into the engine as the Mac build links botlib. */
#include Q3_CHAT_SOURCE
#include <stdio.h>
#include <stdlib.h>

botlib_import_t botimport;
int botDeveloper;
int botlibErrors;
static float botlibTime;
static const char *botfiles;

/* Excerpts of the stock syn.c, match.c and rchat.c entries the reply path uses; the rchat line names the player. */
static const char *textFiles[][2] = {
	{"syn.c", "#define CONTEXT_NEARBYITEM 2\nCONTEXT_NEARBYITEM\n{\n"
		"\t[(\"Heavy Armor\", 0), (\"red armor\", 0), (\"Heavy Armour\", 0), (\"red armour\", 0),(\"ra\", 0)]\n}\n"},
	{"match.c", "#define MTCONTEXT_REPLYCHAT 128\n#define MSG_CHATALL 200\n#define NETNAME 0\n#define MESSAGE 2\n"
		"MTCONTEXT_REPLYCHAT\n{\n\tNETNAME, \": \", MESSAGE = (MSG_CHATALL, 0);\n}\n"},
	{"rnd.c", "// no random strings\n"},
	{"rchat.c", "[\"armor\"] = 5\n{\n\t\"Armor won't save you, \", 7, \".\";\n}\n"}
};
static char *openFiles[8];
static int openLengths[8];

/** Count botlib errors and show them; the reply path reports none. */
static void QDECL BotlibPrint( int type, char *format, ... ) {
	va_list ap;
	if (type != PRT_ERROR && type != PRT_FATAL) return;
	botlibErrors++;
	va_start(ap, format); vfprintf(stderr, format, ap); va_end(ap);
}
/** Zone memory placed so the block after botlib's one-long header keeps malloc's alignment, as script_t needs. */
static void *BotlibAlloc( int size ) {
	char *base = calloc(1, size + 32), *block = base + 32 - sizeof(unsigned long);
	if (!base) return NULL;
	memcpy(block - sizeof(base), &base, sizeof(base));
	return block;
}
static void BotlibFree( void *memory ) {
	char *base;
	memcpy(&base, (char *)memory - sizeof(base), sizeof(base));
	free(base);
}
/** Serve a bot file by name from the excerpts, or from Q3_TEST_BOTFILES (extracted retail botfiles, never committed). */
static int BotlibOpen( const char *qpath, fileHandle_t *file, fsMode_t mode ) {
	const char *name = strrchr(qpath, '/') ? strrchr(qpath, '/') + 1 : qpath;
	char path[1024], *text = NULL;
	int i, length = -1;
	FILE *disk;
	*file = 0;
	if (mode != FS_READ) return -1;
	for (i = 1; i < 8 && openFiles[i]; i++);
	if (i == 8) return -1;
	if (botfiles) {
		snprintf(path, sizeof(path), "%s/%s", botfiles, name);
		if (!(disk = fopen(path, "rb"))) return -1;
		fseek(disk, 0, SEEK_END); length = ftell(disk); fseek(disk, 0, SEEK_SET);
		text = malloc(length + 1);
		if (text && fread(text, 1, length, disk) != (size_t)length) { free(text); text = NULL; }
		fclose(disk);
	} else {
		for (length = 0; length < (int)(sizeof(textFiles) / sizeof(textFiles[0])); length++) {
			if (!strcmp(textFiles[length][0], name)) break;
		}
		if (length == (int)(sizeof(textFiles) / sizeof(textFiles[0]))) return -1;
		text = strdup(textFiles[length][1]);
		length = text ? strlen(text) : -1;
	}
	if (!text) return -1;
	openFiles[i] = text; openLengths[i] = length; *file = i;
	return length;
}
static int BotlibRead( void *buffer, int length, fileHandle_t file ) {
	if (length > openLengths[file]) length = openLengths[file];
	memcpy(buffer, openFiles[file], length);
	return length;
}
static void BotlibClose( fileHandle_t file ) { free(openFiles[file]); openFiles[file] = NULL; }
void QDECL Log_Write( char *format, ... ) { (void)format; }
/** Each round runs later than CHATMESSAGE_RECENTTIME, so the reply line is available again. */
float AAS_Time( void ) { return botlibTime; }
void BotlibNextRound( void ) { botlibTime += 100; }

/** Load the chat AI from its files and export the chat entry points as be_interface.c does. */
int BotlibSetup( botlib_export_t *api, const char *directory ) {
	botfiles = directory;
	botimport.Print = BotlibPrint;
	botimport.GetMemory = BotlibAlloc; botimport.FreeMemory = BotlibFree; botimport.HunkAlloc = BotlibAlloc;
	botimport.FS_FOpenFile = BotlibOpen; botimport.FS_Read = BotlibRead; botimport.FS_FCloseFile = BotlibClose;
	if (BotSetupChatAI() != BLERR_NOERROR || !synonyms || !matchtemplates || !replychats) return 0;
	api->ai.BotAllocChatState = BotAllocChatState;
	api->ai.BotFreeChatState = BotFreeChatState;
	api->ai.BotQueueConsoleMessage = BotQueueConsoleMessage;
	api->ai.BotRemoveConsoleMessage = BotRemoveConsoleMessage;
	api->ai.BotNextConsoleMessage = BotNextConsoleMessage;
	api->ai.BotReplyChat = BotReplyChat;
	api->ai.BotGetChatMessage = BotGetChatMessage;
	api->ai.BotFindMatch = BotFindMatch;
	api->ai.BotMatchVariable = BotMatchVariable;
	api->ai.UnifyWhiteSpaces = UnifyWhiteSpaces;
	api->ai.BotReplaceSynonyms = BotReplaceSynonyms;
	api->ai.BotSetChatName = BotSetChatName;
	return 1;
}
void BotlibShutdown( void ) { BotShutdownChatAI(); LibVarDeAllocAll(); }

#else
/* Engine object: the real sv_game.c dispatcher, entered through the syscall a native game module uses. */
#include "../code/server/sv_game.c"
#include "../code/qcommon/vm_local.h"
#include "../code/game/match.h"
#include "../code/game/syn.h"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#define IMAGE_SIZE 8192
#define GAME_NETNAME 36	/* g_local.h MAX_NETNAME */
server_t sv;
serverStatic_t svs;
cvar_t *sv_maxclients;
static vm_t vm;
vm_t *gvm = &vm;
static botlib_export_t api;
extern int botlibErrors;
int BotlibSetup( botlib_export_t *api, const char *directory );
void BotlibShutdown( void );
void BotlibNextRound( void );
int QDECL VM_DllSyscall( int arg, ... );

/** Fail on any unexpected result. */
static void Check( int ok, const char *message ) {
	if (!ok) { fprintf(stderr, "Bot reply chat server regression failed: %s\n", message); exit(1); }
}
/** Fail when the trap reaches an engine service the chat traps never use. */
static void Unexpected( const char *name ) {
	fprintf(stderr, "Bot reply chat server regression reached %s\n", name); exit(1);
}
/** ERR_DROP here is SV_Shutdown for every client: no chat line may reach it. */
void QDECL Com_Error( int level, const char *format, ... ) {
	char message[MAX_STRING_CHARS];
	va_list ap;
	va_start(ap, format); vsnprintf(message, sizeof(message), format, ap); va_end(ap);
	fprintf(stderr, "Bot reply chat server regression failed: server error %d: %s\n", level, message);
	exit(1);
}
void QDECL Com_Printf( const char *format, ... ) { (void)format; }
void Com_Memcpy( void *dest, const void *src, const size_t count ) { memcpy(dest, src, count); }
void Com_Memset( void *dest, const int val, const size_t count ) { memset(dest, val, count); }
qboolean UI_GameCommand( void ) { Unexpected(__func__); return qfalse; }
int BotImport_DebugPolygonCreate( int color, int numPoints, vec3_t *points ) { Unexpected(__func__); return 0; }
void BotImport_DebugPolygonDelete( int id ) { Unexpected(__func__); }
void Cbuf_ExecuteText( int exec_when, const char *text ) { Unexpected(__func__); }
int Cmd_Argc( void ) { Unexpected(__func__); return 0; }
void Cmd_ArgvBuffer( int arg, char *buffer, int bufferLength ) { Unexpected(__func__); }
void CM_AdjustAreaPortalState( int area1, int area2, qboolean open ) { Unexpected(__func__); }
qboolean CM_AreasConnected( int area1, int area2 ) { Unexpected(__func__); return qfalse; }
byte *CM_ClusterPVS( int cluster ) { Unexpected(__func__); return NULL; }
clipHandle_t CM_InlineModel( int index ) { Unexpected(__func__); return 0; }
int CM_LeafArea( int leafnum ) { Unexpected(__func__); return 0; }
int CM_LeafCluster( int leafnum ) { Unexpected(__func__); return 0; }
void CM_ModelBounds( clipHandle_t model, vec3_t mins, vec3_t maxs ) { Unexpected(__func__); }
int CM_PointLeafnum( const vec3_t p ) { Unexpected(__func__); return 0; }
void CM_TransformedBoxTrace( trace_t *results, const vec3_t start, const vec3_t end, vec3_t mins, vec3_t maxs,
	clipHandle_t model, int brushmask, const vec3_t origin, const vec3_t angles, int capsule ) { Unexpected(__func__); }
int Com_RealTime( qtime_t *qtime ) { Unexpected(__func__); return 0; }
char *Cvar_InfoString( int bit ) { Unexpected(__func__); return NULL; }
void Cvar_Register( vmCvar_t *vmCvar, const char *varName, const char *defaultValue, int flags ) { Unexpected(__func__); }
void Cvar_Set( const char *var_name, const char *value ) { Unexpected(__func__); }
void Cvar_SetSafe( const char *var_name, const char *value ) { Unexpected(__func__); }
void Cvar_Update( vmCvar_t *vmCvar ) { Unexpected(__func__); }
int Cvar_VariableIntegerValue( const char *var_name ) { Unexpected(__func__); return 0; }
void Cvar_VariableStringBuffer( const char *var_name, char *buffer, int bufsize ) { Unexpected(__func__); }
qboolean EA_ClientValid( int client ) { Unexpected(__func__); return qfalse; }
void FS_FCloseFile( fileHandle_t f ) { Unexpected(__func__); }
int FS_FOpenFileByMode( const char *qpath, fileHandle_t *f, fsMode_t mode ) { Unexpected(__func__); return 0; }
int FS_GetFileList( const char *path, const char *extension, char *listbuf, int bufsize ) { Unexpected(__func__); return 0; }
int FS_Read2( void *buffer, int len, fileHandle_t f ) { Unexpected(__func__); return 0; }
int FS_Seek( fileHandle_t f, long offset, int origin ) { Unexpected(__func__); return 0; }
int FS_Write( const void *buffer, int len, fileHandle_t f ) { Unexpected(__func__); return 0; }
int SV_AreaEntities( const vec3_t mins, const vec3_t maxs, int *entityList, int maxcount ) { Unexpected(__func__); return 0; }
int SV_BotAllocateClient( void ) { Unexpected(__func__); return -1; }
void SV_BotFreeClient( int clientNum ) { Unexpected(__func__); }
int SV_BotGetConsoleMessage( int client, char *buf, int size ) { Unexpected(__func__); return 0; }
int SV_BotGetSnapshotEntity( int client, int ent ) { Unexpected(__func__); return -1; }
int SV_BotLibSetup( void ) { Unexpected(__func__); return 0; }
int SV_BotLibShutdown( void ) { Unexpected(__func__); return 0; }
void SV_ClientThink( client_t *cl, usercmd_t *cmd ) { Unexpected(__func__); }
clipHandle_t SV_ClipHandleForEntity( const sharedEntity_t *ent ) { Unexpected(__func__); return 0; }
void SV_DropClient( client_t *drop, const char *reason ) { Unexpected(__func__); }
void SV_GetConfigstring( int index, char *buffer, int bufferSize ) { Unexpected(__func__); }
void SV_GetUserinfo( int index, char *buffer, int bufferSize ) { Unexpected(__func__); }
void SV_LinkEntity( sharedEntity_t *ent ) { Unexpected(__func__); }
int SV_PointContents( const vec3_t p, int passEntityNum ) { Unexpected(__func__); return 0; }
void QDECL SV_SendServerCommand( client_t *cl, const char *fmt, ... ) { Unexpected(__func__); }
void SV_SetConfigstring( int index, const char *val ) { Unexpected(__func__); }
void SV_SetUserinfo( int index, const char *val ) { Unexpected(__func__); }
void SV_Trace( trace_t *results, const vec3_t start, vec3_t mins, vec3_t maxs, const vec3_t end,
	int passEntityNum, int contentmask, int capsule ) { Unexpected(__func__); }
void SV_UnlinkEntity( sharedEntity_t *ent ) { Unexpected(__func__); }
int Sys_Milliseconds( void ) { Unexpected(__func__); return 0; }
void Sys_SnapVector( float *v ) { Unexpected(__func__); }
int VM_CallCompiled( vm_t *target, int *args ) { Unexpected(__func__); return 0; }
int VM_CallInterpreted( vm_t *target, int *args ) { Unexpected(__func__); return 0; }

/** Mark the game as a native module, as the Mac OS 9 build links it. */
static int QDECL NativeEntry( int command, ... ) { (void)command; return 0; }
/** Map the module's memory below 4 GiB so native pointers fit the int syscall ABI. */
static byte *LowPages( void ) {
	byte *page;
#ifdef MAP_32BIT
	page = mmap(NULL, IMAGE_SIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_32BIT, -1, 0);
#else
	page = mmap((void *)0x10000000, IMAGE_SIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
#endif
	Check(page != MAP_FAILED && (unsigned long)page == (unsigned int)(unsigned long)page, "low module pages");
	return page;
}
/** A module argument: a native address, or a QVM image offset. */
static int Arg( void *pointer ) {
	if (!pointer) return 0;
	return vm.entryPoint ? (int)(unsigned long)pointer : (int)((byte *)pointer - vm.dataBase);
}
/** Make a game syscall: native modules call VM_DllSyscall, QVM traps pass their argument array. */
static int Trap( int trap, int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9, int a10, int a11, int a12 ) {
	int args[16] = {0};
	if (vm.entryPoint) return VM_DllSyscall(trap, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, 0, 0, 0);
	args[0] = trap; args[1] = a1; args[2] = a2; args[3] = a3; args[4] = a4; args[5] = a5; args[6] = a6;
	args[7] = a7; args[8] = a8; args[9] = a9; args[10] = a10; args[11] = a11; args[12] = a12;
	vm.currentlyInterpreting = qtrue;
	return vm.systemCall(args);
}
#define TRAP3(t, a, b, c) Trap(t, a, b, c, 0, 0, 0, 0, 0, 0, 0, 0, 0)

/* The game module's locals in BotCheckConsoleMessages, inside the module image. */
typedef struct {
	union { bot_consolemessage_t native; qvmBotConsoleMessage_t qvm; } console;
	bot_match_t match;
	char said[MAX_MESSAGE_SIZE], message[MAX_MESSAGE_SIZE], reply[MAX_MESSAGE_SIZE];
	char botname[GAME_NETNAME], netname[GAME_NETNAME];
} gameLocals_t;
static gameLocals_t *game;
static int messageLength, netnameLength;

/** A name of the given length, as ClientCleanName keeps it (at most 35 characters). */
static const char *Name( const char *base, int length ) {
	static char names[2][GAME_NETNAME];
	static int which;
	char *name = names[which ^= 1];
	int i;
	for (i = 0; i < length; i++) name[i] = base[i % strlen(base)];
	name[length] = 0;
	return name;
}
/** A say line of "ra ra ..." at most 149 characters long, as g_cmds.c caps it. */
static const char *RaLine( int length ) {
	static char line[MAX_SAY_TEXT];
	int i;
	for (i = 0; i < length; i++) line[i] = i % 3 == 2 ? ' ' : "ra"[i % 3];
	line[length] = 0;
	return line;
}
/** One say line to one bot through the traps BotCheckConsoleMessages makes (ai_dmq3.c); returns the reply result. */
static int PlayerSays( const char *botName, const char *playerName, const char *text ) {
	int context = CONTEXT_NORMAL|CONTEXT_NEARBYITEM|CONTEXT_NAMES, cs, handle, replied, errors = botlibErrors;
	char *console, *chat;
	memset(game, 0, sizeof(*game));
	BotlibNextRound();
	cs = TRAP3(BOTLIB_AI_ALLOC_CHAT_STATE, 0, 0, 0);
	Check(cs > 0, "chat state");
	Q_strncpyz(game->botname, botName, sizeof(game->botname));
	TRAP3(BOTLIB_AI_SET_CHAT_NAME, cs, Arg(game->botname), 1);
	/* G_Say sends "name^7\x19: ^2text"; BotAI strips the colors before queueing it */
	Com_sprintf(game->said, sizeof(game->said), "%s\x19: %s", playerName, text);
	TRAP3(BOTLIB_AI_QUEUE_CONSOLE_MESSAGE, cs, CMS_CHAT, Arg(game->said));
	handle = TRAP3(BOTLIB_AI_NEXT_CONSOLE_MESSAGE, cs, Arg(&game->console), 0);
	Check(handle > 0, "queued chat message");
	console = vm.entryPoint ? game->console.native.message : game->console.qvm.message;
	chat = console;
	if (TRAP3(BOTLIB_AI_FIND_MATCH, Arg(console), Arg(&game->match), MTCONTEXT_REPLYCHAT) &&
	    game->match.variables[MESSAGE].offset >= 0) {
		chat = console + game->match.variables[MESSAGE].offset;
	}
	TRAP3(BOTLIB_AI_UNIFY_WHITE_SPACES, Arg(chat), 0, 0);
	TRAP3(BOTLIB_AI_REPLACE_SYNONYMS, Arg(chat), context, console + MAX_MESSAGE_SIZE - chat);
	Check(TRAP3(BOTLIB_AI_FIND_MATCH, Arg(console), Arg(&game->match), MTCONTEXT_REPLYCHAT), "reply chat match");
	Trap(BOTLIB_AI_MATCH_VARIABLE, Arg(&game->match), NETNAME, Arg(game->netname), sizeof(game->netname), 0, 0, 0, 0, 0, 0, 0, 0);
	Trap(BOTLIB_AI_MATCH_VARIABLE, Arg(&game->match), MESSAGE, Arg(game->message), sizeof(game->message), 0, 0, 0, 0, 0, 0, 0, 0);
	TRAP3(BOTLIB_AI_UNIFY_WHITE_SPACES, Arg(game->message), 0, 0);
	messageLength = strlen(game->message); netnameLength = strlen(game->netname);
	replied = Trap(BOTLIB_AI_REPLY_CHAT, cs, Arg(game->message), context, CONTEXT_REPLY,
		0, 0, 0, 0, 0, 0, Arg(game->botname), Arg(game->netname));
	if (replied) TRAP3(BOTLIB_AI_GET_CHAT_MESSAGE, cs, Arg(game->reply), sizeof(game->reply));
	TRAP3(BOTLIB_AI_REMOVE_CONSOLE_MESSAGE, cs, handle, 0);
	TRAP3(BOTLIB_AI_FREE_CHAT_STATE, cs, 0, 0);
	Check(botlibErrors == errors, "botlib reported an error");
	Check(strlen(game->reply) < MAX_MESSAGE_SIZE, "reply stays inside its buffer");
	return replied;
}

/** The issue's line: 149 characters of "ra" from the default player name to Sarge, natively. */
static void IssueLine( void ) {
	vm.entryPoint = NativeEntry;
	Check(PlayerSays("Sarge", "UnnamedPlayer", RaLine(149)) == 1, "Sarge replies to the issue #340 line");
	Check(messageLength == 239 && netnameLength == 14 && messageLength + 5 + netnameLength >= MAX_MESSAGE_SIZE,
	      "the synonym pass grows the line to 239 characters, past the old 256-byte guard with both names");
	Check(!strcmp(game->reply, "Armor won't save you, ."), "the player name that does not fit is left unset");
}

/** Inputs that fit keep the retail replies, including a netname retail already read as unset past byte 127. */
static void Retail( void ) {
	vm.entryPoint = NativeEntry;
	Check(PlayerSays("Sarge", "Player7", RaLine(149)) == 1 && messageLength == 239 && netnameLength == 8 &&
	      !strcmp(game->reply, "Armor won't save you, ."), "a line that fits the old guard keeps its retail reply");
	Check(PlayerSays("Sarge", "UnnamedPlayer", "ra") == 1 && !strcmp(game->message, "Heavy Armor") &&
	      !strcmp(game->reply, "Armor won't save you, UnnamedPlayer\x19."), "a short line names the player");
	Check(PlayerSays("Sarge", "UnnamedPlayer", "hello there") == 0 && !game->reply[0], "a line without a reply key is skipped");
}

/** No bot name, player name or line length drops the server. A QVM's synonym pass keeps the
 *  original span, so "ra" is not expanded there and the line has no reply key: the bot skips it. */
static void Sweep( void ) {
	int native, bot, player, length;
	for (native = 1; native >= 0; native--) {
		vm.entryPoint = native ? NativeEntry : NULL;
		for (bot = 1; bot < GAME_NETNAME; bot++) {
			for (player = 1; player < GAME_NETNAME; player++) {
				Check(PlayerSays(Name("Sarge", bot), Name("UnnamedPlayer", player), RaLine(149)) == native,
				      "every bot and player name length replies natively and is skipped from a QVM");
			}
		}
		for (length = 2; length < MAX_SAY_TEXT; length++) {
			Check(PlayerSays(Name("Sarge", GAME_NETNAME - 1), Name("UnnamedPlayer", GAME_NETNAME - 1), RaLine(length)) == native,
			      "every line length replies natively and is skipped from a QVM");
		}
	}
}

/** A QVM may pass a full 255-character message with the longest names; botlib leaves them unset. */
static void QvmReply( void ) {
	int cs;
	vm.entryPoint = NULL;
	memset(game, 0, sizeof(*game));
	BotlibNextRound();
	cs = TRAP3(BOTLIB_AI_ALLOC_CHAT_STATE, 0, 0, 0);
	strcpy(game->botname, Name("Sarge", GAME_NETNAME - 1)); strcpy(game->netname, Name("UnnamedPlayer", GAME_NETNAME - 1));
	memset(game->message, 'x', MAX_MESSAGE_SIZE - 1); memcpy(game->message, "armor ", 6);
	Check(Trap(BOTLIB_AI_REPLY_CHAT, cs, Arg(game->message), 0, CONTEXT_REPLY,
		0, 0, 0, 0, 0, 0, Arg(game->botname), Arg(game->netname)) == 1, "QVM full-length reply");
	TRAP3(BOTLIB_AI_GET_CHAT_MESSAGE, cs, Arg(game->reply), sizeof(game->reply));
	Check(!strcmp(game->reply, "Armor won't save you, ."), "QVM names past the match string are left unset");
	TRAP3(BOTLIB_AI_FREE_CHAT_STATE, cs, 0, 0);
}

/** With Q3_TEST_BOTFILES naming extracted retail botfiles, report the issue line and a stock reply key line. */
static void RetailFiles( void ) {
	static const char *lines[] = {"", "pub "};
	char line[MAX_SAY_TEXT];
	int i, replied;
	vm.entryPoint = NativeEntry;
	for (i = 0; i < 2; i++) {
		Com_sprintf(line, sizeof(line), "%s%s", lines[i], RaLine(149 - strlen(lines[i])));
		replied = PlayerSays("Sarge", "UnnamedPlayer", line);
		printf("stock files, %d-character \"%.8s...\": message %d + bot 5 + netname %d = %d bytes; replied %d: \"%s\"\n",
		       (int)strlen(line), line, messageLength, netnameLength, messageLength + 5 + netnameLength, replied, game->reply);
		Check(messageLength + 5 + netnameLength >= MAX_MESSAGE_SIZE, "stock files reach the old guard");
	}
}

int main( int argc, char **argv ) {
	const char *botfiles = getenv("Q3_TEST_BOTFILES");
	int proof = argc > 1 ? atoi(argv[1]) : -1;
	vm.dataBase = LowPages(); vm.dataMask = IMAGE_SIZE - 1;
	vm.systemCall = SV_GameSystemCalls; currentVM = &vm;
	game = (gameLocals_t *)(vm.dataBase + 64);
	Check(sizeof(*game) + 64 <= IMAGE_SIZE, "module locals fit the image");
	Check(BotlibSetup(&api, botfiles && *botfiles ? botfiles : NULL) && !botlibErrors, "botlib chat AI setup");
	botlib_export = &api;
	if (botfiles && *botfiles) RetailFiles();
	else if (proof == 0) IssueLine();
	else if (proof == 1) Retail();
	else if (proof == 2) QvmReply();
	else {
		IssueLine(); Retail(); QvmReply(); Sweep();
		puts("A player's chat line reaches the bot reply chat without dropping the server (issue #340)");
	}
	BotlibShutdown();
	munmap(vm.dataBase, IMAGE_SIZE);
	return 0;
}
#endif
