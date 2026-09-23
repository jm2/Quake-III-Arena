/* Issue #245: compare chat word search and synonym replacement with the 1.32 source. */
#include "../code/botlib/be_ai_chat.c"
#include <setjmp.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

botlib_import_t botimport;
/** Fail on any divergence from the retail routines. */
static void Check( int ok, const char *message ) {
	if(!ok) { fprintf(stderr,"Bot chat word regression failed: %s\n",message); exit(1); }
}
/** Keep the chat routines linked without the full engine. */
void Com_Memcpy( void *dest, const void *src, size_t size ) { memcpy(dest,src,size); }
/** Support shared utility initialization. */
void Com_Memset( void *dest, int value, size_t size ) { memset(dest,value,size); }
/** Treat unexpected shared utility errors as regression failures. */
void QDECL Com_Error( int level, const char *format, ... ) { (void)level; (void)format; Check(0,"engine error"); }
/** Ignore unrelated shared utility output. */
void QDECL Com_Printf( const char *format, ... ) { (void)format; }

/* dbe4ddb code/botlib/be_ai_chat.c bodies; only the function names are prefixed. */
static char *OriginalStringContainsWord(char *str1, char *str2, int casesensitive)
{
	int len, i, j;

	len = strlen(str1) - strlen(str2);
	for (i = 0; i <= len; i++, str1++)
	{
		//if not at the start of the string
		if (i)
		{
			//skip to the start of the next word
			while(*str1 && *str1 != ' ' && *str1 != '.' && *str1 != ',' && *str1 != '!') str1++;
			if (!*str1) break;
			str1++;
		} //end for
		//compare the word
		for (j = 0; str2[j]; j++)
		{
			if (casesensitive)
			{
				if (str1[j] != str2[j]) break;
			} //end if
			else
			{
				if (toupper(str1[j]) != toupper(str2[j])) break;
			} //end else
		} //end for
		//if there was a word match
		if (!str2[j])
		{
			//if the first string has an end of word
			if (!str1[j] || str1[j] == ' ' || str1[j] == '.' || str1[j] == ',' || str1[j] == '!') return str1;
		} //end if
	} //end for
	return NULL;
} //end of the function OriginalStringContainsWord
static void OriginalStringReplaceWords(char *string, char *synonym, char *replacement)
{
	char *str, *str2;

	//find the synonym in the string
	str = OriginalStringContainsWord(string, synonym, qfalse);
	//if the synonym occured in the string
	while(str)
	{
		//if the synonym isn't part of the replacement which is already in the string
		//usefull for abreviations
		str2 = OriginalStringContainsWord(string, replacement, qfalse);
		while(str2)
		{
			if (str2 <= str && str < str2 + strlen(replacement)) break;
			str2 = OriginalStringContainsWord(str2+1, replacement, qfalse);
		} //end while
		if (!str2)
		{
			memmove(str + strlen(replacement), str+strlen(synonym), strlen(str+strlen(synonym))+1);
			//append the synonum replacement
			Com_Memcpy(str, replacement, strlen(replacement));
		} //end if
		//find the next synonym in the string
		str = OriginalStringContainsWord(str+strlen(replacement), synonym, qfalse);
	} //end if
} //end of the function OriginalStringReplaceWords
static void OriginalBotReplaceSynonyms(char *string, unsigned long int context)
{
	bot_synonymlist_t *syn;
	bot_synonym_t *synonym;

	for (syn = synonyms; syn; syn = syn->next)
	{
		if (!(syn->context & context)) continue;
		for (synonym = syn->firstsynonym->next; synonym; synonym = synonym->next)
		{
			OriginalStringReplaceWords(string, synonym->string, syn->firstsynonym->string);
		} //end for
	} //end for
} //end of the function OriginalBotReplaceSynonyms
static void OriginalBotReplaceReplySynonyms(char *string, unsigned long int context)
{
	char *str1, *str2, *replacement;
	bot_synonymlist_t *syn;
	bot_synonym_t *synonym;

	for (str1 = string; *str1; )
	{
		//go to the start of the next word
		while(*str1 && *str1 <= ' ') str1++;
		if (!*str1) break;
		//
		for (syn = synonyms; syn; syn = syn->next)
		{
			if (!(syn->context & context)) continue;
			for (synonym = syn->firstsynonym->next; synonym; synonym = synonym->next)
			{
				str2 = synonym->string;
				//if the synonym is not at the front of the string continue
				str2 = OriginalStringContainsWord(str1, synonym->string, qfalse);
				if (!str2 || str2 != str1) continue;
				//
				replacement = syn->firstsynonym->string;
				//if the replacement IS in front of the string continue
				str2 = OriginalStringContainsWord(str1, replacement, qfalse);
				if (str2 && str2 == str1) continue;
				//
				memmove(str1 + strlen(replacement), str1+strlen(synonym->string),
							strlen(str1+strlen(synonym->string)) + 1);
				//append the synonum replacement
				Com_Memcpy(str1, replacement, strlen(replacement));
				//
				break;
			} //end for
			//if a synonym has been replaced
			if (synonym) break;
		} //end for
		//skip over this word
		while(*str1 && *str1 > ' ') str1++;
		if (!*str1) break;
	} //end while
} //end of the function OriginalBotReplaceReplySynonyms

