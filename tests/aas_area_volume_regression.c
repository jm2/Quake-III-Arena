/* An AAS file passes its index checks with a face that has no edges, and that
 * face may name the end of the edge index. Actual native AAS_AreaVolume, with
 * arrays exact to the byte so that AddressSanitizer sees any read past them. */
#include "../code/game/q_shared.h"
#include "../code/botlib/aasfile.h"
#include "../code/game/botlib.h"
#include "../code/game/be_aas.h"
#include "../code/botlib/be_aas_def.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
aas_t aasworld;
#include Q3_AAS_VOLUME_BODY
static void Check(int condition,const char *message) {
    if(!condition){fprintf(stderr,"AAS area volume regression failed: %s\n",message);exit(1);}
}
static void *Copy(const void *data,size_t size) {
    void *copy=malloc(size);
    Check(copy!=NULL,"allocation");
    memcpy(copy,data,size);
    return copy;
}
/* Area 1 is a unit tetrahedron's base seen from the far corner, as one face. */
static void Build(int emptyFaces,int realFaces) {
    static const aas_vertex_t vertexes[]={{0,0,0},{1,0,0},{0,1,0},{0,0,1}};
    static const aas_edge_t edges[]={{{0,0}},{{0,1}},{{1,2}},{{2,0}}};
    static const aas_edgeindex_t edgeindex[]={1,2,3};
    aas_plane_t planes[2];
    aas_face_t faces[3];
    aas_faceindex_t faceindex[2];
    aas_area_t areas[2];
    int i,count=0;
    memset(planes,0,sizeof(planes));
    memset(faces,0,sizeof(faces));
    memset(areas,0,sizeof(areas));
    planes[0].normal[2]=1;
    planes[1].normal[2]=-1;
    for(i=0;i<emptyFaces;i++) {
        faces[count+1].firstedge=3;
        faces[count+1].backarea=1;
        faceindex[count]=count+1;
        count++;
    }
    for(i=0;i<realFaces;i++) {
        faces[count+1].numedges=3;
        faces[count+1].backarea=1;
        faceindex[count]=count+1;
        count++;
    }
    areas[1].numfaces=count;
    aasworld.vertexes=Copy(vertexes,sizeof(vertexes));
    aasworld.edges=Copy(edges,sizeof(edges));
    aasworld.edgeindex=Copy(edgeindex,sizeof(edgeindex));
    aasworld.planes=Copy(planes,sizeof(planes));
    aasworld.faces=Copy(faces,sizeof(aas_face_t)*(count+1));
    aasworld.faceindex=Copy(faceindex,sizeof(aas_faceindex_t)*(count?count:1));
    aasworld.areas=Copy(areas,sizeof(areas));
}
static void Release(void) {
    free(aasworld.vertexes);free(aasworld.edges);free(aasworld.edgeindex);
    free(aasworld.planes);free(aasworld.faces);free(aasworld.faceindex);free(aasworld.areas);
}
int main(void) {
    float plain,mixed;
    Build(0,1);
    plain=AAS_AreaVolume(1);
    Release();
    Check(plain==0,"the corner lies in the only face's plane");
    Build(1,1);
    mixed=AAS_AreaVolume(1);
    Release();
    Check(mixed==plain,"an empty first face leaves the corner to the next face");
    Build(2,0);
    Check(AAS_AreaVolume(1)==0,"an area of empty faces has no volume");
    Release();
    Build(0,0);
    Check(AAS_AreaVolume(1)==0,"an area with no faces has no volume");
    Release();
    puts("AAS area volume regressions passed");
    return 0;
}
