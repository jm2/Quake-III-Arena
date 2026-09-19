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
//

/*****************************************************************************
 * name:		l_precomp.c
 *
 * desc:		pre compiler
 *
 * $Archive: /MissionPack/code/botlib/l_precomp.c $
 *
 *****************************************************************************/

//Notes:			fix: PC_StringizeTokens

//#define SCREWUP
//#define BOTLIB
//#define QUAKE
//#define QUAKEC
//#define MEQCC

#include <float.h>

#ifdef SCREWUP
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include "l_memory.h"
#include "l_script.h"
#include "l_precomp.h"

typedef enum {qfalse, qtrue}	qboolean;
#endif //SCREWUP

#ifdef BOTLIB
#include "../game/q_shared.h"
#include "../game/botlib.h"
#include "be_interface.h"
#include "l_memory.h"
#include "l_script.h"
#include "l_precomp.h"
#include "l_log.h"
#endif //BOTLIB

#ifdef MEQCC
#include "qcc.h"
#include "time.h"   //time & ctime
#include "math.h"   //fabs
#include "l_memory.h"
#include "l_script.h"
#include "l_precomp.h"
#include "l_log.h"

#define qtrue	true
#define qfalse	false
#endif //MEQCC

#ifdef BSPC
//include files for usage in the BSP Converter
#include "../bspc/qbsp.h"
#include "../bspc/l_log.h"
#include "../bspc/l_mem.h"
#include "l_precomp.h"

#define qtrue	true
#define qfalse	false
#define Q_stricmp	stricmp

#endif //BSPC

#if defined(QUAKE) && !defined(BSPC)
#include "l_utils.h"
#endif //QUAKE

//#define DEBUG_EVAL

#define MAX_DEFINEPARMS			128

#define DEFINEHASHING			1

//directive name with parse function
typedef struct directive_s
{
	char *name;
	int (*func)(source_t *source);
} directive_t;

#define DEFINEHASHSIZE		1024

#define TOKEN_HEAP_SIZE		4096
#define MAX_SOURCE_TOKEN_WORK	TOKEN_HEAP_SIZE
#define MAX_SOURCE_TOKEN_DEPTH	128

int numtokens;
/*
int tokenheapinitialized;				//true when the token heap is initialized
token_t token_heap[TOKEN_HEAP_SIZE];	//heap with tokens
token_t *freetokens;					//free tokens from the heap
*/

//list with global defines added to every source loaded
define_t *globaldefines;

//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
void QDECL SourceError(source_t *source, char *str, ...)
{
	char text[1024];
	va_list ap;

	source->errorsequence++;
	if (source->scriptstack) source->scriptstack->flags |= SCFL_SOURCEERROR;
	va_start(ap, str);
	Q_vsnprintf(text, sizeof(text), str, ap);
	va_end(ap);
	text[sizeof(text) - 1] = '\0';
#ifdef BOTLIB
	botimport.Print(PRT_ERROR, "file %s, line %d: %s\n", source->scriptstack->filename, source->scriptstack->line, text);
#endif	//BOTLIB
#ifdef MEQCC
	printf("error: file %s, line %d: %s\n", source->scriptstack->filename, source->scriptstack->line, text);
#endif //MEQCC
#ifdef BSPC
	Log_Print("error: file %s, line %d: %s\n", source->scriptstack->filename, source->scriptstack->line, text);
#endif //BSPC
} //end of the function SourceError
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void QDECL SourceWarning(source_t *source, char *str, ...)
{
	char text[1024];
	va_list ap;

	va_start(ap, str);
	Q_vsnprintf(text, sizeof(text), str, ap);
	va_end(ap);
	text[sizeof(text) - 1] = '\0';
#ifdef BOTLIB
	botimport.Print(PRT_WARNING, "file %s, line %d: %s\n", source->scriptstack->filename, source->scriptstack->line, text);
#endif //BOTLIB
#ifdef MEQCC
	printf("warning: file %s, line %d: %s\n", source->scriptstack->filename, source->scriptstack->line, text);
#endif //MEQCC
#ifdef BSPC
	Log_Print("warning: file %s, line %d: %s\n", source->scriptstack->filename, source->scriptstack->line, text);
#endif //BSPC
} //end of the function ScriptWarning
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
int PC_PushIndent(source_t *source, int type, int skip)
{
	indent_t *indent;

	indent = (indent_t *) GetMemory(sizeof(indent_t));
	if (!indent)
	{
		SourceError(source, "could not allocate conditional indent");
		return qfalse;
	} //end if
	indent->type = type;
	indent->script = source->scriptstack;
	indent->skip = (skip != 0);
	source->skip += indent->skip;
	indent->next = source->indentstack;
	source->indentstack = indent;
	return qtrue;
} //end of the function PC_PushIndent
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
void PC_PopIndent(source_t *source, int *type, int *skip)
{
	indent_t *indent;

	*type = 0;
	*skip = 0;

	indent = source->indentstack;
	if (!indent) return;

	//must be an indent from the current script
	if (source->indentstack->script != source->scriptstack) return;

	*type = indent->type;
	*skip = indent->skip;
	source->indentstack = source->indentstack->next;
	source->skip -= indent->skip;
	FreeMemory(indent);
} //end of the function PC_PopIndent
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
int PC_PushScript(source_t *source, script_t *script)
{
	script_t *s;

	for (s = source->scriptstack; s; s = s->next)
	{
		if (!Q_stricmp(s->filename, script->filename))
		{
			SourceError(source, "%s recursively included", script->filename);
			return qfalse;
		} //end if
	} //end for
	//push the script on the script stack
	script->next = source->scriptstack;
	source->scriptstack = script;
	return qtrue;
} //end of the function PC_PushScript
//============================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//============================================================================
void PC_InitTokenHeap(void)
{
	/*
	int i;

	if (tokenheapinitialized) return;
	freetokens = NULL;
	for (i = 0; i < TOKEN_HEAP_SIZE; i++)
	{
		token_heap[i].next = freetokens;
		freetokens = &token_heap[i];
	} //end for
	tokenheapinitialized = qtrue;
	*/
} //end of the function PC_InitTokenHeap
//============================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//============================================================================
token_t *PC_CopyToken(token_t *token)
{
	token_t *t;

//	t = (token_t *) malloc(sizeof(token_t));
	t = (token_t *) GetMemory(sizeof(token_t));
//	t = freetokens;
	if (!t)
	{
#ifdef BSPC
		Error("out of token space\n");
#else
		Com_Error(ERR_FATAL, "out of token space\n");
#endif
		return NULL;
	} //end if
//	freetokens = freetokens->next;
	Com_Memcpy(t, token, sizeof(token_t));
	t->next = NULL;
	numtokens++;
	return t;
} //end of the function PC_CopyToken
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
void PC_FreeToken(token_t *token)
{
	//free(token);
	FreeMemory(token);
//	token->next = freetokens;
//	freetokens = token;
	numtokens--;
} //end of the function PC_FreeToken

static void PC_DiscardSourceTokens(source_t *source)
{
	token_t *token;
	while (source && source->tokens)
	{
		token = source->tokens;
		source->tokens = token->next;
		PC_FreeToken(token);
	}
}

static int PC_EnterTokenWork(source_t *source)
{
	if (!source) return qfalse;
	if (!source->tokenworkdepth) source->tokenwork = 0;
	if (source->tokenworkdepth >= MAX_SOURCE_TOKEN_DEPTH)
	{
		SourceError(source, "preprocessor token recursion limit exceeded");
		PC_DiscardSourceTokens(source);
		return qfalse;
	}
	source->tokenworkdepth++;
	return qtrue;
}

static void PC_LeaveTokenWork(source_t *source)
{
	if (source && source->tokenworkdepth) source->tokenworkdepth--;
}

static int PC_ConsumeTokenWork(source_t *source)
{
	if (!source || !source->tokenworkdepth) return qtrue;
	if (source->tokenwork >= MAX_SOURCE_TOKEN_WORK)
	{
		SourceError(source, "preprocessor token work limit exceeded");
		PC_DiscardSourceTokens(source);
		return qfalse;
	}
	source->tokenwork++;
	return qtrue;
}
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
static int PC_SourceErrorFlag(source_t *source, int mask)
{
	script_t *script;
	for (script = source ? source->scriptstack : NULL; script; script = script->next)
	{
		if (script->flags & mask) return qtrue;
	}
	return qfalse;
}

int PC_SourceHasError(source_t *source)
{
	return PC_SourceErrorFlag(source, SCFL_LEXERROR | SCFL_SOURCEERROR);
}

int PC_ReadSourceToken(source_t *source, token_t *token)
{
	token_t *t;
	script_t *script;
	int type, skip;

	if (PC_SourceErrorFlag(source, SCFL_LEXERROR)) return qfalse;
	//if there's no token already available
	while(!source->tokens)
	{
		//if there's a token to read from the script
		if (PS_ReadToken(source->scriptstack, token))
		{
			if (!PC_ConsumeTokenWork(source)) return qfalse;
			return qtrue;
		}
		if (PC_SourceErrorFlag(source, SCFL_LEXERROR)) return qfalse;
		//if at the end of the script
		if (EndOfScript(source->scriptstack))
		{
			//remove all indents of the script
			while(source->indentstack &&
					source->indentstack->script == source->scriptstack)
			{
				SourceWarning(source, "missing #endif");
				PC_PopIndent(source, &type, &skip);
			} //end if
		} //end if
		//if this was the initial script
		if (!source->scriptstack->next) return qfalse;
		//remove the script and return to the last one
		script = source->scriptstack;
		source->scriptstack = source->scriptstack->next;
		source->scriptstack->flags |= script->flags & SCFL_SOURCEERROR;
		FreeScript(script);
	} //end while
	//copy the already available token
	Com_Memcpy(token, source->tokens, sizeof(token_t));
	//free the read token
	t = source->tokens;
	source->tokens = source->tokens->next;
	PC_FreeToken(t);
	if (!PC_ConsumeTokenWork(source))
	{
		PC_DiscardSourceTokens(source);
		return qfalse;
	}
	return qtrue;
} //end of the function PC_ReadSourceToken
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
int PC_UnreadSourceToken(source_t *source, token_t *token)
{
	token_t *t;

	t = PC_CopyToken(token);
	if (!t)
	{
		SourceError(source, "could not unread source token");
		return qfalse;
	} //end if
	t->next = source->tokens;
	source->tokens = t;
	return qtrue;
} //end of the function PC_UnreadSourceToken
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
int PC_ReadDefineParms(source_t *source, define_t *define, token_t **parms, int maxparms)
{
	token_t token, *t, *last;
	int i, done, lastcomma, numparms, indent;

	if (!PC_ReadSourceToken(source, &token))
	{
		SourceError(source, "define %s missing parms", define->name);
		return qfalse;
	} //end if
	//
	if (define->numparms > maxparms)
	{
		SourceError(source, "define with more than %d parameters", maxparms);
		return qfalse;
	} //end if
	//
	for (i = 0; i < define->numparms; i++) parms[i] = NULL;
	//if no leading "("
	if (strcmp(token.string, "("))
	{
		PC_UnreadSourceToken(source, &token);
		SourceError(source, "define %s missing parms", define->name);
		return qfalse;
	} //end if
	//read the define parameters
	for (done = 0, numparms = 0, indent = 0; !done;)
	{
		if (numparms >= maxparms)
		{
			SourceError(source, "define %s with too many parms", define->name);
			return qfalse;
		} //end if
		if (numparms >= define->numparms)
		{
			SourceWarning(source, "define %s has too many parms", define->name);
			return qfalse;
		} //end if
		parms[numparms] = NULL;
		lastcomma = 1;
		last = NULL;
		while(!done)
		{
			//
			if (!PC_ReadSourceToken(source, &token))
			{
				SourceError(source, "define %s incomplete", define->name);
				return qfalse;
			} //end if
			//
			if (!strcmp(token.string, ","))
			{
				if (indent <= 0)
				{
					if (lastcomma) SourceWarning(source, "too many comma's");
					lastcomma = 1;
					break;
				} //end if
			} //end if
			lastcomma = 0;
			//
			if (!strcmp(token.string, "("))
			{
				indent++;
				continue;
			} //end if
			else if (!strcmp(token.string, ")"))
			{
				if (--indent <= 0)
				{
					if (!parms[define->numparms-1])
					{
						SourceWarning(source, "too few define parms");
					} //end if
					done = 1;
					break;
				} //end if
			} //end if
			//
			if (numparms < define->numparms)
			{
				//
				t = PC_CopyToken(&token);
				if (!t) return qfalse;
				t->next = NULL;
				if (last) last->next = t;
				else parms[numparms] = t;
				last = t;
			} //end if
		} //end while
		numparms++;
	} //end for
	return qtrue;
} //end of the function PC_ReadDefineParms
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
/* Append only a complete string, including its terminator. */
static int PC_AppendString(char *output, size_t capacity, const char *input)
{
	size_t used = strlen(output), length = strlen(input);
	if (used >= capacity || length >= capacity - used) return qfalse;
	memmove(output + used, input, length + 1);
	return qtrue;
}

