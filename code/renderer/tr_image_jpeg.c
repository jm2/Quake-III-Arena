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
#define JPEG_INTERNALS
#include "../jpeg-6/jpeglib.h"
#include "../jpeg-6/jerror.h"
#include <setjmp.h>

extern refimport_t ri;

typedef struct {
	struct jpeg_error_mgr pub;
	jmp_buf jump;
	char message[JMSG_LENGTH_MAX];
} rendererJPEGError_t;

typedef struct {
	union { struct jpeg_compress_struct encode; struct jpeg_decompress_struct decode; } info;
	rendererJPEGError_t error;
	qboolean compress;
	byte *input, *pixels, *row, *encoded;
	unsigned int capacity, length;
} rendererJPEG_t;

typedef struct {
	struct jpeg_destination_mgr pub;
	rendererJPEG_t *owner;
} rendererJPEGDest_t;

/** Return parser/library errors to the owning operation before any engine error is raised. */
static void R_JPEGErrorExit( j_common_ptr info ) {
	rendererJPEGError_t *error = (rendererJPEGError_t *)info->err;
	(*error->pub.format_message)(info, error->message);
	longjmp(error->jump, 1);
}

/** Keep recoverable decoder warnings visible without selecting the fatal default handler. */
static void R_JPEGOutputMessage( j_common_ptr info ) {
	char message[JMSG_LENGTH_MAX];
	(*info->err->format_message)(info, message);
	ri.Printf(PRINT_DEVELOPER, "JPEG: %s\n", message);
}

/** Store cleanup state on the heap so libjpeg longjmps never invalidate modified automatic data. */
static rendererJPEG_t *R_JPEGContext( qboolean compress ) {
	rendererJPEG_t *context = ri.Malloc(sizeof(*context));
	if ( !context ) return NULL;
	memset(context, 0, sizeof(*context)); context->compress = compress;
	jpeg_std_error(&context->error.pub);
	context->error.pub.error_exit = R_JPEGErrorExit;
	context->error.pub.output_message = R_JPEGOutputMessage;
	if ( compress ) context->info.encode.err = &context->error.pub;
	else context->info.decode.err = &context->error.pub;
	return context;
}

/** Route validation/allocation failures through the same complete ownership cleanup. */
static void R_JPEGFail( rendererJPEG_t *context, const char *message ) {
	size_t length = strlen(message);
	if ( length >= sizeof(context->error.message) ) length = sizeof(context->error.message) - 1;
	memcpy(context->error.message, message, length); context->error.message[length] = 0;
	longjmp(context->error.jump, 1);
}

/** Destroy partial libjpeg state and release each file, pixel, row and encoded buffer once. */
static void R_JPEGRelease( rendererJPEG_t *context ) {
	if ( context->compress ) jpeg_destroy_compress(&context->info.encode);
	else jpeg_destroy_decompress(&context->info.decode);
	if ( context->pixels ) ri.Free(context->pixels);
	if ( context->row ) ri.Free(context->row);
	if ( context->encoded ) ri.Free(context->encoded);
	if ( context->input ) ri.FS_FreeFile(context->input);
	ri.Free(context);
}

/** Release all ownership before recoverable load errors or nonfatal screenshot failure warnings. */
static void R_JPEGRecover( rendererJPEG_t *context, const char *name ) {
	char message[JMSG_LENGTH_MAX];
	qboolean compress = context->compress;
	memcpy(message, context->error.message, sizeof(message)); message[sizeof(message)-1] = 0;
	R_JPEGRelease(context);
	if ( compress ) ri.Printf(PRINT_WARNING, "SaveJPG: %s (%s)\n", message, name);
	else ri.Error(ERR_DROP, "LoadJPG: %s (%s)", message, name);
}

/** Validate RGBA and row arithmetic while reserving signed native allocator bookkeeping space. */
static qboolean R_JPEGDimensions( unsigned int columns, unsigned int rows ) {
	return columns && rows && columns <= JPEG_MAX_DIMENSION && rows <= JPEG_MAX_DIMENSION &&
	       rows <= (R_IMAGE_MAX_BYTES / 4u) / columns;
}

