/* Issue #42: actual portable TGA loader, complete header/ID/raw/RLE preflight and ownership. */
#include "../code/renderer/tr_image_tga.c"
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

refimport_t ri;
static byte fixture[300000];
static const byte colors[4][4]={{255,0,0,17},{0,255,0,33},{0,0,255,129},{31,31,31,255}};
static int fixtureSize, inputAlignment, reads, frees, allocations, warnings, failAllocation, missing, reportedNegative, expectedError;
static byte *fileAllocation, *fileBuffer, *outputAllocation, *rejectPic;
static int rejectWidth,rejectHeight;
static jmp_buf errorJump;

/** Stop on any pixel, allocation, publication or ownership failure. */
static void Check( int ok, const char *message ) { if(!ok) { fprintf(stderr,"TGA regression failed: %s\n",message); exit(1); } }
/** Exact input ends at the allocator boundary for all four possible header alignments. */
static int Read( const char *name, void **buffer ) {
	(void)name; reads++;
	if(missing) { *buffer=NULL; return -1; }
	fileAllocation=malloc(fixtureSize+inputAlignment ? fixtureSize+inputAlignment : 1); Check(fileAllocation!=NULL,"file allocation");
	fileBuffer=fileAllocation+inputAlignment; memcpy(fileBuffer,fixture,fixtureSize); *buffer=fileBuffer; return reportedNegative?-1:fixtureSize;
}
static void FreeFile( void *buffer ) { Check(fileAllocation && buffer==fileBuffer,"file ownership"); free(fileAllocation); fileAllocation=fileBuffer=NULL; frees++; }
static void *Allocate( int size ) {
	allocations++; Check(size>0 && size<=1024*1024 && !outputAllocation,"unexpected output allocation");
	if(failAllocation) return NULL;
	outputAllocation=malloc(size); Check(outputAllocation!=NULL,"output allocation"); memset(outputAllocation,0xcd,size); return outputAllocation;
}
static void FreeOutput( void *buffer ) { Check(buffer==outputAllocation,"output ownership"); free(buffer); outputAllocation=NULL; }
/** Check cleanup before the engine longjmp; post-jump published-state storage is static. */
static void QDECL Error( int level, const char *format, ... ) {
	(void)format; Check(expectedError && level==ERR_DROP && reads==frees && !fileAllocation && !outputAllocation,"ownership before error"); longjmp(errorJump,1);
}
/** A successful top-down declaration retains the old warning and native output row order. */
static void QDECL Print( int level, const char *format, ... ) {
	(void)format; Check(!expectedError && level==PRINT_WARNING && reads==frees && !fileAllocation && outputAllocation,"ownership before orientation warning"); warnings++;
}
static void LE( int position, unsigned int value ) { fixture[position]=value; fixture[position+1]=value>>8; }
static void Header( int type, int depth, unsigned int columns, unsigned int rows, int id, int attributes ) {
	memset(fixture,0,sizeof(fixture)); fixtureSize=18+id;
	reads=frees=allocations=warnings=failAllocation=missing=reportedNegative=0;
	fixture[0]=id; fixture[2]=type; LE(12,columns); LE(14,rows); fixture[16]=depth; fixture[17]=attributes;
	memset(fixture+18,0xee,id);
}
static void Byte( byte value ) { Check(fixtureSize<(int)sizeof(fixture),"fixture capacity"); fixture[fixtureSize++]=value; }
static void Color( int index, int depth ) { Byte(colors[index][2]); Byte(colors[index][1]); Byte(colors[index][0]); if(depth==32) Byte(colors[index][3]); }
static void Golden( byte *pixel, int index, int depth ) { memcpy(pixel,colors[index],4); if(depth!=32) pixel[3]=255; }
/** Compare every RGBA byte, dimensions and the single exact allocation. */
static void Valid( const byte *golden, int columns, int rows, int nullable ) {
	byte *pic=(byte *)1; int width=-1,height=-1;
	reads=frees=allocations=warnings=0; expectedError=0;
	R_LoadTGA("test.tga",&pic,nullable?NULL:&width,nullable?NULL:&height);
	Check(pic && pic==outputAllocation && !memcmp(pic,golden,columns*rows*4),"golden TGA pixels");
	Check(nullable || (width==columns && height==rows),"published dimensions");
	Check(reads==1 && frees==1 && allocations==1 && warnings==((fixture[17]&32)!=0) && !fileAllocation,"success ownership"); FreeOutput(pic);
}
/** Malformed data is rejected before output allocation or publication, releasing its file. */
static void Reject( int expectedAllocations ) {
	rejectPic=(byte *)1; rejectWidth=rejectHeight=-1; reads=frees=allocations=warnings=0; expectedError=1;
	if(!setjmp(errorJump)) { R_LoadTGA("bad.tga",&rejectPic,&rejectWidth,&rejectHeight); Check(0,"invalid TGA accepted"); }
	Check(!rejectPic && !rejectWidth && !rejectHeight && reads==1 && frees==1 && allocations==expectedAllocations && !warnings,"rejected state"); expectedError=0;
}
static void Truncations( int complete ) { int i; for(i=0;i<complete;i++) { fixtureSize=i; Reject(0); } fixtureSize=complete; }

