/* Issue #45: real swept collision traversal, clipping and bounded stack ownership.
 * The small legacy traversal oracle derives from Id Software's GPL engine. */
#define BSP_FIXTURE_NATIVE_AREA_FLOOD
#include "bsp_fixture.h"
void CM_TraceThroughTree(traceWork_t *work,int node,float startFraction,float endFraction,vec3_t start,vec3_t end);
void CM_TraceThroughLeaf(traceWork_t *work,cLeaf_t *leaf);
static void *temporary[2];
static int live,requests,releases,failRequest,peak;
void *TraceMalloc(size_t size) {
	void *pointer;Check(size>0 && size<2000000 && live<2,"bounded trace growth and two-buffer ownership");
	requests++;if(failRequest==requests)return NULL;
	pointer=malloc(size);Check(pointer!=NULL,"trace temporary fixture allocation");temporary[live++]=pointer;if(live>peak)peak=live;return pointer;
}
void TraceFree(void *pointer) {
	int i;for(i=0;i<live;i++)if(temporary[i]==pointer) { free(pointer);temporary[i]=temporary[--live];releases++;return; }Check(0,"unowned trace temporary release");
}
void CM_TraceThroughPatchCollide(traceWork_t *work,const struct patchCollide_s *patch) { (void)work;(void)patch;Check(0,"unexpected patch trace"); }
qboolean CM_PositionTestInPatchCollide(traceWork_t *work,const struct patchCollide_s *patch) { (void)work;(void)patch;Check(0,"unexpected patch position test");return qfalse; }
static unsigned int At(int lump,int offset) { return BSP_FileWord(source+8+lump*8)+offset; }
static unsigned int Append(int lump,int size) { unsigned int offset=(sourceSize+3)&~3u;memset(source+sourceSize,0,offset-sourceSize+size);Lump(lump,offset,size);sourceSize=offset+size;return offset; }
static void Build(int count,int balanced,int varied) {
	int i,j;unsigned int offset;BuildCM(2);offset=Append(LUMP_NODES,count*sizeof(dnode_t));
	for(i=0;i<count;i++) {
		Word(offset+i*sizeof(dnode_t),varied?2*(i%6):0);
		for(j=0;j<2;j++) {
			int child=balanced?2*i+1+j:j==0?i+1:count;
			Word(offset+i*sizeof(dnode_t)+offsetof(dnode_t,children)+j*4,child<count?(unsigned int)child:i==count-1 && j==0?0xfffffffeu:0xffffffffu);
		}
	}
	offset=Append(LUMP_LEAFS,2*sizeof(dleaf_t));Word(offset+offsetof(dleaf_t,cluster),0xffffffffu);Word(offset+offsetof(dleaf_t,area),0xffffffffu);Word(offset+sizeof(dleaf_t)+offsetof(dleaf_t,numLeafBrushes),1);
}
static void Load(void) { int checksum;FreeHunks();readable=advertised=sourceSize;missing=alignment=0;CM_LoadMap("trace.bsp",qfalse,&checksum);Check(!fileAllocation && cm.numNodes>0,"native collision publication and FS ownership"); }
static void Prepare(traceWork_t *work,const vec3_t start,const vec3_t end,int shape) {
	int i,j;memset(work,0,sizeof(*work));work->trace.fraction=1;work->contents=1;work->isPoint=shape==0;
	VectorCopy(start,work->start);VectorCopy(end,work->end);
	for(i=0;i<3;i++) {
		work->size[1][i]=shape?(i==2?0.5f:0.25f):0;work->size[0][i]=-work->size[1][i];work->extents[i]=work->size[1][i];work->maxOffset+=work->extents[i];
		work->bounds[0][i]=(start[i]<end[i]?start[i]:end[i])-work->extents[i];work->bounds[1][i]=(start[i]>end[i]?start[i]:end[i])+work->extents[i];
	}
	for(j=0;j<8;j++)for(i=0;i<3;i++)work->offsets[j][i]=work->size[(j>>i)&1][i];
	if(shape==2) { work->sphere.use=qtrue;work->sphere.radius=0.25f;work->sphere.halfheight=0.5f;work->sphere.offset[2]=0.25f; }
}
/* Never invoke this unchanged recursive oracle on large/untrusted-depth inputs. */
static void LegacyTraceThroughTree( traceWork_t *tw, int num, float p1f, float p2f, vec3_t p1, vec3_t p2) {
	cNode_t		*node;
	cplane_t	*plane;
	float		t1, t2, offset;
	float		frac, frac2;
	float		idist;
	vec3_t		mid;
	int			side;
	float		midf;

	if (tw->trace.fraction <= p1f) {
		return;		// already hit something nearer
	}

	// if < 0, we are in a leaf node
	if (num < 0) {
		CM_TraceThroughLeaf( tw, &cm.leafs[-1-num] );
		return;
	}

	//
	// find the point distances to the seperating plane
	// and the offset for the size of the box
	//
	node = cm.nodes + num;
	plane = node->plane;

	// adjust the plane distance apropriately for mins/maxs
	if ( plane->type < 3 ) {
		t1 = p1[plane->type] - plane->dist;
		t2 = p2[plane->type] - plane->dist;
		offset = tw->extents[plane->type];
	} else {
		t1 = DotProduct (plane->normal, p1) - plane->dist;
		t2 = DotProduct (plane->normal, p2) - plane->dist;
		if ( tw->isPoint ) {
			offset = 0;
		} else {
#if 0 // bk010201 - DEAD
			// an axial brush right behind a slanted bsp plane
			// will poke through when expanded, so adjust
			// by sqrt(3)
			offset = fabs(tw->extents[0]*plane->normal[0]) +
				fabs(tw->extents[1]*plane->normal[1]) +
				fabs(tw->extents[2]*plane->normal[2]);

			offset *= 2;
			offset = tw->maxOffset;
#endif
			// this is silly
			offset = 2048;
		}
	}

	// see which sides we need to consider
	if ( t1 >= offset + 1 && t2 >= offset + 1 ) {
		LegacyTraceThroughTree( tw, node->children[0], p1f, p2f, p1, p2 );
		return;
	}
	if ( t1 < -offset - 1 && t2 < -offset - 1 ) {
		LegacyTraceThroughTree( tw, node->children[1], p1f, p2f, p1, p2 );
		return;
	}

	// put the crosspoint SURFACE_CLIP_EPSILON pixels on the near side
	if ( t1 < t2 ) {
		idist = 1.0/(t1-t2);
		side = 1;
		frac2 = (t1 + offset + SURFACE_CLIP_EPSILON)*idist;
		frac = (t1 - offset + SURFACE_CLIP_EPSILON)*idist;
	} else if (t1 > t2) {
		idist = 1.0/(t1-t2);
		side = 0;
		frac2 = (t1 - offset - SURFACE_CLIP_EPSILON)*idist;
		frac = (t1 + offset + SURFACE_CLIP_EPSILON)*idist;
	} else {
		side = 0;
		frac = 1;
		frac2 = 0;
	}

	// move up to the node
	if ( frac < 0 ) {
		frac = 0;
	}
	if ( frac > 1 ) {
		frac = 1;
	}
		
	midf = p1f + (p2f - p1f)*frac;

	mid[0] = p1[0] + frac*(p2[0] - p1[0]);
	mid[1] = p1[1] + frac*(p2[1] - p1[1]);
	mid[2] = p1[2] + frac*(p2[2] - p1[2]);

	LegacyTraceThroughTree( tw, node->children[side], p1f, midf, p1, mid );


	// go past the node
	if ( frac2 < 0 ) {
		frac2 = 0;
	}
	if ( frac2 > 1 ) {
		frac2 = 1;
	}
		
	midf = p1f + (p2f - p1f)*frac2;

	mid[0] = p1[0] + frac2*(p2[0] - p1[0]);
	mid[1] = p1[1] + frac2*(p2[1] - p1[1]);
	mid[2] = p1[2] + frac2*(p2[2] - p1[2]);

	LegacyTraceThroughTree( tw, node->children[side^1], midf, p2f, mid, p2 );
}

