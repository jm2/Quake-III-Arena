/* The whole engine, headless, on real game data: Sys_* for tests/real_content_boot.py.
 *
 * It stands where code/mac is in the Mac build, and with no display, sound
 * device or network it runs the portable engine only.  GL is a table of no-ops
 * (the runner generates it from qgl.h), so nothing is drawn, but everything
 * up to the driver runs: every map, model, shader, image, sound and bot file a
 * level loads, the static modules, and the module reset of issue #457, which
 * the Mac build does in Sys_LoadDll.
 *
 * HOST_FUZZ=<seed> in the environment injects seeded keys, characters and
 * mouse movement, which walks the menus.
 *
 * The demo pak is not one of the retail paks FS_IdPakGame knows.  With
 * REAL_CONTENT_STOCK_PAK that returns the base game for it, so the stock-QVM
 * choice in VM_Create, and the pure checks it feeds, run as on retail data.
 */
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <stdlib.h>
#include "../code/client/client.h"
#include "../code/renderer/tr_local.h"
#include "../code/client/snd_local.h"
extern int Game_vmMain(), CGame_vmMain(), UI_vmMain();
extern void UI_dllEntry(int (QDECL *)(int,...)), CGame_dllEntry(int (QDECL *)(int,...)), Game_dllEntry(int (QDECL *)(int,...));
static int fakeMs; static int useFake;
void Sys_Error(const char*e,...){va_list a;char t[4096];va_start(a,e);vsnprintf(t,sizeof t,e,a);va_end(a);fprintf(stderr,"Sys_Error: %s\n",t);exit(1);}
void Sys_Quit(void){printf("Sys_Quit: clean exit\n");fflush(stdout);exit(0);}
void Sys_Print(const char*m){fputs(m,stdout);fflush(stdout);}
void Sys_Init(void){useFake=1;}
void Sys_ShowConsole(int a,qboolean b){}
void Sys_BeginProfiling(void){}
void Sys_BeginStreamedFile(int h,int r){} void Sys_EndStreamedFile(int h){}
int Sys_StreamedRead(void*b,int s,int c,int h){return FS_Read(b,s*c,h);}
void Sys_StreamSeek(int h,int o,int w){FS_Seek(h,o,w);}
void Sys_ShowIP(void){} char*Sys_GetClipboardData(void){return NULL;} qboolean Sys_LowPhysicalMemory(void){return qfalse;}
qboolean Sys_CheckCD(void){return qtrue;}
void Sys_UnloadDll(void*h){}
void Sys_UnloadBotLib(void){}
#define M(x) extern char __start_moddata_##x[],__stop_moddata_##x[],__start_modbss_##x[],__stop_modbss_##x[]; static char *snap_##x;
M(game) M(cgame) M(ui)
/* whole sections, across the redzones ASan keeps between globals */
__attribute__((no_sanitize("address"))) static void RawCopy(char *d,const char *s,size_t n){volatile char *v=d;while(n--)*v++=*s++;}
__attribute__((no_sanitize("address"))) static void RawZero(char *d,size_t n){volatile char *v=d;while(n--)*v++=0;}
#define SNAP(x) do{size_t z=__stop_moddata_##x-__start_moddata_##x;snap_##x=malloc(z+1);RawCopy(snap_##x,__start_moddata_##x,z);}while(0)
#define RESET(x) do{RawCopy(__start_moddata_##x,snap_##x,__stop_moddata_##x-__start_moddata_##x);RawZero(__start_modbss_##x,__stop_modbss_##x-__start_modbss_##x);}while(0)
void*Sys_LoadDll(const char*n,char*fq,int(QDECL**e)(int,...),int(*s)(int,...)){
 if(!Q_stricmp(n,"ui"))RESET(ui); else if(!Q_stricmp(n,"cgame"))RESET(cgame); else RESET(game);
 if(!Q_stricmp(n,"ui")){*e=(void*)UI_vmMain;UI_dllEntry((void*)s);return (void*)1;}
 if(!Q_stricmp(n,"cgame")){*e=(void*)CGame_vmMain;CGame_dllEntry((void*)s);return (void*)2;}
 *e=(void*)Game_vmMain;Game_dllEntry((void*)s);return (void*)3;}