int PC_StringizeTokens(token_t *tokens, token_t *token)
{
	token_t *t;
	char string[MAX_TOKEN];
	strcpy(string, "\"");
	for (t = tokens; t; t = t->next)
	{
		/* Reserve the closing quote as well as the terminator. */
		if (!PC_AppendString(string, sizeof(string) - 1, t->string)) return qfalse;
	} //end for
	if (!PC_AppendString(string, sizeof(string), "\"")) return qfalse;

	token->type = TT_STRING;
	token->subtype = strlen(string);
	token->whitespace_p = NULL;
	token->endwhitespace_p = NULL;
	memcpy(token->string, string, strlen(string) + 1);
	return qtrue;
} //end of the function PC_StringizeTokens
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
int PC_MergeTokens(token_t *t1, token_t *t2)
{
	size_t firstlength, secondlength;
	//merging of a name with a name or number
	if (t1->type == TT_NAME && (t2->type == TT_NAME || t2->type == TT_NUMBER))
	{
		return PC_AppendString(t1->string, sizeof(t1->string), t2->string);
	} //end if
	//merging of two strings
	if (t1->type == TT_STRING && t2->type == TT_STRING)
	{
		firstlength = strlen(t1->string);
		secondlength = strlen(t2->string);
		if (firstlength < 2 || secondlength < 2 ||
			firstlength - 1 >= sizeof(t1->string) ||
			secondlength - 1 >= sizeof(t1->string) - (firstlength - 1)) return qfalse;
		//remove trailing double quote
		t1->string[firstlength-1] = '\0';
		//concat without leading double quote
		PC_AppendString(t1->string, sizeof(t1->string), &t2->string[1]);
		return qtrue;
	} //end if
	//FIXME: merging of two number of the same sub type
	return qfalse;
} //end of the function PC_MergeTokens
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
/*
void PC_PrintDefine(define_t *define)
{
	printf("define->name = %s\n", define->name);
	printf("define->flags = %d\n", define->flags);
	printf("define->builtin = %d\n", define->builtin);
	printf("define->numparms = %d\n", define->numparms);
//	token_t *parms;					//define parameters
//	token_t *tokens;					//macro tokens (possibly containing parm tokens)
//	struct define_s *next;			//next defined macro in a list
} //end of the function PC_PrintDefine*/
#if DEFINEHASHING
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
void PC_PrintDefineHashTable(define_t **definehash)
{
	int i;
	define_t *d;

	for (i = 0; i < DEFINEHASHSIZE; i++)
	{
		Log_Write("%4d:", i);
		for (d = definehash[i]; d; d = d->hashnext)
		{
			Log_Write(" %s", d->name);
		} //end for
		Log_Write("\n");
	} //end for
} //end of the function PC_PrintDefineHashTable
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
//char primes[16] = {1, 3, 5, 7, 11, 13, 17, 19, 23, 27, 29, 31, 37, 41, 43, 47};

int PC_NameHash(char *name)
{
	int register hash, i;

	hash = 0;
	for (i = 0; name[i] != '\0'; i++)
	{
		hash += name[i] * (119 + i);
		//hash += (name[i] << 7) + i;
		//hash += (name[i] << (i&15));
	} //end while
	hash = (hash ^ (hash >> 10) ^ (hash >> 20)) & (DEFINEHASHSIZE-1);
	return hash;
} //end of the function PC_NameHash
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
void PC_AddDefineToHash(define_t *define, define_t **definehash)
{
	int hash;

	hash = PC_NameHash(define->name);
	define->hashnext = definehash[hash];
	definehash[hash] = define;
} //end of the function PC_AddDefineToHash
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
define_t *PC_FindHashedDefine(define_t **definehash, char *name)
{
	define_t *d;
	int hash;

	hash = PC_NameHash(name);
	for (d = definehash[hash]; d; d = d->hashnext)
	{
		if (!strcmp(d->name, name)) return d;
	} //end for
	return NULL;
} //end of the function PC_FindHashedDefine
#endif //DEFINEHASHING
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
define_t *PC_FindDefine(define_t *defines, char *name)
{
	define_t *d;

	for (d = defines; d; d = d->next)
	{
		if (!strcmp(d->name, name)) return d;
	} //end for
	return NULL;
} //end of the function PC_FindDefine
//============================================================================
//
// Parameter:				-
// Returns:					number of the parm
//								if no parm found with the given name -1 is returned
// Changes Globals:		-
//============================================================================
int PC_FindDefineParm(define_t *define, char *name)
{
	token_t *p;
	int i;

	i = 0;
	for (p = define->parms; p; p = p->next)
	{
		if (!strcmp(p->string, name)) return i;
		i++;
	} //end for
	return -1;
} //end of the function PC_FindDefineParm
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
void PC_FreeDefine(define_t *define)
{
	token_t *t, *next;

	//free the define parameters
	for (t = define->parms; t; t = next)
	{
		next = t->next;
		PC_FreeToken(t);
	} //end for
	//free the define tokens
	for (t = define->tokens; t; t = next)
	{
		next = t->next;
		PC_FreeToken(t);
	} //end for
	//free the define
	FreeMemory(define);
} //end of the function PC_FreeDefine
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
void PC_AddBuiltinDefines(source_t *source)
{
	int i;
	define_t *define;
	struct builtin
	{
		char *string;
		int builtin;
	} builtin[] = { // bk001204 - brackets
		{ "__LINE__",	BUILTIN_LINE },
		{ "__FILE__",	BUILTIN_FILE },
		{ "__DATE__",	BUILTIN_DATE },
		{ "__TIME__",	BUILTIN_TIME },
//		{ "__STDC__", BUILTIN_STDC },
		{ NULL, 0 }
	};

	for (i = 0; builtin[i].string; i++)
	{
		define = (define_t *) GetMemory(sizeof(define_t) + strlen(builtin[i].string) + 1);
		Com_Memset(define, 0, sizeof(define_t));
		define->name = (char *) define + sizeof(define_t);
		strcpy(define->name, builtin[i].string);
		define->flags |= DEFINE_FIXED;
		define->builtin = builtin[i].builtin;
		//add the define to the source
#if DEFINEHASHING
		PC_AddDefineToHash(define, source->definehash);
#else
		define->next = source->defines;
		source->defines = define;
#endif //DEFINEHASHING
	} //end for
} //end of the function PC_AddBuiltinDefines
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
int PC_ExpandBuiltinDefine(source_t *source, token_t *deftoken, define_t *define,
										token_t **firsttoken, token_t **lasttoken)
{
	token_t *token;
	// time_t t; removed here, declared locally in cases
	const char *curtime;

	*firsttoken = NULL;
	*lasttoken = NULL;
	token = PC_CopyToken(deftoken);
	if (!token) return qfalse;
	switch(define->builtin)
	{
		case BUILTIN_LINE:
		{
			sprintf(token->string, "%d", deftoken->line);
#ifdef NUMBERVALUE
			token->intvalue = deftoken->line;
			token->floatvalue = deftoken->line;
#endif //NUMBERVALUE
			token->type = TT_NUMBER;
			token->subtype = TT_DECIMAL | TT_INTEGER;
			*firsttoken = token;
			*lasttoken = token;
			break;
		} //end case
		case BUILTIN_FILE:
		{
			strcpy(token->string, source->scriptstack->filename);
			token->type = TT_NAME;
			token->subtype = strlen(token->string);
			*firsttoken = token;
			*lasttoken = token;
			break;
		} //end case
		case BUILTIN_DATE:
		{
			time_t t;
			t = time(NULL);
			curtime = ctime(&t);
			if (!curtime)
			{
				PC_FreeToken(token);
				SourceError(source, "could not expand __DATE__");
				return qfalse;
			} //end if
			strcpy(token->string, "\"");
			strncat(token->string, curtime+4, 7);
			strncat(token->string+7, curtime+20, 4);
			strcat(token->string, "\"");
			token->type = TT_NAME;
			token->subtype = strlen(token->string);
			*firsttoken = token;
			*lasttoken = token;
			break;
		} //end case
		case BUILTIN_TIME:
		{
			time_t t;
			t = time(NULL);
			curtime = ctime(&t);
			if (!curtime)
			{
				PC_FreeToken(token);
				SourceError(source, "could not expand __TIME__");
				return qfalse;
			} //end if
			strcpy(token->string, "\"");
			strncat(token->string, curtime+11, 8);
			strcat(token->string, "\"");
			token->type = TT_NAME;
			token->subtype = strlen(token->string);
			*firsttoken = token;
			*lasttoken = token;
			break;
		} //end case
		case BUILTIN_STDC:
		default:
		{
			PC_FreeToken(token);
			*firsttoken = NULL;
			*lasttoken = NULL;
			break;
		} //end case
	} //end switch
	return qtrue;
} //end of the function PC_ExpandBuiltinDefine
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
int PC_ExpandDefine(source_t *source, token_t *deftoken, define_t *define,
										token_t **firsttoken, token_t **lasttoken)
{
	token_t *parms[MAX_DEFINEPARMS], *dt, *pt, *t;
	token_t *t1, *t2, *first, *last, *nextpt, token;
	int parmnum, i, result = qfalse;

	//if it is a builtin define
	if (define->builtin)
	{
		return PC_ExpandBuiltinDefine(source, deftoken, define, firsttoken, lasttoken);
	} //end if
	*firsttoken = *lasttoken = NULL;
	first = last = NULL;
	Com_Memset(parms, 0, sizeof(parms));
	if (define->numparms < 0 || define->numparms > MAX_DEFINEPARMS)
	{
		SourceError(source, "define parameter count out of range");
		return qfalse;
	} //end if
	//if the define has parameters
	if (define->numparms)
	{
		if (!PC_ReadDefineParms(source, define, parms, MAX_DEFINEPARMS)) goto cleanup;
#ifdef DEBUG_EVAL
		for (i = 0; i < define->numparms; i++)
		{
			Log_Write("define parms %d:", i);
			for (pt = parms[i]; pt; pt = pt->next)
			{
				Log_Write("%s", pt->string);
			} //end for
		} //end for
#endif //DEBUG_EVAL
	} //end if
	//empty list at first
	first = NULL;
	last = NULL;
	//create a list with tokens of the expanded define
	for (dt = define->tokens; dt; dt = dt->next)
	{
		parmnum = -1;
		//if the token is a name, it could be a define parameter
		if (dt->type == TT_NAME)
		{
			parmnum = PC_FindDefineParm(define, dt->string);
		} //end if
		//if it is a define parameter
		if (parmnum >= 0)
		{
			for (pt = parms[parmnum]; pt; pt = pt->next)
			{
				if (!PC_ConsumeTokenWork(source)) goto cleanup;
				t = PC_CopyToken(pt);
				if (!t) goto cleanup;
				//add the token to the list
				t->next = NULL;
				if (last) last->next = t;
				else first = t;
				last = t;
			} //end for
		} //end if
		else
		{
			//if stringizing operator
			if (dt->string[0] == '#' && dt->string[1] == '\0')
			{
				//the stringizing operator must be followed by a define parameter
				if (dt->next) parmnum = PC_FindDefineParm(define, dt->next->string);
				else parmnum = -1;
				//
				if (parmnum >= 0)
				{
					//step over the stringizing operator
					dt = dt->next;
					//stringize the define parameter tokens
					token = *deftoken;
					if (!PC_StringizeTokens(parms[parmnum], &token))
					{
						SourceError(source, "can't stringize tokens");
						goto cleanup;
					} //end if
					t = PC_CopyToken(&token);
				} //end if
				else
				{
					SourceWarning(source, "stringizing operator without define parameter");
					continue;
				} //end if
			} //end if
			else
			{
				if (!PC_ConsumeTokenWork(source)) goto cleanup;
				t = PC_CopyToken(dt);
			} //end else
			//add the token to the list
			if (!t) goto cleanup;
			t->next = NULL;
			if (last) last->next = t;
			else first = t;
			last = t;
		} //end else
	} //end for
	//check for the merging operator
	for (t = first; t; )
	{
		if (t->next)
		{
			//if the merging operator
			if (t->next->string[0] == '#' && t->next->string[1] == '#')
			{
				t1 = t;
				t2 = t->next->next;
				if (t2)
				{
					if (!PC_MergeTokens(t1, t2))
					{
						SourceError(source, "can't merge %s with %s", t1->string, t2->string);
						goto cleanup;
					} //end if
					PC_FreeToken(t1->next);
					t1->next = t2->next;
					if (t2 == last) last = t1;
					PC_FreeToken(t2);
					continue;
				} //end if
			} //end if
		} //end if
		t = t->next;
	} //end for
	//store the first and last token of the list
	*firsttoken = first;
	*lasttoken = last;
	first = NULL;
	result = qtrue;
cleanup:
	while (first)
	{
		nextpt = first->next;
		PC_FreeToken(first);
		first = nextpt;
	} //end while
	//free all the parameter tokens
	for (i = 0; i < define->numparms; i++)
	{
		for (pt = parms[i]; pt; pt = nextpt)
		{
			nextpt = pt->next;
			PC_FreeToken(pt);
		} //end for
	} //end for
	//
	return result;
} //end of the function PC_ExpandDefine
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
int PC_ExpandDefineIntoSource(source_t *source, token_t *deftoken, define_t *define)
{
	token_t *firsttoken, *lasttoken;

	if (!PC_ExpandDefine(source, deftoken, define, &firsttoken, &lasttoken)) return qfalse;

	if (firsttoken && lasttoken)
	{
		lasttoken->next = source->tokens;
		source->tokens = firsttoken;
		return qtrue;
	} //end if
	//A complete empty expansion consumes the macro; callers continue reading.
	return qtrue;
} //end of the function PC_ExpandDefineIntoSource
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
void PC_ConvertPath(char *path)
{
	char *ptr;

	//remove double path seperators
	for (ptr = path; *ptr;)
	{
		if ((*ptr == '\\' || *ptr == '/') &&
				(*(ptr+1) == '\\' || *(ptr+1) == '/'))
		{
			memmove(ptr, ptr+1, strlen(ptr));
		} //end if
		else
		{
			ptr++;
		} //end else
	} //end while
	//set OS dependent path seperators
	for (ptr = path; *ptr;)
	{
		if (*ptr == '/' || *ptr == '\\') *ptr = PATHSEPERATOR_CHAR;
		ptr++;
	} //end while
} //end of the function PC_ConvertPath

