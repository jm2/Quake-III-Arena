/* Issue #251: box queries keep crossed nodes' back children on a fixed stack.
 *
 * The walks below are master's CM_BoxLeafnums_r and R_BoxSurfaces_r from
 * before #251, renamed, which climb the parents and recompute plane sides on
 * the way up, plus the stock recursive walks for shallow trees. Random trees,
 * random boxes and every query root must give the same callbacks, lists,
 * overflow results, last leaves, brush deduplication and surface view
 * counters. Deep chains fill the stack, so the parent walk takes over for
 * some front subtrees, at every depth from the first full stack onwards. */
#include "../code/qcommon/cm_local.h"
#include "../code/renderer/tr_marks.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

clipMap_t cm;
trGlobals_t tr;
refimport_t ri;

static void Check(int ok, const char *message) {
	if (!ok) { fprintf(stderr, "box query identity regression failed: %s\n", message); exit(1); }
}

/* ---- master before #251, renamed ---- */
static void Master_CM_BoxLeafnums_r( leafList_t *ll, int nodenum ) {
	cNode_t *node;
	int root = nodenum, side = 0, child, completed, s;
	qboolean entering = qtrue;

	if ( nodenum < 0 ) {
		ll->storeLeafs( ll, nodenum );
		return;
	}
	for ( ;; ) {
		node = &cm.nodes[nodenum];
		if ( entering ) {
			s = BoxOnPlaneSide( ll->bounds[0], ll->bounds[1], node->plane );
			side = s == 2 ? 1 : 0;
			child = node->children[side];
		} else {
			s = side == 0 ? BoxOnPlaneSide( ll->bounds[0], ll->bounds[1], node->plane ) : 1;
			if ( side == 0 && s != 1 && s != 2 ) {
				/* Finish the front subtree before visiting the back subtree. */
				side = 1;
				child = node->children[1];
			} else {
				if ( nodenum == root ) return;
				completed = nodenum;
				nodenum = node->parent;
				side = cm.nodes[nodenum].children[0] == completed ? 0 : 1;
				continue;
			}
		}
		if ( child >= 0 ) {
			nodenum = child;
			entering = qtrue;
		} else {
			ll->storeLeafs( ll, child );
			entering = qfalse;
		}
	}
}

static void Master_R_BoxSurfaces_r(mnode_t *node,vec3_t mins,vec3_t maxs,surfaceType_t **list,int listsize,int *listlength,vec3_t dir) {
	mnode_t *root=node,*child,*completed;
	int side=0,s;
	qboolean entering=qtrue;

	if ( node->contents != -1 ) {
		R_BoxLeafSurfaces(node,mins,maxs,list,listsize,listlength,dir);
		return;
	}
	for ( ;; ) {
		if ( entering ) {
			s=BoxOnPlaneSide(mins,maxs,node->plane);
			side=s==2?1:0;
			child=node->children[side];
		} else {
			s=side==0?BoxOnPlaneSide(mins,maxs,node->plane):1;
			if ( side==0 && s!=1 && s!=2 ) {
				side=1;
				child=node->children[1];
			} else {
				if ( node==root ) return;
				completed=node;
				node=node->parent;
				side=node->children[0]==completed?0:1;
				continue;
			}
		}
		if ( child->contents==-1 ) {
			node=child;
			entering=qtrue;
		} else {
			R_BoxLeafSurfaces(child,mins,maxs,list,listsize,listlength,dir);
			entering=qfalse;
		}
	}
}

/* ---- stock recursion, only for shallow trees ---- */
static void Stock_CM_BoxLeafnums_r( leafList_t *ll, int nodenum ) {
	cNode_t *node;
	int s;
	while ( 1 ) {
		if ( nodenum < 0 ) { ll->storeLeafs( ll, nodenum ); return; }
		node = &cm.nodes[nodenum];
		s = BoxOnPlaneSide( ll->bounds[0], ll->bounds[1], node->plane );
		if ( s == 1 ) nodenum = node->children[0];
		else if ( s == 2 ) nodenum = node->children[1];
		else { Stock_CM_BoxLeafnums_r( ll, node->children[0] ); nodenum = node->children[1]; }
	}
}
static void Stock_R_BoxSurfaces_r(mnode_t *node,vec3_t mins,vec3_t maxs,surfaceType_t **list,int listsize,int *listlength,vec3_t dir) {
	int s;
	while ( node->contents == -1 ) {
		s = BoxOnPlaneSide( mins, maxs, node->plane );
		if ( s == 1 ) node = node->children[0];
		else if ( s == 2 ) node = node->children[1];
		else { Stock_R_BoxSurfaces_r(node->children[0],mins,maxs,list,listsize,listlength,dir); node = node->children[1]; }
	}
	R_BoxLeafSurfaces(node,mins,maxs,list,listsize,listlength,dir);
}

