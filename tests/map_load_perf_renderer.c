/* Issue #251: renderer half of map_load_perf.c, a separate unit because the
 * renderer and collision headers each define their own world types.
 *
 *   rload <dir> <map> <reps>            time RE_LoadWorldMap and its patch preflight
 *   rbox <dir> <map> <boxes> <reps>     time R_BoxSurfaces_r on random boxes
 */
#define __QGL_H__
typedef unsigned int GLuint;
#define GL_CLAMP 0x2900
#include "tr_bsp.c"
#include "tr_curve.c"
#include "tr_marks.c"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

trGlobals_t tr;
glconfig_t glConfig;
refimport_t ri;
static cvar_t zero, subdivisions, overbright;
cvar_t *r_vertexLight = &zero, *r_lightmap = &zero, *r_mapOverBrightBits = &overbright;
cvar_t *r_singleShader = &zero, *r_fullbright = &zero, *r_subdivisions = &subdivisions;
static shader_t perfShader;
static model_t perfModels[MAX_MOD_KNOWN];
static image_t perfImage;
static int perfModelCount;

int FS_ReadFile(const char *name, void **buffer);
void FS_FreeFile(void *buffer);
void *Hunk_Alloc(int size, ha_pref preference);
void PerfResetHunk(void);
void PerfReport(const char *name, double *times, int reps);
double PerfNow(void);
float PerfRandom(void);
void PerfHash(const void *data, size_t size);
unsigned long long PerfHashValue(void);

shader_t *R_FindShader(const char *name, int lightmap, qboolean mipmap) { (void)name; (void)lightmap; (void)mipmap; return &perfShader; }
model_t *R_AllocModel(void) { if (perfModelCount == MAX_MOD_KNOWN) return NULL; return &perfModels[perfModelCount++]; }
void R_SyncRenderThread(void) { }
void R_RemapShader(const char *oldName, const char *newName, const char *time) { (void)oldName; (void)newName; (void)time; }
image_t *R_CreateImage(const char *name, const byte *pixels, int width, int height, qboolean mipmap, qboolean picmip, int wrap) {
	(void)name; (void)pixels; (void)width; (void)height; (void)mipmap; (void)picmip; (void)wrap;
	return &perfImage;
}
static void QDECL PerfPrint(int level, const char *message, ...) { (void)level; (void)message; }
static void QDECL PerfError(int level, const char *message, ...) {
	va_list args;
	(void)level;
	va_start(args, message); fputs("map_load_perf: renderer error: ", stderr); vfprintf(stderr, message, args); va_end(args);
	fputc('\n', stderr); exit(1);
}
static void *PerfHunk(int size, ha_pref preference) { (void)preference; return Hunk_Alloc(size, h_high); }
static void *PerfMalloc(int size) { return calloc(1, size); }

static void LoadWorld(const char *map) {
	char name[MAX_QPATH];
	Com_sprintf(name, sizeof(name), "maps/%s.bsp", map);
	PerfResetHunk();
	memset(&tr, 0, sizeof(tr)); memset(&s_worldData, 0, sizeof(s_worldData));
	perfModelCount = 0;
	RE_LoadWorldMap(name);
}

static void Setup(void) {
	ri.Error = PerfError; ri.Printf = PerfPrint; ri.FS_ReadFile = FS_ReadFile; ri.FS_FreeFile = FS_FreeFile;
	ri.Hunk_Alloc = PerfHunk; ri.Malloc = PerfMalloc; ri.Free = free;
	subdivisions.value = 4; subdivisions.integer = 4;
	overbright.value = 2; overbright.integer = 2;
}

/* Surface data pointers become surface numbers for the hash. */
static int SurfaceNumber(surfaceType_t *data) {
	int i;
	for (i = 0; i < s_worldData.numsurfaces; i++) if (s_worldData.surfaces[i].data == data) return i;
	return -1;
}

int PerfRenderer(const char *mode, const char *map, int count, int reps) {
	double *times = malloc(reps * sizeof(double)), start;
	int r, i, j;
	Setup();
	if (!strcmp(mode, "rload")) {
		for (r = 0; r < reps; r++) { start = PerfNow(); LoadWorld(map); times[r] = PerfNow() - start; }
		PerfReport("renderer_load", times, reps);
#ifndef Q3_PERF_RETAIL
		{
			char name[MAX_QPATH]; void *buffer; int length; dheader_t header;
			Com_sprintf(name, sizeof(name), "maps/%s.bsp", map);
			length = FS_ReadFile(name, &buffer);
			if (BSP_ValidateHeader(buffer, length, &header)) PerfError(ERR_DROP, "header");
			for (r = 0; r < reps; r++) {
				start = PerfNow();
				if (R_ValidateBSPGeometry(buffer, &header)) PerfError(ERR_DROP, "geometry");
				times[r] = PerfNow() - start;
			}
			PerfReport("renderer_geometry_preflight", times, reps);
			FS_FreeFile(buffer);
		}
#endif
		/* the last load's surface types and every refined patch grid */
		PerfHash(&s_worldData.numsurfaces, sizeof(int));
		for (i = 0; i < s_worldData.numsurfaces; i++) {
			surfaceType_t type = *s_worldData.surfaces[i].data;
			PerfHash(&type, sizeof(type));
			if (type == SF_GRID) {
				srfGridMesh_t *grid = (srfGridMesh_t *)s_worldData.surfaces[i].data;
				PerfHash(&grid->width, sizeof(int)); PerfHash(&grid->height, sizeof(int));
				for (j = 0; j < grid->width * grid->height; j++) PerfHash(grid->verts[j].xyz, sizeof(vec3_t));
			}
		}
		printf("hash renderer_load %016llx\n", PerfHashValue());
		return 0;
	}
	if (!strcmp(mode, "rbox")) {
		vec3_t (*boxes)[2] = malloc(count * sizeof(*boxes)), dir = { 0, 0, -1 };
		surfaceType_t *list[64];
		int length, n;
		LoadWorld(map);
		for (i = 0; i < count; i++) {
			float size = PerfRandom() < 0.9f ? 8 + PerfRandom() * 56 : 64 + PerfRandom() * 448;
			for (j = 0; j < 3; j++) {
				float lo = s_worldData.bmodels[0].bounds[0][j], hi = s_worldData.bmodels[0].bounds[1][j];
				float centre = lo + PerfRandom() * (hi - lo);
				boxes[i][0][j] = centre - size * (0.5f + PerfRandom());
				boxes[i][1][j] = centre + size * (0.5f + PerfRandom());
			}
		}
		for (r = 0; r < reps; r++) {
			start = PerfNow();
			for (i = 0; i < count; i++) { tr.viewCount++; length = 0; R_BoxSurfaces_r(s_worldData.nodes, boxes[i][0], boxes[i][1], list, 64, &length, dir); }
			times[r] = PerfNow() - start;
		}
		PerfReport("renderer_box_surfaces", times, reps);
		for (i = 0; i < count; i++) {
			tr.viewCount++; length = 0;
			R_BoxSurfaces_r(s_worldData.nodes, boxes[i][0], boxes[i][1], list, 64, &length, dir);
			PerfHash(&length, sizeof(length));
			for (j = 0; j < length; j++) { n = SurfaceNumber(list[j]); PerfHash(&n, sizeof(n)); }
		}
		printf("hash renderer_box_surfaces %016llx\n", PerfHashValue());
		return 0;
	}
	return -1;
}