static void References(void) {
	static const vec3_t points[]={{-3,0,0},{-2,-2,-2},{-1,0,0.25},{0,0,0},{2,0,0},{3,2,1},{0,1.5,0},{0,0,2}};
	traceWork_t actual,expected;int i,j,shape,before;
	Check(cm.numNodes<=7,"recursive oracle restricted to tiny trusted trees");
	for(i=0;i<8;i++)for(j=0;j<8;j++)for(shape=0;shape<3;shape++) {
		Prepare(&actual,points[i],points[j],shape);expected=actual;before=requests;
		cm.checkcount++;CM_TraceThroughTree(&actual,0,0,1,actual.start,actual.end);
		cm.checkcount++;LegacyTraceThroughTree(&expected,0,0,1,expected.start,expected.end);
		Check(!memcmp(&actual,&expected,sizeof(actual)),"all native point/box/capsule clipping results and inputs match stock traversal exactly");
		Check(!live && requests==before,"ordinary trace uses inline storage only");
	}
}
static void Goldens(void) {
	vec3_t start={-2,0,0},end={2,0,0},inside={0,0,0};trace_t result;
	BuildCM(2);Load();References();
	CM_BoxTrace(&result,start,end,NULL,NULL,0,1,0);
	Check(result.fraction==0.21875f && result.endpos[0]==-1.125f && result.plane.normal[0]==-1 && result.plane.dist==1 && result.contents==1 && !result.startsolid && !result.allsolid,"native public point sweep epsilon, plane, fraction, contents and endpoint golden");
	CM_BoxTrace(&result,inside,end,NULL,NULL,0,1,0);Check(result.startsolid && !result.allsolid && result.fraction==1 && result.endpos[0]==2,"native escape-from-solid golden");
	CM_BoxTrace(&result,inside,inside,NULL,NULL,0,1,0);Check(result.startsolid && result.allsolid && !result.fraction,"actual position test via box-leaf traversal");
}
static void Deep(void) {
	vec3_t start={-1,0,0},end={-1,0.5f,0},frontStart={-3,0,0},frontEnd={-3,0.5f,0},backStart={2,0,0},backEnd={2,0.5f,0};traceWork_t work;trace_t result;int before,i;
	Build(4096,0,0);Load();Prepare(&work,start,end,0);work.contents=0;before=requests;
	cm.checkcount++;CM_TraceThroughTree(&work,0,0,1,work.start,work.end);
	Check(work.trace.fraction==1 && !memcmp(work.start,start,sizeof(start)) && !memcmp(work.end,end,sizeof(end)) && !live,"deep crossing trace preserves inputs and releases temporary frames");
	Check(requests>before,"deep overlapping traversal grew beyond inline frames");
	before=requests;CM_BoxTrace(&result,frontStart,frontEnd,NULL,NULL,0,1,0);CM_BoxTrace(&result,backStart,backEnd,NULL,NULL,0,1,0);Check(result.fraction==1 && requests==before && !live,"deep one-sided traces remain allocation free");
	CM_BoxTrace(&result,start,end,NULL,NULL,0,1,0);Check(result.allsolid && result.startsolid && !result.fraction && !live,"near-hit pruning releases every pending far frame");
	for(i=1;i<=6;i++) {
		cNode_t *retainedNodes=cm.nodes;cLeaf_t *retainedLeaves=cm.leafs;int retainedCount=cm.numNodes;
		Prepare(&work,start,end,0);work.contents=0;requests=0;failRequest=i;expectError=1;
		if(!setjmp(errorJump)) { cm.checkcount++;CM_TraceThroughTree(&work,0,0,1,work.start,work.end);Check(0,"injected trace growth OOM was ignored"); }
		expectError=failRequest=0;Check(requests==i && !live && cm.nodes==retainedNodes && cm.leafs==retainedLeaves && cm.numNodes==retainedCount,"every growth failure cleans owned frames and retains the loaded world");
	}
}
int main(void) {
	Goldens();Build(3,0,1);Load();References();Build(7,1,1);Load();References();Deep();
	Check(!live && peak==2 && !fileAllocation,"all trace temporary ownership released");FreeHunks();puts("BSP iterative swept trace, stock clipping goldens, deep pruning and allocation ownership regressions passed (issue #45)");return 0;
}
