/* Issue #35: random() includes one, which must never select rankings[numranks]. */
#include "../code/botlib/be_ai_gen.c"
#include <stdlib.h>
#include <string.h>

botlib_import_t botimport;
/** Stop on an invalid selection, changed input, or native error. */
static void Check( int ok, const char *message ) {
	if(!ok) { fprintf(stderr,"Bot genetic regression failed: %s\n",message); exit(1); }
}
/** Force the inclusive random endpoint deterministically. */
int rand( void ) { return 0x7fff; }
/** Support the real native ranking copy. */
void Com_Memcpy( void *dest, const void *src, size_t size ) { memcpy(dest,src,size); }
/** Accept expected native invalid-selection diagnostics. */
static void Print( int type, char *format, ... ) { (void)type; (void)format; }
/** Exercise endpoint selection with exact-sized ranking arrays and three distinct results. */
int main( void ) {
	float *ranks; int p1,p2,child,n,i;
	botimport.Print=Print;
	Check(GeneticSelection(0,NULL)==0 && GeneticSelection(-1,NULL)==0 && GeneticSelection(INT_MAX,NULL)==0,"invalid selection dimensions");
	for(n=3;n<=256;n+=253) {
		ranks=calloc(n,sizeof(*ranks)); Check(ranks!=NULL,"allocation");
		Check(GeneticSelection(n,ranks)==n-1,"inclusive endpoint clamp");
		Check(GeneticParentsAndChildSelection(n,ranks,&p1,&p2,&child),"parent selection");
		Check(p1>=0 && p1<n && p2>=0 && p2<n && child>=0 && child<n && p1!=p2 && p1!=child && p2!=child,"distinct bounded indices");
		for(i=0;i<n;i++) Check(ranks[i]==0,"source rankings unchanged"); free(ranks);
	}
	puts("Bot native genetic endpoint regressions passed (issue #35)"); return 0;
}
