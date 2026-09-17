/* Issue #42: actual PCX loader, exact input/output allocations, padding and bounded RLE. */
#include "../code/renderer/tr_image_pcx.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

refimport_t ri;
static byte fixture[131072];
static int fixtureSize, encodedEnd, inputAlignment, reads, frees, allocations, warnings, failAllocation, missing, reportedNegative;
static byte *fileAllocation, *fileBuffer, *outputAllocation;

/** Stop on wrong pixels, allocation bounds, ownership or publication. */
static void Check( int ok, const char *message ) { if(!ok) { fprintf(stderr,"PCX regression failed: %s\n",message); exit(1); } }
/** Return no trailing allocation slack at any of four input alignments. */
static int Read( const char *name, void **buffer ) {
	(void)name; reads++;
	if(missing) { *buffer=NULL; return -1; }
	fileAllocation=malloc(fixtureSize+inputAlignment ? fixtureSize+inputAlignment : 1); Check(fileAllocation!=NULL,"file allocation");
	fileBuffer=fileAllocation+inputAlignment; memcpy(fileBuffer,fixture,fixtureSize); *buffer=fileBuffer;
	return reportedNegative?-1:fixtureSize;
}
static void FreeFile( void *buffer ) { Check(fileAllocation && buffer==fileBuffer,"file ownership"); free(fileAllocation); fileAllocation=fileBuffer=NULL; frees++; }
/** Allocate exactly the claimed output and refuse sizes outside the retained native image cap. */
static void *Allocate( int size ) {
	allocations++; Check(size>0 && size<=1024*1024*4 && !outputAllocation,"output allocation bounds");
	if(failAllocation) return NULL;
	outputAllocation=malloc(size); Check(outputAllocation!=NULL,"output allocation"); memset(outputAllocation,0xcd,size); return outputAllocation;
}
static void FreeOutput( void *buffer ) { Check(buffer==outputAllocation,"output ownership"); free(buffer); outputAllocation=NULL; }
/** Invalid PCX data remains nonfatal, with all ownership released before its warning. */
static void QDECL Print( int level, const char *format, ... ) {
	(void)format; Check(level==PRINT_ALL && reads==frees && !fileAllocation && !outputAllocation,"ownership before warning"); warnings++;
}
static void QDECL Error( int level, const char *format, ... ) { (void)level; (void)format; Check(0,"PCX error became fatal"); }
static void LE( int position, unsigned int value ) { fixture[position]=value; fixture[position+1]=value>>8; }
/** Build a standard single-plane, 8-bit version-5 header with explicit origins and row stride. */
static void Header( unsigned int columns, unsigned int rows, unsigned int bytesPerLine, unsigned int xmin, unsigned int ymin ) {
	memset(fixture,0,sizeof(fixture)); encodedEnd=128; fixtureSize=0;
	reads=frees=allocations=warnings=failAllocation=missing=reportedNegative=0;
	fixture[0]=10; fixture[1]=5; fixture[2]=1; fixture[3]=8;
	LE(4,xmin); LE(6,ymin); LE(8,xmin+columns-1); LE(10,ymin+rows-1); fixture[65]=1; LE(66,bytesPerLine);
}
static void Encoded( byte value ) { Check(encodedEnd<(int)sizeof(fixture)-769,"fixture capacity"); fixture[encodedEnd++]=value; }
static void Run( int count, byte value ) { Check(count>0 && count<=63,"fixture run"); Encoded(0xc0|count); Encoded(value); }
/** Choose a palette with three independent channels and a fixed, non-marker leading byte. */
static void Footer( void ) {
	int i; Encoded(12); fixtureSize=encodedEnd+768; Check(fixtureSize<=(int)sizeof(fixture),"footer capacity");
	for(i=0;i<256;i++) { fixture[encodedEnd+i*3]=i; fixture[encodedEnd+i*3+1]=255-i; fixture[encodedEnd+i*3+2]=i/2; }
}
static void Golden( byte *output, int index ) { output[0]=index; output[1]=255-index; output[2]=index/2; output[3]=255; }
/** Assert the complete decoded image and a single allocation, including optional dimensions. */
static void Valid( const byte *golden, int columns, int rows, int nullable ) {
	byte *pic=(byte *)1; int width=-1,height=-1;
	reads=frees=allocations=warnings=0;
	R_LoadPCX("test.pcx",&pic,nullable?NULL:&width,nullable?NULL:&height);
	Check(pic && pic==outputAllocation && !memcmp(pic,golden,columns*rows*4),"golden RGBA pixels");
	Check(nullable || (width==columns && height==rows),"published dimensions");
	Check(reads==1 && frees==1 && allocations==1 && !warnings && !fileAllocation,"success ownership"); FreeOutput(pic);
}
/** Bad input must fail preflight without a partial image, allocation or leaked file. */
static void Reject( int expectedAllocations ) {
	byte *pic=(byte *)1; int width=-1,height=-1;
	reads=frees=allocations=warnings=0; R_LoadPCX("bad.pcx",&pic,&width,&height);
	Check(!pic && !width && !height && reads==1 && frees==1 && allocations==expectedAllocations && warnings==1 && !fileAllocation,"rejected state");
}
/** Every prefix of a complete standard file loses either header, encoded rows or palette marker. */
static void Truncations( int complete ) { int i; for(i=0;i<complete;i++) { fixtureSize=i; Reject(0); } fixtureSize=complete; }

