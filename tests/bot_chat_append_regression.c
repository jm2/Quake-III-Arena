/* Issue #306: initial and reply chat variables stay inside the native match string. */
#include "../code/botlib/be_ai_chat.c"
#include <stdlib.h>
#include <string.h>

botlib_import_t botimport;
/** Fail on any unexpected native output. */
static void Check( int ok, const char *message ) {
	if(!ok) { fprintf(stderr,"Bot chat append regression failed: %s\n",message); exit(1); }
}
static int printErrors;
/** Count botlib errors; appending variables never reports one. */
static void QDECL CountPrint( int type, char *format, ... ) { (void)format; if(type==PRT_ERROR||type==PRT_FATAL) printErrors++; }
/** Keep the real chat routines linked without the full engine. */
void Com_Memcpy( void *dest, const void *src, size_t size ) { memcpy(dest,src,size); }
/** Support the real chat helpers' native initialization. */
void Com_Memset( void *dest, int value, size_t size ) { memset(dest,value,size); }
/** Treat unexpected shared utility errors as regression failures. */
void QDECL Com_Error( int level, const char *format, ... ) { (void)level; (void)format; Check(0,"engine error"); }
/** Ignore unrelated shared utility output. */
void QDECL Com_Printf( const char *format, ... ) { (void)format; }
/** Chat lines are timed against a fixed native clock. */
float AAS_Time( void ) { return 125.5f; }
/** bot_testrchat stays off so the selected reply line is constructed once. */
float LibVarGetValue( char *var_name ) { (void)var_name; return 0; }

/* dbe4ddb BotInitialChat/BotReplyChat append (eight identical strcat blocks); only used for inputs that fit. */
static void OriginalAppend( bot_match_t *match, char **vars ) {
	int i, index = strlen(match->string);
	for(i=0;i<MAX_MATCHVARIABLES;i++) {
		if(!vars[i]) continue;
		strcat(match->string,vars[i]);
		match->variables[i].offset = index;
		match->variables[i].length = strlen(vars[i]);
		index += strlen(vars[i]);
	}
}

static bot_chatstate_t state;
static bot_chat_t chat;
static bot_chattype_t killType, sweepType;
static bot_chatmessage_t killLine, sweepLine, replyLine;
static bot_matchstring_t areYou={"are you ",NULL};
static bot_matchpiece_t captured={MT_VARIABLE,NULL,0,NULL}, prefix={MT_STRING,&areYou,0,&captured};
static bot_replychatkey_t replyKey={RCKFL_VARIABLES,NULL,&prefix,NULL};
static bot_replychat_t reply={&replyKey,6,1,&replyLine,NULL};
static char killText[64], sweepText[64], replyText[64], longText[1024];

/** Build a retail-shaped chat state: rchat.c ("are you ", 0) = 6 and a kill line with two names. */
static void Setup( void ) {
	int i;
	botimport.Print=CountPrint;
	memset(&state,0,sizeof(state)); strcpy(state.name,"Sarge"); state.chat=&chat; botchatstates[1]=&state;
	sprintf(killText,"%cv0%c was fragged by %cv1%c",ESCAPE_CHAR,ESCAPE_CHAR,ESCAPE_CHAR,ESCAPE_CHAR);
	for(i=0;i<MAX_MATCHVARIABLES;i++) sprintf(sweepText+4*i,"%cv%d%c",ESCAPE_CHAR,i,ESCAPE_CHAR);
	sprintf(replyText,"I'm not %cv0%c, %cv7%c.",ESCAPE_CHAR,ESCAPE_CHAR,ESCAPE_CHAR,ESCAPE_CHAR);
	/* lines used recently take the deterministic oldest-line path instead of random() */
	killLine.chatmessage=killText; killLine.time=1000;
	sweepLine.chatmessage=sweepText; sweepLine.time=1000;
	replyLine.chatmessage=replyText; replyLine.time=1000;
	strcpy(killType.name,"kill"); killType.numchatmessages=1; killType.firstchatmessage=&killLine; killType.next=&sweepType;
	strcpy(sweepType.name,"sweep"); sweepType.numchatmessages=1; sweepType.firstchatmessage=&sweepLine;
	chat.types=&killType; replychats=&reply;
}
/** Fill a variable with a recognizable letter. */
static char *Fill( char *buffer, int c, int length ) { memset(buffer,c,length); buffer[length]=0; return buffer; }
/** Expected message text: "I'm not <captured>, <netname>." */
static void ReplyExpect( char *out, const char *capture, const char *netname ) {
	sprintf(out,"I'm not %s, %s.",capture,netname);
}

/** Normal-length initial and reply chats are byte-identical to retail. */
static void Golden( void ) {
	char capture[MAX_MESSAGE_SIZE], expect[MAX_MESSAGE_SIZE];
	int length;
	printErrors=0;
	BotInitialChat(1,"kill",0,"Player","Sarge",NULL,NULL,NULL,NULL,NULL,NULL);
	Check(!strcmp(state.chatmessage,"Player was fragged by Sarge") && !printErrors,"normal initial chat unchanged");
	Check(BotReplyChat(1,"are you sure",0,0,NULL,NULL,NULL,NULL,NULL,NULL,"Sarge","Player") &&
	      !strcmp(state.chatmessage,"I'm not sure, Player.") && !printErrors,"normal reply chat unchanged");
	/* the netname starts at byte 127, then 128: past 127 it reads unset as the retail signed char offset did */
	for(length=122;length<=123;length++) {
		Fill(longText,'x',length); memcpy(longText,"are you ",8);
		ReplyExpect(expect,Fill(capture,'x',length-8),length+5>127?"":"Player");
		Check(BotReplyChat(1,longText,0,0,NULL,NULL,NULL,NULL,NULL,NULL,"Sarge","Player") &&
		      !strcmp(state.chatmessage,expect) && !printErrors,"retail signed offset boundary for the netname");
	}
}

