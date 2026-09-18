/* Issue #42: actual portable BMP file loader, exact inputs/outputs and ownership before errors. */
#include "../code/renderer/tr_image_bmp.c"
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

refimport_t ri;
static byte fixture[4096];
static int fixtureSize, reads, frees, allocations, failAllocation, inputAlignment, missing, expectedError;
static byte *fileAllocation, *fileBuffer, *outputAllocation, *rejectPic;
static int rejectWidth, rejectHeight;
static jmp_buf errorJump;

/** Fail immediately if layout, ownership or publication differs from the required behavior. */
static void Check( int ok, const char *message ) { if(!ok) { fprintf(stderr,"BMP regression failed: %s\n",message); exit(1); } }
/** Allocate an exact file end at all four alignments to expose both OOB and unaligned casts. */
static int Read( const char *name, void **buffer ) {
	(void)name; reads++;
	if(missing) { *buffer=NULL; return -1; }
	/* Leave no trailing byte: the returned span ends at the actual malloc boundary. */
	fileAllocation=malloc(fixtureSize+inputAlignment ? fixtureSize+inputAlignment : 1);
	Check(fileAllocation!=NULL,"exact file allocation"); fileBuffer=fileAllocation+inputAlignment;
	memcpy(fileBuffer,fixture,fixtureSize); *buffer=fileBuffer; return fixtureSize;
}
static void FreeFile( void *buffer ) { Check(buffer==fileBuffer && fileAllocation,"file ownership"); free(fileAllocation); fileAllocation=fileBuffer=NULL; frees++; }
/** Refuse unexpected large allocations before any corrupted header can consume host memory. */
static void *Allocate( int size ) {
	allocations++; Check(size>0 && size<=4096 && !outputAllocation,"output allocation bounds");
	if(failAllocation) return NULL;
	outputAllocation=malloc(size); Check(outputAllocation!=NULL,"output allocation"); memset(outputAllocation,0xcd,size); return outputAllocation;
}
static void FreeOutput( void *buffer ) { Check(buffer==outputAllocation,"output ownership"); free(buffer); outputAllocation=NULL; }
static void QDECL Error( int level, const char *format, ... ) {
	(void)format; Check(expectedError && level==ERR_DROP && reads==frees && !fileAllocation && !outputAllocation,"ownership before error"); longjmp(errorJump,1);
}
static void QDECL Print( int level, const char *format, ... ) { (void)level; (void)format; }
/** Encode metadata without host endian/alignment dependencies. */
static void LE( int position, unsigned int value, int bytes ) { int i; for(i=0;i<bytes;i++) fixture[position+i]=value>>(8*i); }
/** Describe a Windows BI_RGB bitmap with explicit palette and pixel offsets. */
static void Header( unsigned int width, unsigned int rawHeight, int depth, int offset, int colors, int size ) {
	memset(fixture,0,sizeof(fixture)); fixtureSize=size; reads=frees=allocations=failAllocation=missing=0;
	fixture[0]='B'; fixture[1]='M'; LE(2,size,4); LE(10,offset,4); LE(14,40,4);
	LE(18,width,4); LE(22,rawHeight,4); LE(26,1,2); LE(28,depth,2); LE(46,colors,4);
}
/** Check the complete published RGBA image and optional dimension arguments. */
static void Valid( const byte *golden, int columns, int rows, int nullable ) {
	byte *pic=(byte *)1; int width=-1,height=-1;
	expectedError=0; reads=frees=allocations=0;
	R_LoadBMP("test.bmp",&pic,nullable?NULL:&width,nullable?NULL:&height);
	Check(pic==outputAllocation && pic && !memcmp(pic,golden,columns*rows*4),"decoded BMP pixels");
	Check(nullable || (width==columns && height==rows),"published dimensions");
	Check(reads==1 && frees==1 && allocations==1 && !fileAllocation,"success ownership"); FreeOutput(pic);
}
/** Reject before output publication and release every owned input/output before ERR_DROP. */
static void Reject( int expectedAllocations ) {
	rejectPic=(byte *)1; rejectWidth=rejectHeight=-1;
	expectedError=1; reads=frees=allocations=0;
	if(!setjmp(errorJump)) { R_LoadBMP("test.bmp",&rejectPic,&rejectWidth,&rejectHeight); Check(0,"invalid BMP accepted"); }
	Check(!rejectPic && !rejectWidth && !rejectHeight && reads==1 && frees==1 && allocations==expectedAllocations,"rejected output/state"); expectedError=0;
}
/** Every truncation updates its declared file size so row/palette/header checks actually execute. */
static void Truncations( int complete ) {
	int size;
	for(size=0;size<complete;size++) { fixtureSize=size; LE(2,size,4); Reject(0); }
	fixtureSize=complete; LE(2,complete,4);
}

