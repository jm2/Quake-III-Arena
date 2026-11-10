/* Issue #251: local timing and identity probe on real maps; see map_load_perf.py.
 *
 *   cm <dir> <map> <reps>             time CM_LoadMap and hash the loaded world
 *   box <dir> <map> <boxes> <reps>    time CM_BoxLeafnums/CM_BoxBrushes on random boxes
 *   aas <dir> <map> <reps>            time AAS_LoadMap, then AAS_UpdateAreaRoutingCache over every area
 *   rload, rbox                       renderer modes, see map_load_perf_renderer.c
 *
 * Each mode prints "time <name> <best ms> <median ms>" and "hash <name> <hex>"
 * lines. The hashes cover the outputs only, never pointers, so builds of two
 * source trees can be compared. Build with -DQ3_PERF_RETAIL for a tree whose
 * botlib import has no TryGetMemory. */
#include "cm_local.h"
#include "cm_patch.h"
#include "l_memory.h"
#include "l_libvar.h"
#include "l_script.h"
#include "l_precomp.h"
#include "l_struct.h"
#include "aasfile.h"
#include "botlib.h"
#include "be_aas.h"
#include "be_aas_funcs.h"
#include "be_interface.h"
#include "be_aas_def.h"
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* botlib's global; the engine headers above do not declare it. */
extern aas_t aasworld;
int AAS_Setup(void);
int AAS_LoadMap(const char *mapname);
void AAS_ContinueInit(float time);
void AAS_FreeRoutingCache(aas_routingcache_t *cache);
aas_routingcache_t *AAS_AllocRoutingCache(int numtraveltimes);
/* qboolean in hardened trees, void in 204fe36; the result is not used. */
void AAS_UpdateAreaRoutingCache(aas_routingcache_t *areacache);

static const char *dataDir;
static byte *hunk;
static int hunkUsed;
#define PERF_HUNK (96 << 20)

static void Fail(const char *message) { fprintf(stderr, "map_load_perf: %s\n", message); exit(1); }

/* ---- engine imports ---- */
void QDECL Com_Error(int level, const char *format, ...) {
	va_list args;
	(void)level;
	va_start(args, format); fputs("map_load_perf: Com_Error: ", stderr); vfprintf(stderr, format, args); va_end(args);
	fputc('\n', stderr); exit(1);
}
void QDECL Com_Printf(const char *format, ...) { (void)format; }
void QDECL Com_DPrintf(const char *format, ...) { (void)format; }
void Com_Memset(void *dest, const int val, const size_t count) { memset(dest, val, count); }
void Com_Memcpy(void *dest, const void *src, const size_t count) { memcpy(dest, src, count); }
cvar_t *Cvar_Get(const char *name, const char *value, int flags) {
	static cvar_t vars[8]; static int count; int i;
	(void)flags;
	for (i = 0; i < count; i++) if (!strcmp(vars[i].name, name)) return &vars[i];
	if (count == 8) Fail("too many cvars");
	vars[count].name = (char *)name; vars[count].value = atof(value); vars[count].integer = atoi(value);
	return &vars[count++];
}
int Hunk_AllocationSize(int size) { return size < 0 || size > INT_MAX - 31 ? -1 : (size + 31) & ~31; }
int Hunk_MemoryRemaining(void) { return PERF_HUNK - hunkUsed; }
void *Hunk_Alloc(int size, ha_pref preference) {
	void *p; int cost = Hunk_AllocationSize(size);
	(void)preference;
	if (cost < 0 || cost > PERF_HUNK - hunkUsed) Fail("hunk exhausted");
	p = hunk + hunkUsed; hunkUsed += cost; memset(p, 0, size);
	return p;
}
void *Z_Malloc(int size) { void *p = calloc(1, size); if (!p) Fail("zone"); return p; }
void Z_Free(void *p) { free(p); }
void BotDrawDebugPolygons(void (*drawPoly)(int color, int numPoints, float *points), int value) { (void)drawPoly; (void)value; }

