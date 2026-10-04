/* Issues #42 and #475: actual PCX loader, exact input/output allocations, bounded RLE, and
   dimension/pixel parity with a reference model of id's 1.32c LoadPCX on memory-safe inputs. */
#include "../code/renderer/tr_image_pcx.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

refimport_t ri;
static byte fixture[131072];
static int fixtureSize, encodedEnd, inputAlignment, reads, frees, allocations, warnings, failAllocation, missing, reportedNegative;
static int retailAccepted, retailRejected, retailUnsafe;
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

enum { RETAIL_REJECT, RETAIL_ACCEPT, RETAIL_UNSAFE };
/**
 * Reference model: dbe4ddb's LoadPCX followed by LoadPCX32 (code/renderer/tr_image.c), kept
 * statement for statement, on a copy that has FS_ReadFile's trailing NUL. xmax/ymax are the
 * unsigned shorts of little-endian retail (LittleShort is a no-op there). Every header, palette,
 * RLE read and pixel write is checked; one id would make out of bounds returns RETAIL_UNSAFE.
 */
static int RetailLoadPCX32( const byte *file, int len, byte **pic, int *width, int *height ) {
	byte *copy, *raw, *out, *pix, *palette, *pic32;
	int x, y, dataByte, runLength, xmax, ymax, size, i, c, p, verdict = RETAIL_UNSAFE;
	*pic = NULL; *width = *height = 0;
	if ( len < 12 ) return RETAIL_UNSAFE; /* header fields read past the file */
	copy = malloc( len + 1 ); Check(copy!=NULL,"reference file"); memcpy( copy, file, len ); copy[len] = 0;
	xmax = copy[8] | copy[9] << 8;
	ymax = copy[10] | copy[11] << 8;
	if ( copy[0] != 0x0a || copy[1] != 5 || copy[2] != 1 || copy[3] != 8 || xmax >= 1024 || ymax >= 1024 ) {
		free( copy ); return RETAIL_REJECT;
	}
	size = (ymax+1) * (xmax+1);
	out = malloc( size ); Check(out!=NULL,"reference pixels");
	pix = out;
	if ( len < 768 ) goto done; /* Com_Memcpy of the palette from before the file */
	palette = copy + len - 768;
	raw = copy + 128;
	for (y=0 ; y<=ymax ; y++, pix += xmax+1) {
		for (x=0 ; x<=xmax ; ) {
			if ( raw - copy > len ) goto done; /* past the NUL terminator */
			dataByte = *raw++;
			if((dataByte & 0xC0) == 0xC0) {
				runLength = dataByte & 0x3F;
				if ( raw - copy > len ) goto done;
				dataByte = *raw++;
			} else
				runLength = 1;
			while(runLength-- > 0) {
				if ( (pix - out) + x >= size ) goto done; /* heap overflow */
				pix[x++] = dataByte;
			}
		}
	}
	if ( raw - copy > len ) { verdict = RETAIL_REJECT; goto done; } /* "PCX file %s was malformed" */
	c = (xmax+1) * (ymax+1);
	pic32 = *pic = malloc( 4 * c ); Check(pic32!=NULL,"reference RGBA");
	for (i = 0 ; i < c ; i++) {
		p = out[i];
		pic32[0] = palette[p*3];
		pic32[1] = palette[p*3 + 1];
		pic32[2] = palette[p*3 + 2];
		pic32[3] = 255;
		pic32 += 4;
	}
	*width = xmax+1; *height = ymax+1; verdict = RETAIL_ACCEPT;
done:
	free( out ); free( copy );
	return verdict;
}