//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
int PC_Directive_include(source_t *source)
{
	script_t *script;
	token_t token;
	char path[MAX_PATH];
	int overflow, closed, filename;
#ifdef QUAKE
	foundfile_t file;
#endif //QUAKE

	if (source->skip > 0) return qtrue;
	//
	if (!PC_ReadSourceToken(source, &token))
	{
		SourceError(source, "#include without file name");
		return qfalse;
	} //end if
	if (token.linescrossed > 0)
	{
		PC_UnreadSourceToken(source, &token);
		SourceError(source, "#include without file name");
		return qfalse;
	} //end if
	if (token.type == TT_STRING)
	{
		StripDoubleQuotes(token.string);
		PC_ConvertPath(token.string);
		script = LoadScriptFile(token.string);
		if (!script)
		{
			path[0] = '\0';
			if (!PC_AppendString(path, sizeof(path), source->includepath) ||
				!PC_AppendString(path, sizeof(path), token.string))
			{
				SourceError(source, "#include path too long");
				return qfalse;
			} //end if
			script = LoadScriptFile(path);
		} //end if
	} //end if
	else if (token.type == TT_PUNCTUATION && *token.string == '<')
	{
		path[0] = '\0';
		overflow = !PC_AppendString(path, sizeof(path), source->includepath);
		closed = filename = 0;
		while(PC_ReadSourceToken(source, &token))
		{
			if (token.linescrossed > 0)
			{
				PC_UnreadSourceToken(source, &token);
				break;
			} //end if
			if (token.type == TT_PUNCTUATION && *token.string == '>')
			{
				closed = 1;
				break;
			} //end if
			if (*token.string) filename = 1;
			if (!overflow && !PC_AppendString(path, sizeof(path), token.string)) overflow = 1;
		} //end while
		if (!closed)
		{
			SourceWarning(source, "#include missing trailing >");
			return qfalse;
		} //end if
		if (overflow)
		{
			SourceError(source, "#include path too long");
			return qfalse;
		} //end if
		if (!filename)
		{
			SourceError(source, "#include without file name between < >");
			return qfalse;
		} //end if
		PC_ConvertPath(path);
		script = LoadScriptFile(path);
	} //end if
	else
	{
		SourceError(source, "#include without file name");
		return qfalse;
	} //end else
#ifdef QUAKE
	if (!script)
	{
		Com_Memset(&file, 0, sizeof(foundfile_t));
		script = LoadScriptFile(path);
		if (script) strncpy(script->filename, path, MAX_PATH);
	} //end if
#endif //QUAKE
	if (!script)
	{
#ifdef SCREWUP
		SourceWarning(source, "file %s not found", path);
		return qtrue;
#else
		SourceError(source, "file %s not found", path);
		return qfalse;
#endif //SCREWUP
	} //end if
	if (!PC_PushScript(source, script))
	{
		FreeScript(script);
		return qfalse;
	} //end if
	return qtrue;
} //end of the function PC_Directive_include
//============================================================================
// reads a token from the current line, continues reading on the next
// line only if a backslash '\' is encountered.
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
int PC_ReadLine(source_t *source, token_t *token)
{
	int crossline;

	crossline = 0;
	do
	{
		if (!PC_ReadSourceToken(source, token)) return qfalse;
		
		if (token->linescrossed > crossline)
		{
			PC_UnreadSourceToken(source, token);
			return qfalse;
		} //end if
		crossline = 1;
	} while(!strcmp(token->string, "\\"));
	return qtrue;
} //end of the function PC_ReadLine
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
int PC_WhiteSpaceBeforeToken(token_t *token)
{
	return token->endwhitespace_p - token->whitespace_p > 0;
} //end of the function PC_WhiteSpaceBeforeToken
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
void PC_ClearTokenWhiteSpace(token_t *token)
{
	token->whitespace_p = NULL;
	token->endwhitespace_p = NULL;
	token->linescrossed = 0;
} //end of the function PC_ClearTokenWhiteSpace
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
int PC_Directive_undef(source_t *source)
{
	token_t token;
	define_t *define, *lastdefine;
	int hash;

	if (source->skip > 0) return qtrue;
	//
	if (!PC_ReadLine(source, &token))
	{
		SourceError(source, "undef without name");
		return qfalse;
	} //end if
	if (token.type != TT_NAME)
	{
		PC_UnreadSourceToken(source, &token);
		SourceError(source, "expected name, found %s", token.string);
		return qfalse;
	} //end if
#if DEFINEHASHING

	hash = PC_NameHash(token.string);
	for (lastdefine = NULL, define = source->definehash[hash]; define; define = define->hashnext)
	{
		if (!strcmp(define->name, token.string))
		{
			if (define->flags & DEFINE_FIXED)
			{
				SourceWarning(source, "can't undef %s", token.string);
			} //end if
			else
			{
				if (lastdefine) lastdefine->hashnext = define->hashnext;
				else source->definehash[hash] = define->hashnext;
				PC_FreeDefine(define);
			} //end else
			break;
		} //end if
		lastdefine = define;
	} //end for
#else //DEFINEHASHING
	for (lastdefine = NULL, define = source->defines; define; define = define->next)
	{
		if (!strcmp(define->name, token.string))
		{
			if (define->flags & DEFINE_FIXED)
			{
				SourceWarning(source, "can't undef %s", token.string);
			} //end if
			else
			{
				if (lastdefine) lastdefine->next = define->next;
				else source->defines = define->next;
				PC_FreeDefine(define);
			} //end else
			break;
		} //end if
		lastdefine = define;
	} //end for
#endif //DEFINEHASHING
	return qtrue;
} //end of the function PC_Directive_undef
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
int PC_Directive_define(source_t *source)
{
	token_t token, *t, *last;
	define_t *define = NULL, *previous;
	unsigned int errorsequence = source->errorsequence;

	if (source->skip > 0) return qtrue;
	//
	if (!PC_ReadLine(source, &token))
	{
		SourceError(source, "#define without name");
		goto failed;
	} //end if
	if (token.type != TT_NAME)
	{
		PC_UnreadSourceToken(source, &token);
		SourceError(source, "expected name after #define, found %s", token.string);
		goto failed;
	} //end if
	//check if the define already exists
#if DEFINEHASHING
	previous = PC_FindHashedDefine(source->definehash, token.string);
#else
	previous = PC_FindDefine(source->defines, token.string);
#endif //DEFINEHASHING
	if (previous)
	{
		if (previous->flags & DEFINE_FIXED)
		{
			SourceError(source, "can't redefine %s", token.string);
			goto failed;
		} //end if
		SourceWarning(source, "redefinition of %s", token.string);
	} //end if
	//allocate define
	define = (define_t *) GetMemory(sizeof(define_t) + strlen(token.string) + 1);
	if (!define)
	{
		SourceError(source, "could not allocate definition");
		goto failed;
	} //end if
	Com_Memset(define, 0, sizeof(define_t));
	define->name = (char *) define + sizeof(define_t);
	strcpy(define->name, token.string);
	//if nothing is defined, just return
	if (!PC_ReadLine(source, &token)) goto publish;
	//if it is a define with parameters
	if (!PC_WhiteSpaceBeforeToken(&token) && !strcmp(token.string, "("))
	{
		//read the define parameters
		last = NULL;
		if (!PC_ReadLine(source, &token))
		{
			SourceError(source, "expected define parameter");
			goto failed;
		} //end if
		if (strcmp(token.string, ")"))
		{
			while(1)
			{
				//if it isn't a name
				if (token.type != TT_NAME)
				{
					SourceError(source, "invalid define parameter");
					goto failed;
				} //end if
				//
				if (PC_FindDefineParm(define, token.string) >= 0)
				{
					SourceError(source, "two the same define parameters");
					goto failed;
				} //end if
				if (define->numparms >= MAX_DEFINEPARMS)
				{
					SourceError(source, "too many define parameters");
					goto failed;
				} //end if
				//add the define parm
				t = PC_CopyToken(&token);
				if (!t)
				{
					SourceError(source, "could not copy definition token");
					goto failed;
				} //end if
				PC_ClearTokenWhiteSpace(t);
				t->next = NULL;
				if (last) last->next = t;
				else define->parms = t;
				last = t;
				define->numparms++;
				//read next token
				if (!PC_ReadLine(source, &token))
				{
					SourceError(source, "define parameters not terminated");
					goto failed;
				} //end if
				//
				if (!strcmp(token.string, ")")) break;
				//then it must be a comma
				if (strcmp(token.string, ","))
				{
					SourceError(source, "define not terminated");
					goto failed;
				} //end if
				if (!PC_ReadLine(source, &token))
				{
					SourceError(source, "expected define parameter");
					goto failed;
				} //end if
			} //end while
		} //end if
		if (!PC_ReadLine(source, &token)) goto publish;
	} //end if
	//read the defined stuff
	last = NULL;
	do
	{
		t = PC_CopyToken(&token);
		if (!t)
		{
			SourceError(source, "could not copy definition token");
			goto failed;
		} //end if
		if (t->type == TT_NAME && !strcmp(t->string, define->name))
		{
			SourceError(source, "recursive define");
			PC_FreeToken(t);
			goto failed;
		} //end if
		PC_ClearTokenWhiteSpace(t);
		t->next = NULL;
		if (last) last->next = t;
		else define->tokens = t;
		last = t;
	} while(PC_ReadLine(source, &token));
	//
	if (last)
	{
		//check for merge operators at the beginning or end
		if (!strcmp(define->tokens->string, "##") ||
				!strcmp(last->string, "##"))
		{
			SourceError(source, "define with misplaced ##");
			goto failed;
		} //end if
	} //end if
publish:
	if (PC_SourceErrorFlag(source, SCFL_LEXERROR) ||
			source->errorsequence != errorsequence) goto failed;
	if (previous)
	{
		define_t **link;
#if DEFINEHASHING
		for (link = &source->definehash[PC_NameHash(previous->name)];
				*link != previous; link = &(*link)->hashnext) ;
		*link = previous->hashnext;
#else
		for (link = &source->defines; *link != previous; link = &(*link)->next) ;
		*link = previous->next;
#endif //DEFINEHASHING
		PC_FreeDefine(previous);
	} //end if
#if DEFINEHASHING
	PC_AddDefineToHash(define, source->definehash);
#else
	define->next = source->defines;
	source->defines = define;
#endif //DEFINEHASHING
	return qtrue;
failed:
	if (define) PC_FreeDefine(define);
	return qfalse;
} //end of the function PC_Directive_define
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
define_t *PC_DefineFromString(char *string)
{
	script_t *script;
	source_t src;
	token_t *t;
	define_t *def = NULL;
	int res, i;
	size_t length;
	if (!string || !*string) return NULL;
	length = strlen(string);
	if (length > INT_MAX) return NULL;
	PC_InitTokenHeap();
	script = LoadScriptMemory(string, (int)length, "*extern");
	if (!script) return NULL;
	Com_Memset(&src, 0, sizeof(src));
	strcpy(src.filename, "*extern");
	src.scriptstack = script;
#if DEFINEHASHING
	src.definehash = GetClearedMemory(DEFINEHASHSIZE * sizeof(define_t *));
	if (!src.definehash)
	{
		FreeScript(script);
		return NULL;
	} //end if
#endif //DEFINEHASHING
	if (!PC_EnterTokenWork(&src))
	{
#if DEFINEHASHING
		FreeMemory(src.definehash);
#endif
		FreeScript(script);
		return NULL;
	}
	res = PC_Directive_define(&src);
	PC_LeaveTokenWork(&src);
	for (t = src.tokens; t; t = src.tokens)
	{
		src.tokens = t->next;
		PC_FreeToken(t);
	} //end for
#if DEFINEHASHING
	for (i = 0; i < DEFINEHASHSIZE; i++)
		if (src.definehash[i]) { def = src.definehash[i]; break; }
	FreeMemory(src.definehash);
#else
	def = src.defines;
#endif //DEFINEHASHING
	if (PC_SourceHasError(&src)) res = qfalse;
	FreeScript(script);
	if (res) return def;
	if (def) PC_FreeDefine(def);
	return NULL;
} //end of the function PC_DefineFromString
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
int PC_AddDefine(source_t *source, char *string)
{
	define_t *define;

	define = PC_DefineFromString(string);
	if (!define) return qfalse;
#if DEFINEHASHING
	PC_AddDefineToHash(define, source->definehash);
#else //DEFINEHASHING
	define->next = source->defines;
	source->defines = define;
#endif //DEFINEHASHING
	return qtrue;
} //end of the function PC_AddDefine
//============================================================================
// add a globals define that will be added to all opened sources
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
int PC_AddGlobalDefine(char *string)
{
	define_t *define;

	define = PC_DefineFromString(string);
	if (!define) return qfalse;
	define->next = globaldefines;
	globaldefines = define;
	return qtrue;
} //end of the function PC_AddGlobalDefine
//============================================================================
// remove the given global define
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
int PC_RemoveGlobalDefine(char *name)
{
	define_t *define, **link;

	if (!name) return qfalse;
	for (link = &globaldefines; *link; link = &(*link)->next)
	{
		define = *link;
		if (!strcmp(define->name, name))
		{
			*link = define->next;
			PC_FreeDefine(define);
			return qtrue;
		}
	} //end for
	return qfalse;
} //end of the function PC_RemoveGlobalDefine
//============================================================================
// remove all globals defines
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
void PC_RemoveAllGlobalDefines(void)
{
	define_t *define;

	for (define = globaldefines; define; define = globaldefines)
	{
		globaldefines = globaldefines->next;
		PC_FreeDefine(define);
	} //end for
} //end of the function PC_RemoveAllGlobalDefines
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
define_t *PC_CopyDefine(source_t *source, define_t *define)
{
	define_t *newdefine;
	token_t *token, *newtoken, *lasttoken;
	size_t length = strlen(define->name);

	if (length > (size_t)INT_MAX - sizeof(define_t) - 1) return NULL;
	newdefine = (define_t *) GetMemory(sizeof(define_t) + length + 1);
	if (!newdefine) return NULL;
	Com_Memset(newdefine, 0, sizeof(define_t));
	//copy the define name
	newdefine->name = (char *) newdefine + sizeof(define_t);
	strcpy(newdefine->name, define->name);
	newdefine->flags = define->flags;
	newdefine->builtin = define->builtin;
	newdefine->numparms = define->numparms;
	//the define is not linked
	newdefine->next = NULL;
	newdefine->hashnext = NULL;
	//copy the define tokens
	newdefine->tokens = NULL;
	for (lasttoken = NULL, token = define->tokens; token; token = token->next)
	{
		newtoken = PC_CopyToken(token);
		if (!newtoken) goto failure;
		newtoken->next = NULL;
		if (lasttoken) lasttoken->next = newtoken;
		else newdefine->tokens = newtoken;
		lasttoken = newtoken;
	} //end for
	//copy the define parameters
	newdefine->parms = NULL;
	for (lasttoken = NULL, token = define->parms; token; token = token->next)
	{
		newtoken = PC_CopyToken(token);
		if (!newtoken) goto failure;
		newtoken->next = NULL;
		if (lasttoken) lasttoken->next = newtoken;
		else newdefine->parms = newtoken;
		lasttoken = newtoken;
	} //end for
	return newdefine;
failure:
	PC_FreeDefine(newdefine);
	return NULL;
} //end of the function PC_CopyDefine
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
int PC_AddGlobalDefinesToSource(source_t *source)
{
	define_t *define, *newdefine;

	for (define = globaldefines; define; define = define->next)
	{
		newdefine = PC_CopyDefine(source, define);
		if (!newdefine) return qfalse;
#if DEFINEHASHING
		PC_AddDefineToHash(newdefine, source->definehash);
#else //DEFINEHASHING
		newdefine->next = source->defines;
		source->defines = newdefine;
#endif //DEFINEHASHING
	} //end for
	return qtrue;
} //end of the function PC_AddGlobalDefinesToSource
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
int PC_Directive_if_def(source_t *source, int type)
{
	token_t token;
	define_t *d;
	int skip;

	if (!PC_ReadLine(source, &token))
	{
		SourceError(source, "#ifdef without name");
		return qfalse;
	} //end if
	if (token.type != TT_NAME)
	{
		PC_UnreadSourceToken(source, &token);
		SourceError(source, "expected name after #ifdef, found %s", token.string);
		return qfalse;
	} //end if
#if DEFINEHASHING
	d = PC_FindHashedDefine(source->definehash, token.string);
#else
	d = PC_FindDefine(source->defines, token.string);
#endif //DEFINEHASHING
	skip = (type == INDENT_IFDEF) == (d == NULL);
	return PC_PushIndent(source, type, skip);
} //end of the function PC_Directiveif_def
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
int PC_Directive_ifdef(source_t *source)
{
	return PC_Directive_if_def(source, INDENT_IFDEF);
} //end of the function PC_Directive_ifdef
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
int PC_Directive_ifndef(source_t *source)
{
	return PC_Directive_if_def(source, INDENT_IFNDEF);
} //end of the function PC_Directive_ifndef
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
int PC_Directive_else(source_t *source)
{
	indent_t *indent = source->indentstack;

	if (!indent || indent->script != source->scriptstack)
	{
		SourceError(source, "misplaced #else");
		return qfalse;
	} //end if
	if (indent->type == INDENT_ELSE)
	{
		SourceError(source, "#else after #else");
		return qfalse;
	} //end if
	//The same script already owns a complete frame; replacement needs no import.
	source->skip -= indent->skip;
	indent->type = INDENT_ELSE;
	indent->skip = !indent->skip;
	source->skip += indent->skip;
	return qtrue;
} //end of the function PC_Directive_else
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
int PC_Directive_endif(source_t *source)
{
	int type, skip;

	PC_PopIndent(source, &type, &skip);
	if (!type)
	{
		SourceError(source, "misplaced #endif");
		return qfalse;
	} //end if
	return qtrue;
} //end of the function PC_Directive_endif
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
typedef struct operator_s
{
	int operator;
	int priority;
	int parentheses;
	struct operator_s *prev, *next;
} operator_t;