static byte *ReadWhole(const char *name, int *length) {
	char path[1024]; const char *base = strrchr(name, '/'); FILE *f; long size; byte *data;
	snprintf(path, sizeof(path), "%s/%s", dataDir, base ? base + 1 : name);
	f = fopen(path, "rb");
	if (!f) return NULL;
	fseek(f, 0, SEEK_END); size = ftell(f); fseek(f, 0, SEEK_SET);
	data = malloc(size + 1);
	if (!data || fread(data, 1, size, f) != (size_t)size) Fail("read");
	fclose(f); *length = (int)size;
	return data;
}
/* Serve files from memory so disk reads stay out of the timings. */
static struct { char name[MAX_QPATH]; byte *data; int length; } files[4];
static const byte *Cached(const char *name, int *length) {
	int i;
	for (i = 0; i < 4 && files[i].data; i++) if (!strcmp(files[i].name, name)) { *length = files[i].length; return files[i].data; }
	if (i == 4) Fail("file cache");
	files[i].data = ReadWhole(name, &files[i].length);
	if (!files[i].data) return NULL;
	Q_strncpyz(files[i].name, name, sizeof(files[i].name));
	*length = files[i].length;
	return files[i].data;
}
int FS_ReadFile(const char *name, void **buffer) {
	int length; const byte *data = Cached(name, &length);
	if (!data) { if (buffer) *buffer = NULL; return -1; }
	if (buffer) { *buffer = malloc(length + 1); memcpy(*buffer, data, length); ((byte *)*buffer)[length] = 0; }
	return length;
}
void FS_FreeFile(void *buffer) { free(buffer); }

/* ---- measurement ---- */
#define Now PerfNow
#define Report PerfReport
#define Random PerfRandom
double PerfNow(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1e3 + t.tv_nsec / 1e6; }
static int Compare(const void *a, const void *b) { double x = *(const double *)a, y = *(const double *)b; return x < y ? -1 : x > y; }
void PerfReport(const char *name, double *times, int reps) {
	qsort(times, reps, sizeof(*times), Compare);
	printf("time %s %.3f %.3f\n", name, times[0], times[reps / 2]);
}
static unsigned long long hashValue = 1469598103934665603ull;
#define Hash PerfHash
void PerfHash(const void *data, size_t size) {
	const byte *p = data; size_t i;
	for (i = 0; i < size; i++) hashValue = (hashValue ^ p[i]) * 1099511628211ull;
}
static void HashInt(int value) { Hash(&value, sizeof(value)); }
static void HashFloats(const float *values, int count) { Hash(values, count * sizeof(float)); }
static unsigned int seed = 12345;
float PerfRandom(void) { seed = seed * 1664525u + 1013904223u; return (seed >> 8) / 16777216.0f; }
unsigned long long PerfHashValue(void) { return hashValue; }
void PerfResetHunk(void) { hunkUsed = 0; }
int PerfRenderer(const char *mode, const char *map, int count, int reps);

