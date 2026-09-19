/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

/*****************************************************************************
 * name:		be_ai_chat.c
 *
 * desc:		bot chat AI
 *
 * $Archive: /MissionPack/code/botlib/be_ai_chat.c $
 *
 *****************************************************************************/

#include "../game/q_shared.h"
#include "l_memory.h"
#include <limits.h>
#include <stdint.h>
#include <float.h>
#include "l_libvar.h"
#include "l_script.h"
#include "l_precomp.h"
#include "l_struct.h"
#include "l_utils.h"
#include "l_log.h"
#include "aasfile.h"
#include "../game/botlib.h"
#include "../game/be_aas.h"
#include "be_aas_funcs.h"
#include "be_interface.h"
#include "../game/be_ea.h"
#include "../game/be_ai_chat.h"


//escape character
#define ESCAPE_CHAR				0x01	//'_'
//
// "hi ", people, " ", 0, " entered the game"
//becomes:
// "hi _rpeople_ _v0_ entered the game"
//

//match piece types
#define MT_VARIABLE					1		//variable match piece
#define MT_STRING					2		//string match piece
//reply chat key flags
#define RCKFL_AND					1		//key must be present
#define RCKFL_NOT					2		//key must be absent
#define RCKFL_NAME					4		//name of bot must be present
#define RCKFL_STRING				8		//key is a string
#define RCKFL_VARIABLES				16		//key is a match template
#define RCKFL_BOTNAMES				32		//key is a series of botnames
#define RCKFL_GENDERFEMALE			64		//bot must be female
#define RCKFL_GENDERMALE			128		//bot must be male
#define RCKFL_GENDERLESS			256		//bot must be genderless
//time to ignore a chat message after using it
#define CHATMESSAGE_RECENTTIME	20

//the actuall chat messages
typedef struct bot_chatmessage_s
{
	char *chatmessage;					//chat message string
	float time;							//last time used
	struct bot_chatmessage_s *next;		//next chat message in a list
} bot_chatmessage_t;
//bot chat type with chat lines
typedef struct bot_chattype_s
{
	char name[MAX_CHATTYPE_NAME];
	int numchatmessages;
	bot_chatmessage_t *firstchatmessage;
	struct bot_chattype_s *next;
} bot_chattype_t;
//bot chat lines
typedef struct bot_chat_s
{
	bot_chattype_t *types;
} bot_chat_t;

//random string
typedef struct bot_randomstring_s
{
	char *string;
	struct bot_randomstring_s *next;
} bot_randomstring_t;
//list with random strings
typedef struct bot_randomlist_s
{
	char *string;
	int numstrings;
	bot_randomstring_t *firstrandomstring;
	struct bot_randomlist_s *next;
} bot_randomlist_t;

//synonym
typedef struct bot_synonym_s
{
	char *string;
	float weight;
	struct bot_synonym_s *next;
} bot_synonym_t;
//list with synonyms
typedef struct bot_synonymlist_s
{
	unsigned long int context;
	float totalweight;
	bot_synonym_t *firstsynonym;
	struct bot_synonymlist_s *next;
} bot_synonymlist_t;

//fixed match string
typedef struct bot_matchstring_s
{
	char *string;
	struct bot_matchstring_s *next;
} bot_matchstring_t;

//piece of a match template
typedef struct bot_matchpiece_s
{
	int type;
	bot_matchstring_t *firststring;
	int variable;
	struct bot_matchpiece_s *next;
} bot_matchpiece_t;
//match template
typedef struct bot_matchtemplate_s
{
	unsigned long int context;
	int type;
	int subtype;
	bot_matchpiece_t *first;
	struct bot_matchtemplate_s *next;
} bot_matchtemplate_t;

//reply chat key
typedef struct bot_replychatkey_s
{
	int flags;
	char *string;
	bot_matchpiece_t *match;
	struct bot_replychatkey_s *next;
} bot_replychatkey_t;
//reply chat
typedef struct bot_replychat_s
{
	bot_replychatkey_t *keys;
	float priority;
	int numchatmessages;
	bot_chatmessage_t *firstchatmessage;
	struct bot_replychat_s *next;
} bot_replychat_t;

//string list
typedef struct bot_stringlist_s
{
	char *string;
	struct bot_stringlist_s *next;
} bot_stringlist_t;

//chat state of a bot
typedef struct bot_chatstate_s
{
	int gender;											//0=it, 1=female, 2=male
	int client;											//client number
	char name[32];										//name of the bot
	char chatmessage[MAX_MESSAGE_SIZE];
	int handle;
	//the console messages visible to the bot
	bot_consolemessage_t *firstmessage;			//first message is the first typed message
	bot_consolemessage_t *lastmessage;			//last message is the last typed message, bottom of console
	//number of console messages stored in the state
	int numconsolemessages;
	//the bot chat lines
	bot_chat_t *chat;
} bot_chatstate_t;

typedef struct {
	bot_chat_t	*chat;
	char		filename[MAX_QPATH];
	char		chatname[MAX_QPATH];
} bot_ichatdata_t;

bot_ichatdata_t	*ichatdata[MAX_CLIENTS];

bot_chatstate_t *botchatstates[MAX_CLIENTS+1];
//console message heap
bot_consolemessage_t *consolemessageheap = NULL;
bot_consolemessage_t *freeconsolemessages = NULL;
static int consolemessageheapcount;
//list with match strings
bot_matchtemplate_t *matchtemplates = NULL;
//list with synonyms
bot_synonymlist_t *synonyms = NULL;
//list with random strings
bot_randomlist_t *randomstrings = NULL;
//reply chats
bot_replychat_t *replychats = NULL;

//========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//========================================================================
bot_chatstate_t *BotChatStateFromHandle(int handle)
{
	if (handle <= 0 || handle > MAX_CLIENTS)
	{
		botimport.Print(PRT_FATAL, "chat state handle %d out of range\n", handle);
		return NULL;
	} //end if
	if (!botchatstates[handle])
	{
		botimport.Print(PRT_FATAL, "invalid chat state %d\n", handle);
		return NULL;
	} //end if
	return botchatstates[handle];
} //end of the function BotChatStateFromHandle
//===========================================================================
// initialize the heap with unused console messages
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
static int InitConsoleMessageHeapChecked(void)
{
	int i, j, max_messages, used = 0;
	libvar_t *variable;
	unsigned int bits;
	volatile unsigned int representation;
	bot_consolemessage_t *candidate, *message, *previous;
	bot_consolemessage_t *first[MAX_CLIENTS + 1] = { NULL };
	bot_consolemessage_t *last[MAX_CLIENTS + 1] = { NULL };
	bot_chatstate_t *cs;
	unsigned char *seen = NULL;
	uintptr_t base, address, offset;
	unsigned long slot;

	variable = LibVar("max_messages", "1024");
	if (!variable)
	{
		botimport.Print(PRT_ERROR, "couldn't initialize max_messages\n");
		return qfalse;
	}
	Com_Memcpy(&bits, &variable->value, sizeof(bits));
	representation = bits;
	if ((representation & 0x7f800000U) == 0x7f800000U ||
			(double)variable->value < INT_MIN || (double)variable->value > INT_MAX)
	{
		botimport.Print(PRT_ERROR, "invalid max_messages\n");
		return qfalse;
	}
	max_messages = (int)variable->value;
	if (max_messages < 1 || (unsigned long)max_messages >
			(unsigned long)INT_MAX / sizeof(bot_consolemessage_t))
	{
		botimport.Print(PRT_ERROR, "invalid console message heap size\n");
		return qfalse;
	}
	for (i = 1; i <= MAX_CLIENTS; i++)
	{
		cs = botchatstates[i];
		if (!cs) continue;
		if (cs->numconsolemessages < 0 || cs->numconsolemessages > max_messages - used ||
				cs->numconsolemessages > consolemessageheapcount - used ||
				(!consolemessageheap && cs->numconsolemessages)) goto invalidqueue;
		used += cs->numconsolemessages;
	}
	if (used > 0)
	{
		seen = (unsigned char *) GetClearedMemory(consolemessageheapcount);
		if (!seen)
		{
			botimport.Print(PRT_ERROR, "couldn't stage console message ownership\n");
			return qfalse;
		}
	}
	base = (uintptr_t)consolemessageheap;
	used = 0;
	// Validate complete existing queues before acquiring persistent storage.
	for (i = 1; i <= MAX_CLIENTS; i++)
	{
		cs = botchatstates[i];
		if (!cs) continue;
		if (cs->numconsolemessages < 0 || cs->numconsolemessages > max_messages - used ||
				cs->numconsolemessages > consolemessageheapcount - used ||
				(!consolemessageheap && cs->numconsolemessages)) goto invalidqueue;
		previous = NULL;
		message = cs->firstmessage;
		for (j = 0; j < cs->numconsolemessages; j++)
		{
			address = (uintptr_t)message;
			if (address < base) goto invalidqueue;
			offset = address - base;
			if (offset >= (uintptr_t)consolemessageheapcount * sizeof(bot_consolemessage_t) ||
					offset % sizeof(bot_consolemessage_t)) goto invalidqueue;
			slot = offset / sizeof(bot_consolemessage_t);
			if (seen[slot]) goto invalidqueue;
			seen[slot] = 1;
			if (message->prev != previous) goto invalidqueue;
			previous = message;
			message = message->next;
		}
		if (message || previous != cs->lastmessage) goto invalidqueue;
		used += cs->numconsolemessages;
	}
	candidate = (bot_consolemessage_t *) GetClearedHunkMemory(
			(unsigned long)max_messages * sizeof(bot_consolemessage_t));
	if (!candidate)
	{
		if (seen) FreeMemory(seen);
		botimport.Print(PRT_ERROR, "couldn't allocate console message heap\n");
		return qfalse;
	}
	used = 0;
	for (i = 1; i <= MAX_CLIENTS; i++)
	{
		cs = botchatstates[i];
		if (!cs || !cs->numconsolemessages) continue;
		first[i] = &candidate[used];
		message = cs->firstmessage;
		for (j = 0; j < cs->numconsolemessages; j++, used++)
		{
			candidate[used] = *message;
			candidate[used].prev = j ? &candidate[used - 1] : NULL;
			candidate[used].next = j + 1 < cs->numconsolemessages ? &candidate[used + 1] : NULL;
			message = message->next;
		}
		last[i] = &candidate[used - 1];
	}
	for (j = used; j < max_messages; j++)
	{
		candidate[j].prev = j > used ? &candidate[j - 1] : NULL;
		candidate[j].next = j + 1 < max_messages ? &candidate[j + 1] : NULL;
	}
	if (seen) FreeMemory(seen);
	if (consolemessageheap) FreeMemory(consolemessageheap);
	consolemessageheap = candidate;
	consolemessageheapcount = max_messages;
	freeconsolemessages = used < max_messages ? &candidate[used] : NULL;
	for (i = 1; i <= MAX_CLIENTS; i++)
	{
		cs = botchatstates[i];
		if (!cs) continue;
		cs->firstmessage = first[i];
		cs->lastmessage = last[i];
	}
	return qtrue;

invalidqueue:
	if (seen) FreeMemory(seen);
	botimport.Print(PRT_ERROR, "console message queues do not fit the complete heap\n");
	return qfalse;
}

