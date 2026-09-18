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
static void NativeBudgets(void) {
	facet_t facet;float plane[4]={1,0,0,123};
	vec3_t a={0,0,123},b={0,1,123},c={1,0,123};
	int i,flipped;
	memset(planes,0,sizeof(planes));
	for(i=0;i<MAX_PATCH_PLANES;i++) { planes[i].plane[0]=1;planes[i].plane[3]=-1000-i; }
	numPlanes=MAX_PATCH_PLANES;
	Check(CM_FindPlane2(plane,&flipped)==CM_PATCH_PLANE_LIMIT && numPlanes==MAX_PATCH_PLANES,"full bevel plane table rejects insertion");
	Check(CM_FindPlane(a,b,c)==CM_PATCH_PLANE_LIMIT && numPlanes==MAX_PATCH_PLANES,"full triangle plane table rejects insertion");
	Vector4Copy(plane,planes[7].plane);
	Check(CM_FindPlane2(plane,&flipped)==7 && !flipped && numPlanes==MAX_PATCH_PLANES,"full plane table retains matching native plane");
	numPlanes=MAX_PATCH_PLANES-1;
	Check(CM_FindPlane2((float[4]){0,1,0,456},&flipped)==MAX_PATCH_PLANES-1 && numPlanes==MAX_PATCH_PLANES,"last native plane insertion");
	BuildFacet(&facet);numPlanes=5;
	for(i=4;i<26;i++) { facet.borderPlanes[i]=4;facet.borderInward[i]=qtrue; }
	facet.numBorders=25;
	Check(CM_AddFacetBevels(&facet)==NULL && facet.numBorders==26 && facet.borderPlanes[25]==facet.surfacePlane && !zoneLive,"last native border holds opposite plane");
	facet.numBorders=26;
	Check(!strcmp(CM_AddFacetBevels(&facet),"MAX_FACET_BORDERS") && facet.numBorders==26 && !zoneLive,"full native border array releases winding before opposite-plane rejection");
	BuildFacet(&facet);numPlanes=5;
	for(i=1;i<=4;i++) { float x=planes[i].plane[0],y=planes[i].plane[1];planes[i].plane[0]=(x-y)*0.70710677f;planes[i].plane[1]=(x+y)*0.70710677f; }
	for(i=4;i<25;i++) { facet.borderPlanes[i]=4;facet.borderInward[i]=qtrue; }
	facet.numBorders=25;
	Check(!strcmp(CM_AddFacetBevels(&facet),"MAX_FACET_BORDERS") && facet.numBorders==25 && !zoneLive,"bevel insertion reserves opposite-plane capacity and releases winding");
	BuildFacet(&facet);numPlanes=MAX_PATCH_PLANES;
	for(i=1;i<=4;i++) { float x=planes[i].plane[0],y=planes[i].plane[1];planes[i].plane[0]=(x-y)*0.70710677f;planes[i].plane[1]=(x+y)*0.70710677f; }
	Check(!strcmp(CM_AddFacetBevels(&facet),"MAX_PATCH_PLANES") && !zoneLive && !c_active_windings,"bevel plane failure releases live winding");
}
static void PreflightState(void) {
	vec3_t points[9],savedPoints[4];int i;
	facet_t oldFacet;patchCollide_t oldPatch;
	for(i=0;i<9;i++)VectorSet(points[i],(i%3)*32,(i/3)*32,0);
	debugBlock=qtrue;
	for(i=0;i<4;i++)VectorSet(debugBlockPoints[i],i+0.25f,i+0.5f,i+0.75f);
	memcpy(savedPoints,debugBlockPoints,sizeof(savedPoints));
	numPlanes=17;numFacets=11;debugFacet=&oldFacet;debugPatchCollide=&oldPatch;
	Check(CM_ValidatePatchCollide(3,3,points)==NULL && numPlanes==17 && numFacets==11 && debugBlock &&
		!memcmp(savedPoints,debugBlockPoints,sizeof(savedPoints)) && debugFacet==&oldFacet && debugPatchCollide==&oldPatch && !zoneLive,
		"native preflight retains build counters and persistent debug ownership");
	debugFacet=NULL;debugPatchCollide=NULL;
}
int main(void) {
	FacetOwnership();CopyOwnership();NativeBudgets();PreflightState();
	Check(zoneAllocations==zoneFrees && !zoneLive && !c_active_windings,"all native winding ownership balanced");
	puts("BSP native facet rejection and winding copy ownership regressions passed (issue #45)");return 0;
}
