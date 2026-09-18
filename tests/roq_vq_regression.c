/* Issue #41: checked native codebooks and VQ preflight, using exact payload allocations. */
#define main RoQStreamRegressions
#include "roq_stream_regression.c"
#undef main

static byte book[2560], stream[4096], beforeTwo[sizeof(vq2)], beforeFour[sizeof(vq4)], beforeEight[sizeof(vq8)];
static byte beforeFrame[sizeof(cin.linbuf)];
static int streamSize, codes, wordPosition;
static unsigned int controlWord;

/** Check known neutral-chroma rounding for the small luminance values in this fixture. */
static void Pixel( const byte *pixel, int luminance ) {
	Check(pixel[0]==luminance+1 && pixel[1]==(luminance<12 ? (luminance ? luminance-1 : 0) : luminance) &&
	      pixel[2]==luminance+1 && pixel[3]==255,"codebook pixel order/color");
}
/** Build every 2x2 entry and all four references for each 4x4/8x8 entry. */
static void Book( void ) {
	int i,j;
	for(i=0;i<256;i++) {
		for(j=0;j<4;j++) book[i*6+j]=(i%8)*16+j*4;
		book[i*6+4]=book[i*6+5]=128;
		for(j=0;j<4;j++) book[1536+i*4+j]=(i*4+j)%256;
	}
}
/** Retain byte snapshots to prove rejected or partial updates do not touch other entries. */
static void SaveBook( void ) { memcpy(beforeTwo,vq2,sizeof(vq2)); memcpy(beforeFour,vq4,sizeof(vq4)); memcpy(beforeEight,vq8,sizeof(vq8)); }
static void SameBook( void ) { Check(!memcmp(beforeTwo,vq2,sizeof(vq2)) && !memcmp(beforeFour,vq4,sizeof(vq4)) && !memcmp(beforeEight,vq8,sizeof(vq8)),"rejected codebook changed tables"); }
/** Cover every full-book truncation, both full sizes and legacy partial-update flags. */
static void Codebooks( void ) {
	int length,i,x,y,q,index,luminance;
	byte *exact,partial[]={16,16,16,16,128,128};
	Book(); memset(vq2,0xa5,sizeof(vq2)); memset(vq4,0xa5,sizeof(vq4)); memset(vq8,0xa5,sizeof(vq8)); SaveBook();
	for(length=0;length<sizeof(book);length++) {
		exact=malloc(length ? length : 1); Check(exact!=NULL,"exact codebook allocation"); memcpy(exact,book,length);
		Check(!decodeCodeBook(exact,exact+length,0),"truncated full codebook accepted"); SameBook(); free(exact);
	}
	Check(decodeCodeBook(book,book+sizeof(book),0),"full codebook rejected");
	Check(sizeof(vq2)==4096 && sizeof(vq4)==16384 && sizeof(vq8)==65536,"fixed RGBA table capacity");
	for(i=0;i<256;i++) {
		for(q=0;q<4;q++) Pixel(vq2[i]+q*4,(i%8)*16+q*4);
		for(y=0;y<4;y++) for(x=0;x<4;x++) {
			index=(i*4+(y/2)*2+x/2)%256; luminance=(index%8)*16+((y%2)*2+x%2)*4;
			Pixel(vq4[i]+(y*4+x)*4,luminance);
		}
		for(y=0;y<8;y++) for(x=0;x<8;x++) {
			index=(i*4+(y/4)*2+x/4)%256; luminance=(index%8)*16+(((y/2)%2)*2+(x/2)%2)*4;
			Pixel(vq8[i]+(y*8+x)*4,luminance);
		}
	}
	SaveBook();
	Check(decodeCodeBook(partial,partial+6,0x0100),"2x2-only update rejected");
	for(q=0;q<4;q++) Pixel(vq2[0]+q*4,16);
	Check(!memcmp((byte *)vq2+16,beforeTwo+16,sizeof(vq2)-16) && !memcmp(beforeFour,vq4,sizeof(vq4)) && !memcmp(beforeEight,vq8,sizeof(vq8)),"partial update changed unrelated entries");
	Check(decodeCodeBook(book,book+sizeof(book),0),"restore full codebook");
	SaveBook(); exact=malloc(1540); Check(exact!=NULL,"partial codebook allocation"); memcpy(exact,book,1536); memset(exact+1536,255,4);
	Check(decodeCodeBook(exact,exact+1540,1),"default 256 2x2/one 4x4 update rejected");
	Check(!memcmp((byte *)vq4+64,beforeFour+64,sizeof(vq4)-64) && !memcmp((byte *)vq8+256,beforeEight+256,sizeof(vq8)-256),"one-entry update changed other 4x4/8x8 entries");
	for(q=0;q<4;q++) for(i=0;i<4;i++) Pixel(vq4[0]+((q/2)*2+i/2)*16+((q%2)*2+i%2)*4,112+i*4);
	free(exact); Check(decodeCodeBook(book,book+sizeof(book),0),"restore full codebook after one-entry update");
}