/** Issue #306: a 245-character player line plus the bot and player names used to run past match.string. */
static void LongReply( int netnameLength ) {
	char netname[512], capture[MAX_MESSAGE_SIZE], expect[MAX_MESSAGE_SIZE];
	int length;
	Fill(netname,'p',netnameLength);
	/* "I'm not <capture>, <netname>." adds three bytes to the line, which must still fit the output */
	for(length=245;length+3<MAX_MESSAGE_SIZE;length++) {
		Fill(longText,'x',length); memcpy(longText,"are you ",8);
		ReplyExpect(expect,Fill(capture,'x',length-8),"");
		printErrors=0;
		Check(BotReplyChat(1,longText,0,0,NULL,NULL,NULL,NULL,NULL,NULL,"Sarge",netname),"long reply still replies");
		Check(!strcmp(state.chatmessage,expect) && !printErrors,
		      "long reply keeps its captured text and leaves names past the match string unset");
	}
}

/** Issue #306: initial chat variables longer than the match string used to run past the stack match. */
static void LongInitial( void ) {
	char name[512];
	printErrors=0;
	BotInitialChat(1,"kill",0,Fill(name,'n',400),"Sarge",NULL,NULL,NULL,NULL,NULL,NULL);
	Check(!strcmp(state.chatmessage," was fragged by Sarge") && !printErrors,
	      "an initial variable that cannot fit is unset and later ones still append");
	BotInitialChat(1,"sweep",0,Fill(name,'a',100),Fill(name+128,'b',155),NULL,NULL,NULL,NULL,NULL,NULL);
	Check(strlen(state.chatmessage)==255 && state.chatmessage[99]=='a' && state.chatmessage[100]=='b',
	      "variables filling the match string exactly still append");
	BotInitialChat(1,"sweep",0,Fill(name,'a',100),Fill(name+128,'b',156),"c",NULL,NULL,NULL,NULL,NULL);
	Check(strlen(state.chatmessage)==101 && state.chatmessage[100]=='c',
	      "one byte past capacity leaves only that variable unset");
}

/** A reply variable that cannot fit is unset even where the template already captured that variable. */
static void SkippedReply( void ) {
	char name[512];
	printErrors=0;
	/* the template captures v0 as "sure" at offset 8, a non-zero offset the skipped v0 must not keep */
	Check(BotReplyChat(1,"are you sure",0,0,Fill(name,'v',300),NULL,NULL,NULL,NULL,NULL,"Sarge","Player") &&
	      !strcmp(state.chatmessage,"I'm not , Player.") && !printErrors,
	      "a reply variable that cannot fit is unset, not the captured text");
}

/** Deterministic generator for the retail comparison sweep. */
static unsigned int seed=306;
static int Next( int range ) { seed=seed*1103515245u+12345u; return (int)((seed>>16)%(unsigned int)range); }

/** Every input that fits produces the dbe4ddb output, through both public entry points. */
static void RetailSweep( void ) {
	char storage[MAX_MATCHVARIABLES][MAX_MESSAGE_SIZE], *vars[MAX_MATCHVARIABLES], expect[MAX_MESSAGE_SIZE];
	char message[MAX_MESSAGE_SIZE];
	bot_match_t match;
	int round, i, total, length;
	for(round=0;round<20000;round++) {
		total=(round&1)?Next(MAX_MESSAGE_SIZE-8)+8:0;
		if(total) { Fill(message,'m',total); memcpy(message,"are you ",8); }
		for(i=0;i<MAX_MATCHVARIABLES;i++) {
			vars[i]=NULL;
			if(Next(3)==0) continue;
			length=Next(4)?Next(40):Next(MAX_MESSAGE_SIZE);
			if(total+length>=MAX_MESSAGE_SIZE) continue;
			vars[i]=Fill(storage[i],'a'+i,length); total+=length;
		}
		memset(&match,0,sizeof(match));
		if(round&1) {
			/* reply: the captured text first, then the caller's variables, as in BotReplyChat */
			strcpy(match.string,message); Check(StringsMatch(&prefix,&match),"retail reply template matches");
			OriginalAppend(&match,vars);
			BotExpandChatMessage(expect,sweepText,0,&match,0,qtrue);
			replyLine.chatmessage=sweepText;
			Check(BotReplyChat(1,message,0,0,vars[0],vars[1],vars[2],vars[3],vars[4],vars[5],vars[6],vars[7]),"sweep reply");
			replyLine.chatmessage=replyText;
		} else {
			OriginalAppend(&match,vars);
			BotExpandChatMessage(expect,sweepText,0,&match,0,qfalse);
			BotInitialChat(1,"sweep",0,vars[0],vars[1],vars[2],vars[3],vars[4],vars[5],vars[6],vars[7]);
		}
		Check(!strcmp(state.chatmessage,expect),"in-bounds chat output matches dbe4ddb");
	}
}

int main( int argc, char **argv ) {
	int proof = argc>1 ? atoi(argv[1]) : -1;
	Setup();
	if(proof==0) LongReply(6);
	else if(proof==1) LongReply(90);
	else if(proof==2) LongInitial();
	else if(proof==3) { Golden(); RetailSweep(); }
	else if(proof==4) SkippedReply();
	else {
		Golden(); RetailSweep(); LongReply(90); LongReply(300); LongInitial(); SkippedReply(); LongReply(36); LongReply(6);
		puts("Initial and reply chat variables stay inside the native match string with retail output (issue #306)");
	}
	botchatstates[1]=NULL; replychats=NULL;
	return 0;
}
