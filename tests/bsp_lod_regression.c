/* Issue #45: native patch LOD propagation with a bounded trusted stock oracle. */
#define __QGL_H__
typedef unsigned int GLuint;
#define GL_CLAMP 0x2900
#include "../code/renderer/tr_bsp.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
trGlobals_t tr;
glconfig_t glConfig;
refimport_t ri;
static msurface_t *surfaces;
static int count,nativeWalk,stockTouches;
static void Check(int condition,const char *message) { if(!condition) { fprintf(stderr,"BSP LOD regression failed: %s\n",message);exit(1); } }
void *__real_malloc(size_t size);
void *__real_calloc(size_t count,size_t size);
void __real_free(void *pointer);
void *__wrap_malloc(size_t size) { Check(!nativeWalk,"native LOD traversal allocates no heap");return __real_malloc(size); }
void *__wrap_calloc(size_t count,size_t size) { Check(!nativeWalk,"native LOD traversal allocates no heap");return __real_calloc(count,size); }
void __wrap_free(void *pointer) { Check(!nativeWalk,"native LOD traversal releases no heap");__real_free(pointer); }
static void Walk(void) { nativeWalk=1;R_FixSharedVertexLodError();nativeWalk=0; }
static void FreeGrids(void) { int i;for(i=0;i<count;i++) { srfGridMesh_t *grid=(void *)surfaces[i].data;free(grid->widthLodError);free(grid->heightLodError);free(grid); }free(surfaces);surfaces=NULL;count=0; }
static void Chain(int length) {
	int i,j;FreeGrids();surfaces=calloc(length,sizeof(*surfaces));Check(length==0 || surfaces!=NULL,"fixture surface array");count=length;
	for(i=0;i<count;i++) {
		srfGridMesh_t *grid=calloc(1,offsetof(srfGridMesh_t,verts)+6*sizeof(drawVert_t));Check(grid!=NULL,"native variable grid");surfaces[i].data=(void *)grid;
		grid->surfaceType=SF_GRID;grid->width=3;grid->height=2;grid->lodRadius=1;
		grid->widthLodError=calloc(3,sizeof(float));grid->heightLodError=calloc(2,sizeof(float));Check(grid->widthLodError && grid->heightLodError,"native LOD arrays");
		grid->widthLodError[1]=i+1;
		for(j=0;j<6;j++) { grid->verts[j].xyz[0]=i+(j>=3);grid->verts[j].xyz[1]=j%3==1?0:100+j; }
	}
	s_worldData.numsurfaces=count;s_worldData.surfaces=surfaces;
}

