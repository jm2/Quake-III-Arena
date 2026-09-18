/* Issue #43: actual renderer JPEG operations and the complete bundled library, with exact allocations. */
#include "../code/renderer/tr_image_jpeg.c"
#include "../code/jpeg-6/jmemsys.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

refimport_t ri;
static byte fixture[400000], original[400000];
static int fixtureSize, originalSize, alignment, missing, negativeLength, reads, fileFrees, writes, warnings, allocationCalls, failAt, live;
static void *owned[512];
static int ownedSize[512];
static byte *fileAllocation, *fileBuffer, *loaded;
static int loadedWidth,loadedHeight,allowDrop,drops;
static jmp_buf dropJump;

/** Fail on wrong ownership, pixels or unexpected allocation size. */
static void Check( int ok, const char *message ) { if(!ok) { fprintf(stderr,"JPEG regression failed: %s\n",message); exit(1); } }
/** Track every codec/context/buffer allocation; injected exhaustion persists through allocator retries. */
static void *Allocate( int size ) {
	int i; void *pointer;
	allocationCalls++; Check(size>0 && size<=1048576,"oversized allocation before validation");
	if(failAt && allocationCalls>=failAt) return NULL;
	pointer=malloc(size); Check(pointer!=NULL,"host allocation"); memset(pointer,0xcd,size);
	for(i=0;i<512;i++) if(!owned[i]) { owned[i]=pointer; ownedSize[i]=size; live++; return pointer; }
	Check(0,"allocation tracking capacity"); return NULL;
}
/** Free only a still-owned pointer and reject leaks/double frees independently of LeakSanitizer. */
static void Free( void *pointer ) {
	int i; for(i=0;i<512;i++) if(owned[i]==pointer) { free(pointer); owned[i]=NULL; ownedSize[i]=0; live--; return; }
	Check(0,"free of unowned allocation");
}
static int Read( const char *name, void **buffer ) {
	(void)name; reads++; if(missing) { *buffer=NULL; return -1; }
	fileAllocation=malloc(fixtureSize+alignment ? fixtureSize+alignment : 1); Check(fileAllocation!=NULL,"exact file allocation");
	fileBuffer=fileAllocation+alignment; memcpy(fileBuffer,fixture,fixtureSize); *buffer=fileBuffer; return negativeLength?-1:fixtureSize;
}
static void FreeFile( void *buffer ) { Check(fileAllocation && buffer==fileBuffer,"file ownership"); free(fileAllocation); fileAllocation=fileBuffer=NULL; fileFrees++; }
/** Snapshot the fully finished JPEG; screenshot failure must never make a partial write. */
static void Write( const char *name, const void *buffer, int length ) {
	(void)name; Check(buffer && length>0 && length<=(int)sizeof(fixture),"encoded output bounds");
	memcpy(fixture,buffer,length); fixtureSize=length; writes++;
}
/** JPEG parse errors must release all ownership before the recoverable engine longjmp. */
static void QDECL Error( int level, const char *format, ... ) {
	(void)format; Check(allowDrop && level==ERR_DROP && !live && !fileAllocation && reads==fileFrees,"recoverable cleanup before error"); drops++; longjmp(dropJump,1);
}
static void QDECL Print( int level, const char *format, ... ) {
	(void)format;
	if(level==PRINT_WARNING) { Check(!live && !fileAllocation,"screenshot cleanup before warning"); warnings++; }
	else Check(level==PRINT_DEVELOPER,"unexpected JPEG message category");
}
static void Reset( void ) { Check(!live && !fileAllocation,"previous operation leaked"); reads=fileFrees=writes=warnings=allocationCalls=drops=failAt=missing=negativeLength=0; }
/** A prefix may recover through a true-EOF EOI; every outcome must remain bounded and owned. */
static int Load( int mayDrop ) {
	loaded=(byte *)1; loadedWidth=loadedHeight=-1; allowDrop=mayDrop;
	if(!setjmp(dropJump)) R_LoadJPG("test.jpg",&loaded,&loadedWidth,&loadedHeight);
	allowDrop=0;
	if(drops) { Check(!loaded && !loadedWidth && !loadedHeight && !live && !fileAllocation,"rejected publication"); return 0; }
	Check(loaded && loadedWidth>0 && loadedHeight>0 && live==1 && reads==1 && fileFrees==1 && !fileAllocation,"success ownership"); return 1;
}
/** Assert native top-down decoding, opaque alpha and the small lossy tolerance of a known grayscale image. */
static void Golden( int columns, int rows, const byte *bottomUp, int tolerance ) {
	int row,column,channel,expected,actual;
	Check(Load(0) && loadedWidth==columns && loadedHeight==rows,"decoded dimensions");
	for(row=0;row<rows;row++) for(column=0;column<columns;column++) {
		for(channel=0;channel<3;channel++) {
			expected=bottomUp[((rows-1-row)*columns+column)*4+channel]; actual=loaded[(row*columns+column)*4+channel];
			Check(abs(actual-expected)<=tolerance,"golden JPEG RGB pixels");
		}
		Check(loaded[(row*columns+column)*4+3]==255,"opaque decoded alpha");
	}
	Free(loaded);
}
static void Reject( void ) { Check(!Load(1) && drops==1,"malformed JPEG accepted"); }
/** Exercise actual source refills at both sides of 4096, without invoking a fake input allocation. */
static void Source( int length ) {
	rendererJPEG_t *context; struct jpeg_decompress_struct *info;
	byte *input=malloc(length?length:1); int i,consumed=0;
	for(i=0;i<length;i++) input[i]=i%251;
	Reset(); context=R_JPEGContext(qfalse); Check(context!=NULL,"source context"); info=&context->info.decode;
	if(!setjmp(context->error.jump)) {
		jpeg_create_decompress(info); jpeg_mem_src(info,input,length); info->src->init_source(info);
		while(consumed<length) {
			int count=length-consumed>4096?4096:length-consumed;
			Check(info->src->fill_input_buffer(info) && info->src->bytes_in_buffer==(size_t)count && !memcmp(info->src->next_input_byte,input+consumed,count),"bounded refill");
			consumed+=count; info->src->bytes_in_buffer=0;
		}
		if(length) { Check(info->src->fill_input_buffer(info) && info->src->bytes_in_buffer==2 && info->src->next_input_byte[0]==255 && info->src->next_input_byte[1]==JPEG_EOI,"EOI at true EOF"); info->src->bytes_in_buffer=0; }
		(void)info->src->fill_input_buffer(info); Check(0,"empty or repeated EOF accepted");
	}
	Check(context->error.pub.msg_code==(length?JERR_INPUT_EOF:JERR_INPUT_EMPTY),"EOF error kind"); R_JPEGRelease(context); free(input); Check(!live,"source cleanup");
}
/** A marker skip crossing true EOF must error rather than repeatedly refilling invented bytes. */
static void SkipEOF( void ) {
	byte input[3]={1,2,3}; rendererJPEG_t *context; struct jpeg_decompress_struct *info;
	Reset(); context=R_JPEGContext(qfalse); info=&context->info.decode;
	if(!setjmp(context->error.jump)) { jpeg_create_decompress(info); jpeg_mem_src(info,input,3); info->src->init_source(info); info->src->skip_input_data(info,32768); Check(0,"oversized marker skip"); }
	Check(context->error.pub.msg_code==JERR_INPUT_EOF,"marker EOF error"); R_JPEGRelease(context); Check(!live,"marker skip cleanup");
}
/** Refuse hard capacity exhaustion before reading/copying any fictitious giant encoded span. */
static void Capacity( void ) {
	rendererJPEG_t *context; rendererJPEGDest_t destination;
	Reset(); context=R_JPEGContext(qtrue); memset(&destination,0,sizeof(destination)); destination.owner=context;
	context->info.encode.dest=&destination.pub; context->encoded=Allocate(1); context->capacity=R_IMAGE_MAX_BYTES;
	if(!setjmp(context->error.jump)) { (void)R_JPEGGrowDestination(&context->info.encode); Check(0,"hard output limit ignored"); }
	R_JPEGRecover(context,"too-large.jpg"); Check(!live && warnings==1 && !writes,"hard output failure cleanup");
}
/** Decode through the restored standard FILE API, whose stream remains owned by its caller. */
static void Stdio( const char *path ) {
	FILE *file; rendererJPEG_t *context; struct jpeg_decompress_struct *info; byte row[8*3]; JSAMPROW pointer=row;
	Reset(); file=fopen(path,"wb"); Check(file!=NULL && fwrite(original,1,originalSize,file)==(size_t)originalSize && !fclose(file),"stdio fixture");
	file=fopen(path,"rb"); Check(file!=NULL,"stdio input"); context=R_JPEGContext(qfalse); info=&context->info.decode;
	if(setjmp(context->error.jump)) Check(0,"standard FILE decoding failed");
	jpeg_create_decompress(info); jpeg_stdio_src(info,file); Check(jpeg_read_header(info,TRUE)==JPEG_HEADER_OK && jpeg_start_decompress(info),"stdio header");
	Check(info->output_width==8 && info->output_height==16 && info->output_components==3,"stdio dimensions");
	while(info->output_scanline<16) Check(jpeg_read_scanlines(info,&pointer,1)==1,"stdio row");
	Check(jpeg_finish_decompress(info),"stdio finish"); R_JPEGRelease(context); Check(!fclose(file) && !live && !remove(path),"stdio caller ownership");
}
/** Generate real grayscale/RGB layouts and unsupported CMYK/progressive streams with the bundled encoder. */
static void ColorLayouts( void ) {
	int kind;
	for(kind=0;kind<4;kind++) {
		rendererJPEG_t *context; rendererJPEGDest_t *destination; struct jpeg_compress_struct *info;
		byte samples[4]={128,64,32,0},rgba[4]={128,64,32,7}; JSAMPROW row=samples;
		Reset(); context=R_JPEGContext(qtrue); info=&context->info.encode;
		if(setjmp(context->error.jump)) Check(0,"color layout encoding failed");
		jpeg_create_compress(info); destination=(*info->mem->alloc_small)((j_common_ptr)info,JPOOL_PERMANENT,sizeof(*destination)); destination->owner=context;
		destination->pub.init_destination=R_JPEGInitDestination; destination->pub.empty_output_buffer=R_JPEGGrowDestination; destination->pub.term_destination=R_JPEGFinishDestination; info->dest=&destination->pub;
		info->image_width=info->image_height=1; info->input_components=kind==0?1:kind==2?4:3; info->in_color_space=kind==0?JCS_GRAYSCALE:kind==2?JCS_CMYK:JCS_RGB;
		jpeg_set_defaults(info); jpeg_set_quality(info,100,TRUE);
		if(kind==1) jpeg_set_colorspace(info,JCS_RGB);
		if(kind==3) jpeg_simple_progression(info);
		jpeg_start_compress(info,TRUE); Check(jpeg_write_scanlines(info,&row,1)==1,"color layout row"); jpeg_finish_compress(info); Write("color.jpg",context->encoded,context->length); R_JPEGRelease(context);
		Reset(); if(kind>=2) Reject(); else { if(kind==0) rgba[1]=rgba[2]=128; Golden(1,1,rgba,1); }
	}
}