/** Cover literal/run pixels, row padding, origins, exact boundaries and malformed metadata. */
int main( void ) {
	int alignment,i,j,remaining;
	byte golden[256*4], *large, *pic;
	ri.FS_ReadFile=Read; ri.FS_FreeFile=FreeFile; ri.Malloc=Allocate; ri.Free=FreeOutput; ri.Printf=Print; ri.Error=Error;
	for(alignment=0;alignment<4;alignment++) {
		inputAlignment=alignment;
		Header(3,2,4,17,23); Encoded(1); Encoded(2); Encoded(3); Encoded(99); Run(3,200); Run(1,255); Footer();
		for(i=0;i<3;i++) Golden(golden+i*4,i+1); for(i=3;i<6;i++) Golden(golden+i*4,200);
		Valid(golden,3,2,0); Truncations(fixtureSize); Valid(golden,3,2,1);
		Header(3,1,4,0,0); Run(4,12); Footer(); for(i=0;i<3;i++) Golden(golden+i*4,12);
		Valid(golden,3,1,0); Truncations(fixtureSize);
		Header(1,1,1,0,0); Run(1,255); Footer(); Golden(golden,255); Valid(golden,1,1,0); Truncations(fixtureSize);
		/* A marker byte is legal encoded image data, but the footer must still have its own marker. */
		Header(1,1,1,0,0); Encoded(12); Footer(); Golden(golden,12); Valid(golden,1,1,0); Truncations(fixtureSize);
	}
	inputAlignment=0;
	Header(256,1,256,0,0); for(i=0;i<256;i++) { if(i>=192) Run(1,i); else Encoded(i); Golden(golden+i*4,i); } Footer(); Valid(golden,256,1,0);
	Header(1,1,65535,0,0); remaining=65535;
	while(remaining) { i=remaining>63?63:remaining; Run(i,31); remaining-=i; } Footer(); Golden(golden,31); Valid(golden,1,1,0);
	Header(1024,1024,1024,0,0);
	for(j=0;j<1024;j++) { remaining=1024; while(remaining) { i=remaining>63?63:remaining; Run(i,j%256); remaining-=i; } }
	Footer(); large=malloc(1024*1024*4); Check(large!=NULL,"golden max allocation");
	for(j=0;j<1024;j++) for(i=0;i<1024;i++) Golden(large+(j*1024+i)*4,j%256);
	Valid(large,1024,1024,0); free(large);
	Header(2,2,2,0,0); Run(3,17); Run(1,18); Footer(); Reject(0); /* run crosses a scanline */
	Header(1,1,1,0,0); Encoded(0xc0); Encoded(17); Footer(); Reject(0); /* zero run cannot advance */
	Header(1,1,1,0,0); Encoded(0xc1); Footer(); Reject(0); /* missing run value cannot consume marker */
	Header(2,1,2,0,0); Encoded(17); Footer(); Reject(0); /* missing literal cannot consume palette */
	Header(1,1,2,0,0); Encoded(17); Footer(); Reject(0); /* complete visible pixels still need row padding */
	Header(1,1,1,0,0); Encoded(1); Footer();
	fixture[0]=9; Reject(0); fixture[0]=10; fixture[1]=4; Reject(0); fixture[1]=5;
	fixture[2]=0; Reject(0); fixture[2]=1; fixture[3]=4; Reject(0); fixture[3]=8;
	fixture[65]=0; Reject(0); fixture[65]=2; Reject(0); fixture[65]=1;
	LE(66,0); Reject(0); LE(66,1);
	LE(4,1); Reject(0); LE(4,0); LE(6,1); Reject(0); LE(6,0);
	LE(8,1024); Reject(0); LE(8,65535); Reject(0); LE(8,0);
	LE(10,1024); Reject(0); LE(10,65535); Reject(0); LE(10,0);
	fixture[fixtureSize-769]=11; Reject(0); fixture[fixtureSize-769]=12;
	failAllocation=1; Reject(1); failAllocation=0;
	reportedNegative=1; Reject(0); reportedNegative=0;
	missing=1; reads=frees=allocations=warnings=0; pic=(byte *)1;
	{ int width=-1,height=-1; R_LoadPCX("missing.pcx",&pic,&width,&height); Check(!pic && !width && !height && reads==1 && !frees && !allocations && !warnings,"missing file"); }
	puts("PCX cursor, RLE, row padding, palette and ownership regressions passed (issue #42)"); return 0;
}
