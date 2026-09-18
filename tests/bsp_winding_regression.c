/* Issue #45: native facet rejection and winding copy ownership. */
#include "../code/qcommon/cm_patch.c"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int c_active_windings;
static void *zones[16];
static int zoneSizes[16],zoneLive,zoneAllocations,zoneFrees;
static void Check(int condition,const char *message) {
	if(!condition) { fprintf(stderr,"BSP winding regression failed: %s\n",message);exit(1); }
}
void *Z_Malloc(int size) {
	void *pointer;
	Check(size>0 && zoneLive<16,"bounded winding fixture allocation");
	pointer=malloc(size);Check(pointer!=NULL,"fixture allocation");
	zones[zoneLive]=pointer;zoneSizes[zoneLive++]=size;zoneAllocations++;
	return pointer;
}
void Z_Free(void *pointer) {
	int i;
	for(i=0;i<zoneLive;i++)if(zones[i]==pointer) {
		free(pointer);zoneLive--;zones[i]=zones[zoneLive];zoneSizes[i]=zoneSizes[zoneLive];zoneFrees++;return;
	}
	Check(0,"unowned winding release");
}
void QDECL Com_Error(int level,const char *format,...) {
	(void)level;(void)format;Check(0,"unexpected native error");exit(1);
}
#ifndef Com_Memcpy
void Com_Memcpy(void *out,const void *in,size_t size) { memcpy(out,in,size); }
#endif
#ifndef Com_Memset
void Com_Memset(void *out,int value,size_t size) { memset(out,value,size); }
#endif
void QDECL Com_Printf(const char *format,...) { (void)format; }
void QDECL Com_DPrintf(const char *format,...) { (void)format; }

static void BuildFacet(facet_t *facet) {
	int i;
	memset(planes,0,sizeof(planes));memset(facet,0,sizeof(*facet));
	planes[0].plane[2]=1;
	VectorSet(planes[1].plane,1,0,0);planes[1].plane[3]=-16;
	VectorSet(planes[2].plane,-1,0,0);planes[2].plane[3]=-16;
	VectorSet(planes[3].plane,0,1,0);planes[3].plane[3]=-16;
	VectorSet(planes[4].plane,0,-1,0);planes[4].plane[3]=-16;
	facet->surfacePlane=0;facet->numBorders=4;
	for(i=0;i<4;i++) { facet->borderPlanes[i]=i+1;facet->borderInward[i]=qtrue; }
}
static void FacetOwnership(void) {
	facet_t facet;int missing,repeat,before;
	BuildFacet(&facet);Check(CM_ValidateFacet(&facet)==qtrue && !zoneLive && !c_active_windings,"valid bounded facet cleanup");
	for(missing=0;missing<4;missing++)for(repeat=0;repeat<128;repeat++) {
		BuildFacet(&facet);facet.borderPlanes[missing]=-1;before=zoneAllocations;
		Check(CM_ValidateFacet(&facet)==qfalse,"native missing border rejection");
		Check(zoneAllocations>before && !zoneLive && !c_active_windings,"missing border releases every live winding");
	}
	BuildFacet(&facet);facet.surfacePlane=-1;before=zoneAllocations;
	Check(CM_ValidateFacet(&facet)==qfalse && zoneAllocations==before && !zoneLive,"missing surface needs no allocation");
	BuildFacet(&facet);planes[1].plane[3]=MAX_MAP_BOUNDS*2;
	Check(CM_ValidateFacet(&facet)==qfalse && !zoneLive && !c_active_windings,"complete clipping frees rejected winding");
	BuildFacet(&facet);facet.numBorders=0;
	Check(CM_ValidateFacet(&facet)==qfalse && !zoneLive && !c_active_windings,"unbounded facet frees before bounds rejection");
}
static void CopyOwnership(void) {
	int count,i,j;
	for(count=0;count<=MAX_POINTS_ON_WINDING;count++) {
		winding_t *original=AllocWinding(count),*copy;
		int bytes=offsetof(winding_t,p)+count*sizeof(vec3_t);
		original->numpoints=count;
		for(i=0;i<count;i++)for(j=0;j<3;j++)original->p[i][j]=(i*3+j-96)*0.125f;
		copy=CopyWinding(original);
		Check(zoneLive==2 && zoneSizes[0]==bytes && zoneSizes[1]==bytes && !memcmp(original,copy,bytes),"exact native winding copy and allocation layout");
		if(count) { copy->p[count-1][2]+=1;Check(original->p[count-1][2]!=copy->p[count-1][2],"copy owns independent points"); }
		FreeWinding(copy);FreeWinding(original);
		Check(!zoneLive && !c_active_windings,"copy and source release ownership");
	}
}
int main(void) {
	FacetOwnership();CopyOwnership();
	Check(zoneAllocations==zoneFrees && !zoneLive && !c_active_windings,"all native winding ownership balanced");
	puts("BSP native facet rejection and winding copy ownership regressions passed (issue #45)");return 0;
}
