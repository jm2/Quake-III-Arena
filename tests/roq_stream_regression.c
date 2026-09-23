/* Issue #41: execute actual cinematic open/run/stop and bounded chunk/audio paths. */
#include "../code/client/cl_cin.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

static byte *source;
static int sourceSize, advertisedSize, position, readLimit = INT_MAX;
static int opens, closes, starts, ends, rawCalls, rawSamples, updates, failReopen, soundStops;
static const char *commandOption = "";
static cvar_t timescale, video;
cvar_t *com_timescale = &timescale, *cl_inGameVideo = &video;
clientStatic_t cls;
glconfig_t glConfig;
vm_t *uivm;
int s_paintedtime, s_rawend, s_soundtime;

/** Stop on any unexpected resource, sample, or playback behavior. */
static void Check( int ok, const char *message ) {
	if ( !ok ) { fprintf(stderr,"RoQ stream regression failed: %s\n",message); exit(1); }
}
void QDECL Com_Error( int level, const char *format, ... ) { (void)level; (void)format; Check(0,"engine error"); }
void QDECL Com_Printf( const char *format, ... ) { (void)format; }
void QDECL Com_DPrintf( const char *format, ... ) { (void)format; }
void Com_Memset( void *out, int value, size_t size ) { memset(out,value,size); }
void Com_Memcpy( void *out, const void *in, size_t size ) { memcpy(out,in,size); }
int CL_ScaledMilliseconds( void ) { return 100; }
void Con_Close( void ) {}
void S_StopAllSounds( void ) { soundStops++; }
char *Cmd_Argv( int argument ) { return argument==1 ? "test.roq" : argument==2 ? (char *)commandOption : ""; }
char *Cvar_VariableString( const char *name ) { (void)name; return ""; }
void Cvar_Set( const char *name, const char *value ) { (void)name; (void)value; }
void Cbuf_ExecuteText( int when, const char *text ) { (void)when; (void)text; Check(0,"nextmap side effect"); }
#ifdef VM_Call
int VM_CallArgs( vm_t *vm, int command, const int *args, int count ) { (void)vm; (void)command; (void)args; (void)count; Check(0,"UI callback"); return 0; }
#else
int QDECL VM_Call( vm_t *vm, int command, ... ) { (void)vm; (void)command; Check(0,"UI callback"); return 0; }
#endif

/** Model a file whose stat length may exceed readable bytes, using exact source allocations. */
int FS_FOpenFileRead( const char *name, fileHandle_t *handle, qboolean unique ) {
	(void)name; (void)unique; opens++; position=0;
	if(opens>1 && failReopen==1) { *handle=0; return -1; }
	if(opens>1 && failReopen==2) readLimit=15;
	*handle=1; return advertisedSize;
}
void FS_FCloseFile( fileHandle_t handle ) { Check(handle==1,"close handle"); closes++; }
int FS_Read( void *out, int count, fileHandle_t handle ) {
	int available=sourceSize-position;
	Check(handle==1 && count>=0,"read request");
	if(count>available) count=available;
	if(count>readLimit) count=readLimit;
	memcpy(out,source+position,count); position+=count; return count;
}
void Sys_BeginStreamedFile( fileHandle_t handle, int ahead ) { Check(handle==1 && ahead==65536,"stream start"); starts++; }
void Sys_EndStreamedFile( fileHandle_t handle ) { Check(handle==1,"stream end"); ends++; }
int Sys_StreamedRead( void *out, int size, int count, fileHandle_t handle ) {
	Check(size==1 && count>0 && count<=65528,"bounded byte stream read");
	return FS_Read(out,count,handle)/size;
}
void S_Update( void ) { updates++; }
/** Read every submitted native sample so ASan checks the complete decoded output. */
void S_RawSamples( int samples, int rate, int width, int channels, const byte *data, float volume ) {
	const short *decoded=(const short *)data;
	int i;
	Check(samples>0 && rate==22050 && width==2 && (channels==1 || channels==2) && volume==1.0f,"audio format");
	for(i=0;i<samples*channels;i++) {
		/* Input delta 1 increments both channels. Preserve the existing mono submission format. */
		Check(decoded[i]==(short)(i/2+1),"audio sample changed");
	}
	rawCalls++; rawSamples+=samples;
}

