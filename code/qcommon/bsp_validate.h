/* Shared collision/renderer preflight for retail version-46 BSP file layout. */
#ifndef BSP_VALIDATE_H
#define BSP_VALIDATE_H

#include "../game/q_shared.h"
#include "qfiles.h"
#include <limits.h>
#include <stddef.h>

/** Decode a complete bounded file word without assuming FS input alignment. */
static unsigned int BSP_FileWord(const byte *data) {
	return (unsigned int)data[0] | ((unsigned int)data[1]<<8) |
	       ((unsigned int)data[2]<<16) | ((unsigned int)data[3]<<24);
}

/** Check every lump before checksum, native payload access, allocation or state reset. */
static const char *BSP_ValidateHeader(const void *buffer,int length,dheader_t *header) {
	static const unsigned int strides[HEADER_LUMPS]={
		1,sizeof(dshader_t),sizeof(dplane_t),sizeof(dnode_t),sizeof(dleaf_t),
		4,4,sizeof(dmodel_t),sizeof(dbrush_t),sizeof(dbrushside_t),sizeof(drawVert_t),
		4,sizeof(dfog_t),sizeof(dsurface_t),LIGHTMAP_WIDTH*LIGHTMAP_HEIGHT*3,8,1
	};
	static const unsigned int limits[HEADER_LUMPS]={
		MAX_MAP_ENTSTRING,MAX_MAP_SHADERS,MAX_MAP_PLANES,MAX_MAP_NODES,MAX_MAP_LEAFS,
		MAX_MAP_LEAFFACES,MAX_MAP_LEAFBRUSHES,MAX_MAP_MODELS,MAX_MAP_BRUSHES,MAX_MAP_BRUSHSIDES,
		MAX_MAP_DRAW_VERTS,MAX_MAP_DRAW_INDEXES,MAX_MAP_FOGS,MAX_MAP_DRAW_SURFS,
		MAX_MAP_LIGHTING/(LIGHTMAP_WIDTH*LIGHTMAP_HEIGHT*3),MAX_MAP_LIGHTGRID/8,MAX_MAP_VISIBILITY
	};
	const byte *data=buffer;
	dheader_t validated;
	unsigned int offsets[HEADER_LUMPS],sizes[HEADER_LUMPS],i,j,offset,size,clusters,rowBytes;
	if(!data || !header || length<(int)sizeof(dheader_t) || length>INT_MAX-4096) return "invalid/truncated BSP header";
	if(BSP_FileWord(data+offsetof(dheader_t,ident))!=BSP_IDENT ||
	   BSP_FileWord(data+offsetof(dheader_t,version))!=BSP_VERSION) return "invalid BSP identification/version";
	validated.ident=BSP_IDENT; validated.version=BSP_VERSION;
	for(i=0;i<HEADER_LUMPS;i++) {
		offset=BSP_FileWord(data+offsetof(dheader_t,lumps)+i*sizeof(lump_t)+offsetof(lump_t,fileofs));
		size=BSP_FileWord(data+offsetof(dheader_t,lumps)+i*sizeof(lump_t)+offsetof(lump_t,filelen));
		if(offset>(unsigned int)length || size>(unsigned int)length-offset ||
		   (size && offset<sizeof(dheader_t))) return "BSP lump outside file";
		if(size%strides[i] || size/strides[i]>limits[i]) return "invalid BSP lump size/count";
		/* Text, RGB lightmaps and lightgrid samples use byte loads; other lumps use words. */
		if(size && i!=LUMP_ENTITIES && i!=LUMP_LIGHTMAPS && i!=LUMP_LIGHTGRID && offset%4) return "unaligned BSP lump";
		offsets[i]=offset; sizes[i]=size;
		validated.lumps[i].fileofs=offset; validated.lumps[i].filelen=size;
	}
	for(i=0;i<HEADER_LUMPS;i++) for(j=i+1;j<HEADER_LUMPS;j++) {
		if(sizes[i] && sizes[j] && offsets[i]<offsets[j]+sizes[j] && offsets[j]<offsets[i]+sizes[i]) return "overlapping BSP lumps";
	}
	if(sizes[LUMP_VISIBILITY]) {
		if(sizes[LUMP_VISIBILITY]<8) return "truncated BSP visibility header";
		clusters=BSP_FileWord(data+offsets[LUMP_VISIBILITY]);
		rowBytes=BSP_FileWord(data+offsets[LUMP_VISIBILITY]+4);
		if(clusters>MAX_MAP_LEAFS || rowBytes>MAX_MAP_VISIBILITY ||
		   rowBytes<(clusters+7)/8 || (clusters && rowBytes>(sizes[LUMP_VISIBILITY]-8)/clusters)) return "invalid BSP visibility dimensions";
	}
	*header=validated;
	return NULL;
}

#endif