/** The engine loader must publish retail's dimensions and RGBA, or reject when retail does or is unsafe. */
static int Compare( int expected, const char *message ) {
	byte *reference, *pic=(byte *)1; int referenceWidth, referenceHeight, width=-1, height=-1, verdict;
	verdict = RetailLoadPCX32(fixture,fixtureSize,&reference,&referenceWidth,&referenceHeight);
	reads=frees=allocations=warnings=0; R_LoadPCX("compare.pcx",&pic,&width,&height);
	if(verdict==RETAIL_ACCEPT) {
		Check(pic && pic==outputAllocation && width==referenceWidth && height==referenceHeight,message);
		Check(!memcmp(pic,reference,width*height*4),message);
		Check(reads==1 && frees==1 && allocations==1 && !warnings && !fileAllocation,"retail success ownership");
		FreeOutput(pic); free(reference); retailAccepted++;
	} else {
		Check(!pic && !width && !height && reads==1 && frees==1 && !allocations && warnings==1 && !fileAllocation,message);
		if(verdict==RETAIL_UNSAFE) retailUnsafe++; else retailRejected++;
	}
	Check(expected<0 || expected==verdict,"unexpected reference verdict");
	return verdict;
}
/** Every prefix of a file must still agree with retail, which reads its palette from the new end. */
static void Prefixes( void ) { int complete=fixtureSize; for(fixtureSize=0;fixtureSize<complete;fixtureSize++) Compare(-1,"retail prefix"); fixtureSize=complete; }

static void LE( int position, unsigned int value ) { fixture[position]=value; fixture[position+1]=value>>8; }
/** Build a standard single-plane, 8-bit version-5 header with explicit origins and maxima. */
static void Header( unsigned int xmin, unsigned int ymin, unsigned int xmax, unsigned int ymax, unsigned int bytesPerLine ) {
	memset(fixture,0,sizeof(fixture)); encodedEnd=128; fixtureSize=0;
	reads=frees=allocations=warnings=failAllocation=missing=reportedNegative=0;
	fixture[0]=10; fixture[1]=5; fixture[2]=1; fixture[3]=8;
	LE(4,xmin); LE(6,ymin); LE(8,xmax); LE(10,ymax); fixture[65]=1; LE(66,bytesPerLine);
}
static void Encoded( byte value ) { Check(encodedEnd<(int)sizeof(fixture)-769,"fixture capacity"); fixture[encodedEnd++]=value; }
static void Run( int count, byte value ) { Check(count>=0 && count<=63,"fixture run"); Encoded(0xc0|count); Encoded(value); }
static void Runs( int count, byte value ) { while(count) { int run=count>63?63:count; Run(run,value); count-=run; } }
/** Choose a palette with three independent channels and a fixed, non-marker leading byte. */
static void Footer( void ) {
	int i; Encoded(12); fixtureSize=encodedEnd+768; Check(fixtureSize<=(int)sizeof(fixture),"footer capacity");
	for(i=0;i<256;i++) { fixture[encodedEnd+i*3]=i; fixture[encodedEnd+i*3+1]=255-i; fixture[encodedEnd+i*3+2]=i/2; }
}
static void Golden( byte *output, int index ) { output[0]=index; output[1]=255-index; output[2]=index/2; output[3]=255; }
/** Assert the complete decoded image and a single allocation, independently of the reference. */
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
static unsigned int seed=475;
static unsigned int Random( unsigned int limit ) { seed=seed*1103515245u+12345u; return (seed>>16)%limit; }

