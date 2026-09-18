/* Issue #45: actual BSP RGB uploads without GL or a window system. */
/* tr_local.h's real structures need only GLuint; suppress unused qgl declarations. */
#define __QGL_H__
typedef unsigned int GLuint;
#define GL_CLAMP 0x2900
#include "../code/renderer/tr_bsp.c"
#include <setjmp.h>
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
void R_SyncRenderThread(void) { syncs++; }
image_t *R_CreateImage(const char *name,const byte *pic,int width,int height,qboolean mipmap,qboolean allowPicmip,int wrap) {
	int j,index=creates++,source=index<sourceCount?index:0;char wanted[40];
	Check(index<MAX_LIGHTMAPS && width==128 && height==128 && !mipmap && !allowPicmip && wrap==GL_CLAMP,"native upload arguments and fixed table bounds");
	snprintf(wanted,sizeof(wanted),"*lightmap%d",index);Check(!strcmp(wanted,name),"native lightmap slot name");
	for(j=0;j<128*128;j++) {
		Check(pic[j*4+3]==255,"every uploaded alpha is opaque");
		if(!lightmap.integer) {
			if(j==128*128-1) Check(pic[j*4]==123 && pic[j*4+1]==231 && pic[j*4+2]==17,"final exact RGB sample");
			else Check(pic[j*4]==(byte)(source+1) && pic[j*4+1]==31 && pic[j*4+2]==127,"native RGB channels and selected source");
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
	ri.Error=Error;ri.Printf=Print;
	for(alignment=0;alignment<4;alignment++) { Run(1,alignment,0);Run(2,alignment,0);Run(1,alignment,2); }
	Run(MAX_LIGHTMAPS,0,0);Run(MAX_LIGHTMAPS+1,0,0);
	tr.numLightmaps=42;creates=syncs=0;R_LoadLightmaps(&empty);Check(!tr.numLightmaps && !creates && !syncs,"empty source resets count without upload");
	for(i=0;i<4;i++) { lump_t lump={0,bad[i]};expectError=1;creates=syncs=0;tr.numLightmaps=13;if(!setjmp(errorJump)) { R_LoadLightmaps(&lump);Check(0,"incomplete records accepted"); }expectError=0;Check(!creates && !syncs && tr.numLightmaps==13,"invalid record changed state before rejection"); }
	lightmap.integer=0;mapOverbright.integer=2;tr.overbrightBits=0;R_ColorShiftLightingRGB(rgb,out);Check(out[0]==255 && out[1]==127 && out[2]==63 && out[3]==255,"native RGB normalization golden");
	R_ColorShiftLightingBytes(color,color);Check(color[0]==255 && color[1]==127 && color[2]==63 && color[3]==234,"geometry RGBA aliases retain alpha");
	puts("BSP exact RGB lightmap, duplicate upload and geometry alpha regressions passed (issue #45)");return 0;
}
