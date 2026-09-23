/* Retail BSP payload references shared by collision and rendering. */
#ifndef BSP_REFERENCES_H
#define BSP_REFERENCES_H

#include "bsp_validate.h"
#include <stdlib.h>

/* Counts are unsigned file words; subtraction rejects negative and overflowing spans. */
static qboolean BSP_ReferenceSpan(unsigned int first,unsigned int count,unsigned int total) {
	return first<=total && count<=total-first;
}

static qboolean BSP_ReferenceName(const byte *name) {
	unsigned int i;
	for(i=0;i<MAX_QPATH;i++) if(!name[i]) return qtrue;
	return qfalse;
}

/* A max tree over blocks bounds each overlapping range query by 62 words plus O(log n). */
typedef struct { const byte *data;unsigned int *maxima,leaves; } bspIndexRanges_t;
static const char *BSP_BuildIndexRanges(const byte *data,unsigned int count,bspIndexRanges_t *ranges) {
	unsigned int i,value,leaf;
	ranges->data=data;ranges->leaves=1;
	while(ranges->leaves<(count+31)/32) ranges->leaves*=2;
	/* The validated signed file size bounds count below INT_MAX/4 and this tree at most 128 MiB. */
	ranges->maxima=calloc(ranges->leaves*2,sizeof(*ranges->maxima));
	if(!ranges->maxima) return "BSP index validation allocation failed";
	for(i=0;i<count;i++) {
		value=BSP_FileWord(data+i*4);leaf=ranges->leaves+i/32;
		if(value>ranges->maxima[leaf]) ranges->maxima[leaf]=value;
	}
	for(i=ranges->leaves-1;i;i--) ranges->maxima[i]=ranges->maxima[i*2]>ranges->maxima[i*2+1]?ranges->maxima[i*2]:ranges->maxima[i*2+1];
	return NULL;
}
static unsigned int BSP_IndexRangeMax(const bspIndexRanges_t *ranges,unsigned int first,unsigned int count) {
	unsigned int end=first+count,largest=0,value,left,right;
	while(first<end && first%32) { value=BSP_FileWord(ranges->data+first++*4);if(value>largest)largest=value; }
	while(first<end && end%32) { value=BSP_FileWord(ranges->data+--end*4);if(value>largest)largest=value; }
	left=ranges->leaves+first/32;right=ranges->leaves+end/32;
	while(left<right) {
		if(left&1) { value=ranges->maxima[left++];if(value>largest)largest=value; }
		if(right&1) { value=ranges->maxima[--right];if(value>largest)largest=value; }
		left/=2;right/=2;
	}
	return largest;
}

