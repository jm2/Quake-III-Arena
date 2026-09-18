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
#ifndef TR_IMAGE_CURSOR_H
#define TR_IMAGE_CURSOR_H

#include <limits.h>
/* Keep signed allocator header/alignment additions below INT_MAX. */
#define R_IMAGE_MAX_BYTES (INT_MAX - 4096u)

/* Include q_shared.h before this private header. */
typedef struct {
	const byte *data;
	unsigned int length, position;
} imageCursor_t;

/** Obtain a complete byte span without overflow or unaligned native loads. */
static ID_INLINE qboolean R_ImageBytes( imageCursor_t *cursor, unsigned int count, const byte **bytes ) {
	if ( cursor->position > cursor->length || count > cursor->length - cursor->position ) return qfalse;
	*bytes = cursor->data + cursor->position;
	cursor->position += count;
	return qtrue;
}

/** Decode checked two/four-byte little-endian header fields identically on PPC and hosts. */
static ID_INLINE qboolean R_ImageLE( imageCursor_t *cursor, unsigned int count, unsigned int *value ) {
	const byte *bytes;
	unsigned int i, result = 0;
	if ( (count != 2 && count != 4) || !R_ImageBytes(cursor, count, &bytes) ) return qfalse;
	for ( i = 0; i < count; i++ ) result |= (unsigned int)bytes[i] << (i * 8);
	*value = result;
	return qtrue;
}

#endif
