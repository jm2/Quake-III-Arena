/* Issue #46: actual commercial 1.32c font decoding, cache and FS ownership. */
#include "renderer_image_gl_stub.h"
#include "../code/renderer/tr_font.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
refimport_t ri;
static byte data[20552];
static fontInfo_t expected,zeroFont;
static void *file,*fileBase;
static int actualLength,alignment,reads,frees,shaderCalls,noBuffer,syncCalls;
static char requested[MAX_QPATH];
static void Check(int condition,const char *message) { if(!condition) { fprintf(stderr,"Font regression failed: %s\n",message);exit(1); } }
void QDECL Com_Error(int level,const char *format,...) { (void)level;(void)format;Check(0,"unexpected error");exit(1); }
void QDECL Com_Printf(const char *format,...) { (void)format; }
static void QDECL Print(int level,const char *format,...) { (void)level;(void)format; }
void R_SyncRenderThread(void) { syncCalls++; }
qhandle_t RE_RegisterShaderNoMip(const char *name) {
	Check(name && memchr(name,0,32) && shaderCalls<256,"bounded serialized shader name");
	Check(!strcmp(name,expected.glyphs[shaderCalls].shaderName),"native registration preserves every glyph name/order");
	return expected.glyphs[shaderCalls++].glyph;
}
#ifndef Com_Memset
void Com_Memset(void *destination,int value,size_t size) { memset(destination,value,size); }
#endif
#ifndef Com_Memcpy
void Com_Memcpy(void *destination,const void *source,size_t size) { memcpy(destination,source,size); }
#endif
static int Read(const char *name,void **buffer) {
	Check(name && buffer && !file,"one actual native FS read with input ownership");reads++;Q_strncpyz(requested,name,sizeof(requested));
	if(noBuffer) { *buffer=NULL;return actualLength; }
	fileBase=malloc((actualLength>0?actualLength:0)+alignment+1);Check(fileBase!=NULL,"exact FS fixture allocation");file=(byte *)fileBase+alignment;
	if(actualLength>0)memcpy(file,data,actualLength);*buffer=file;return actualLength;
}
static void FreeFile(void *pointer) { Check(pointer && pointer==file,"native FS free exactly once");free(fileBase);fileBase=file=NULL;frees++; }
static void Word(int offset,uint32_t word) { data[offset]=(byte)word;data[offset+1]=(byte)(word>>8);data[offset+2]=(byte)(word>>16);data[offset+3]=(byte)(word>>24); }
static void Float(int offset,float value) { uint32_t word;memcpy(&word,&value,4);Word(offset,word); }
static void Build(void) {
	int i,offset;glyphInfo_t *glyph;memset(data,0,sizeof(data));memset(&expected,0,sizeof(expected));
	for(i=0;i<256;i++) {
		offset=i*80;glyph=&expected.glyphs[i];
		glyph->height=i+5;glyph->top=-i-7;glyph->bottom=-1;glyph->pitch=4*(i%64+1);glyph->xSkip=i-50;glyph->imageWidth=i%200;glyph->imageHeight=i%150;
		Word(offset,glyph->height);Word(offset+4,glyph->top);Word(offset+8,glyph->bottom);Word(offset+12,glyph->pitch);Word(offset+16,glyph->xSkip);Word(offset+20,glyph->imageWidth);Word(offset+24,glyph->imageHeight);
		glyph->s=-0.0625f*(i%4);glyph->t=0.125f;glyph->s2=0.75f;glyph->t2=1.0f;
		Float(offset+28,glyph->s);Float(offset+32,glyph->t);Float(offset+36,glyph->s2);Float(offset+40,glyph->t2);Word(offset+44,0xfedcba98u);
		if(i==0) { memset(glyph->shaderName,'a',31);glyph->shaderName[31]=0; }
		else if(i!=255)snprintf(glyph->shaderName,sizeof(glyph->shaderName),"fonts/page%d",i);
		glyph->glyph=i==255?0:1000+i;memcpy(data+offset+48,glyph->shaderName,32);
	}
	expected.glyphScale=1.5f;Float(20480,expected.glyphScale);memset(data+20484,'z',64);
}
static void Reset(void) { Check(!file && !fileBase,"balanced native font ownership between cases");registeredFontCount=0;memset(registeredFont,0,sizeof(registeredFont));reads=frees=shaderCalls=syncCalls=noBuffer=alignment=0;actualLength=20548;Build(); }
static void Register(fontInfo_t *font,int pointSize) { memset(font,0xa5,sizeof(*font));RE_RegisterFont("font.ttf",pointSize,font);Check(!file && !fileBase,"registration releases all FS ownership"); }
static void Reject(fontInfo_t *font) { Register(font,12);Check(!memcmp(font,&zeroFont,sizeof(*font)) && !registeredFontCount && !shaderCalls && reads==1 && frees==!noBuffer,"invalid font has zero default output, no cache/shader publication and balanced input"); }
static void Native(fontInfo_t *font) {
	int align,beforeReads,beforeFrees,beforeShaders;
	for(align=0;align<4;align++) {
		Reset();alignment=align;Register(font,12);Q_strncpyz(expected.name,"fonts/fontImage_12.dat",sizeof(expected.name));
		Check(sizeof(*font)==20548 && sizeof(glyphInfo_t)==80 && !memcmp(font,&expected,sizeof(*font)),"all legacy signed integers, floats, names, runtime handles and scale decode exactly at every FS alignment");
		Check(reads==1 && frees==1 && shaderCalls==256 && registeredFontCount==1 && !strcmp(requested,expected.name),"actual native successful input/cache and all 256 glyph imports");
		beforeReads=reads;beforeFrees=frees;beforeShaders=shaderCalls;Register(font,12);
		Check(!memcmp(font,&expected,sizeof(*font)) && reads==beforeReads && frees==beforeFrees && shaderCalls==beforeShaders,"native cache reuse imports no file or shader");
	}
	Reset();Register(font,0);Check(!strcmp(requested,"fonts/fontImage_12.dat") && font->glyphScale==1.5f,"native default point size remains 12");
	Reset();RE_RegisterFont(NULL,12,NULL);Check(!reads && !frees && !syncCalls,"null output has no imports");
}
static void Lengths(fontInfo_t *font) {
	int length,align;
	for(length=0;length<20548;length++) { Reset();actualLength=length;alignment=length%4;Reject(font); }
	for(align=0;align<4;align++) { Reset();alignment=align;actualLength=20549;Reject(font);Reset();alignment=align;actualLength=-1;Reject(font); }
	for(length=-1;length<=1;length++) { Reset();noBuffer=1;actualLength=length;Reject(font); }
	Reset();noBuffer=1;actualLength=20548;Reject(font);
}
static void NamesAndFloats(fontInfo_t *font) {
	const uint32_t nonfinite[]={0x7f800000u,0xff800000u,0x7fc00001u,0x7f800001u};
	const uint32_t badScale[]={0,0x80000000u,0xbf800000u};
	int glyph,align,field,kind;
	for(glyph=0;glyph<256;glyph++)for(align=0;align<4;align++) { Reset();alignment=align;memset(data+glyph*80+48,'a',32);Reject(font); }
	for(glyph=0;glyph<256;glyph++)for(field=0;field<4;field++)for(kind=0;kind<4;kind++) { Reset();alignment=(glyph+field+kind)%4;Word(glyph*80+28+field*4,nonfinite[kind]);Reject(font); }
	for(kind=0;kind<4;kind++)for(align=0;align<4;align++) { Reset();alignment=align;Word(20480,nonfinite[kind]);Reject(font); }
	for(kind=0;kind<3;kind++) { Reset();Word(20480,badScale[kind]);Reject(font); }
	Reset();Word(0,0x80000000u);expected.glyphs[0].height=(-2147483647-1);Word(4,0x7fffffffu);expected.glyphs[0].top=2147483647;Register(font,12);
	Check(font->glyphs[0].height==(-2147483647-1) && font->glyphs[0].top==2147483647,"full signed legacy bit patterns use defined decoding");
}
static void CacheCapacity(fontInfo_t *font) {
	int size,beforeReads,beforeFrees,beforeShaders;
	Reset();for(size=1;size<=MAX_FONTS;size++) { shaderCalls=0;Register(font,size);Check(registeredFontCount==size && shaderCalls==256,"fill native font cache"); }
	beforeReads=reads;beforeFrees=frees;beforeShaders=shaderCalls;Register(font,MAX_FONTS);
	Check(!strcmp(font->name,"fonts/fontImage_6.dat") && reads==beforeReads && frees==beforeFrees && shaderCalls==beforeShaders,"full cache still returns an existing font");
	Register(font,MAX_FONTS+1);Check(!memcmp(font,&zeroFont,sizeof(*font)) && reads==beforeReads && shaderCalls==beforeShaders && registeredFontCount==MAX_FONTS,"full cache rejects a new font without imports");
}
int main(void) {
	fontInfo_t *font=malloc(sizeof(*font));Check(font!=NULL,"exact output allocation");ri.Printf=Print;ri.FS_ReadFile=Read;ri.FS_FreeFile=FreeFile;
	Native(font);Lengths(font);NamesAndFloats(font);CacheCapacity(font);Check(!file && !fileBase,"final FS ownership balance");free(font);
	puts("Legacy font signed/float/name/layout, all prefixes/alignments, no-publication failures, cache and FS ownership checks passed (issue #46)");return 0;
}
