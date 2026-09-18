/* Issue #46: actual enabled native generation flow with isolated FT/FS/GL. */
#include "renderer_image_gl_stub.h"
#include "font_freetype_stub.h"
#define BUILD_FREETYPE
#include Q3_FONT_FREETYPE_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
refimport_t ri;
static cvar_t save;
cvar_t *r_saveFontData=&save;
static image_t image;
static FT_GlyphSlotRec slot;
static int glyphPixels=4;
static byte pageAlpha[256][256*256];
static byte serialized[20548];
static int dataWrites,legacyInput,legacyShaderCalls;
static byte smallTga[64];
static void *file,*zones[4096];
static int zoneSizes[4096],zoneCount,mode,newFaces,doneFaces,aliveFace,reads,frees,images,writes,initCalls,doneLibraries;
static void Check(int condition,const char *message) { if(!condition) { fprintf(stderr,"FreeType regression failed: %s\n",message);exit(1); } }
void QDECL Com_Error(int level,const char *format,...) { (void)level;(void)format;Check(0,"unexpected native error");exit(1); }
void QDECL Com_Printf(const char *format,...) { (void)format; }
static void QDECL Print(int level,const char *format,...) { (void)level;(void)format; }
void R_SyncRenderThread(void) {}
#ifndef Com_Memset
void Com_Memset(void *destination,int value,size_t size) { memset(destination,value,size); }
#endif
#ifndef Com_Memcpy
void Com_Memcpy(void *destination,const void *source,size_t size) { memcpy(destination,source,size); }
#endif
void *Z_Malloc(int size) { void *p;Check(size>0 && size<2000000 && zoneCount<4096,"bounded native temporary font allocation");if((mode==3 && size==65536) || (mode==4 && size==262144) || (mode==11 && size==sizeof(FT_Bitmap)) || (mode==12 && size==16) || (mode==15 && size==262162) || (mode==19 && size==20548))return NULL;p=calloc(1,size);Check(p!=NULL,"exact native allocation");zones[zoneCount]=p;zoneSizes[zoneCount++]=size;return p; }
void Z_Free(void *pointer) { int i;for(i=0;i<zoneCount;i++)if(zones[i]==pointer) { free(pointer);zones[i]=zones[--zoneCount];zoneSizes[i]=zoneSizes[zoneCount];return; }Check(0,"temporary zone free exactly once"); }
static int Read(const char *name,void **buffer) { Check(name && buffer,"actual FS read");reads++;if(strstr(name,".dat")) { if(legacyInput) { file=malloc(20548);Check(file!=NULL,"exact generated legacy input");memcpy(file,serialized,20548);*buffer=file;return 20548; }*buffer=NULL;return -1; }Check(!file,"one owned face input");if(mode==8) { *buffer=NULL;return 4; }file=malloc(4);memcpy(file,"font",4);*buffer=file;return mode==6?0:mode==7?-1:4; }
static void FreeFile(void *pointer) { Check(pointer && pointer==file && !aliveFace,"face closes before freeing its retained input");free(pointer);file=NULL;frees++; }
static void Write(const char *name,const void *data,int length) { Check(name && data && length>0,"native output write");if(strstr(name,".dat")) { Check(length==20548,"unchanged legacy serialized byte count");memcpy(serialized,data,length);dataWrites++; }else if(length<=64)memcpy(smallTga,data,length);writes++; }
qhandle_t RE_RegisterShaderNoMip(const char *name) { Check(legacyInput && legacyShaderCalls<256 && !strcmp(name,(char *)serialized+legacyShaderCalls*80+48),"generated legacy file registers each exact bounded glyph name");return 70+legacyShaderCalls++; }
image_t *R_CreateImage(const char *name,const byte *pixels,int width,int height,qboolean mipmap,qboolean picmip,int wrap) { Check(name && pixels && width==256 && height==256 && !mipmap && !picmip && wrap==GL_CLAMP,"native font image import");if(mode==14)return NULL;Check(images<256,"native page count cannot exceed glyph count");for(int i=0;i<65536;i++) { Check(pixels[i*4]==255 && pixels[i*4+1]==255 && pixels[i*4+2]==255,"native white font page RGB");pageAlpha[images][i]=pixels[i*4+3]; }images++;return &image; }
qhandle_t RE_RegisterShaderFromImage(const char *name,int lightmap,image_t *texture,qboolean mipmap) { Check(name && lightmap==LIGHTMAP_2D && texture==&image && !mipmap,"native image shader import");return 10+images; }
int FT_Init_FreeType(FT_Library *library) { initCalls++;*library=mode==9?NULL:&slot;return mode==9; }
int FT_Done_FreeType(FT_Library library) { Check(library==&slot && !aliveFace,"library shutdown with no owned faces");doneLibraries++;return 0; }
int FT_New_Memory_Face(FT_Library library,const void *data,long length,long index,FT_Face *face) { Check(library==&slot && data==file && length==4 && index==0,"native FT memory face retains owned FS input");newFaces++;if(mode==1)return 1;*face=malloc(sizeof(**face));Check(*face!=NULL,"fixture face allocation");(*face)->glyph=&slot;aliveFace=1;return 0; }
int FT_Set_Char_Size(FT_Face face,long width,long height,unsigned int horizontal,unsigned int vertical) { Check(face && aliveFace && width==(mode==25?(INT_MAX/64)*64:768) && height==width && horizontal==72 && vertical==72,"native character size");return mode==2; }
int FT_Done_Face(FT_Face face) { Check(face && aliveFace && file,"face shutdown while backing input remains owned");free(face);aliveFace=0;doneFaces++;return 0; }
unsigned int FT_Get_Char_Index(FT_Face face,unsigned long character) { Check(face && aliveFace && character<=255,"native glyph index import");return character; }
int FT_Load_Glyph(FT_Face face,unsigned int index,int flags) { Check(face && aliveFace && index<=255 && flags==FT_LOAD_DEFAULT,"native glyph load");memset(&slot,0,sizeof(slot));slot.metrics.width=glyphPixels*64;slot.metrics.height=glyphPixels*64;slot.metrics.horiBearingY=glyphPixels*64;slot.metrics.horiAdvance=glyphPixels*64;slot.format=mode==23?0:ft_glyph_format_outline;if(mode==21) { slot.metrics.width=LONG_MAX;slot.metrics.horiBearingX=1; }if(mode==22)slot.metrics.width=-1;if(mode==24 && index==255) { slot.metrics.height=254*64;slot.metrics.horiBearingY=254*64; }return mode==10; }
void FT_Outline_Translate(FT_Outline *outline,long x,long y) { Check(outline==&slot.outline && x==0 && y==0,"native outline translation"); }
int FT_Outline_Get_Bitmap(FT_Library library,FT_Outline *outline,FT_Bitmap *bitmap) { Check(library==&slot && outline==&slot.outline && bitmap && bitmap->pitch==((glyphPixels+3)&-4) && bitmap->rows==glyphPixels,"native glyph bitmap import");memset(bitmap->buffer,64,bitmap->pitch*bitmap->rows);return mode==13; }
static void Case(int scenario) {
	fontInfo_t font,zero;int expectedReads,expectedFrees,expectedNew,expectedDone;const char *name="fixture.ttf";int pointSize=12;int valid;
	Check(!file && !zoneCount && !aliveFace && !ftLibrary,"previous native ownership fully released");
	mode=scenario;glyphPixels=4;dataWrites=legacyInput=legacyShaderCalls=0;newFaces=doneFaces=reads=frees=images=writes=initCalls=doneLibraries=0;save.integer=mode==15 || mode==19;
	if(mode==16)name=NULL;if(mode==17)name="";if(mode==18)pointSize=INT_MAX;if(mode==20)glyphPixels=254;if(mode==25) { glyphPixels=252;pointSize=INT_MAX/64; }
	memset(&font,0xa5,sizeof(font));memset(&zero,0,sizeof(zero));R_InitFreeType();if(mode!=9)R_InitFreeType();
	Check(initCalls==1,"successful repeat init reuses the owned FT library");
	RE_RegisterFont(name,pointSize,&font);
	expectedReads=(mode==9 || (mode>=16 && mode<=18))?1:2;expectedFrees=(mode==8 || mode==9 || (mode>=16 && mode<=18))?0:1;
	expectedNew=(mode>=6 && mode<=9) || (mode>=16 && mode<=18)?0:1;expectedDone=expectedNew && mode!=1;
	Check(reads==expectedReads && frees==expectedFrees && !file && !zoneCount && !aliveFace && newFaces==expectedNew && doneFaces==expectedDone,"face/file/temporary ownership balances on every success and failure");
	valid=mode==0 || mode==15 || mode==19;
	if(valid)Check(registeredFontCount==1 && images==1 && font.glyphScale==4.0f && font.glyphs[0].glyph==11 && font.glyphs[0].xSkip==5,"successful generation keeps native output");
	else Check(!registeredFontCount && !memcmp(&font,&zero,sizeof(font)),"controlled failures publish zero default output and no font cache");
	if(mode==19)Check(writes==1 && dataWrites==0,"optional legacy output allocation failure retains usable generated font");
	if(mode==25)Check(images==100,"overlong generated serialized name rejects before importing the offending page");
	if(mode==15)Check(writes==1,"optional TGA output allocation failure does not leak or discard usable generated font");
	R_DoneFreeType();R_DoneFreeType();Check(doneLibraries==(mode==9?0:1) && !ftLibrary && !registeredFontCount,"idempotent shutdown releases the owned library/cache");
}
static void Metrics(void) {
	int values[7],expectedValues[]={-128,64,3,192,0,3,4};int scenario,i;
	memset(&slot,0,sizeof(slot));slot.metrics.horiBearingX=-65;slot.metrics.width=129;slot.metrics.horiBearingY=129;slot.metrics.height=129;
	Check(R_GetGlyphInfo(&slot,&values[0],&values[1],&values[2],&values[3],&values[4],&values[5],&values[6]) && !memcmp(values,expectedValues,sizeof(values)),"fractional/negative legacy metric rounding remains exact");
	for(scenario=0;scenario<12;scenario++) {
		memset(&slot,0,sizeof(slot));slot.metrics.width=256;slot.metrics.height=256;slot.metrics.horiBearingY=256;
		if(scenario==0) { slot.metrics.width=LONG_MAX;slot.metrics.horiBearingX=1; }
		if(scenario==1)slot.metrics.height=LONG_MAX;
		if(scenario==2)slot.metrics.horiBearingX=LONG_MIN;
		if(scenario==3)slot.metrics.horiBearingY=LONG_MIN;
		if(scenario==4)slot.metrics.horiAdvance=LONG_MAX;
		if(scenario==5) { slot.metrics.horiBearingX=INT_MAX;slot.metrics.width=64; }
		if(scenario==6)slot.metrics.horiBearingX=INT_MIN;
		if(scenario==7) { slot.metrics.horiBearingY=INT_MIN;slot.metrics.height=64; }
		if(scenario==8)slot.metrics.width=-1;
		if(scenario==9)slot.metrics.height=-1;
		if(scenario==10)slot.metrics.width=253*64;
		if(scenario==11) { slot.metrics.height=254*64;slot.metrics.horiBearingY=254*64; }
		for(i=0;i<7;i++)values[i]=777;
		Check(!R_GetGlyphInfo(&slot,&values[0],&values[1],&values[2],&values[3],&values[4],&values[5],&values[6]),"oversized/negative/nonrepresentable native metrics reject before arithmetic/allocation");
		for(i=0;i<7;i++)Check(values[i]==777,"failed native metric query keeps output untouched");
	}
	Check(!zoneCount,"metric rejection imports no allocations");
}
static void VerifyGlyphs(const fontInfo_t *font) {
	int glyph,x,y,width,height,page,row,column;
	for(glyph=0;glyph<256;glyph++) {
		const glyphInfo_t *g=&font->glyphs[glyph];page=g->glyph-11;x=(int)(g->s*256);y=(int)(g->t*256);width=g->imageWidth;height=g->imageHeight;
		Check(page>=0 && page<images && x>=0 && y>=0 && x+width<=256 && y+height<=256 && g->s2<=1.0f && g->t2<=1.0f,"each glyph including page boundaries/final glyph is present within its page");
		for(row=0;row<height;row++)for(column=0;column<width;column++)Check(pageAlpha[page][(y+row)*256+x+column]==255,"actual uploaded page contains every declared glyph rectangle");
	}
}
static void Atlases(void) {
	const int sizes[]={0,1,4,64,252};int size,beforeReads,beforeImages;fontInfo_t font,cached;
	for(size=0;size<5;size++) {
		Check(!file && !zoneCount && !aliveFace && !ftLibrary,"prior atlas ownership balance");mode=0;legacyInput=legacyShaderCalls=0;newFaces=doneFaces=reads=frees=images=writes=dataWrites=0;glyphPixels=sizes[size];save.integer=0;
		R_InitFreeType();RE_RegisterFont("fixture.ttf",12,&font);Check(registeredFontCount==1 && !file && !zoneCount && !aliveFace && doneFaces==1 && frees==1,"native atlas output and resource balance");VerifyGlyphs(&font);
		Check(!strcmp(font.name,"fonts/fontImage_12.dat"),"generated font cache uses its serialized asset path");beforeReads=reads;beforeImages=images;RE_RegisterFont("fixture.ttf",12,&cached);
		Check(!memcmp(&font,&cached,sizeof(font)) && reads==beforeReads && images==beforeImages,"generated cache reuse imports no face/file/page");R_DoneFreeType();
	}
	glyphPixels=4;
}
static void TgaBounds(void) {
	const int sizes[][2]={{INT_MAX,1},{65536,1},{1,65536},{0,1},{1,0},{-1,1},{1,-1},{65535,65535}};
	const byte pixels[]={1,2,3,4,5,6,7,8};const byte payload[]={3,2,1,4,7,6,5,8};int i,before=writes;
	for(i=0;i<8;i++)WriteTGA("fixture.tga",(byte *)pixels,sizes[i][0],sizes[i][1]);
	WriteTGA(NULL,(byte *)pixels,2,1);WriteTGA("fixture.tga",NULL,2,1);
	Check(writes==before && !zoneCount,"invalid TGA shape/input rejects before temporary allocation/read/write");
	WriteTGA("fixture.tga",(byte *)pixels,2,1);
	Check(writes==before+1 && !zoneCount && smallTga[2]==2 && smallTga[12]==2 && smallTga[13]==0 && smallTga[14]==1 && smallTga[16]==32 && !memcmp(smallTga+18,payload,8),"native valid TGA header and RGBA-to-BGRA payload remain exact with balanced ownership");
}
static void Encoding(void) {
	fontInfo_t font,decoded;int i,before;const byte prefix[]={0,0,0,128,255,255,255,127,255,255,255,255,4,0,0,0,254,255,255,255,17,0,0,0,19,0,0,0,0,0,128,191,0,0,128,62,0,0,64,63,0,0,128,63,0,0,0,0};
	memset(&font,0,sizeof(font));
	for(i=0;i<256;i++) {
		font.glyphs[i].height=INT_MIN;font.glyphs[i].top=INT_MAX;font.glyphs[i].bottom=-1;font.glyphs[i].pitch=4;font.glyphs[i].xSkip=-2;font.glyphs[i].imageWidth=17;font.glyphs[i].imageHeight=19;
		font.glyphs[i].s=-1.0f;font.glyphs[i].t=0.25f;font.glyphs[i].s2=0.75f;font.glyphs[i].t2=1.0f;font.glyphs[i].glyph=-99;
		snprintf(font.glyphs[i].shaderName,32,"fonts/oracle%d",i);
	}
	font.glyphScale=1.5f;strcpy(font.name,"fonts/fontImage_12.dat");mode=0;dataWrites=0;
	R_WriteLegacyFont(font.name,&font);Check(dataWrites==1 && !zoneCount,"explicit legacy writer balances temporary output ownership");
	for(i=0;i<256;i++)Check(!memcmp(serialized+i*80,prefix,48) && !memcmp(serialized+i*80+48,font.glyphs[i].shaderName,32),"each encoded glyph matches independent literal LE signed/float/zero-handle bytes");
	Check(serialized[20480]==0 && serialized[20481]==0 && serialized[20482]==192 && serialized[20483]==63 && !memcmp(serialized+20484,font.name,64),"serialized scale/name match fixed legacy tail offsets");
	memset(&decoded,0,sizeof(decoded));Check(R_ReadLegacyFont(serialized,20548,&decoded),"generated LE output passes actual legacy reader");
	for(i=0;i<256;i++)font.glyphs[i].glyph=0;memset(font.name,0,64);Check(!memcmp(&font,&decoded,sizeof(font)),"actual reader retains every encoded field except obsolete handles and unused stored name");
	before=writes;mode=19;R_WriteLegacyFont("fonts/fontImage_12.dat",&font);Check(writes==before && !zoneCount,"optional serialized output allocation failure performs no writes or leaks");mode=0;
}
static void SavedGeneration(void) {
	fontInfo_t generated,loaded;int i;mode=0;legacyInput=legacyShaderCalls=0;glyphPixels=64;save.integer=1;newFaces=doneFaces=reads=frees=images=writes=dataWrites=0;
	R_InitFreeType();RE_RegisterFont("fixture.ttf",12,&generated);Check(images==29 && dataWrites==1 && writes==30 && !zoneCount && !file,"native multiple-page output includes one legacy file and all 29 TGA pages with balanced ownership");VerifyGlyphs(&generated);R_DoneFreeType();
	legacyInput=1;legacyShaderCalls=0;save.integer=0;reads=frees=0;RE_RegisterFont(NULL,12,&loaded);
	Check(reads==1 && frees==1 && legacyShaderCalls==256 && !zoneCount && !file && registeredFontCount==1 && !strcmp(loaded.name,"fonts/fontImage_12.dat"),"actual public legacy reader accepts saved generated font with no FreeType dependency");
	for(i=0;i<256;i++) { generated.glyphs[i].glyph=70+i;Check(!memcmp(&generated.glyphs[i],&loaded.glyphs[i],sizeof(glyphInfo_t)),"saved native generation preserves each glyph rectangle/name/metrics with refreshed renderer handle"); }
	Check(generated.glyphScale==loaded.glyphScale,"saved glyph scale roundtrip");R_DoneFreeType();legacyInput=0;glyphPixels=4;
}
int main(void) {
	int scenario;ri.Printf=Print;ri.FS_ReadFile=Read;ri.FS_FreeFile=FreeFile;ri.FS_WriteFile=Write;
	for(scenario=0;scenario<=25;scenario++)if(scenario!=5)Case(scenario);
	Metrics();Atlases();TgaBounds();Encoding();SavedGeneration();
	puts("Enabled native font ownership, metric bounds, complete bounded atlases, cache, TGA and literal/public legacy LE output checks passed (issue #46)");return 0;
}