static const char *BSP_ValidateSurfaceReferences(const byte *base,const dheader_t *h) {
	unsigned int shaders=h->lumps[LUMP_SHADERS].filelen/sizeof(dshader_t);
	unsigned int fogs=h->lumps[LUMP_FOGS].filelen/sizeof(dfog_t);
	unsigned int surfaces=h->lumps[LUMP_SURFACES].filelen/sizeof(dsurface_t);
	unsigned int vertices=h->lumps[LUMP_DRAWVERTS].filelen/sizeof(drawVert_t);
	unsigned int indexes=h->lumps[LUMP_DRAWINDEXES].filelen/4;
	unsigned int i,value,first,count,type;
	qboolean needed=qfalse;
	const byte *record;const char *error;bspIndexRanges_t ranges;
	for(i=0;i<surfaces;i++) {
		record=base+h->lumps[LUMP_SURFACES].fileofs+i*sizeof(dsurface_t);
		value=BSP_FileWord(record+offsetof(dsurface_t,fogNum));type=BSP_FileWord(record+offsetof(dsurface_t,surfaceType));
		/* q3map leaves flare fogNum 0 even without fogs; the renderer treats that as no fog. */
		if(BSP_FileWord(record+offsetof(dsurface_t,shaderNum))>=shaders || (type!=MST_FLARE && value!=0xffffffffu && value>=fogs) ||
		   type<MST_PLANAR || type>MST_FLARE) return "invalid BSP surface material/type";
		/* Flare geometry is held in the surface record; its unused array fields stay ignored. */
		if(type!=MST_FLARE && !BSP_ReferenceSpan(BSP_FileWord(record+offsetof(dsurface_t,firstVert)),BSP_FileWord(record+offsetof(dsurface_t,numVerts)),vertices)) return "invalid BSP surface vertex span";
		if(type==MST_PLANAR || type==MST_TRIANGLE_SOUP) {
			first=BSP_FileWord(record+offsetof(dsurface_t,firstIndex));count=BSP_FileWord(record+offsetof(dsurface_t,numIndexes));
			if(!BSP_ReferenceSpan(first,count,indexes)) return "invalid BSP surface index span";
			if(count) needed=qtrue;
		}
		if(type==MST_PLANAR || type==MST_PATCH) {
			value=BSP_FileWord(record+offsetof(dsurface_t,lightmapNum));
			/* Positive unavailable lightmaps retain R_FindShader's vertex-light fallback. */
			if((value&0x80000000u) && value<0xfffffffcu) return "invalid BSP negative lightmap selector";
		}
	}
	if(!needed) return NULL;
	error=BSP_BuildIndexRanges(base+h->lumps[LUMP_DRAWINDEXES].fileofs,indexes,&ranges);
	if(error) return error;
	for(i=0;i<surfaces;i++) {
		record=base+h->lumps[LUMP_SURFACES].fileofs+i*sizeof(dsurface_t);
		type=BSP_FileWord(record+offsetof(dsurface_t,surfaceType));
		if(type!=MST_PLANAR && type!=MST_TRIANGLE_SOUP) continue;
		first=BSP_FileWord(record+offsetof(dsurface_t,firstIndex));count=BSP_FileWord(record+offsetof(dsurface_t,numIndexes));
		if(count && BSP_IndexRangeMax(&ranges,first,count)>=BSP_FileWord(record+offsetof(dsurface_t,numVerts))) {
			free(ranges.maxima);return "invalid BSP surface local index";
		}
	}
	free(ranges.maxima);return NULL;
}