/** Cover supported legacy formats, native row order, packet endpoints and hostile metadata. */
int main( void ) {
	int alignment,depth,type,i,complete,attributes;
	const int rawOrder[4]={2,3,0,1}, mixedOrder[12]={3,0,1,2,3,3,3,3,0,1,2,3};
	byte golden[512], *wide, *pic;
	ri.FS_ReadFile=Read; ri.FS_FreeFile=FreeFile; ri.Malloc=Allocate; ri.Free=FreeOutput; ri.Error=Error; ri.Printf=Print;
	for(alignment=0;alignment<4;alignment++) {
		inputAlignment=alignment;
		for(depth=24;depth<=32;depth+=8) {
			for(type=2;type<=3;type++) {
				Header(type,depth,2,2,9,0); for(i=0;i<4;i++) Color(i,depth);
				for(i=0;i<4;i++) Golden(golden+i*4,rawOrder[i],depth);
				Valid(golden,2,2,0); Truncations(fixtureSize); Valid(golden,2,2,1);
				for(attributes=16;attributes<=48;attributes+=16) { fixture[17]=attributes; Valid(golden,2,2,0); }
			}
			Header(10,depth,4,3,255,0); Byte(2); Color(0,depth); Color(1,depth); Color(2,depth); Byte(0x85); Color(3,depth);
			Byte(2); Color(0,depth); Color(1,depth); Color(2,depth);
			for(i=0;i<12;i++) Golden(golden+i*4,mixedOrder[i],depth);
			Valid(golden,4,3,0); Truncations(fixtureSize); fixture[17]=32; Valid(golden,4,3,1);
			Header(10,depth,1,1,0,0); Byte(0x80); Color(0,depth); Golden(golden,0,depth); Valid(golden,1,1,0); Truncations(fixtureSize);
			Header(10,depth,1,1,0,0); Byte(0); Color(1,depth); Golden(golden,1,depth); Valid(golden,1,1,0); Truncations(fixtureSize);
		}
		Header(3,8,2,2,0,0); Byte(1); Byte(17); Byte(129); Byte(255);
		{ const byte gray[]={129,129,129,255,255,255,255,255,1,1,1,255,17,17,17,255}; memcpy(golden,gray,sizeof(gray)); }
		Valid(golden,2,2,0); Truncations(fixtureSize);
	}
	inputAlignment=0;
	Header(10,32,1,128,0,0); Byte(0xff); Color(2,32); for(i=0;i<128;i++) Golden(golden+i*4,2,32); Valid(golden,1,128,0); Truncations(fixtureSize);
	Header(10,24,1,128,0,0); Byte(0x7f); for(i=0;i<128;i++) Color(i%4,24); for(i=0;i<128;i++) Golden(golden+i*4,(127-i)%4,24);
	Valid(golden,1,128,0); Truncations(fixtureSize);
	wide=malloc(65535*4); Check(wide!=NULL,"wide golden allocation");
	Header(3,8,65535,1,0,0); for(i=0;i<65535;i++) { Byte(i%256); wide[i*4]=wide[i*4+1]=wide[i*4+2]=i%256; wide[i*4+3]=255; } Valid(wide,65535,1,0);
	Header(3,8,1,65535,0,0); for(i=0;i<65535;i++) { Byte(i%256); wide[i*4]=wide[i*4+1]=wide[i*4+2]=(65534-i)%256; wide[i*4+3]=255; } Valid(wide,1,65535,0); free(wide);
	Header(2,24,1,1,0,0); Color(0,24); Golden(golden,0,24); complete=fixtureSize;
	memset(fixture+fixtureSize,0xee,26); fixtureSize+=26; Valid(golden,1,1,0); fixtureSize=complete; LE(8,65535); LE(10,65535); Valid(golden,1,1,0);
	Header(10,24,1,1,0,0); Byte(0x81); Color(0,24); Reject(0); /* run exceeds entire image */
	Header(10,24,1,1,0,0); Byte(1); Color(0,24); Color(1,24); Reject(0); /* raw packet exceeds image */
	Header(10,24,1,2,0,0); Byte(0x80); Color(0,24); Byte(0x81); Color(1,24); Reject(0); /* later bad packet: zero allocation */
	Header(10,24,1,1,0,0); Byte(0xff); Color(0,24); Reject(0);
	Header(2,24,1,1,0,0); Color(0,24);
	fixture[0]=255; Reject(0); fixture[0]=0; fixture[1]=1; Reject(0); fixture[1]=0;
	for(type=0;type<=11;type++) if(type!=2 && type!=3 && type!=10) { fixture[2]=type; Reject(0); } fixture[2]=2;
	for(depth=0;depth<=40;depth+=8) if(depth!=24 && depth!=32) { fixture[16]=depth; Reject(0); } fixture[16]=24;
	fixture[2]=3; fixture[16]=16; Reject(0); fixture[2]=2; fixture[16]=24;
	LE(12,0); Reject(0); LE(12,1); LE(14,0); Reject(0); LE(14,1);
	LE(12,65535); LE(14,65535); Reject(0); LE(12,32768); LE(14,16384); Reject(0);
	LE(12,32767); Reject(0); /* valid arithmetic, but no complete raw payload */
	LE(12,1); LE(14,1); failAllocation=1; Reject(1); failAllocation=0; reportedNegative=1; Reject(0); reportedNegative=0;
	missing=1; reads=frees=allocations=warnings=0; expectedError=0; pic=(byte *)1;
	{ int width=-1,height=-1; R_LoadTGA("missing.tga",&pic,&width,&height); Check(!pic && !width && !height && reads==1 && !frees && !allocations && !warnings,"missing file"); }
	puts("TGA cursor, ID, raw/RLE payload and ownership regressions passed (issue #42)"); return 0;
}
