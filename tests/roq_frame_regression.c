/* Issue #41: reuse exact file/engine stubs, then exercise real quad geometry and renderer output. */
#define main RoQStreamRegressions
#include "roq_stream_regression.c"
#undef main

refexport_t re;
static void *tempMemory;
static int tempBytes, draws, uploads, patchMode;
static int sourceWidth, sourceHeight, textureWidth, textureHeight;

/** Allocate exactly the texture bytes; every renderer callback reads that complete allocation. */
void *Hunk_AllocateTempMemory( int size ) {
	Check(size>0 && size<=(int)sizeof(cin.linbuf)/2 && !tempMemory,"texture allocation bounds");
	tempMemory=malloc(size); Check(tempMemory!=NULL,"texture allocation"); tempBytes=size; return tempMemory;
}
void Hunk_FreeTempMemory( void *memory ) { Check(memory==tempMemory,"texture ownership"); free(tempMemory); tempMemory=NULL; }
void SCR_AdjustFrom640( float *x, float *y, float *w, float *h ) { (void)x; (void)y; (void)w; (void)h; }
/** Check every submitted pixel, including resampling of the final source row and column. */
static void Pixels( int cols, int rows, const byte *data ) {
	int x,y,offset,sx,sy,blue;
	Check(cols==textureWidth && rows==textureHeight,"renderer texture geometry");
	if(tempMemory) Check(tempBytes==cols*rows*4,"texture byte size");
	for(y=0;y<rows;y++) for(x=0;x<cols;x++) {
		sx=x*sourceWidth/cols; sy=y*sourceHeight/rows; offset=(y*cols+x)*4;
		blue=!x && !y && patchMode ? (patchMode==1 ? 6 : 2) : 64;
		Check(data[offset]==sx%128 && data[offset+1]==sy%128 && data[offset+2]==blue && data[offset+3]==255,"renderer pixel or source bounds");
	}
}
static void Draw( int x,int y,int w,int h,int cols,int rows,const byte *data,int client,qboolean dirty ) {
	(void)x; (void)y; (void)w; (void)h; (void)dirty; Check(client==0,"draw client"); Pixels(cols,rows,data); draws++;
}
static void Upload( int w,int h,int cols,int rows,const byte *data,int client,qboolean dirty ) {
	(void)dirty; Check(w==cols && h==rows && client==0,"upload dimensions"); Pixels(cols,rows,data); uploads++;
}