/* ---- collision map ---- */
static int LoadCM(const char *map) {
	char name[MAX_QPATH]; int checksum;
	Com_sprintf(name, sizeof(name), "maps/%s.bsp", map);
	hunkUsed = 0;
	CM_ClearMap();
	CM_LoadMap(name, qfalse, &checksum);
	return checksum;
}
static void HashCM(int checksum) {
	int i, j;
	HashInt(checksum);
	HashInt(cm.numShaders); Hash(cm.shaders, cm.numShaders * sizeof(dshader_t));
	HashInt(cm.numPlanes);
	for (i = 0; i < cm.numPlanes; i++) { HashFloats(cm.planes[i].normal, 3); HashFloats(&cm.planes[i].dist, 1); HashInt(cm.planes[i].type); HashInt(cm.planes[i].signbits); }
	HashInt(cm.numNodes);
	for (i = 0; i < cm.numNodes; i++) { HashInt(cm.nodes[i].plane - cm.planes); HashInt(cm.nodes[i].children[0]); HashInt(cm.nodes[i].children[1]); }
	HashInt(cm.numLeafs); Hash(cm.leafs, cm.numLeafs * sizeof(cLeaf_t));
	HashInt(cm.numLeafBrushes); Hash(cm.leafbrushes, cm.numLeafBrushes * sizeof(int));
	HashInt(cm.numLeafSurfaces); Hash(cm.leafsurfaces, cm.numLeafSurfaces * sizeof(int));
	HashInt(cm.numBrushSides);
	for (i = 0; i < cm.numBrushSides; i++) { HashInt(cm.brushsides[i].plane - cm.planes); HashInt(cm.brushsides[i].surfaceFlags); HashInt(cm.brushsides[i].shaderNum); }
	HashInt(cm.numBrushes);
	for (i = 0; i < cm.numBrushes; i++) {
		cbrush_t *b = &cm.brushes[i];
		HashInt(b->shaderNum); HashInt(b->contents); HashFloats(b->bounds[0], 6); HashInt(b->numsides); HashInt(b->sides - cm.brushsides);
	}
	HashInt(cm.numSubModels);
	for (i = 0; i < cm.numSubModels; i++) { HashFloats(cm.cmodels[i].mins, 3); HashFloats(cm.cmodels[i].maxs, 3); Hash(&cm.cmodels[i].leaf, sizeof(cLeaf_t)); }
	HashInt(cm.numClusters); HashInt(cm.clusterBytes); HashInt(cm.vised); Hash(cm.visibility, cm.numClusters * cm.clusterBytes);
	HashInt(cm.numEntityChars); Hash(cm.entityString, cm.numEntityChars);
	HashInt(cm.numAreas); Hash(cm.areas, cm.numAreas * sizeof(cArea_t)); Hash(cm.areaPortals, cm.numAreas * cm.numAreas * sizeof(int));
	HashInt(cm.numSurfaces);
	for (i = 0; i < cm.numSurfaces; i++) {
		cPatch_t *patch = cm.surfaces[i];
		if (!patch) { HashInt(-1); continue; }
		HashInt(patch->surfaceFlags); HashInt(patch->contents);
		HashFloats(patch->pc->bounds[0], 6);
		HashInt(patch->pc->numPlanes); Hash(patch->pc->planes, patch->pc->numPlanes * sizeof(patchPlane_t));
		HashInt(patch->pc->numFacets);
		for (j = 0; j < patch->pc->numFacets; j++) Hash(&patch->pc->facets[j], sizeof(facet_t));
	}
}
static void ModeCM(const char *map, int reps) {
	double *times = malloc(reps * sizeof(double)); int i, checksum = 0; double start;
	for (i = 0; i < reps; i++) { start = Now(); checksum = LoadCM(map); times[i] = Now() - start; }
	Report("cm_load", times, reps);
	HashCM(checksum);
	printf("hash cm_load %016llx\n", hashValue);
	{
		/* facet and plane bytes, about what the preflight keeps for the loader */
		int patches = 0, bytes = 0;
		for (i = 0; i < cm.numSurfaces; i++) {
			if (!cm.surfaces[i]) continue;
			patches++;
			bytes += cm.surfaces[i]->pc->numFacets * sizeof(facet_t) + cm.surfaces[i]->pc->numPlanes * sizeof(patchPlane_t);
		}
		printf("info hunk_bytes %d surfaces %d patches %d patch_bytes %d\n", hunkUsed, cm.numSurfaces, patches, bytes);
	}
}

