/* Unique pk3 reads stream through their own archive, as retail 1.32c did,
 * instead of copying whole entries into the zone (#260).
 *
 * The real files.c and unzip.c run against real pk3s.  Z_Malloc models a zone
 * with a fixed amount of free space and raises ERR_FATAL when it runs out, as
 * common.c's does.  Every streamed byte is checked against the CRC32 that
 * Python's zlib computed and against a direct unzip read of the same entry. */
#include <dirent.h>
#include <setjmp.h>
#include <stdarg.h>
#include <unistd.h>
#include "../code/qcommon/files.c"
#include "../code/qcommon/unzip.c"

#define STREAM_HANDLE_ZONE	(128 * 1024)	/* unz_s, entry reader, 64 KiB read buffer, inflate */
#define INFLATE_BLOCK_ZONE	(8 * 1024)		/* code lengths and codes, per deflate block */
#define SMALL_ZONE_FREE		(2 * 1024 * 1024)
#define MAX_OWNERS			8192
#define MAX_ENTRIES			2048

qboolean com_fullyInitialized;
cvar_t *com_journal;
fileHandle_t com_journalDataFile;
static cvar_t debugVar, restrictVar, copyVar;
static searchpath_t search;

static struct { void *p; int size; qboolean tracked; } owners[MAX_OWNERS];
static long zoneBytes, zonePeak, zoneBudget;
static int untracked;
static jmp_buf dropTarget;
static int expectDrop;

typedef struct { unsigned crc; int size; char name[MAX_ZPATH]; } entry_t;
static entry_t entries[MAX_ENTRIES];
static int numEntries;

static void Fail(const char *message) {
	fprintf(stderr, "unique stream regression failed: %s\n", message);
	exit(1);
}

static void Check(int condition, const char *message) {
	if (!condition) Fail(message);
}

void QDECL Com_Error(int level, const char *format, ...) {
	char text[1024];
	va_list args;
	va_start(args, format);
	vsnprintf(text, sizeof(text), format, args);
	va_end(args);
	if (expectDrop && level == ERR_DROP) longjmp(dropTarget, 1);
	fprintf(stderr, "unique stream regression failed: engine %s: %s\n",
			level == ERR_FATAL ? "ERR_FATAL" : "error", text);
	exit(1);
}

void QDECL Com_Printf(const char *format, ...) { (void)format; }
void QDECL Com_DPrintf(const char *format, ...) { (void)format; }

/* A zone with zoneBudget bytes free: running out is fatal, as in common.c. */
void *Z_Malloc(int size) {
	int i;
	Check(size >= 0, "non-negative zone request");
	if (!untracked && zoneBytes + size > zoneBudget) {
		Com_Error(ERR_FATAL, "Z_Malloc: failed on allocation of %i bytes from the main zone", size);
	}
	for (i = 0; i < MAX_OWNERS; i++) {
		if (!owners[i].p) {
			owners[i].p = calloc(1, size ? size : 1);
			Check(owners[i].p != NULL, "host allocation");
			owners[i].size = size;
			owners[i].tracked = !untracked;
			if (owners[i].tracked) {
				zoneBytes += size;
				if (zoneBytes > zonePeak) zonePeak = zoneBytes;
			}
			return owners[i].p;
		}
	}
	Fail("zone owner table full");
	return NULL;
}

void Z_Free(void *p) {
	int i;
	for (i = 0; i < MAX_OWNERS; i++) {
		if (owners[i].p == p && p) {
			if (owners[i].tracked) zoneBytes -= owners[i].size;
			free(p);
			owners[i].p = NULL;
			return;
		}
	}
	Fail("zone free of an unowned pointer");
}

void *Hunk_AllocateTempMemory(int size) { return malloc(size ? size : 1); }
void Hunk_FreeTempMemory(void *p) { free(p); }
void Hunk_ClearTempMemory(void) {}
void Sys_Mkdir(const char *p) { (void)p; }
void Com_Memset(void *out, int value, size_t size) { memset(out, value, size); }
void Com_Memcpy(void *out, const void *in, size_t size) { memcpy(out, in, size); }

