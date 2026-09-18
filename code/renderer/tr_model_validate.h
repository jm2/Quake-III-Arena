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
#ifndef TR_MODEL_VALIDATE_H
#define TR_MODEL_VALIDATE_H

#include "../game/q_shared.h"
#include "../qcommon/qfiles.h"
#include <limits.h>
#include <stddef.h>

typedef struct { unsigned int offset, size; } modelRange_t;

/** Read file words bytewise, including when FS input has no native alignment. */
static unsigned int R_ModelWord( const byte *data ) {
	return (unsigned int)data[0] | ((unsigned int)data[1] << 8) |
	       ((unsigned int)data[2] << 16) | ((unsigned int)data[3] << 24);
}

/** Check a complete array and its native-copy alignment before forming pointers. */
static qboolean R_ModelRange( unsigned int limit, unsigned int headerSize,
                             unsigned int offset, unsigned int count, unsigned int stride,
                             unsigned int alignment, modelRange_t *range ) {
	if ( offset > limit || offset % alignment || count > (limit - offset) / stride ||
	     (count && offset < headerSize) ) return qfalse;
	range->offset = offset; range->size = count * stride;
	return qtrue;
}

/** Independent writable sections must not alias fields that native conversion changes. */
static qboolean R_ModelDisjoint( const modelRange_t *ranges, unsigned int count ) {
	unsigned int i, j;
	for ( i = 0; i < count; i++ ) for ( j = i + 1; j < count; j++ ) {
		if ( ranges[i].size && ranges[j].size && ranges[i].offset < ranges[j].offset + ranges[j].size &&
		     ranges[j].offset < ranges[i].offset + ranges[i].size ) return qfalse;
	}
	return qtrue;
}

/** Reject nonfinite serialized floats without unaligned native loads or math-library assumptions. */
static qboolean R_ModelFloats( const byte *data, unsigned int count ) {
	unsigned int i;
	for ( i = 0; i < count; i++ ) if ( (R_ModelWord(data + i * 4) & 0x7f800000u) == 0x7f800000u ) return qfalse;
	return qtrue;
}

/** Decode an already bounded finite float for frame radius/bounds comparisons. */
static float R_ModelFloat( const byte *data ) {
	unsigned int word = R_ModelWord(data); float value;
	memcpy(&value, &word, sizeof(value)); return value;
}

/** Validate a model's culling metadata before converting any frame in place. */
static qboolean R_ModelFrame( const byte *data ) {
	unsigned int axis;
	if ( !R_ModelFloats(data, 10) || R_ModelFloat(data + 36) < 0 ) return qfalse;
	for ( axis = 0; axis < 3; axis++ ) if ( R_ModelFloat(data + axis * 4) > R_ModelFloat(data + 12 + axis * 4) ) return qfalse;
	return qtrue;
}

#define R_MODEL_FIELD(data,type,field) R_ModelWord((data) + offsetof(type,field))