/* Use this unchanged recursion only on tiny trusted fixtures. */
void LegacyLodError( int start, srfGridMesh_t *grid1 ) {
	int j, k, l, m, n, offset1, offset2, touch;
	srfGridMesh_t *grid2;

	for ( j = start; j < s_worldData.numsurfaces; j++ ) {
		//
		grid2 = (srfGridMesh_t *) s_worldData.surfaces[j].data;
		// if this surface is not a grid
		if ( grid2->surfaceType != SF_GRID ) continue;
		// if the LOD errors are already fixed for this patch
		if ( grid2->lodFixed == 2 ) continue;
		// grids in the same LOD group should have the exact same lod radius
		if ( grid1->lodRadius != grid2->lodRadius ) continue;
		// grids in the same LOD group should have the exact same lod origin
		if ( grid1->lodOrigin[0] != grid2->lodOrigin[0] ) continue;
		if ( grid1->lodOrigin[1] != grid2->lodOrigin[1] ) continue;
		if ( grid1->lodOrigin[2] != grid2->lodOrigin[2] ) continue;
		//
		touch = qfalse;
		for (n = 0; n < 2; n++) {
			//
			if (n) offset1 = (grid1->height-1) * grid1->width;
			else offset1 = 0;
			if (R_MergedWidthPoints(grid1, offset1)) continue;
			for (k = 1; k < grid1->width-1; k++) {
				for (m = 0; m < 2; m++) {

					if (m) offset2 = (grid2->height-1) * grid2->width;
					else offset2 = 0;
					if (R_MergedWidthPoints(grid2, offset2)) continue;
					for ( l = 1; l < grid2->width-1; l++) {
					//
						if ( fabs(grid1->verts[k + offset1].xyz[0] - grid2->verts[l + offset2].xyz[0]) > .1) continue;
						if ( fabs(grid1->verts[k + offset1].xyz[1] - grid2->verts[l + offset2].xyz[1]) > .1) continue;
						if ( fabs(grid1->verts[k + offset1].xyz[2] - grid2->verts[l + offset2].xyz[2]) > .1) continue;
						// ok the points are equal and should have the same lod error
						grid2->widthLodError[l] = grid1->widthLodError[k];
						touch = qtrue;
					}
				}
				for (m = 0; m < 2; m++) {

					if (m) offset2 = grid2->width-1;
					else offset2 = 0;
					if (R_MergedHeightPoints(grid2, offset2)) continue;
					for ( l = 1; l < grid2->height-1; l++) {
					//
						if ( fabs(grid1->verts[k + offset1].xyz[0] - grid2->verts[grid2->width * l + offset2].xyz[0]) > .1) continue;
						if ( fabs(grid1->verts[k + offset1].xyz[1] - grid2->verts[grid2->width * l + offset2].xyz[1]) > .1) continue;
						if ( fabs(grid1->verts[k + offset1].xyz[2] - grid2->verts[grid2->width * l + offset2].xyz[2]) > .1) continue;
						// ok the points are equal and should have the same lod error
						grid2->heightLodError[l] = grid1->widthLodError[k];
						touch = qtrue;
					}
				}
			}
		}
		for (n = 0; n < 2; n++) {
			//
			if (n) offset1 = grid1->width-1;
			else offset1 = 0;
			if (R_MergedHeightPoints(grid1, offset1)) continue;
			for (k = 1; k < grid1->height-1; k++) {
				for (m = 0; m < 2; m++) {

					if (m) offset2 = (grid2->height-1) * grid2->width;
					else offset2 = 0;
					if (R_MergedWidthPoints(grid2, offset2)) continue;
					for ( l = 1; l < grid2->width-1; l++) {
					//
						if ( fabs(grid1->verts[grid1->width * k + offset1].xyz[0] - grid2->verts[l + offset2].xyz[0]) > .1) continue;
						if ( fabs(grid1->verts[grid1->width * k + offset1].xyz[1] - grid2->verts[l + offset2].xyz[1]) > .1) continue;
						if ( fabs(grid1->verts[grid1->width * k + offset1].xyz[2] - grid2->verts[l + offset2].xyz[2]) > .1) continue;
						// ok the points are equal and should have the same lod error
						grid2->widthLodError[l] = grid1->heightLodError[k];
						touch = qtrue;
					}
				}
				for (m = 0; m < 2; m++) {

					if (m) offset2 = grid2->width-1;
					else offset2 = 0;
					if (R_MergedHeightPoints(grid2, offset2)) continue;
					for ( l = 1; l < grid2->height-1; l++) {
					//
						if ( fabs(grid1->verts[grid1->width * k + offset1].xyz[0] - grid2->verts[grid2->width * l + offset2].xyz[0]) > .1) continue;
						if ( fabs(grid1->verts[grid1->width * k + offset1].xyz[1] - grid2->verts[grid2->width * l + offset2].xyz[1]) > .1) continue;
						if ( fabs(grid1->verts[grid1->width * k + offset1].xyz[2] - grid2->verts[grid2->width * l + offset2].xyz[2]) > .1) continue;
						// ok the points are equal and should have the same lod error
						grid2->heightLodError[l] = grid1->heightLodError[k];
						touch = qtrue;
					}
				}
			}
		}
		if (touch) {
			stockTouches++;
			grid2->lodFixed = 2;
			LegacyLodError ( start, grid2 );
			//NOTE: this would be correct but makes things really slow
			//grid2->lodFixed = 1;
		}
	}
}

