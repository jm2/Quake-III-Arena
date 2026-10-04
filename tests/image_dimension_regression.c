/* Issue #42: every file image loader rejects empty, overflowing or oversized declared sizes,
   and declared sizes larger than the file, with a warning and no image: no engine error and no
   allocation larger than the biggest accepted image (4096x4096 RGBA). */
#include "../code/renderer/tr_image_tga.c"
#include "../code/renderer/tr_image_bmp.c"
#include "../code/renderer/tr_image_pcx.c"
#include "../code/renderer/tr_image_jpeg.c"
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Independent of the header so the runner also builds, and fails, against older loaders. */
#define LARGEST_IMAGE_BYTES (4096 * 4096 * 4)

refimport_t ri;
static byte *fixture;
static int fixtureSize, fixtureCapacity, reads, frees, warnings, live, failures, inCase;
static byte *fileBuffer;
static const char *currentCase = "";
static jmp_buf caseJump;

/** Report a failed case and abandon it (its allocations leak); setup failures stop the run. */
static void Fail( const char *message ) {
	fprintf(stderr, "image dimension regression failed (%s): %s\n", currentCase, message);
	if ( !inCase ) exit(1);
	failures++; longjmp(caseJump, 1);
}
static void Check( int ok, const char *message ) { if ( !ok ) Fail(message); }
/** The decoded file is an exact-size heap copy, so ASan sees any read past its end. */
static int Read( const char *name, void **buffer ) {
	(void)name; reads++;
	fileBuffer = malloc(fixtureSize ? fixtureSize : 1); Check(fileBuffer != NULL, "file allocation");
	memcpy(fileBuffer, fixture, fixtureSize); *buffer = fileBuffer; return fixtureSize;
}
static void FreeFile( void *buffer ) { Check(buffer == fileBuffer, "file ownership"); free(fileBuffer); fileBuffer = NULL; frees++; }
/** A request above the largest accepted image is the bug: never make it. */
static void *Allocate( int size ) {
	char message[128]; void *pointer;
	if ( size <= 0 || size > LARGEST_IMAGE_BYTES ) { snprintf(message, sizeof(message), "allocation of %d bytes from a declared size", size); Fail(message); }
	pointer = malloc(size); Check(pointer != NULL, "host allocation"); live++; return pointer;
}
static void FreeAllocation( void *pointer ) { Check(pointer != NULL && live > 0, "free ownership"); free(pointer); live--; }
static void QDECL Error( int level, const char *format, ... ) {
	char message[1024], report[1100]; va_list args;
	va_start(args, format); vsnprintf(message, sizeof(message), format, args); va_end(args);
	snprintf(report, sizeof(report), "engine error %d for bad image data: %s", level, message); Fail(report);
}
static void QDECL Print( int level, const char *format, ... ) {
	(void)format;
	if ( level == PRINT_DEVELOPER ) return;
	Check(!fileBuffer, "file released before the warning"); warnings++;
}
static void Write( const char *name, const void *buffer, int length ) {
	(void)name; Check(length > 0 && length <= fixtureCapacity, "encoded fixture size");
	memcpy(fixture, buffer, length); fixtureSize = length;
}

static void Reserve( int size ) {
	if ( size <= fixtureCapacity ) return;
	fixture = realloc(fixture, size); Check(fixture != NULL, "fixture allocation"); fixtureCapacity = size;
}
static void Put( int position, byte value ) { Reserve(position + 1); fixture[position] = value; if ( position >= fixtureSize ) fixtureSize = position + 1; }
static void LE( int position, unsigned int value, int bytes ) { int i; for ( i = 0; i < bytes; i++ ) Put(position + i, (byte)(value >> (8 * i))); }

typedef void (*loader_t)( const char *name, byte **pic, int *width, int *height );

/** Start a case from a clean slate even if the previous one was abandoned. */
static void Begin( const char *name ) {
	if ( fileBuffer ) { free(fileBuffer); fileBuffer = NULL; }
	currentCase = name; reads = frees = warnings = live = 0; inCase = 1;
}

/** The loader must publish nothing, free the file, warn, and leave no allocation behind. */
static void Reject( loader_t loader, const char *name ) {
	static byte *pic; static int width, height;
	pic = (byte *)1; width = height = -1; Begin(name);
	if ( !setjmp(caseJump) ) {
		loader(name, &pic, &width, &height);
		Check(!pic && !width && !height, "rejected image published");
		Check(reads == 1 && frees == 1 && !fileBuffer && !live, "ownership after rejection");
		Check(warnings == 1, "rejection warned");
	}
	inCase = 0;
}
/** A valid image loads with its declared size and every pixel equal to rgba. */
static void Accept( loader_t loader, const char *name, int columns, int rows, const byte *rgba ) {
	static byte *pic; static int width, height, i;
	pic = NULL; width = height = 0; Begin(name);
	if ( !setjmp(caseJump) ) {
		loader(name, &pic, &width, &height);
		Check(pic && width == columns && height == rows && !warnings && live == 1, "valid image loaded");
		for ( i = 0; i < columns * rows; i++ ) if ( memcmp(pic + i * 4, rgba, 4) ) Check(0, "valid image pixels");
		FreeAllocation(pic);
	}
	inCase = 0;
}

