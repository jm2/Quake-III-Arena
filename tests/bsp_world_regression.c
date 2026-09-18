/* Issue #45: actual world traversal/culling/dlight state without graphics hardware. */
#include "bsp_fixture.h"
#define __QGL_H__
typedef unsigned int GLuint;
#define GL_CLAMP 0x2900
#include "../code/renderer/tr_bsp.c"
#include "../code/renderer/tr_curve.c"
static void *WorldMalloc(size_t size);
static void WorldFree(void *pointer);
#define malloc WorldMalloc
#define free WorldFree
#include "../code/renderer/tr_world.c"
#undef malloc
#undef free
#include "../code/renderer/tr_light.c"

trGlobals_t tr;
glconfig_t glConfig;
refimport_t ri;
static cvar_t rendererVariable,subdivisions,noCull,lockPvs,drawWorld;
cvar_t *r_vertexLight=&rendererVariable,*r_lightmap=&rendererVariable,*r_mapOverBrightBits=&rendererVariable;
cvar_t *r_singleShader=&rendererVariable,*r_fullbright=&rendererVariable,*r_subdivisions=&subdivisions;
cvar_t *r_nocull=&noCull,*r_nocurves=&rendererVariable,*r_facePlaneCull=&rendererVariable;
cvar_t *r_lockpvs=&lockPvs,*r_showcluster=&rendererVariable,*r_novis=&rendererVariable,*r_drawworld=&drawWorld;
static model_t knownModel;
static shader_t knownShader,shaders[3];
static msurface_t surfaces[3],*frontMarks[2],*backMarks[2];
static srfSurfaceFace_t faces[3];
static dlight_t lights[MAX_DLIGHTS];
static void *temporary[2];
static int live,requests,failRequest,draws,peak;
static surfaceType_t *drawn[8];
static int drawMasks[8],drawFog[8];
static void *WorldMalloc(size_t size) {
	void *pointer;Check(size>0 && size<2000000 && live<2,"bounded renderer traversal growth/ownership");requests++;if(failRequest==requests)return NULL;
	pointer=malloc(size);Check(pointer!=NULL,"renderer traversal temporary");temporary[live++]=pointer;if(live>peak)peak=live;return pointer;
}
static void WorldFree(void *pointer) { int i;for(i=0;i<live;i++)if(temporary[i]==pointer) { free(pointer);temporary[i]=temporary[--live];return; }Check(0,"unowned renderer traversal release"); }
static void *RendererHunk(int size,ha_pref preference) { Check(preference==h_low,"native renderer hunk preference");return Hunk_Alloc(size,h_high); }
static void *RendererMalloc(int size) { Check(size>0,"curve temporary size");return calloc(1,size); }
static void RendererFree(void *pointer) { free(pointer); }
static void QDECL Print(int level,const char *message,...) { (void)level;(void)message; }
shader_t *R_FindShader(const char *name,int lightmap,qboolean mipmap) { (void)name;(void)lightmap;(void)mipmap;return &knownShader; }
model_t *R_AllocModel(void) { return &knownModel; }
void R_SyncRenderThread(void) { }
void R_RemapShader(const char *oldName,const char *newName,const char *time) { (void)oldName;(void)newName;(void)time;Check(0,"unexpected world fixture remap"); }
image_t *R_CreateImage(const char *name,const byte *pixels,int width,int height,qboolean mipmap,qboolean picmip,int wrap) { (void)name;(void)pixels;(void)width;(void)height;(void)mipmap;(void)picmip;(void)wrap;Check(0,"unexpected texture upload");return NULL; }
int R_CullLocalBox(vec3_t bounds[2]) { (void)bounds;Check(0,"unexpected primitive box cull import");return CULL_IN; }
int R_CullPointAndRadius(vec3_t origin,float radius) { (void)origin;(void)radius;Check(0,"unexpected primitive sphere cull import");return CULL_IN; }
int R_CullLocalPointAndRadius(vec3_t origin,float radius) { (void)origin;(void)radius;Check(0,"unexpected local sphere cull import");return CULL_IN; }
void R_AddDrawSurf(surfaceType_t *surface,shader_t *shader,int fog,int dynamic) {
	Check(draws<8 && shader>=shaders && shader<shaders+3,"bounded actual draw import and shader");drawn[draws]=surface;drawMasks[draws]=dynamic;drawFog[draws++]=fog;
}
static unsigned int Append(int lump,int size) { unsigned int offset=(sourceSize+3)&~3u;memset(source+sourceSize,0,offset-sourceSize+size);Lump(lump,offset,size);sourceSize=offset+size;return offset; }
static void Build(int count) {
	int i;unsigned int offset;BuildCM(2);offset=Append(LUMP_NODES,count*sizeof(dnode_t));
	for(i=0;i<count;i++) { Word(offset+i*sizeof(dnode_t)+offsetof(dnode_t,children),i+1<count?(unsigned int)i+1:0xfffffffeu);Word(offset+i*sizeof(dnode_t)+offsetof(dnode_t,children)+4,0xffffffffu); }
	offset=Append(LUMP_LEAFS,2*sizeof(dleaf_t));Word(offset+offsetof(dleaf_t,cluster),0xffffffffu);Word(offset+offsetof(dleaf_t,area),0xffffffffu);
}
static void Load(int count) {
	Build(count);FreeHunks();tr.worldMapLoaded=qfalse;readable=advertised=sourceSize;alignment=missing=0;RE_LoadWorldMap("world-query.bsp");
	Check(tr.worldMapLoaded && tr.world==&s_worldData && s_worldData.numDecisionNodes==count && !fileAllocation,"actual renderer world publication/ownership");
}
static void Prepare(int count,int cull,int visibility) {
	int i,j;memset(surfaces,0,sizeof(surfaces));memset(shaders,0,sizeof(shaders));memset(faces,0,sizeof(faces));memset(lights,0,sizeof(lights));memset(&tr.pc,0,sizeof(tr.pc));
	tr.visCount=71;tr.viewCount=72;tr.smpFrame=0;tr.refdef.num_dlights=2;tr.refdef.dlights=lights;noCull.integer=cull;draws=0;
	lights[0].origin[0]=-2;lights[1].origin[0]=0;lights[0].radius=lights[1].radius=0.25f;
	for(i=0;i<count+2;i++) {
		mnode_t *node=s_worldData.nodes+i;node->visframe=tr.visCount;
		for(j=0;j<3;j++) { node->mins[j]=j==0?-2:-1;node->maxs[j]=j==0?0:1; }
	}
	if(visibility==1)s_worldData.nodes[1].visframe=0;if(visibility==2)s_worldData.nodes[count+1].visframe=0;
	for(i=0;i<3;i++) {
		faces[i].surfaceType=SF_FACE;faces[i].plane.normal[2]=1;faces[i].plane.type=2;shaders[i].cullType=CT_TWO_SIDED;
		surfaces[i].shader=shaders+i;surfaces[i].data=&faces[i].surfaceType;surfaces[i].fogIndex=10+i;
	}
	frontMarks[0]=surfaces;frontMarks[1]=surfaces+1;backMarks[0]=surfaces+1;backMarks[1]=surfaces+2;
	s_worldData.nodes[count+1].firstmarksurface=frontMarks;s_worldData.nodes[count+1].nummarksurfaces=2;
	s_worldData.nodes[count].firstmarksurface=backMarks;s_worldData.nodes[count].nummarksurfaces=2;
	s_worldData.nodes[count+1].mins[0]=s_worldData.nodes[count+1].maxs[0]=-2;s_worldData.nodes[count].mins[0]=s_worldData.nodes[count].maxs[0]=0;
	memset(tr.viewParms.frustum,0,sizeof(tr.viewParms.frustum));
	tr.viewParms.frustum[0].normal[0]=1;tr.viewParms.frustum[0].dist=-3;
	tr.viewParms.frustum[1].normal[0]=-1;tr.viewParms.frustum[1].dist=1;tr.viewParms.frustum[1].type=3;tr.viewParms.frustum[1].signbits=1;
	tr.viewParms.frustum[2].normal[1]=1;tr.viewParms.frustum[2].dist=-2;tr.viewParms.frustum[2].type=1;
	tr.viewParms.frustum[3].normal[1]=-1;tr.viewParms.frustum[3].dist=-2;tr.viewParms.frustum[3].type=3;tr.viewParms.frustum[3].signbits=2;
	ClearBounds(tr.viewParms.visBounds[0],tr.viewParms.visBounds[1]);
}
/* Unchanged original oracle runs only on tiny trusted fixture trees. */
static void LegacyWorldNode( mnode_t *node, int planeBits, int dlightBits ) {

	do {
		int			newDlights[2];

		// if the node wasn't marked as potentially visible, exit
		if (node->visframe != tr.visCount) {
			return;
		}

		// if the bounding volume is outside the frustum, nothing
		// inside can be visible OPTIMIZE: don't do this all the way to leafs?

		if ( !r_nocull->integer ) {
			int		r;

			if ( planeBits & 1 ) {
				r = BoxOnPlaneSide(node->mins, node->maxs, &tr.viewParms.frustum[0]);
				if (r == 2) {
					return;						// culled
				}
				if ( r == 1 ) {
					planeBits &= ~1;			// all descendants will also be in front
				}
			}

			if ( planeBits & 2 ) {
				r = BoxOnPlaneSide(node->mins, node->maxs, &tr.viewParms.frustum[1]);
				if (r == 2) {
					return;						// culled
				}
				if ( r == 1 ) {
					planeBits &= ~2;			// all descendants will also be in front
				}
			}

			if ( planeBits & 4 ) {
				r = BoxOnPlaneSide(node->mins, node->maxs, &tr.viewParms.frustum[2]);
				if (r == 2) {
					return;						// culled
				}
				if ( r == 1 ) {
					planeBits &= ~4;			// all descendants will also be in front
				}
			}

			if ( planeBits & 8 ) {
				r = BoxOnPlaneSide(node->mins, node->maxs, &tr.viewParms.frustum[3]);
				if (r == 2) {
					return;						// culled
				}
				if ( r == 1 ) {
					planeBits &= ~8;			// all descendants will also be in front
				}
			}

		}

		if ( node->contents != -1 ) {
			break;
		}

		// node is just a decision point, so go down both sides
		// since we don't care about sort orders, just go positive to negative

		// determine which dlights are needed
		newDlights[0] = 0;
		newDlights[1] = 0;
		if ( dlightBits ) {
			int	i;

			for ( i = 0 ; i < tr.refdef.num_dlights ; i++ ) {
				dlight_t	*dl;
				float		dist;

				if ( dlightBits & ( 1 << i ) ) {
					dl = &tr.refdef.dlights[i];
					dist = DotProduct( dl->origin, node->plane->normal ) - node->plane->dist;
					
					if ( dist > -dl->radius ) {
						newDlights[0] |= ( 1 << i );
					}
					if ( dist < dl->radius ) {
						newDlights[1] |= ( 1 << i );
					}
				}
			}
		}

		// recurse down the children, front side first
		LegacyWorldNode (node->children[0], planeBits, newDlights[0] );

		// tail recurse
		node = node->children[1];
		dlightBits = newDlights[1];
	} while ( 1 );

	{
		// leaf node, so add mark surfaces
		int			c;
		msurface_t	*surf, **mark;

		tr.pc.c_leafs++;

		// add to z buffer bounds
		if ( node->mins[0] < tr.viewParms.visBounds[0][0] ) {
			tr.viewParms.visBounds[0][0] = node->mins[0];
		}
		if ( node->mins[1] < tr.viewParms.visBounds[0][1] ) {
			tr.viewParms.visBounds[0][1] = node->mins[1];
		}
		if ( node->mins[2] < tr.viewParms.visBounds[0][2] ) {
			tr.viewParms.visBounds[0][2] = node->mins[2];
		}

		if ( node->maxs[0] > tr.viewParms.visBounds[1][0] ) {
			tr.viewParms.visBounds[1][0] = node->maxs[0];
		}
		if ( node->maxs[1] > tr.viewParms.visBounds[1][1] ) {
			tr.viewParms.visBounds[1][1] = node->maxs[1];
		}
		if ( node->maxs[2] > tr.viewParms.visBounds[1][2] ) {
			tr.viewParms.visBounds[1][2] = node->maxs[2];
		}

		// add the individual surfaces
		mark = node->firstmarksurface;
		c = node->nummarksurfaces;
		while (c--) {
			// the surface may have already been added if it
			// spans multiple leafs
			surf = *mark;
			R_AddWorldSurface( surf, dlightBits );
			mark++;
		}
	}

}


