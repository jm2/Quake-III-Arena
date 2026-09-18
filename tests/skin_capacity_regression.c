/* Issue #46: actual native skin parsing, capacity, allocation and file ownership. */
#include "renderer_image_gl_stub.h"
#include "../code/renderer/tr_image.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
trGlobals_t tr;
glconfig_t glConfig;
refimport_t ri;
static shader_t material;
static char input[16000],shaderNames[MD3_MAX_SURFACES][MAX_TOKEN_CHARS];
static void *owned[1024],*file;
static int sizes[1024],allocations,reads,frees,shaderCalls,missing;
static void Check(int condition,const char *message) { if(!condition) { fprintf(stderr,"Skin regression failed: %s\n",message);exit(1); } }
void QDECL Com_Error(int level,const char *format,...) { (void)level;(void)format;Check(0,"unexpected native error");exit(1); }
void QDECL Com_Printf(const char *format,...) { (void)format; }
static void QDECL Print(int level,const char *format,...) { (void)level;(void)format; }
static void *Allocate(int size,ha_pref preference) { void *p;Check(size>0 && size<100000 && preference==h_low && allocations<1024,"bounded native skin hunk allocation");p=calloc(1,size);Check(p!=NULL,"exact fixture allocation");sizes[allocations]=size;owned[allocations++]=p;return p; }
static int Read(const char *name,void **buffer) { int size=strlen(input);Check(name && !file,"native skin input ownership");reads++;if(missing) { *buffer=NULL;return -1; }file=malloc(size+1);Check(file!=NULL,"exact NUL-terminated FS input");memcpy(file,input,size+1);*buffer=file;return size; }
static void FreeFile(void *pointer) { Check(pointer && pointer==file,"native skin file release");free(file);file=NULL;frees++; }
shader_t *R_FindShader(const char *name,int lightmap,qboolean mipmap) { Check(name && lightmap==LIGHTMAP_NONE && mipmap && shaderCalls<MD3_MAX_SURFACES,"bounded native skin shader import");Q_strncpyz(shaderNames[shaderCalls++],name,MAX_TOKEN_CHARS);return &material; }
void R_SyncRenderThread(void) {}
#ifndef Com_Memset
void Com_Memset(void *destination,int value,size_t size) { memset(destination,value,size); }
#endif
#ifndef Com_Memcpy
void Com_Memcpy(void *destination,const void *source,size_t size) { memcpy(destination,source,size); }
#endif
static void Release(void) { Check(!file,"no retained native skin file");while(allocations)free(owned[--allocations]); }
static void Reset(void) {
	Release();memset(&tr,0,sizeof(tr));tr.defaultShader=&material;missing=reads=frees=shaderCalls=0;input[0]=0;R_InitSkins();
	Check(tr.numSkins==1 && allocations==2 && sizes[0]==sizeof(skin_t) && sizes[1]==sizeof(skinSurface_t),"default skin owns complete surface storage");
	Check(tr.skins[0]->numSurfaces==1 && tr.skins[0]->surfaces[0]->shader==&material,"native default skin shader");
	Check(R_GetSkinByHandle(-1)==tr.skins[0] && R_GetSkinByHandle(MAX_SKINS)==tr.skins[0],"invalid handles select native default");
}
static void Build(int count) { int i;char line[100];input[0]=0;for(i=0;i<count;i++) { snprintf(line,sizeof(line),"Surface%d,textures/test%d\n",i,i);strcat(input,line); } }
static void Cache(const char *name,qhandle_t expected) {
	int before=allocations,beforeReads=reads,beforeFrees=frees,beforeShaders=shaderCalls;
	Check(RE_RegisterSkin(name)==expected,"native valid/default cache reuse");
	Check(allocations==before && reads==beforeReads && frees==beforeFrees && shaderCalls==beforeShaders,"cached skin has no allocation/file/shader imports");
}
static void PlainNames(void) {
	int length;char *name;char longName[MAX_QPATH+1];qhandle_t handle;
	Reset();memset(longName,'a',MAX_QPATH);longName[MAX_QPATH]=0;
	Check(!RE_RegisterSkin(NULL) && !RE_RegisterSkin("") && !RE_RegisterSkin(longName) && allocations==2 && tr.numSkins==1,"empty/overlong names have no side effects");
	for(length=1;length<MAX_QPATH;length++) {
		Reset();name=malloc(length+1);Check(name!=NULL,"exact short name allocation");memset(name,'a',length);name[length]=0;
		handle=RE_RegisterSkin(name);Check(handle==1 && allocations==4 && sizes[3]==sizeof(skinSurface_t),"all native plain-name lengths allocate a complete surface");
		Check(shaderCalls==1 && !strcmp(shaderNames[0],name) && !reads && !frees,"plain names import their native shader directly");
		Cache(name,handle);free(name);
	}
	Reset();Check(RE_RegisterSkin("case.SKIN")==1 && !reads,"native extension comparison remains case-sensitive");
}
static void Surfaces(void) {
	int count,i;char expected[MAX_QPATH];qhandle_t handle;
	for(count=0;count<=34;count++) {
		Reset();Build(count);handle=RE_RegisterSkin("limits.skin");
		Check(handle==(count>0 && count<=32?1:0) && tr.numSkins==2,"native surface cap/default result");
		Check(reads==1 && frees==1 && !file && shaderCalls==(count<32?count:32),"bounded shader imports and balanced input ownership");
		Check(allocations==3+(count<32?count:32),"no allocation for excess surface");
		Check(tr.skins[1]->numSurfaces==(handle?count:0),"rejected skin caches native fallback");
		for(i=0;i<(count<32?count:32);i++) {
			snprintf(expected,sizeof(expected),"surface%d",i);Check(!strcmp(tr.skins[1]->surfaces[i]->name,expected) && sizes[3+i]==sizeof(skinSurface_t),"native lowercase surface and exact full allocation");
			snprintf(expected,sizeof(expected),"textures/test%d",i);Check(!strcmp(shaderNames[i],expected) && tr.skins[1]->surfaces[i]->shader==&material,"native surface-to-shader order");
		}
		Cache("LIMITS.SKIN",handle);
	}
	Reset();strcpy(input,"// leading\n/* block */\ntag_weapon,\nfoo_tag_legacy,\n\"UpPeR\",\"textures/comma,name\"\n// trailing");
	Check(RE_RegisterSkin("quoted.skin")==1 && shaderCalls==1 && !strcmp(tr.skins[1]->surfaces[0]->name,"upper") && !strcmp(shaderNames[0],"textures/comma,name") && frees==1,"native tags, quoted commas, comments and lowercase names");
	Reset();strcpy(input,"first,\"\"\nsecond,textures/a\n");Check(RE_RegisterSkin("empty-map.skin")==1 && shaderCalls==2 && !shaderNames[0][0] && !strcmp(shaderNames[1],"textures/a") && tr.skins[1]->surfaces[0]->shader==tr.defaultShader && frees==1,"explicit empty quoted shader maps to native default without discarding following surfaces");Cache("empty-map.skin",1);
	Reset();strcpy(input,"surface,\"\"");Check(RE_RegisterSkin("empty-map-eof.skin")==1 && shaderCalls==1 && !shaderNames[0][0] && frees==1,"explicit empty quoted shader remains valid at EOF");
	Reset();Build(32);strcat(input,"tag_extra,\n");Check(RE_RegisterSkin("tags.skin")==1 && shaderCalls==32,"ignored tag after full native surface array");
	Reset();missing=1;Check(!RE_RegisterSkin("missing.skin") && reads==1 && !frees && !file,"missing file keeps balanced ownership");Cache("missing.skin",0);
	Reset();strcpy(input," \n// empty\n/* empty */");Check(!RE_RegisterSkin("empty.skin") && frees==1,"empty skin uses default");
}
static void Tokens(void) {
	int length,quoted,field;char *text,*token;char *buffer; qboolean invalid;
	for(quoted=0;quoted<=1;quoted++)for(length=1022;length<=1025;length++) {
		buffer=malloc(length+quoted*2+1);Check(buffer!=NULL,"exact token input");
		if(quoted)buffer[0]='"';memset(buffer+quoted,'a',length);if(quoted)buffer[length+1]='"';buffer[length+quoted*2]=0;text=buffer;invalid=qfalse;
		token=CommaParse(&text,&invalid);Check(invalid==(length>=1024),"quoted/unquoted token bounds reserve the terminator");
		if(length<1024) { Check(strlen(token)==length && text==buffer+length+quoted*2,"native maximum valid token content and position");Check(!CommaParse(&text,&invalid)[0] && !text && !invalid,"token EOF never advances beyond input"); }
		else Check(!token[0] && !text,"overlong token is rejected safely");free(buffer);
		for(field=0;field<=1;field++) {
			Reset();Build(1);text=input+strlen(input);if(field)strcpy(text,"next,");text+=strlen(text);
			if(quoted)*text++='"';memset(text,'a',length);text+=length;if(quoted)*text++='"';strcpy(text,field?"\n":",textures/next\n");
			Check(RE_RegisterSkin("tokens.skin")== (length<1024?1:0) && reads==1 && frees==1 && !file,"whole skin rejects overlong surface/shader token and frees input");
			Check(shaderCalls==(length<1024?2:1),"invalid token performs no additional shader import");Cache("tokens.skin",length<1024?1:0);
		}
	}
}
static void Truncated(void) {
	const char *tails[]={"\"unterminated", "next,\"unterminated", "next,", "/* unterminated", "next,/* unterminated"};
	const char *corpus="/* header */\ntag_weapon,\n\"UpPeR\",\"textures/a\"\nLower,textures/b\n// tail\n";
	int mode;size_t prefix; qhandle_t result;
	for(mode=0;mode<5;mode++) { Reset();Build(1);strcat(input,tails[mode]);Check(!RE_RegisterSkin("truncated.skin") && tr.skins[1]->numSurfaces==0 && shaderCalls==1 && frees==1 && !file,"truncated quote/pair/comment uses cached fallback with balanced ownership");Cache("truncated.skin",0); }
	for(prefix=0;prefix<=strlen(corpus);prefix++) {
		Reset();memcpy(input,corpus,prefix);input[prefix]=0;result=RE_RegisterSkin("prefix.skin");
		Check((result==0 || result==1) && shaderCalls<=2 && frees==1 && !file && tr.skins[1]->numSurfaces<=2,"every exact input prefix stays bounded and frees its file");Cache("prefix.skin",result);
	}
	Reset();strcpy(input,corpus);Check(RE_RegisterSkin("complete.skin")==1 && shaderCalls==2 && !strcmp(shaderNames[0],"textures/a") && !strcmp(shaderNames[1],"textures/b"),"complete legacy corpus keeps valid native assignments");
}
static void FullCache(void) {
	int i,before;Reset();Check(RE_RegisterSkin("existing")==1,"existing native cached shader");
	for(i=2;i<MAX_SKINS;i++)tr.skins[i]=tr.skins[1];tr.numSkins=MAX_SKINS;before=allocations;
	Check(!RE_RegisterSkin("new") && allocations==before && shaderCalls==1 && !reads,"full skin cache rejects without imports");Cache("existing",1);
}
int main(void) {
	ri.Printf=Print;ri.Hunk_Alloc=Allocate;ri.FS_ReadFile=Read;ri.FS_FreeFile=FreeFile;
	PlainNames();Surfaces();Tokens();Truncated();FullCache();Release();
	puts("Native skin allocation, names, 0-34 surfaces, tokens, truncated prefixes and ownership/cache checks passed (issue #46)");return 0;
}
