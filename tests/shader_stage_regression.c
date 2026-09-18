/* Issue #46: actual shader parser/registration boundaries and synchronization. */
#define __QGL_H__
typedef unsigned int GLuint;
#define GL_CLAMP 0x2900
#define GL_REPEAT 0x2901
#define GL_MODULATE 0x2100
#define GL_DECAL 0x2101
#define GL_ADD 0x0104
static void (*qglActiveTextureARB)(unsigned int);
#include "../code/renderer/tr_shader.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
trGlobals_t tr;
glconfig_t glConfig;
refimport_t ri;
static cvar_t zero,one={.integer=1};
cvar_t *r_ignoreFastPath=&one,*r_smp=&zero,*r_printShaders=&zero,*r_detailTextures=&one,*r_vertexLight=&zero,*r_uiFullScreen=&zero;
static backEndData_t commands;
backEndData_t *backEndData[SMP_FRAMES]={&commands,&commands};
static image_t white;
static void *owned[512];
static int allocations,videos;
static char archive[64000];
static char *emptyHash[1];
static char *scriptFiles[]={"fixture.shader",NULL};
static void *scriptFile;
static int scriptReads,scriptFrees,listFrees;
static void Check(int condition,const char *message) { if(!condition) { fprintf(stderr,"Shader stage regression failed: %s\n",message);exit(1); } }
void QDECL Com_Error(int level,const char *format,...) { (void)level;(void)format;Check(0,"unexpected native error");exit(1); }
void QDECL Com_Printf(const char *format,...) { (void)format; }
static void QDECL Print(int level,const char *format,...) { (void)level;(void)format; }
static void *Allocate(int size,ha_pref preference) { void *p;Check(size>=0 && size<100000 && preference==h_low && allocations<512,"bounded native shader allocation");p=calloc(1,size?size:1);Check(p!=NULL,"shader fixture allocation");owned[allocations++]=p;return p; }
static void Release(void) { while(allocations)free(owned[--allocations]);memset(&tr,0,sizeof(tr));memset(hashTable,0,sizeof(hashTable)); }
image_t *R_FindImageFile(const char *name,qboolean mipmap,qboolean picmip,int wrap) { (void)name;(void)mipmap;(void)picmip;(void)wrap;return &white; }
void R_InitSkyTexCoords(float height) { (void)height; }
void R_SyncRenderThread(void) { Check(0,"unexpected threaded import"); }
void RB_StageIteratorGeneric(void) {}
void RB_StageIteratorSky(void) {}
void RB_StageIteratorVertexLitTexture(void) {}
void RB_StageIteratorLightmappedMultitexture(void) {}
void R_DecomposeSort(unsigned int sort,int *entity,shader_t **material,int *fog,int *light) { (void)sort;(void)entity;(void)material;(void)fog;(void)light;Check(0,"unexpected live render command"); }
static int Video(const char *name,int x,int y,int width,int height,int flags) { (void)name;(void)x;(void)y;(void)width;(void)height;(void)flags;videos++;return -1; }
#ifndef Com_Memset
void Com_Memset(void *destination,int value,size_t size) { memset(destination,value,size); }
#endif
#ifndef Com_Memcpy
void Com_Memcpy(void *destination,const void *source,size_t size) { memcpy(destination,source,size); }
#endif
static char **ListScripts(const char *path,const char *extension,int *number) { Check(!strcmp(path,"scripts") && !strcmp(extension,".shader"),"native shader file listing");*number=1;return scriptFiles; }
static void FreeScriptList(char **list) { Check(list==scriptFiles,"native shader list release");listFrees++; }
static int ReadScript(const char *name,void **buffer) { int size=strlen(archive);Check(!strcmp(name,"scripts/fixture.shader") && !scriptFile,"native shader file ownership");scriptFile=malloc(size+1);Check(scriptFile!=NULL,"native script fixture allocation");memcpy(scriptFile,archive,size+1);*buffer=scriptFile;scriptReads++;return size; }
static void FreeScript(void *pointer) { Check(pointer && pointer==scriptFile,"native script file release");free(pointer);scriptFile=NULL;scriptFrees++; }
static void ResetParser(void) { int i;memset(&shader,0,sizeof(shader));memset(stages,0,sizeof(stages));memset(texMods,0,sizeof(texMods));Q_strncpyz(shader.name,"tests/material",sizeof(shader.name));shader.lightmapIndex=LIGHTMAP_NONE;for(i=0;i<MAX_SHADER_STAGES;i++)stages[i].bundle[0].texMods=texMods[i]; }
static char *Build(int count) { int i;strcpy(archive,"tests/material\n{\n");for(i=0;i<count;i++)strcat(archive,"{\nmap $whiteimage\nrgbGen vertex\nalphaGen vertex\nblendFunc blend\ndepthWrite\ntcMod scroll 0.5 -0.25\n}\n");strcat(archive,"}\ntests/following\n{\n{\nmap $whiteimage\n}\n}\n");return archive; }
static void NativeStages(void) {
	char *text;int count,i,expected;shader_t before;
	for(count=0;count<=10;count++) {
		text=Build(count);Check(!strcmp(COM_ParseExt(&text,qtrue),"tests/material"),"first shader label");ResetParser();before=shader;
		expected=count>0 && count<=MAX_SHADER_STAGES;
		Check(ParseShader(&text)==expected,"zero-to-ten stages return native valid/fallback result");
		Check(!strcmp(COM_ParseExt(&text,qtrue),"tests/following"),"following shader label stays synchronized");
		for(i=0;i<MAX_SHADER_STAGES;i++) {
			shaderStage_t *stage=&stages[i];Check(stage->active==(i<count),"only legal stage slots become active");
			if(i<count)Check(stage->bundle[0].image[0]==&white && stage->rgbGen==CGEN_VERTEX && stage->alphaGen==AGEN_VERTEX && stage->stateBits==(GLS_DEPTHMASK_TRUE|GLS_SRCBLEND_SRC_ALPHA|GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA) && stage->bundle[0].numTexMods==1 && stage->bundle[0].texMods[0].type==TMOD_SCROLL && stage->bundle[0].texMods[0].scroll[0]==0.5f && stage->bundle[0].texMods[0].scroll[1]==-0.25f,"native image/color/alpha/blend/depth/texture modification remains unchanged");
		}
		Check(shader.explicitlyDefined==expected && !strcmp(shader.name,before.name),"bounded parser metadata");
		ResetParser();Check(ParseShader(&text),"following shader parses normally");Check(!COM_ParseExt(&text,qtrue)[0],"following shader ends at EOF");
	}
}
static void TailCases(void) {
	char *text,*tail;int mode,before;
	const char *tails[]={
		"{\nvideoMap forbidden.roq\n}\n}\ntests/following\n{\n{\nmap $whiteimage\n}\n}\n",
		"{\nmap \"}\"\nmap textures/{embedded}\n/* } { */\n// } {\n{ nested }\n}\n{ ignored }\n}\ntests/following\n{\n{\nmap $whiteimage\n}\n}\n",
		"{\nmap }foo\n}\n}\ntests/following\n{\n{\nmap $whiteimage\n}\n}\n",
		"{\nmap {foo\n}\n}\ntests/following\n{\n{\nmap $whiteimage\n}\n}\n",
		"{\nvideoMap never-open.roq\n",
		"{\n/* unterminated { }",
		"{\nmap \"unterminated { }"
	};
	for(mode=0;mode<7;mode++) {
		text=Build(8);tail=strstr(archive,"}\ntests/following");Check(tail!=NULL,"native overflowing-tail fixture");strcpy(tail,tails[mode]);COM_ParseExt(&text,qtrue);ResetParser();before=videos;
		Check(!ParseShader(&text) && videos==before,"excess stages skip cinematic/resource parsing and fall back");
		if(mode<4) { Check(!strcmp(COM_ParseExt(&text,qtrue),"tests/following"),"comments/quotes/nesting preserve following label");ResetParser();Check(ParseShader(&text),"following shader survives skipped malformed tail"); }
		else Check(text==NULL,"truncated/comment/quote tail stops safely at EOF");
	}
}
static void Registration(void) {
	int count,before;shader_t *material,*following;char *text;
	for(count=0;count<=10;count++) {
		Release();tr.whiteImage=&white;text=Build(count);s_shaderText=text;
		material=R_FindShader("tests/material",LIGHTMAP_NONE,qtrue);
		Check(material && material->defaultShader==!(count>0 && count<=MAX_SHADER_STAGES) && material->numUnfoggedPasses==(count>MAX_SHADER_STAGES?MAX_SHADER_STAGES:count),"native registration caches bounded valid/default shader");
		before=allocations;Check(R_FindShader("tests/material",LIGHTMAP_NONE,qtrue)==material && allocations==before,"native valid/default cache reuse");
		following=R_FindShader("tests/following",LIGHTMAP_NONE,qtrue);Check(following && !following->defaultShader && following->numUnfoggedPasses==1,"native following shader registration after invalid definition");
	}
}
int main(void) {
	int i;char *text;ri.Printf=Print;ri.Hunk_Alloc=Allocate;ri.CIN_PlayCinematic=Video;tr.whiteImage=&white;
	for(i=0;i<MAX_SHADERTEXT_HASH;i++)shaderTextHashTable[i]=emptyHash;
	NativeStages();TailCases();
	ResetParser();text="{\nsurfaceParm fog\n}\n";Check(ParseShader(&text),"native zero-stage fog remains valid");
	ResetParser();text="{\nskyparms - 512 -\n}\n";Check(ParseShader(&text) && shader.isSky,"native zero-stage sky remains valid");
	Registration();
	for(i=0;i<5;i++) {
		int bucket,beforeReads=scriptReads,beforeFrees=scriptFrees,beforeLists=listFrees;
		const char *maps[]={"map \"}\"\n/* { } */", "map }foo", "map {foo", "map }foo\nmap {foo", "map }foo\nmap {foo"};
		Release();tr.whiteImage=&white;
		for(bucket=0;bucket<MAX_SHADERTEXT_HASH;bucket++)shaderTextHashTable[bucket]=emptyHash;
		Build(i==4?1:8);text=strstr(archive,"}\ntests/following");Check(text!=NULL,"brace-token archive fixture");
		if(i==4) {
			text=strstr(archive,"map $whiteimage");Check(text!=NULL,"valid brace-token stage fixture");
			snprintf(text,sizeof(archive)-(text-archive),"%s\n}\n}\ntests/following\n{\n{\nmap $whiteimage\n}\n}\n",maps[i]);
		} else snprintf(text,sizeof(archive)-(text-archive),"{\n%s\n}\n}\ntests/following\n{\n{\nmap $whiteimage\n}\n}\n",maps[i]);
		s_shaderText=archive;
		Check(R_FindShader("tests/material",LIGHTMAP_NONE,qtrue)->defaultShader==(i!=4),"brace-token stage registers native valid/fallback result");
		Check(!R_FindShader("tests/following",LIGHTMAP_NONE,qtrue)->defaultShader,"native lookup skips quoted and brace-prefixed filename tokens");
		Release();tr.whiteImage=&white;ri.FS_ListFiles=ListScripts;ri.FS_FreeFileList=FreeScriptList;ri.FS_ReadFile=ReadScript;ri.FS_FreeFile=FreeScript;
		ScanAndLoadShaderFiles();
		Check(scriptReads==beforeReads+1 && scriptFrees==beforeFrees+1 && listFrees==beforeLists+1 && !scriptFile,"native shader archive indexing frees script/list ownership");
		Check(R_FindShader("tests/material",LIGHTMAP_NONE,qtrue)->defaultShader==(i!=4),"indexed brace-token shader uses native valid/fallback result");
		Check(!R_FindShader("tests/following",LIGHTMAP_NONE,qtrue)->defaultShader,"both native hash passes preserve following definition with brace-prefixed filename tokens");
	}
	Release();puts("Native shader zero-to-ten stages, synchronization, EOF and registration/cache checks passed (issue #46)");return 0;
}
