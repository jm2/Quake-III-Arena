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
#include "../game/q_shared.h"
#include "../qcommon/qcommon.h"
#include "tr_public.h"
#include "tr_image_cursor.h"

extern refimport_t ri;

/** Decode complete PCX scanlines, including padding, without letting runs cross their ends. */
static qboolean R_PCXRows( imageCursor_t *cursor, unsigned int columns, unsigned int rows,
                          unsigned int bytesPerLine, const byte *palette, byte *output ) {
	unsigned int row, column, run, visible, i;
	const byte *token, *value;
	byte *pixel;
	for ( row = 0; row < rows; row++ ) {
		column = 0;
		while ( column < bytesPerLine ) {
			if ( !R_ImageBytes(cursor, 1, &token) ) return qfalse;
			value = token; run = 1;
			if ( (*token & 0xc0) == 0xc0 ) {
				run = *token & 0x3f;
				if ( !run || !R_ImageBytes(cursor, 1, &value) ) return qfalse;
			}
			if ( run > bytesPerLine - column ) return qfalse;
			if ( output && column < columns ) {
				visible = run < columns - column ? run : columns - column;
				pixel = output + (row * columns + column) * 4;
				for ( i = 0; i < visible; i++, pixel += 4 ) {
					pixel[0] = palette[*value * 3]; pixel[1] = palette[*value * 3 + 1];
					pixel[2] = palette[*value * 3 + 2]; pixel[3] = 255;
				}
			}
			column += run;
		}
	}
	return qtrue;
}

/** Validate the whole 8-bit PCX layout and RLE before allocating the single RGBA output. */
static qboolean R_DecodePCX( const byte *buffer, unsigned int length, byte **pic, int *width, int *height ) {
	imageCursor_t header, encoded, preflight;
	const byte *format, *ignored, *planes, *palette;
	unsigned int xmin, ymin, xmax, ymax, columns, rows, bytesPerLine;
	byte *output;
	/* The standard version-5 palette marker separates compressed bytes from all 256 colors. */
	if ( length < 128 + 769 || buffer[length - 769] != 0x0c ) return qfalse;
	header.data = buffer; header.length = 128; header.position = 0;
	if ( !R_ImageBytes(&header, 4, &format) || format[0] != 0x0a || format[1] != 5 ||
	     format[2] != 1 || format[3] != 8 ||
	     !R_ImageLE(&header, 2, &xmin) || !R_ImageLE(&header, 2, &ymin) ||
	     !R_ImageLE(&header, 2, &xmax) || !R_ImageLE(&header, 2, &ymax) ||
	     !R_ImageBytes(&header, 53, &ignored) || !R_ImageBytes(&header, 1, &planes) ||
	     !R_ImageLE(&header, 2, &bytesPerLine) || !R_ImageBytes(&header, 60, &ignored) ||
	     *planes != 1 || xmax < xmin || ymax < ymin ) return qfalse;
	columns = xmax - xmin + 1; rows = ymax - ymin + 1;
	/* Retain the original 1024-pixel axis limit; the RGBA product is then at most 4 MiB. */
	if ( columns > 1024 || rows > 1024 || bytesPerLine < columns ) return qfalse;
	palette = buffer + length - 768;
	encoded.data = buffer; encoded.length = length - 769; encoded.position = 128;
	preflight = encoded;
	if ( !R_PCXRows(&preflight, columns, rows, bytesPerLine, palette, NULL) ) return qfalse;
	output = ri.Malloc( columns * rows * 4 );
	if ( !output ) return qfalse;
	if ( !R_PCXRows(&encoded, columns, rows, bytesPerLine, palette, output) ) {
		ri.Free(output); return qfalse;
	}
	*pic = output;
	if ( width ) *width = columns;
	if ( height ) *height = rows;
	return qtrue;
}

/** Preserve nonfatal PCX rejection and release the file before warnings or output publication. */
void R_LoadPCX( const char *name, byte **pic, int *width, int *height ) {
	byte *buffer = NULL, *output = NULL;
	int length, columns = 0, rows = 0;
	qboolean valid;
	*pic = NULL;
	if ( width ) *width = 0;
	if ( height ) *height = 0;
	length = ri.FS_ReadFile( name, (void **)&buffer );
	if ( !buffer ) return;
	valid = length >= 0 && R_DecodePCX( buffer, length, &output, &columns, &rows );
	ri.FS_FreeFile( buffer );
	if ( !valid ) { ri.Printf( PRINT_ALL, "Bad, truncated or unsupported PCX file: %s\n", name ); return; }
	*pic = output;
	if ( width ) *width = columns;
	if ( height ) *height = rows;
}