/* ---- random trees ---- */
#define MAX_NODES 5000
#define MAX_LEAFS 72
#define MAX_BRUSHES 64
#define MAX_SURFS 64
#define STOCK_DEPTH 200

static cplane_t planes[MAX_NODES];
static cNode_t nodes[MAX_NODES];
static int depths[MAX_NODES], numNodes, maxDepth;
static cLeaf_t leafs[MAX_LEAFS];
static int leafbrushes[MAX_LEAFS * 4];
static cbrush_t brushes[MAX_BRUSHES];
static mnode_t rnodes[MAX_NODES + MAX_LEAFS];
static msurface_t surfs[MAX_SURFS], *marks[MAX_LEAFS * 4];
static shader_t shaders[4];
static srfSurfaceFace_t faces[MAX_SURFS];
static surfaceType_t otherTypes[MAX_SURFS];
static unsigned int seed = 251;
static long queries, deepQueries, stockQueries;

static unsigned int Next(void) { seed = seed * 1664525u + 1013904223u; return seed >> 8; }
static int Range(int n) { return (int)(Next() % (unsigned int)n); }
static float Unit(void) { return Next() / 16777216.0f; }

/* Integral axial planes make boxes touch planes exactly, a separate BoxOnPlaneSide case. */
static void RandomPlane(cplane_t *plane, int extent) {
	int i;
	memset(plane, 0, sizeof(*plane));
	if (Range(10) < 6) {
		plane->normal[Range(3)] = Range(2) ? 1 : -1;
		plane->dist = (float)(Range(2 * extent + 1) - extent);
	} else {
		for (i = 0; i < 3; i++) plane->normal[i] = Unit() * 2 - 1;
		if (VectorNormalize(plane->normal) == 0) plane->normal[2] = 1;
		plane->dist = (Unit() * 2 - 1) * extent;
	}
	plane->type = PlaneTypeForNormal(plane->normal);
	SetPlaneSignbits(plane);
}

/* Past depth 200, the back leaves of each band of 24 levels share a leaf of
 * their own holding one surface of its own that is always marked. Front-first
 * walks reach back leaves late and deepest first, so the mark lists show how
 * deep a walk went, and in what order, even once shallow leaves fill them. */
#define DEEP_LEAFS 32
static int RandomLeaf(int depth, int side) {
	if (depth <= 200 || !side) return -1 - Range(MAX_LEAFS - DEEP_LEAFS);
	depth = (depth - 201) / 24;
	return -1 - (MAX_LEAFS - DEEP_LEAFS) - (depth < DEEP_LEAFS - 1 ? depth : DEEP_LEAFS - 1);
}

/* shape 0: random growth; 1: a chain that can fill the pending stack; 2: a long chain then random growth */
static void BuildTree(int count, int shape, int extent) {
	static int slots[2 * MAX_NODES];
	int numSlots = 0, i, slot, node, side, chain;
	numNodes = 1; maxDepth = 0;
	nodes[0].parent = -1; depths[0] = 0;
	nodes[0].children[0] = RandomLeaf(1, 0); nodes[0].children[1] = RandomLeaf(1, 1);
	chain = shape == 0 ? 0 : shape == 1 ? count : 1100 + Range(200);
	if (chain > count) chain = count;
	for (i = 1; i < chain; i++) {
		node = i - 1; side = Range(2);
		nodes[node].children[side] = i;
		nodes[i].parent = node; depths[i] = depths[node] + 1;
		nodes[i].children[0] = RandomLeaf(depths[i] + 1, 0); nodes[i].children[1] = RandomLeaf(depths[i] + 1, 1);
		numNodes++;
	}
	for (i = 0; i < numNodes; i++) for (side = 0; side < 2; side++) if (nodes[i].children[side] < 0) slots[numSlots++] = i * 2 + side;
	while (numNodes < count && numSlots) {
		slot = Range(numSlots);
		node = slots[slot] / 2; side = slots[slot] % 2;
		slots[slot] = slots[--numSlots];
		/* Leave some leaf slots unfilled so leaves appear at every depth. */
		if (Range(8) == 0) continue;
		i = numNodes++;
		nodes[node].children[side] = i;
		nodes[i].parent = node; depths[i] = depths[node] + 1;
		nodes[i].children[0] = RandomLeaf(depths[i] + 1, 0); nodes[i].children[1] = RandomLeaf(depths[i] + 1, 1);
		slots[numSlots++] = i * 2; slots[numSlots++] = i * 2 + 1;
	}
	for (i = 0; i < numNodes; i++) {
		RandomPlane(&planes[i], extent);
		if (i < chain) {
			/* Chain planes pass near the origin, so big boxes cross them all and
			 * reach every depth; a rare far plane ends the walk at a random depth. */
			planes[i].dist = Range(1500) ? planes[i].dist * 0.5f : (Range(2) ? 3.0f : -3.0f) * extent;
		}
		nodes[i].plane = &planes[i];
		if (depths[i] > maxDepth) maxDepth = depths[i];
	}
}

