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

/* Original actual getter bodies at 32e2a8295442205228ae841a204e014c515cf80d.
 * Only function names change; used solely for representable valid comparisons. */
int NativeCharacteristic_Integer(int character, int index)
{
	bot_character_t *ch;

	ch = BotCharacterFromHandle(character);
	if (!ch) return 0;
	//check if the index is in range
	if (!CheckCharacteristicIndex(character, index)) return 0;
	//an integer will just be returned
	if (ch->c[index].type == CT_INTEGER)
	{
		return ch->c[index].value.integer;
	} //end if
	//floats are casted to integers
	else if (ch->c[index].type == CT_FLOAT)
	{
		return (int) ch->c[index].value._float;
	} //end else if
	else
	{
		botimport.Print(PRT_ERROR, "characteristic %d is not a integer\n", index);
		return 0;
	} //end else if
//	return 0;
}
int NativeCharacteristic_BInteger(int character, int index, int min, int max)
{
	int value;
	bot_character_t *ch;

	ch = BotCharacterFromHandle(character);
	if (!ch) return 0;
	if (min > max)
	{
		botimport.Print(PRT_ERROR, "cannot bound characteristic %d between %d and %d\n", index, min, max);
		return 0;
	} //end if
	value = NativeCharacteristic_Integer(character, index);
	if (value < min) return min;
	if (value > max) return max;
	return value;
}