/** Validate all MD3 file/surface layouts, strings and indexes before hunk allocation or swapping. */
static const char *R_ValidateMD3( const void *buffer, int length, int *validatedSize ) {
	const byte *data = buffer, *surface, *entry;
	unsigned int size, frames, tags, surfaces, surfaceOffset, surfaceSize, vertices, triangles, shaders;
	unsigned int i, j, k, surfaceIndex;
	modelRange_t fileRanges[3], ranges[4];
	if ( !data || length < (int)sizeof(md3Header_t) ) return "truncated header";
	if ( R_MODEL_FIELD(data,md3Header_t,ident) != MD3_IDENT || R_MODEL_FIELD(data,md3Header_t,version) != MD3_VERSION ) return "invalid identification/version";
	size = R_MODEL_FIELD(data,md3Header_t,ofsEnd);
	frames = R_MODEL_FIELD(data,md3Header_t,numFrames);
	tags = R_MODEL_FIELD(data,md3Header_t,numTags);
	surfaces = R_MODEL_FIELD(data,md3Header_t,numSurfaces);
	if ( size < sizeof(md3Header_t) || size > (unsigned int)length || size > INT_MAX - 4096u ||
	     !frames || frames > MD3_MAX_FRAMES || tags > MD3_MAX_TAGS || surfaces > MD3_MAX_SURFACES ||
	     R_MODEL_FIELD(data,md3Header_t,numSkins) > INT_MAX ) return "invalid size/counts";
	if ( !R_ModelRange(size, sizeof(md3Header_t), R_MODEL_FIELD(data,md3Header_t,ofsFrames), frames, sizeof(md3Frame_t), 4, &fileRanges[0]) ||
	     !R_ModelRange(size, sizeof(md3Header_t), R_MODEL_FIELD(data,md3Header_t,ofsTags), frames * tags, sizeof(md3Tag_t), 4, &fileRanges[1]) ) return "invalid frame/tag span";
	for ( i = 0; i < frames; i++ ) if ( !R_ModelFrame(data + fileRanges[0].offset + i * sizeof(md3Frame_t)) ) return "invalid frame bounds";
	for ( i = 0; i < frames * tags; i++ ) {
		entry = data + fileRanges[1].offset + i * sizeof(md3Tag_t);
		if ( !memchr(entry, 0, MAX_QPATH) || !R_ModelFloats(entry + offsetof(md3Tag_t,origin), 12) ) return "invalid tag";
	}
	surfaceOffset = R_MODEL_FIELD(data,md3Header_t,ofsSurfaces);
	if ( !R_ModelRange(size, sizeof(md3Header_t), surfaceOffset, surfaces, sizeof(md3Surface_t), 4, &fileRanges[2]) ) return "invalid surface span";
	for ( surfaceIndex = 0; surfaceIndex < surfaces; surfaceIndex++ ) {
		if ( sizeof(md3Surface_t) > size - surfaceOffset ) return "truncated surface";
		surface = data + surfaceOffset;
		surfaceSize = R_MODEL_FIELD(surface,md3Surface_t,ofsEnd);
		vertices = R_MODEL_FIELD(surface,md3Surface_t,numVerts);
		triangles = R_MODEL_FIELD(surface,md3Surface_t,numTriangles);
		shaders = R_MODEL_FIELD(surface,md3Surface_t,numShaders);
		if ( R_MODEL_FIELD(surface,md3Surface_t,ident) != MD3_IDENT ||
		     R_MODEL_FIELD(surface,md3Surface_t,numFrames) != frames ||
		     surfaceSize < sizeof(md3Surface_t) || surfaceSize > size - surfaceOffset || surfaceSize % 4 ||
		     vertices >= SHADER_MAX_VERTEXES || triangles >= SHADER_MAX_INDEXES / 3 || shaders > MD3_MAX_SHADERS ||
		     !memchr(surface + offsetof(md3Surface_t,name), 0, MAX_QPATH) ) return "invalid surface header/counts";
		if ( !R_ModelRange(surfaceSize,sizeof(md3Surface_t),R_MODEL_FIELD(surface,md3Surface_t,ofsTriangles),triangles,sizeof(md3Triangle_t),4,&ranges[0]) ||
		     !R_ModelRange(surfaceSize,sizeof(md3Surface_t),R_MODEL_FIELD(surface,md3Surface_t,ofsShaders),shaders,sizeof(md3Shader_t),4,&ranges[1]) ||
		     !R_ModelRange(surfaceSize,sizeof(md3Surface_t),R_MODEL_FIELD(surface,md3Surface_t,ofsSt),vertices,sizeof(md3St_t),4,&ranges[2]) ||
		     !R_ModelRange(surfaceSize,sizeof(md3Surface_t),R_MODEL_FIELD(surface,md3Surface_t,ofsXyzNormals),vertices * frames,sizeof(md3XyzNormal_t),2,&ranges[3]) ||
		     !R_ModelDisjoint(ranges,4) ) return "invalid/overlapping surface arrays";
		for ( j = 0; j < triangles; j++ ) for ( k = 0; k < 3; k++ ) {
			if ( R_ModelWord(surface + ranges[0].offset + j * sizeof(md3Triangle_t) + k * 4) >= vertices ) return "invalid triangle index";
		}
		for ( j = 0; j < shaders; j++ ) {
			if ( !memchr(surface + ranges[1].offset + j * sizeof(md3Shader_t),0,MAX_QPATH) ) return "unterminated shader name";
		}
		if ( !R_ModelFloats(surface + ranges[2].offset,vertices * 2) ) return "invalid texture coordinates";
		surfaceOffset += surfaceSize;
	}
	fileRanges[2].size = surfaceOffset - fileRanges[2].offset;
	if ( !R_ModelDisjoint(fileRanges,3) ) return "overlapping file sections";
	*validatedSize = size;
	return NULL;
}

#endif