static void BuildLeaves(int extent) {
	int i, j, k, count = 0;
	for (i = 0; i < MAX_BRUSHES; i++) {
		memset(&brushes[i], 0, sizeof(brushes[i]));
		for (j = 0; j < 3; j++) {
			float a = (float)(Range(2 * extent + 1) - extent), b = (float)(Range(2 * extent + 1) - extent);
			brushes[i].bounds[0][j] = a < b ? a : b; brushes[i].bounds[1][j] = a < b ? b : a;
		}
	}
	for (i = 0; i < MAX_LEAFS; i++) {
		memset(&leafs[i], 0, sizeof(leafs[i]));
		leafs[i].cluster = Range(3) ? i : -1; /* opaque leaves do not move lastLeaf */
		leafs[i].firstLeafBrush = count;
		leafs[i].numLeafBrushes = Range(5);
		for (k = 0; k < leafs[i].numLeafBrushes; k++) leafbrushes[count++] = Range(MAX_BRUSHES); /* shared and repeated brushes */
	}
}

/* The renderer tree mirrors the collision tree, with marks shared between leaves. */
static void BuildRenderer(void) {
	int i, j, k, child, count = 0;
	memset(rnodes, 0, sizeof(rnodes));
	for (i = 0; i < MAX_SURFS; i++) {
		memset(&surfs[i], 0, sizeof(surfs[i]));
		surfs[i].shader = &shaders[Range(8) == 0 ? 1 + Range(3) : 0];
		k = Range(4);
		if (k < 2) {
			memset(&faces[i], 0, sizeof(faces[i]));
			faces[i].surfaceType = SF_FACE;
			RandomPlane(&faces[i].plane, 16);
			surfs[i].data = &faces[i].surfaceType;
		} else {
			otherTypes[i] = k == 2 ? SF_GRID : SF_TRIANGLES;
			surfs[i].data = &otherTypes[i];
		}
	}
	for (i = 0; i < MAX_LEAFS; i++) {
		mnode_t *leaf = &rnodes[MAX_NODES + i];
		leaf->contents = 0;
		leaf->firstmarksurface = marks + count;
		if (i >= MAX_LEAFS - DEEP_LEAFS) {
			j = MAX_SURFS - MAX_LEAFS + i;
			surfs[j].shader = &shaders[0]; otherTypes[j] = SF_GRID; surfs[j].data = &otherTypes[j];
			leaf->nummarksurfaces = 1; marks[count++] = &surfs[j];
			continue;
		}
		leaf->nummarksurfaces = Range(5);
		for (k = 0; k < leaf->nummarksurfaces; k++) marks[count++] = &surfs[Range(MAX_SURFS - DEEP_LEAFS)];
	}
	for (i = 0; i < numNodes; i++) {
		rnodes[i].contents = -1;
		rnodes[i].plane = nodes[i].plane;
		rnodes[i].parent = nodes[i].parent >= 0 ? &rnodes[nodes[i].parent] : NULL;
		for (j = 0; j < 2; j++) {
			child = nodes[i].children[j];
			rnodes[i].children[j] = child >= 0 ? &rnodes[child] : &rnodes[MAX_NODES + (-1 - child)];
		}
	}
}