/** Cover retail sizes, odd padded rows, overshooting runs, boundaries and malformed metadata. */
int main( void ) {
	int alignment,i,j,trial;
	byte golden[1024*4], *large, *pic;
	ri.FS_ReadFile=Read; ri.FS_FreeFile=FreeFile; ri.Malloc=Allocate; ri.Free=FreeOutput; ri.Printf=Print; ri.Error=Error;
	for(alignment=0;alignment<4;alignment++) {
		inputAlignment=alignment;
		/* Odd width with a padded bytes_per_line: retail reads the pad as the next row's first pixel. */
		Header(0,0,2,1,4); Encoded(1); Encoded(2); Encoded(3); Encoded(99); Encoded(4); Encoded(5); Encoded(6); Encoded(98); Footer();
		Golden(golden,1); Golden(golden+4,2); Golden(golden+8,3); Golden(golden+12,99); Golden(golden+16,4); Golden(golden+20,5);
		Valid(golden,3,2,0); Valid(golden,3,2,1); Compare(RETAIL_ACCEPT,"odd padded width"); Prefixes();
		/* xmin/ymin are ignored: the size is xmax+1 by ymax+1 even when the origin is past it. */
		Header(2,1,4,2,4); Runs(5,7); Run(5,8); Run(5,9); Footer();
		for(i=0;i<15;i++) Golden(golden+i*4,7+i/5);
		Valid(golden,5,3,0); Compare(RETAIL_ACCEPT,"nonzero origin"); Prefixes();
		Header(17,23,2,1,4); Run(3,200); Run(3,201); Footer();
		for(i=0;i<6;i++) Golden(golden+i*4,200+i/3);
		Valid(golden,3,2,0); Compare(RETAIL_ACCEPT,"origin beyond maxima");
		/* A run past a row end is dropped when id's overshoot lands inside its buffer. */
		Header(0,0,1,1,2); Run(3,17); Run(1,18); Footer();
		Golden(golden,17); Golden(golden+4,17); Golden(golden+8,18); Golden(golden+12,12);
		Valid(golden,2,2,0); Compare(RETAIL_ACCEPT,"run crosses a row into the marker");
		Header(0,0,0,2,1); Run(3,5); Encoded(6); Encoded(7); Footer();
		Golden(golden,5); Golden(golden+4,6); Golden(golden+8,7);
		Valid(golden,1,3,0); Compare(RETAIL_ACCEPT,"run spans several narrow rows"); Prefixes();
		/* Zero-length runs are no-ops; a short stream continues into the marker and palette. */
		Header(0,0,0,0,1); Encoded(0xc0); Encoded(17); Footer(); Golden(golden,12); Valid(golden,1,1,0); Compare(RETAIL_ACCEPT,"zero run");
		Header(0,0,0,0,1); Encoded(0xc1); Footer(); Golden(golden,12); Valid(golden,1,1,0); Compare(RETAIL_ACCEPT,"run value is the marker");
		Header(0,0,1,0,2); Encoded(17); Footer(); Golden(golden,17); Golden(golden+4,12); Valid(golden,2,1,0); Compare(RETAIL_ACCEPT,"literal is the marker");
		Header(0,0,0,0,1); Encoded(12); Footer(); Golden(golden,12); Valid(golden,1,1,0); Compare(RETAIL_ACCEPT,"marker as pixel"); Prefixes();
		/* A run past the end of the last row would overflow id's heap buffer. */
		Header(0,0,2,0,4); Run(4,12); Footer(); Compare(RETAIL_UNSAFE,"last-row overshoot"); Reject(0);
		Header(0,0,2,1,4); Encoded(1); Encoded(2); Encoded(3); Encoded(99); Run(3,200); Footer(); Compare(RETAIL_UNSAFE,"padded last row overshoot");
		Header(0,0,0,0,65535); Runs(65535,31); Footer(); Compare(RETAIL_UNSAFE,"huge stride run"); Reject(0);
	}
	inputAlignment=0;
	Header(0,0,255,0,256); for(i=0;i<256;i++) { if(i>=192) Run(1,i); else Encoded(i); Golden(golden+i*4,i); } Footer();
	Valid(golden,256,1,0); Compare(RETAIL_ACCEPT,"all palette indices");
	/* The axis limit is on xmax/ymax themselves, independent of xmin/ymin. */
	Header(0,0,1023,0,1024); Runs(1024,40); Footer(); for(i=0;i<1024;i++) Golden(golden+i*4,40);
	Valid(golden,1024,1,0); Compare(RETAIL_ACCEPT,"xmax 1023");
	LE(4,5); Valid(golden,1024,1,0); Compare(RETAIL_ACCEPT,"xmax 1023 with xmin");
	LE(8,1024); Compare(RETAIL_REJECT,"xmax 1024 with xmin"); LE(4,0); Compare(RETAIL_REJECT,"xmax 1024");
	Header(0,0,0,1023,1); for(i=0;i<1024;i++) Encoded(i%192); Footer(); for(i=0;i<1024;i++) Golden(golden+i*4,i%192);
	Valid(golden,1,1024,0); Compare(RETAIL_ACCEPT,"ymax 1023");
	LE(6,1000); Valid(golden,1,1024,0); Compare(RETAIL_ACCEPT,"ymax 1023 with ymin");
	LE(10,1024); Compare(RETAIL_REJECT,"ymax 1024");
	Header(0,0,1023,1023,1024);
	for(j=0;j<1024;j++) Runs(1024,j%256);
	Footer(); large=malloc(1024*1024*4); Check(large!=NULL,"golden max allocation");
	for(j=0;j<1024;j++) for(i=0;i<1024;i++) Golden(large+(j*1024+i)*4,j%256);
	Valid(large,1024,1024,0); Compare(RETAIL_ACCEPT,"1024 square"); free(large);
	Header(0,0,1023,1023,1024); Run(63,1); Footer(); Compare(RETAIL_UNSAFE,"stream past the file"); Reject(0);
	/* Header fields retail ignores: color_planes, bytes_per_line, xmin/ymin, palette marker. */
	Header(0,0,1,1,2); Encoded(1); Encoded(2); Encoded(3); Encoded(4); Footer();
	for(i=0;i<4;i++) Golden(golden+i*4,i+1);
	fixture[65]=0; Valid(golden,2,2,0); Compare(RETAIL_ACCEPT,"zero planes");
	fixture[65]=3; Valid(golden,2,2,0); Compare(RETAIL_ACCEPT,"three planes"); fixture[65]=1;
	LE(66,0); Valid(golden,2,2,0); Compare(RETAIL_ACCEPT,"zero stride"); LE(66,1); Compare(RETAIL_ACCEPT,"short stride"); LE(66,2);
	LE(4,1); Valid(golden,2,2,0); LE(4,0); LE(6,1); Valid(golden,2,2,0); LE(6,0);
	fixture[fixtureSize-769]=11; Valid(golden,2,2,0); Compare(RETAIL_ACCEPT,"no palette marker"); fixture[fixtureSize-769]=12;
	/* Malformed metadata and lengths still reject cleanly. */
	fixture[0]=9; Reject(0); fixture[0]=10; fixture[1]=4; Reject(0); fixture[1]=5;
	fixture[2]=0; Reject(0); fixture[2]=1; fixture[3]=4; Reject(0); fixture[3]=8;
	LE(8,1024); Reject(0); LE(8,32768); Reject(0); LE(8,65535); Reject(0); LE(8,1);
	LE(10,1024); Reject(0); LE(10,32768); Reject(0); LE(10,65535); Reject(0); LE(10,1);
	i=fixtureSize; fixtureSize=767; Compare(RETAIL_UNSAFE,"palette before file"); Reject(0); fixtureSize=i;
	failAllocation=1; Reject(1); failAllocation=0;
	reportedNegative=1; Reject(0); reportedNegative=0;
	missing=1; reads=frees=allocations=warnings=0; pic=(byte *)1;
	{ int width=-1,height=-1; R_LoadPCX("missing.pcx",&pic,&width,&height); Check(!pic && !width && !height && reads==1 && !frees && !allocations && !warnings,"missing file"); }
	missing=0;
	/* Deterministic differential: small random headers and run-heavy streams against the reference. */
	for(trial=0;trial<30000;trial++) {
		int length=768+Random(400), xmin=Random(8), ymin=Random(8), xmax=Random(9), ymax=Random(6);
		Header(xmin,ymin,xmax,ymax,Random(12));
		fixture[65]=Random(4);
		if(!Random(16)) { i=Random(12); fixture[i]^=1<<Random(8); }
		if(!Random(64)) { LE(8,1020+Random(8)); LE(10,Random(3)); }
		for(i=128;i<length;i++) fixture[i]=Random(4) ? (Random(3) ? 0xc0|Random(8) : Random(256)) : Random(192);
		if(Random(2) && length>768) fixture[length-769]=12;
		fixtureSize=length; inputAlignment=Random(4); Compare(-1,"random retail differential");
	}
	Check(retailAccepted>1000 && retailRejected>100 && retailUnsafe>1000,"differential coverage");
	printf("PCX retail parity (%d accepted, %d rejected, %d unsafe-in-retail rejected), RLE, palette and ownership regressions passed (issues #42, #475)\n",
		retailAccepted,retailRejected,retailUnsafe);
	return 0;
}
