/* Real permanent allocator: cacheline costs, both banks and failure ownership. */
#include <limits.h>
#include <stdint.h>
#include <setjmp.h>
void Hunk_SmallLog(void);
#include Q3_HUNK_COMMON_SOURCE
static unsigned char arena[1088] __attribute__((aligned(32)));
static jmp_buf failure;
static int expecting, errors;
static void Check(int condition,const char *message) { if(!condition) { fprintf(stderr,"Hunk regression failed: %s\n",message);exit(1); } }
void QDECL Com_Error(int level,const char *format,...) { (void)format;Check(expecting && level==ERR_DROP,"controlled allocation rejection");errors++;longjmp(failure,1); }
void Hunk_Log(void) {}
void Hunk_SmallLog(void) {}
static void Reset(int high) {
	memset(arena,0xa5,sizeof(arena));s_hunkData=arena+32;s_hunkTotal=1024;
	memset(&hunk_low,0,sizeof(hunk_low));memset(&hunk_high,0,sizeof(hunk_high));
	hunk_permanent=high?&hunk_high:&hunk_low;hunk_temp=high?&hunk_low:&hunk_high;hunkblocks=NULL;
}
static int Header(void) {
#ifdef HUNK_DEBUG
	return sizeof(hunkblock_t);
#else
	return 0;
#endif
}
static void Guards(void) { int i;for(i=0;i<32;i++)Check(arena[i]==0xa5 && arena[1056+i]==0xa5,"arena guard bytes"); }
static void Reject(int size) {
	hunkUsed_t low=hunk_low,high=hunk_high;hunkUsed_t *permanent=hunk_permanent,*temporary=hunk_temp;
	hunkblock_t *blocks=hunkblocks;unsigned char before[sizeof(arena)];int count=errors;
	memcpy(before,arena,sizeof(arena));expecting=1;
	if(!setjmp(failure)) { Hunk_Alloc(size,h_dontcare);Check(0,"oversized allocation accepted"); }
	expecting=0;Check(errors==count+1,"one recoverable drop");
	Check(!memcmp(&low,&hunk_low,sizeof(low)) && !memcmp(&high,&hunk_high,sizeof(high)) && hunk_permanent==permanent && hunk_temp==temporary && hunkblocks==blocks,"failure retains both banks and debug ownership");
	Check(!memcmp(before,arena,sizeof(arena)),"failure retains arena contents");
}
void Com_Memset(void *destination,const int value,const size_t size) { memset(destination,value,size); }
int main(void) {
	int high,size,cost,before,i;void *memory;int boundaries[]={INT_MIN,-1,INT_MAX,INT_MAX-1,INT_MAX-30};
	for(high=0;high<2;high++)for(size=0;size<=257;size++) {
		Reset(high);cost=(int)(((int64_t)size+Header()+31)/32*32);before=Hunk_MemoryRemaining();
		Check(Hunk_AllocationSize(size)==cost,"preview equals independent cacheline cost");memory=Hunk_Alloc(size,high?h_high:h_low);
		Check(before-Hunk_MemoryRemaining()==cost,"actual native allocation uses preview cost");
		Check((byte *)memory==s_hunkData+(high?1024-cost:0)+Header(),"native bank address and debug header");
		for(i=0;i<size;i++)Check(((byte *)memory)[i]==0,"requested payload is zeroed");Guards();
	}
	for(high=0;high<2;high++) {
		Reset(high);hunk_low.permanent=hunk_low.temp=64;hunk_high.permanent=64;hunk_high.temp=128;
		before=Hunk_MemoryRemaining();Check(before==832,"live temporary memory reduces available capacity");
		Reject(before+1);for(i=0;i<(int)(sizeof(boundaries)/sizeof(boundaries[0]));i++)Reject(boundaries[i]);
		Reset(high);size=1024-Header();Check(Hunk_AllocationSize(size)==1024,"exact-fit payload accounts for debug header");Hunk_Alloc(size,high?h_high:h_low);Check(Hunk_MemoryRemaining()==0,"exact capacity is usable");Reject(1);Guards();
		Reset(high);memory=Hunk_Alloc(65,high?h_high:h_low);memset(memory,0x3c,65);hunk_temp->tempHighwater=256;Reject(INT_MIN);Reject(1024);Guards();
		Reset(high);hunk_low.temp=hunk_low.permanent=INT_MAX-64;s_hunkTotal=INT_MAX;Reject(96);
	}
	for(size=0;size<=63;size++) { int request=INT_MAX-size;int64_t expected=((int64_t)request+Header()+31)/32*32;Check(Hunk_AllocationSize(request)==(expected>INT_MAX?-1:(int)expected),"signed maximum alignment and debug boundaries"); }
	Check(Hunk_AllocationSize(-1)==-1 && Hunk_AllocationSize(INT_MIN)==-1,"negative previews reject");
	printf("Native hunk allocation checks passed (%s).\n",Header()?"debug":"release");return 0;
}