static int PC_EvalFloatFinite(const double *value)
{
	unsigned long long bits;
	volatile unsigned long long representation;
	Com_Memcpy(&bits, value, sizeof(bits));
	representation = bits;
	return (representation & 0x7ff0000000000000ULL) != 0x7ff0000000000000ULL;
}

static int PC_EvalAdd(long a, long b, long *out)
{
	if ((b > 0 && a > LONG_MAX - b) || (b < 0 && a < LONG_MIN - b)) return qfalse;
	*out = a + b;
	return qtrue;
}

static int PC_EvalSubtract(long a, long b, long *out)
{
	if ((b < 0 && a > LONG_MAX + b) || (b > 0 && a < LONG_MIN + b)) return qfalse;
	*out = a - b;
	return qtrue;
}

static int PC_EvalMultiply(long a, long b, long *out)
{
	unsigned long left = (unsigned long)a, right = (unsigned long)b, word, limit;
	int negative = (a < 0) != (b < 0);
	if (a < 0) left = 0UL - left;
	if (b < 0) right = 0UL - right;
	limit = (unsigned long)LONG_MAX + (negative ? 1UL : 0UL);
	if (right && left > limit / right) return qfalse;
	word = left * right;
	if (negative) word = 0UL - word;
	Com_Memcpy(out, &word, sizeof(word));
	return qtrue;
}

static int PC_EvalShift(long value, long count, int right, long *out)
{
	unsigned long word = (unsigned long)value;
	int width = sizeof(word) * CHAR_BIT;
	if (count < 0 || count >= width) return qfalse;
	if (right)
	{
		word >>= count;
		if (value < 0 && count) word |= ~0UL << (width - count);
	} //end if
	else word <<= count;
	Com_Memcpy(out, &word, sizeof(word));
	return qtrue;
}

typedef struct value_s
{
	signed long int intvalue;
	double floatvalue;
	int parentheses;
	struct value_s *prev, *next;
} value_t;

int PC_OperatorPriority(int op)
{
	switch(op)
	{
		case P_MUL: return 15;
		case P_DIV: return 15;
		case P_MOD: return 15;
		case P_ADD: return 14;
		case P_SUB: return 14;

		case P_LOGIC_AND: return 7;
		case P_LOGIC_OR: return 6;
		case P_LOGIC_GEQ: return 12;
		case P_LOGIC_LEQ: return 12;
		case P_LOGIC_EQ: return 11;
		case P_LOGIC_UNEQ: return 11;

		case P_LOGIC_NOT: return 16;
		case P_LOGIC_GREATER: return 12;
		case P_LOGIC_LESS: return 12;

		case P_RSHIFT: return 13;
		case P_LSHIFT: return 13;

		case P_BIN_AND: return 10;
		case P_BIN_OR: return 8;
		case P_BIN_XOR: return 9;
		case P_BIN_NOT: return 16;

		case P_COLON: return 5;
		case P_QUESTIONMARK: return 5;
	} //end switch
	return qfalse;
} //end of the function PC_OperatorPriority