void InitConsoleMessageHeap(void)
{
	InitConsoleMessageHeapChecked();
} //end of the function InitConsoleMessageHeap
//===========================================================================
// allocate one console message from the heap
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
bot_consolemessage_t *AllocConsoleMessage(void)
{
	bot_consolemessage_t *message;
	message = freeconsolemessages;
	if (freeconsolemessages) freeconsolemessages = freeconsolemessages->next;
	if (freeconsolemessages) freeconsolemessages->prev = NULL;
	return message;
} //end of the function AllocConsoleMessage
//===========================================================================
// deallocate one console message from the heap
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void FreeConsoleMessage(bot_consolemessage_t *message)
{
	if (freeconsolemessages) freeconsolemessages->prev = message;
	message->prev = NULL;
	message->next = freeconsolemessages;
	freeconsolemessages = message;
} //end of the function FreeConsoleMessage
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void BotRemoveConsoleMessage(int chatstate, int handle)
{
	bot_consolemessage_t *m, *nextm;
	bot_chatstate_t *cs;

	cs = BotChatStateFromHandle(chatstate);
	if (!cs) return;

	for (m = cs->firstmessage; m; m = nextm)
	{
		nextm = m->next;
		if (m->handle == handle)
		{
			if (m->next) m->next->prev = m->prev;
			else cs->lastmessage = m->prev;
			if (m->prev) m->prev->next = m->next;
			else cs->firstmessage = m->next;

			FreeConsoleMessage(m);
			cs->numconsolemessages--;
			break;
		} //end if
	} //end for
} //end of the function BotRemoveConsoleMessage
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void BotQueueConsoleMessage(int chatstate, int type, char *message)
{
	bot_consolemessage_t *m;
	bot_chatstate_t *cs;

	cs = BotChatStateFromHandle(chatstate);
	if (!cs) return;

	if (!message)
	{
		botimport.Print(PRT_ERROR, "missing console message input\n");
		return;
	}
	m = AllocConsoleMessage();
	if (!m)
	{
		botimport.Print(PRT_ERROR, "empty console message heap\n");
		return;
	} //end if
	cs->handle++;
	if (cs->handle <= 0 || cs->handle > 8192) cs->handle = 1;
	m->handle = cs->handle;
	m->time = AAS_Time();
	m->type = type;
	Q_strncpyz(m->message, message, sizeof(m->message));
	m->next = NULL;
	if (cs->lastmessage)
	{
		cs->lastmessage->next = m;
		m->prev = cs->lastmessage;
		cs->lastmessage = m;
	} //end if
	else
	{
		cs->lastmessage = m;
		cs->firstmessage = m;
		m->prev = NULL;
	} //end if
	cs->numconsolemessages++;
} //end of the function BotQueueConsoleMessage
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
int BotNextConsoleMessage(int chatstate, bot_consolemessage_t *cm)
{
	bot_chatstate_t *cs;

	cs = BotChatStateFromHandle(chatstate);
	if (!cs) return 0;
	if (cs->firstmessage)
	{
		if (!cm)
		{
			botimport.Print(PRT_ERROR, "missing console message output\n");
			return 0;
		}
		Com_Memcpy(cm, cs->firstmessage, sizeof(bot_consolemessage_t));
		cm->next = cm->prev = NULL;
		return cm->handle;
	} //end if
	return 0;
} //end of the function BotConsoleMessage
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
int BotNumConsoleMessages(int chatstate)
{
	bot_chatstate_t *cs;

	cs = BotChatStateFromHandle(chatstate);
	if (!cs) return 0;
	return cs->numconsolemessages;
} //end of the function BotNumConsoleMessages
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
int IsWhiteSpace(char c)
{
	if ((c >= 'a' && c <= 'z')
		|| (c >= 'A' && c <= 'Z')
		|| (c >= '0' && c <= '9')
		|| c == '(' || c == ')'
		|| c == '?' || c == ':'
		|| c == '\''|| c == '/'
		|| c == ',' || c == '.'
		|| c == '['	|| c == ']'
		|| c == '-' || c == '_'
		|| c == '+' || c == '=') return qfalse;
	return qtrue;
} //end of the function IsWhiteSpace
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
void BotRemoveTildes(char *message)
{
	int i;

	//remove all tildes from the chat message
	for (i = 0; message[i]; i++)
	{
		if (message[i] == '~')
		{
			memmove(&message[i], &message[i+1], strlen(&message[i+1])+1);
		} //end if
	} //end for
} //end of the function BotRemoveTildes
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void UnifyWhiteSpaces(char *string)
{
	char *ptr, *oldptr;

	for (ptr = oldptr = string; *ptr; oldptr = ptr)
	{
		while(*ptr && IsWhiteSpace(*ptr)) ptr++;
		if (ptr > oldptr)
		{
			//if not at the start and not at the end of the string
			//write only one space
			if (oldptr > string && *ptr) *oldptr++ = ' ';
			//remove all other white spaces
			if (ptr > oldptr) memmove(oldptr, ptr, strlen(ptr)+1);
		} //end if
		while(*ptr && !IsWhiteSpace(*ptr)) ptr++;
	} //end while
} //end of the function UnifyWhiteSpaces
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
int StringContains(char *str1, char *str2, int casesensitive)
{
	int len, i, j, index;

	if (str1 == NULL || str2 == NULL) return -1;

	len = strlen(str1) - strlen(str2);
	index = 0;
	for (i = 0; i <= len; i++, str1++, index++)
	{
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
		if (!str2[j]) return index;
	} //end for
	return -1;
} //end of the function StringContains
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
/** Find a whole word without stepping beyond the input terminator. */
char *StringContainsWord(char *str1, char *str2, int casesensitive)
{
	char *word;
	int j;
	if (!*str2) return str1;
	for (word = str1; *word; word++) {
		if (word > str1 && word[-1] != ' ' && word[-1] != '.' && word[-1] != ',' && word[-1] != '!') continue;
		for (j = 0; str2[j] && word[j]; j++) {
			if (casesensitive ? word[j] != str2[j] : toupper((unsigned char)word[j]) != toupper((unsigned char)str2[j])) break;
		}
		if (!str2[j] && (!word[j] || word[j] == ' ' || word[j] == '.' || word[j] == ',' || word[j] == '!')) return word;
	}
	return NULL;
}
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
/** Replace a word only within the capacity established by its caller. */
static qboolean BotReplaceChatWord( char *string, char *at, const char *synonym, const char *replacement, size_t capacity ) {
	size_t length = strlen(string), oldLength = strlen(synonym), newLength = strlen(replacement);
	if ( capacity > MAX_MESSAGE_SIZE ) capacity = MAX_MESSAGE_SIZE;
	if ( !oldLength || length >= capacity || oldLength > length ||
	     newLength >= capacity - (length - oldLength) ) return qfalse;
	memmove( at + newLength, at + oldLength, strlen(at + oldLength) + 1 );
	memcpy( at, replacement, newLength );
	return qtrue;
}

/** Replace whole words within explicit storage and bound the next search position. */
static void StringReplaceWordsSized(char *string, char *synonym, char *replacement, size_t capacity)
{
	char *str, *str2;

	if (!*synonym) return;
	//find the synonym in the string
	str = StringContainsWord(string, synonym, qfalse);
	//if the synonym occured in the string
	while(str)
	{
		//if the synonym isn't part of the replacement which is already in the string
		//usefull for abreviations
		str2 = *replacement ? StringContainsWord(string, replacement, qfalse) : NULL;
		while(str2)
		{
			if (str2 <= str && str < str2 + strlen(replacement)) break;
			str2 = StringContainsWord(str2+1, replacement, qfalse);
		} //end while
		if (!str2)
		{
			if (!BotReplaceChatWord(string, str, synonym, replacement, capacity)) break;
		} //end if
		//find the next synonym in the string
		str = StringContainsWord(str + strlen(str2 ? synonym : replacement), synonym, qfalse);
	} //end if
} //end of the function StringReplaceWords
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void BotDumpSynonymList(bot_synonymlist_t *synlist)
{
	FILE *fp;
	bot_synonymlist_t *syn;
	bot_synonym_t *synonym;

	fp = Log_FilePointer();
	if (!fp) return;
	for (syn = synlist; syn; syn = syn->next)
	{
	        fprintf(fp, "%ld : [", syn->context);
		for (synonym = syn->firstsynonym; synonym; synonym = synonym->next)
		{
			fprintf(fp, "(\"%s\", %1.2f)", synonym->string, synonym->weight);
			if (synonym->next) fprintf(fp, ", ");
		} //end for
		fprintf(fp, "]\n");
	} //end for
} //end of the function BotDumpSynonymList
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
/* Count/claim aligned private entries before advancing either parse pass. */
static int BotSynonymReserve(int *used, int bytes, int alignment, int capacity,
		char *block, char **entry)
{
	int padding = (alignment - *used % alignment) % alignment;
	if (bytes < 0 || padding > INT_MAX - *used ||
			bytes > INT_MAX - *used - padding) return qfalse;
	if (*used + padding + bytes > capacity) return qfalse;
	*used += padding;
	if (block) *entry = block + *used;
	*used += bytes;
	return qtrue;
}

static int BotSynonymFloatFinite(const float *value)
{
	unsigned int bits;
	volatile unsigned int representation;
	Com_Memcpy(&bits, value, sizeof(bits));
	representation = bits;
	return (representation & 0x7f800000u) != 0x7f800000u;
}