/** Uniform 24-bit run-length TGA: 4 bytes per 128 pixels, so a small file can declare a huge image. */
static void TGA( int type, unsigned int columns, unsigned int rows, unsigned int pixels ) {
	unsigned int count;
	fixtureSize = 0; Reserve(18 + (pixels / 128 + 1) * 4); memset(fixture, 0, 18); fixtureSize = 18;
	fixture[2] = type; LE(12, columns, 2); LE(14, rows, 2); fixture[16] = 24;
	while ( pixels ) {
		count = pixels > 128 ? 128 : pixels;
		Put(fixtureSize, 0x80 | (count - 1)); Put(fixtureSize, 30); Put(fixtureSize, 20); Put(fixtureSize, 10);
		pixels -= count;
	}
}
/** 8-bit BMP with a 256-colour palette and every index 0; the payload holds rows of the given width. */
static void BMP( unsigned int columns, unsigned int rawHeight, unsigned int payloadRows ) {
	unsigned int size = 54 + 1024 + (payloadRows ? ((columns + 3) & ~3u) * payloadRows : 0);
	fixtureSize = 0; Reserve(size); memset(fixture, 0, size); fixtureSize = size;
	fixture[0] = 'B'; fixture[1] = 'M'; LE(2, size, 4); LE(10, 54 + 1024, 4); LE(14, 40, 4);
	LE(18, columns, 4); LE(22, rawHeight, 4); LE(26, 1, 2); LE(28, 8, 2);
	fixture[54] = 10; fixture[55] = 20; fixture[56] = 30;
}
/**
 * Version 5 8-bit PCX whose scanlines are runs of colour 0, then the 0x0c palette marker.
 * Like retail 1.32c (#475), rows are xmax+1 pixels whatever xmin is.
 */
static void PCX( unsigned int xmin, unsigned int xmax, unsigned int ymax, unsigned int lines ) {
	unsigned int bytesPerLine = xmax + 1, line, left, run;
	fixtureSize = 0; Reserve(128 + 769); memset(fixture, 0, 128); fixtureSize = 128;
	fixture[0] = 0x0a; fixture[1] = 5; fixture[2] = 1; fixture[3] = 8;
	LE(4, xmin, 2); LE(6, 0, 2); LE(8, xmax, 2); LE(10, ymax, 2); fixture[65] = 1; LE(66, bytesPerLine, 2);
	for ( line = 0; line < lines; line++ ) {
		for ( left = bytesPerLine; left; left -= run ) { run = left > 63 ? 63 : left; Put(fixtureSize, 0xc0 | run); Put(fixtureSize, 0); }
	}
	Put(fixtureSize, 0x0c); Put(fixtureSize, 30); Put(fixtureSize, 20); Put(fixtureSize, 10);
	Reserve(fixtureSize + 765); memset(fixture + fixtureSize, 0, 765); fixtureSize += 765;
}
/** Overwrite the frame size in a JPEG made by the real encoder. */
static void JPEGSize( const byte *original, int originalSize, unsigned int columns, unsigned int rows ) {
	int i;
	memcpy(fixture, original, originalSize); fixtureSize = originalSize;
	for ( i = 0; i + 8 < fixtureSize; i++ ) if ( fixture[i] == 0xff && fixture[i + 1] == 0xc0 ) break;
	Check(i + 8 < fixtureSize, "baseline SOF in encoded fixture");
	fixture[i + 5] = rows >> 8; fixture[i + 6] = rows; fixture[i + 7] = columns >> 8; fixture[i + 8] = columns;
}