/** Requires the complete header/layout to have passed BSP_ValidateHeader. */
static const char *BSP_ValidateReferences(const void *buffer,const dheader_t *h) {
	const byte *base=buffer,*record,*brush;
	unsigned int shaders=h->lumps[LUMP_SHADERS].filelen/sizeof(dshader_t);
	unsigned int planes=h->lumps[LUMP_PLANES].filelen/sizeof(dplane_t);
	unsigned int nodes=h->lumps[LUMP_NODES].filelen/sizeof(dnode_t);
	unsigned int leaves=h->lumps[LUMP_LEAFS].filelen/sizeof(dleaf_t);
	unsigned int leafSurfaces=h->lumps[LUMP_LEAFSURFACES].filelen/4;
	unsigned int leafBrushes=h->lumps[LUMP_LEAFBRUSHES].filelen/4;
	unsigned int models=h->lumps[LUMP_MODELS].filelen/sizeof(dmodel_t);
	unsigned int brushes=h->lumps[LUMP_BRUSHES].filelen/sizeof(dbrush_t);
	unsigned int sides=h->lumps[LUMP_BRUSHSIDES].filelen/sizeof(dbrushside_t);
	unsigned int surfaces=h->lumps[LUMP_SURFACES].filelen/sizeof(dsurface_t);
	unsigned int fogs=h->lumps[LUMP_FOGS].filelen/sizeof(dfog_t);
	unsigned int i,j,value,first,count,cluster,visClusters=0;
	if(!shaders || !planes || !nodes || !leaves || !models) return "BSP missing required collision arrays";
	if(h->lumps[LUMP_VISIBILITY].filelen) visClusters=BSP_FileWord(base+h->lumps[LUMP_VISIBILITY].fileofs);
	for(i=0;i<shaders;i++) {
		record=base+h->lumps[LUMP_SHADERS].fileofs+i*sizeof(dshader_t);
		if(!BSP_ReferenceName(record+offsetof(dshader_t,shader))) return "unterminated BSP shader name";
	}
	for(i=0;i<sides;i++) {
		record=base+h->lumps[LUMP_BRUSHSIDES].fileofs+i*sizeof(dbrushside_t);
		if(BSP_FileWord(record+offsetof(dbrushside_t,planeNum))>=planes ||
		   BSP_FileWord(record+offsetof(dbrushside_t,shaderNum))>=shaders) return "invalid BSP brush-side reference";
	}
	for(i=0;i<brushes;i++) {
		record=base+h->lumps[LUMP_BRUSHES].fileofs+i*sizeof(dbrush_t);
		first=BSP_FileWord(record+offsetof(dbrush_t,firstSide));count=BSP_FileWord(record+offsetof(dbrush_t,numSides));
		/* Collision bounds and renderer fog bounds read the six leading axial sides. */
		if(count<6 || !BSP_ReferenceSpan(first,count,sides) ||
		   BSP_FileWord(record+offsetof(dbrush_t,shaderNum))>=shaders) return "invalid BSP brush reference";
	}
	for(i=0;i<nodes;i++) {
		record=base+h->lumps[LUMP_NODES].fileofs+i*sizeof(dnode_t);
		if(BSP_FileWord(record+offsetof(dnode_t,planeNum))>=planes) return "invalid BSP node plane";
		for(j=0;j<2;j++) {
			value=BSP_FileWord(record+offsetof(dnode_t,children)+j*4);
			if(value&0x80000000u) { if((~value)>=leaves) return "invalid BSP node leaf"; }
			else if(value>=nodes) return "invalid BSP node child";
		}
	}
	for(i=0;i<leafSurfaces;i++) if(BSP_FileWord(base+h->lumps[LUMP_LEAFSURFACES].fileofs+i*4)>=surfaces) return "invalid BSP leaf-surface index";
	for(i=0;i<leafBrushes;i++) if(BSP_FileWord(base+h->lumps[LUMP_LEAFBRUSHES].fileofs+i*4)>=brushes) return "invalid BSP leaf-brush index";
	for(i=0;i<leaves;i++) {
		record=base+h->lumps[LUMP_LEAFS].fileofs+i*sizeof(dleaf_t);
		cluster=BSP_FileWord(record+offsetof(dleaf_t,cluster));
		/* Both loaders add one and round derived cluster counts before allocating novis. */
		if(cluster!=0xffffffffu && (cluster>INT_MAX-4096u-64 ||
		   (h->lumps[LUMP_VISIBILITY].filelen && cluster>=visClusters))) return "invalid BSP leaf cluster";
		/* The retail area-visibility ABI has a fixed MAX_MAP_AREA_BYTES bit vector. */
		value=BSP_FileWord(record+offsetof(dleaf_t,area));
		/* Original compilers leave opaque leaves unassigned to an area. */
		if(value>=MAX_MAP_AREA_BYTES*8 && !(value==0xffffffffu && cluster==0xffffffffu)) return "invalid BSP leaf area";
		if(!BSP_ReferenceSpan(BSP_FileWord(record+offsetof(dleaf_t,firstLeafSurface)),BSP_FileWord(record+offsetof(dleaf_t,numLeafSurfaces)),leafSurfaces) ||
		   !BSP_ReferenceSpan(BSP_FileWord(record+offsetof(dleaf_t,firstLeafBrush)),BSP_FileWord(record+offsetof(dleaf_t,numLeafBrushes)),leafBrushes)) return "invalid BSP leaf span";
	}
	for(i=0;i<models;i++) {
		record=base+h->lumps[LUMP_MODELS].fileofs+i*sizeof(dmodel_t);
		if(!BSP_ReferenceSpan(BSP_FileWord(record+offsetof(dmodel_t,firstSurface)),BSP_FileWord(record+offsetof(dmodel_t,numSurfaces)),surfaces) ||
		   !BSP_ReferenceSpan(BSP_FileWord(record+offsetof(dmodel_t,firstBrush)),BSP_FileWord(record+offsetof(dmodel_t,numBrushes)),brushes)) return "invalid BSP model span";
	}
	for(i=0;i<fogs;i++) {
		record=base+h->lumps[LUMP_FOGS].fileofs+i*sizeof(dfog_t);
		value=BSP_FileWord(record+offsetof(dfog_t,brushNum));
		if(!BSP_ReferenceName(record+offsetof(dfog_t,shader)) || value>=brushes) return "invalid BSP fog brush/name";
		brush=base+h->lumps[LUMP_BRUSHES].fileofs+value*sizeof(dbrush_t);
		value=BSP_FileWord(record+offsetof(dfog_t,visibleSide));
		if(value!=0xffffffffu && value>=BSP_FileWord(brush+offsetof(dbrush_t,numSides))) return "invalid BSP fog visible side";
	}
	return BSP_ValidateSurfaceReferences(base,h);
}

#endif