bot_synonymlist_t *BotLoadSynonyms(char *filename)
{
	int pass, size, used, contextlevel, numsynonyms, stringsize;
	float weight, totalweight;
	unsigned long int context, contextstack[32];
	char *ptr = NULL, *staged = NULL, *published;
	source_t *source = NULL;
	token_t token;
	bot_synonymlist_t *synlist, *lastsyn, *syn;
	bot_synonym_t *synonym, *lastsynonym;

	if (!filename || !*filename)
	{
		botimport.Print(PRT_ERROR, "missing synonym filename\n");
		return NULL;
	} //end if
	size = 0;
	synlist = NULL; //make compiler happy
	syn = NULL; //make compiler happy
	synonym = NULL; //make compiler happy
	//the synonyms are parsed in two phases
	for (pass = 0; pass < 2; pass++)
	{
		//
		if (pass && size)
		{
			staged = (char *) GetClearedMemory(size);
			if (!staged)
			{
				botimport.Print(PRT_ERROR, "could not stage synonyms\n");
				return NULL;
			} //end if
		} //end if
		used = 0;
		//
		PC_SetBaseFolder(BOTFILESBASEFOLDER);
		source = LoadSourceFile(filename);
		if (!source)
		{
			botimport.Print(PRT_ERROR, "counldn't load %s\n", filename);
			goto failed;
		} //end if
		//
		context = 0;
		contextlevel = 0;
		synlist = NULL; //list synonyms
		lastsyn = NULL; //last synonym in the list
		//
		while(PC_ReadToken(source, &token))
		{
			if (token.type == TT_NUMBER)
			{
				context |= token.intvalue;
				contextstack[contextlevel] = token.intvalue;
				contextlevel++;
				if (contextlevel >= 32)
				{
					SourceError(source, "more than 32 context levels");
					goto failed;
				} //end if
				if (!PC_ExpectTokenString(source, "{"))
				{
					goto failed;
				} //end if
			} //end if
			else if (token.type == TT_PUNCTUATION)
			{
				if (!strcmp(token.string, "}"))
				{
					contextlevel--;
					if (contextlevel < 0)
					{
						SourceError(source, "too many }");
						goto failed;
					} //end if
					context &= ~contextstack[contextlevel];
				} //end if
				else if (!strcmp(token.string, "["))
				{
					if (!BotSynonymReserve(&used, sizeof(bot_synonymlist_t), sizeof(void *),
							pass ? size : INT_MAX, staged, &ptr))
					{
						SourceError(source, "synonym list exceeds measured capacity");
						goto failed;
					} //end if
					totalweight = 0;
					if (pass)
					{
						syn = (bot_synonymlist_t *) ptr;
						syn->context = context;
						syn->firstsynonym = NULL;
						syn->next = NULL;
						if (lastsyn) lastsyn->next = syn;
						else synlist = syn;
						lastsyn = syn;
					} //end if
					numsynonyms = 0;
					lastsynonym = NULL;
					while(1)
					{
						if (!PC_ExpectTokenString(source, "(") ||
							!PC_ExpectTokenType(source, TT_STRING, 0, &token))
						{
							goto failed;
						} //end if
						StripDoubleQuotes(token.string);
						if (strlen(token.string) <= 0)
						{
							SourceError(source, "empty string", token.string);
							goto failed;
						} //end if
						stringsize = (int)strlen(token.string) + 1;
						if (!BotSynonymReserve(&used, sizeof(bot_synonym_t), sizeof(void *),
								pass ? size : INT_MAX, staged, &ptr))
						{
							SourceError(source, "synonym exceeds measured capacity");
							goto failed;
						} //end if
						if (pass)
						{
							synonym = (bot_synonym_t *) ptr;
						} //end if
						if (!BotSynonymReserve(&used, stringsize, 1,
								pass ? size : INT_MAX, staged, &ptr))
						{
							SourceError(source, "synonym string exceeds measured capacity");
							goto failed;
						} //end if
						if (pass)
						{
							synonym->string = ptr;
							strcpy(synonym->string, token.string);
							//
							if (lastsynonym) lastsynonym->next = synonym;
							else syn->firstsynonym = synonym;
							lastsynonym = synonym;
						} //end if
						numsynonyms++;
						if (!PC_ExpectTokenString(source, ",") ||
							!PC_ExpectTokenType(source, TT_NUMBER, 0, &token) ||
							!PC_ExpectTokenString(source, ")"))
						{
							goto failed;
						} //end if
						if (!(token.floatvalue >= -FLT_MAX && token.floatvalue <= FLT_MAX))
						{
							SourceError(source, "synonym weight is not representable");
							goto failed;
						} //end if
						weight = (float)token.floatvalue;
						totalweight += weight;
						if (!BotSynonymFloatFinite(&weight) || !BotSynonymFloatFinite(&totalweight))
						{
							SourceError(source, "synonym weight is not finite");
							goto failed;
						} //end if
						if (pass)
						{
							synonym->weight = weight;
							syn->totalweight = totalweight;
						} //end if
						if (PC_CheckTokenString(source, "]")) break;
						if (!PC_ExpectTokenString(source, ","))
						{
							goto failed;
						} //end if
					} //end while
					if (numsynonyms < 2)
					{
						SourceError(source, "synonym must have at least two entries\n");
						goto failed;
					} //end if
				} //end else
				else
				{
					SourceError(source, "unexpected %s", token.string);
					goto failed;
				} //end if
			} //end else if
		} //end while
		//
		if (contextlevel > 0)
		{
			SourceError(source, "missing }");
			goto failed;
		} //end if
		if (PC_SourceHasError(source)) goto failed;
		FreeSource(source);
		source = NULL;
		if (!pass) size = used;
		else if (used != size)
		{
			botimport.Print(PRT_ERROR, "synonyms changed between parse passes\n");
			goto failed;
		} //end else if
	} //end for
	if (size)
	{
		published = (char *) GetClearedHunkMemory(size);
		if (!published)
		{
			botimport.Print(PRT_ERROR, "could not publish synonyms\n");
			goto failed;
		} //end if
		Com_Memcpy(published, staged, size);
		for (syn = synlist; syn; syn = syn->next)
		{
			bot_synonymlist_t *out = (bot_synonymlist_t *)(published + ((char *)syn - staged));
			out->next = syn->next ? (bot_synonymlist_t *)(published + ((char *)syn->next - staged)) : NULL;
			out->firstsynonym = (bot_synonym_t *)(published + ((char *)syn->firstsynonym - staged));
			for (synonym = syn->firstsynonym; synonym; synonym = synonym->next)
			{
				bot_synonym_t *entry = (bot_synonym_t *)(published + ((char *)synonym - staged));
				entry->next = synonym->next ? (bot_synonym_t *)(published + ((char *)synonym->next - staged)) : NULL;
				entry->string = published + (synonym->string - staged);
			} //end for
		} //end for
		synlist = (bot_synonymlist_t *)(published + ((char *)synlist - staged));
		FreeMemory(staged);
	} //end if
	botimport.Print(PRT_MESSAGE, "loaded %s\n", filename);
	//
	//BotDumpSynonymList(synlist);
	//
	return synlist;
failed:
	if (source) FreeSource(source);
	if (staged) FreeMemory(staged);
	return NULL;
} //end of the function BotLoadSynonyms
//===========================================================================
// replace all the synonyms in the string
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
/** Normalize synonyms in an internal chat buffer whose allocation size is known. */
static void BotReplaceSynonymsSized(char *string, unsigned long int context, size_t capacity)
{
	bot_synonymlist_t *syn;
	bot_synonym_t *synonym;

	for (syn = synonyms; syn; syn = syn->next)
	{
		if (!(syn->context & context)) continue;
		for (synonym = syn->firstsynonym->next; synonym; synonym = synonym->next)
		{
			StringReplaceWordsSized(string, synonym->string, syn->firstsynonym->string, capacity);
		} //end for
	} //end for
}