typedef struct { vec3_t mins, maxs; } box_t;
static void MakeBoxes(box_t *boxes, int count) {
	int i, j; float size;
	for (i = 0; i < count; i++) {
		/* Mostly entity sized, sometimes room sized, like SV_LinkEntity and marks. */
		size = Random() < 0.9f ? 8 + Random() * 56 : 64 + Random() * 448;
		for (j = 0; j < 3; j++) {
			float lo = cm.cmodels[0].mins[j], hi = cm.cmodels[0].maxs[j];
			float centre = lo + Random() * (hi - lo);
			boxes[i].mins[j] = centre - size * (0.5f + Random());
			boxes[i].maxs[j] = centre + size * (0.5f + Random());
		}
	}
}
static void ModeBox(const char *map, int count, int reps) {
	box_t *boxes = malloc(count * sizeof(box_t)); double *times = malloc(reps * sizeof(double)), start;
	int list[128], i, r, n, last, total = 0; cbrush_t *brushes[256]; unsigned long long leafHash, brushHash;
	LoadCM(map);
	MakeBoxes(boxes, count);
	for (r = 0; r < reps; r++) {
		start = Now();
		for (i = 0; i < count; i++) total += CM_BoxLeafnums(boxes[i].mins, boxes[i].maxs, list, 128, &last);
		times[r] = Now() - start;
	}
	Report("box_leafnums", times, reps);
	for (r = 0; r < reps; r++) {
		start = Now();
		for (i = 0; i < count; i++) total += CM_BoxBrushes(boxes[i].mins, boxes[i].maxs, brushes, 256);
		times[r] = Now() - start;
	}
	Report("box_brushes", times, reps);
	/* Hash pass: full lists, an overflowing short list and brush indices. */
	for (i = 0; i < count; i++) {
		n = CM_BoxLeafnums(boxes[i].mins, boxes[i].maxs, list, 128, &last);
		HashInt(n); HashInt(last); Hash(list, n * sizeof(int));
		n = CM_BoxLeafnums(boxes[i].mins, boxes[i].maxs, list, 3, &last);
		HashInt(n); HashInt(last); Hash(list, n * sizeof(int));
	}
	leafHash = hashValue; hashValue = 1469598103934665603ull;
	for (i = 0; i < count; i++) {
		n = CM_BoxBrushes(boxes[i].mins, boxes[i].maxs, brushes, 256);
		HashInt(n); for (r = 0; r < n; r++) HashInt(brushes[r] - cm.brushes);
		n = CM_BoxBrushes(boxes[i].mins, boxes[i].maxs, brushes, 5);
		HashInt(n); for (r = 0; r < n; r++) HashInt(brushes[r] - cm.brushes);
	}
	brushHash = hashValue;
	printf("hash box_leafnums %016llx\nhash box_brushes %016llx\ninfo checksum_of_counts %d\n", leafHash, brushHash, total);
}

/* ---- AAS ---- */
static int BotPrintLevel;
static void QDECL BotPrint(int type, char *format, ...) {
	va_list args;
	if (type < PRT_ERROR || !BotPrintLevel) return;
	va_start(args, format); vfprintf(stderr, format, args); va_end(args);
}
static void *BotGetMemory(int size) { return malloc(size); }
static void BotFreeMemory(void *p) { free(p); }
static int BotAvailableMemory(void) { return 256 << 20; }
static void *BotHunkAlloc(int size) { return Hunk_Alloc(size, h_high); }
static struct { const byte *data; int length, offset; } botFile;
static int BotOpen(const char *qpath, fileHandle_t *file, fsMode_t mode) {
	if (mode != FS_READ) return -1;
	botFile.data = Cached(qpath, &botFile.length);
	if (!botFile.data) { *file = 0; return -1; }
	botFile.offset = 0; *file = 1;
	return botFile.length;
}
static int BotRead(void *buffer, int len, fileHandle_t f) {
	(void)f;
	if (len > botFile.length - botFile.offset) len = botFile.length - botFile.offset;
	memcpy(buffer, botFile.data + botFile.offset, len); botFile.offset += len;
	return len;
}
static int BotSeek(fileHandle_t f, long offset, int origin) {
	(void)f;
	if (origin == FS_SEEK_SET) botFile.offset = offset;
	else if (origin == FS_SEEK_CUR) botFile.offset += offset;
	else botFile.offset = botFile.length + offset;
	return 0;
}
static void BotClose(fileHandle_t f) { (void)f; }
static char *BotEntities(void) { return cm.entityString ? cm.entityString : ""; }