/* ---- comparisons ---- */
static int captured[3][100000], capturedCount[3];
static int captureSlot;
static void Capture(leafList_t *ll, int nodenum) {
	(void)ll;
	Check(capturedCount[captureSlot] < 100000, "bounded captured callbacks");
	captured[captureSlot][capturedCount[captureSlot]++] = nodenum;
}
static void CompareCallbacks(const vec3_t mins, const vec3_t maxs, int root) {
	leafList_t ll;
	int slot, stock = root < 0 || maxDepth - depths[root] < STOCK_DEPTH;
	void (*walks[3])(leafList_t *, int) = { CM_BoxLeafnums_r, Master_CM_BoxLeafnums_r, Stock_CM_BoxLeafnums_r };
	for (slot = 0; slot < (stock ? 3 : 2); slot++) {
		memset(&ll, 0, sizeof(ll));
		VectorCopy(mins, ll.bounds[0]); VectorCopy(maxs, ll.bounds[1]);
		ll.storeLeafs = Capture;
		captureSlot = slot; capturedCount[slot] = 0;
		walks[slot](&ll, root);
	}
	Check(capturedCount[0] == capturedCount[1] && !memcmp(captured[0], captured[1], capturedCount[0] * sizeof(int)),
		"callback order matches master at every root");
	if (stock) {
		Check(capturedCount[0] == capturedCount[2] && !memcmp(captured[0], captured[2], capturedCount[0] * sizeof(int)),
			"callback order matches the stock recursion");
		stockQueries++;
	}
}

/* CM_BoxLeafnums and CM_BoxBrushes with master's walk in place of the new one. */
static int MasterBoxLeafnums(const vec3_t mins, const vec3_t maxs, int *list, int listsize, int *lastLeaf) {
	leafList_t ll;
	cm.checkcount++;
	VectorCopy(mins, ll.bounds[0]); VectorCopy(maxs, ll.bounds[1]);
	ll.count = 0; ll.maxcount = listsize; ll.list = list; ll.storeLeafs = CM_StoreLeafs; ll.lastLeaf = 0; ll.overflowed = qfalse;
	Master_CM_BoxLeafnums_r(&ll, 0);
	*lastLeaf = ll.lastLeaf;
	return ll.count;
}
static int MasterBoxBrushes(const vec3_t mins, const vec3_t maxs, cbrush_t **list, int listsize) {
	leafList_t ll;
	cm.checkcount++;
	VectorCopy(mins, ll.bounds[0]); VectorCopy(maxs, ll.bounds[1]);
	ll.count = 0; ll.maxcount = listsize; ll.list = (void *)list; ll.storeLeafs = CM_StoreBrushes; ll.lastLeaf = 0; ll.overflowed = qfalse;
	Master_CM_BoxLeafnums_r(&ll, 0);
	return ll.count;
}
static void CompareLists(const vec3_t mins, const vec3_t maxs) {
	static const int sizes[] = { 0, 1, 3, 16, 1024 };
	int list[2][1024], last[2], count[2], i;
	cbrush_t *found[2][1024];
	for (i = 0; i < (int)(sizeof(sizes) / sizeof(sizes[0])); i++) {
		memset(list, 0x5a, sizeof(list));
		last[0] = last[1] = -7;
		count[0] = CM_BoxLeafnums(mins, maxs, list[0], sizes[i], &last[0]);
		count[1] = MasterBoxLeafnums(mins, maxs, list[1], sizes[i], &last[1]);
		Check(count[0] == count[1] && last[0] == last[1] && !memcmp(list[0], list[1], sizeof(list[0])),
			"leaf list, overflow prefix, guard and last leaf match master");
		memset(found, 0, sizeof(found));
		count[0] = CM_BoxBrushes(mins, maxs, found[0], sizes[i]);
		count[1] = MasterBoxBrushes(mins, maxs, found[1], sizes[i]);
		Check(count[0] == count[1] && !memcmp(found[0], found[1], sizeof(found[0])),
			"deduplicated brush list and overflow prefix match master");
	}
}
static void CompareMarks(vec3_t mins, vec3_t maxs, mnode_t *root, int rootDepth) {
	static const int sizes[] = { 0, 2, 64 };
	surfaceType_t *list[3][64];
	int length[3], seen[3][MAX_SURFS], i, j, slot, stock = rootDepth < 0 || maxDepth - rootDepth < STOCK_DEPTH;
	vec3_t dir;
	for (j = 0; j < 3; j++) dir[j] = Unit() * 2 - 1;
	for (i = 0; i < (int)(sizeof(sizes) / sizeof(sizes[0])); i++) {
		for (slot = 0; slot < (stock ? 3 : 2); slot++) {
			memset(list[slot], 0, sizeof(list[slot]));
			length[slot] = 0;
			tr.viewCount++;
			if (slot == 0) R_BoxSurfaces_r(root, mins, maxs, list[slot], sizes[i], &length[slot], dir);
			else if (slot == 1) Master_R_BoxSurfaces_r(root, mins, maxs, list[slot], sizes[i], &length[slot], dir);
			else Stock_R_BoxSurfaces_r(root, mins, maxs, list[slot], sizes[i], &length[slot], dir);
			for (j = 0; j < MAX_SURFS; j++) seen[slot][j] = surfs[j].viewCount == tr.viewCount;
		}
		Check(length[0] == length[1] && !memcmp(list[0], list[1], sizeof(list[0])) && !memcmp(seen[0], seen[1], sizeof(seen[0])),
			"mark surface list, capacity prefix and view counters match master");
		if (stock) Check(length[0] == length[2] && !memcmp(list[0], list[2], sizeof(list[0])) && !memcmp(seen[0], seen[2], sizeof(seen[0])),
			"mark surface list matches the stock recursion");
	}
}