/** Preserve the size-less retail ABI by keeping replacements within the original string span. */
void BotReplaceSynonyms(char *string, unsigned long int context) {
	if ( string ) BotReplaceSynonymsSized(string, context, strlen(string) + 1);
}
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void BotReplaceWeightedSynonyms(char *string, unsigned long int context)
{
	bot_synonymlist_t *syn;
	bot_synonym_t *synonym, *replacement;
	float weight, curweight;

	for (syn = synonyms; syn; syn = syn->next)
	{
		if (!(syn->context & context)) continue;
		//choose a weighted random replacement synonym
		weight = random() * syn->totalweight;
		if (!weight) continue;
		curweight = 0;
		for (replacement = syn->firstsynonym; replacement; replacement = replacement->next)
		{
			curweight += replacement->weight;
			if (weight < curweight) break;
		} //end for
		if (!replacement) continue;
		//replace all synonyms with the replacement
		for (synonym = syn->firstsynonym; synonym; synonym = synonym->next)
		{
			if (synonym == replacement) continue;
			StringReplaceWordsSized(string, synonym->string, replacement->string, MAX_MESSAGE_SIZE);
		} //end for
	} //end for
} //end of the function BotReplaceWeightedSynonyms
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
/** Normalize reply variables within their caller-owned temporary chat buffer. */
static void BotReplaceReplySynonymsSized(char *string, unsigned long int context, size_t capacity)
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
				str2 = StringContainsWord(str1, synonym->string, qfalse);
				if (!str2 || str2 != str1) continue;
				//
				replacement = syn->firstsynonym->string;
				//if the replacement IS in front of the string continue
				str2 = StringContainsWord(str1, replacement, qfalse);
				if (str2 && str2 == str1) continue;
				//
				if (!BotReplaceChatWord(string, str1, synonym->string, replacement, capacity)) return;
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
} //end of the function BotReplaceReplySynonymsSized
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
int BotLoadChatMessage(source_t *source, char *chatmessagestring)
{
	token_t token;
	char staged[MAX_MESSAGE_SIZE], component[MAX_MESSAGE_SIZE], *value;
	size_t used = 0, bytes;

	if (!source || !chatmessagestring)
	{
		botimport.Print(PRT_ERROR, "missing encoded message source/output\n");
		return qfalse;
	}
	staged[0] = 0;
	while (1)
	{
		if (!PC_ExpectAnyToken(source, &token)) return qfalse;
		if (token.type == TT_STRING)
		{
			StripDoubleQuotes(token.string);
			value = token.string;
		}
		else if (token.type == TT_NUMBER && (token.subtype & TT_INTEGER))
		{
			if (token.intvalue >= MAX_MATCHVARIABLES)
			{
				SourceError(source, "chat variable exceeds native slot range");
				return qfalse;
			}
			component[0] = ESCAPE_CHAR;
			component[1] = 'v';
			component[2] = '0' + token.intvalue;
			component[3] = ESCAPE_CHAR;
			component[4] = 0;
			value = component;
		}
		else if (token.type == TT_NAME)
		{
			bytes = strlen(token.string);
			if (bytes > sizeof(component) - 4) goto toolong;
			component[0] = ESCAPE_CHAR;
			component[1] = 'r';
			Com_Memcpy(component + 2, token.string, bytes);
			component[bytes + 2] = ESCAPE_CHAR;
			component[bytes + 3] = 0;
			value = component;
		}
		else
		{
			SourceError(source, "unknown message component %s", token.string);
			return qfalse;
		}
		bytes = strlen(value);
		if (bytes >= sizeof(staged) - used) goto toolong;
		Com_Memcpy(staged + used, value, bytes + 1);
		used += bytes;
		if (PC_CheckTokenString(source, ";")) break;
		if (!PC_ExpectTokenString(source, ",")) return qfalse;
	}
	strcpy(chatmessagestring, staged);
	return qtrue;
toolong:
	SourceError(source, "chat message too long");
	return qfalse;
} //end of the function BotLoadChatMessage
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void BotDumpRandomStringList(bot_randomlist_t *randomlist)
{
	FILE *fp;
	bot_randomlist_t *random;
	bot_randomstring_t *rs;

	fp = Log_FilePointer();
	if (!fp) return;
	for (random = randomlist; random; random = random->next)
	{
		fprintf(fp, "%s = {", random->string);
		for (rs = random->firstrandomstring; rs; rs = rs->next)
		{
			fprintf(fp, "\"%s\"", rs->string);
			if (rs->next) fprintf(fp, ", ");
			else fprintf(fp, "}\n");
		} //end for
	} //end for
} //end of the function BotDumpRandomStringList
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
bot_randomlist_t *BotLoadRandomStrings(char *filename)
{
	int pass, size = 0, used, bytes;
	char *ptr, *staged = NULL, *published, chatmessagestring[MAX_MESSAGE_SIZE];
	source_t *source = NULL;
	token_t token;
	bot_randomlist_t *randomlist = NULL, *lastrandom, *random = NULL, *out;
	bot_randomstring_t *randomstring, *entry;

	if (!filename || !filename[0] || strlen(filename) >= MAX_PATH)
	{
		botimport.Print(PRT_ERROR, "invalid random dictionary filename\n");
		return NULL;
	}
	for (pass = 0; pass < 2; pass++)
	{
		if (pass && size)
		{
			staged = (char *)GetClearedMemory(size);
			if (!staged) goto failed;
		}
		used = 0;
		PC_SetBaseFolder(BOTFILESBASEFOLDER);
		source = LoadSourceFile(filename);
		if (!source) goto failed;
		randomlist = lastrandom = NULL;
		while (!PC_SourceHasError(source) && PC_ReadToken(source, &token))
		{
			if (PC_SourceHasError(source)) goto failed;
			if (token.type != TT_NAME)
			{
				SourceError(source, "unknown random %s", token.string);
				goto failed;
			}
			if (!BotSynonymReserve(&used, sizeof(bot_randomlist_t), sizeof(void *),
					pass ? size : INT_MAX, staged, &ptr)) goto excessive;
			if (pass) random = (bot_randomlist_t *)ptr;
			bytes = (int)strlen(token.string) + 1;
			if (!BotSynonymReserve(&used, bytes, 1, pass ? size : INT_MAX, staged, &ptr)) goto excessive;
			if (pass)
			{
				random->string = ptr;
				strcpy(ptr, token.string);
				if (lastrandom) lastrandom->next = random;
				else randomlist = random;
				lastrandom = random;
			}
			if (!PC_ExpectTokenString(source, "=") || !PC_ExpectTokenString(source, "{")) goto failed;
			while (1)
			{
				if (PC_SourceHasError(source)) goto failed;
				if (PC_CheckTokenString(source, "}")) break;
				if (PC_SourceHasError(source)) goto failed;
				if (!BotLoadChatMessage(source, chatmessagestring) || PC_SourceHasError(source)) goto failed;
				if (!BotSynonymReserve(&used, sizeof(bot_randomstring_t), sizeof(void *),
						pass ? size : INT_MAX, staged, &ptr)) goto excessive;
				if (pass) randomstring = (bot_randomstring_t *)ptr;
				bytes = (int)strlen(chatmessagestring) + 1;
				if (!BotSynonymReserve(&used, bytes, 1, pass ? size : INT_MAX, staged, &ptr)) goto excessive;
				if (pass)
				{
					randomstring->string = ptr;
					strcpy(ptr, chatmessagestring);
					random->numstrings++;
					randomstring->next = random->firstrandomstring;
					random->firstrandomstring = randomstring;
				}
			}
		}
		if (PC_SourceHasError(source)) goto failed;
		FreeSource(source);
		source = NULL;
		if (!pass) size = used;
		else if (used != size) goto excessive;
	}
	if (size)
	{
		published = (char *)GetClearedHunkMemory(size);
		if (!published) goto failed;
		Com_Memcpy(published, staged, size);
		for (random = randomlist; random; random = random->next)
		{
			out = (bot_randomlist_t *)(published + ((char *)random - staged));
			out->string = published + (random->string - staged);
			out->next = random->next ? (bot_randomlist_t *)(published + ((char *)random->next - staged)) : NULL;
			out->firstrandomstring = random->firstrandomstring ?
				(bot_randomstring_t *)(published + ((char *)random->firstrandomstring - staged)) : NULL;
			for (randomstring = random->firstrandomstring; randomstring; randomstring = randomstring->next)
			{
				entry = (bot_randomstring_t *)(published + ((char *)randomstring - staged));
				entry->string = published + (randomstring->string - staged);
				entry->next = randomstring->next ?
					(bot_randomstring_t *)(published + ((char *)randomstring->next - staged)) : NULL;
			}
		}
		randomlist = (bot_randomlist_t *)(published + ((char *)randomlist - staged));
		FreeMemory(staged);
	}
	botimport.Print(PRT_MESSAGE, "loaded %s\n", filename);
	return randomlist;
excessive:
	botimport.Print(PRT_ERROR, "random dictionary exceeds measured native capacity\n");
failed:
	if (source) FreeSource(source);
	if (staged) FreeMemory(staged);
	botimport.Print(PRT_ERROR, "could not load complete random dictionary\n");
	return NULL;
} //end of the function BotLoadRandomStrings
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
char *RandomString(char *name)
{
	bot_randomlist_t *random;
	bot_randomstring_t *rs;
	int i;

	for (random = randomstrings; random; random = random->next)
	{
		if (!strcmp(random->string, name))
		{
			i = random() * random->numstrings;
			for (rs = random->firstrandomstring; rs; rs = rs->next)
			{
				if (--i < 0) break;
			} //end for
			if (rs)
			{
				return rs->string;
			} //end if
		} //end for
	} //end for
	return NULL;
} //end of the function RandomString
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void BotDumpMatchTemplates(bot_matchtemplate_t *matches)
{
	FILE *fp;
	bot_matchtemplate_t *mt;
	bot_matchpiece_t *mp;
	bot_matchstring_t *ms;

	fp = Log_FilePointer();
	if (!fp) return;
	for (mt = matches; mt; mt = mt->next)
	{
	        fprintf(fp, "{ " );
		for (mp = mt->first; mp; mp = mp->next)
		{
			if (mp->type == MT_STRING)
			{
				for (ms = mp->firststring; ms; ms = ms->next)
				{
					fprintf(fp, "\"%s\"", ms->string);
					if (ms->next) fprintf(fp, "|");
				} //end for
			} //end if
			else if (mp->type == MT_VARIABLE)
			{
				fprintf(fp, "%d", mp->variable);
			} //end else if
			if (mp->next) fprintf(fp, ", ");
		} //end for
		fprintf(fp, " = (%d, %d);}\n", mt->type, mt->subtype);
	} //end for
} //end of the function BotDumpMatchTemplates
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void BotFreeMatchPieces(bot_matchpiece_t *matchpieces)
{
	bot_matchpiece_t *mp, *nextmp;
	bot_matchstring_t *ms, *nextms;

	for (mp = matchpieces; mp; mp = nextmp)
	{
		nextmp = mp->next;
		if (mp->type == MT_STRING)
		{
			for (ms = mp->firststring; ms; ms = nextms)
			{
				nextms = ms->next;
				FreeMemory(ms);
			} //end for
		} //end if
		FreeMemory(mp);
	} //end for
} //end of the function BotFreeMatchPieces
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
bot_matchpiece_t *BotLoadMatchPieces(source_t *source, char *endtoken)
{
	int lastwasvariable, emptystring;
	token_t token;
	bot_matchpiece_t *matchpiece, *firstpiece, *lastpiece;
	bot_matchstring_t *matchstring, *lastmatchstring;

	if (!source || !endtoken || !endtoken[0])
	{
		botimport.Print(PRT_ERROR, "missing match piece source/delimiter\n");
		return NULL;
	}
	firstpiece = NULL;
	lastpiece = NULL;
	//
	lastwasvariable = qfalse;
	//
	while(!PC_SourceHasError(source) && PC_ReadToken(source, &token))
	{
		if (PC_SourceHasError(source)) goto failed;
		if (token.type == TT_NUMBER && (token.subtype & TT_INTEGER))
		{
			if (token.intvalue >= MAX_MATCHVARIABLES)
			{
				SourceError(source, "can't have more than %d match variables\n", MAX_MATCHVARIABLES);
				goto failed;
			} //end if
			if (lastwasvariable)
			{
				SourceError(source, "not allowed to have adjacent variables\n");
				goto failed;
			} //end if
			lastwasvariable = qtrue;
			//
			matchpiece = (bot_matchpiece_t *) GetClearedMemory(sizeof(bot_matchpiece_t));
			if (!matchpiece) goto failed;
			matchpiece->type = MT_VARIABLE;
			matchpiece->variable = token.intvalue;
			matchpiece->next = NULL;
			if (lastpiece) lastpiece->next = matchpiece;
			else firstpiece = matchpiece;
			lastpiece = matchpiece;
		} //end if
		else if (token.type == TT_STRING)
		{
			//
			matchpiece = (bot_matchpiece_t *) GetClearedMemory(sizeof(bot_matchpiece_t));
			if (!matchpiece) goto failed;
			matchpiece->firststring = NULL;
			matchpiece->type = MT_STRING;
			matchpiece->variable = 0;
			matchpiece->next = NULL;
			if (lastpiece) lastpiece->next = matchpiece;
			else firstpiece = matchpiece;
			lastpiece = matchpiece;
			//
			lastmatchstring = NULL;
			emptystring = qfalse;
			//
			do
			{
				if (matchpiece->firststring)
				{
					if (!PC_ExpectTokenType(source, TT_STRING, 0, &token))
					{
						goto failed;
					} //end if
				} //end if
				StripDoubleQuotes(token.string);
				matchstring = (bot_matchstring_t *) GetClearedMemory(sizeof(bot_matchstring_t) + strlen(token.string) + 1);
				if (!matchstring) goto failed;
				matchstring->string = (char *) matchstring + sizeof(bot_matchstring_t);
				strcpy(matchstring->string, token.string);
				if (!strlen(token.string)) emptystring = qtrue;
				matchstring->next = NULL;
				if (lastmatchstring) lastmatchstring->next = matchstring;
				else matchpiece->firststring = matchstring;
				lastmatchstring = matchstring;
			} while(!PC_SourceHasError(source) && PC_CheckTokenString(source, "|"));
			//if there was no empty string found
			if (!emptystring) lastwasvariable = qfalse;
		} //end if
		else
		{
			SourceError(source, "invalid token %s\n", token.string);
			goto failed;
		} //end else
		if (PC_SourceHasError(source)) goto failed;
		if (PC_CheckTokenString(source, endtoken))
		{
			if (PC_SourceHasError(source)) goto failed;
			return firstpiece;
		}
		if (PC_SourceHasError(source)) goto failed;
		if (!PC_ExpectTokenString(source, ","))
		{
			goto failed;
		} //end if
	} //end while
	if (!PC_SourceHasError(source)) SourceError(source, "missing match piece delimiter %s", endtoken);
failed:
	BotFreeMatchPieces(firstpiece);
	if (!PC_SourceHasError(source)) SourceError(source, "could not load complete match pieces");
	return NULL;
} //end of the function BotLoadMatchPieces
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void BotFreeMatchTemplates(bot_matchtemplate_t *mt)
{
	bot_matchtemplate_t *nextmt;

	for (; mt; mt = nextmt)
	{
		nextmt = mt->next;
		BotFreeMatchPieces(mt->first);
		FreeMemory(mt);
	} //end for
} //end of the function BotFreeMatchTemplates
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
bot_matchtemplate_t *BotLoadMatchTemplates(char *matchfile)
{
	source_t *source;
	token_t token;
	bot_matchtemplate_t *matchtemplate, *matches, *lastmatch;
	unsigned long int context;
	int closedcontext;

	if (!matchfile || !matchfile[0] || strlen(matchfile) >= MAX_PATH)
	{
		botimport.Print(PRT_ERROR, "invalid match template filename\n");
		return NULL;
	}
	PC_SetBaseFolder(BOTFILESBASEFOLDER);
	source = LoadSourceFile(matchfile);
	if (!source)
	{
		botimport.Print(PRT_ERROR, "couldn't load %s\n", matchfile);
		return NULL;
	}
	matches = NULL;
	lastmatch = NULL;

	while(!PC_SourceHasError(source) && PC_ReadToken(source, &token))
	{
		if (PC_SourceHasError(source)) goto failed;
		if (token.type != TT_NUMBER || !(token.subtype & TT_INTEGER))
		{
			SourceError(source, "expected integer, found %s\n", token.string);
			goto failed;
		}
		context = token.intvalue;
		if (!PC_ExpectTokenString(source, "{")) goto failed;
		closedcontext = qfalse;
		while(!PC_SourceHasError(source) && PC_ReadToken(source, &token))
		{
			if (PC_SourceHasError(source)) goto failed;
			if (!strcmp(token.string, "}"))
			{
				closedcontext = qtrue;
				break;
			}
			PC_UnreadLastToken(source);
			if (PC_SourceHasError(source)) goto failed;
			matchtemplate = (bot_matchtemplate_t *) GetClearedMemory(sizeof(bot_matchtemplate_t));
			if (!matchtemplate) goto failed;
			matchtemplate->context = context;
			if (lastmatch) lastmatch->next = matchtemplate;
			else matches = matchtemplate;
			lastmatch = matchtemplate;
			matchtemplate->first = BotLoadMatchPieces(source, "=");
			if (!matchtemplate->first) goto failed;
			if (!PC_ExpectTokenString(source, "(") ||
				!PC_ExpectTokenType(source, TT_NUMBER, TT_INTEGER, &token)) goto failed;
			matchtemplate->type = token.intvalue;
			if (!PC_ExpectTokenString(source, ",") ||
				!PC_ExpectTokenType(source, TT_NUMBER, TT_INTEGER, &token)) goto failed;
			matchtemplate->subtype = token.intvalue;
			if (!PC_ExpectTokenString(source, ")") ||
				!PC_ExpectTokenString(source, ";")) goto failed;
		}
		if (!closedcontext)
		{
			if (!PC_SourceHasError(source)) SourceError(source, "missing match context delimiter }");
			goto failed;
		}
	}
	if (PC_SourceHasError(source)) goto failed;
	FreeSource(source);
	botimport.Print(PRT_MESSAGE, "loaded %s\n", matchfile);
	return matches;
failed:
	if (!PC_SourceHasError(source)) SourceError(source, "could not load complete match templates");
	BotFreeMatchTemplates(matches);
	FreeSource(source);
	return NULL;
} //end of the function BotLoadMatchTemplates
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
int StringsMatch(bot_matchpiece_t *pieces, bot_match_t *match)
{
	int lastvariable, index;
	char *strptr, *newstrptr;
	bot_matchpiece_t *mp;
	bot_matchstring_t *ms;

	//no last variable
	lastvariable = -1;
	//pointer to the string to compare the match string with
	strptr = match->string;
	//Log_Write("match: %s", strptr);
	//compare the string with the current match string
	for (mp = pieces; mp; mp = mp->next)
	{
		//if it is a piece of string
		if (mp->type == MT_STRING)
		{
			newstrptr = NULL;
			for (ms = mp->firststring; ms; ms = ms->next)
			{
				if (!strlen(ms->string))
				{
					newstrptr = strptr;
					break;
				} //end if
				//Log_Write("MT_STRING: %s", mp->string);
				index = StringContains(strptr, ms->string, qfalse);
				if (index >= 0)
				{
					newstrptr = strptr + index;
					if (lastvariable >= 0)
					{
						match->variables[lastvariable].length =
								(newstrptr - match->string) - match->variables[lastvariable].offset;
								//newstrptr - match->variables[lastvariable].ptr;
						lastvariable = -1;
						break;
					} //end if
					else if (index == 0)
					{
						break;
					} //end else
					newstrptr = NULL;
				} //end if
			} //end for
			if (!newstrptr) return qfalse;
			strptr = newstrptr + strlen(ms->string);
		} //end if
		//if it is a variable piece of string
		else if (mp->type == MT_VARIABLE)
		{
			//Log_Write("MT_VARIABLE");
			match->variables[mp->variable].offset = strptr - match->string;
			lastvariable = mp->variable;
		} //end else if
	} //end for
	//if a match was found
	if (!mp && (lastvariable >= 0 || !strlen(strptr)))
	{
		//if the last piece was a variable string
		if (lastvariable >= 0)
		{
        		assert( match->variables[lastvariable].offset >= 0 ); // bk001204
			match->variables[lastvariable].length =
				strlen(&match->string[ (int) match->variables[lastvariable].offset]);
		} //end if
		return qtrue;
	} //end if
	return qfalse;
} //end of the function StringsMatch
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
/** Keep copied match strings terminated even when native input exceeds the fixed capacity. */
int BotFindMatch(char *str, bot_match_t *match, unsigned long int context)
{
	int i;
	bot_matchtemplate_t *ms;

	if (!str || !match)
	{
		botimport.Print(PRT_ERROR, "missing match input/output\n");
		return qfalse;
	}
	Q_strncpyz(match->string, str, sizeof(match->string));
	//remove any trailing enters
	while(strlen(match->string) &&
			match->string[strlen(match->string)-1] == '\n')
	{
		match->string[strlen(match->string)-1] = '\0';
	} //end while
	//compare the string with all the match strings
	for (ms = matchtemplates; ms; ms = ms->next)
	{
		if (!(ms->context & context)) continue;
		//reset the match variable offsets
		for (i = 0; i < MAX_MATCHVARIABLES; i++) match->variables[i].offset = -1;
		//
		if (StringsMatch(ms->first, match))
		{
			match->type = ms->type;
			match->subtype = ms->subtype;
			return qtrue;
		} //end if
	} //end for
	return qfalse;
} //end of the function BotFindMatch
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
/** Bound embedded match spans and support overlapping substring output. */
void BotMatchVariable(bot_match_t *match, int variable, char *buf, int size)
{
	char *end;
	int offset, length;
	if (!buf || size <= 0) return;
	if (!match || variable < 0 || variable >= MAX_MATCHVARIABLES) { buf[0] = '\0'; return; }
	offset = match->variables[variable].offset;
	length = match->variables[variable].length;
	end = memchr(match->string, '\0', sizeof(match->string));
	if (offset < 0 || !end || length < 0 || offset > end - match->string ||
	    length > end - match->string - offset) { buf[0] = '\0'; return; }
	if (length >= size) length = size - 1;
	memmove(buf, match->string + offset, length);
	buf[length] = '\0';
}
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
bot_stringlist_t *BotFindStringInList(bot_stringlist_t *list, char *string)
{
	bot_stringlist_t *s;

	for (s = list; s; s = s->next)
	{
		if (!strcmp(s->string, string)) return s;
	} //end for
	return NULL;
} //end of the function BotFindStringInList
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
bot_stringlist_t *BotCheckChatMessageIntegrety(char *message, bot_stringlist_t *stringlist)
{
	int i;
	char *msgptr;
	char temp[MAX_MESSAGE_SIZE];
	bot_stringlist_t *s;

	msgptr = message;
	//
	while(*msgptr)
	{
		if (*msgptr == ESCAPE_CHAR)
		{
			msgptr++;
			switch(*msgptr)
			{
				case 'v': //variable
				{
					//step over the 'v'
					msgptr++;
					while(*msgptr && *msgptr != ESCAPE_CHAR) msgptr++;
					//step over the trailing escape char
					if (*msgptr) msgptr++;
					break;
				} //end case
				case 'r': //random
				{
					//step over the 'r'
					msgptr++;
					for (i = 0; (*msgptr && *msgptr != ESCAPE_CHAR); i++)
					{
						temp[i] = *msgptr++;
					} //end while
					temp[i] = '\0';
					//step over the trailing escape char
					if (*msgptr) msgptr++;
					//find the random keyword
					if (!RandomString(temp))
					{
						if (!BotFindStringInList(stringlist, temp))
						{
							Log_Write("%s = {\"%s\"} //MISSING RANDOM\r\n", temp, temp);
							s = GetClearedMemory(sizeof(bot_stringlist_t) + strlen(temp) + 1);
							s->string = (char *) s + sizeof(bot_stringlist_t);
							strcpy(s->string, temp);
							s->next = stringlist;
							stringlist = s;
						} //end if
					} //end if
					break;
				} //end case
				default:
				{
					botimport.Print(PRT_FATAL, "BotCheckChatMessageIntegrety: message \"%s\" invalid escape char\n", message);
					break;
				} //end default
			} //end switch
		} //end if
		else
		{
			msgptr++;
		} //end else
	} //end while
	return stringlist;
} //end of the function BotCheckChatMessageIntegrety
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void BotCheckInitialChatIntegrety(bot_chat_t *chat)
{
	bot_chattype_t *t;
	bot_chatmessage_t *cm;
	bot_stringlist_t *stringlist, *s, *nexts;

	stringlist = NULL;
	for (t = chat->types; t; t = t->next)
	{
		for (cm = t->firstchatmessage; cm; cm = cm->next)
		{
			stringlist = BotCheckChatMessageIntegrety(cm->chatmessage, stringlist);
		} //end for
	} //end for
	for (s = stringlist; s; s = nexts)
	{
		nexts = s->next;
		FreeMemory(s);
	} //end for
} //end of the function BotCheckInitialChatIntegrety
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void BotCheckReplyChatIntegrety(bot_replychat_t *replychat)
{
	bot_replychat_t *rp;
	bot_chatmessage_t *cm;
	bot_stringlist_t *stringlist, *s, *nexts;

	stringlist = NULL;
	for (rp = replychat; rp; rp = rp->next)
	{
		for (cm = rp->firstchatmessage; cm; cm = cm->next)
		{
			stringlist = BotCheckChatMessageIntegrety(cm->chatmessage, stringlist);
		} //end for
	} //end for
	for (s = stringlist; s; s = nexts)
	{
		nexts = s->next;
		FreeMemory(s);
	} //end for
} //end of the function BotCheckReplyChatIntegrety
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void BotDumpReplyChat(bot_replychat_t *replychat)
{
	FILE *fp;
	bot_replychat_t *rp;
	bot_replychatkey_t *key;
	bot_chatmessage_t *cm;
	bot_matchpiece_t *mp;

	fp = Log_FilePointer();
	if (!fp) return;
	fprintf(fp, "BotDumpReplyChat:\n");
	for (rp = replychat; rp; rp = rp->next)
	{
		fprintf(fp, "[");
		for (key = rp->keys; key; key = key->next)
		{
			if (key->flags & RCKFL_AND) fprintf(fp, "&");
			else if (key->flags & RCKFL_NOT) fprintf(fp, "!");
			//
			if (key->flags & RCKFL_NAME) fprintf(fp, "name");
			else if (key->flags & RCKFL_GENDERFEMALE) fprintf(fp, "female");
			else if (key->flags & RCKFL_GENDERMALE) fprintf(fp, "male");
			else if (key->flags & RCKFL_GENDERLESS) fprintf(fp, "it");
			else if (key->flags & RCKFL_VARIABLES)
			{
				fprintf(fp, "(");
				for (mp = key->match; mp; mp = mp->next)
				{
					if (mp->type == MT_STRING) fprintf(fp, "\"%s\"", mp->firststring->string);
					else fprintf(fp, "%d", mp->variable);
					if (mp->next) fprintf(fp, ", ");
				} //end for
				fprintf(fp, ")");
			} //end if
			else if (key->flags & RCKFL_STRING)
			{
				fprintf(fp, "\"%s\"", key->string);
			} //end if
			if (key->next) fprintf(fp, ", ");
			else fprintf(fp, "] = %1.0f\n", rp->priority);
		} //end for
		fprintf(fp, "{\n");
		for (cm = rp->firstchatmessage; cm; cm = cm->next)
		{
			fprintf(fp, "\t\"%s\";\n", cm->chatmessage);
		} //end for
		fprintf(fp, "}\n");
	} //end for
} //end of the function BotDumpReplyChat
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void BotFreeReplyChat(bot_replychat_t *replychat)
{
	bot_replychat_t *rp, *nextrp;
	bot_replychatkey_t *key, *nextkey;
	bot_chatmessage_t *cm, *nextcm;

	for (rp = replychat; rp; rp = nextrp)
	{
		nextrp = rp->next;
		for (key = rp->keys; key; key = nextkey)
		{
			nextkey = key->next;
			if (key->match) BotFreeMatchPieces(key->match);
			if (key->string) FreeMemory(key->string);
			FreeMemory(key);
		} //end for
		for (cm = rp->firstchatmessage; cm; cm = nextcm)
		{
			nextcm = cm->next;
			FreeMemory(cm);
		} //end for
		FreeMemory(rp);
	} //end for
} //end of the function BotFreeReplyChat
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
void BotCheckValidReplyChatKeySet(source_t *source, bot_replychatkey_t *keys)
{
	int allprefixed, hasvariableskey, hasstringkey;
	bot_matchpiece_t *m;
	bot_matchstring_t *ms;
	bot_replychatkey_t *key, *key2;

	//
	allprefixed = qtrue;
	hasvariableskey = hasstringkey = qfalse;
	for (key = keys; key; key = key->next)
	{
		if (!(key->flags & (RCKFL_AND|RCKFL_NOT)))
		{
			allprefixed = qfalse;
			if (key->flags & RCKFL_VARIABLES)
			{
				for (m = key->match; m; m = m->next)
				{
					if (m->type == MT_VARIABLE) hasvariableskey = qtrue;
				} //end for
			} //end if
			else if (key->flags & RCKFL_STRING)
			{
				hasstringkey = qtrue;
			} //end else if
		} //end if
		else if ((key->flags & RCKFL_AND) && (key->flags & RCKFL_STRING))
		{
			for (key2 = keys; key2; key2 = key2->next)
			{
				if (key2 == key) continue;
				if (key2->flags & RCKFL_NOT) continue;
				if (key2->flags & RCKFL_VARIABLES)
				{
					for (m = key2->match; m; m = m->next)
					{
						if (m->type == MT_STRING)
						{
							for (ms = m->firststring; ms; ms = ms->next)
							{
								if (StringContains(ms->string, key->string, qfalse) != -1)
								{
									break;
								} //end if
							} //end for
							if (ms) break;
						} //end if
						else if (m->type == MT_VARIABLE)
						{
							break;
						} //end if
					} //end for
					if (!m)
					{
						SourceWarning(source, "one of the match templates does not "
										"leave space for the key %s with the & prefix", key->string);
					} //end if
				} //end if
			} //end for
		} //end else
		if ((key->flags & RCKFL_NOT) && (key->flags & RCKFL_STRING))
		{
			for (key2 = keys; key2; key2 = key2->next)
			{
				if (key2 == key) continue;
				if (key2->flags & RCKFL_NOT) continue;
				if (key2->flags & RCKFL_STRING)
				{
					if (StringContains(key2->string, key->string, qfalse) != -1)
					{
						SourceWarning(source, "the key %s with prefix ! is inside the key %s", key->string, key2->string);
					} //end if
				} //end if
				else if (key2->flags & RCKFL_VARIABLES)
				{
					for (m = key2->match; m; m = m->next)
					{
						if (m->type == MT_STRING)
						{
							for (ms = m->firststring; ms; ms = ms->next)
							{
								if (StringContains(ms->string, key->string, qfalse) != -1)
								{
									SourceWarning(source, "the key %s with prefix ! is inside "
												"the match template string %s", key->string, ms->string);
								} //end if
							} //end for
						} //end if
					} //end for
				} //end else if
			} //end for
		} //end if
	} //end for
	if (allprefixed) SourceWarning(source, "all keys have a & or ! prefix");
	if (hasvariableskey && hasstringkey)
	{
		SourceWarning(source, "variables from the match template(s) could be "
								"invalid when outputting one of the chat messages");
	} //end if
} //end of the function BotCheckValidReplyChatKeySet
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
bot_replychat_t *BotLoadReplyChat(char *filename)
{
	char chatmessagestring[MAX_MESSAGE_SIZE];
	char namebuffer[MAX_MESSAGE_SIZE];
	source_t *source;
	token_t token;
	bot_chatmessage_t *chatmessage = NULL;
	bot_replychat_t *replychat, *replychatlist;
	bot_replychatkey_t *key;

	PC_SetBaseFolder(BOTFILESBASEFOLDER);
	source = LoadSourceFile(filename);
	if (!source)
	{
		botimport.Print(PRT_ERROR, "counldn't load %s\n", filename);
		return NULL;
	} //end if
	//
	replychatlist = NULL;
	//
	while(PC_ReadToken(source, &token))
	{
		if (strcmp(token.string, "["))
		{
			SourceError(source, "expected [, found %s", token.string);
			BotFreeReplyChat(replychatlist);
			FreeSource(source);
			return NULL;
		} //end if
		//
		replychat = GetClearedHunkMemory(sizeof(bot_replychat_t));
		replychat->keys = NULL;
		replychat->next = replychatlist;
		replychatlist = replychat;
		//read the keys, there must be at least one key
		do
		{
			//allocate a key
			key = (bot_replychatkey_t *) GetClearedHunkMemory(sizeof(bot_replychatkey_t));
			key->flags = 0;
			key->string = NULL;
			key->match = NULL;
			key->next = replychat->keys;
			replychat->keys = key;
			//check for MUST BE PRESENT and MUST BE ABSENT keys
			if (PC_CheckTokenString(source, "&")) key->flags |= RCKFL_AND;
			else if (PC_CheckTokenString(source, "!")) key->flags |= RCKFL_NOT;
			//special keys
			if (PC_CheckTokenString(source, "name")) key->flags |= RCKFL_NAME;
			else if (PC_CheckTokenString(source, "female")) key->flags |= RCKFL_GENDERFEMALE;
			else if (PC_CheckTokenString(source, "male")) key->flags |= RCKFL_GENDERMALE;
			else if (PC_CheckTokenString(source, "it")) key->flags |= RCKFL_GENDERLESS;
			else if (PC_CheckTokenString(source, "(")) //match key
			{
				key->flags |= RCKFL_VARIABLES;
				key->match = BotLoadMatchPieces(source, ")");
				if (!key->match)
				{
					FreeSource(source);
					BotFreeReplyChat(replychatlist);
					return NULL;
				} //end if
			} //end else if
			else if (PC_CheckTokenString(source, "<")) //bot names
			{
				key->flags |= RCKFL_BOTNAMES;
				strcpy(namebuffer, "");
				do
				{
					if (!PC_ExpectTokenType(source, TT_STRING, 0, &token))
					{
						BotFreeReplyChat(replychatlist);
						FreeSource(source);
						return NULL;
					} //end if
					StripDoubleQuotes(token.string);
					if (strlen(namebuffer)) strcat(namebuffer, "\\");
					strcat(namebuffer, token.string);
				} while(PC_CheckTokenString(source, ","));
				if (!PC_ExpectTokenString(source, ">"))
				{
					BotFreeReplyChat(replychatlist);
					FreeSource(source);
					return NULL;
				} //end if
				key->string = (char *) GetClearedHunkMemory(strlen(namebuffer) + 1);
				strcpy(key->string, namebuffer);
			} //end else if
			else //normal string key
			{
				key->flags |= RCKFL_STRING;
				if (!PC_ExpectTokenType(source, TT_STRING, 0, &token))
				{
					BotFreeReplyChat(replychatlist);
					FreeSource(source);
					return NULL;
				} //end if
				StripDoubleQuotes(token.string);
				key->string = (char *) GetClearedHunkMemory(strlen(token.string) + 1);
				strcpy(key->string, token.string);
			} //end else
			//
			PC_CheckTokenString(source, ",");
		} while(!PC_CheckTokenString(source, "]"));
		//
		BotCheckValidReplyChatKeySet(source, replychat->keys);
		//read the = sign and the priority
		if (!PC_ExpectTokenString(source, "=") ||
			!PC_ExpectTokenType(source, TT_NUMBER, 0, &token))
		{
			BotFreeReplyChat(replychatlist);
			FreeSource(source);
			return NULL;
		} //end if
		replychat->priority = token.floatvalue;
		//read the leading {
		if (!PC_ExpectTokenString(source, "{"))
		{
			BotFreeReplyChat(replychatlist);
			FreeSource(source);
			return NULL;
		} //end if
		replychat->numchatmessages = 0;
		//while the trailing } is not found
		while(!PC_CheckTokenString(source, "}"))
		{
			if (!BotLoadChatMessage(source, chatmessagestring))
			{
				BotFreeReplyChat(replychatlist);
				FreeSource(source);
				return NULL;
			} //end if
			chatmessage = (bot_chatmessage_t *) GetClearedHunkMemory(sizeof(bot_chatmessage_t) + strlen(chatmessagestring) + 1);
			chatmessage->chatmessage = (char *) chatmessage + sizeof(bot_chatmessage_t);
			strcpy(chatmessage->chatmessage, chatmessagestring);
			chatmessage->time = -2*CHATMESSAGE_RECENTTIME;
			chatmessage->next = replychat->firstchatmessage;
			//add the chat message to the reply chat
			replychat->firstchatmessage = chatmessage;
			replychat->numchatmessages++;
		} //end while
	} //end while
	FreeSource(source);
	botimport.Print(PRT_MESSAGE, "loaded %s\n", filename);
	//
	//BotDumpReplyChat(replychatlist);
	if (bot_developer)
	{
		BotCheckReplyChatIntegrety(replychatlist);
	} //end if
	//
	if (!replychatlist) botimport.Print(PRT_MESSAGE, "no rchats\n");
	//
	return replychatlist;
} //end of the function BotLoadReplyChat
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void BotDumpInitialChat(bot_chat_t *chat)
{
	bot_chattype_t *t;
	bot_chatmessage_t *m;

	Log_Write("{");
	for (t = chat->types; t; t = t->next)
	{
		Log_Write(" type \"%s\"", t->name);
		Log_Write(" {");
		Log_Write("  numchatmessages = %d", t->numchatmessages);
		for (m = t->firstchatmessage; m; m = m->next)
		{
			Log_Write("  \"%s\"", m->chatmessage);
		} //end for
		Log_Write(" }");
	} //end for
	Log_Write("}");
} //end of the function BotDumpInitialChat
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
bot_chat_t *BotLoadInitialChat(char *chatfile, char *chatname)
{
	int pass, foundchat, indent, size;
	char *ptr = NULL;
	char chatmessagestring[MAX_MESSAGE_SIZE];
	source_t *source;
	token_t token;
	bot_chat_t *chat = NULL;
	bot_chattype_t *chattype = NULL;
	bot_chatmessage_t *chatmessage = NULL;
#ifdef DEBUG
	int starttime;

	starttime = Sys_MilliSeconds();
#endif //DEBUG
	//
	size = 0;
	foundchat = qfalse;
	//a bot chat is parsed in two phases
	for (pass = 0; pass < 2; pass++)
	{
		//allocate memory
		if (pass && size) ptr = (char *) GetClearedMemory(size);
		//load the source file
		PC_SetBaseFolder(BOTFILESBASEFOLDER);
		source = LoadSourceFile(chatfile);
		if (!source)
		{
			botimport.Print(PRT_ERROR, "counldn't load %s\n", chatfile);
			return NULL;
		} //end if
		//chat structure
		if (pass)
		{
			chat = (bot_chat_t *) ptr;
			ptr += sizeof(bot_chat_t);
		} //end if
		size = sizeof(bot_chat_t);
		//
		while(PC_ReadToken(source, &token))
		{
			if (!strcmp(token.string, "chat"))
			{
				if (!PC_ExpectTokenType(source, TT_STRING, 0, &token))
				{
					FreeSource(source);
					return NULL;
				} //end if
				StripDoubleQuotes(token.string);
				//after the chat name we expect a opening brace
				if (!PC_ExpectTokenString(source, "{"))
				{
					FreeSource(source);
					return NULL;
				} //end if
				//if the chat name is found
				if (!Q_stricmp(token.string, chatname))
				{
					foundchat = qtrue;
					//read the chat types
					while(1)
					{
						if (!PC_ExpectAnyToken(source, &token))
						{
							FreeSource(source);
							return NULL;
						} //end if
						if (!strcmp(token.string, "}")) break;
						if (strcmp(token.string, "type"))
						{
							SourceError(source, "expected type found %s\n", token.string);
							FreeSource(source);
							return NULL;
						} //end if
						//expect the chat type name
						if (!PC_ExpectTokenType(source, TT_STRING, 0, &token) ||
							!PC_ExpectTokenString(source, "{"))
						{
							FreeSource(source);
							return NULL;
						} //end if
						StripDoubleQuotes(token.string);
						if (pass)
						{
							chattype = (bot_chattype_t *) ptr;
							strncpy(chattype->name, token.string, MAX_CHATTYPE_NAME);
							chattype->firstchatmessage = NULL;
							//add the chat type to the chat
							chattype->next = chat->types;
							chat->types = chattype;
							//
							ptr += sizeof(bot_chattype_t);
						} //end if
						size += sizeof(bot_chattype_t);
						//read the chat messages
						while(!PC_CheckTokenString(source, "}"))
						{
							if (!BotLoadChatMessage(source, chatmessagestring))
							{
								FreeSource(source);
								return NULL;
							} //end if
							if (pass)
							{
								chatmessage = (bot_chatmessage_t *) ptr;
								chatmessage->time = -2*CHATMESSAGE_RECENTTIME;
								//put the chat message in the list
								chatmessage->next = chattype->firstchatmessage;
								chattype->firstchatmessage = chatmessage;
								//store the chat message
								ptr += sizeof(bot_chatmessage_t);
								chatmessage->chatmessage = ptr;
								strcpy(chatmessage->chatmessage, chatmessagestring);
								ptr += strlen(chatmessagestring) + 1;
								//the number of chat messages increased
								chattype->numchatmessages++;
							} //end if
							size += sizeof(bot_chatmessage_t) + strlen(chatmessagestring) + 1;
						} //end if
					} //end while
				} //end if
				else //skip the bot chat
				{
					indent = 1;
					while(indent)
					{
						if (!PC_ExpectAnyToken(source, &token))
						{
							FreeSource(source);
							return NULL;
						} //end if
						if (!strcmp(token.string, "{")) indent++;
						else if (!strcmp(token.string, "}")) indent--;
					} //end while
				} //end else
			} //end if
			else
			{
				SourceError(source, "unknown definition %s\n", token.string);
				FreeSource(source);
				return NULL;
			} //end else
		} //end while
		//free the source
		FreeSource(source);
		//if the requested character is not found
		if (!foundchat)
		{
			botimport.Print(PRT_ERROR, "couldn't find chat %s in %s\n", chatname, chatfile);
			return NULL;
		} //end if
	} //end for
	//
	botimport.Print(PRT_MESSAGE, "loaded %s from %s\n", chatname, chatfile);
	//
	//BotDumpInitialChat(chat);
	if (bot_developer)
	{
		BotCheckInitialChatIntegrety(chat);
	} //end if
#ifdef DEBUG
	botimport.Print(PRT_MESSAGE, "initial chats loaded in %d msec\n", Sys_MilliSeconds() - starttime);
#endif //DEBUG
	//character was read succesfully
	return chat;
} //end of the function BotLoadInitialChat
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
void BotFreeChatFile(int chatstate)
{
	bot_chatstate_t *cs;

	cs = BotChatStateFromHandle(chatstate);
	if (!cs) return;
	if (cs->chat) FreeMemory(cs->chat);
	cs->chat = NULL;
} //end of the function BotFreeChatFile
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
int BotLoadChatFile(int chatstate, char *chatfile, char *chatname)
{
	bot_chatstate_t *cs;
	int n, avail = 0;

	cs = BotChatStateFromHandle(chatstate);
	if (!cs) return BLERR_CANNOTLOADICHAT;
	BotFreeChatFile(chatstate);

	if (!LibVarGetValue("bot_reloadcharacters"))
	{
		avail = -1;
		for( n = 0; n < MAX_CLIENTS; n++ ) {
			if( !ichatdata[n] ) {
				if( avail == -1 ) {
					avail = n;
				}
				continue;
			}
			if( strcmp( chatfile, ichatdata[n]->filename ) != 0 ) { 
				continue;
			}
			if( strcmp( chatname, ichatdata[n]->chatname ) != 0 ) { 
				continue;
			}
			cs->chat = ichatdata[n]->chat;
		//		botimport.Print( PRT_MESSAGE, "retained %s from %s\n", chatname, chatfile );
			return BLERR_NOERROR;
		}

		if( avail == -1 ) {
			botimport.Print(PRT_FATAL, "ichatdata table full; couldn't load chat %s from %s\n", chatname, chatfile);
			return BLERR_CANNOTLOADICHAT;
		}
	}

	cs->chat = BotLoadInitialChat(chatfile, chatname);
	if (!cs->chat)
	{
		botimport.Print(PRT_FATAL, "couldn't load chat %s from %s\n", chatname, chatfile);
		return BLERR_CANNOTLOADICHAT;
	} //end if
	if (!LibVarGetValue("bot_reloadcharacters"))
	{
		ichatdata[avail] = GetClearedMemory( sizeof(bot_ichatdata_t) );
		ichatdata[avail]->chat = cs->chat;
		Q_strncpyz( ichatdata[avail]->chatname, chatname, sizeof(ichatdata[avail]->chatname) );
		Q_strncpyz( ichatdata[avail]->filename, chatfile, sizeof(ichatdata[avail]->filename) );
	} //end if

	return BLERR_NOERROR;
} //end of the function BotLoadChatFile
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
// Stage complete expansion before publishing text; retain the legacy flag wrapper.
static int BotExpandChatMessageChecked(char *output, char *message, unsigned long mcontext,
		bot_match_t *match, unsigned long vcontext, int reply, int *expanded)
{
	int num, digits, i, offset, length;
	size_t len = 0, bytes;
	char *ptr, *msgptr, *end;
	char temp[MAX_MESSAGE_SIZE];

	if (!message || strlen(message) >= MAX_MESSAGE_SIZE) goto invalid;
	*expanded = qfalse;
	msgptr = message;
	while (*msgptr)
	{
		if (*msgptr != ESCAPE_CHAR)
		{
			if (len >= MAX_MESSAGE_SIZE - 1) goto invalid;
			output[len++] = *msgptr++;
			continue;
		}
		msgptr++;
		switch (*msgptr++)
		{
			case 'v':
				num = digits = 0;
				while (*msgptr && *msgptr != ESCAPE_CHAR)
				{
					if (*msgptr < '0' || *msgptr > '9' || num * 10 + *msgptr - '0' >= MAX_MATCHVARIABLES)
						goto invalid;
					num = num * 10 + *msgptr++ - '0';
					digits++;
				}
				if (!digits || *msgptr != ESCAPE_CHAR || !match) goto invalid;
				msgptr++;
				offset = match->variables[num].offset;
				if (offset < 0) continue;
				length = match->variables[num].length;
				end = (char *)memchr(match->string, '\0', sizeof(match->string));
				if (!end || length < 0 || offset > end - match->string || length > end - match->string - offset)
					goto invalid;
				Com_Memcpy(temp, match->string + offset, length);
				temp[length] = 0;
				if (reply) BotReplaceReplySynonymsSized(temp, vcontext, sizeof(temp));
				else BotReplaceSynonymsSized(temp, vcontext, sizeof(temp));
				ptr = temp;
				break;
			case 'r':
				for (i = 0; *msgptr && *msgptr != ESCAPE_CHAR; i++)
				{
					if (i >= sizeof(temp) - 1) goto invalid;
					temp[i] = *msgptr++;
				}
				if (*msgptr != ESCAPE_CHAR) goto invalid;
				msgptr++;
				temp[i] = 0;
				ptr = RandomString(temp);
				if (!ptr) goto invalid;
				*expanded = qtrue;
				break;
			default:
				goto invalid;
		}
		bytes = strlen(ptr);
		if (bytes >= MAX_MESSAGE_SIZE - len) goto invalid;
		Com_Memcpy(output + len, ptr, bytes);
		len += bytes;
	}
	output[len] = 0;
	BotReplaceWeightedSynonyms(output, mcontext);
	return qtrue;
invalid:
	botimport.Print(PRT_ERROR, "invalid or excessive encoded chat message\n");
	return qfalse;
}