#define SCRATCH_SIZE 8192
#define CANARY_SIZE 64
#define LISTS 7
enum { REPLACE_WORDS, REPLACE_SYNONYMS, REPLACE_REPLY };
/* Retail syn.c shapes: growing contractions and items, shrinking words, and synonyms inside their
   replacement. The last context grows forever in the 1.32 reply routine, so replies never use it. */
static char *synonymTable[LISTS][5]={
	{"I am","I'm","Im"}, {"you are","you're","youre"}, {"who is","who's","whos"}, {"Rocket Launcher","rl","RL"},
	{"hi","hello"}, {"Rocket Launcher","launcher"}, {"team leader","leader","tl"}
};
static unsigned long synonymContexts[LISTS]={1,1,1,2,4,8,8};
static bot_synonym_t synonymEntries[LISTS][5];
static bot_synonymlist_t synonymLists[LISTS];
static char *words[]={"","I'm","I","am","I am","who's","who","is","you're","the","team","leader","team leader",
	"Sarge","rl","RL","Rocket Launcher","launcher","hi","hello","a","b","x","ab",".",", "," "};
static char *texts[]={"I'm the team leader","who's the team leader","Sarge you're the leader","rl","RL","get the rl",
	"I'm  the  team  leader"," I'm the team leader.","rl, rl!","hi hello hi","the team leader leader","",
	" ","  ",".","a"," a","  a",". a",".a","a ","a  ","a.","a. ","foo ","foo    ","a  b","a, b","a,b",
	"a!!b","a. . b","ab ab","b a","a b a","rl rl rl","tl, rl. who's"};
static char *pieces[]={"I'm","I","am","who's","who","is","you're","you","are","the","team","leader","Sarge",
	"rl","RL","Rocket","Launcher","launcher","hi","hello","a","b","x","tl","Im"};
static char *separators[]={""," "," "," ","  ",".",",","!",", ",". ","!!"};
static sigjmp_buf faultJump;
static char *guardPage;
static long pageSize;
static unsigned int seed=245;
static int searches, overreads, replacements, overflows;

