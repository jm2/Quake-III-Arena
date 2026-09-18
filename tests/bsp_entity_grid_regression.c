/* Issue #45: actual bounded worldspawn loading and light-grid boundary sampling. */
#include "bsp_fixture.h"
#include <float.h>
static void *gridTemporary;
static int failGridTemporary;
static void *GridMalloc(size_t size) {
	Check(!gridTemporary,"one owned entity validation allocation");
	if(failGridTemporary) { failGridTemporary=0;return NULL; }
	gridTemporary=malloc(size);Check(gridTemporary!=NULL,"entity validation temporary");return gridTemporary;
}
static void GridFree(void *pointer) { Check(pointer==gridTemporary && pointer,"owned entity validation release");free(pointer);gridTemporary=NULL; }
#define __QGL_H__
typedef unsigned int GLuint;
#define GL_CLAMP 0x2900
#define malloc GridMalloc
#define free GridFree
#include "../code/renderer/tr_bsp.c"
#undef malloc
#undef free
#include "../code/renderer/tr_curve.c"
#include "../code/renderer/tr_light.c"

trGlobals_t tr;
glconfig_t glConfig;
refimport_t ri;
static cvar_t rendererVariable,scale,subdivisions;
cvar_t *r_vertexLight=&rendererVariable,*r_lightmap=&rendererVariable,*r_mapOverBrightBits=&rendererVariable;
cvar_t *r_singleShader=&rendererVariable,*r_fullbright=&rendererVariable,*r_subdivisions=&subdivisions;
cvar_t *r_ambientScale=&scale,*r_directedScale=&scale,*r_debugLight=&rendererVariable;
static shader_t knownShader;
static model_t knownModel;
static world_t retainedWorld;
static int shaderCalls,modelCalls,rendererHunks,remaps,warnings;
static unsigned int At(int lump,int offset) { return BSP_FileWord(source+8+lump*8)+offset; }
static unsigned int Append(int lump,int size) { unsigned int offset=(sourceSize+3)&~3u;memset(source+sourceSize,0,offset-sourceSize+size);Lump(lump,offset,size);sourceSize=offset+size;return offset; }
static void *RendererHunk(int size,ha_pref preference) { Check(preference==h_low,"native renderer hunk preference");rendererHunks++;return Hunk_Alloc(size,h_high); }
static void *RendererMalloc(int size) { Check(size>0,"native curve temporary size");return calloc(1,size); }
static void RendererFree(void *pointer) { free(pointer); }
shader_t *R_FindShader(const char *name,int lightmap,qboolean mipmap) { (void)name;(void)lightmap;(void)mipmap;shaderCalls++;return &knownShader; }
model_t *R_AllocModel(void) { modelCalls++;return &knownModel; }
void R_SyncRenderThread(void) { }
void R_RemapShader(const char *oldName,const char *newName,const char *time) { Check(!strcmp(oldName,"old") && !strcmp(newName,"new") && !strcmp(time,"0"),"native remap arguments");remaps++; }
image_t *R_CreateImage(const char *name,const byte *pixels,int width,int height,qboolean mipmap,qboolean picmip,int wrap) { (void)name;(void)pixels;(void)width;(void)height;(void)mipmap;(void)picmip;(void)wrap;Check(0,"unexpected entity fixture texture upload");return NULL; }
static void QDECL Print(int level,const char *message,...) { (void)message;if(level==PRINT_WARNING)warnings++; }
static void Header(dheader_t *header) { Check(!BSP_ValidateHeader(source,sourceSize,header) && !BSP_ValidateReferences(source,header) && !R_ValidateBSPGeometry(source,header),"entity/grid mutation retains validated layout/references/finite geometry"); }
static void Build(const char *text,int length,int samples) {
	unsigned int offset;int i,j;
	BuildCM(2);
	if(samples) {
		offset=Append(LUMP_LIGHTGRID,samples*8);
		for(i=0;i<samples;i++) { for(j=0;j<3;j++) { source[offset+i*8+j]=11+i+j;source[offset+i*8+3+j]=51+i+j; }source[offset+i*8+6]=source[offset+i*8+7]=0; }
	}
	/* Final entity bytes have no undeclared trailing NUL. */
	offset=Append(LUMP_ENTITIES,length);memcpy(source+offset,text,length);
}
static void Bounds(float minimum,float maximum) { int i;for(i=0;i<3;i++) { Float(At(LUMP_MODELS,i*4),minimum);Float(At(LUMP_MODELS,12+i*4),maximum); } }
static void RejectRenderer(void) {
	trGlobals_t beforeTr=tr;world_t beforeWorld=s_worldData;
	int readBefore=reads,freeBefore=frees,hunkBefore=rendererHunks,shaderBefore=shaderCalls,modelBefore=modelCalls,remapBefore=remaps;
	readable=advertised=sourceSize;missing=0;expectError=1;
	if(!setjmp(errorJump)) { RE_LoadWorldMap("bad-entity-grid.bsp");Check(0,"renderer accepted unsafe entity/grid"); }
	expectError=0;
	Check(reads==readBefore+1 && frees==freeBefore+1 && rendererHunks==hunkBefore && shaderCalls==shaderBefore && modelCalls==modelBefore && remaps==remapBefore && !gridTemporary && !fileAllocation && !memcmp(&tr,&beforeTr,sizeof(tr)) && !memcmp(&s_worldData,&beforeWorld,sizeof(s_worldData)),"entity/grid rejection changed world, ownership or callback state");
}
static void RejectGrid(void) {
	int a;dheader_t header;bspLightGrid_t grid;Header(&header);Check(R_ValidateBSPLightGrid(source,&header,&grid)!=NULL && !gridTemporary,"unsafe grid rejected by side-effect-free preflight");
	tr.world=&retainedWorld;tr.worldMapLoaded=qfalse;tr.numModels=7;
	for(a=0;a<4;a++) { alignment=a;RejectRenderer(); }alignment=0;
}
static void Load(void) {
	int beforeReads=reads,beforeFrees=frees;
	FreeHunks();tr.worldMapLoaded=qfalse;alignment=0;readable=advertised=sourceSize;missing=0;
	RE_LoadWorldMap("entity-grid.bsp");
	Check(tr.worldMapLoaded && tr.world==&s_worldData && reads==beforeReads+1 && frees==beforeFrees+1 && !gridTemporary && !fileAllocation,"actual renderer world publication and temporary ownership");
}
static void TextCase(const char *text,int length,int samples,float x,float y,float z,int remapCount) {
	int beforeRemaps=remaps;unsigned int offset;
	Build(text,length,samples);offset=At(LUMP_ENTITIES,0);Load();
	Check(!memcmp(s_worldData.entityString,source+offset,length) && !s_worldData.entityString[length] && s_worldData.lightGridSize[0]==x && s_worldData.lightGridSize[1]==y && s_worldData.lightGridSize[2]==z && remaps==beforeRemaps+remapCount,"exact entity copy, defaults/partial custom grid and native remaps");
}
static void Sample(float x,float y,float z,float expected) {
	vec3_t point={x,y,z},ambient,directed,direction;int i;
	Check(R_LightForPoint(point,ambient,directed,direction),"actual light-grid sampler");
	for(i=0;i<3;i++)Check(fabs(ambient[i]-(11+expected+i))<0.0001 && fabs(directed[i]-(51+expected+i))<0.0001,"bounded trilinear RGB golden");
	Check(fabs(direction[0])<0.0001 && fabs(direction[1])<0.0001 && fabs(direction[2]-1)<0.0001,"native grid light direction bytes");
}
int main(void) {
	const char *partial[]={"{ \"gridsize\" \"32\" }","{ \"gridsize\" \"32 48\" }","{ \"gridsize\" \"32 48 96 extra\" }","{ \"gridsize\" \"junk\" }","{ \"gridsize\" \"32junk\" }","{ \"gridsize\" \"32 48"};
	const char *invalid[]={"0","-1","nan","inf","-inf","1e500","1e-500","1e-40","64 0","64 64 nan"};
	const char *remap="{ \"remapshader1\" \"old;new\" \"vertexremapshader1\" \"old;new\" \"gridsize\" \"32 48 96\" }";
	const char *grid="{ \"gridsize\" \"1 1 1\" }",*truncated="{ \"gridsize\" \"32 48 96\" }";
	char bad[256],token[MAX_TOKEN_CHARS],*cursor;int i,beforeWarnings;dheader_t header;bspLightGrid_t checked;byte *exact,*ownedGrid;
	ri.Error=Com_Error;ri.Printf=Print;ri.FS_ReadFile=FS_ReadFile;ri.FS_FreeFile=FS_FreeFile;ri.Hunk_Alloc=RendererHunk;ri.Malloc=RendererMalloc;ri.Free=RendererFree;
	scale.value=1;subdivisions.value=4;tr.sinTable[FUNCTABLE_SIZE/4]=1;tr.world=&retainedWorld;
	TextCase("",0,0,64,64,128,0);TextCase("{}",2,0,64,64,128,0);TextCase("abc",3,0,64,64,128,0);
	for(i=0;i<6;i++) TextCase(partial[i],strlen(partial[i]),0,i==3?64:32,i==1 || i==2 || i==5?48:64,i==2?96:128,0);
	TextCase(remap,strlen(remap),0,32,48,96,1);rendererVariable.integer=1;TextCase(remap,strlen(remap),0,32,48,96,2);rendererVariable.integer=0;
	TextCase("{ \"remapshader\" \"broken\" \"gridsize\" \"0\" }",strlen("{ \"remapshader\" \"broken\" \"gridsize\" \"0\" }"),0,64,64,128,0);
	/* Every prefix reaches the real bounded tokenizer, including quotes/comments at EOF. */
	for(i=0;i<=(int)strlen(truncated);i++) { Build(truncated,i,0);Header(&header);Check(!R_ValidateBSPLightGrid(source,&header,&checked),"safe short entity prefix");Load(); }
	TextCase("{ \"key\" \"unterminated",strlen("{ \"key\" \"unterminated"),0,64,64,128,0);
	for(i=0;i<8 && R_GetEntityToken(token,sizeof(token));i++) { }Check(i<8 && s_worldData.entityParsePoint==s_worldData.entityString,"entity token API safely resets after an unterminated quote");
	TextCase("{ /",3,0,64,64,128,0);TextCase("{ /*",4,0,64,64,128,0);TextCase("{ //",4,0,64,64,128,0);
	/* The common tokenizer also stops its public cursor at the NUL. */
	cursor=malloc(5);memcpy(cursor,"\"abc",5);{ char *base=cursor;Check(!strcmp(COM_ParseExt(&cursor,qtrue),"abc") && !cursor && !*COM_ParseExt(&cursor,qtrue),"quoted token EOF never publishes an unreadable cursor");free(base); }
	for(i=0;i<10;i++) { snprintf(bad,sizeof(bad),"{ \"remapshader\" \"old;new\" \"gridsize\" \"%s\" }",invalid[i]);Build(bad,strlen(bad),1);RejectGrid(); }
	Build(grid,strlen(grid),1);Bounds(-FLT_MAX,FLT_MAX);RejectGrid();
	Build(grid,strlen(grid),1);Bounds(0,0);Float(At(LUMP_MODELS,12),16777215);Float(At(LUMP_MODELS,16),15);RejectGrid();
	Build("{ \"gridsize\" \"1e38 1e38 1e38\" }",strlen("{ \"gridsize\" \"1e38 1e38 1e38\" }"),1);Bounds(FLT_MAX,FLT_MAX);RejectGrid();
	Build("{}",2,1);Header(&header);tr.worldMapLoaded=qfalse;tr.world=&retainedWorld;for(i=0;i<4;i++) { alignment=i;failGridTemporary=1;RejectRenderer();Check(!failGridTemporary,"injected temporary failure consumed"); }alignment=0;
	/* Entity parsing uses its declared span even when another non-NUL lump follows it. */
	Build("{ \"gridsize\" \"32 48 96\" }",strlen("{ \"gridsize\" \"32 48 96\" }"),0);Append(LUMP_LIGHTGRID,8);memset(source+At(LUMP_LIGHTGRID,0),'X',8);Load();Check(s_worldData.lightGridSize[2]==96 && !s_worldData.entityString[strlen("{ \"gridsize\" \"32 48 96\" }")],"following sample bytes stay outside entity text");
	/* Safe partial/mismatched records preserve the native disabled-grid behavior. */
	Build(grid,strlen(grid),1);Bounds(0,1);beforeWarnings=warnings;Load();Check(!s_worldData.lightGridData && warnings==beforeWarnings+1,"safe mismatch disables data with one warning");
	Build(grid,strlen(grid),1);Bounds(0.25f,0.5f);beforeWarnings=warnings;Load();Check(!s_worldData.lightGridData && warnings==beforeWarnings+1,"empty grid intersection never publishes a zero-sized sample array");
	Build(grid,strlen(grid),1);Bounds(-FLT_MAX,FLT_MAX);Lump(LUMP_LIGHTGRID,0,0);Load();Check(!s_worldData.lightGridData,"empty light grid needs no overflowing derived coordinates");
	/* Keep metadata/calculation within the actual native integer capacity, without compiler caps. */
	Build(grid,strlen(grid),1);Bounds(0,0);Float(At(LUMP_MODELS,12),16777214);Float(At(LUMP_MODELS,16),14);Header(&header);Check(!R_ValidateBSPLightGrid(source,&header,&checked) && checked.bounds[0]==16777215 && checked.bounds[1]==15 && checked.numPoints==251658225,"safe raised grid budget fits actual native strides");
	TextCase("{}\0{ \"gridsize\" \"0\" }",sizeof("{}\0{ \"gridsize\" \"0\" }")-1,0,64,64,128,0);
	/* Final sample channels/direction survive both in-place overbright operations. */
	Build(grid,strlen(grid),1);Bounds(0,0);Load();Check(s_worldData.lightGridBounds[0]==1 && !memcmp(s_worldData.lightGridData,source+At(LUMP_LIGHTGRID,0),8),"one-point grid copy and final direction bytes");
	rendererVariable.integer=2;Load();for(i=0;i<6;i++)Check(s_worldData.lightGridData[i]==(source[At(LUMP_LIGHTGRID,i)]<<2),"both RGB triplets receive native overbright conversion");Check(!s_worldData.lightGridData[6] && !s_worldData.lightGridData[7],"final direction bytes stay unchanged");rendererVariable.integer=0;Load();
	/* Exact input-backed sample allocations make all upper-corner accesses observable. */
	ownedGrid=s_worldData.lightGridData;exact=malloc(8);memcpy(exact,ownedGrid,8);s_worldData.lightGridData=exact;
	Sample(0,0,0,0);Sample(FLT_MAX,-FLT_MAX,FLT_MAX,0);Sample(INFINITY,-INFINITY,NAN,0);free(exact);s_worldData.lightGridData=ownedGrid;
	Build(grid,strlen(grid),8);Bounds(0,1);Load();exact=malloc(64);memcpy(exact,s_worldData.lightGridData,64);ownedGrid=s_worldData.lightGridData;s_worldData.lightGridData=exact;
	Sample(.5f,.5f,.5f,3.5f);
	for(i=0;i<8;i++)Sample(i&1?1:0,i&2?1:0,i&4?1:0,i);
	Sample(FLT_MAX,FLT_MAX,FLT_MAX,7);Sample(-FLT_MAX,-FLT_MAX,-FLT_MAX,0);Sample(INFINITY,NAN,-INFINITY,1);
	free(exact);s_worldData.lightGridData=ownedGrid;
	/* Black wall corners are still ignored, with surviving weights renormalized. */
	for(i=0;i<3;i++)s_worldData.lightGridData[i]=0;Sample(.5f,.5f,.5f,4);
	Check(!gridTemporary && !fileAllocation,"all entity/grid input and temporary ownership released");FreeHunks();
	puts("BSP bounded entity, world publication, light-grid calculation and edge sampling regressions passed (issue #45)");return 0;
}