int main( void ) {
	static const byte tgaColor[4] = { 10, 20, 30, 255 }, paletteColor[4] = { 30, 20, 10, 255 };
	byte gray[8 * 8 * 4], *original;
	int originalSize, i;
	ri.FS_ReadFile = Read; ri.FS_FreeFile = FreeFile; ri.FS_WriteFile = Write; ri.Malloc = Allocate; ri.TryMalloc = Allocate; ri.Free = FreeAllocation;
	ri.Error = Error; ri.Printf = Print;

	/* TGA: run-length data makes the declared size nearly free for the file. */
	TGA(10, 4097, 4096, 4097u * 4096u); Reject(R_LoadTGA, "TGA 4097x4096 (one row over the cap), complete RLE payload");
	TGA(10, 65535, 8191, 65535u * 8191u); Reject(R_LoadTGA, "TGA 65535x8191 (2 GiB RGBA), complete RLE payload");
	TGA(10, 65535, 65535, 0); Reject(R_LoadTGA, "TGA 65535x65535 (w*h*4 overflows 32 bits), header only");
	TGA(10, 65535, 257, 65535u * 257u); Reject(R_LoadTGA, "TGA 65535x257 over the pixel cap");
	TGA(10, 0, 16, 0); Reject(R_LoadTGA, "TGA zero width");
	TGA(10, 16, 0, 0); Reject(R_LoadTGA, "TGA zero height");
	TGA(10, 64, 64, 64 * 64 - 1); Reject(R_LoadTGA, "TGA RLE payload one pixel short of its declared size");
	TGA(2, 64, 64, 0); for ( i = 0; i < 64 * 64 * 3 - 1; i++ ) Put(fixtureSize, 7); Reject(R_LoadTGA, "TGA raw payload one byte short of its declared size");
	TGA(10, 4096, 4096, 4096u * 4096u); Accept(R_LoadTGA, "TGA 4096x4096 at the cap", 4096, 4096, tgaColor);
	TGA(10, 65535, 256, 65535u * 256u); Accept(R_LoadTGA, "TGA 65535x256 within the cap", 65535, 256, tgaColor);

	/* BMP: raw rows must be present, so the file size bounds what can be declared. */
	BMP(4100, 4096, 4096); Reject(R_LoadBMP, "BMP 4100x4096 8-bit over the cap, complete payload");
	BMP(0, 16, 16); Reject(R_LoadBMP, "BMP zero width");
	BMP(16, 0, 0); Reject(R_LoadBMP, "BMP zero height");
	BMP(16, 0x80000000u, 1); Reject(R_LoadBMP, "BMP top-down height -2^31");
	BMP(0x7fffffffu, 1, 0); Reject(R_LoadBMP, "BMP width 2^31-1");
	BMP(0x40000000u, 4, 0); Reject(R_LoadBMP, "BMP 2^30x4 (w*h*4 overflows 32 bits)");
	BMP(64, 64, 63); Reject(R_LoadBMP, "BMP pixel rows one short of its declared height");
	BMP(64, 64, 64); LE(2, fixtureSize + 1, 4); Reject(R_LoadBMP, "BMP header file size larger than the file");
	BMP(4096, 4096, 4096); Accept(R_LoadBMP, "BMP 4096x4096 8-bit at the cap", 4096, 4096, paletteColor);

	/* PCX keeps the retail 1024-per-side limit. */
	PCX(0, 1024, 0, 1); Reject(R_LoadPCX, "PCX 1025 wide");
	PCX(0, 0, 1024, 1025); Reject(R_LoadPCX, "PCX 1025 tall");
	/* Retail ignores xmin (#475): xmax below xmin is a 6x1 image, never a negative or wrapped size. */
	PCX(10, 5, 0, 1); Accept(R_LoadPCX, "PCX xmax below xmin sized from xmax", 6, 1, paletteColor);
	/* Retail lets a short stream run on into the palette; a missing row longer than it reads past the file. */
	PCX(0, 1023, 63, 63); Reject(R_LoadPCX, "PCX scanlines one short of its declared height");
	PCX(0, 1023, 1023, 1024); Accept(R_LoadPCX, "PCX 1024x1024 at the retail limit", 1024, 1024, paletteColor);

	/* JPEG: a valid stream from the real encoder with its frame size rewritten. */
	for ( i = 0; i < 8 * 8; i++ ) { gray[i * 4] = gray[i * 4 + 1] = gray[i * 4 + 2] = 128; gray[i * 4 + 3] = 255; }
	Reserve(65536); fixtureSize = 0; currentCase = "JPEG fixture"; SaveJPG("fixture.jpg", 100, 8, 8, gray);
	Check(fixtureSize > 0 && !live, "encoded JPEG fixture");
	originalSize = fixtureSize; original = malloc(originalSize); Check(original != NULL, "JPEG copy"); memcpy(original, fixture, originalSize);
	JPEGSize(original, originalSize, 4097, 4096); Reject(R_LoadJPG, "JPEG 4097x4096 over the cap");
	JPEGSize(original, originalSize, 65000, 65000); Reject(R_LoadJPG, "JPEG 65000x65000");
	JPEGSize(original, originalSize, 65500, 257); Reject(R_LoadJPG, "JPEG 65500x257 within libjpeg's limit, over the pixel cap");
	JPEGSize(original, originalSize, 8, 8); Accept(R_LoadJPG, "JPEG 8x8", 8, 8, gray);
	free(original); free(fixture);
	if ( failures ) { fprintf(stderr, "%d image dimension case(s) failed (issue #42)\n", failures); return 1; }
	puts("Image loader dimension regressions passed (issue #42)");
	return 0;
}