/** Decode a length-aware source and publish opaque RGBA only after all JPEG operations finish. */
void R_LoadJPG( const char *name, byte **pic, int *width, int *height ) {
	rendererJPEG_t *context;
	struct jpeg_decompress_struct *info;
	byte *input = NULL, *output;
	int length;
	unsigned int columns, rows, components, sourceIndex, destinationIndex;
	byte value;
	JSAMPROW row;
	*pic = NULL;
	if ( width ) *width = 0;
	if ( height ) *height = 0;
	length = ri.FS_ReadFile(name, (void **)&input);
	if ( !input ) return;
	if ( length < 0 ) { ri.FS_FreeFile(input); ri.Error(ERR_DROP, "LoadJPG: invalid file length (%s)", name); return; }
	context = R_JPEGContext(qfalse);
	if ( !context ) { ri.FS_FreeFile(input); ri.Error(ERR_DROP, "LoadJPG: context allocation failed (%s)", name); return; }
	context->input = input;
	if ( setjmp(context->error.jump) ) { R_JPEGRecover(context, name); return; }
	info = &context->info.decode;
	jpeg_create_decompress(info);
	jpeg_mem_src(info, input, length);
	if ( jpeg_read_header(info, TRUE) != JPEG_HEADER_OK ) R_JPEGFail(context, "invalid image header");
	if ( !R_JPEGDimensions(info->image_width, info->image_height) ) R_JPEGFail(context, "invalid image dimensions");
	info->out_color_space = info->jpeg_color_space == JCS_GRAYSCALE ? JCS_GRAYSCALE : JCS_RGB;
	jpeg_calc_output_dimensions(info);
	if ( !R_JPEGDimensions(info->output_width, info->output_height) ) R_JPEGFail(context, "invalid output dimensions");
	components = info->out_color_space == JCS_GRAYSCALE ? 1 : 3;
	if ( !jpeg_start_decompress(info) || info->output_components != components ) R_JPEGFail(context, "unsupported output format");
	columns = info->output_width; rows = info->output_height;
	context->pixels = ri.Malloc(columns * rows * 4);
	if ( !context->pixels ) R_JPEGFail(context, "pixel allocation failed");
	while ( info->output_scanline < rows ) {
		row = context->pixels + columns * components * info->output_scanline;
		if ( jpeg_read_scanlines(info, &row, 1) != 1 ) R_JPEGFail(context, "incomplete image scanline");
	}
	if ( !jpeg_finish_decompress(info) ) R_JPEGFail(context, "incomplete image end");
	/* Expand backwards so the three-byte input and four-byte output may share one allocation. */
	sourceIndex = columns * rows * components; destinationIndex = columns * rows * 4;
	while ( sourceIndex ) {
		context->pixels[--destinationIndex] = 255;
		if ( components == 1 ) {
			value = context->pixels[--sourceIndex];
			context->pixels[--destinationIndex] = value;
			context->pixels[--destinationIndex] = value;
			context->pixels[--destinationIndex] = value;
		} else {
			context->pixels[--destinationIndex] = context->pixels[--sourceIndex];
			context->pixels[--destinationIndex] = context->pixels[--sourceIndex];
			context->pixels[--destinationIndex] = context->pixels[--sourceIndex];
		}
	}
	output = context->pixels; context->pixels = NULL; R_JPEGRelease(context);
	*pic = output;
	if ( width ) *width = columns;
	if ( height ) *height = rows;
}

/** Start compression into separately owned storage, including tiny images larger than raw RGBA. */
static void R_JPEGInitDestination( j_compress_ptr info ) {
	rendererJPEGDest_t *destination = (rendererJPEGDest_t *)info->dest;
	rendererJPEG_t *context = destination->owner;
	context->capacity = 4096; context->encoded = ri.Malloc(context->capacity);
	if ( !context->encoded ) R_JPEGFail(context, "encoded allocation failed");
	destination->pub.next_output_byte = context->encoded;
	destination->pub.free_in_buffer = context->capacity;
}