//#define AllocValue()			GetClearedMemory(sizeof(value_t));
//#define FreeValue(val)		FreeMemory(val)
//#define AllocOperator(op)		op = (operator_t *) GetClearedMemory(sizeof(operator_t));
//#define FreeOperator(op)		FreeMemory(op);

#define MAX_VALUES		64
#define MAX_OPERATORS	64
#define AllocValue(val)									\
	if (numvalues >= MAX_VALUES) {						\
		SourceError(source, "out of value space\n");		\
		error = 1;										\
		break;											\
	}													\
	else												\
		val = &value_heap[numvalues++];
#define FreeValue(val)
//
#define AllocOperator(op)								\
	if (numoperators >= MAX_OPERATORS) {				\
		SourceError(source, "out of operator space\n");	\
		error = 1;										\
		break;											\
	}													\
	else												\
		op = &operator_heap[numoperators++];
#define FreeOperator(op)

int PC_EvaluateTokens(source_t *source, token_t *tokens, signed long int *intvalue,
																	double *floatvalue, int integer)
{
	operator_t *o, *firstoperator, *lastoperator;
	value_t *v, *firstvalue, *lastvalue, *v1, *v2;
	token_t *t;
	int brace = 0;
	int parentheses = 0;
	int error = 0;
	int lastwasvalue = 0;
	int negativevalue = 0;
	signed long int questmarkintvalue = 0;
	double questmarkfloatvalue = 0;
	int gotquestmarkvalue = qfalse;
	int lastoperatortype = 0;
	//
	operator_t operator_heap[MAX_OPERATORS];
	int numoperators = 0;
	value_t value_heap[MAX_VALUES];
	int numvalues = 0;

	firstoperator = lastoperator = NULL;
	firstvalue = lastvalue = NULL;
	if (intvalue) *intvalue = 0;
	if (floatvalue) *floatvalue = 0;
	for (t = tokens; t; t = t->next)
	{
		switch(t->type)
		{
			case TT_NAME:
			{
				if (lastwasvalue || negativevalue)
				{
					SourceError(source, "syntax error in #if/#elif");
					error = 1;
					break;
				} //end if
				if (strcmp(t->string, "defined"))
				{
					SourceError(source, "undefined name %s in #if/#elif", t->string);
					error = 1;
					break;
				} //end if
				t = t->next;
				if (!t)
				{
					SourceError(source, "defined without name in #if/#elif");
					error = 1;
					break;
				} //end if
				if (!strcmp(t->string, "("))
				{
					brace = qtrue;
					t = t->next;
				} //end if
				if (!t || t->type != TT_NAME)
				{
					SourceError(source, "defined without name in #if/#elif");
					error = 1;
					break;
				} //end if
				//v = (value_t *) GetClearedMemory(sizeof(value_t));
				AllocValue(v);
#if DEFINEHASHING
				if (PC_FindHashedDefine(source->definehash, t->string))
#else			
				if (PC_FindDefine(source->defines, t->string))
#endif //DEFINEHASHING
				{
					v->intvalue = 1;
					v->floatvalue = 1;
				} //end if
				else
				{
					v->intvalue = 0;
					v->floatvalue = 0;
				} //end else
				v->parentheses = parentheses;
				v->next = NULL;
				v->prev = lastvalue;
				if (lastvalue) lastvalue->next = v;
				else firstvalue = v;
				lastvalue = v;
				if (brace)
				{
					t = t->next;
					if (!t || strcmp(t->string, ")"))
					{
						SourceError(source, "defined without ) in #if/#elif");
						error = 1;
						break;
					} //end if
				} //end if
				brace = qfalse;
				// defined() creates a value
				lastwasvalue = 1;
				break;
			} //end case
			case TT_NUMBER:
			{
				if (lastwasvalue)
				{
					SourceError(source, "syntax error in #if/#elif");
					error = 1;
					break;
				} //end if
				//v = (value_t *) GetClearedMemory(sizeof(value_t));
				AllocValue(v);
				if (integer)
				{
					if (negativevalue)
					{
						unsigned int word = 0u - (unsigned int)t->intvalue;
						signed int native;
						Com_Memcpy(&native, &word, sizeof(word));
						v->intvalue = native;
					} //end if
					else Com_Memcpy(&v->intvalue, &t->intvalue, sizeof(v->intvalue));
				} //end if
				else v->intvalue = 0;
				if (!(t->floatvalue >= -DBL_MAX && t->floatvalue <= DBL_MAX))
				{
					SourceError(source, "expression operand exceeds double range");
					error = 1;
					break;
				} //end if
				v->floatvalue = negativevalue ? -t->floatvalue : t->floatvalue;
				if (!integer && !PC_EvalFloatFinite(&v->floatvalue))
				{
					SourceError(source, "expression operand is not finite");
					error = 1;
					break;
				} //end if
				v->parentheses = parentheses;
				v->next = NULL;
				v->prev = lastvalue;
				if (lastvalue) lastvalue->next = v;
				else firstvalue = v;
				lastvalue = v;
				//last token was a value
				lastwasvalue = 1;
				//
				negativevalue = 0;
				break;
			} //end case
			case TT_PUNCTUATION:
			{
				if (negativevalue)
				{
					SourceError(source, "misplaced minus sign in #if/#elif");
					error = 1;
					break;
				} //end if
				if (t->subtype == P_PARENTHESESOPEN)
				{
					parentheses++;
					break;
				} //end if
				else if (t->subtype == P_PARENTHESESCLOSE)
				{
					parentheses--;
					if (parentheses < 0)
					{
						SourceError(source, "too many ) in #if/#elsif");
						error = 1;
					} //end if
					break;
				} //end else if
				//check for invalid operators on floating point values
				if (!integer)
				{
					if (t->subtype == P_BIN_NOT || t->subtype == P_MOD ||
						t->subtype == P_RSHIFT || t->subtype == P_LSHIFT ||
						t->subtype == P_BIN_AND || t->subtype == P_BIN_OR ||
						t->subtype == P_BIN_XOR)
					{
						SourceError(source, "illigal operator %s on floating point operands\n", t->string);
						error = 1;
						break;
					} //end if
				} //end if
				switch(t->subtype)
				{
					case P_LOGIC_NOT:
					case P_BIN_NOT:
					{
						if (lastwasvalue)
						{
							SourceError(source, "! or ~ after value in #if/#elif");
							error = 1;
							break;
						} //end if
						break;
					} //end case
					case P_INC:
					case P_DEC:
					{
						SourceError(source, "++ or -- used in #if/#elif");
						break;
					} //end case
					case P_SUB:
					{
						if (!lastwasvalue)
						{
							negativevalue = 1;
							break;
						} //end if
					} //end case
					
					case P_MUL:
					case P_DIV:
					case P_MOD:
					case P_ADD:

					case P_LOGIC_AND:
					case P_LOGIC_OR:
					case P_LOGIC_GEQ:
					case P_LOGIC_LEQ:
					case P_LOGIC_EQ:
					case P_LOGIC_UNEQ:

					case P_LOGIC_GREATER:
					case P_LOGIC_LESS:

					case P_RSHIFT:
					case P_LSHIFT:

					case P_BIN_AND:
					case P_BIN_OR:
					case P_BIN_XOR:

					case P_COLON:
					case P_QUESTIONMARK:
					{
						if (!lastwasvalue)
						{
							SourceError(source, "operator %s after operator in #if/#elif", t->string);
							error = 1;
							break;
						} //end if
						break;
					} //end case
					default:
					{
						SourceError(source, "invalid operator %s in #if/#elif", t->string);
						error = 1;
						break;
					} //end default
				} //end switch
				if (!error && !negativevalue)
				{
					//o = (operator_t *) GetClearedMemory(sizeof(operator_t));
					AllocOperator(o);
					o->operator = t->subtype;
					o->priority = PC_OperatorPriority(t->subtype);
					o->parentheses = parentheses;
					o->next = NULL;
					o->prev = lastoperator;
					if (lastoperator) lastoperator->next = o;
					else firstoperator = o;
					lastoperator = o;
					lastwasvalue = 0;
				} //end if
				break;
			} //end case
			default:
			{
				SourceError(source, "unknown %s in #if/#elif", t->string);
				error = 1;
				break;
			} //end default
		} //end switch
		if (error) break;
	} //end for
	if (!error)
	{
		if (!lastwasvalue)
		{
			SourceError(source, "trailing operator in #if/#elif");
			error = 1;
		} //end if
		else if (parentheses)
		{
			SourceError(source, "too many ( in #if/#elif");
			error = 1;
		} //end else if
	} //end if
	//
	gotquestmarkvalue = qfalse;
	questmarkintvalue = 0;
	questmarkfloatvalue = 0;
	//while there are operators
	while(!error && firstoperator)
	{
		v = firstvalue;
		for (o = firstoperator; o->next; o = o->next)
		{
			//if the current operator is nested deeper in parentheses
			//than the next operator
			if (o->parentheses > o->next->parentheses) break;
			//if the current and next operator are nested equally deep in parentheses
			if (o->parentheses == o->next->parentheses)
			{
				//if the priority of the current operator is equal or higher
				//than the priority of the next operator
				if (o->priority >= o->next->priority) break;
			} //end if
			//if the arity of the operator isn't equal to 1
			if (o->operator != P_LOGIC_NOT
					&& o->operator != P_BIN_NOT) v = v->next;
			//if there's no value or no next value
			if (!v)
			{
				SourceError(source, "mising values in #if/#elif");
				error = 1;
				break;
			} //end if
		} //end for
		if (error) break;
		v1 = v;
		v2 = v->next;
#ifdef DEBUG_EVAL
		if (integer)
		{
			Log_Write("operator %s, value1 = %d", PunctuationFromNum(source->scriptstack, o->operator), v1->intvalue);
			if (v2) Log_Write("value2 = %d", v2->intvalue);
		} //end if
		else
		{
			Log_Write("operator %s, value1 = %f", PunctuationFromNum(source->scriptstack, o->operator), v1->floatvalue);
			if (v2) Log_Write("value2 = %f", v2->floatvalue);
		} //end else
#endif //DEBUG_EVAL
		switch(o->operator)
		{
			case P_LOGIC_NOT:		v1->intvalue = !v1->intvalue;
									v1->floatvalue = !v1->floatvalue; break;
			case P_BIN_NOT:			v1->intvalue = ~v1->intvalue;
									break;
			case P_MUL:				if (integer && !PC_EvalMultiply(v1->intvalue, v2->intvalue, &v1->intvalue))
									{
										SourceError(source, "integer expression multiplication overflow");
										error = 1;
										break;
									}
									v1->floatvalue *= v2->floatvalue; break;
			case P_DIV:				if ((integer && !v2->intvalue) || (!integer && !v2->floatvalue))
									{
										SourceError(source, "divide by zero in #if/#elif\n");
										error = 1;
										break;
									}
									if (integer)
									{
										if (v1->intvalue == LONG_MIN && v2->intvalue == -1)
										{
											SourceError(source, "integer expression division overflow");
											error = 1;
											break;
										}
										v1->intvalue /= v2->intvalue;
									}
									v1->floatvalue /= v2->floatvalue; break;
			case P_MOD:				if (!v2->intvalue)
									{
										SourceError(source, "divide by zero in #if/#elif\n");
										error = 1;
										break;
									}
									if (v1->intvalue == LONG_MIN && v2->intvalue == -1)
									{
										SourceError(source, "integer expression remainder overflow");
										error = 1;
										break;
									}
									v1->intvalue %= v2->intvalue; break;
			case P_ADD:				if (integer && !PC_EvalAdd(v1->intvalue, v2->intvalue, &v1->intvalue))
									{
										SourceError(source, "integer expression addition overflow");
										error = 1;
										break;
									}
									v1->floatvalue += v2->floatvalue; break;
			case P_SUB:				if (integer && !PC_EvalSubtract(v1->intvalue, v2->intvalue, &v1->intvalue))
									{
										SourceError(source, "integer expression subtraction overflow");
										error = 1;
										break;
									}
									v1->floatvalue -= v2->floatvalue; break;
			case P_LOGIC_AND:		v1->intvalue = v1->intvalue && v2->intvalue;
									v1->floatvalue = v1->floatvalue && v2->floatvalue; break;
			case P_LOGIC_OR:		v1->intvalue = v1->intvalue || v2->intvalue;
									v1->floatvalue = v1->floatvalue || v2->floatvalue; break;
			case P_LOGIC_GEQ:		v1->intvalue = v1->intvalue >= v2->intvalue;
									v1->floatvalue = v1->floatvalue >= v2->floatvalue; break;
			case P_LOGIC_LEQ:		v1->intvalue = v1->intvalue <= v2->intvalue;
									v1->floatvalue = v1->floatvalue <= v2->floatvalue; break;
			case P_LOGIC_EQ:		v1->intvalue = v1->intvalue == v2->intvalue;
									v1->floatvalue = v1->floatvalue == v2->floatvalue; break;
			case P_LOGIC_UNEQ:		v1->intvalue = v1->intvalue != v2->intvalue;
									v1->floatvalue = v1->floatvalue != v2->floatvalue; break;
			case P_LOGIC_GREATER:	v1->intvalue = v1->intvalue > v2->intvalue;
									v1->floatvalue = v1->floatvalue > v2->floatvalue; break;
			case P_LOGIC_LESS:		v1->intvalue = v1->intvalue < v2->intvalue;
									v1->floatvalue = v1->floatvalue < v2->floatvalue; break;
			case P_RSHIFT:			if (!PC_EvalShift(v1->intvalue, v2->intvalue, qtrue, &v1->intvalue))
									{
										SourceError(source, "invalid expression right shift");
										error = 1;
										break;
									}
									break;
			case P_LSHIFT:			if (!PC_EvalShift(v1->intvalue, v2->intvalue, qfalse, &v1->intvalue))
									{
										SourceError(source, "invalid expression left shift");
										error = 1;
										break;
									}
									break;
			case P_BIN_AND:			v1->intvalue &= v2->intvalue;
									break;
			case P_BIN_OR:			v1->intvalue |= v2->intvalue;
									break;
			case P_BIN_XOR:			v1->intvalue ^= v2->intvalue;
									break;
			case P_COLON:
			{
				if (!gotquestmarkvalue)
				{
					SourceError(source, ": without ? in #if/#elif");
					error = 1;
					break;
				} //end if
				if (integer)
				{
					if (!questmarkintvalue) v1->intvalue = v2->intvalue;
				} //end if
				else
				{
					if (!questmarkfloatvalue) v1->floatvalue = v2->floatvalue;
				} //end else
				gotquestmarkvalue = qfalse;
				break;
			} //end case
			case P_QUESTIONMARK:
			{
				if (gotquestmarkvalue)
				{
					SourceError(source, "? after ? in #if/#elif");
					error = 1;
					break;
				} //end if
				questmarkintvalue = v1->intvalue;
				questmarkfloatvalue = v1->floatvalue;
				gotquestmarkvalue = qtrue;
				break;
			} //end if
		} //end switch
		if (!error && !integer && !PC_EvalFloatFinite(&v1->floatvalue))
		{
			SourceError(source, "floating expression result is not finite");
			error = 1;
		} //end if
#ifdef DEBUG_EVAL
		if (integer) Log_Write("result value = %d", v1->intvalue);
		else Log_Write("result value = %f", v1->floatvalue);
#endif //DEBUG_EVAL
		if (error) break;
		lastoperatortype = o->operator;
		//if not an operator with arity 1
		if (o->operator != P_LOGIC_NOT
				&& o->operator != P_BIN_NOT)
		{
			//remove the second value if not question mark operator
			if (o->operator != P_QUESTIONMARK) v = v->next;
			//
			if (v->prev) v->prev->next = v->next;
			else firstvalue = v->next;
			if (v->next) v->next->prev = v->prev;
			else lastvalue = v->prev;
			//FreeMemory(v);
			FreeValue(v);
		} //end if
		//remove the operator
		if (o->prev) o->prev->next = o->next;
		else firstoperator = o->next;
		if (o->next) o->next->prev = o->prev;
		else lastoperator = o->prev;
		//FreeMemory(o);
		FreeOperator(o);
	} //end while
	if (firstvalue)
	{
		if (intvalue) *intvalue = firstvalue->intvalue;
		if (floatvalue) *floatvalue = firstvalue->floatvalue;
	} //end if
	for (o = firstoperator; o; o = lastoperator)
	{
		lastoperator = o->next;
		//FreeMemory(o);
		FreeOperator(o);
	} //end for
	for (v = firstvalue; v; v = lastvalue)
	{
		lastvalue = v->next;
		//FreeMemory(v);
		FreeValue(v);
	} //end for
	if (!error) return qtrue;
	if (intvalue) *intvalue = 0;
	if (floatvalue) *floatvalue = 0;
	return qfalse;
} //end of the function PC_EvaluateTokens
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
int PC_Evaluate(source_t *source, signed long int *intvalue,
												double *floatvalue, int integer)
{
	token_t token, *firsttoken, *lasttoken;
	token_t *t, *nexttoken;
	define_t *define;
	int defined = qfalse, result = qfalse;
	int workowner = source && !source->tokenworkdepth;
	unsigned int errorsequence = source->errorsequence;

	firsttoken = lasttoken = NULL;
	if (intvalue) *intvalue = 0;
	if (floatvalue) *floatvalue = 0;
	if (workowner && !PC_EnterTokenWork(source)) return qfalse;
	//
	if (!PC_ReadLine(source, &token))
	{
		SourceError(source, "no value after #if/#elif");
		goto cleanup;
	} //end if
	firsttoken = NULL;
	lasttoken = NULL;
	do
	{
		//if the token is a name
		if (token.type == TT_NAME)
		{
			if (defined)
			{
				defined = qfalse;
				t = PC_CopyToken(&token);
				if (!t)
				{
					SourceError(source, "could not copy expression operand");
					goto cleanup;
				} //end if
				t->next = NULL;
				if (lasttoken) lasttoken->next = t;
				else firsttoken = t;
				lasttoken = t;
			} //end if
			else if (!strcmp(token.string, "defined"))
			{
				defined = qtrue;
				t = PC_CopyToken(&token);
				if (!t)
				{
					SourceError(source, "could not copy expression operand");
					goto cleanup;
				} //end if
				t->next = NULL;
				if (lasttoken) lasttoken->next = t;
				else firsttoken = t;
				lasttoken = t;
			} //end if
			else
			{
				//then it must be a define
#if DEFINEHASHING
				define = PC_FindHashedDefine(source->definehash, token.string);
#else
				define = PC_FindDefine(source->defines, token.string);
#endif //DEFINEHASHING
				if (!define)
				{
					SourceError(source, "can't evaluate %s, not defined", token.string);
					goto cleanup;
				} //end if
				if (!PC_ExpandDefineIntoSource(source, &token, define)) goto cleanup;
			} //end else
		} //end if
		//if the token is a number or a punctuation
		else if (token.type == TT_NUMBER || token.type == TT_PUNCTUATION)
		{
			t = PC_CopyToken(&token);
			if (!t)
			{
				SourceError(source, "could not copy expression operand");
				goto cleanup;
			} //end if
			t->next = NULL;
			if (lasttoken) lasttoken->next = t;
			else firsttoken = t;
			lasttoken = t;
		} //end else
		else //can't evaluate the token
		{
			SourceError(source, "can't evaluate %s", token.string);
			goto cleanup;
		} //end else
	} while(PC_ReadLine(source, &token));
	//
	if (PC_SourceErrorFlag(source, SCFL_LEXERROR) ||
			source->errorsequence != errorsequence) goto cleanup;
	result = PC_EvaluateTokens(source, firsttoken, intvalue, floatvalue, integer);
	//
#ifdef DEBUG_EVAL
	Log_Write("eval:");
#endif //DEBUG_EVAL
cleanup:
	for (t = firsttoken; t; t = nexttoken)
	{
#ifdef DEBUG_EVAL
		Log_Write(" %s", t->string);
#endif //DEBUG_EVAL
		nexttoken = t->next;
		PC_FreeToken(t);
	} //end for
#ifdef DEBUG_EVAL
	if (integer) Log_Write("eval result: %d", *intvalue);
	else Log_Write("eval result: %f", *floatvalue);
#endif //DEBUG_EVAL
	//
	if (workowner) PC_LeaveTokenWork(source);
	return result;
} //end of the function PC_Evaluate
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
int PC_DollarEvaluate(source_t *source, signed long int *intvalue,
												double *floatvalue, int integer)
{
	int indent = 0, defined = qfalse, result = qfalse;
	int workowner = source && !source->tokenworkdepth;
	unsigned int errorsequence = source->errorsequence;
	token_t token, *firsttoken, *lasttoken;
	token_t *t, *nexttoken;
	define_t *define;

	firsttoken = lasttoken = NULL;
	if (intvalue) *intvalue = 0;
	if (floatvalue) *floatvalue = 0;
	if (workowner && !PC_EnterTokenWork(source)) return qfalse;
	//
	if (!PC_ReadSourceToken(source, &token))
	{
		SourceError(source, "no leading ( after $evalint/$evalfloat");
		goto cleanup;
	} //end if
	if (token.type != TT_PUNCTUATION || strcmp(token.string, "("))
	{
		SourceError(source, "no leading ( after $evalint/$evalfloat");
		goto cleanup;
	} //end if
	if (!PC_ReadSourceToken(source, &token))
	{
		SourceError(source, "nothing to evaluate");
		goto cleanup;
	} //end if
	indent = 1;
	firsttoken = NULL;
	lasttoken = NULL;
	do
	{
		//if the token is a name
		if (token.type == TT_NAME)
		{
			if (defined)
			{
				defined = qfalse;
				t = PC_CopyToken(&token);
				if (!t)
				{
					SourceError(source, "could not copy expression operand");
					goto cleanup;
				} //end if
				t->next = NULL;
				if (lasttoken) lasttoken->next = t;
				else firsttoken = t;
				lasttoken = t;
			} //end if
			else if (!strcmp(token.string, "defined"))
			{
				defined = qtrue;
				t = PC_CopyToken(&token);
				if (!t)
				{
					SourceError(source, "could not copy expression operand");
					goto cleanup;
				} //end if
				t->next = NULL;
				if (lasttoken) lasttoken->next = t;
				else firsttoken = t;
				lasttoken = t;
			} //end if
			else
			{
				//then it must be a define
#if DEFINEHASHING
				define = PC_FindHashedDefine(source->definehash, token.string);
#else
				define = PC_FindDefine(source->defines, token.string);
#endif //DEFINEHASHING
				if (!define)
				{
					SourceError(source, "can't evaluate %s, not defined", token.string);
					goto cleanup;
				} //end if
				if (!PC_ExpandDefineIntoSource(source, &token, define)) goto cleanup;
			} //end else
		} //end if
		//if the token is a number or a punctuation
		else if (token.type == TT_NUMBER || token.type == TT_PUNCTUATION)
		{
			if (*token.string == '(') indent++;
			else if (*token.string == ')') indent--;
			if (indent <= 0) break;
			t = PC_CopyToken(&token);
			if (!t)
			{
				SourceError(source, "could not copy expression operand");
				goto cleanup;
			} //end if
			t->next = NULL;
			if (lasttoken) lasttoken->next = t;
			else firsttoken = t;
			lasttoken = t;
		} //end else
		else //can't evaluate the token
		{
			SourceError(source, "can't evaluate %s", token.string);
			goto cleanup;
		} //end else
	} while(PC_ReadSourceToken(source, &token));
	if (indent > 0)
	{
		SourceError(source, "missing ) after $evalint/$evalfloat");
		goto cleanup;
	} //end if
	//
	if (PC_SourceErrorFlag(source, SCFL_LEXERROR) ||
			source->errorsequence != errorsequence) goto cleanup;
	result = PC_EvaluateTokens(source, firsttoken, intvalue, floatvalue, integer);
	//
#ifdef DEBUG_EVAL
	Log_Write("$eval:");
#endif //DEBUG_EVAL
cleanup:
	for (t = firsttoken; t; t = nexttoken)
	{
#ifdef DEBUG_EVAL
		Log_Write(" %s", t->string);
#endif //DEBUG_EVAL
		nexttoken = t->next;
		PC_FreeToken(t);
	} //end for
#ifdef DEBUG_EVAL
	if (integer) Log_Write("$eval result: %d", *intvalue);
	else Log_Write("$eval result: %f", *floatvalue);
#endif //DEBUG_EVAL
	//
	if (workowner) PC_LeaveTokenWork(source);
	return result;
} //end of the function PC_DollarEvaluate
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
int PC_Directive_elif(source_t *source)
{
	signed long int value;
	indent_t *indent;
	script_t *script = source->scriptstack;

	indent = source->indentstack;
	if (source->scriptstack != script || !indent ||
			indent->script != source->scriptstack ||
			indent->type == INDENT_ELSE)
	{
		SourceError(source, "misplaced #elif");
		return qfalse;
	} //end if
	//Raw expression reads ignore skip. Keep the frame until evaluation succeeds.
	if (!PC_Evaluate(source, &value, NULL, qtrue)) return qfalse;
	//Evaluation can unwind an exhausted script. Reacquire instead of using a
	//frame that EOF may have freed, and reject a branch without its condition.
	indent = source->indentstack;
	if (source->scriptstack != script || !indent ||
			indent->script != source->scriptstack ||
			indent->type == INDENT_ELSE)
	{
		SourceError(source, "conditional ended while evaluating #elif");
		return qfalse;
	} //end if
	source->skip -= indent->skip;
	indent->type = INDENT_ELIF;
	indent->skip = (value == 0);
	source->skip += indent->skip;
	return qtrue;
} //end of the function PC_Directive_elif
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
int PC_Directive_if(source_t *source)
{
	signed long int value;
	int skip;

	if (!PC_Evaluate(source, &value, NULL, qtrue)) return qfalse;
	skip = (value == 0);
	return PC_PushIndent(source, INDENT_IF, skip);
} //end of the function PC_Directive
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
int PC_Directive_line(source_t *source)
{
	SourceError(source, "#line directive not supported");
	return qfalse;
} //end of the function PC_Directive_line
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
int PC_Directive_error(source_t *source)
{
	token_t token;

	strcpy(token.string, "");
	PC_ReadSourceToken(source, &token);
	SourceError(source, "#error directive: %s", token.string);
	return qfalse;
} //end of the function PC_Directive_error
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
int PC_Directive_pragma(source_t *source)
{
	token_t token;

	SourceWarning(source, "#pragma directive not supported");
	while(PC_ReadLine(source, &token)) ;
	return qtrue;
} //end of the function PC_Directive_pragma
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================

