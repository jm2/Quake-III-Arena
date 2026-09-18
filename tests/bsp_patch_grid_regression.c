/* Issue #45: actual native collision patch refinement and caller preflight. */
#define BSP_FIXTURE_NATIVE_PATCH_COLLISION
#define BSP_FIXTURE_NATIVE_AREA_FLOOD
#include "bsp_fixture.h"
#include "../code/qcommon/cm_patch.h"
#include <float.h>

static void *gridTemporary, *zones[64];
static int failGrid, gridAllocations, gridFrees, zoneLive;

void *PatchGridMalloc(size_t size) {
	Check(size==sizeof(cGrid_t) && !gridTemporary,"one exact native grid preflight workspace");
	if(failGrid) { failGrid=0;return NULL; }
	gridTemporary=malloc(size);
	Check(gridTemporary!=NULL,"grid preflight workspace");
	gridAllocations++;
	return gridTemporary;
}
void PatchGridFree(void *pointer) {
	Check(pointer && pointer==gridTemporary,"owned grid workspace release");
	free(pointer);gridTemporary=NULL;gridFrees++;
}
void *Z_Malloc(int size) {
	void *pointer;
	Check(size>0 && size<1000000 && zoneLive<64,"bounded winding fixture allocation");
	pointer=calloc(1,size);
	Check(pointer!=NULL,"winding allocation");
	zones[zoneLive++]=pointer;
	return pointer;
}
void Z_Free(void *pointer) {
	int i;
	for(i=0;i<zoneLive;i++) {
		if(zones[i]==pointer) { free(pointer);zones[i]=zones[--zoneLive];return; }
	}
	Check(0,"unowned winding release");
}
static unsigned int At(int lump,int offset) {
	return BSP_FileWord(source+8+lump*8)+offset;
}
static unsigned int Append(int lump,int size) {
	unsigned int offset=(sourceSize+3)&~3u;
	memset(source+sourceSize,0,offset-sourceSize+size);
	Lump(lump,offset,size);sourceSize=offset+size;
	return offset;
}
static void Build(int width,int height,int curveAxis,int amplitude) {
	unsigned int offset;
	int i,j,vertices=width*height;
	BuildCM(2);
	offset=Append(LUMP_DRAWVERTS,vertices*sizeof(drawVert_t));
	for(j=0;j<height;j++)for(i=0;i<width;i++) {
		unsigned int vertex=offset+(j*width+i)*sizeof(drawVert_t);
		Float(vertex,i*32);Float(vertex+4,j*32);
		Float(vertex+8,((curveAxis==1?i:curveAxis==2?j:curveAxis>=3?i+j:0)%2?amplitude:0)+(curveAxis==4?i*j*7:0));
	}
	offset=Append(LUMP_SURFACES,sizeof(dsurface_t));
	Word(offset+offsetof(dsurface_t,fogNum),0xffffffffu);
	Word(offset+offsetof(dsurface_t,lightmapNum),0xffffffffu);
	Word(offset+offsetof(dsurface_t,surfaceType),MST_PATCH);
	Word(offset+offsetof(dsurface_t,numVerts),vertices);
	Word(offset+offsetof(dsurface_t,patchWidth),width);
	Word(offset+offsetof(dsurface_t,patchHeight),height);
	offset=Append(LUMP_LEAFSURFACES,4);Word(offset,0);
	Word(At(LUMP_LEAFS,offsetof(dleaf_t,numLeafSurfaces)),1);
	Word(At(LUMP_MODELS,offsetof(dmodel_t,numSurfaces)),1);
}
/* Captured from the unchanged generator: every native bound, plane and facet word. */
static unsigned int HashWords(unsigned int hash,const void *data,int size) {
	int i;const byte *bytes=data;
	Check(size%4==0,"whole native golden words");
	for(i=0;i<size;i+=4) {
		unsigned int word;memcpy(&word,bytes+i,4);
		hash=(hash^word)*16777619u;
	}
	return hash;
}
static unsigned int Fingerprint(patchCollide_t *patch) {
	unsigned int hash=2166136261u;
	hash=HashWords(hash,patch->bounds,sizeof(patch->bounds));
	hash=HashWords(hash,&patch->numPlanes,4);
	hash=HashWords(hash,patch->planes,patch->numPlanes*sizeof(*patch->planes));
	hash=HashWords(hash,&patch->numFacets,4);
	return HashWords(hash,patch->facets,patch->numFacets*sizeof(*patch->facets));
}
static void Load(int width,int height,unsigned int golden) {
	int checksum;patchCollide_t *patch;
	FreeHunks();readable=advertised=sourceSize;alignment=missing=0;
	CM_LoadMap("patch-grid.bsp",qfalse,&checksum);
	Check(!fileAllocation && !gridTemporary && !zoneLive && cm.numSurfaces==1 && cm.surfaces[0],
		"native patch publication and all temporary ownership");
	patch=cm.surfaces[0]->pc;
	Check(patch && Fingerprint(patch)==golden,"bit-exact original native bounds/planes/facets golden");
	Check(patch->numFacets>0 && patch->numFacets<=MAX_FACETS && patch->numPlanes>0 && patch->numPlanes<=MAX_PATCH_PLANES,
		"actual native patch facets and planes");
	Check(patch->bounds[0][0]==-1 && patch->bounds[0][1]==-1 &&
		patch->bounds[1][0]==(width-1)*32+1 && patch->bounds[1][1]==(height-1)*32+1,
		"native exact endpoint bounds and epsilon expansion");
}
static void Reject(int width,int height,int axis,int amplitude) {
	int a;unsigned int golden=Fingerprint(cm.surfaces[0]->pc);Build(width,height,axis,amplitude);
	for(a=0;a<4;a++) {
		alignment=a;RejectCM();
		Check(Fingerprint(cm.surfaces[0]->pc)==golden,"retained native planes/facets after rejected preflight");
		Check(!gridTemporary && !zoneLive,"unsafe grid rejects before world/checksum/hunk changes and releases input/workspace");
	}
	alignment=0;
}
static void RejectDirect(int width,int height,int axis) {
	vec3_t points[MAX_PATCH_VERTS];
	int i,j,allocBefore=allocations;
	for(j=0;j<height;j++)for(i=0;i<width;i++) {
		VectorSet(points[j*width+i],i*32,j*32,((axis==1?i:axis==2?j:i+j)%2?(axis>=3?32:4096):0));
	}
	previous=cm;expectError=1;
	if(!setjmp(errorJump)) {
		CM_GeneratePatchCollide(width,height,points);
		Check(0,"direct unsafe subdivision accepted");
	}
	expectError=0;
	Check(allocations==allocBefore && !memcmp(&cm,&previous,sizeof(cm)) && !zoneLive && !gridTemporary,
		"direct grid guard precedes hunk/facet publication");
}
static void RejectBudget(int width,int height,int axis,int amplitude,const char *expected) {
	vec3_t points[MAX_PATCH_VERTS];int i,j;unsigned int offset;
	const char *error;
	Build(width,height,axis,amplitude);offset=BSP_FileWord(source+8+LUMP_DRAWVERTS*8);
	for(i=0;i<width*height;i++)for(j=0;j<3;j++)points[i][j]=BSP_GeometryFloat(source+offset+i*sizeof(drawVert_t)+j*4);
	error=CM_ValidatePatchCollide(width,height,points);
	if(!error || strcmp(error,expected))fprintf(stderr,"Native patch preflight: expected %s, got %s\n",expected,error?error:"success");
	Check(error && !strcmp(error,expected) && !gridTemporary && !zoneLive,"actual native budget and preflight ownership");
	Reject(width,height,axis,amplitude);
}
static void RejectNumeric(void) {
	vec3_t points[9];unsigned int offset,golden=Fingerprint(cm.surfaces[0]->pc);
	int i,j,a,mode;const char *error;
	for(mode=0;mode<5;mode++) {
		Build(3,3,0,0);offset=BSP_FileWord(source+8+LUMP_DRAWVERTS*8);
		for(i=0;i<9;i++) {
			float x=i%3-1,y=i/3-1,z=0;
			if(mode==0) { x*=1e20f;y*=1e20f; }
			if(mode==1) { x*=1e12f;y*=1e12f; }
			if(mode==2) x*=FLT_MAX;
			if(mode==3) x=FLT_MAX;
			if(mode==4 && i%3==1) z=FLT_MAX;
			Float(offset+i*sizeof(drawVert_t),x);Float(offset+i*sizeof(drawVert_t)+4,y);Float(offset+i*sizeof(drawVert_t)+8,z);
			for(j=0;j<3;j++)points[i][j]=BSP_GeometryFloat(source+offset+i*sizeof(drawVert_t)+j*4);
		}
		error=CM_ValidatePatchCollide(3,3,points);
		Check(error && strstr(error,"nonfinite collision patch") && !gridTemporary && !zoneLive,"finite source overflows reject derived geometry and release preflight ownership");
		for(a=0;a<4;a++) {
			alignment=a;RejectCM();
			Check(!gridTemporary && !zoneLive && Fingerprint(cm.surfaces[0]->pc)==golden,"nonfinite derived geometry retains loaded patch and releases all temporaries");
		}
		alignment=0;
	}
}
int main(void) {
	Build(3,3,0,0);Load(3,3,0xe6e8e7d6u);
	RejectNumeric();
	RejectBudget(31,31,3,32,"MAX_FACETS");
	RejectBudget(31,31,4,128,"MAX_PATCH_PLANES");
	RejectDirect(31,31,3);
	Reject(129,3,1,4096);Reject(3,129,2,4096);
	RejectDirect(129,3,1);RejectDirect(3,129,2);
	Build(129,3,0,0);Load(129,3,0xf006df84u);
	Build(3,129,0,0);Load(3,129,0xaeb2c0d9u);
	Build(127,3,1,32);Load(127,3,0x5aff68c4u);
	Build(3,127,2,32);Load(3,127,0xc36fe2bfu);
	Build(3,3,0,0);
	for(alignment=0;alignment<4;alignment++) {
		failGrid=1;RejectCM();
		Check(!failGrid && !gridTemporary && !zoneLive,"workspace OOM cleanup preserves old collision world");
	}
	alignment=0;
	Check(CM_ValidatePatchCollide(2,3,NULL)!=NULL && CM_ValidatePatchCollide(4,3,(vec3_t *)source)!=NULL &&
		CM_ValidatePatchCollide(131,3,(vec3_t *)source)!=NULL,"private generator parameter checks before source reads");
	Check(gridAllocations==gridFrees && !fileAllocation && !zoneLive,"every grid/winding ownership released");
	FreeHunks();
	puts("BSP native patch-grid bounds, refinement goldens, caller preflight and workspace ownership regressions passed (issue #45)");
	return 0;
}