/** Encode standard fixed quad sizes around candidate dimensions. */
static void Info( unsigned int width, unsigned int height ) {
	Fixture(ROQ_QUAD_INFO,8,0,24);
	source[16]=width; source[17]=width>>8; source[18]=height; source[19]=height>>8;
	source[20]=8; source[21]=0; source[22]=4; source[23]=0;
}
/** Validate all constructed 8x8/4x4 blocks in both halves and the termination reservation. */
static void Geometry( unsigned int width, unsigned int height ) {
	int handle,i,size;
	long offset,last,expected=(width/8)*(height/8)*5;
	Info(width,height); glConfig.maxTextureSize=1024; glConfig.hardwareType=GLHW_GENERIC;
	handle=CIN_PlayCinematic("test.roq",0,0,32,32,0); Check(handle==0,"geometry open");
	cin.qStatus[0][expected+64]=cin.qStatus[1][expected+64]=(byte *)1;
	Check(CIN_RunCinematic(handle)==FMV_PLAY,"valid quad geometry rejected");
	Check(cinTable[handle].onQuad==expected && cinTable[handle].screenDelta==width*height*4 &&
	      cinTable[handle].t[0]==width*height*4 && cinTable[handle].t[1]==-(long)(width*height*4),"frame products and signed offsets");
	for(i=0;i<expected;i++) {
		size=i%5 ? 4 : 8; offset=cin.qStatus[0][i]-cin.linbuf;
		last=offset+(size-1)*width*4+size*4;
		Check(offset>=0 && last<=cinTable[handle].screenDelta && offset%(width*4)+size*4<=width*4,"quad crosses first frame");
		Check(cin.qStatus[1][i]-cin.qStatus[0][i]==cinTable[handle].screenDelta,"quad second frame offset");
	}
	for(i=expected;i<expected+64;i++) Check(!cin.qStatus[0][i] && !cin.qStatus[1][i],"quad terminator bounds");
	Check(cin.qStatus[0][expected+64]==(byte *)1 && cin.qStatus[1][expected+64]==(byte *)1,"quad reservation overrun");
	Check(CIN_StopCinematic(handle)==FMV_EOF && closes==1 && ends==1,"geometry stop");
}
/** Invalid geometry must stop without applying dimensions or constructing any blocks. */
static void BadInfo( unsigned int width, unsigned int height, int maxsize, int minsize ) {
	int handle; Info(width,height); source[20]=maxsize; source[22]=minsize;
	handle=CIN_PlayCinematic("test.roq",0,0,32,32,CIN_loop); Check(handle==0,"bad geometry header open");
	Check(CIN_RunCinematic(handle)==FMV_EOF && !cinTable[handle].screenDelta && !cinTable[handle].onQuad &&
	      !cinTable[handle].xsize && !cinTable[handle].ysize && closes==1 && opens==1 && ends==1,"invalid geometry applied or looped");
}
/** Use an exact standalone source frame so source reads outside its half trip ASan. */
static void Render( unsigned int width, unsigned int height, int maxTexture, int hardware, int targetWidth, int targetHeight ) {
	int handle,x,y,offset;
	byte *frame;
	cin_cache *movie;
	Info(width,height); glConfig.maxTextureSize=maxTexture; glConfig.hardwareType=hardware;
	handle=CIN_PlayCinematic("test.roq",0,0,32,32,0); Check(handle==0 && CIN_RunCinematic(handle)==FMV_PLAY,"render geometry");
	movie=&cinTable[handle];
	Check(movie->drawX==targetWidth && movie->drawY==targetHeight,"draw hardware/source limit");
	frame=malloc(movie->screenDelta); Check(frame!=NULL,"exact source frame allocation"); movie->buf=frame;
	for(y=0;y<height;y++) for(x=0;x<width;x++) {
		offset=(y*width+x)*4; frame[offset]=x%128; frame[offset+1]=y%128; frame[offset+2]=64; frame[offset+3]=255;
	}
	patchMode=0;
	if(width==512 && height==512 && targetWidth==256 && targetHeight==256) {
		frame[2]=0; frame[6]=4; frame[width*4+2]=8; frame[width*4+6]=12; patchMode=1;
	} else if(width==512 && height==256 && targetWidth==256 && targetHeight==256) {
		frame[2]=0; frame[6]=4; patchMode=2;
	}
	sourceWidth=width; sourceHeight=height; textureWidth=targetWidth; textureHeight=targetHeight;
	draws=uploads=0; movie->dirty=qtrue;
	CIN_DrawCinematic(handle); Check(draws==1 && !tempMemory,"preview texture cleanup");
	movie->dirty=qtrue; CIN_UploadCinematic(handle); Check(uploads==1 && !tempMemory,"videoMap texture cleanup");
	Check(CIN_StopCinematic(handle)==FMV_EOF,"render stop");
	CIN_DrawCinematic(handle); CIN_UploadCinematic(handle); Check(draws==1 && uploads==1,"stopped frame submitted");
	free(frame);
}

/** Cover minimum/retail/max-area geometry, invalid products, rectangular resizing, and native averages. */
int main( void ) {
	int i;
	unsigned int invalid[][2]={{0,8},{8,0},{1,8},{8,7},{9,8},{8,9},{65535,65535},{512,520},{16,16392},{65528,8}};
	timescale.value=1; video.integer=1; re.DrawStretchRaw=Draw; re.UploadCinematic=Upload;
	for(i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++) BadInfo(invalid[i][0],invalid[i][1],8,4);
	BadInfo(8,8,4,4); BadInfo(8,8,8,8);
	Geometry(8,8); Geometry(16,16); Geometry(24,24); Geometry(256,256); Geometry(512,256); Geometry(512,512);
	Geometry(8,32768); Geometry(32768,8);
	Fixture(ROQ_QUAD_VQ,2,0,18); RunToClose(CIN_loop,0,0);
	Render(8,8,1024,GLHW_GENERIC,8,8);
	Render(8,512,256,GLHW_GENERIC,8,256); Render(512,8,256,GLHW_GENERIC,256,8);
	Render(24,264,256,GLHW_GENERIC,16,256);
	Render(512,512,1024,GLHW_GENERIC,512,512);
	Render(512,512,256,GLHW_GENERIC,256,256); Render(512,256,256,GLHW_GENERIC,256,256);
	Render(256,512,256,GLHW_GENERIC,256,256);
	Render(512,512,128,GLHW_GENERIC,128,128);
	Render(512,512,1024,GLHW_RAGEPRO,256,256);
	Render(8,32768,1024,GLHW_GENERIC,8,1024); Render(32768,8,1024,GLHW_GENERIC,1024,8);
	free(source); puts("RoQ frame geometry and renderer bounds regressions passed (issue #41)"); return 0;
}