//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
/* Build complete magnitude tokens before making a signed result readable. */
static void PC_InitEvalToken(source_t *source, token_t *token)
{
	Com_Memset(token, 0, sizeof(*token));
	token->line = source->scriptstack->line;
	token->whitespace_p = source->scriptstack->script_p;
	token->endwhitespace_p = source->scriptstack->script_p;
}

static int PC_QueueEvalToken(source_t *source, token_t *token, int negative)
{
	token_t sign, *number, *minus = NULL;
	number = PC_CopyToken(token);
	if (!number)
	{
		SourceError(source, "could not allocate expression result");
		return qfalse;
	} //end if
	if (negative)
	{
		PC_InitEvalToken(source, &sign);
		strcpy(sign.string, "-");
		sign.type = TT_PUNCTUATION;
		sign.subtype = P_SUB;
		minus = PC_CopyToken(&sign);
		if (!minus)
		{
			PC_FreeToken(number);
			SourceError(source, "could not allocate expression sign");
			return qfalse;
		} //end if
	} //end if
	number->next = source->tokens;
	if (minus)
	{
		minus->next = number;
		source->tokens = minus;
	} //end if
	else source->tokens = number;
	return qtrue;
}

static int PC_IntegerEvalToken(source_t *source, signed long value)
{
	token_t token;
	unsigned long magnitude = (unsigned long)value;
	if (value < 0) magnitude = 0UL - magnitude;
	PC_InitEvalToken(source, &token);
	snprintf(token.string, sizeof(token.string), "%lu", magnitude);
	token.type = TT_NUMBER;
	token.subtype = TT_INTEGER|TT_LONG|TT_DECIMAL;
#ifdef NUMBERVALUE
	token.intvalue = magnitude;
	token.floatvalue = magnitude;
#endif //NUMBERVALUE
	return PC_QueueEvalToken(source, &token, value < 0);
}