void VM_Compile(void*v,void*h){} int VM_CallCompiled(void*v,int*a){return 0;}
void NET_Sleep(int m){} void Sys_SendPacket(int l,const void*d,netadr_t t){} qboolean Sys_StringToAdr(const char*s,netadr_t*a){return qfalse;}
qboolean Sys_IsLANAddress(netadr_t a){return qtrue;}
void Sys_PumpEvents(void){}
static unsigned fz; static int fzOn,fzNext,fzUp=-1,fzCnt;
static int fzkeys[]={K_ENTER,K_ESCAPE,K_UPARROW,K_DOWNARROW,K_LEFTARROW,K_RIGHTARROW,K_MOUSE1,K_MOUSE1,K_MOUSE1,K_TAB,K_SPACE,'a','1'};
sysEvent_t Sys_GetEvent(void){sysEvent_t e;int now=Sys_Milliseconds();memset(&e,0,sizeof e);e.evTime=now;e.evType=SE_NONE;
 if(!fzNext){char*s=getenv("HOST_FUZZ");fzOn=s?1:0;fz=s?atoi(s):1;fzNext=now+3000;}
 if(fzOn&&now>=fzNext){fzNext=now+60;fz=fz*1103515245u+12345u;
  if(fzUp>=0){e.evType=SE_KEY;e.evValue=fzUp;e.evValue2=0;fzUp=-1;return e;}
  if((fz>>16)%3==0){e.evType=SE_MOUSE;e.evValue=(int)((fz>>4)%121)-60;e.evValue2=(int)((fz>>12)%91)-45;return e;}
  if((fz>>16)%3==1){int k=fzkeys[(fz>>20)%(sizeof fzkeys/sizeof fzkeys[0])];e.evType=SE_KEY;e.evValue=k;e.evValue2=1;fzUp=k;fzCnt++;return e;}
  e.evType=SE_CHAR;e.evValue='a'+((fz>>20)%26);return e;}
 return e;}
void IN_Init(void){} void IN_Frame(void){} void IN_Shutdown(void){} void IN_Restart(void){} void Sys_SendKeyEvents(void){}
void Sys_InitInput(void){}
/* renderer glue */
void GLimp_EndFrame(void){} void GLimp_Init(void){
 extern glconfig_t glConfig; static int d;
 memset(&glConfig,0,sizeof glConfig);
 glConfig.vidWidth=640;glConfig.vidHeight=480;glConfig.windowAspect=1.3333f;glConfig.colorBits=16;glConfig.depthBits=16;glConfig.stencilBits=0;glConfig.deviceSupportsGamma=qfalse;glConfig.isFullscreen=qtrue;
 strcpy(glConfig.vendor_string,"Stub");strcpy(glConfig.renderer_string,"Stub");strcpy(glConfig.version_string,"1.2");strcpy(glConfig.extensions_string,"GL_EXT_texture_env_add");
 glConfig.textureEnvAddAvailable=qtrue;
}
void GLimp_Shutdown(void){ memset(&glConfig,0,sizeof glConfig);} void GLimp_LogComment(char*c){} void GLimp_SetGamma(unsigned char*r,unsigned char*g,unsigned char*b){}
qboolean GLimp_SpawnRenderThread(void(*f)(void)){return qfalse;} void*GLimp_RendererSleep(void){return NULL;} void GLimp_FrontEndSleep(void){} void GLimp_WakeRenderer(void*d){}
void GLimp_EnableLogging(qboolean e){} qboolean QGL_Init(const char*d){return qtrue;} void QGL_Shutdown(void){}
/* DMA */
static short ring[0x8000]; 
qboolean SNDDMA_Init(void){dma.channels=2;dma.samples=0x8000;dma.submission_chunk=0x100;dma.samplebits=16;dma.speed=22050;dma.buffer=(byte*)ring;return qtrue;}
int SNDDMA_GetDMAPos(void){return (Sys_Milliseconds()*44)&0x7ff0;}
void SNDDMA_Shutdown(void){} void SNDDMA_BeginPainting(void){} void SNDDMA_Submit(void){}
void Sys_ConsoleInputShutdown(void){}
int main(int c,char**v){static char l[8192];int i;SNAP(game);SNAP(cgame);SNAP(ui);for(i=1;i<c;i++){strcat(l,v[i]);strcat(l," ");}
 Com_Init(l);
 for(;;)Com_Frame();}
void Com_Memcpy(void*d,const void*s,const size_t c){memcpy(d,s,c);}
void Com_Memset(void*d,const int v,const size_t c){memset(d,v,c);}
void Sys_SnapVector(float*v){v[0]=rint(v[0]);v[1]=rint(v[1]);v[2]=rint(v[2]);}
int in_subframe;
void Debug_Breadcrumb(int c){} unsigned Sys_Entropy(void){return 12345;}
void Snd_Memset(void*d,const int v,const size_t c){memset(d,v,c);}
extern int *snd_p,snd_linear_count; extern short *snd_out;
void S_WriteLinearBlastStereo16(void){int i,v;for(i=0;i<snd_linear_count;i+=2){v=snd_p[i]>>8;snd_out[i]=v>32767?32767:v<-32768?-32768:v;v=snd_p[i+1]>>8;snd_out[i+1]=v>32767?32767:v<-32768?-32768:v;}}

#ifdef REAL_CONTENT_STOCK_PAK
const char *FS_IdPakGame( int checksum ) { return BASEGAME; }
#endif