/* The Mac port's synchronous stream shims (code/mac/mac_main.c). */
void Sys_BeginStreamedFile(fileHandle_t f, int readAhead) { (void)f; (void)readAhead; }
void Sys_EndStreamedFile(fileHandle_t f) { (void)f; }
int Sys_StreamedRead(void *buffer, int size, int count, fileHandle_t f) {
	return FS_Read(buffer, size * count, f);
}
void Sys_StreamSeek(fileHandle_t f, int offset, int origin) { FS_Seek(f, offset, origin); }
/* FS_Shutdown unregisters the filesystem commands. */
void Cmd_RemoveCommand(const char *name) { (void)name; }
/* FS_FOpenFileByMode's write modes are linked in but never used here. */
void S_ClearSoundBuffer(void) { Fail("unexpected write-mode open"); }

static unsigned Crc32(unsigned crc, const unsigned char *p, int n) {
	static unsigned table[256];
	int i, k;
	if (!table[1]) {
		for (i = 0; i < 256; i++) {
			unsigned c = i;
			for (k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320U ^ (c >> 1) : c >> 1;
			table[i] = c;
		}
	}
	crc = ~crc;
	while (n-- > 0) crc = table[(crc ^ *p++) & 255] ^ (crc >> 8);
	return ~crc;
}

static int OpenDescriptors(void) {
	DIR *dir = opendir("/proc/self/fd");
	struct dirent *e;
	int count = 0;
	Check(dir != NULL, "descriptor inventory");
	while ((e = readdir(dir)) != NULL) if (e->d_name[0] != '.') count++;
	closedir(dir);
	return count;
}

static void LoadManifest(const char *path) {
	FILE *m = fopen(path, "r");
	char line[512];
	Check(m != NULL, "manifest opens");
	numEntries = 0;
	while (fgets(line, sizeof(line), m)) {
		entry_t *e = &entries[numEntries];
		int used = 0;
		Check(numEntries < MAX_ENTRIES, "manifest fits");
		Check(sscanf(line, "%x %d %n", &e->crc, &e->size, &used) == 2 && used > 0, "manifest line");
		line[strcspn(line, "\n")] = 0;
		Q_strncpyz(e->name, line + used, sizeof(e->name));
		numEntries++;
	}
	fclose(m);
}

static const entry_t *Entry(const char *name) {
	int i;
	for (i = 0; i < numEntries; i++) if (!strcmp(entries[i].name, name)) return &entries[i];
	Fail("entry missing from manifest");
	return NULL;
}

/* The whole entry through a direct unzip handle, outside the modelled zone. */
static unsigned char *Reference(const char *pk3, const entry_t *e) {
	unzFile z;
	unsigned char *bytes = malloc(e->size ? e->size : 1);
	int got;
	untracked++;
	z = unzOpen(pk3);
	Check(z && bytes, "direct unzip reference archive opens");
	Check(unzGoToFirstFile(z) == UNZ_OK && unzLocateFile(z, e->name, 2) == UNZ_OK,
		  "direct unzip reference finds the entry");
	Check(unzOpenCurrentFile(z) == UNZ_OK, "direct unzip reference opens the entry");
	got = e->size ? unzReadCurrentFile(z, bytes, e->size) : 0;
	Check(got == e->size, "direct unzip reference reads the declared size");
	unzCloseCurrentFile(z);
	unzClose(z);
	untracked--;
	Check(Crc32(0, bytes, e->size) == e->crc, "direct unzip reference matches Python's CRC32");
	return bytes;
}

static long mountedZone;
static int mountedDescriptors;

static void Mount(const char *pk3, long budget) {
	memset(fsh, 0, sizeof(fsh));
	memset(&search, 0, sizeof(search));
	fs_searchpaths = &search;
	fs_debug = &debugVar;
	fs_restrict = &restrictVar;
	fs_copyfiles = &copyVar;
	/* The mounted directory lives in the zone already; only what follows is budgeted. */
	zoneBudget = LONG_MAX;
	search.pack = FS_LoadZipFile((char *)pk3, "test.pk3");
	Check(search.pack != NULL, "pk3 mounts");
	mountedZone = zoneBytes;
	zonePeak = zoneBytes;
	zoneBudget = zoneBytes + budget;
	mountedDescriptors = OpenDescriptors();
}

static void Unmount(void) {
	int i;
	for (i = 0; i < MAX_FILE_HANDLES; i++) Check(!fsh[i].handleFiles.file.o, "every handle closed");
	Check(zoneBytes == mountedZone, "every stream returns its zone memory");
	Check(OpenDescriptors() == mountedDescriptors, "every stream closes its own pk3 descriptor");
	unzClose(search.pack->handle);
	Z_Free(search.pack->buildBuffer);
	Z_Free(search.pack);
	search.pack = NULL;
	Check(zoneBytes == 0, "unmount releases the zone");
}

/* Opens a unique handle and checks it has a private archive and a bounded zone cost. */
static fileHandle_t OpenUnique(const entry_t *e, int open) {
	fileHandle_t f;
	long before = zoneBytes;
	Check(FS_FOpenFileRead(e->name, &f, qtrue) == e->size && f > 0, "unique open returns the entry size");
	Check(fsh[f].zipFile && fsh[f].handleFiles.unique && fsh[f].handleFiles.file.z &&
		  fsh[f].handleFiles.file.z != search.pack->handle,
		  "unique open streams through its own reopened archive, as retail did");
	Check(zoneBytes - before <= STREAM_HANDLE_ZONE,
		  "unique open costs one decoder, never a whole-entry zone copy");
	Check(OpenDescriptors() == mountedDescriptors + open, "one pk3 descriptor per unique handle");
	return f;
}

static const int chunkSizes[] = {1, 7, 4096, 65536 + 3, 300000, 17};

/* Streams a whole entry in uneven chunks, then seeks within it. */
static void StreamOne(const char *pk3, const entry_t *e, fileHandle_t f, int viaVM, int decoders) {
	unsigned char *ref = Reference(pk3, e), *chunk = malloc(300000);
	unsigned crc = 0;
	int pos = 0, c = 0, n, target;
	/* Repositioning builds a new decoder before freeing the old one. */
	long bound = (long)(decoders + 1) * STREAM_HANDLE_ZONE + INFLATE_BLOCK_ZONE;
	Check(chunk != NULL, "chunk buffer");
	while (pos < e->size) {
		int want = chunkSizes[c++ % (sizeof(chunkSizes) / sizeof(chunkSizes[0]))];
		if (want > e->size - pos) want = e->size - pos;
		n = viaVM ? FS_Read2(chunk, want, f) : FS_Read(chunk, want, f);
		Check(n == want, "streamed read returns every requested byte");
		Check(!memcmp(chunk, ref + pos, n), "streamed bytes match a direct unzip read");
		crc = Crc32(crc, chunk, n);
		pos += n;
		Check(FS_FTell(f) == pos, "tell follows the stream");
		Check(zonePeak - mountedZone <= bound, "streaming zone use stays bounded by the open decoders");
	}
	Check(crc == e->crc && pos == e->size, "streamed entry matches Python's CRC32 and size");
	Check(FS_Read(chunk, 1, f) == 0, "stable end of entry");
	if (e->size > 0) {
		target = e->size / 3;
		FS_Seek(f, target, FS_SEEK_SET);
		n = e->size - target < 4096 ? e->size - target : 4096;
		Check(FS_FTell(f) == target && FS_Read(chunk, n, f) == n && !memcmp(chunk, ref + target, n),
			  "backward seek re-reads identical bytes");
		target = e->size - 1;
		FS_Seek(f, target, FS_SEEK_SET);
		Check(FS_Read(chunk, 1, f) == 1 && chunk[0] == ref[target], "forward seek reaches the last byte");
	}
	Check(zonePeak - mountedZone <= bound, "seeking zone use stays bounded by the open decoders");
	free(chunk);
	free(ref);
}

/* Every entry in the pk3, one unique handle at a time, inside a small zone. */
static void Stream(const char *pk3) {
	int i;
	Mount(pk3, SMALL_ZONE_FREE);
	for (i = 0; i < numEntries; i++) {
		fileHandle_t f = OpenUnique(&entries[i], 1);
		StreamOne(pk3, &entries[i], f, 0, 1);
		FS_FCloseFile(f);
		Check(zoneBytes == mountedZone && OpenDescriptors() == mountedDescriptors,
			  "closing a unique stream releases its decoder and descriptor");
	}
	Check(zonePeak - mountedZone <= 2 * STREAM_HANDLE_ZONE + INFLATE_BLOCK_ZONE,
		  "peak zone use stays one decoder, two while a seek replaces it");
	Unmount();
}

/* VM reads (FS_FOpenFileByMode FS_READ) get unique handles again, as in retail. */
static void VMRead(const char *pk3) {
	const entry_t *largest = &entries[0];
	fileHandle_t f, shared;
	unsigned char bytes[64];
	int i;
	for (i = 1; i < numEntries; i++) if (entries[i].size > largest->size) largest = &entries[i];
	Mount(pk3, SMALL_ZONE_FREE);
	/* A shared reader mid-entry must not disturb the VM's stream, nor it the reader. */
	Check(FS_FOpenFileRead(largest->name, &shared, qfalse) == largest->size, "shared reader opens");
	Check(FS_Read(bytes, sizeof(bytes), shared) == sizeof(bytes), "shared reader reads");
	Check(FS_FOpenFileByMode(largest->name, &f, FS_READ) == largest->size && f > 0,
		  "VM read open returns the entry size");
	Check(fsh[f].handleFiles.unique && fsh[f].handleFiles.file.z != search.pack->handle &&
		  fsh[f].streamed, "VM read gets a unique streamed handle (retail passes qtrue)");
	StreamOne(pk3, largest, f, 1, 2);
	{
		unsigned char *ref = Reference(pk3, largest);
		Check(FS_Read(bytes, sizeof(bytes), shared) == sizeof(bytes) &&
			  !memcmp(bytes, ref + sizeof(bytes), sizeof(bytes)), "shared reader continues unchanged");
		free(ref);
	}
	FS_FCloseFile(f);
	FS_FCloseFile(shared);
	Unmount();
}

/* Every free handle slot holds a unique stream at once; reads interleave. */
static void Concurrent(const char *pk3) {
	fileHandle_t handles[MAX_FILE_HANDLES];
	const entry_t *which[MAX_FILE_HANDLES];
	unsigned char *refs[MAX_ENTRIES], chunk[32768];
	int pos[MAX_FILE_HANDLES], limit[MAX_FILE_HANDLES], order[MAX_ENTRIES];
	int count = 0, i, j, active, extra;
	/* Entries largest first, so the biggest RoQ/music streams take part. */
	for (i = 0; i < numEntries; i++) order[i] = i;
	for (i = 0; i < numEntries; i++)
		for (j = i + 1; j < numEntries; j++)
			if (entries[order[j]].size > entries[order[i]].size) { int t = order[i]; order[i] = order[j]; order[j] = t; }
	memset(refs, 0, sizeof(refs));
	Mount(pk3, (long)(MAX_FILE_HANDLES - 1) * STREAM_HANDLE_ZONE);
	for (i = 0; i < MAX_FILE_HANDLES - 1; i++) {
		int idx = order[i % (numEntries < 48 ? numEntries : 48)];
		which[i] = &entries[idx];
		if (!refs[idx]) refs[idx] = Reference(pk3, which[i]);
		handles[i] = OpenUnique(which[i], i + 1);
		pos[i] = 0;
		/* Bound the work: every stream reads at least 2 MiB or its whole entry. */
		limit[i] = which[i]->size < 2 * 1024 * 1024 ? which[i]->size : 2 * 1024 * 1024;
		count++;
	}
	for (i = 0; i < count; i++)
		for (j = 0; j < i; j++)
			Check(fsh[handles[i]].handleFiles.file.z != fsh[handles[j]].handleFiles.file.z,
				  "every concurrent handle owns its archive");
	/* The table is full: one more open is retail's ERR_DROP, not a fatal error. */
	expectDrop = 1;
	if (!setjmp(dropTarget)) {
		FS_FOpenFileRead(which[0]->name, &extra, qtrue);
		Fail("an open past the handle limit must drop");
	}
	expectDrop = 0;
	Check(zoneBytes - mountedZone <= (long)count * STREAM_HANDLE_ZONE &&
		  OpenDescriptors() == mountedDescriptors + count,
		  "refused open leaves zone and descriptors unchanged");
	do {
		active = 0;
		for (i = 0; i < count; i++) {
			int want = (int)sizeof(chunk) - i * 97, n, idx = which[i] - entries;
			if (pos[i] >= limit[i]) continue;
			if (want > limit[i] - pos[i]) want = limit[i] - pos[i];
			n = FS_Read(chunk, want, handles[i]);
			Check(n == want && !memcmp(chunk, refs[idx] + pos[i], n),
				  "interleaved unique streams return identical bytes");
			pos[i] += n;
			active = 1;
		}
	} while (active);
	for (i = count - 1; i >= 0; i--) FS_FCloseFile(handles[i]);
	for (i = 0; i < numEntries; i++) free(refs[i]);
	Unmount();
}

/* A deflate stream that breaks halfway returns its good prefix, then stops. */
static void Corrupt(const char *pk3, const char *prefixPath) {
	const entry_t *bad = Entry("corrupt.bin"), *good = Entry("good.bin");
	unsigned char *prefix, *ref, chunk[4096];
	fileHandle_t f, g;
	long prefixSize;
	int pos = 0, gpos = 0, n;
	FILE *p = fopen(prefixPath, "rb");
	Check(p && !fseek(p, 0, SEEK_END) && (prefixSize = ftell(p)) > 0 && !fseek(p, 0, SEEK_SET),
		  "corrupt prefix fixture");
	prefix = malloc(prefixSize);
	Check(prefix && fread(prefix, 1, prefixSize, p) == (size_t)prefixSize, "corrupt prefix reads");
	fclose(p);
	Mount(pk3, SMALL_ZONE_FREE);
	ref = Reference(pk3, good);
	f = OpenUnique(bad, 1);
	g = OpenUnique(good, 2);
	for (;;) {
		n = FS_Read(chunk, sizeof(chunk), f);
		if (n <= 0) break;
		Check(pos + n <= prefixSize && !memcmp(chunk, prefix + pos, n),
			  "corrupt entry yields only its valid prefix");
		pos += n;
		if (gpos < good->size) {
			int want = good->size - gpos < (int)sizeof(chunk) ? good->size - gpos : (int)sizeof(chunk);
			Check(FS_Read(chunk, want, g) == want && !memcmp(chunk, ref + gpos, want),
				  "a sibling stream is unaffected by the corrupt one");
			gpos += want;
		}
	}
	/* zlib 1.1 flushes its 32 KiB window in batches, so output inflated
	 * just before the error is not returned. */
	if (!(pos >= prefixSize - 65536 && pos < bad->size)) {
		fprintf(stderr, "corrupt entry stopped after %d of %d bytes (valid prefix %ld)\n",
				pos, bad->size, prefixSize);
		Fail("corrupt entry stops at the damaged block without a full read");
	}
	Check(FS_Read(chunk, sizeof(chunk), f) <= 0, "corrupt entry stays at its error");
	FS_FCloseFile(f);
	while (gpos < good->size) {
		int want = good->size - gpos < (int)sizeof(chunk) ? good->size - gpos : (int)sizeof(chunk);
		Check(FS_Read(chunk, want, g) == want && !memcmp(chunk, ref + gpos, want),
			  "sibling stream finishes after the corrupt one closes");
		gpos += want;
	}
	FS_FCloseFile(g);
	free(ref);
	free(prefix);
	Unmount();
}

/* The pk3 is cut short after mounting; streams end early without fatal errors. */
static void Truncated(const char *pk3, const char *copy) {
	const entry_t *big = Entry("video/big.roq");
	unsigned char *ref, chunk[65536];
	unsigned long entryStart;
	FILE *in = fopen(pk3, "rb"), *out = fopen(copy, "wb");
	fileHandle_t f;
	int pos = 0, n, i, failed = 0;
	size_t got;
	Check(in && out, "truncation copy opens");
	while ((got = fread(chunk, 1, sizeof(chunk), in)) > 0) Check(fwrite(chunk, 1, got, out) == got, "copy");
	fclose(in);
	fclose(out);
	Mount(copy, SMALL_ZONE_FREE);
	ref = Reference(copy, big);
	Check(unzGoToFirstFile(search.pack->handle) == UNZ_OK &&
		  unzLocateFile(search.pack->handle, big->name, 2) == UNZ_OK, "locate the large entry");
	entryStart = ((unz_s *)search.pack->handle)->cur_file_info_internal.offset_curfile;
	f = OpenUnique(big, 1);
	while (pos < 1024 * 1024 && (n = FS_Read(chunk, sizeof(chunk), f)) > 0) {
		Check(!memcmp(chunk, ref + pos, n), "bytes before the cut are identical");
		pos += n;
	}
	/* The media goes bad mid-stream: the local header is tiny, so half the
	 * entry past it is inside its data. */
	Check(truncate(copy, entryStart + big->size / 2) == 0, "pk3 truncates inside the large entry");
	while ((n = FS_Read(chunk, sizeof(chunk), f)) > 0) {
		Check(!memcmp(chunk, ref + pos, n), "bytes before the cut are identical");
		pos += n;
	}
	/* Data still in the read buffer and inflate window at the cut is lost. */
	if (!(pos < big->size && pos > big->size / 2 - 4 * 65536)) {
		fprintf(stderr, "truncated stream read %d of %d bytes\n", pos, big->size);
		Fail("truncated stream ends near the cut, short of the declared size");
	}
	FS_FCloseFile(f);
	/* Every entry now fails to open (the central directory is gone) or reads
	 * short; none is fatal and none leaks. */
	for (i = 0; i < numEntries; i++) {
		fileHandle_t g;
		int total = 0;
		if (FS_FOpenFileRead(entries[i].name, &g, qtrue) < 0) { failed++; continue; }
		while ((n = FS_Read(chunk, sizeof(chunk), g)) > 0) total += n;
		if (total < entries[i].size) failed++;
		FS_FCloseFile(g);
		Check(zoneBytes == mountedZone, "each failed stream releases its decoder");
	}
	Check(failed > 0, "entries past the cut do not read in full");
	free(ref);
	Unmount();
}

/* If the pk3 cannot be reopened, the unique read shares the mounted archive. */
static void Fallback(const char *pk3, const char *moved) {
	const entry_t *e = Entry("music/track.wav");
	fileHandle_t f, other;
	Mount(pk3, SMALL_ZONE_FREE);
	Check(rename(pk3, moved) == 0, "pk3 moves away after mounting");
	Check(FS_FOpenFileRead(e->name, &f, qtrue) == e->size && f > 0, "unique open still succeeds");
	Check(fsh[f].handleFiles.file.z == search.pack->handle && !fsh[f].handleFiles.unique,
		  "unique open falls back to the mounted archive");
	Check(zoneBytes - mountedZone <= STREAM_HANDLE_ZONE, "fallback copies no whole entry");
	/* An interleaved shared reader forces the fallback stream to re-position. */
	Check(FS_FOpenFileRead("video/stored.roq", &other, qfalse) > 0, "second shared reader");
	{
		unsigned char b[16];
		Check(FS_Read(b, sizeof(b), other) == sizeof(b), "second shared reader reads");
	}
	Check(rename(moved, pk3) == 0, "pk3 moves back for the reference reader");
	StreamOne(pk3, e, f, 0, 1);
	FS_FCloseFile(other);
	FS_FCloseFile(f);
	Check(search.pack->handle != NULL, "closing the fallback keeps the mounted archive");
	Unmount();
}

/* A searchpath owned by the zone, as FS_Startup builds it, so that
 * FS_Shutdown can free it. */
static void MountOwned(const char *pk3) {
	searchpath_t *path = Z_Malloc(sizeof(*path));
	path->pack = FS_LoadZipFile((char *)pk3, "test.pk3");
	Check(path->pack != NULL, "owned pk3 mounts");
	fs_searchpaths = path;
}

/* FS_Restart (FS_ConditionalRestart from a demo's gamestate) runs FS_Shutdown
 * and then mounts the paks again.  A unique stream owns its reopened archive
 * and continues, as in retail; VM handles and handles reading through the
 * freed pack are closed. */
static void Restart(const char *pk3, const char *moved) {
	const entry_t *e = Entry("music/track.wav"), *vm = Entry("video/stored.roq");
	unsigned char *ref = Reference(pk3, e), *chunk = malloc(100000), b[16];
	fileHandle_t unique, byMode, shared, fallback, later;
	unz_s *archive;
	int pos = 0, n;
	Check(chunk != NULL, "chunk buffer");
	memset(fsh, 0, sizeof(fsh));
	fs_debug = &debugVar;
	fs_restrict = &restrictVar;
	fs_copyfiles = &copyVar;
	zoneBudget = LONG_MAX;
	mountedDescriptors = OpenDescriptors();
	MountOwned(pk3);
	Check(FS_FOpenFileRead(e->name, &unique, qtrue) == e->size && fsh[unique].handleFiles.unique,
		  "unique stream opens, as CL_PlayDemo opens a demo");
	Check(FS_Read(chunk, 100000, unique) == 100000 && !memcmp(chunk, ref, 100000),
		  "unique stream reads before the restart");
	pos = 100000;
	archive = fsh[unique].handleFiles.file.z;
	Check(FS_FOpenFileByMode(vm->name, &byMode, FS_READ) == vm->size && FS_Read2(b, sizeof(b), byMode) == sizeof(b),
		  "VM read handle opens");
	Check(FS_FOpenFileRead("maps/m00.bin", &shared, qfalse) > 0 && FS_Read(b, sizeof(b), shared) == sizeof(b),
		  "shared handle opens");
	Check(rename(pk3, moved) == 0, "pk3 moves away");
	Check(FS_FOpenFileRead("maps/m01.bin", &fallback, qtrue) > 0 && !fsh[fallback].handleFiles.unique,
		  "fallback handle shares the mounted pack");
	Check(rename(moved, pk3) == 0, "pk3 moves back");

	FS_Shutdown(qfalse);

	Check(!fs_searchpaths, "the restart frees the search paths");
	Check(!fsh[byMode].handleFiles.file.o, "the restart closes VM handles, as retail did");
	Check(!fsh[shared].handleFiles.file.o && !fsh[fallback].handleFiles.file.o,
		  "the restart closes handles that read through the freed pack");
	Check(fsh[unique].handleFiles.file.z == archive && fsh[unique].handleFiles.unique &&
		  fsh[unique].zipOffset == pos, "the restart keeps the unique stream and its position");
	MountOwned(pk3);
	Check(FS_FOpenFileRead("maps/m02.bin", &later, qtrue) > 0 && later != unique,
		  "a new open after the restart does not take the stream's slot");
	FS_FCloseFile(later);
	while ((n = FS_Read(chunk, 65536, unique)) > 0) {
		Check(pos + n <= e->size && !memcmp(chunk, ref + pos, n),
			  "the unique stream continues byte-identically after the restart");
		pos += n;
	}
	Check(pos == e->size, "the unique stream reads to its end after the restart");
	FS_FCloseFile(unique);
	FS_Shutdown(qfalse);
	Check(zoneBytes == 0, "every zone owner is released");
	Check(OpenDescriptors() == mountedDescriptors, "every descriptor is closed");
	mountedZone = 0;
	free(chunk);
	free(ref);
}

int main(int argc, char **argv) {
	if (argc < 4) Fail("usage: <mode> <pk3> <manifest> [extra]");
	LoadManifest(argv[3]);
	if (!strcmp(argv[1], "stream")) { Stream(argv[2]); VMRead(argv[2]); }
	else if (!strcmp(argv[1], "concurrent")) Concurrent(argv[2]);
	else if (!strcmp(argv[1], "corrupt") && argc > 4) Corrupt(argv[2], argv[4]);
	else if (!strcmp(argv[1], "truncated") && argc > 4) Truncated(argv[2], argv[4]);
	else if (!strcmp(argv[1], "fallback") && argc > 4) Fallback(argv[2], argv[4]);
	else if (!strcmp(argv[1], "restart") && argc > 4) Restart(argv[2], argv[4]);
	else Fail("unknown mode");
	printf("unique stream %s: %d entries, peak zone %ld bytes above the mounted pk3\n",
		   argv[1], numEntries, zonePeak - mountedZone);
	return 0;
}