/** Link the synonym table into the global list in table order. */
static void LinkSynonyms( void ) {
	int list, entry;
	for(list=0;list<LISTS;list++) {
		for(entry=0;entry<5 && synonymTable[list][entry];entry++) {
			synonymEntries[list][entry].string=synonymTable[list][entry];
			synonymEntries[list][entry].weight=1;
			synonymEntries[list][entry].next=entry<4 && synonymTable[list][entry+1] ? &synonymEntries[list][entry+1] : NULL;
		}
		synonymLists[list].context=synonymContexts[list];
		synonymLists[list].firstsynonym=synonymEntries[list];
		synonymLists[list].next=list+1<LISTS ? &synonymLists[list+1] : NULL;
	}
	synonyms=synonymLists;
}
/** Return a deterministic pseudo-random index. */
static unsigned int Random( unsigned int range ) { seed=seed*1103515245u+12345u; return (seed>>16)%range; }
/** Build text from retail words and single, doubled, leading or trailing delimiters. */
static void RandomText( char *out, size_t size, int count ) {
	const char *piece;
	out[0]=0;
	if(Random(4)==0) strcat(out,separators[Random(11)]);
	while(count-- > 0) {
		piece=pieces[Random(25)];
		if(strlen(out)+strlen(piece)+2>=size) break;
		strcat(out,piece); strcat(out,separators[Random(11)]);
	}
}
/** Return from a retail read that crossed the guard page. */
static void Fault( int signal ) { (void)signal; siglongjmp(faultJump,1); }
/** Run the retail search with its terminator on the last readable byte; -2 reports a read past it. */
static long OriginalSearch( const char *text, char *word, int sensitive ) {
	size_t length=strlen(text)+1; char *copy=guardPage+pageSize-length, *found;
	memcpy(copy,text,length);
	if(sigsetjmp(faultJump,1)) return -2;
	found=OriginalStringContainsWord(copy,word,sensitive);
	return found ? found-copy : -1;
}
/** Run the retail search over an empty tail, all a read past a trailing delimiter may use. */
static long PaddedSearch( const char *text, char *word, int sensitive ) {
	static char padded[SCRATCH_SIZE]; char *found;
	memset(padded,0,sizeof(padded)); strcpy(padded,text);
	found=OriginalStringContainsWord(padded,word,sensitive);
	return found ? found-padded : -1;
}
/** Run the current search on an exact-size heap copy. */
static long CurrentSearch( const char *text, char *word, int sensitive ) {
	size_t length=strlen(text)+1; char *copy=malloc(length), *found; long offset;
	Check(copy!=NULL,"search allocation"); memcpy(copy,text,length);
	found=StringContainsWord(copy,word,sensitive); offset=found ? found-copy : -1;
	free(copy); return offset;
}
/** Require the retail result, or its empty-tail result where 1.32 read past the terminator. */
static void CompareSearch( const char *text, char *word, int sensitive ) {
	long original=OriginalSearch(text,word,sensitive), current=CurrentSearch(text,word,sensitive);
	searches++;
	if(original==-2) { overreads++; original=PaddedSearch(text,word,sensitive); }
	if(current!=original) fprintf(stderr,"search \"%s\" for \"%s\" (%d): %ld, 1.32 %ld\n",text,word,sensitive,current,original);
	Check(current==original,"word search differs from 1.32");
}
/** Replace from identical storage; results must match unless 1.32 wrote past the capacity. */
static void CompareReplace( int mode, const char *text, size_t capacity, char *synonym, char *replacement, unsigned long context ) {
	static char original[SCRATCH_SIZE]; char *current=malloc(capacity); size_t length=strlen(text), i; int overflow=0;
	Check(current && length<capacity && capacity+CANARY_SIZE<SCRATCH_SIZE,"replacement setup");
	memset(original,0,sizeof(original)); memcpy(original,text,length+1); memcpy(current,original,capacity);
	memset(original+capacity,0xa5,CANARY_SIZE);
	if(mode==REPLACE_WORDS) {
		OriginalStringReplaceWords(original,synonym,replacement); StringReplaceWordsSized(current,synonym,replacement,capacity);
	} else if(mode==REPLACE_SYNONYMS) {
		OriginalBotReplaceSynonyms(original,context); BotReplaceSynonyms(current,context,(int)capacity);
	} else {
		OriginalBotReplaceReplySynonyms(original,context); BotReplaceReplySynonymsSized(current,context,capacity);
	}
	for(i=0;i<CANARY_SIZE;i++) if((unsigned char)original[capacity+i]!=0xa5) overflow=1;
	replacements++; overflows+=overflow;
	Check(memchr(current,0,capacity)!=NULL,"replacement lost its terminator");
	if(!overflow && strcmp(current,original))
		fprintf(stderr,"replace \"%s\" (%d, %lu): \"%s\", 1.32 \"%s\"\n",text,mode,(unsigned long)capacity,current,original);
	Check(overflow || !strcmp(current,original),"replacement differs from 1.32");
	free(current);
}
/** Compare every replacement entry point at the legacy span, a small margin and the full message size. */
static void CompareReplacements( const char *text ) {
	size_t capacities[3], length=strlen(text); unsigned long contexts[]={1,2,4,7,8,15}; int c, i, list, entry;
	capacities[0]=length+1; capacities[1]=length+1+Random(16); capacities[2]=MAX_MESSAGE_SIZE;
	for(c=0;c<3;c++) {
		if(capacities[c]>MAX_MESSAGE_SIZE) continue;
		for(list=0;list<LISTS;list++) for(entry=1;entry<5 && synonymTable[list][entry];entry++)
			CompareReplace(REPLACE_WORDS,text,capacities[c],synonymTable[list][entry],synonymTable[list][0],0);
		for(i=0;i<6;i++) {
			CompareReplace(REPLACE_SYNONYMS,text,capacities[c],NULL,NULL,contexts[i]);
			if(!(contexts[i]&8)) CompareReplace(REPLACE_REPLY,text,capacities[c],NULL,NULL,contexts[i]);
		}
	}
}
/** Replace synonyms in a fresh exact-size buffer and compare the result. */
static void Expect( const char *text, unsigned long context, int size, const char *expected, const char *message ) {
	char *buffer=malloc(size>(int)strlen(text) ? (size_t)size : strlen(text)+1);
	Check(buffer!=NULL,"expectation setup"); strcpy(buffer,text);
	BotReplaceSynonyms(buffer,context,size);
	if(strcmp(buffer,expected)) fprintf(stderr,"\"%s\" -> \"%s\"\n",text,buffer);
	Check(!strcmp(buffer,expected),message); free(buffer);
}
/** Cover capacity-limited growth, the legacy span, and differential 1.32 search/replacement. */
int main( void ) {
	char text[MAX_MESSAGE_SIZE], *message=malloc(MAX_MESSAGE_SIZE); struct sigaction action; int t, w, s;
	pageSize=sysconf(_SC_PAGESIZE);
	guardPage=mmap(NULL,pageSize*2,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
	Check(message && guardPage!=MAP_FAILED && !mprotect(guardPage+pageSize,pageSize,PROT_NONE),"guard page");
	memset(&action,0,sizeof(action)); action.sa_handler=Fault; sigemptyset(&action.sa_mask);
	Check(!sigaction(SIGSEGV,&action,NULL) && !sigaction(SIGBUS,&action,NULL),"fault handler");
	LinkSynonyms();
	Expect("I'm the team leader",1,MAX_MESSAGE_SIZE,"I am the team leader","known capacity grows I'm");
	Expect("who's the team leader",1,MAX_MESSAGE_SIZE,"who is the team leader","known capacity grows who's");
	Expect("Sarge you're the leader",1,MAX_MESSAGE_SIZE,"Sarge you are the leader","known capacity grows you're");
	Expect("rl",2,MAX_MESSAGE_SIZE,"Rocket Launcher","known capacity grows item names");
	Expect("I'm the team leader",1,21,"I am the team leader","exact growth capacity");
	Expect("I'm the team leader",1,20,"I'm the team leader","growth one byte over capacity");
	Expect("rl",2,16,"Rocket Launcher","exact item capacity");
	Expect("rl x rl",2,MAX_MESSAGE_SIZE,"Rocket Launcher x Rocket Launcher","known capacity grows every word");
	Expect("rl x rl",2,34,"Rocket Launcher x Rocket Launcher","exact capacity for every word");
	Expect("rl x rl",2,33,"Rocket Launcher x rl","later growth past capacity");
	Expect("who's the team leader",1,22,"who's the team leader","legacy span skips growth");
	Expect("rl",2,3,"rl","legacy item span skips growth");
	Expect("hello there hello",4,18,"hi there hi","legacy span shrinks");
	Expect("I'm the team leader",1,0,"I'm the team leader","missing capacity");
	Expect("hello there",4,-1,"hello there","negative capacity");
	strcpy(message,"Sarge: I'm the team leader");
	BotReplaceSynonyms(message+7,1,MAX_MESSAGE_SIZE-7); Check(!strcmp(message,"Sarge: I am the team leader"),"interior message capacity");
	memset(message,'x',MAX_MESSAGE_SIZE); strcpy(message+MAX_MESSAGE_SIZE-20,"I'm the team leader");
	BotReplaceSynonyms(message+MAX_MESSAGE_SIZE-20,1,20); Check(!strcmp(message+MAX_MESSAGE_SIZE-20,"I'm the team leader"),"interior message end");
	for(t=0;t<(int)(sizeof(texts)/sizeof(texts[0]));t++) {
		for(w=0;w<(int)(sizeof(words)/sizeof(words[0]));w++) for(s=0;s<2;s++) CompareSearch(texts[t],words[w],s);
		CompareReplacements(texts[t]);
	}
	for(t=0;t<20000;t++) {
		RandomText(text,t%50 ? 96 : MAX_MESSAGE_SIZE,t%50 ? (int)Random(12) : 200);
		for(w=0;w<(int)(sizeof(words)/sizeof(words[0]));w++) CompareSearch(text,words[w],(int)Random(2));
		if(t%4==0) CompareReplacements(text);
	}
	Check(overreads>0 && overflows>0,"differential coverage");
	printf("Compared %d searches (%d 1.32 reads past the terminator) and %d replacements (%d 1.32 overflows)\n",
		searches,overreads,replacements,overflows);
	munmap(guardPage,pageSize*2); free(message);
	puts("Bot chat word regressions passed (issue #245)"); return 0;
}
