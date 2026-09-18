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
void *Z_Malloc(int size) { void *p;Check(size>0 && size<2000000 && zoneCount<4096,"bounded native temporary font allocation");if((mode==3 && size==1048576) || (mode==4 && size==262144) || (mode==11 && size==sizeof(FT_Bitmap)) || (mode==12 && size==16) || (mode==15 && size==262162))return NULL;p=calloc(1,size);Check(p!=NULL,"exact native allocation");zones[zoneCount]=p;zoneSizes[zoneCount++]=size;return p; }
void Z_Free(void *pointer) { int i;for(i=0;i<zoneCount;i++)if(zones[i]==pointer) { free(pointer);zones[i]=zones[--zoneCount];zoneSizes[i]=zoneSizes[zoneCount];return; }Check(0,"temporary zone free exactly once"); }
static int Read(const char *name,void **buffer) { Check(name && buffer,"actual FS read");reads++;if(strstr(name,".dat")) { *buffer=NULL;return -1; }Check(!file,"one owned face input");if(mode==8) { *buffer=NULL;return 4; }file=malloc(4);memcpy(file,"font",4);*buffer=file;return mode==6?0:mode==7?-1:4; }
static void FreeFile(void *pointer) { Check(pointer && pointer==file && !aliveFace,"face closes before freeing its retained input");free(pointer);file=NULL;frees++; }
static void Write(const char *name,const void *data,int length) { Check(name && data && length>0,"native output write");writes++; }
qhandle_t RE_RegisterShaderNoMip(const char *name) { (void)name;Check(0,"no legacy shader imports on missing .dat");return 0; }
image_t *R_CreateImage(const char *name,const byte *pixels,int width,int height,qboolean mipmap,qboolean picmip,int wrap) { Check(name && pixels && width==256 && height==256 && !mipmap && !picmip && wrap==GL_CLAMP,"native font image import");if(mode==14)return NULL;images++;return &image; }
qhandle_t RE_RegisterShaderFromImage(const char *name,int lightmap,image_t *texture,qboolean mipmap) { Check(name && lightmap==LIGHTMAP_2D && texture==&image && !mipmap,"native image shader import");return 10+images; }
int FT_Init_FreeType(FT_Library *library) { initCalls++;*library=mode==9?NULL:&slot;return mode==9; }
int FT_Done_FreeType(FT_Library library) { Check(library==&slot && !aliveFace,"library shutdown with no owned faces");doneLibraries++;return 0; }
int FT_New_Memory_Face(FT_Library library,const void *data,long length,long index,FT_Face *face) { Check(library==&slot && data==file && length==4 && index==0,"native FT memory face retains owned FS input");newFaces++;if(mode==1)return 1;*face=malloc(sizeof(**face));Check(*face!=NULL,"fixture face allocation");(*face)->glyph=&slot;aliveFace=1;return 0; }
int FT_Set_Char_Size(FT_Face face,long width,long height,unsigned int horizontal,unsigned int vertical) { Check(face && aliveFace && width==768 && height==768 && horizontal==72 && vertical==72,"native character size");return mode==2; }
int FT_Done_Face(FT_Face face) { Check(face && aliveFace && file,"face shutdown while backing input remains owned");free(face);aliveFace=0;doneFaces++;return 0; }
unsigned int FT_Get_Char_Index(FT_Face face,unsigned long character) { Check(face && aliveFace && character<=255,"native glyph index import");return character; }
int FT_Load_Glyph(FT_Face face,unsigned int index,int flags) { Check(face && aliveFace && index<=255 && flags==FT_LOAD_DEFAULT,"native glyph load");memset(&slot,0,sizeof(slot));slot.metrics.width=256;slot.metrics.height=256;slot.metrics.horiBearingY=256;slot.metrics.horiAdvance=256;slot.format=ft_glyph_format_outline;return mode==10; }
void FT_Outline_Translate(FT_Outline *outline,long x,long y) { Check(outline==&slot.outline && x==0 && y==0,"native outline translation"); }
int FT_Outline_Get_Bitmap(FT_Library library,FT_Outline *outline,FT_Bitmap *bitmap) { Check(library==&slot && outline==&slot.outline && bitmap && bitmap->pitch==4 && bitmap->rows==4,"native glyph bitmap import");memset(bitmap->buffer,64,16);return mode==13; }
static void Case(int scenario) {
	fontInfo_t font,zero;int expectedReads,expectedFrees,expectedNew,expectedDone;const char *name="fixture.ttf";int pointSize=12;int valid;
	Check(!file && !zoneCount && !aliveFace && !ftLibrary,"previous native ownership fully released");
	mode=scenario;newFaces=doneFaces=reads=frees=images=writes=initCalls=doneLibraries=0;save.integer=mode==15;
	if(mode==16)name=NULL;if(mode==17)name="";if(mode==18)pointSize=INT_MAX;
	memset(&font,0xa5,sizeof(font));memset(&zero,0,sizeof(zero));R_InitFreeType();if(mode!=9)R_InitFreeType();
	Check(initCalls==1,"successful repeat init reuses the owned FT library");
	RE_RegisterFont(name,pointSize,&font);
	expectedReads=(mode==9 || mode>=16)?1:2;expectedFrees=(mode==8 || mode==9 || mode>=16)?0:1;
	expectedNew=(mode>=6 && mode<=9) || mode>=16?0:1;expectedDone=expectedNew && mode!=1;
	Check(reads==expectedReads && frees==expectedFrees && !file && !zoneCount && !aliveFace && newFaces==expectedNew && doneFaces==expectedDone,"face/file/temporary ownership balances on every success and failure");
	valid=mode==0 || mode==15;
	if(valid)Check(registeredFontCount==1 && images==1 && font.glyphScale==4.0f && font.glyphs[0].glyph==11 && font.glyphs[0].xSkip==5,"successful generation keeps native output");
	else Check(!registeredFontCount && !memcmp(&font,&zero,sizeof(font)),"controlled failures publish zero default output and no font cache");
	if(mode==15)Check(writes==1,"optional TGA output allocation failure does not leak or discard usable generated font");
	R_DoneFreeType();R_DoneFreeType();Check(doneLibraries==(mode==9?0:1) && !ftLibrary && !registeredFontCount,"idempotent shutdown releases the owned library/cache");
}
int main(void) {
	int scenario;ri.Printf=Print;ri.FS_ReadFile=Read;ri.FS_FreeFile=FreeFile;ri.FS_WriteFile=Write;
	for(scenario=0;scenario<=18;scenario++)if(scenario!=5)Case(scenario);
	puts("Enabled native font face/file/bitmap/page ownership, failure/default output, checked request and repeat init/shutdown checks passed (issue #46)");return 0;
}