/** Cover all depths, orientation, rows, offsets, tiny files, malformed palettes and extreme metadata. */
int main( void ) {
	int alignment,i;
	byte redGreen[]={255,0,0,255,0,255,0,255};
	byte fourPixels[]={255,0,0,255,0,255,0,255,0,255,0,255,255,0,0,255};
	byte alphaPixels[]={255,0,0,17,0,255,0,129};
	byte palettePixels[256*4], *pic;
	ri.FS_ReadFile=Read; ri.FS_FreeFile=FreeFile; ri.Malloc=Allocate; ri.Free=FreeOutput; ri.Error=Error; ri.Printf=Print;
	for(alignment=0;alignment<4;alignment++) {
		inputAlignment=alignment;
		Header(1,2,24,59,0,67); memset(fixture+54,0xee,5);
		fixture[60]=255; fixture[62]=0xbe; fixture[65]=255; fixture[66]=0xbe;
		Valid(redGreen,1,2,0); Truncations(67); Valid(redGreen,1,2,1);
		Header(1,0xfffffffeu,24,54,0,62); fixture[56]=255; fixture[59]=255;
		Valid(redGreen,1,2,0); Truncations(62);
		Header(1,2,16,54,0,62); LE(54,0x03e0,2); LE(58,0x7c00,2); fixture[56]=fixture[57]=fixture[60]=fixture[61]=0xbe;
		Valid(redGreen,1,2,0); Truncations(62);
		Header(2,1,32,54,0,62); fixture[56]=255; fixture[57]=17; fixture[59]=255; fixture[61]=129;
		Valid(alphaPixels,2,1,0); Truncations(62);
		Header(2,2,8,62,2,70); fixture[56]=255; fixture[59]=255;
		fixture[62]=1; fixture[63]=0; fixture[66]=0; fixture[67]=1; fixture[64]=fixture[65]=fixture[68]=fixture[69]=0xff;
		Valid(fourPixels,2,2,0); Truncations(70); fixture[62]=2; Reject(0);
		Header(1,1,24,122,0,126); LE(14,108,4); fixture[124]=255;
		Valid(redGreen,1,1,0); Truncations(126);
	}
	inputAlignment=0;
	Header(256,1,8,1078,0,1334);
	for(i=0;i<256;i++) {
		fixture[54+i*4]=i; fixture[54+i*4+1]=255-i; fixture[54+i*4+2]=i/2; fixture[1078+i]=i;
		palettePixels[i*4]=i/2; palettePixels[i*4+1]=255-i; palettePixels[i*4+2]=i; palettePixels[i*4+3]=255;
	}
	Valid(palettePixels,256,1,0);
	Header(1,1,24,54,0,58); fixture[56]=255;
	failAllocation=1; Reject(1); failAllocation=0;
	fixture[0]='X'; Reject(0); fixture[0]='B'; fixture[1]='X'; Reject(0); fixture[1]='M';
	LE(2,57,4); Reject(0); LE(2,58,4);
	LE(14,39,4); Reject(0); LE(14,UINT_MAX,4); Reject(0); LE(14,40,4);
	LE(26,2,2); Reject(0); LE(26,1,2);
	LE(28,4,2); Reject(0); LE(28,15,2); Reject(0); LE(28,24,2);
	LE(30,3,4); Reject(0); LE(30,0,4);
	LE(10,53,4); Reject(0); LE(10,UINT_MAX,4); Reject(0); LE(10,54,4);
	LE(34,3,4); Reject(0); LE(34,5,4); Reject(0); LE(34,0,4);
	LE(46,UINT_MAX,4); Reject(0); LE(46,0,4);
	LE(18,0,4); Reject(0); LE(18,UINT_MAX,4); Reject(0); LE(18,INT_MAX,4); Reject(0); LE(18,536870911,4); Reject(0); LE(18,1,4);
	LE(22,0,4); Reject(0); LE(22,0x80000000u,4); Reject(0); LE(22,INT_MAX,4); Reject(0); LE(22,1,4);
	Header(1,1,8,54,257,58); Reject(0);
	missing=1; reads=frees=allocations=0; expectedError=0; pic=(byte *)1;
	{ int width=-1,height=-1; R_LoadBMP("missing.bmp",&pic,&width,&height); Check(!pic && !width && !height && reads==1 && !frees && !allocations,"missing image"); }
	puts("BMP cursor, row, palette, and ownership regressions passed (issue #42)"); return 0;
}
