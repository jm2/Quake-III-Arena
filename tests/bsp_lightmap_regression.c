/* Issue #45: actual BSP RGB uploads without GL or a window system. */
/* tr_local.h's real structures need only GLuint; suppress unused qgl declarations. */
#define __QGL_H__
typedef unsigned int GLuint;
#define GL_CLAMP 0x2900
#include "../code/renderer/tr_bsp.c"
#include <setjmp.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

trGlobals_t tr;
glconfig_t glConfig;
refimport_t ri;
static cvar_t vertexLight,lightmap,mapOverbright;
cvar_t *r_vertexLight=&vertexLight,*r_lightmap=&lightmap,*r_mapOverBrightBits=&mapOverbright;
static image_t images[MAX_LIGHTMAPS];
static byte captured[2][LIGHTMAP_SIZE*LIGHTMAP_SIZE*4];
static int creates,syncs,warnings,sourceCount,expectError;
static jmp_buf errorJump;
static void Check(int ok,const char *message) { if(!ok) { fprintf(stderr,"BSP lightmap regression failed: %s\n",message);exit(1); } }
/* Original defined arithmetic is an independent golden for ordinary shifts. */
static void LegacyGolden(const byte rgb[3],int shift,byte result[4]) {
	int color[3],maximum=0,i;
	Check(shift>=0 && shift<=8,"golden uses only defined legacy shifts/products");
	for(i=0;i<3;i++) { color[i]=(int)rgb[i]<<shift;if(color[i]>maximum)maximum=color[i]; }
	for(i=0;i<3;i++)result[i]=maximum>255?color[i]*255/maximum:color[i];
	result[3]=255;
}
static void ColorCase(int map,int hardware,const byte rgb[3],const byte expected[4]) {
	byte actual[4],rgba[4],overlap[8];int alignment;
	mapOverbright.integer=map;tr.overbrightBits=hardware;
	for(alignment=0;alignment<4;alignment++) {
		memcpy(overlap+alignment,rgb,3);R_ColorShiftLightingRGB(overlap+alignment,actual);
		Check(!memcmp(actual,expected,4),"RGB arithmetic golden at each byte alignment");
		memcpy(rgba,rgb,3);rgba[3]=173;R_ColorShiftLightingBytes(rgba,rgba);
		Check(!memcmp(rgba,expected,3) && rgba[3]==173,"in-place geometry RGB and alpha");
	}
	/* All RGB channels and geometry alpha are captured before overlapping writes. */
	memcpy(overlap+1,rgb,3);overlap[4]=173;R_ColorShiftLightingBytes(overlap+1,overlap);
	Check(!memcmp(overlap,expected,3) && overlap[3]==173,"backward-overlapping geometry output");
	memcpy(overlap,rgb,3);overlap[3]=173;R_ColorShiftLightingBytes(overlap,overlap+1);
	Check(!memcmp(overlap+1,expected,3) && overlap[4]==173,"forward-overlapping geometry output");
}
static void Arithmetic(void) {
	static const byte values[]={0,1,2,7,15,31,32,63,64,127,128,254,255};
	static const byte odd[3]={9,5,3},black[3]={0,0,0};
	static const byte dim[4]={4,2,1,255},dark[4]={0,0,0,255},normalized[4]={255,141,85,255};
	byte rgb[3],expected[4];int shift,r,g,b;
	for(shift=0;shift<=8;shift++)for(r=0;r<sizeof(values);r++)for(g=0;g<sizeof(values);g++)for(b=0;b<sizeof(values);b++) {
		rgb[0]=values[r];rgb[1]=values[g];rgb[2]=values[b];LegacyGolden(rgb,shift,expected);ColorCase(shift,0,rgb,expected);
	}
	ColorCase(0,1,odd,dim);ColorCase(1,2,odd,dim);
	ColorCase(-8,0,odd,dark);ColorCase(INT_MIN,2,odd,dark);
	ColorCase(INT_MAX,0,odd,normalized);ColorCase(INT_MAX,INT_MIN,odd,normalized);
	ColorCase(INT_MIN,INT_MAX,odd,dark);ColorCase(INT_MAX,0,black,dark);
	/* Every source byte tests the exact shift-to-black and saturation boundary. */
	for(r=0;r<=255;r++) {
		rgb[0]=r;rgb[1]=255-r;rgb[2]=r/2;
		for(shift=1;shift<=8;shift++) { for(g=0;g<3;g++)expected[g]=rgb[g]>>shift;expected[3]=255;ColorCase(-shift,0,rgb,expected); }
	}
	mapOverbright.integer=tr.overbrightBits=0;
}
static void *GridHunk(int size,ha_pref preference) { Check(size==8 && preference==h_low,"exact single grid allocation");return malloc(size); }
static void GridColors(void) {
	byte source[8]={9,5,3,31,15,7,234,201};
	static const byte native[8]={18,10,6,62,30,14,234,201},dim[8]={4,2,1,15,7,3,234,201};
	static const byte bright[8]={255,141,85,255,123,57,234,201},dark[8]={0,0,0,0,0,0,234,201};
	const byte *expected[]={native,dim,bright,dark};int maps[]={1,0,INT_MAX,INT_MIN},hardware[]={0,1,0,2},i;
	bspLightGrid_t grid={{64,64,128},{1.0f/64,1.0f/64,1.0f/128},{0,0,0},{1,1,1},1};lump_t lump={0,8};
	ri.Hunk_Alloc=GridHunk;fileBase=source;
	for(i=0;i<4;i++) {
		mapOverbright.integer=maps[i];tr.overbrightBits=hardware[i];R_LoadLightGrid(&lump,&grid);
		Check(s_worldData.lightGridData && !memcmp(s_worldData.lightGridData,expected[i],8),"actual grid ambient/directed RGB, overlapping alpha and direction golden");
		free(s_worldData.lightGridData);s_worldData.lightGridData=NULL;
		Check(source[3]==31 && source[6]==234 && source[7]==201,"grid source remains immutable");
	}
	mapOverbright.integer=tr.overbrightBits=0;fileBase=NULL;
}
#ifndef Com_Memcpy
void Com_Memcpy(void *out,const void *in,size_t size) { memcpy(out,in,size); }
#endif
void R_SyncRenderThread(void) { syncs++; }
image_t *R_CreateImage(const char *name,const byte *pic,int width,int height,qboolean mipmap,qboolean allowPicmip,int wrap) {
	int j,index=creates++,source=index<sourceCount?index:0;char wanted[40];
	Check(index<MAX_LIGHTMAPS && width==128 && height==128 && !mipmap && !allowPicmip && wrap==GL_CLAMP,"native upload arguments and fixed table bounds");
	snprintf(wanted,sizeof(wanted),"*lightmap%d",index);Check(!strcmp(wanted,name),"native lightmap slot name");
	for(j=0;j<128*128;j++) {
		Check(pic[j*4+3]==255,"every uploaded alpha is opaque");
		if(!lightmap.integer) {
			byte rgb[3]={(byte)(source+1),31,127},expected[4];int shift=mapOverbright.integer-tr.overbrightBits;
			if(j==128*128-1) { rgb[0]=123;rgb[1]=231;rgb[2]=17; }
			if(shift<0) { int c;for(c=0;c<3;c++)expected[c]=rgb[c]>>-shift;expected[3]=255; }
			else LegacyGolden(rgb,shift,expected);
			Check(!memcmp(pic+j*4,expected,4),"actual uploaded RGB normalization/dimming golden");
		}
	}
	if(index<2)memcpy(captured[index],pic,sizeof(captured[index]));
	return &images[index];
}
static void QDECL Error(int level,const char *message,...) { (void)message;Check(level==ERR_DROP && expectError,"controlled invalid record rejection");longjmp(errorJump,1); }
static void QDECL Print(int level,const char *message,...) { (void)message;if(level==PRINT_WARNING)warnings++; }
void QDECL Com_Error(int level,const char *message,...) { (void)level;(void)message;Check(0,"unexpected shared error"); }
void QDECL Com_Printf(const char *message,...) { (void)message; }
void QDECL Com_DPrintf(const char *message,...) { (void)message; }
static void Run(int count,int alignment,int colorCoding) {
	int i,j,previousWarnings=warnings;unsigned int size=count*128*128*3;byte *allocation=malloc(size+alignment),*input=allocation+alignment;lump_t lump={0,size};
	Check(allocation!=NULL,"exact lightmap source");
	for(i=0;i<count;i++) {
		byte *record=input+i*128*128*3;
		for(j=0;j<128*128;j++) { record[j*3]=i+1;record[j*3+1]=31;record[j*3+2]=127; }
		record[(128*128-1)*3]=123;record[(128*128-1)*3+1]=231;record[(128*128-1)*3+2]=17;
	}
	creates=syncs=0;sourceCount=count;lightmap.integer=colorCoding;fileBase=input;
	R_LoadLightmaps(&lump);
	Check(syncs==1 && creates==(count==1?2:count<MAX_LIGHTMAPS?count:MAX_LIGHTMAPS) && tr.numLightmaps==creates,"legacy one-source duplication and fixed upload capacity");
	for(i=0;i<creates;i++)Check(tr.lightmaps[i]==&images[i],"published native image slots");
	if(count==1)Check(!memcmp(captured[0],captured[1],sizeof(captured[0])),"single source uploads identical complete textures");
	Check(warnings==previousWarnings+(count>=MAX_LIGHTMAPS),"existing capacity warning behavior");
	free(allocation);fileBase=NULL;
}
int main(void) {
	int alignment,i;byte color[4]={64,32,16,234},rgb[3]={64,32,16},out[4];int bad[]={-1,1,128*128*3-1,128*128*3+1};lump_t empty={0,0};
	ri.Error=Error;ri.Printf=Print;Arithmetic();GridColors();
	for(alignment=0;alignment<4;alignment++) { Run(1,alignment,0);Run(2,alignment,0);Run(1,alignment,2); }
	Run(MAX_LIGHTMAPS,0,0);Run(MAX_LIGHTMAPS+1,0,0);
	for(alignment=0;alignment<4;alignment++) {
		mapOverbright.integer=2;tr.overbrightBits=0;Run(1,alignment,0);
		mapOverbright.integer=0;tr.overbrightBits=1;Run(2,alignment,0);
	}
	mapOverbright.integer=tr.overbrightBits=0;
	tr.numLightmaps=42;creates=syncs=0;R_LoadLightmaps(&empty);Check(!tr.numLightmaps && !creates && !syncs,"empty source resets count without upload");
	for(i=0;i<4;i++) { lump_t lump={0,bad[i]};expectError=1;creates=syncs=0;tr.numLightmaps=13;if(!setjmp(errorJump)) { R_LoadLightmaps(&lump);Check(0,"incomplete records accepted"); }expectError=0;Check(!creates && !syncs && tr.numLightmaps==13,"invalid record changed state before rejection"); }
	lightmap.integer=0;mapOverbright.integer=2;tr.overbrightBits=0;R_ColorShiftLightingRGB(rgb,out);Check(out[0]==255 && out[1]==127 && out[2]==63 && out[3]==255,"native RGB normalization golden");
	R_ColorShiftLightingBytes(color,color);Check(color[0]==255 && color[1]==127 && color[2]==63 && color[3]==234,"geometry RGBA aliases retain alpha");
	puts("BSP bounded lighting arithmetic, actual RGB/grid uploads and geometry alpha regressions passed (issue #45)");return 0;
}