/** Encode a complete little-endian chunk header, including its high size byte. */
static void Header( byte *out, unsigned int id, unsigned int size, unsigned int flags ) {
	int i; out[0]=id; out[1]=id>>8;
	for(i=0;i<4;i++) out[i+2]=size>>(i*8);
	out[6]=flags; out[7]=flags>>8;
}
/** Reset all playback state and allocate exactly the readable file size. */
static void Fixture( unsigned int id, unsigned int payload, unsigned int flags, int readable ) {
	free(source); source=malloc(readable ? readable : 1); Check(source!=NULL,"source allocation");
	sourceSize=advertisedSize=readable; position=0; readLimit=INT_MAX;
	opens=closes=starts=ends=rawCalls=rawSamples=updates=failReopen=soundStops=0;
	memset(&cls,0,sizeof(cls)); commandOption="";
	memset(&cin,0,sizeof(cin)); memset(cinTable,0,sizeof(cinTable));
	currentHandle=CL_handle=-1;
	memset(source,1,readable);
	if(readable>=16) { Header(source,0x1084,UINT_MAX,30); Header(source+8,id,payload,flags); }
}
/** Reject malformed initial reads with no stream or resource left open. */
static void RejectOpen( void ) {
	Check(CIN_PlayCinematic("test.roq",0,0,32,32,CIN_loop)==-1,"invalid open accepted");
	Check(opens==1 && closes==1 && !starts && !ends && !rawCalls && currentHandle==-1 && !cinTable[0].fileName[0],"invalid open leaked state");
}
/** Run through EOF/failure and prove cleanup works before any frame buffer exists. */
static void RunToClose( int bits, int expectedCalls, int expectedSamples ) {
	int handle=CIN_PlayCinematic("test.roq",0,0,32,32,bits);
	Check(handle==0,"valid open rejected");
	Check(CIN_RunCinematic(handle)==FMV_EOF,"playback did not stop");
	Check(rawCalls==expectedCalls && rawSamples==expectedSamples,"audio dispatch count");
	Check(opens==1 && closes==1 && starts==1 && ends==1 && currentHandle==-1 && !cinTable[handle].fileName[0],"stop resource ownership");
	Check(CIN_RunCinematic(handle)==FMV_EOF && CIN_StopCinematic(handle)==FMV_EOF && closes==1,"retired handle reused");
}