/** Grow full storage before allowing the compressor to continue, preserving all previously emitted bytes. */
static boolean R_JPEGGrowDestination( j_compress_ptr info ) {
	rendererJPEGDest_t *destination = (rendererJPEGDest_t *)info->dest;
	rendererJPEG_t *context = destination->owner;
	unsigned int capacity;
	byte *encoded;
	if ( context->capacity >= R_IMAGE_MAX_BYTES ) R_JPEGFail(context, "encoded image exceeds allocator capacity");
	capacity = context->capacity > R_IMAGE_MAX_BYTES / 2 ? R_IMAGE_MAX_BYTES : context->capacity * 2;
	encoded = ri.Malloc(capacity);
	if ( !encoded ) R_JPEGFail(context, "encoded growth allocation failed");
	memcpy(encoded, context->encoded, context->capacity);
	destination->pub.next_output_byte = encoded + context->capacity;
	destination->pub.free_in_buffer = capacity - context->capacity;
	ri.Free(context->encoded); context->encoded = encoded; context->capacity = capacity;
	return TRUE;
}

/** Record the actual encoded byte count without global state shared between screenshots. */
static void R_JPEGFinishDestination( j_compress_ptr info ) {
	rendererJPEGDest_t *destination = (rendererJPEGDest_t *)info->dest;
	destination->owner->length = destination->owner->capacity - destination->pub.free_in_buffer;
}

/** Compress bottom-up RGBA as standard RGB and write once after complete, recoverable encoding. */
void SaveJPG( char *name, int quality, int columns, int rows, unsigned char *image ) {
	rendererJPEG_t *context;
	rendererJPEGDest_t *destination;
	struct jpeg_compress_struct *info;
	const byte *source;
	JSAMPROW row;
	int column;
	if ( !image || columns <= 0 || rows <= 0 || !R_JPEGDimensions(columns, rows) ) {
		ri.Printf(PRINT_WARNING, "SaveJPG: invalid input image (%s)\n", name); return;
	}
	context = R_JPEGContext(qtrue);
	if ( !context ) { ri.Printf(PRINT_WARNING, "SaveJPG: context allocation failed (%s)\n", name); return; }
	if ( setjmp(context->error.jump) ) { R_JPEGRecover(context, name); return; }
	info = &context->info.encode; jpeg_create_compress(info);
	destination = (rendererJPEGDest_t *)(*info->mem->alloc_small)((j_common_ptr)info, JPOOL_PERMANENT, sizeof(*destination));
	destination->owner = context;
	destination->pub.init_destination = R_JPEGInitDestination;
	destination->pub.empty_output_buffer = R_JPEGGrowDestination;
	destination->pub.term_destination = R_JPEGFinishDestination;
	info->dest = &destination->pub;
	info->image_width = columns; info->image_height = rows; info->input_components = 3; info->in_color_space = JCS_RGB;
	jpeg_set_defaults(info); jpeg_set_quality(info, quality, TRUE);
	context->row = ri.Malloc(columns * 3);
	if ( !context->row ) R_JPEGFail(context, "RGB row allocation failed");
	jpeg_start_compress(info, TRUE);
	while ( info->next_scanline < (unsigned int)rows ) {
		source = image + ((unsigned int)rows - 1 - info->next_scanline) * (unsigned int)columns * 4;
		for ( column = 0; column < columns; column++ ) memcpy(context->row + column * 3, source + column * 4, 3);
		row = context->row;
		if ( jpeg_write_scanlines(info, &row, 1) != 1 ) R_JPEGFail(context, "incomplete encoded scanline");
	}
	jpeg_finish_compress(info);
	ri.FS_WriteFile(name, context->encoded, context->length);
	R_JPEGRelease(context);
}
