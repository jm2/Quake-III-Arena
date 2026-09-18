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
 * name:		l_libvar.c
 *
 * desc:		bot library variables
 *
 * $Archive: /MissionPack/code/botlib/l_libvar.c $
 *
 *****************************************************************************/

#include "../game/q_shared.h"
#include <float.h>
#include "l_memory.h"
#include "l_libvar.h"

//list with library variables
libvar_t *libvarlist;

//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
float LibVarStringValue(char *string)
{
	int dotfound = 0;
	float value = 0;
	double divisor = 10.0, next;

	while (*string)
	{
		if (*string >= '0' && *string <= '9')
		{
			if (dotfound)
			{
				value += (float)((double)(*string - '0') / divisor);
				if (divisor <= DBL_MAX / 10.0) divisor *= 10.0;
				else divisor = DBL_MAX;
			}
			else
			{
				next = value * 10.0 + (float)(*string - '0');
				if (next > FLT_MAX) return 0;
				value = (float)next;
			}
		}
		else if (!dotfound && *string == '.') dotfound = 1;
		else return 0;
		string++;
	}
	return value;
} //end of the function LibVarStringValue
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
static char *LibVarCopyString(const char *value)
{
	size_t length;
	char *copy;
	if (!value) return NULL;
	length = strlen(value);
	if (length > (size_t)INT_MAX - 1) return NULL;
	copy = (char *)GetMemory(length + 1);
	if (copy) memcpy(copy, value, length + 1);
	return copy;
}

libvar_t *LibVarAlloc(char *var_name)
{
	libvar_t *v;
	size_t length;

	if (!var_name) return NULL;
	length = strlen(var_name);
	if (length > (size_t)INT_MAX - sizeof(libvar_t) - 1) return NULL;
	v = (libvar_t *)GetMemory(sizeof(libvar_t) + length + 1);
	if (!v) return NULL;
	Com_Memset(v, 0, sizeof(libvar_t));
	v->name = (char *) v + sizeof(libvar_t);
	strcpy(v->name, var_name);
	//add the variable in the list
	v->next = libvarlist;
	libvarlist = v;
	return v;
} //end of the function LibVarAlloc
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void LibVarDeAlloc(libvar_t *v)
{
	if (v->string) FreeMemory(v->string);
	FreeMemory(v);
} //end of the function LibVarDeAlloc
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void LibVarDeAllocAll(void)
{
	libvar_t *v;

	for (v = libvarlist; v; v = libvarlist)
	{
		libvarlist = libvarlist->next;
		LibVarDeAlloc(v);
	} //end for
	libvarlist = NULL;
} //end of the function LibVarDeAllocAll
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
libvar_t *LibVarGet(char *var_name)
{
	libvar_t *v;
	if (!var_name) return NULL;

	for (v = libvarlist; v; v = v->next)
	{
		if (!Q_stricmp(v->name, var_name))
		{
			return v;
		} //end if
	} //end for
	return NULL;
} //end of the function LibVarGet
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
char *LibVarGetString(char *var_name)
{
	libvar_t *v;

	v = LibVarGet(var_name);
	if (v)
	{
		return v->string ? v->string : "";
	} //end if
	else
	{
		return "";
	} //end else
} //end of the function LibVarGetString
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
float LibVarGetValue(char *var_name)
{
	libvar_t *v;

	v = LibVarGet(var_name);
	if (v)
	{
		return v->value;
	} //end if
	else
	{
		return 0;
	} //end else
} //end of the function LibVarGetValue
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
libvar_t *LibVar(char *var_name, char *value)
{
	libvar_t *v;
	char *copy;
	if (!var_name) return NULL;
	v = LibVarGet(var_name);
	if (v) return v;
	// Complete both owners before a new variable can become visible.
	copy = LibVarCopyString(value);
	if (!copy) return NULL;
	v = LibVarAlloc(var_name);
	if (!v) { FreeMemory(copy); return NULL; }
	v->string = copy;
	v->value = LibVarStringValue(copy);
	v->modified = qtrue;
	return v;
} //end of the function LibVar
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
char *LibVarString(char *var_name, char *value)
{
	libvar_t *v;

	v = LibVar(var_name, value);
	return v && v->string ? v->string : "";
} //end of the function LibVarString
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
float LibVarValue(char *var_name, char *value)
{
	libvar_t *v;

	v = LibVar(var_name, value);
	return v ? v->value : 0;
} //end of the function LibVarValue
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void LibVarSet(char *var_name, char *value)
{
	libvar_t *v;
	char *copy;
	if (!var_name) return;
	v = LibVarGet(var_name);
	// Inputs may point into the existing value; clone before its release.
	copy = LibVarCopyString(value);
	if (!copy) return;
	if (!v)
	{
		v = LibVarAlloc(var_name);
		if (!v) { FreeMemory(copy); return; }
	}
	else if (v->string) FreeMemory(v->string);
	v->string = copy;
	v->value = LibVarStringValue(copy);
	v->modified = qtrue;
} //end of the function LibVarSet
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
qboolean LibVarChanged(char *var_name)
{
	libvar_t *v;

	v = LibVarGet(var_name);
	if (v)
	{
		return v->modified;
	} //end if
	else
	{
		return qfalse;
	} //end else
} //end of the function LibVarChanged
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void LibVarSetNotModified(char *var_name)
{
	libvar_t *v;

	v = LibVarGet(var_name);
	if (v)
	{
		v->modified = qfalse;
	} //end if
} //end of the function LibVarSetNotModified