/** Cover file accounting, capacity thresholds, stereo pairing, packets, and early cleanup. */
int main( void ) {
	int i, handle;
	unsigned int sizes[]={16385,32769,65529,65536};
	timescale.value=1; video.integer=1;
	for(i=0;i<16;i++) { Fixture(ROQ_QUAD_HANG,0,0,i); RejectOpen(); }
	for(i=0;i<16;i++) { Fixture(ZA_SOUND_MONO,2,0,18); readLimit=i; RejectOpen(); }
	Fixture(ROQ_QUAD_HANG,0,0,16); source[0]=0; RejectOpen();
	Fixture(ROQ_QUAD_HANG,0x01000000,0,16); RejectOpen();
	Fixture(ROQ_QUAD_HANG,UINT_MAX,0,16); RejectOpen();
	Fixture(0x1084,0,0,16); RejectOpen();
	for(i=0;i<4;i++) {
		Fixture(ZA_SOUND_MONO,sizes[i],0,16+sizes[i]);
		if(sizes[i]>65528) RejectOpen(); else RunToClose(CIN_loop,0,0);
		Fixture(ZA_SOUND_STEREO,sizes[i],0,16+sizes[i]);
		if(sizes[i]>65528) RejectOpen(); else RunToClose(CIN_loop|CIN_silent,0,0);
	}
	Fixture(ZA_SOUND_MONO,16384,0,16400); RunToClose(0,1,16384);
	Fixture(ZA_SOUND_STEREO,32768,0,32784); RunToClose(0,1,16384);
	Fixture(ZA_SOUND_STEREO,32770,0,32786); RunToClose(CIN_loop,0,0);
	Fixture(ZA_SOUND_STEREO,1,0,17); RunToClose(CIN_loop,0,0);
	Fixture(ROQ_QUAD_HANG,65528,0,65544); RunToClose(0,0,0);
	Fixture(ROQ_QUAD_HANG,0,0,16); RunToClose(0,0,0);
	/* Every payload truncation after a correct stat/header must stop before audio dispatch. */
	for(i=0;i<4;i++) { Fixture(ZA_SOUND_MONO,4,0,16+i); advertisedSize=20; RunToClose(CIN_loop,0,0); }
	for(i=1;i<8;i++) { Fixture(ROQ_QUAD_HANG,0,0,16+i); RunToClose(CIN_loop,0,0); }
	/* A second on-disk header is read independently, and its final payload is decoded. */
	Fixture(ROQ_QUAD_HANG,0,0,28); Header(source+16,ZA_SOUND_MONO,4,0); RunToClose(0,1,4);
	/* Declared full headers that become short after stat are rejected as well. */
	for(i=0;i<8;i++) { Fixture(ROQ_QUAD_HANG,0,0,16+i); advertisedSize=24; RunToClose(CIN_loop,0,0); }
	/* A malformed embedded header is rejected before the earlier valid audio child runs. */
	Fixture(ROQ_PACKET,22,2,38); Header(source+16,ZA_SOUND_MONO,4,0); Header(source+28,ZA_SOUND_STEREO,2,0); RunToClose(0,2,5);
	Fixture(ROQ_PACKET,22,2,38); Header(source+16,ZA_SOUND_MONO,4,0); Header(source+28,ZA_SOUND_STEREO,65536,0); RunToClose(CIN_loop,0,0);
	Fixture(ROQ_PACKET,8,2,24); Header(source+16,ROQ_QUAD_HANG,0,0); RunToClose(CIN_loop,0,0);
	Fixture(ROQ_PACKET,9,1,25); Header(source+16,ROQ_QUAD_HANG,0,0); RunToClose(CIN_loop,0,0);
	Fixture(ROQ_PACKET,16,1,32); Header(source+16,ROQ_PACKET,8,1); Header(source+24,ROQ_QUAD_HANG,0,0); RunToClose(0,0,0);
	Fixture(ROQ_PACKET,8*(ROQ_MAX_PACKET_DEPTH+1),1,16+8*(ROQ_MAX_PACKET_DEPTH+1));
	for(i=0;i<=ROQ_MAX_PACKET_DEPTH;i++) Header(source+16+8*i,i==ROQ_MAX_PACKET_DEPTH?ROQ_QUAD_HANG:ROQ_PACKET,8*(ROQ_MAX_PACKET_DEPTH-i),1);
	RunToClose(CIN_loop,0,0);
	for(i=0;i<10;i++) { Fixture(ROQ_CODEBOOK,i,0x0101,16+i); RunToClose(CIN_loop,0,0); }
	Fixture(ROQ_CODEBOOK,10,0x0101,26); memset(source+16,0,10); source[20]=source[21]=128; RunToClose(0,0,0);
	Check(((byte *)vq2)[0]==1 && ((byte *)vq2)[1]==0 && ((byte *)vq2)[2]==1 && ((byte *)vq2)[3]==255,"neutral codebook pixel");
	/* Issue #247: retail idlogo.RoQ's 1,536-byte argument-0 codebook holds only 2x2 entries and must keep playing. */
	Fixture(ROQ_CODEBOOK,1536,0,16+1536); handle=CIN_PlayCinematic("test.roq",0,0,32,32,CIN_loop);
	Check(handle==0 && CIN_RunCinematic(handle)==FMV_PLAY && opens==2 && closes==1 && starts==2 && ends==1,"argument-0 2x2-only codebook stopped playback");
	Check(CIN_StopCinematic(handle)==FMV_EOF && closes==2 && ends==2,"argument-0 codebook stop");
	Fixture(ROQ_CODEBOOK,1535,0,16+1535); RunToClose(CIN_loop,0,0);
	for(i=0;i<8;i++) { Fixture(ROQ_QUAD_INFO,i,0,16+i); RunToClose(CIN_loop,0,0); }
	/* Hold and explicit stop also release a movie that produced no image. */
	Fixture(ROQ_QUAD_HANG,0,0,16); handle=CIN_PlayCinematic("test.roq",0,0,32,32,CIN_hold);
	Check(handle==0 && CIN_RunCinematic(handle)==FMV_IDLE && !closes,"hold state");
	Check(CIN_StopCinematic(handle)==FMV_EOF && closes==1 && ends==1,"held movie stop");
	Fixture(ROQ_QUAD_HANG,0,0,16); handle=CIN_PlayCinematic("test.roq",0,0,32,32,0);
	Check(handle==0 && CIN_StopCinematic(handle)==FMV_EOF && closes==1 && ends==1,"stop before first frame");
	/* A valid loop reopens once and can then be stopped. */
	Fixture(ZA_SOUND_MONO,2,0,18); handle=CIN_PlayCinematic("test.roq",0,0,32,32,CIN_loop);
	Check(handle==0 && CIN_RunCinematic(handle)==FMV_PLAY && opens==2 && closes==1 && starts==2 && ends==1 && rawCalls==1,"valid loop");
	Check(CIN_StopCinematic(handle)==FMV_EOF && closes==2 && ends==2,"loop stop");
	for(i=1;i<=2;i++) {
		Fixture(ZA_SOUND_MONO,2,0,18); failReopen=i;
		handle=CIN_PlayCinematic("test.roq",0,0,32,32,CIN_loop);
		Check(handle==0 && CIN_RunCinematic(handle)==FMV_EOF && opens==2 && closes==i && starts==1 && ends==1 && currentHandle==-1 && rawCalls==1,"failed loop reopen cleanup");
	}
	/* The actual console command must leave its wait loop after a pre-frame shutdown. */
	for(i=0;i<3;i++) {
		Fixture(ROQ_QUAD_INFO,8,0,16); advertisedSize=24; commandOption=i==1?"1":i==2?"2":"";
		CL_PlayCinematic_f();
		Check(currentHandle==-1 && CL_handle==-1 && cls.state==CA_DISCONNECTED && closes==1 && starts==1 && ends==1 && soundStops==1 && !cinTable[0].buf && !cinTable[0].fileName[0],"console wait loop pre-frame shutdown");
	}
	free(source); puts("RoQ file, packet, and audio regressions passed (issue #41)"); return 0;
}