static void RandomBox(vec3_t mins, vec3_t maxs, int extent) {
	int j, kind = Range(5);
	if (kind == 4) { /* big boxes around the origin */
		for (j = 0; j < 3; j++) { mins[j] = -1.5f * extent - Unit(); maxs[j] = 1.5f * extent + Unit(); }
		return;
	}
	for (j = 0; j < 3; j++) {
		if (kind == 0) { mins[j] = maxs[j] = (float)(Range(2 * extent + 1) - extent); } /* point boxes on integral planes */
		else if (kind == 1) { float a = (float)(Range(2 * extent + 1) - extent), b = (float)(Range(2 * extent + 1) - extent); mins[j] = a < b ? a : b; maxs[j] = a < b ? b : a; }
		else { float c = (Unit() * 2 - 1) * extent, h = Unit() * extent * (kind == 3 ? 1.0f : 0.1f); mins[j] = c - h; maxs[j] = c + h; }
	}
}

static void RunTree(int count, int shape, int extent, int boxes) {
	vec3_t mins, maxs;
	int b, root;
	BuildTree(count, shape, extent);
	BuildLeaves(extent);
	BuildRenderer();
	cm.nodes = nodes; cm.numNodes = numNodes; cm.planes = planes; cm.numPlanes = numNodes;
	cm.leafs = leafs; cm.numLeafs = MAX_LEAFS; cm.leafbrushes = leafbrushes; cm.brushes = brushes; cm.numBrushes = MAX_BRUSHES;
	for (b = 0; b < boxes; b++) {
		RandomBox(mins, maxs, extent);
		/* the world root, any subtree root, and a leaf as the root */
		CompareCallbacks(mins, maxs, 0);
		root = Range(numNodes);
		CompareCallbacks(mins, maxs, root);
		CompareCallbacks(mins, maxs, -1 - Range(MAX_LEAFS));
		CompareLists(mins, maxs);
		CompareMarks(mins, maxs, &rnodes[0], 0);
		CompareMarks(mins, maxs, &rnodes[root], depths[root]);
		CompareMarks(mins, maxs, &rnodes[MAX_NODES + Range(MAX_LEAFS)], -1);
		queries++;
		if (maxDepth >= 2 * CM_BOX_PENDING) deepQueries++;
	}
}

int main(void) {
	int i;
	Check(CM_BOX_PENDING == R_BOX_PENDING, "both walks hold the same number of back subtrees");
	shaders[1].surfaceFlags = SURF_NOIMPACT; shaders[2].surfaceFlags = SURF_NOMARKS; shaders[3].contentFlags = CONTENTS_FOG;
	tr.viewCount = 1;
	for (i = 0; i < 400; i++) RunTree(1 + Range(400), 0, 4 + Range(64), 40);
	for (i = 0; i < 20; i++) RunTree(1500 + Range(1500), 0, 16, 40);
	/* Small extents make boxes cross planes all the way down the chain. */
	/* about half the crossed chain nodes leave a back leaf pending, so these end near a full stack */
	for (i = 0; i < 12; i++) RunTree(2 * CM_BOX_PENDING - 8 + Range(17), 1, 2, 60);
	for (i = 0; i < 6; i++) RunTree(MAX_NODES, 1, 2 + Range(3), 30);
	for (i = 0; i < 10; i++) RunTree(1400 + Range(MAX_NODES - 1400), 2, 3 + Range(4), 30);
	Check(deepQueries > 0 && stockQueries > 0, "deep and stock-comparable queries both ran");
	printf("Box query pending stack matches master and stock walks on %ld random boxes (%ld on trees that can fill it) (issue #251)\n", queries, deepQueries);
	return 0;
}