static int PC_FloatEvalToken(source_t *source, double value)
{
	token_t token;
	double magnitude;
	int length;
	if (!PC_EvalFloatFinite(&value))
	{
		SourceError(source, "expression result is not finite");
		return qfalse;
	} //end if
	magnitude = fabs(value);
	PC_InitEvalToken(source, &token);
	length = snprintf(token.string, sizeof(token.string), "%1.2f", magnitude);
	if (length < 0 || (size_t)length >= sizeof(token.string))
	{
		SourceError(source, "expression result text is too long");
		return qfalse;
	} //end if
	token.type = TT_NUMBER;
	token.subtype = TT_FLOAT|TT_LONG|TT_DECIMAL;
#ifdef NUMBERVALUE
	if ((long double)magnitude >= (long double)ULONG_MAX) token.intvalue = ULONG_MAX;
	else token.intvalue = (unsigned long)magnitude;
	token.floatvalue = magnitude;
#endif //NUMBERVALUE
	return PC_QueueEvalToken(source, &token, value < 0);
}

int PC_Directive_eval(source_t *source)
{
	signed long int value;
	if (!PC_Evaluate(source, &value, NULL, qtrue)) return qfalse;
	return PC_IntegerEvalToken(source, value);
} //end of the function PC_Directive_eval
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
int PC_Directive_evalfloat(source_t *source)
{
	double value;
	if (!PC_Evaluate(source, NULL, &value, qfalse)) return qfalse;
	return PC_FloatEvalToken(source, value);
} //end of the function PC_Directive_evalfloat
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
directive_t directives[20] =
{
	{"if", PC_Directive_if},
	{"ifdef", PC_Directive_ifdef},
	{"ifndef", PC_Directive_ifndef},
	{"elif", PC_Directive_elif},
	{"else", PC_Directive_else},
	{"endif", PC_Directive_endif},
	{"include", PC_Directive_include},
	{"define", PC_Directive_define},
	{"undef", PC_Directive_undef},
	{"line", PC_Directive_line},
	{"error", PC_Directive_error},
	{"pragma", PC_Directive_pragma},
	{"eval", PC_Directive_eval},
	{"evalfloat", PC_Directive_evalfloat},
	{NULL, NULL}
};

int PC_ReadDirective(source_t *source)
{
	token_t token;
	int i;

	//read the directive name
	if (!PC_ReadSourceToken(source, &token))
	{
		SourceError(source, "found # without name");
		return qfalse;
	} //end if
	//directive name must be on the same line
	if (token.linescrossed > 0)
	{
		PC_UnreadSourceToken(source, &token);
		SourceError(source, "found # at end of line");
		return qfalse;
	} //end if
	//if if is a name
	if (token.type == TT_NAME)
	{
		//find the precompiler directive
		for (i = 0; directives[i].name; i++)
		{
			if (!strcmp(directives[i].name, token.string))
			{
				return directives[i].func(source);
			} //end if
		} //end for
	} //end if
	SourceError(source, "unknown precompiler directive %s", token.string);
	return qfalse;
} //end of the function PC_ReadDirective
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
int PC_DollarDirective_evalint(source_t *source)
{
	signed long int value;
	if (!PC_DollarEvaluate(source, &value, NULL, qtrue)) return qfalse;
	return PC_IntegerEvalToken(source, value);
} //end of the function PC_DollarDirective_evalint
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
int PC_DollarDirective_evalfloat(source_t *source)
{
	double value;
	if (!PC_DollarEvaluate(source, NULL, &value, qfalse)) return qfalse;
	return PC_FloatEvalToken(source, value);
} //end of the function PC_DollarDirective_evalfloat
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
directive_t dollardirectives[20] =
{
	{"evalint", PC_DollarDirective_evalint},
	{"evalfloat", PC_DollarDirective_evalfloat},
	{NULL, NULL}
};

int PC_ReadDollarDirective(source_t *source)
{
	token_t token;
	int i;

	//read the directive name
	if (!PC_ReadSourceToken(source, &token))
	{
		SourceError(source, "found $ without name");
		return qfalse;
	} //end if
	//directive name must be on the same line
	if (token.linescrossed > 0)
	{
		PC_UnreadSourceToken(source, &token);
		SourceError(source, "found $ at end of line");
		return qfalse;
	} //end if
	//if if is a name
	if (token.type == TT_NAME)
	{
		//find the precompiler directive
		for (i = 0; dollardirectives[i].name; i++)
		{
			if (!strcmp(dollardirectives[i].name, token.string))
			{
				return dollardirectives[i].func(source);
			} //end if
		} //end for
	} //end if
	PC_UnreadSourceToken(source, &token);
	SourceError(source, "unknown precompiler directive %s", token.string);
	return qfalse;
} //end of the function PC_ReadDirective

#ifdef QUAKEC
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
int BuiltinFunction(source_t *source)
{
	token_t token;

	if (!PC_ReadSourceToken(source, &token)) return qfalse;
	if (token.type == TT_NUMBER)
	{
		PC_UnreadSourceToken(source, &token);
		return qtrue;
	} //end if
	else
	{
		PC_UnreadSourceToken(source, &token);
		return qfalse;
	} //end else
} //end of the function BuiltinFunction
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
int QuakeCMacro(source_t *source)
{
	int i;
	token_t token;

	if (!PC_ReadSourceToken(source, &token)) return qtrue;
	if (token.type != TT_NAME)
	{
		PC_UnreadSourceToken(source, &token);
		return qtrue;
	} //end if
	//find the precompiler directive
	for (i = 0; dollardirectives[i].name; i++)
	{
		if (!strcmp(dollardirectives[i].name, token.string))
		{
			PC_UnreadSourceToken(source, &token);
			return qfalse;
		} //end if
	} //end for
	PC_UnreadSourceToken(source, &token);
	return qtrue;
} //end of the function QuakeCMacro
#endif //QUAKEC
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
static int PC_ReadTokenInternal(source_t *source, token_t *token)
{
	define_t *define;

	while(1)
	{
		if (!PC_ReadSourceToken(source, token)) return qfalse;
		//check for precompiler directives
		if (token->type == TT_PUNCTUATION && *token->string == '#')
		{
#ifdef QUAKEC
			if (!BuiltinFunction(source))
#endif //QUAKC
			{
				//read the precompiler directive
				if (!PC_ReadDirective(source)) return qfalse;
				continue;
			} //end if
		} //end if
		if (token->type == TT_PUNCTUATION && *token->string == '$')
		{
#ifdef QUAKEC
			if (!QuakeCMacro(source))
#endif //QUAKEC
			{
				//read the precompiler directive
				if (!PC_ReadDollarDirective(source)) return qfalse;
				continue;
			} //end if
		} //end if
		// recursively concatenate strings that are behind each other still resolving defines
		if (token->type == TT_STRING)
		{
			token_t newtoken;
			unsigned int errorsequence = source->errorsequence;
			if (PC_ReadToken(source, &newtoken))
			{
				if (newtoken.type == TT_STRING)
				{
					token->string[strlen(token->string)-1] = '\0';
					if (strlen(token->string) + strlen(newtoken.string+1) + 1 >= MAX_TOKEN)
					{
						SourceError(source, "string longer than MAX_TOKEN %d\n", MAX_TOKEN);
						return qfalse;
					}
					strcat(token->string, newtoken.string+1);
				}
				else
				{
					PC_UnreadToken(source, &newtoken);
				}
			}
			else if (PC_SourceErrorFlag(source, SCFL_LEXERROR) ||
					source->errorsequence != errorsequence) return qfalse;
		} //end if
		//if skipping source because of conditional compilation
		if (source->skip) continue;
		//if the token is a name
		if (token->type == TT_NAME)
		{
			//check if the name is a define macro
#if DEFINEHASHING
			define = PC_FindHashedDefine(source->definehash, token->string);
#else
			define = PC_FindDefine(source->defines, token->string);
#endif //DEFINEHASHING
			//if it is a define macro
			if (define)
			{
				//expand the defined macro
				if (!PC_ExpandDefineIntoSource(source, token, define)) return qfalse;
				continue;
			} //end if
		} //end if
		//copy token for unreading
		Com_Memcpy(&source->token, token, sizeof(token_t));
		//found a token
		return qtrue;
	} //end while
} //end of the function PC_ReadTokenInternal

