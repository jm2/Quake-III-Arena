/* Native BSP control storage and finite consumed geometry; validated layout/references required. */
#ifndef BSP_GEOMETRY_H
#define BSP_GEOMETRY_H
#include "bsp_references.h"
#include <string.h>

static qboolean BSP_FiniteWords(const byte *data,unsigned int count) {
	unsigned int i;
	for(i=0;i<count;i++) if((BSP_FileWord(data+i*4)&0x7f800000u)==0x7f800000u) return qfalse;
	return qtrue;
}
static float BSP_GeometryFloat(const byte *data) {
	unsigned int word=BSP_FileWord(data);float value;memcpy(&value,&word,sizeof(value));return value;
}
/** Native caller capacities differ: collision grid storage and renderer grid/control storage. */
static const char *BSP_ValidateGeometry(const void *buffer,const dheader_t *h,unsigned int gridDimension,unsigned int controlCapacity) {
	const byte *base=buffer,*record;
	unsigned int i,j,count,type,width,height;
	if(!BSP_FiniteWords(base+h->lumps[LUMP_PLANES].fileofs,h->lumps[LUMP_PLANES].filelen/4)) return "nonfinite BSP plane";
	count=h->lumps[LUMP_MODELS].filelen/sizeof(dmodel_t);
	for(i=0;i<count;i++) {
		record=base+h->lumps[LUMP_MODELS].fileofs+i*sizeof(dmodel_t);
		if(!BSP_FiniteWords(record,6)) return "nonfinite BSP model bounds";
		for(j=0;j<3;j++) if(BSP_GeometryFloat(record+j*4)>BSP_GeometryFloat(record+12+j*4)) return "reversed BSP model bounds";
	}
	/* Each input vertex is checked once; color's arbitrary four bytes are excluded. */
	count=h->lumps[LUMP_DRAWVERTS].filelen/sizeof(drawVert_t);
	for(i=0;i<count;i++) if(!BSP_FiniteWords(base+h->lumps[LUMP_DRAWVERTS].fileofs+i*sizeof(drawVert_t),10)) return "nonfinite BSP vertex";
	count=h->lumps[LUMP_SURFACES].filelen/sizeof(dsurface_t);
	for(i=0;i<count;i++) {
		record=base+h->lumps[LUMP_SURFACES].fileofs+i*sizeof(dsurface_t);type=BSP_FileWord(record+offsetof(dsurface_t,surfaceType));
		if(type==MST_PATCH) {
			width=BSP_FileWord(record+offsetof(dsurface_t,patchWidth));height=BSP_FileWord(record+offsetof(dsurface_t,patchHeight));
			if(width<3 || height<3 || !(width&1) || !(height&1) || width>gridDimension || height>gridDimension ||
			   width>controlCapacity/height || width*height>BSP_FileWord(record+offsetof(dsurface_t,numVerts))) return "invalid BSP patch controls";
			if(!BSP_FiniteWords(record+offsetof(dsurface_t,lightmapVecs),6)) return "nonfinite BSP patch LOD bounds";
		} else if(type==MST_PLANAR) {
			if(!BSP_FiniteWords(record+offsetof(dsurface_t,lightmapVecs)+6*4,3)) return "nonfinite BSP face plane";
		} else if(type==MST_FLARE) {
			if(!BSP_FiniteWords(record+offsetof(dsurface_t,lightmapOrigin),3) ||
			   !BSP_FiniteWords(record+offsetof(dsurface_t,lightmapVecs),3) ||
			   !BSP_FiniteWords(record+offsetof(dsurface_t,lightmapVecs)+6*4,3)) return "nonfinite BSP flare";
		}
	}
	return NULL;
}
#endif