int BotExpandChatMessage(char *outmessage, char *message, unsigned long mcontext,
		bot_match_t *match, unsigned long vcontext, int reply)
{
	char staged[MAX_MESSAGE_SIZE];
	int expanded;

	if (!outmessage)
	{
		botimport.Print(PRT_ERROR, "missing expansion output\n");
		return qfalse;
	}
	if (!BotExpandChatMessageChecked(staged, message, mcontext, match, vcontext, reply, &expanded))
		return qfalse;
	strcpy(outmessage, staged);
	return expanded;
}

static int BotConstructChatMessageChecked(bot_chatstate_t *chatstate, char *message, unsigned long mcontext,
		bot_match_t *match, unsigned long vcontext, int reply)
{
	int i, expanded;
	char srcmessage[MAX_MESSAGE_SIZE], staged[MAX_MESSAGE_SIZE];

	if (!chatstate || !message || strlen(message) >= sizeof(srcmessage))
	{
		botimport.Print(PRT_ERROR, "invalid construction input\n");
		return qfalse;
	}
	strcpy(srcmessage, message);
	for (i = 0; i < 10; i++)
	{
		if (!BotExpandChatMessageChecked(staged, srcmessage, mcontext, match, vcontext, reply, &expanded))
			return qfalse;
		if (!expanded) break;
		strcpy(srcmessage, staged);
	}
	if (i >= 10)
	{
		botimport.Print(PRT_WARNING, "too many expansions in chat message\n");
		botimport.Print(PRT_WARNING, "%s\n", staged);
	}
	strcpy(chatstate->chatmessage, staged);
	return qtrue;
}