static void ModeAAS(const char *map, int reps) {
	double *times = malloc(reps * sizeof(double)), start;
	int area, r, cluster, updates = 0, i, n;
	char checksum[16];
	/* The entity string, and the checksum the AAS file must match. */
	Com_sprintf(checksum, sizeof(checksum), "%d", LoadCM(map));
	memset(&botimport, 0, sizeof(botimport));
	botimport.Print = BotPrint; botimport.GetMemory = BotGetMemory; botimport.FreeMemory = BotFreeMemory;
	botimport.AvailableMemory = BotAvailableMemory; botimport.HunkAlloc = BotHunkAlloc;
#ifndef Q3_PERF_RETAIL
	botimport.TryGetMemory = BotGetMemory;
#endif
	botimport.FS_FOpenFile = BotOpen; botimport.FS_Read = BotRead; botimport.FS_Seek = BotSeek; botimport.FS_FCloseFile = BotClose;
	botimport.BSPEntityData = BotEntities;
	BotPrintLevel = 1;
	AAS_Setup();
	LibVarSet("sv_mapChecksum", checksum);
	/* Each load replaces the last; its hunk use only grows the harness arena. */
	for (r = 0; r < reps; r++) {
		start = Now();
		if (AAS_LoadMap(map)) Fail("AAS_LoadMap");
		times[r] = Now() - start;
	}
	Report("aas_load", times, reps);
	AAS_ContinueInit(0);
	if (!aasworld.initialized) Fail("AAS routing did not initialize");
	for (r = 0; r < reps; r++) {
		start = Now();
		for (area = 1; area < aasworld.numareas; area++) {
			cluster = aasworld.areasettings[area].cluster;
			if (cluster <= 0) continue;
			{
				aas_routingcache_t *cache = AAS_AllocRoutingCache(aasworld.clusters[cluster].numreachabilityareas);
				if (!cache) Fail("cache allocation");
				cache->cluster = cluster; cache->areanum = area; cache->travelflags = TFL_DEFAULT; cache->starttraveltime = 1;
				AAS_UpdateAreaRoutingCache(cache);
				if (r == 0) {
					n = aasworld.clusters[cluster].numreachabilityareas;
					HashInt(area); Hash(cache->traveltimes, n * sizeof(unsigned short));
					for (i = 0; i < n; i++) if (cache->traveltimes[i]) HashInt(cache->reachabilities[i]);
					updates++;
				}
				AAS_FreeRoutingCache(cache);
			}
		}
		times[r] = Now() - start;
	}
	Report("aas_route_updates", times, reps);
	printf("hash aas_route_updates %016llx\ninfo aas_updates_per_pass %d\n", hashValue, updates);
}

int main(int argc, char **argv) {
	if (argc < 5) Fail("usage: cm|box|aas <dir> <map> <args>");
	dataDir = argv[2];
	hunk = malloc(PERF_HUNK);
	if (!hunk) Fail("hunk");
	if (!strcmp(argv[1], "cm")) ModeCM(argv[3], atoi(argv[4]));
	else if (!strcmp(argv[1], "box") && argc > 5) ModeBox(argv[3], atoi(argv[4]), atoi(argv[5]));
	else if (!strcmp(argv[1], "aas")) ModeAAS(argv[3], atoi(argv[4]));
	else if (!strcmp(argv[1], "rload")) PerfRenderer("rload", argv[3], 0, atoi(argv[4]));
	else if (!strcmp(argv[1], "rbox") && argc > 5) PerfRenderer("rbox", argv[3], atoi(argv[4]), atoi(argv[5]));
	else Fail("unknown mode");
	return 0;
}