/** Pack control words where the decoder refills them, interleaved with operand bytes. */
static void Stream( void ) { streamSize=codes=0; controlWord=0; wordPosition=0; memset(stream,0,sizeof(stream)); }
static void Code( unsigned int code ) {
	if(!(codes%8)) { wordPosition=streamSize; streamSize+=2; controlWord=0; }
	controlWord|=code<<(14-2*(codes%8)); stream[wordPosition]=controlWord; stream[wordPosition+1]=controlWord>>8; codes++;
}
static void Index( unsigned int index ) { Check(streamSize<sizeof(stream),"VQ fixture capacity"); stream[streamSize++]=index; }
/** Decode geometry using actual public playback, then select either frame half for the VQ fixture. */
static void Prepare( unsigned int width, unsigned int height, int half ) {
	int handle; cin_cache *movie;
	Fixture(ROQ_QUAD_INFO,8,0,24); glConfig.maxTextureSize=1024;
	source[16]=width; source[17]=width>>8; source[18]=height; source[19]=height>>8;
	source[20]=8; source[21]=0; source[22]=4; source[23]=0;
	handle=CIN_PlayCinematic("test.roq",0,0,32,32,0); Check(handle==0 && CIN_RunCinematic(handle)==FMV_PLAY,"VQ geometry");
	movie=&cinTable[handle]; movie->numQuads=half; movie->normalBuffer0=movie->t[half];
	memset(cin.linbuf,0xcc,sizeof(cin.linbuf));
	memset(cin.linbuf+half*movie->screenDelta,0xab,movie->screenDelta);
	memset(cin.linbuf+(1-half)*movie->screenDelta,0x34,movie->screenDelta);
	RoQPrepMcomp(0,0);
}
/** Reject exact payload prefixes atomically, even when an early block would have been valid. */
static void Reject( byte **status, int length ) {
	byte *exact=malloc(length ? length : 1); Check(exact!=NULL,"exact VQ allocation"); memcpy(exact,stream,length);
	memcpy(beforeFrame,cin.linbuf,sizeof(cin.linbuf)); SaveBook();
	if(blitVQQuad32fs(status,exact,exact+length)) {
		fprintf(stderr,"Accepted VQ rejection fixture: %ux%u, half %ld, length %d/%d, first %02x %02x %02x\n",cinTable[0].xsize,cinTable[0].ysize,cinTable[0].numQuads,length,streamSize,stream[0],stream[1],stream[2]);
		Check(0,"invalid VQ accepted");
	}
	Check(!memcmp(beforeFrame,cin.linbuf,sizeof(cin.linbuf)),"rejected VQ partially wrote a frame"); SameBook(); free(exact);
}
/** Mix every root/sub-block opcode, including four 2x2 references and signed motion offsets. */
static void Mixed( void ) {
	Stream(); Code(2); Index(0); Code(3); Code(2); Index(0); Code(3);
	Index(0); Index(1); Index(2); Index(3); Code(1); Index(136); Code(0);
	Code(1); Index(136); Code(0);
}
/** Check the complete mixed output and the untouched opposite frame/unused allocation. */
static void MixedPixels( int half ) {
	int x,y,index,luminance,i; cin_cache *movie=&cinTable[0];
	byte *output=cin.linbuf+half*movie->screenDelta;
	for(y=0;y<16;y++) for(x=0;x<16;x++) {
		byte *pixel=output+(y*16+x)*4;
		if(x<8 && y<8) {
			index=(y/4)*2+x/4; luminance=index*16+(((y/2)%2)*2+(x/2)%2)*4; Pixel(pixel,luminance);
		} else if(x>=8 && y<4) {
			index=(y/2)*2+((x-8)%4)/2; luminance=index*16+((y%2)*2+x%2)*4; Pixel(pixel,luminance);
		} else {
			int fill=(x<8 || (x<12 && y<8)) ? 0x34 : 0xab;
			for(i=0;i<4;i++) Check(pixel[i]==fill,"motion or skipped pixel changed");
		}
	}
	for(i=0;i<movie->screenDelta;i++) Check(cin.linbuf[(1-half)*movie->screenDelta+i]==0x34,"motion source written");
	for(i=2*movie->screenDelta;i<sizeof(cin.linbuf);i++) Check(cin.linbuf[i]==0xcc,"write beyond both frame halves");
}