void BotConstructChatMessage(bot_chatstate_t *chatstate, char *message, unsigned long mcontext,
		bot_match_t *match, unsigned long vcontext, int reply)
{
	BotConstructChatMessageChecked(chatstate, message, mcontext, match, vcontext, reply);
} //end of the function BotConstructChatMessage
//===========================================================================
// randomly chooses one of the chat message of the given type
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
static bot_chatmessage_t *BotChooseInitialChatLine(bot_chatstate_t *cs, char *type, int *recent)
{
	int n, numchatmessages;
	float besttime;
	bot_chattype_t *t;
	bot_chatmessage_t *m, *bestchatmessage;
	bot_chat_t *chat;

	*recent = qfalse;
	if (!cs || !cs->chat || !type) return NULL;
	chat = cs->chat;
	for (t = chat->types; t; t = t->next)
	{
		if (!Q_stricmp(t->name, type))
		{
			numchatmessages = 0;
			for (m = t->firstchatmessage; m; m = m->next)
			{
				if (m->time > AAS_Time()) continue;
				numchatmessages++;
			} //end if
			//if all chat messages have been used recently
			if (numchatmessages <= 0)
			{
				besttime = 0;
				bestchatmessage = NULL;
				for (m = t->firstchatmessage; m; m = m->next)
				{
					if (!besttime || m->time < besttime)
					{
						bestchatmessage = m;
						besttime = m->time;
					} //end if
				} //end for
				if (bestchatmessage) return bestchatmessage;
			} //end if
			else //choose a chat message randomly
			{
				n = random() * numchatmessages;
				for (m = t->firstchatmessage; m; m = m->next)
				{
					if (m->time > AAS_Time()) continue;
					if (--n < 0)
					{
						*recent = qtrue;
						return m;
					} //end if
				} //end for
			} //end else
			return NULL;
		} //end if
	} //end for
	return NULL;
}