static void References(void) {
	int cull,visibility,p,before,masks,planes[]={0,1,2,15},expectedCount,i;frontEndCounters_t expectedPc;
	vec3_t expectedBounds[2];surfaceType_t *expectedDraws[8];int expectedMasks[8],expectedFog[8];msurface_t expectedSurfaces[3];srfSurfaceFace_t expectedFaces[3];
	for(cull=0;cull<2;cull++)for(visibility=0;visibility<3;visibility++)for(p=0;p<4;p++)for(masks=0;masks<4;masks++) {
		Prepare(3,cull,visibility);LegacyWorldNode(s_worldData.nodes,planes[p],masks);expectedCount=draws;expectedPc=tr.pc;
		if(cull==1 && visibility==0 && planes[p]==15 && masks==3)Check(faces[2].dlightBits[0]==0,"stock first-visit deduplication retains the earlier shared opaque-leaf lighting mask");
		memcpy(expectedBounds,tr.viewParms.visBounds,sizeof(expectedBounds));memcpy(expectedDraws,drawn,sizeof(drawn));memcpy(expectedMasks,drawMasks,sizeof(drawMasks));memcpy(expectedFog,drawFog,sizeof(drawFog));memcpy(expectedSurfaces,surfaces,sizeof(surfaces));memcpy(expectedFaces,faces,sizeof(faces));
		Prepare(3,cull,visibility);before=requests;R_RecursiveWorldNode(s_worldData.nodes,planes[p],masks);
		Check(draws==expectedCount && !memcmp(&tr.pc,&expectedPc,sizeof(expectedPc)) && !memcmp(tr.viewParms.visBounds,expectedBounds,sizeof(expectedBounds)) && !memcmp(surfaces,expectedSurfaces,sizeof(surfaces)) && !memcmp(faces,expectedFaces,sizeof(faces)),"native visibility/frustum/dlight/leaf/surface state matches stock exactly");
		for(i=0;i<draws;i++)Check(drawn[i]==expectedDraws[i] && drawMasks[i]==expectedMasks[i] && drawFog[i]==expectedFog[i],"native draw order, fog and dynamic-light Boolean");
		Check(!live && requests==before,"ordinary world query uses inline storage only");
	}
}
static void Deep(void) {
	int before,i;Load(4096);Prepare(4096,1,0);before=requests;R_RecursiveWorldNode(s_worldData.nodes,15,3);
	Check(draws==3 && drawn[0]==surfaces[0].data && drawn[1]==surfaces[1].data && drawn[2]==surfaces[2].data && faces[0].dlightBits[0]==1 && faces[1].dlightBits[0]==1 && faces[2].dlightBits[0]==0 && tr.pc.c_leafs==4097 && requests>before && !live,"deep front-first draw/deduplication/dlight/leaf-count golden and cleanup");
	Check(tr.viewParms.visBounds[0][0]==-2 && tr.viewParms.visBounds[1][0]==0,"deep native visible bounds");
	Prepare(4096,0,0);R_RecursiveWorldNode(s_worldData.nodes,15,3);Check(draws==2 && tr.pc.c_leafs==1 && tr.viewParms.visBounds[0][0]==-2 && tr.viewParms.visBounds[1][0]==-2 && !live,"deep frustum rejection resumes pending siblings safely");
	for(i=1;i<=6;i++) {
		mnode_t *retainedNodes=s_worldData.nodes;world_t *retainedWorld=tr.world;
		Prepare(4096,1,0);requests=0;failRequest=i;expectError=1;
		if(!setjmp(errorJump)) { R_RecursiveWorldNode(s_worldData.nodes,15,3);Check(0,"renderer growth failure ignored"); }
		expectError=failRequest=0;Check(requests==i && !live && tr.world==retainedWorld && s_worldData.nodes==retainedNodes && tr.worldMapLoaded,"every renderer growth failure cleans frames and retains world");
	}
}
static void FullLightMasks(void) {
	int count,i,j;unsigned int expected;bmodel_t model;trRefEntity_t entity;srfGridMesh_t grid;srfTriangles_t triangles;
	Load(3);memset(&model,0,sizeof(model));memset(&entity,0,sizeof(entity));
	for(count=0;count<=32;count++) {
		Prepare(3,1,0);memset(&grid,0,sizeof(grid));memset(&triangles,0,sizeof(triangles));grid.surfaceType=SF_GRID;triangles.surfaceType=SF_TRIANGLES;surfaces[1].data=&grid.surfaceType;surfaces[2].data=&triangles.surfaceType;
		for(i=0;i<32;i++) { VectorClear(lights[i].origin);lights[i].radius=4; }
		for(i=0;i<3;i++) { grid.meshBounds[0][i]=-4;grid.meshBounds[1][i]=4;model.bounds[0][i]=-4;model.bounds[1][i]=4; }
		tr.refdef.num_dlights=count;tr.refdef.rdflags=0;lockPvs.integer=drawWorld.integer=1;R_AddWorldSurfaces();
		expected=count==32?~0u:((1u<<count)-1);
		Check((unsigned int)faces[0].dlightBits[0]==expected && (unsigned int)grid.dlightBits[0]==expected && (unsigned int)triangles.dlightBits[0]==expected && !live,"actual public world query preserves every lighting bit for counts zero through 32");
		model.numSurfaces=3;model.firstSurface=surfaces;tr.currentEntity=&entity;memset(&tr.or,0,sizeof(tr.or));for(j=0;j<3;j++)tr.or.axis[j][j]=1;
		R_DlightBmodel(&model);Check((unsigned int)faces[0].dlightBits[0]==expected && (unsigned int)grid.dlightBits[0]==expected && (unsigned int)triangles.dlightBits[0]==expected && entity.needDlights==(count!=0),"actual brush lighting retains all native mask bits and Boolean");
	}
	for(i=0;i<31;i++) { lights[i].origin[0]=100;lights[i].radius=0.25f; }VectorClear(lights[31].origin);lights[31].radius=0.25f;
	R_DlightBmodel(&model);Check((unsigned int)faces[0].dlightBits[0]==0x80000000u && (unsigned int)grid.dlightBits[0]==0x80000000u && (unsigned int)triangles.dlightBits[0]==0x80000000u,"highest light bit alone remains native in each primitive type");
	for(count=0;count<2;count++) {
		Prepare(3,1,0);for(i=0;i<32;i++) { VectorClear(lights[i].origin);lights[i].radius=4; }
		tr.refdef.num_dlights=count?33:-1;R_AddWorldSurfaces();expected=count?~0u:0;
		Check(tr.refdef.num_dlights==(count?32:0) && (unsigned int)faces[0].dlightBits[0]==expected && !live,"public light count preserves legacy upper clamp and safely handles negative internal count");
	}
}
int main(void) {
	ri.Error=Com_Error;ri.Printf=Print;ri.FS_ReadFile=FS_ReadFile;ri.FS_FreeFile=FS_FreeFile;ri.Hunk_Alloc=RendererHunk;ri.Malloc=RendererMalloc;ri.Free=RendererFree;subdivisions.value=4;
	Load(3);References();Deep();FullLightMasks();Check(!live && peak==2 && !fileAllocation,"all renderer traversal ownership released");FreeHunks();puts("BSP iterative world traversal, stock culling/draw goldens, full lighting masks and growth ownership regressions passed (issue #45)");return 0;
}
