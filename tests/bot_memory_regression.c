/* Issues #47/#48: actual bot allocator adapters with bounded arena callbacks. */
#include "../code/botlib/l_memory.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
botlib_import_t botimport;
static void *hunk[64];static int hunkCount,requests,frees,failRequest,lastRequest;
static void Check(int condition,const char *message) {if(!condition){fprintf(stderr,"Bot memory regression failed: %s\n",message);exit(1);}}
#ifndef Com_Memset
void Com_Memset(void *out,int value,size_t n) {memset(out,value,n);}
#endif
static void QDECL Print(int type,char *format,...) {(void)type;(void)format;}
static void *Allocate(int size,int fromHunk) {void *p;requests++;lastRequest=size;if(requests==failRequest)return NULL;Check(size>=0&&size<=1024,"signed bounded import allocation contract");p=malloc(size?size:1);Check(p!=NULL,"fixture allocation");memset(p,0xa5,size);if(fromHunk){Check(hunkCount<64,"bounded physical arena model");hunk[hunkCount++]=p;}return p;}
static void *Heap(int size) {return Allocate(size,0);}
static void *Hunk(int size) {return Allocate(size,1);}
static void Release(void *p) {Check(p!=NULL,"native heap release nonnull");frees++;free(p);}
#ifdef MEMDEBUG
#define RAW(size) GetMemoryDebug(size,"fixture",__FILE__,__LINE__)
#define CLEAR(size) GetClearedMemoryDebug(size,"fixture",__FILE__,__LINE__)
#define HRAW(size) GetHunkMemoryDebug(size,"fixture",__FILE__,__LINE__)
#define HCLEAR(size) GetClearedHunkMemoryDebug(size,"fixture",__FILE__,__LINE__)
#else
#define RAW(size) GetMemory(size)
#define CLEAR(size) GetClearedMemory(size)
#define HRAW(size) GetHunkMemory(size)
#define HCLEAR(size) GetClearedHunkMemory(size)
#endif
static void *Call(int kind,unsigned long size) {switch(kind){case 0:return RAW(size);case 1:return CLEAR(size);case 2:return HRAW(size);default:return HCLEAR(size);}}
static void EmptyTracking(void) {
#ifdef MEMORYMANEGER
 Check(!memory&&!allocatedmemory&&!totalmemorysize&&!numblocks,"failed/released allocation leaves all tracked ownership and counters empty");
#endif
}
static void Reset(void) {while(hunkCount)free(hunk[--hunkCount]);requests=frees=failRequest=lastRequest=0;EmptyTracking();}
static void Successes(void) {int kind;unsigned long size;for(kind=0;kind<4;kind++)for(size=0;size<=65;size++){unsigned char *p;unsigned long i;Reset();p=Call(kind,size);Check(p!=NULL&&requests==1&&lastRequest>(int)size,"native zero-to-65-byte payload plus ownership header");for(i=0;i<size;i++)Check(p[i]==((kind&1)?0:0xa5),"native raw/cleared payload behavior retained");memset(p,0x5a,size);FreeMemory(p);Check(frees==(kind<2),"heap physically releases and hunk retains arena bytes");EmptyTracking();}Reset();}
static void Failures(void) {int kind;const unsigned long invalid[]={ULONG_MAX,(unsigned long)INT_MAX,(unsigned long)INT_MAX+1ul};size_t i;for(kind=0;kind<4;kind++){Reset();failRequest=1;Check(Call(kind,31)==NULL&&requests==1&&!hunkCount&&!frees,"all nullable imports propagate failure without writes/ownership");EmptyTracking();for(i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++){Reset();Check(Call(kind,invalid[i])==NULL&&!requests&&!frees&&!hunkCount,"unsigned/prefix/signed import overflow rejects before allocation");EmptyTracking();}}Reset();FreeMemory(NULL);Check(!frees&&!requests,"nullable cleanup has no arena effects");EmptyTracking();}
#ifdef MEMORYMANEGER
static void AccountingFailures(void) {int kind;for(kind=0;kind<4;kind++){Reset();allocatedmemory=INT_MAX;Check(Call(kind,16)==NULL&&!requests,"tracked allocation counter overflow rejects");allocatedmemory=0;totalmemorysize=INT_MAX;Check(Call(kind,16)==NULL&&!requests,"tracked total counter overflow rejects");totalmemorysize=0;numblocks=INT_MAX;Check(Call(kind,16)==NULL&&!requests,"tracked block counter overflow rejects");numblocks=0;}Reset();}
#endif
int main(int argc,char **argv) {int proof=argc>1?atoi(argv[1]):-1;botimport.GetMemory=Heap;botimport.HunkAlloc=Hunk;botimport.FreeMemory=Release;botimport.Print=Print;
 if(proof==0){failRequest=1;Check(CLEAR(31)==NULL,"cleared heap import failure");}
 else if(proof==1){failRequest=1;Check(HCLEAR(31)==NULL,"cleared hunk import failure");}
 else if(proof==2){Check(RAW(ULONG_MAX)==NULL,"unsigned allocation/prefix wrap rejects");}
 else if(proof==3){FreeMemory(NULL);}
 else{Successes();Failures();
#ifdef MEMORYMANEGER
 AccountingFailures();
#endif
 puts("Native bot allocation lengths, nullable clearing, heap/hunk ownership and cleanup passed (issues #47/#48)");}
 Reset();return 0;
}