char *BotChooseInitialChatMessage(bot_chatstate_t *cs, char *type)
{
	int recent;
	bot_chatmessage_t *line = BotChooseInitialChatLine(cs, type, &recent);
	if (line && recent) line->time = AAS_Time() + CHATMESSAGE_RECENTTIME;
	return line ? line->chatmessage : NULL;
} //end of the function BotChooseInitialChatMessage
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
int BotNumInitialChats(int chatstate, char *type)
{
	bot_chatstate_t *cs;
	bot_chattype_t *t;

	cs = BotChatStateFromHandle(chatstate);
	if (!cs || !cs->chat || !type) return 0;

	for (t = cs->chat->types; t; t = t->next)
	{
		if (!Q_stricmp(t->name, type))
		{
			if (LibVarGetValue("bot_testichat")) {
				botimport.Print(PRT_MESSAGE, "%s has %d chat lines\n", type, t->numchatmessages);
				botimport.Print(PRT_MESSAGE, "-------------------\n");
			}
			return t->numchatmessages;
		} //end if
	} //end for
	return 0;
} //end of the function BotNumInitialChats
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
// Append optional variables only within the fixed native match array.
static int BotAppendChatVariables(bot_match_t *match, char **variables)
{
	int i, index = (int)strlen(match->string);
	size_t length;

	for (i = 0; i < MAX_MATCHVARIABLES; i++)
	{
		if (!variables[i]) continue;
		length = strlen(variables[i]);
		if (length >= sizeof(match->string) - index)
		{
			botimport.Print(PRT_ERROR, "chat variables exceed the native match capacity\n");
			return qfalse;
		}
		Com_Memcpy(match->string + index, variables[i], length + 1);
		match->variables[i].offset = index;
		match->variables[i].length = length;
		index += length;
	}
	return qtrue;
}