/** Cover every small input size/prefix, refill boundaries, malformed dimensions and screenshot failure. */
int main( int argc, char **argv ) {
	byte image[8*16*4], color[8*8*4], tiny[4]={31,31,31,0}, *noise=malloc(128*128*4), *copy=malloc(128*128*4);
	int i,j,size,baseline,position; const int boundaries[]={4095,4096,4097,8191,8192,8193}; unsigned int state=1;
	Check(argc==2 && noise && copy,"fixture arguments/allocation"); ri.Malloc=Allocate; ri.Free=Free; ri.FS_ReadFile=Read; ri.FS_FreeFile=FreeFile; ri.FS_WriteFile=Write; ri.Error=Error; ri.Printf=Print;
	for(i=0;i<8*16;i++) { image[i*4]=image[i*4+1]=image[i*4+2]=i<8*8?32:224; image[i*4+3]=i%256; }
	Reset(); SaveJPG("test.jpg",100,8,16,image); Check(writes==1 && !warnings && !live,"complete screenshot"); originalSize=fixtureSize; memcpy(original,fixture,originalSize);
	for(alignment=0;alignment<4;alignment++) { Reset(); Golden(8,16,image,1); }
	alignment=0;
	for(size=0;size<originalSize;size++) { Reset(); fixtureSize=size; if(Load(1)) { Check(loadedWidth==8 && loadedHeight==16,"prefix dimensions"); for(i=0;i<8*16;i++) Check(loaded[i*4+3]==255,"prefix alpha"); Free(loaded); } }
	fixtureSize=originalSize; memcpy(fixture,original,originalSize);
	Reset(); Check(Load(0),"baseline allocation count"); baseline=allocationCalls; Free(loaded);
	for(i=1;i<=baseline;i++) { Reset(); failAt=i; Reject(); }
	memset(fixture,0x51,4096);
	for(size=0;size<4096;size++) { Reset(); fixtureSize=size; Reject(); }
	for(i=0;i<6;i++) {
		int padding=boundaries[i]-originalSize-4; Check(padding>=0 && padding<=65533,"APP fixture span");
		fixture[0]=255; fixture[1]=216; fixture[2]=255; fixture[3]=239; fixture[4]=(padding+2)>>8; fixture[5]=padding+2;
		memset(fixture+6,0x5a,padding); memcpy(fixture+6+padding,original+2,originalSize-2); fixtureSize=boundaries[i]; Reset(); Golden(8,16,image,1);
	}
	Reset(); R_LoadJPG("nullable.jpg",&loaded,NULL,NULL); Check(loaded && live==1 && fileFrees==1,"nullable dimensions"); Free(loaded);
	for(i=0;i<6;i++) Source(boundaries[i]); Source(0); Source(1); SkipEOF(); Capacity(); Stdio(argv[1]); ColorLayouts();
	memcpy(fixture,original,originalSize); fixtureSize=originalSize;
	position=-1; for(i=0;i<originalSize-21;i++) if(fixture[i]==255 && fixture[i+1]==196) { position=i; break; } Check(position>=0 && fixture[position+4]==0,"baseline DHT");
	memset(fixture+position+5,0,16); fixture[position+5]=3; fixture[position+20]=9; Reset(); Reject();
	memcpy(fixture,original,originalSize); fixture[position+21]=16; Reset(); Reject();
	memcpy(fixture,original,originalSize); fixture[position+4]=4; Reset(); Reject();
	memcpy(fixture,original,originalSize); fixture[position+4]=32; Reset(); Reject();
	memcpy(fixture,original,originalSize);
	position=-1; for(i=0;i<originalSize-8;i++) if(fixture[i]==255 && fixture[i+1]==192) { position=i; break; } Check(position>=0,"baseline SOF");
	fixture[position+5]=fixture[position+7]=65000>>8; fixture[position+6]=fixture[position+8]=65000&255; Reset(); Reject();
	memcpy(fixture,original,originalSize); fixture[position+7]=fixture[position+8]=255; Reset(); Reject();
	memcpy(fixture,original,originalSize); fixture[position+7]=fixture[position+8]=0; Reset(); Reject();
	memcpy(fixture,original,originalSize); Reset(); negativeLength=1; Reject();
	Reset(); missing=1; loaded=(byte *)1; loadedWidth=loadedHeight=-1; R_LoadJPG("missing.jpg",&loaded,&loadedWidth,&loadedHeight); Check(!loaded && !loadedWidth && !loadedHeight && reads==1 && !fileFrees && !live,"missing file");
	for(i=0;i<8*8;i++) { color[i*4]=180; color[i*4+1]=40; color[i*4+2]=90; color[i*4+3]=i; }
	Reset(); SaveJPG("color.jpg",100,8,8,color); Check(writes==1 && !warnings && !live,"YCbCr color screenshot"); Reset(); Golden(8,8,color,2);
	Reset(); SaveJPG("tiny.jpg",100,1,1,tiny); Check(writes==1 && fixtureSize>4 && !warnings && !live,"JPEG larger than raw tiny image"); Reset(); Golden(1,1,tiny,1);
	for(i=0;i<128*128;i++) { state=state*1664525u+1013904223u; noise[i*4]=noise[i*4+1]=noise[i*4+2]=state>>24; noise[i*4+3]=i%256; } memcpy(copy,noise,128*128*4);
	Reset(); SaveJPG("noise.jpg",100,128,128,noise); Check(writes==1 && fixtureSize>4096 && !warnings && !live && !memcmp(copy,noise,128*128*4),"growing screenshot"); baseline=allocationCalls;
	Reset(); Golden(128,128,noise,2);
	for(j=1;j<=baseline;j++) { Reset(); failAt=j; SaveJPG("failed.jpg",100,128,128,noise); Check(!writes && warnings==1 && !live && !memcmp(copy,noise,128*128*4),"exhausted screenshot cleanup"); }
	Reset(); Check(!jpeg_get_small(NULL,(size_t)R_IMAGE_MAX_BYTES+1) && !jpeg_get_large(NULL,(size_t)-1) && !allocationCalls,"native JPEG allocation size guard");
	Reset(); SaveJPG("invalid.jpg",100,0,1,tiny); SaveJPG("invalid.jpg",100,-1,1,tiny); SaveJPG("invalid.jpg",100,1,0,tiny); SaveJPG("invalid.jpg",100,65000,65000,tiny); SaveJPG("invalid.jpg",100,65535,1,tiny); SaveJPG("invalid.jpg",100,1,1,NULL);
	Check(warnings==6 && !writes && !allocationCalls && !live,"invalid screenshot rejected before allocation"); free(copy); free(noise);
	puts("JPEG source, recovery, RGB/RGBA, growth and ownership regressions passed (issue #43)"); return 0;
}