int PC_ReadToken(source_t *source, token_t *token)
{
	int result;
	if (!PC_EnterTokenWork(source)) return qfalse;
	result = PC_ReadTokenInternal(source, token);
	PC_LeaveTokenWork(source);
	return result;
}
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
int PC_ExpectTokenString(source_t *source, char *string)
{
	token_t token;

	if (!PC_ReadToken(source, &token))
	{
		SourceError(source, "couldn't find expected %s", string);
		return qfalse;
	} //end if

	if (strcmp(token.string, string))
	{
		SourceError(source, "expected %s, found %s", string, token.string);
		return qfalse;
	} //end if
	return qtrue;
} //end of the function PC_ExpectTokenString
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
int PC_ExpectTokenType(source_t *source, int type, int subtype, token_t *token)
{
	char str[MAX_TOKEN];

	if (!PC_ReadToken(source, token))
	{
		SourceError(source, "couldn't read expected token");
		return qfalse;
	} //end if

	if (token->type != type)
	{
		strcpy(str, "");
		if (type == TT_STRING) strcpy(str, "string");
		if (type == TT_LITERAL) strcpy(str, "literal");
		if (type == TT_NUMBER) strcpy(str, "number");
		if (type == TT_NAME) strcpy(str, "name");
		if (type == TT_PUNCTUATION) strcpy(str, "punctuation");
		SourceError(source, "expected a %s, found %s", str, token->string);
		return qfalse;
	} //end if
	if (token->type == TT_NUMBER)
	{
		if ((token->subtype & subtype) != subtype)
		{
			if (subtype & TT_DECIMAL) strcpy(str, "decimal");
			if (subtype & TT_HEX) strcpy(str, "hex");
			if (subtype & TT_OCTAL) strcpy(str, "octal");
			if (subtype & TT_BINARY) strcpy(str, "binary");
			if (subtype & TT_LONG) strcat(str, " long");
			if (subtype & TT_UNSIGNED) strcat(str, " unsigned");
			if (subtype & TT_FLOAT) strcat(str, " float");
			if (subtype & TT_INTEGER) strcat(str, " integer");
			SourceError(source, "expected %s, found %s", str, token->string);
			return qfalse;
		} //end if
	} //end if
	else if (token->type == TT_PUNCTUATION)
	{
		if (token->subtype != subtype)
		{
			SourceError(source, "found %s", token->string);
			return qfalse;
		} //end if
	} //end else if
	return qtrue;
} //end of the function PC_ExpectTokenType
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
int PC_ExpectAnyToken(source_t *source, token_t *token)
{
	if (!PC_ReadToken(source, token))
	{
		SourceError(source, "couldn't read expected token");
		return qfalse;
	} //end if
	else
	{
		return qtrue;
	} //end else
} //end of the function PC_ExpectAnyToken
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
int PC_CheckTokenString(source_t *source, char *string)
{
	token_t tok;

	if (!PC_ReadToken(source, &tok)) return qfalse;
	//if the token is available
	if (!strcmp(tok.string, string)) return qtrue;
	//
	PC_UnreadSourceToken(source, &tok);
	return qfalse;
} //end of the function PC_CheckTokenString
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
int PC_CheckTokenType(source_t *source, int type, int subtype, token_t *token)
{
	token_t tok;

	if (!PC_ReadToken(source, &tok)) return qfalse;
	//if the type matches
	if (tok.type == type &&
			(tok.subtype & subtype) == subtype)
	{
		Com_Memcpy(token, &tok, sizeof(token_t));
		return qtrue;
	} //end if
	//
	PC_UnreadSourceToken(source, &tok);
	return qfalse;
} //end of the function PC_CheckTokenType
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
int PC_SkipUntilString(source_t *source, char *string)
{
	token_t token;

	while(PC_ReadToken(source, &token))
	{
		if (!strcmp(token.string, string)) return qtrue;
	} //end while
	return qfalse;
} //end of the function PC_SkipUntilString
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
void PC_UnreadLastToken(source_t *source)
{
	PC_UnreadSourceToken(source, &source->token);
} //end of the function PC_UnreadLastToken
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
void PC_UnreadToken(source_t *source, token_t *token)
{
	PC_UnreadSourceToken(source, token);
} //end of the function PC_UnreadToken
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
void PC_SetIncludePath(source_t *source, char *path)
{
	char copy[sizeof(source->includepath)];
	size_t length;
	copy[0] = '\0';
	if (!PC_AppendString(copy, sizeof(copy), path))
	{
		SourceError(source, "include prefix too long");
		return;
	} //end if
	length = strlen(copy);
	//add trailing path seperator
	if (length && copy[length-1] != '\\' && copy[length-1] != '/')
	{
		if (!PC_AppendString(copy, sizeof(copy), PATHSEPERATOR_STR))
		{
			SourceError(source, "include prefix too long");
			return;
		} //end if
	} //end if
	memcpy(source->includepath, copy, strlen(copy) + 1);
} //end of the function PC_SetIncludePath
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
void PC_SetPunctuations(source_t *source, punctuation_t *p)
{
	source->punctuations = p;
} //end of the function PC_SetPunctuations
//============================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//============================================================================
/* Publish a source only after every required dictionary owner is complete. */
static source_t *PC_CreateSource(script_t *script, const char *name)
{
	source_t *source;
	if (!script) return NULL;

	script->next = NULL;

	source = (source_t *) GetMemory(sizeof(source_t));
	if (!source)
	{
		FreeScript(script);
		return NULL;
	}
	Com_Memset(source, 0, sizeof(source_t));

	memcpy(source->filename, name, strlen(name) + 1);
	source->scriptstack = script;
	source->tokens = NULL;
	source->defines = NULL;
	source->indentstack = NULL;
	source->skip = 0;

#if DEFINEHASHING
	source->definehash = GetClearedMemory(DEFINEHASHSIZE * sizeof(define_t *));
	if (!source->definehash)
	{
		FreeScript(script);
		FreeMemory(source);
		return NULL;
	}
#endif //DEFINEHASHING
	if (!PC_AddGlobalDefinesToSource(source))
	{
		FreeSource(source);
		return NULL;
	}
	return source;
}

source_t *LoadSourceFile(const char *filename)
{
	PC_InitTokenHeap();
	return PC_CreateSource(LoadScriptFile(filename), filename);
} //end of the function LoadSourceFile
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
source_t *LoadSourceMemory(char *ptr, int length, char *name)
{
	PC_InitTokenHeap();
	return PC_CreateSource(LoadScriptMemory(ptr, length, name), name);
} //end of the function LoadSourceMemory
//============================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//============================================================================
void FreeSource(source_t *source)
{
	script_t *script;
	token_t *token;
	define_t *define;
	indent_t *indent;
	int i;

	//PC_PrintDefineHashTable(source->definehash);
	//free all the scripts
	while(source->scriptstack)
	{
		script = source->scriptstack;
		source->scriptstack = source->scriptstack->next;
		FreeScript(script);
	} //end for
	//free all the tokens
	while(source->tokens)
	{
		token = source->tokens;
		source->tokens = source->tokens->next;
		PC_FreeToken(token);
	} //end for
#if DEFINEHASHING
	for (i = 0; i < DEFINEHASHSIZE; i++)
	{
		while(source->definehash[i])
		{
			define = source->definehash[i];
			source->definehash[i] = source->definehash[i]->hashnext;
			PC_FreeDefine(define);
		} //end while
	} //end for
#else //DEFINEHASHING
	//free all defines
	while(source->defines)
	{
		define = source->defines;
		source->defines = source->defines->next;
		PC_FreeDefine(define);
	} //end for
#endif //DEFINEHASHING
	//free all indents
	while(source->indentstack)
	{
		indent = source->indentstack;
		source->indentstack = source->indentstack->next;
		FreeMemory(indent);
	} //end for
#if DEFINEHASHING
	//
	if (source->definehash) FreeMemory(source->definehash);
#endif //DEFINEHASHING
	//free the source itself
	FreeMemory(source);
} //end of the function FreeSource
//============================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//============================================================================

#define MAX_SOURCEFILES		64

source_t *sourceFiles[MAX_SOURCEFILES];

int PC_LoadSourceHandle(const char *filename)
{
	source_t *source;
	int i;

	for (i = 1; i < MAX_SOURCEFILES; i++)
	{
		if (!sourceFiles[i])
			break;
	} //end for
	if (i >= MAX_SOURCEFILES)
		return 0;
	PS_SetBaseFolder("");
	source = LoadSourceFile(filename);
	if (!source)
		return 0;
	sourceFiles[i] = source;
	return i;
} //end of the function PC_LoadSourceHandle
//============================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//============================================================================
int PC_FreeSourceHandle(int handle)
{
	if (handle < 1 || handle >= MAX_SOURCEFILES)
		return qfalse;
	if (!sourceFiles[handle])
		return qfalse;

	FreeSource(sourceFiles[handle]);
	sourceFiles[handle] = NULL;
	return qtrue;
} //end of the function PC_FreeSourceHandle
//============================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//============================================================================
int PC_ReadTokenHandle(int handle, pc_token_t *pc_token)
{
	token_t token;
	int ret;

	if (handle < 1 || handle >= MAX_SOURCEFILES)
		return 0;
	if (!sourceFiles[handle])
		return 0;

	ret = PC_ReadToken(sourceFiles[handle], &token);
	strcpy(pc_token->string, token.string);
	pc_token->type = token.type;
	pc_token->subtype = token.subtype;
	pc_token->intvalue = token.intvalue;
	pc_token->floatvalue = token.floatvalue;
	if (pc_token->type == TT_STRING)
		StripDoubleQuotes(pc_token->string);
	return ret;
} //end of the function PC_ReadTokenHandle
//============================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//============================================================================
int PC_SourceFileAndLine(int handle, char *filename, int *line)
{
	if (handle < 1 || handle >= MAX_SOURCEFILES)
		return qfalse;
	if (!sourceFiles[handle])
		return qfalse;

	// The QVM syscall ABI supplies a MAX_QPATH-byte output.
	Q_strncpyz(filename, sourceFiles[handle]->filename, MAX_QPATH);
	if (sourceFiles[handle]->scriptstack)
		*line = sourceFiles[handle]->scriptstack->line;
	else
		*line = 0;
	return qtrue;
} //end of the function PC_SourceFileAndLine
//============================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//============================================================================
void PC_SetBaseFolder(char *path)
{
	PS_SetBaseFolder(path);
} //end of the function PC_SetBaseFolder
//============================================================================
//
// Parameter:			-
// Returns:				-
// Changes Globals:		-
//============================================================================
void PC_CheckOpenSourceHandles(void)
{
	int i;

	for (i = 1; i < MAX_SOURCEFILES; i++)
	{
		if (sourceFiles[i])
		{
#ifdef BOTLIB
			botimport.Print(PRT_ERROR, "file %s still open in precompiler\n", sourceFiles[i]->scriptstack->filename);
#endif	//BOTLIB
		} //end if
	} //end for
} //end of the function PC_CheckOpenSourceHandles