/** Cover complete code streams, every small truncation, control-word refill and motion edges. */
int main( void ) {
	int half,length,i,x,y;
	roqChunk_t chunk;
	timescale.value=1; video.integer=1; ROQ_GenYUVTables(); Codebooks();
	for(half=0;half<2;half++) {
		Prepare(16,16,half); Mixed();
		for(length=0;length<streamSize;length++) Reject(cin.qStatus[half],length);
		Check(blitVQQuad32fs(cin.qStatus[half],stream,stream+streamSize),"mixed VQ rejected"); MixedPixels(half);
		stream[streamSize]=0xff; stream[streamSize+1]=0xff;
		Check(blitVQQuad32fs(cin.qStatus[half],stream,stream+streamSize+2),"legacy trailing VQ padding rejected"); MixedPixels(half);
		Check(CIN_StopCinematic(0)==FMV_EOF,"mixed VQ stop");
	}
	Prepare(8,72,0); Stream(); for(i=0;i<9;i++) Code(0);
	for(length=0;length<streamSize;length++) Reject(cin.qStatus[0],length);
	Check(blitVQQuad32fs(cin.qStatus[0],stream,stream+streamSize),"second control word rejected"); CIN_StopCinematic(0);
	Prepare(512,512,0); Stream(); for(i=0;i<4096;i++) Code(0);
	Check(blitVQQuad32fs(cin.qStatus[0],stream,stream+streamSize),"maximum frame skip rejected"); CIN_StopCinematic(0);
	Prepare(8,8,0); Stream(); Code(2); Index(255);
	Check(blitVQQuad32fs(cin.qStatus[0],stream,stream+streamSize),"last codebook index rejected");
	for(y=0;y<8;y++) Check(!memcmp(cin.linbuf+y*32,vq8[255]+y*32,32),"last codebook copied incorrectly"); CIN_StopCinematic(0);
	/* The second block would cross a row while its address still fits the combined allocation. */
	Prepare(16,16,0); Stream(); Code(2); Index(0); Code(1); Index(120); Code(0); Code(0); Reject(cin.qStatus[0],streamSize); CIN_StopCinematic(0);
	/* All motion indices at the top-left of an 8x8 frame: only a zero linear offset fits. */
	Prepare(8,8,0);
	for(i=0;i<256;i++) { Stream(); Code(1); Index(i); if(i!=136 && i!=9) Reject(cin.qStatus[0],streamSize); }
	Stream(); Code(1); Index(136); Check(blitVQQuad32fs(cin.qStatus[0],stream,stream+streamSize),"zero displacement motion rejected"); CIN_StopCinematic(0);
	Prepare(16,16,0); Stream(); Code(0); Code(0); Code(1); Index(135); Code(0); Reject(cin.qStatus[0],streamSize);
	Stream(); Code(1); Index(136); Code(0); Code(0); Code(0); RoQPrepMcomp(-128,127); Reject(cin.qStatus[0],streamSize); CIN_StopCinematic(0);
	/* An odd-pixel source requires no double alignment; the 4:1 aspect rule retains doubled motion. */
	for(i=0;i<2;i++) {
		int width=i ? 64 : 16, shift=i ? 2 : 1; cin_cache *movie;
		Prepare(width,16,0); movie=&cinTable[0];
		for(y=0;y<16;y++) for(x=0;x<width;x++) { byte *p=cin.linbuf+movie->screenDelta+(y*width+x)*4; p[0]=x; p[1]=y; p[2]=64; p[3]=255; }
		Stream(); Code(1); Index(120); for(x=1;x<(width/8)*2;x++) Code(0);
		Check(blitVQQuad32fs(cin.qStatus[0],stream,stream+streamSize),"unaligned/aspect motion rejected");
		for(y=0;y<8;y++) for(x=0;x<8;x++) { byte *p=cin.linbuf+(y*width+x)*4; Check(p[0]==x+shift && p[1]==y && p[2]==64 && p[3]==255,"unaligned/aspect motion pixels"); }
		CIN_StopCinematic(0);
	}
	/* Successful outer dispatch invalidates cached scaled pixels; rejected dispatch preserves frames/counters. */
	Prepare(16,16,0); Mixed(); memset(&chunk,0,sizeof(chunk)); chunk.id=ROQ_QUAD_VQ; chunk.size=streamSize;
	cin.scaledValid=qtrue;
	Check(RoQDecodeChunk(&chunk,stream,stream+streamSize,0) && !cin.scaledValid && cinTable[0].numQuads==1 && cinTable[0].dirty,"successful VQ dispatch state");
	Check(!memcmp(cin.linbuf,cin.linbuf+cinTable[0].screenDelta,cinTable[0].screenDelta),"first frame initialization"); CIN_StopCinematic(0);
	Prepare(16,16,0); Mixed(); memcpy(beforeFrame,cin.linbuf,sizeof(cin.linbuf));
	Check(!RoQDecodeChunk(&chunk,stream,stream+1,0) && !cinTable[0].numQuads && !memcmp(beforeFrame,cin.linbuf,sizeof(cin.linbuf)),"rejected VQ dispatch changed state"); CIN_StopCinematic(0);
	free(source); puts("RoQ codebook and VQ cursor/motion regressions passed (issue #41)"); return 0;
}