typedef struct { byte *grid;float width[5],height[5];size_t size; } snapshot_t;
static unsigned int Random(unsigned int *seed) { *seed=*seed*1664525u+1013904223u;return *seed; }
static void Meshes(unsigned int seed) {
	int i,j,k;FreeGrids();count=7;surfaces=calloc(count,sizeof(*surfaces));Check(surfaces!=NULL,"tiny native surface array");
	for(i=0;i<count;i++) {
		int width=Random(&seed)&1?3:5,height=Random(&seed)&1?3:5;
		srfGridMesh_t *grid=calloc(1,offsetof(srfGridMesh_t,verts)+width*height*sizeof(drawVert_t));Check(grid!=NULL,"tiny variable grid");surfaces[i].data=(void *)grid;
		grid->surfaceType=Random(&seed)%9==0?SF_FACE:SF_GRID;grid->width=width;grid->height=height;grid->lodFixed=Random(&seed)%5<3?0:Random(&seed)%3;
		grid->lodRadius=Random(&seed)%2+1;grid->lodOrigin[0]=Random(&seed)%2;grid->dlightBits[0]=Random(&seed);
		grid->widthLodError=calloc(width,sizeof(float));grid->heightLodError=calloc(height,sizeof(float));Check(grid->widthLodError && grid->heightLodError,"tiny native LOD arrays");
		for(j=0;j<width;j++)grid->widthLodError[j]=i*16+j+1;
		for(j=0;j<height;j++)grid->heightLodError[j]=i*32+j+1;
		for(j=0;j<width*height;j++) { for(k=0;k<2;k++)grid->verts[j].xyz[k]=(Random(&seed)%4)+(Random(&seed)%3)*0.05f;grid->verts[j].color[0]=i+j;grid->verts[j].normal[2]=1;grid->verts[j].st[0]=j; }
		if(i%3==0) { VectorCopy(grid->verts[1].xyz,grid->verts[2].xyz);VectorCopy(grid->verts[width].xyz,grid->verts[width*2].xyz); }
	}
	s_worldData.numsurfaces=count;s_worldData.surfaces=surfaces;
}
static void Snap(snapshot_t *images,int restore) {
	int i;for(i=0;i<count;i++) {
		srfGridMesh_t *grid=(void *)surfaces[i].data;size_t size=offsetof(srfGridMesh_t,verts)+grid->width*grid->height*sizeof(drawVert_t);
		if(restore) { memcpy(grid,images[i].grid,images[i].size);memcpy(grid->widthLodError,images[i].width,grid->width*sizeof(float));memcpy(grid->heightLodError,images[i].height,grid->height*sizeof(float)); }
		else { images[i].grid=malloc(size);Check(images[i].grid!=NULL,"oracle snapshot");images[i].size=size;memcpy(images[i].grid,grid,size);memcpy(images[i].width,grid->widthLodError,grid->width*sizeof(float));memcpy(images[i].height,grid->heightLodError,grid->height*sizeof(float)); }
	}
}
static void Oracle(void) {
	snapshot_t before[7],after[7];int i;Check(count<=7,"stock recursion restricted to tiny trusted cases");Snap(before,0);Walk();Snap(after,0);Snap(before,1);
	for(i=0;i<count;i++) { srfGridMesh_t *grid=(void *)surfaces[i].data;if(grid->surfaceType!=SF_GRID || grid->lodFixed)continue;grid->lodFixed=2;LegacyLodError(i+1,grid); }
	for(i=0;i<count;i++) {
		srfGridMesh_t *grid=(void *)surfaces[i].data;
		Check(!memcmp(grid,after[i].grid,after[i].size) && !memcmp(grid->widthLodError,after[i].width,grid->width*sizeof(float)) && !memcmp(grid->heightLodError,after[i].height,grid->height*sizeof(float)),"all native vertices/header/LOD arrays exactly match stock propagation");
		free(before[i].grid);free(after[i].grid);
	}
}
static void ChainCheck(int length) {
	int i;Chain(length);Walk();
	for(i=0;i<count;i++) { srfGridMesh_t *grid=(void *)surfaces[i].data;Check(grid->lodFixed==2 && grid->widthLodError[1]==1 && !grid->lodParent && !grid->lodNextSurface,"native chain propagation and cleared continuation fields"); }
}
int main(void) {
	int i;for(i=0;i<400;i++) { Meshes(12345u+i*7919u);Oracle(); }
	Check(stockTouches>100,"tiny oracle corpus exercises actual matching propagation");
	Chain(7);Oracle();ChainCheck(0);ChainCheck(1);ChainCheck(2);ChainCheck(4096);
	FreeGrids();puts("BSP native LOD stock oracles, deep constant-stack propagation and no-allocation checks passed (issue #45)");return 0;
}