void BotInitialChat(int chatstate, char *type, int mcontext, char *var0, char *var1, char *var2, char *var3, char *var4, char *var5, char *var6, char *var7)
{
	bot_chatmessage_t *line;
	int recent;
	char *variables[MAX_MATCHVARIABLES] = { var0, var1, var2, var3, var4, var5, var6, var7 };
	bot_match_t match;
	bot_chatstate_t *cs;

	cs = BotChatStateFromHandle(chatstate);
	if (!cs) return;
	//if no chat file is loaded
	if (!cs->chat || !type) return;
	Com_Memset(&match, 0, sizeof(match));
	if (!BotAppendChatVariables(&match, variables)) return;
	//choose a chat message randomly of the given type
	line = BotChooseInitialChatLine(cs, type, &recent);
	//if there's no message of the given type
	if (!line)
	{
#ifdef DEBUG
		botimport.Print(PRT_MESSAGE, "no chat messages of type %s\n", type);
#endif //DEBUG
		return;
	} //end if
	//
 	//
	if (!BotConstructChatMessageChecked(cs, line->chatmessage, mcontext, &match, 0, qfalse)) return;
	if (recent) line->time = AAS_Time() + CHATMESSAGE_RECENTTIME;
} //end of the function BotInitialChat
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void BotPrintReplyChatKeys(bot_replychat_t *replychat)
{
	bot_replychatkey_t *key;
	bot_matchpiece_t *mp;

	botimport.Print(PRT_MESSAGE, "[");
	for (key = replychat->keys; key; key = key->next)
	{
		if (key->flags & RCKFL_AND) botimport.Print(PRT_MESSAGE, "&");
		else if (key->flags & RCKFL_NOT) botimport.Print(PRT_MESSAGE, "!");
		//
		if (key->flags & RCKFL_NAME) botimport.Print(PRT_MESSAGE, "name");
		else if (key->flags & RCKFL_GENDERFEMALE) botimport.Print(PRT_MESSAGE, "female");
		else if (key->flags & RCKFL_GENDERMALE) botimport.Print(PRT_MESSAGE, "male");
		else if (key->flags & RCKFL_GENDERLESS) botimport.Print(PRT_MESSAGE, "it");
		else if (key->flags & RCKFL_VARIABLES)
		{
			botimport.Print(PRT_MESSAGE, "(");
			for (mp = key->match; mp; mp = mp->next)
			{
				if (mp->type == MT_STRING) botimport.Print(PRT_MESSAGE, "\"%s\"", mp->firststring->string);
				else botimport.Print(PRT_MESSAGE, "%d", mp->variable);
				if (mp->next) botimport.Print(PRT_MESSAGE, ", ");
			} //end for
			botimport.Print(PRT_MESSAGE, ")");
		} //end if
		else if (key->flags & RCKFL_STRING)
		{
			botimport.Print(PRT_MESSAGE, "\"%s\"", key->string);
		} //end if
		if (key->next) botimport.Print(PRT_MESSAGE, ", ");
		else botimport.Print(PRT_MESSAGE, "] = %1.0f\n", replychat->priority);
	} //end for
	botimport.Print(PRT_MESSAGE, "{\n");
} //end of the function BotPrintReplyChatKeys
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
int BotReplyChat(int chatstate, char *message, int mcontext, int vcontext, char *var0, char *var1, char *var2, char *var3, char *var4, char *var5, char *var6, char *var7)
{
	bot_replychat_t *rchat, *bestrchat;
	bot_replychatkey_t *key;
	bot_chatmessage_t *m, *bestchatmessage;
	bot_match_t match, bestmatch;
	int bestpriority, num, found, res, numchatmessages;
	bot_chatstate_t *cs;
	char *variables[MAX_MATCHVARIABLES] = { var0, var1, var2, var3, var4, var5, var6, var7 };

	cs = BotChatStateFromHandle(chatstate);
	if (!cs) return qfalse;
	if (!message || strlen(message) >= sizeof(match.string))
	{
		botimport.Print(PRT_ERROR, "invalid reply message input\n");
		return qfalse;
	}
	Com_Memset(&match, 0, sizeof(bot_match_t));
	strcpy(match.string, message);
	bestpriority = -1;
	bestchatmessage = NULL;
	bestrchat = NULL;
	//go through all the reply chats
	for (rchat = replychats; rchat; rchat = rchat->next)
	{
		found = qfalse;
		for (key = rchat->keys; key; key = key->next)
		{
			res = qfalse;
			//get the match result
			if (key->flags & RCKFL_NAME) res = (StringContains(message, cs->name, qfalse) != -1);
			else if (key->flags & RCKFL_BOTNAMES) res = (StringContains(key->string, cs->name, qfalse) != -1);
			else if (key->flags & RCKFL_GENDERFEMALE) res = (cs->gender == CHAT_GENDERFEMALE);
			else if (key->flags & RCKFL_GENDERMALE) res = (cs->gender == CHAT_GENDERMALE);
			else if (key->flags & RCKFL_GENDERLESS) res = (cs->gender == CHAT_GENDERLESS);
			else if (key->flags & RCKFL_VARIABLES) res = StringsMatch(key->match, &match);
			else if (key->flags & RCKFL_STRING) res = (StringContainsWord(message, key->string, qfalse) != NULL);
			//if the key must be present
			if (key->flags & RCKFL_AND)
			{
				if (!res)
				{
					found = qfalse;
					break;
				} //end if
			} //end else if
			//if the key must be absent
			else if (key->flags & RCKFL_NOT)
			{
				if (res)
				{
					found = qfalse;
					break;
				} //end if
			} //end if
			else if (res)
			{
				found = qtrue;
			} //end else
		} //end for
		//
		if (found)
		{
			if (rchat->priority > bestpriority)
			{
				numchatmessages = 0;
				for (m = rchat->firstchatmessage; m; m = m->next)
				{
					if (m->time > AAS_Time()) continue;
					numchatmessages++;
				} //end if
				num = random() * numchatmessages;
				for (m = rchat->firstchatmessage; m; m = m->next)
				{
					if (--num < 0) break;
					if (m->time > AAS_Time()) continue;
				} //end for
				//if the reply chat has a message
				if (m)
				{
					Com_Memcpy(&bestmatch, &match, sizeof(bot_match_t));
					bestchatmessage = m;
					bestrchat = rchat;
					bestpriority = rchat->priority;
				} //end if
			} //end if
		} //end if
	} //end for
	if (bestchatmessage)
	{
		if (!BotAppendChatVariables(&bestmatch, variables)) return qfalse;
		if (LibVarGetValue("bot_testrchat"))
		{
			for (m = bestrchat->firstchatmessage; m; m = m->next)
			{
				if (!BotConstructChatMessageChecked(cs, m->chatmessage, mcontext, &bestmatch, vcontext, qtrue))
					return qfalse;
				BotRemoveTildes(cs->chatmessage);
				botimport.Print(PRT_MESSAGE, "%s\n", cs->chatmessage);
			} //end if
		} //end if
		else
		{
			if (!BotConstructChatMessageChecked(cs, bestchatmessage->chatmessage, mcontext, &bestmatch, vcontext, qtrue))
				return qfalse;
			bestchatmessage->time = AAS_Time() + CHATMESSAGE_RECENTTIME;
		} //end else
		return qtrue;
	} //end if
	return qfalse;
} //end of the function BotReplyChat
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
int BotChatLength(int chatstate)
{
	bot_chatstate_t *cs;

	cs = BotChatStateFromHandle(chatstate);
	if (!cs) return 0;
	return strlen(cs->chatmessage);
} //end of the function BotChatLength
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
void BotEnterChat(int chatstate, int clientto, int sendto)
{
	bot_chatstate_t *cs;

	cs = BotChatStateFromHandle(chatstate);
	if (!cs) return;

	if (strlen(cs->chatmessage))
	{
		BotRemoveTildes(cs->chatmessage);
		if (LibVarGetValue("bot_testichat")) {
			botimport.Print(PRT_MESSAGE, "%s\n", cs->chatmessage);
		}
		else {
			switch(sendto) {
				case CHAT_TEAM:
					EA_Command(cs->client, va("say_team %s", cs->chatmessage));
					break;
				case CHAT_TELL:
					EA_Command(cs->client, va("tell %d %s", clientto, cs->chatmessage));
					break;
				default: //CHAT_ALL
					EA_Command(cs->client, va("say %s", cs->chatmessage));
					break;
			}
		}
		//clear the chat message from the state
		strcpy(cs->chatmessage, "");
	} //end if
} //end of the function BotEnterChat
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
void BotGetChatMessage(int chatstate, char *buf, int size)
{
	bot_chatstate_t *cs;

	cs = BotChatStateFromHandle(chatstate);
	if (!cs) return;

	if (!buf || size < 1)
	{
		botimport.Print(PRT_ERROR, "invalid chat message output\n");
		return;
	}
	BotRemoveTildes(cs->chatmessage);
	strncpy(buf, cs->chatmessage, size-1);
	buf[size-1] = '\0';
	//clear the chat message from the state
	strcpy(cs->chatmessage, "");
} //end of the function BotGetChatMessage
//===========================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//===========================================================================
void BotSetChatGender(int chatstate, int gender)
{
	bot_chatstate_t *cs;

	cs = BotChatStateFromHandle(chatstate);
	if (!cs) return;
	switch(gender)
	{
		case CHAT_GENDERFEMALE: cs->gender = CHAT_GENDERFEMALE; break;
		case CHAT_GENDERMALE: cs->gender = CHAT_GENDERMALE; break;
		default: cs->gender = CHAT_GENDERLESS; break;
	} //end switch
} //end of the function BotSetChatGender
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void BotSetChatName(int chatstate, char *name, int client)
{
	bot_chatstate_t *cs;

	cs = BotChatStateFromHandle(chatstate);
	if (!cs) return;
	if (!name)
	{
		botimport.Print(PRT_ERROR, "missing chat name input\n");
		return;
	}
	cs->client = client;
	Com_Memset(cs->name, 0, sizeof(cs->name));
	strncpy(cs->name, name, sizeof(cs->name));
	cs->name[sizeof(cs->name)-1] = '\0';
} //end of the function BotSetChatName
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void BotResetChatAI(void)
{
	bot_replychat_t *rchat;
	bot_chatmessage_t *m;

	for (rchat = replychats; rchat; rchat = rchat->next)
	{
		for (m = rchat->firstchatmessage; m; m = m->next)
		{
			m->time = 0;
		} //end for
	} //end for
} //end of the function BotResetChatAI
//========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//========================================================================
int BotAllocChatState(void)
{
	int i;

	for (i = 1; i <= MAX_CLIENTS; i++)
	{
		if (!botchatstates[i])
		{
			botchatstates[i] = GetClearedMemory(sizeof(bot_chatstate_t));
			if (!botchatstates[i])
			{
				botimport.Print(PRT_ERROR, "couldn't allocate chat state\n");
				return 0;
			}
			return i;
		} //end if
	} //end for
	return 0;
} //end of the function BotAllocChatState
//========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//========================================================================
void BotFreeChatState(int handle)
{
	bot_chatstate_t *cs;
	bot_consolemessage_t m;
	int h;

	if (handle <= 0 || handle > MAX_CLIENTS)
	{
		botimport.Print(PRT_FATAL, "chat state handle %d out of range\n", handle);
		return;
	} //end if
	if (!botchatstates[handle])
	{
		botimport.Print(PRT_FATAL, "invalid chat state %d\n", handle);
		return;
	} //end if
	cs = botchatstates[handle];
	if (LibVarGetValue("bot_reloadcharacters"))
	{
		BotFreeChatFile(handle);
	} //end if
	//free all the console messages left in the chat state
	for (h = BotNextConsoleMessage(handle, &m); h; h = BotNextConsoleMessage(handle, &m))
	{
		//remove the console message
		BotRemoveConsoleMessage(handle, h);
	} //end for
	FreeMemory(botchatstates[handle]);
	botchatstates[handle] = NULL;
} //end of the function BotFreeChatState
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
int BotSetupChatAI(void)
{
	char *file;

#ifdef DEBUG
	int starttime = Sys_MilliSeconds();
#endif //DEBUG

	if (!InitConsoleMessageHeapChecked()) return BLERR_LIBRARYNOTSETUP;

	file = LibVarString("synfile", "syn.c");
	synonyms = BotLoadSynonyms(file);
	file = LibVarString("rndfile", "rnd.c");
	randomstrings = BotLoadRandomStrings(file);
	file = LibVarString("matchfile", "match.c");
	matchtemplates = BotLoadMatchTemplates(file);
	//
	if (!LibVarValue("nochat", "0"))
	{
		file = LibVarString("rchatfile", "rchat.c");
		replychats = BotLoadReplyChat(file);
	} //end if


#ifdef DEBUG
	botimport.Print(PRT_MESSAGE, "setup chat AI %d msec\n", Sys_MilliSeconds() - starttime);
#endif //DEBUG
	return BLERR_NOERROR;
} //end of the function BotSetupChatAI
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void BotShutdownChatAI(void)
{
	int i;

	//free all remaining chat states
	for(i = 1; i <= MAX_CLIENTS; i++)
	{
		if (botchatstates[i])
		{
			BotFreeChatState(i);
		} //end if
	} //end for
	//free all cached chats
	for(i = 0; i < MAX_CLIENTS; i++)
	{
		if (ichatdata[i])
		{
			FreeMemory(ichatdata[i]->chat);
			FreeMemory(ichatdata[i]);
			ichatdata[i] = NULL;
		} //end if
	} //end for
	if (consolemessageheap) FreeMemory(consolemessageheap);
	consolemessageheap = NULL;
	freeconsolemessages = NULL;
	consolemessageheapcount = 0;
	if (matchtemplates) BotFreeMatchTemplates(matchtemplates);
	matchtemplates = NULL;
	if (randomstrings) FreeMemory(randomstrings);
	randomstrings = NULL;
	if (synonyms) FreeMemory(synonyms);
	synonyms = NULL;
	if (replychats) BotFreeReplyChat(replychats);
	replychats = NULL;
} //end of the function BotShutdownChatAI
