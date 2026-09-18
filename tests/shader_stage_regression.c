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
static int traceImages,imageFinds,skyInitializations;
static char imageNames[16][MAX_QPATH];
static float skyHeight;
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
static void Release(void) { while(allocations)free(owned[--allocations]);memset(&tr,0,sizeof(tr));memset(hashTable,0,sizeof(hashTable));memset(shaderTextHashTable,0,sizeof(shaderTextHashTable));s_shaderText=NULL;s_shaderTextIndexed=qfalse; }
image_t *R_FindImageFile(const char *name,qboolean mipmap,qboolean picmip,int wrap) { (void)mipmap;(void)picmip;(void)wrap;if(traceImages){Check(imageFinds<16,"bounded sky import trace");Q_strncpyz(imageNames[imageFinds++],name,MAX_QPATH);}return &white; }
void R_InitSkyTexCoords(float height) { if(traceImages){skyInitializations++;skyHeight=height;} }
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
static void AlphaIdentity(void) {
	char *text;
	const char *bodies[]={"map $whiteimage\nrgbGen identity\nalphaGen identity\n}", "map $whiteimage\nrgbGen lightingDiffuse\nalphaGen identity\n}", "map $whiteimage\nrgbGen vertex\nalphaGen identity\n}", "map $whiteimage\nrgbGen identity\nalphaGen vertex\n}"};
	int mode;
	for(mode=0;mode<4;mode++) { ResetParser();text=(char *)bodies[mode];Check(ParseStage(&stages[0],&text),"native alpha stage parses");Check(stages[0].alphaGen==(mode<2?AGEN_SKIP:mode==2?AGEN_IDENTITY:AGEN_VERTEX),"identity/diffuse can skip identity alpha while other native alpha modes remain unchanged"); }
}
static void Texture(unsigned int unit) { (void)unit;Check(0,"collapse must not issue graphics calls"); }
static void AlphaWaves(void) {
	int mode;char *text;shaderStage_t before[MAX_SHADER_STAGES];shader_t material;
	for(mode=0;mode<8;mode++) {
		ResetParser();text="{\n{\nmap $whiteimage\nrgbGen vertex\nalphaGen wave sin 0.2 0.3 0.4 0.5\n}\n{\nmap $whiteimage\nrgbGen vertex\nalphaGen wave sin 0.2 0.3 0.4 0.5\nblendFunc filter\n}\n}\n";
		Check(ParseShader(&text) && stages[0].alphaGen==AGEN_WAVEFORM && stages[1].alphaGen==AGEN_WAVEFORM,"actual native waveform pair parses");
		if(mode==1)stages[1].alphaWave.base+=1;
		if(mode==2)stages[1].alphaWave.amplitude+=1;
		if(mode==3)stages[1].alphaWave.phase+=1;
		if(mode==4)stages[1].alphaWave.frequency+=1;
		if(mode==5)stages[1].alphaWave.func=GF_SQUARE;
		if(mode==6) { stages[0].alphaGen=stages[1].alphaGen=AGEN_PORTAL;stages[1].alphaWave.base+=1; }
		if(mode==7) { stages[0].rgbGen=stages[1].rgbGen=CGEN_WAVEFORM;stages[1].rgbWave.base+=1; }
		memcpy(before,stages,sizeof(before));material=shader;qglActiveTextureARB=Texture;
		Check(CollapseMultitexture()==(mode==0 || mode==6),"only identical alpha waveforms collapse; unused portal wave fields do not block collapse");
		if(mode==0 || mode==6)Check(shader.multitextureEnv==GL_MODULATE && stages[0].bundle[1].image[0]==&white && !stages[1].active && stages[0].alphaGen==before[0].alphaGen && !memcmp(&stages[0].alphaWave,&before[0].alphaWave,sizeof(waveForm_t)),"accepted collapse preserves native alpha/texture stage");
		else Check(!memcmp(stages,before,sizeof(before)) && !memcmp(&shader,&material,sizeof(material)),"rejected waveform collapse leaves both stages unchanged");
		qglActiveTextureARB=NULL;
	}
	for(mode=0;mode<2;mode++) {
		shader_t *registered;int beforeAllocations;
		Release();tr.whiteImage=&white;
		snprintf(archive,sizeof(archive),"tests/material\n{\n{\nmap $whiteimage\nrgbGen vertex\nalphaGen wave sin 0.2 0.3 0.4 0.5\n}\n{\nmap $whiteimage\nrgbGen vertex\nalphaGen wave sin %s 0.3 0.4 0.5\nblendFunc filter\n}\n}\n",mode?"0.7":"0.2");
		s_shaderText=archive;qglActiveTextureARB=Texture;
		registered=R_FindShader("tests/material",LIGHTMAP_NONE,qtrue);
		Check(!registered->defaultShader && registered->numUnfoggedPasses==(mode?2:1) && registered->stages[0]->alphaGen==AGEN_WAVEFORM,"actual shader registration retains different alpha passes and collapses identical waveforms");
		beforeAllocations=allocations;Check(R_FindShader("tests/material",LIGHTMAP_NONE,qtrue)==registered && allocations==beforeAllocations,"waveform shader cache reuse");
		qglActiveTextureARB=NULL;
	}
	Release();tr.whiteImage=&white;
}
static void FastAlpha(void) {
	int kind,mode;const alphaGen_t alpha[]={AGEN_IDENTITY,AGEN_SKIP,AGEN_VERTEX,AGEN_WAVEFORM};
	shader_t *registered;r_ignoreFastPath=&zero;
	for(kind=0;kind<2;kind++)for(mode=0;mode<4;mode++) {
		ResetParser();shader.numUnfoggedPasses=1;stages[0].rgbGen=kind?CGEN_IDENTITY:CGEN_LIGHTING_DIFFUSE;stages[0].alphaGen=alpha[mode];stages[0].bundle[0].tcGen=TCGEN_TEXTURE;
		if(kind) { stages[0].bundle[1].tcGen=TCGEN_LIGHTMAP;shader.multitextureEnv=GL_MODULATE; }
		ComputeStageIteratorFunc();
		Check(shader.optimalStageIteratorFunc==(mode<2?(kind?RB_StageIteratorLightmappedMultitexture:RB_StageIteratorVertexLitTexture):RB_StageIteratorGeneric),"identity/skipped alpha retains eligible fast paths while varying alpha uses generic rendering");
	}
	Release();tr.whiteImage=&white;s_shaderText="tests/diffuse\n{\n{\nmap $whiteimage\nrgbGen lightingDiffuse\nalphaGen identity\n}\n}\n";
	registered=R_FindShader("tests/diffuse",LIGHTMAP_NONE,qtrue);Check(!registered->defaultShader && registered->stages[0]->alphaGen==AGEN_SKIP && registered->optimalStageIteratorFunc==RB_StageIteratorVertexLitTexture,"actual explicit diffuse registration retains vertex-lit iterator after alpha skip");
	Release();tr.whiteImage=&white;tr.numLightmaps=1;tr.lightmaps[0]=&white;qglActiveTextureARB=Texture;
	s_shaderText="tests/lightmapped\n{\n{\nmap $whiteimage\nrgbGen identity\nalphaGen identity\n}\n{\nmap $lightmap\nrgbGen identity\nalphaGen identity\nblendFunc filter\n}\n}\n";
	registered=R_FindShader("tests/lightmapped",0,qtrue);Check(!registered->defaultShader && registered->numUnfoggedPasses==1 && registered->stages[0]->alphaGen==AGEN_SKIP && registered->optimalStageIteratorFunc==RB_StageIteratorLightmappedMultitexture,"actual collapsed lightmapped registration retains its specialized iterator after alpha skip");
	qglActiveTextureARB=NULL;r_ignoreFastPath=&one;Release();tr.whiteImage=&white;
}
static void ConstantVectors(int proof) {
    int mode,i;char body[512],*text;shader_t *registered;
    const char *bad[]={"nan","inf","-inf","1e400","1e39","-1e39"};
    const char *broken[]={"bad", "( 0.1 0.2 )", "( 0.1 0.2 0.3 bad", "( 0.1\n", "( \"\" 0.2 0.3 )"};
    if(proof>=0) {
        ResetParser();text=proof==0?"map $whiteimage\nrgbGen const ( nan 0.2 0.3 )\n}":proof==1?"map $whiteimage\nalphaGen const nan\n}":"map $whiteimage\ntcGen vector bad bad\n}";
        Check(!ParseStage(&stages[0],&text),"non-finite constants and malformed tc vectors must reject");return;
    }
    for(mode=0;mode<6;mode++) {
        ResetParser();snprintf(body,sizeof(body),"map $whiteimage\nrgbGen const ( %s 0.2 0.3 )\n}",bad[mode]);text=body;
        if(ParseStage(&stages[0],&text)) {fprintf(stderr,"Accepted invalid RGB component: %s\n",bad[mode]);Check(0,"non-finite/out-of-float-range RGB vectors reject before byte conversion");}
        if(mode<4) {ResetParser();snprintf(body,sizeof(body),"map $whiteimage\nalphaGen const %s\n}",bad[mode]);text=body;Check(!ParseStage(&stages[0],&text),"non-finite alpha rejects before byte conversion");}
        ResetParser();snprintf(body,sizeof(body),"map $whiteimage\ntcGen vector ( 0 1 %s ) ( 1 0 0 )\n}",bad[mode]);text=body;
        Check(!ParseStage(&stages[0],&text),"non-finite/overflow first texture vector rejects");
        ResetParser();snprintf(body,sizeof(body),"map $whiteimage\ntcGen vector ( 0 1 0 ) ( 1 %s 0 )\n}",bad[mode]);text=body;
        Check(!ParseStage(&stages[0],&text),"non-finite/overflow second texture vector rejects");
    }
    for(mode=0;mode<5;mode++) {
        ResetParser();snprintf(body,sizeof(body),"map $whiteimage\nrgbGen const %s\n}",broken[mode]);text=body;
        Check(!ParseStage(&stages[0],&text),"malformed constant RGB propagates vector failure");
        ResetParser();snprintf(body,sizeof(body),"map $whiteimage\ntcGen vector %s bad\n}",broken[mode]);text=body;
        Check(!ParseStage(&stages[0],&text),"malformed texture vector propagates failure");
    }
    ResetParser();text="map $whiteimage\ntcGen vector bad bad\n}";Check(!ParseStage(&stages[0],&text),"both missing texture parentheses reject");
    ResetParser();text="map $whiteimage\nalphaGen const\n}";Check(!ParseStage(&stages[0],&text),"missing alpha constant rejects");
    for(i=0;i<=512;i++) {
        float value=(float)i/512.0f;double a=(double)i/512.0;byte rgb=(byte)(255.0f*value),alpha=(byte)(255.0*a);
        ResetParser();snprintf(body,sizeof(body),"map $whiteimage\nrgbGen const ( %.9g %.9g %.9g )\nalphaGen const %.17g\n}",value,value,value,a);text=body;
        Check(ParseStage(&stages[0],&text) && stages[0].rgbGen==CGEN_CONST && stages[0].alphaGen==AGEN_CONST && stages[0].constantColor[0]==rgb && stages[0].constantColor[1]==rgb && stages[0].constantColor[2]==rgb && stages[0].constantColor[3]==alpha,"all 513 valid RGB float/alpha double quantization goldens stay native");
    }
    for(i=1;i<255;i++)for(mode=0;mode<2;mode++) {
        float value=nextafterf((float)i/255.0f,mode?1.0f:0.0f);byte rgb=(byte)(255.0f*value);
        ResetParser();snprintf(body,sizeof(body),"map $whiteimage\nrgbGen const ( %.9g %.9g %.9g )\n}",value,value,value);text=body;
        Check(ParseStage(&stages[0],&text) && stages[0].constantColor[0]==rgb && stages[0].constantColor[1]==rgb && stages[0].constantColor[2]==rgb,"adjacent float byte thresholds retain native rounding");
    }
    ResetParser();text="map $whiteimage\nrgbGen const ( -1e30 0.5 1e30 )\nalphaGen const -1e300\n}";
    Check(ParseStage(&stages[0],&text) && stages[0].constantColor[0]==0 && stages[0].constantColor[1]==127 && stages[0].constantColor[2]==255 && stages[0].constantColor[3]==0,"finite out-of-range constants saturate before byte conversion");
    ResetParser();text="map $whiteimage\nalphaGen const 1e300\ntcGen vector ( -3.4e38 0 3.4e38 ) ( 0 -1 1 )\n}";
    Check(ParseStage(&stages[0],&text) && stages[0].constantColor[3]==255 && stages[0].bundle[0].tcGen==TCGEN_VECTOR && stages[0].bundle[0].tcGenVectors[0][0]<0 && stages[0].bundle[0].tcGenVectors[0][2]>0,"large finite alpha clamps and full finite texture vector range remains accepted");
    for(mode=0;mode<4;mode++) {
        Release();tr.whiteImage=&white;
        snprintf(archive,sizeof(archive),"tests/material\n{\n{\nmap $whiteimage\n%s\n}\n}\ntests/following\n{\n{\nmap $whiteimage\n}\n}\n",mode==0?"rgbGen const bad":mode==1?"rgbGen const ( nan 0 0 )":mode==2?"alphaGen const nan":"tcGen vector bad bad");
        s_shaderText=archive;registered=R_FindShader("tests/material",LIGHTMAP_NONE,qtrue);Check(registered->defaultShader,"actual invalid constant/vector definition caches default fallback");
        i=allocations;Check(R_FindShader("tests/material",LIGHTMAP_NONE,qtrue)==registered && i==allocations,"invalid vector fallback cache reuse");
        Check(!R_FindShader("tests/following",LIGHTMAP_NONE,qtrue)->defaultShader,"following definition survives malformed constant/vector");
    }
    Release();tr.whiteImage=&white;
}
static unsigned int NumericFingerprint(const void *data,size_t size) {
    const unsigned char *p=data;unsigned int hash=2166136261u;while(size--) {hash^=*p++;hash*=16777619u;}return hash;
}
static void WaveModifiers(int proof) {
    int i,mode,count;char body[4096],*text;shader_t *registered;
    const char *bad[]={"nan","inf","-inf","1e400","1e39","-1e39"};
    const char *valid[]={"turb 0.2 0.3 0.4 0.5","scale 0.5 -0.25","scroll 0.5 -0.25","stretch sin 0.2 0.3 0.4 0.5","transform 1 2 3 4 -0.5 0.25","rotate -120.5","entityTranslate"};
    static const unsigned int tcGolden[]={0x6a70d248,0x378d5e5e,0xd586c719,0x76bc2d66,0xe602d276,0x24c57d34,0x84b4e9e2};
    static const unsigned int deformGolden[]={0x4a1289c0,0xcde66f33,0x4c9fcd62,0x56d4dbea,0x4c8fbe27,0x104312b3,0x14395e84,0xcbb02c02};
    const char *broken[]={"turb 0.2 0.3 0.4","scale 0.5","scroll 0.5","stretch sin 0.2 0.3 0.4","transform 1 2 3 4 5","rotate","missingType"};
    const char *deforms[]={"projectionShadow","autosprite","autosprite2","text7","bulge 1.5 -2.5 3.5","wave 2 sin 0.2 0.3 0.4 0.5","normal 0.5 0.25","move 1 -2 3 sin 0.2 0.3 0.4 0.5"};
    if(proof>=0 && proof<4) {
        const char *bodies[]={"map $whiteimage\nrgbGen wave sin 0.2 0.3\n}","map $whiteimage\nrgbGen wave sin nan 0.3 0.4 0.5\n}","map $whiteimage\ntcMod scale nan 1\n}","map $whiteimage\ntcMod scale 0.5\n}"};
        ResetParser();text=(char*)bodies[proof];Check(!ParseStage(&stages[0],&text),"invalid/missing waveform/modifier fields must reject");return;
    }
    for(i=0;i<7;i++) {
        ResetParser();snprintf(body,sizeof(body),"map $whiteimage\ntcMod %s\n}",valid[i]);text=body;Check(ParseStage(&stages[0],&text),"native valid texture modifier");
        if(proof==4)printf("TC %d %08x\n",i,NumericFingerprint(&texMods[0][0],sizeof(texMods[0][0])));
        else Check(stages[0].bundle[0].numTexMods==1 && NumericFingerprint(&texMods[0][0],sizeof(texMods[0][0]))==tcGolden[i],"complete valid modifier retains stock bytes");
    }
    for(i=0;i<8;i++) {
        ResetParser();snprintf(body,sizeof(body),"{\ndeformVertexes %s\n{\nmap $whiteimage\n}\n}\n",deforms[i]);text=body;Check(ParseShader(&text),"native valid shader deformation");
        if(proof==4)printf("DEFORM %d %08x\n",i,NumericFingerprint(&shader.deforms[0],sizeof(shader.deforms[0])));
        else Check(shader.numDeforms==1 && NumericFingerprint(&shader.deforms[0],sizeof(shader.deforms[0]))==deformGolden[i],"complete valid deformation retains stock bytes");
    }
    if(proof==4)return;
    for(count=0;count<2;count++)for(i=0;i<(count?8:7);i++) {
        char scratch[256],words[12][48],*p;int wordCount=0,j,k;
        strcpy(scratch,count?deforms[i]:valid[i]);p=scratch;
        while(1) {char *word=COM_ParseExt(&p,qtrue);if(!word[0])break;Check(wordCount<12,"bounded numeric mutation fixture");Q_strncpyz(words[wordCount++],word,sizeof(words[0]));}
        for(j=1;j<wordCount;j++)if((words[j][0]>='0' && words[j][0]<='9') || words[j][0]=='-')for(mode=0;mode<6;mode++) {
            ResetParser();strcpy(body,count?"{\ndeformVertexes ":"map $whiteimage\ntcMod ");
            for(k=0;k<wordCount;k++) {strcat(body,k==j?bad[mode]:words[k]);strcat(body," ");}
            strcat(body,count?"\n{\nmap $whiteimage\n}\n}\n":"\n}");text=body;
            Check(!(count?ParseShader(&text):ParseStage(&stages[0],&text)),"each numeric field in every modifier/deformation type rejects non-finite/overflow");
            if(!count)Check(!stages[0].bundle[0].numTexMods,"invalid numeric modifier never publishes staging");
        }
    }
    for(count=0;count<4;count++) {
        const char *partial[]={"sin","sin 0.2","sin 0.2 0.3","sin 0.2 0.3 0.4"};
        ResetParser();snprintf(body,sizeof(body),"map $whiteimage\nrgbGen wave %s\n}",partial[count]);text=body;Check(!ParseStage(&stages[0],&text),"all incomplete RGB waveform prefixes reject");
        ResetParser();snprintf(body,sizeof(body),"map $whiteimage\nalphaGen wave %s\n}",partial[count]);text=body;Check(!ParseStage(&stages[0],&text),"all incomplete alpha waveform prefixes reject");
    }
    ResetParser();memset(&texMods[0][0],0xa5,sizeof(texMods[0][0]));text="map $whiteimage\ntcMod scale 0.5 -0.25\n}";
    Check(ParseStage(&stages[0],&text) && NumericFingerprint(&texMods[0][0],sizeof(texMods[0][0]))==tcGolden[1],"complete modifier staging clears stale unused fields");
    for(count=0;count<3;count++) {
        const char *zeros[]={"0","-0","garbage"};ResetParser();snprintf(body,sizeof(body),"{\ndeformVertexes wave %s sin 0 1 0 1\n{\nmap $whiteimage\n}\n}\n",zeros[count]);text=body;
        Check(ParseShader(&text) && shader.deforms[0].deformationSpread==100,"native zero/negative-zero/legacy atof fallback spread remains accepted");
    }
    for(mode=0;mode<6;mode++)for(i=0;i<4;i++) {
        ResetParser();snprintf(body,sizeof(body),"map $whiteimage\nrgbGen wave sin %s %s %s %s\n}",i==0?bad[mode]:"0.2",i==1?bad[mode]:"0.3",i==2?bad[mode]:"0.4",i==3?bad[mode]:"0.5");text=body;
        Check(!ParseStage(&stages[0],&text),"every non-finite/overflow RGB waveform field rejects");
        ResetParser();snprintf(body,sizeof(body),"map $whiteimage\nalphaGen wave sin %s %s %s %s\n}",i==0?bad[mode]:"0.2",i==1?bad[mode]:"0.3",i==2?bad[mode]:"0.4",i==3?bad[mode]:"0.5");text=body;Check(!ParseStage(&stages[0],&text),"every non-finite/overflow alpha waveform field rejects");
        ResetParser();snprintf(body,sizeof(body),"map $whiteimage\ntcMod turb %s %s %s %s\n}",i==0?bad[mode]:"0.2",i==1?bad[mode]:"0.3",i==2?bad[mode]:"0.4",i==3?bad[mode]:"0.5");text=body;Check(!ParseStage(&stages[0],&text),"every non-finite/overflow turbulent modifier field rejects");
    }
    for(i=0;i<7;i++) {
        ResetParser();snprintf(body,sizeof(body),"map $whiteimage\ntcMod %s\n}",broken[i]);text=body;
        Check(!ParseStage(&stages[0],&text) && !stages[0].bundle[0].numTexMods,"incomplete/unknown modifier does not publish a slot");
    }
    for(count=0;count<=TR_MAX_TEXMODS+1;count++) {
        ResetParser();strcpy(body,"map $whiteimage\n");for(i=0;i<count;i++)strcat(body,"tcMod scroll 0.5 -0.25\n");strcat(body,"}");text=body;
        Check(ParseStage(&stages[0],&text)==(count<=TR_MAX_TEXMODS) && stages[0].bundle[0].numTexMods==(count>TR_MAX_TEXMODS?TR_MAX_TEXMODS:count),"texture modifier count cap precedes access and falls back");
    }
    ResetParser();strcpy(body,"map $whiteimage\ntcMod scale 1 1 ");memset(body+strlen(body),'x',1500);body[strlen("map $whiteimage\ntcMod scale 1 1 ")+1500]=0;strcat(body,"\n}");text=body;
    Check(!ParseStage(&stages[0],&text),"overlong modifier line rejects before truncation");
    for(mode=0;mode<6;mode++) {
        ResetParser();snprintf(body,sizeof(body),"map $whiteimage\nanimMap %s fixture.tga\n}",bad[mode]);text=body;Check(!ParseStage(&stages[0],&text),"invalid animation speed rejects");
        ResetParser();snprintf(body,sizeof(body),"map $whiteimage\nalphaGen portal %s\n}",bad[mode]);text=body;Check(!ParseStage(&stages[0],&text),"invalid portal range rejects");
        ResetParser();snprintf(body,sizeof(body),"{\ndeformVertexes bulge 1 %s 3\n{\nmap $whiteimage\n}\n}",bad[mode]);text=body;Check(!ParseShader(&text),"invalid deformation field rejects");
    }
    ResetParser();text="{\ndeformVertexes wave 1e-320 sin 0 1 0 1\n{\nmap $whiteimage\n}\n}";Check(!ParseShader(&text),"overflowed reciprocal deformation spread rejects");
    ResetParser();text="{\ndeformVertexes normal 0.5\n{\nmap $whiteimage\n}\n}";Check(!ParseShader(&text),"missing deformation field rejects");
    for(count=0;count<=MAX_SHADER_DEFORMS+1;count++) {
        ResetParser();strcpy(body,"{\n");for(i=0;i<count;i++)strcat(body,"deformVertexes autosprite\n");strcat(body,"{\nmap $whiteimage\n}\n}\n");text=body;
        Check(ParseShader(&text)==(count<=MAX_SHADER_DEFORMS),"deformation cap falls back before out-of-range access");
    }
    for(mode=0;mode<4;mode++) {
        Release();tr.whiteImage=&white;snprintf(archive,sizeof(archive),"tests/material\n{\n%s\n{\nmap $whiteimage\n%s\n}\n}\ntests/following\n{\n{\nmap $whiteimage\n}\n}\n",mode==3?"deformVertexes wave 1e-320 sin 0 1 0 1":"",mode==0?"rgbGen wave sin nan 0.3 0.4 0.5":mode==1?"tcMod scale nan 1":mode==2?"tcMod scale 0.5":"");
        s_shaderText=archive;registered=R_FindShader("tests/material",LIGHTMAP_NONE,qtrue);Check(registered->defaultShader,"invalid numeric/modifier shader uses native fallback");
        i=allocations;Check(R_FindShader("tests/material",LIGHTMAP_NONE,qtrue)==registered && i==allocations,"invalid numeric/modifier fallback cached");Check(!R_FindShader("tests/following",LIGHTMAP_NONE,qtrue)->defaultShader,"following definition survives numeric/modifier fallback");
    }
    Release();tr.whiteImage=&white;
}
static void Metadata(int proof) {
    int mode,i;char body[4096],path[64],*text;vec3_t oldLight={3,4,5},oldDirection={6,7,8};
    const char *bad[]={"nan","inf","-inf","1e400","1e39","-1e39"};
    const char *sorts[]={"portal","sky","opaque","decal","seeThrough","banner","additive","nearest","underwater","4.5","garbage"};
    const float sortValues[]={SS_PORTAL,SS_ENVIRONMENT,SS_OPAQUE,SS_DECAL,SS_SEE_THROUGH,SS_BANNER,SS_BLEND1,SS_NEAREST,SS_UNDERWATER,4.5,0};
    if(proof>=0 && proof<4) {
        const char *bodies[]={"{\nq3map_sun 1 nan 3 100 45 60\n{\nmap $whiteimage\n}\n}","{\nsort nan\n{\nmap $whiteimage\n}\n}","{\nskyparms - nan -\n}","{\nfogParms ( 0.2 0.3 0.4 ) nan\n{\nmap $whiteimage\n}\n}"};
        ResetParser();text=(char*)bodies[proof];Check(!ParseShader(&text),"non-finite shader metadata must reject");return;
    }
    for(mode=0;mode<11;mode++) {
        ResetParser();snprintf(body,sizeof(body),"{\nsort %s\n{\nmap $whiteimage\n}\n}",sorts[mode]);text=body;Check(ParseShader(&text) && shader.sort==sortValues[mode],"all native named/numeric/legacy-zero sort values remain unchanged");
    }
    ResetParser();text="{\nq3map_sun 1 2 3 100 45 60\nfogParms ( 0.2 0.3 0.4 ) 256\nclampTime 12.5\n{\nmap $whiteimage\n}\n}";Check(ParseShader(&text),"valid native metadata corpus");
    if(proof==4) {printf("SUNLIGHT %08x\n",NumericFingerprint(tr.sunLight,sizeof(tr.sunLight)));printf("SUNDIRECTION %08x\n",NumericFingerprint(tr.sunDirection,sizeof(tr.sunDirection)));printf("FOG %08x\n",NumericFingerprint(&shader.fogParms,sizeof(shader.fogParms)));return;}
    Check(NumericFingerprint(tr.sunLight,sizeof(tr.sunLight))==0x8f2a7bb8 && NumericFingerprint(tr.sunDirection,sizeof(tr.sunDirection))==0x4872e335 && NumericFingerprint(&shader.fogParms,sizeof(shader.fogParms))==0xf9df3e46,"complete valid metadata retains captured stock bytes");
    Check(shader.clampTime==12.5f && shader.fogParms.depthForOpaque==256 && shader.fogParms.color[0]==0.2f && shader.fogParms.color[1]==0.3f && shader.fogParms.color[2]==0.4f,"native valid fog/clamp fields");
    for(mode=0;mode<6;mode++) {
        for(i=0;i<6;i++) {
            ResetParser();VectorCopy(oldLight,tr.sunLight);VectorCopy(oldDirection,tr.sunDirection);
            snprintf(body,sizeof(body),"{\nq3map_sun %s %s %s %s %s %s\n{\nmap $whiteimage\n}\n}",i==0?bad[mode]:"1",i==1?bad[mode]:"2",i==2?bad[mode]:"3",i==3?bad[mode]:"100",i==4?bad[mode]:"45",i==5?bad[mode]:"60");text=body;
            Check(!ParseShader(&text),"every non-finite/overflow sun parameter rejects");Check(!memcmp(tr.sunLight,oldLight,sizeof(oldLight)) && !memcmp(tr.sunDirection,oldDirection,sizeof(oldDirection)),"rejected sun parameters retain renderer state");
        }
        ResetParser();snprintf(body,sizeof(body),"{\nsort %s\n{\nmap $whiteimage\n}\n}",bad[mode]);text=body;Check(!ParseShader(&text),"invalid numeric sort rejects");
        ResetParser();snprintf(body,sizeof(body),"{\nclampTime %s\n{\nmap $whiteimage\n}\n}",bad[mode]);text=body;Check(!ParseShader(&text),"invalid clamp time rejects");
        ResetParser();snprintf(body,sizeof(body),"{\nfogParms ( 0.2 0.3 0.4 ) %s\n{\nmap $whiteimage\n}\n}",bad[mode]);text=body;Check(!ParseShader(&text),"invalid fog depth rejects");
        ResetParser();traceImages=1;imageFinds=0;skyInitializations=0;snprintf(body,sizeof(body),"{\nskyparms outer %s inner\n}",bad[mode]);text=body;Check(!ParseShader(&text) && !imageFinds && !skyInitializations,"invalid sky number rejects before imports");traceImages=0;
    }
    ResetParser();VectorCopy(oldLight,tr.sunLight);VectorCopy(oldDirection,tr.sunDirection);text="{\nq3map_sun 1 2 3 100 45 60\nunknownParameter\n}";
    Check(!ParseShader(&text) && !memcmp(tr.sunLight,oldLight,sizeof(oldLight)) && !memcmp(tr.sunDirection,oldDirection,sizeof(oldDirection)),"later shader failure never publishes pending valid sun parameters");
    ResetParser();text="{\nq3map_sun 3.4e38 3.4e38 3.4e38 100 45 60\n{\nmap $whiteimage\n}\n}";
    Check(!ParseShader(&text),"finite-source sun length overflow rejects");
    for(mode=0;mode<6;mode++) {
        const char *partial[]={"","1","1 2","1 2 3","1 2 3 100","1 2 3 100 45"};ResetParser();snprintf(body,sizeof(body),"{\nq3map_sun %s\n{\nmap $whiteimage\n}\n}",partial[mode]);text=body;Check(!ParseShader(&text),"every truncated sun prefix rejects");
    }
    for(mode=0;mode<4;mode++) {
        const char *partial[]={"skyparms","skyparms outer","skyparms outer 512","skyparms outer 512 \"\""};
        ResetParser();traceImages=1;imageFinds=0;skyInitializations=0;snprintf(body,sizeof(body),"{\n%s\n}",partial[mode]);text=body;Check(!ParseShader(&text) && !imageFinds && !skyInitializations,"incomplete sky rejects before imports");traceImages=0;
    }
    for(mode=0;mode<2;mode++) {
        ResetParser();traceImages=1;imageFinds=0;skyInitializations=0;memset(path,'x',57);path[57]=0;
        snprintf(body,sizeof(body),"{\nskyparms %s 512 %s\n}",mode?"outer":path,mode?path:"inner");text=body;
        Check(!ParseShader(&text) && !imageFinds && !skyInitializations,"oversize completed sky image path rejects before imports");traceImages=0;
    }
    for(mode=0;mode<2;mode++) {
        ResetParser();traceImages=1;imageFinds=0;skyInitializations=0;snprintf(body,sizeof(body),"{\nskyparms outer %s inner\n}",mode?"0":"512");text=body;
        Check(ParseShader(&text) && shader.isSky && shader.sky.cloudHeight==512 && imageFinds==12 && skyInitializations==1 && skyHeight==512,"native valid/zero-height sky imports and defaults remain unchanged");
        Check(!strcmp(imageNames[0],"outer_rt.tga") && !strcmp(imageNames[5],"outer_dn.tga") && !strcmp(imageNames[6],"inner_rt.tga") && !strcmp(imageNames[11],"inner_dn.tga"),"native sky face suffix/order remains unchanged");traceImages=0;
    }
    ResetParser();traceImages=1;imageFinds=0;skyInitializations=0;memset(path,'x',56);path[56]=0;snprintf(body,sizeof(body),"{\nskyparms %s 512 -\n}",path);text=body;
    Check(ParseShader(&text) && imageFinds==6 && strlen(imageNames[0])==MAX_QPATH-1,"maximum full native sky image path accepted");traceImages=0;
    ResetParser();text="{\nfogParms ( -1e30 0.5 1e30 ) 1\n{\nmap $whiteimage\n}\n}";Check(ParseShader(&text) && shader.fogParms.color[0]==0 && shader.fogParms.color[1]==0.5f && shader.fogParms.color[2]==1,"finite out-of-range fog colors clamp before later byte conversion");
    for(mode=0;mode<4;mode++) {
        const char *invalid[]={"q3map_sun 1 nan 3 100 45 60","sort nan","skyparms - nan -","fogParms ( 0.2 0.3 0.4 ) nan"};shader_t *registered;int before;
        Release();tr.whiteImage=&white;VectorCopy(oldLight,tr.sunLight);VectorCopy(oldDirection,tr.sunDirection);
        snprintf(archive,sizeof(archive),"tests/material\n{\n%s\n{\nmap $whiteimage\n}\n}\ntests/following\n{\n{\nmap $whiteimage\n}\n}\n",invalid[mode]);s_shaderText=archive;
        registered=R_FindShader("tests/material",LIGHTMAP_NONE,qtrue);Check(registered->defaultShader,"public invalid metadata uses native default fallback");before=allocations;
        Check(R_FindShader("tests/material",LIGHTMAP_NONE,qtrue)==registered && allocations==before,"public invalid metadata cache reuse");
        Check(!memcmp(tr.sunLight,oldLight,sizeof(oldLight)) && !memcmp(tr.sunDirection,oldDirection,sizeof(oldDirection)),"public invalid metadata never alters renderer sun");
        Check(!R_FindShader("tests/following",LIGHTMAP_NONE,qtrue)->defaultShader,"following definition survives invalid metadata");
    }
    Release();tr.whiteImage=&white;
    ResetParser();text="{\nsort\n{\nmap $whiteimage\n}\n}";Check(!ParseShader(&text),"missing sort parameter rejects");
    ResetParser();text="{\nclampTime\n{\nmap $whiteimage\n}\n}";Check(!ParseShader(&text),"missing clamp time rejects");
    ResetParser();text="{\nfogParms ( 0.2 0.3 0.4 )\n{\nmap $whiteimage\n}\n}";Check(!ParseShader(&text),"missing fog depth rejects");
}
int main(int argc,char **argv) {
	int i;char *text;ri.Printf=Print;ri.Hunk_Alloc=Allocate;ri.CIN_PlayCinematic=Video;ri.Error=Com_Error;tr.whiteImage=&white;
	for(i=0;i<MAX_SHADERTEXT_HASH;i++)shaderTextHashTable[i]=emptyHash;
	if(argc>1) {int proof=atoi(argv[1]);if(proof<3)ConstantVectors(proof);else if(proof<8)WaveModifiers(proof-3);else Metadata(proof-8);Release();return 0;}
	Metadata(-1);WaveModifiers(-1);ConstantVectors(-1);AlphaIdentity();AlphaWaves();FastAlpha();NativeStages();TailCases();
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
