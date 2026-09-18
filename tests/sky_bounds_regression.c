/* Issue #46: actual native sky subdivision/cloud assembly, isolated GL. */
#include "renderer_sky_gl_stub.h"
#include "../code/renderer/tr_sky.c"
#include Q3_SKY_COMMON_MATH
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <setjmp.h>
#include <float.h>
trGlobals_t tr;
backEndState_t backEnd;
shaderCommands_t tess;
refimport_t ri;
static shader_t material;
static shaderStage_t stage;
static image_t images[6];
static jmp_buf failure;
static int expectError,errors,binds,strips,vertices,coordinates;
static unsigned int trace;
static void Check(int condition,const char *message) {if(!condition){fprintf(stderr,"Sky regression failed: %s\n",message);exit(1);}}
void QDECL Com_Error(int level,const char *format,...) {(void)level;(void)format;Check(expectError,"unexpected native error");errors++;longjmp(failure,1);}
void QDECL Com_Printf(const char *format,...) {(void)format;}
#ifndef Com_Memcpy
void Com_Memcpy(void *out,const void *in,size_t n) {memcpy(out,in,n);}
#endif
#ifndef Com_Memset
void Com_Memset(void *out,int value,size_t n) {memset(out,value,n);}
#endif
static unsigned int Hash(unsigned int hash,const void *data,size_t n) {const byte *p=data;while(n--){hash^=*p++;hash*=16777619u;}return hash;}
void GL_Bind(image_t *image) {Check(image>=images&&image<images+6,"native side image");binds++;trace=Hash(trace,&image->texnum,sizeof(image->texnum));}
static void Begin(GLenum mode) {Check(mode==GL_TRIANGLE_STRIP,"native strip mode");strips++;}
static void End(void) {}
static void TexCoord(const GLfloat *v) {Check(R_FiniteFloat(v[0])&&R_FiniteFloat(v[1]),"finite native sky UV");coordinates++;trace=Hash(trace,v,2*sizeof(*v));}
static void Vertex(const GLfloat *v) {Check(R_FiniteFloat(v[0])&&R_FiniteFloat(v[1])&&R_FiniteFloat(v[2]),"finite native sky position");vertices++;trace=Hash(trace,v,3*sizeof(*v));}
void (*qglBegin)(GLenum)=Begin;
void (*qglEnd)(void)=End;
void (*qglTexCoord2fv)(const GLfloat *)=TexCoord;
void (*qglVertex3fv)(const GLfloat *)=Vertex;
static void Setup(void) {
    int side,axis;memset(&tr,0,sizeof(tr));memset(&backEnd,0,sizeof(backEnd));memset(&tess,0,sizeof(tess));memset(&material,0,sizeof(material));
    ri.Error=Com_Error;material.isSky=qtrue;material.sky.cloudHeight=512;tess.shader=&material;tess.xstages=material.stages;backEnd.viewParms.zFar=1024;
    binds=strips=vertices=coordinates=errors=expectError=0;trace=2166136261u;
    for(side=0;side<6;side++){images[side].texnum=side+1;material.sky.outerbox[side]=&images[side];for(axis=0;axis<2;axis++){sky_mins[axis][side]=-1;sky_maxs[axis][side]=1;}}
    tess.xstages[0]=&stage;R_InitSkyTexCoords(512);
}
static unsigned int CloudHash(int indexedOnly) {
    unsigned int hash=2166136261u;int i,count=indexedOnly?405:tess.numVertexes;
    hash=Hash(hash,&count,sizeof(count));hash=Hash(hash,&tess.numIndexes,sizeof(tess.numIndexes));
    for(i=0;i<count;i++){hash=Hash(hash,tess.xyz[i],sizeof(vec4_t));hash=Hash(hash,tess.texCoords[i],sizeof(tess.texCoords[i]));}
    return Hash(hash,tess.indexes,tess.numIndexes*sizeof(tess.indexes[0]));
}
/* Native pre-fix oracle: trusted bounds/counts only. */
static void StockDrawSkyBox( shader_t *shader )
{
	int		i;

	sky_min = 0;
	sky_max = 1;

	Com_Memset( s_skyTexCoords, 0, sizeof( s_skyTexCoords ) );

	for (i=0 ; i<6 ; i++)
	{
		int sky_mins_subd[2], sky_maxs_subd[2];
		int s, t;

		sky_mins[0][i] = floor( sky_mins[0][i] * HALF_SKY_SUBDIVISIONS ) / HALF_SKY_SUBDIVISIONS;
		sky_mins[1][i] = floor( sky_mins[1][i] * HALF_SKY_SUBDIVISIONS ) / HALF_SKY_SUBDIVISIONS;
		sky_maxs[0][i] = ceil( sky_maxs[0][i] * HALF_SKY_SUBDIVISIONS ) / HALF_SKY_SUBDIVISIONS;
		sky_maxs[1][i] = ceil( sky_maxs[1][i] * HALF_SKY_SUBDIVISIONS ) / HALF_SKY_SUBDIVISIONS;

		if ( ( sky_mins[0][i] >= sky_maxs[0][i] ) ||
			 ( sky_mins[1][i] >= sky_maxs[1][i] ) )
		{
			continue;
		}

		sky_mins_subd[0] = sky_mins[0][i] * HALF_SKY_SUBDIVISIONS;
		sky_mins_subd[1] = sky_mins[1][i] * HALF_SKY_SUBDIVISIONS;
		sky_maxs_subd[0] = sky_maxs[0][i] * HALF_SKY_SUBDIVISIONS;
		sky_maxs_subd[1] = sky_maxs[1][i] * HALF_SKY_SUBDIVISIONS;

		if ( sky_mins_subd[0] < -HALF_SKY_SUBDIVISIONS ) 
			sky_mins_subd[0] = -HALF_SKY_SUBDIVISIONS;
		else if ( sky_mins_subd[0] > HALF_SKY_SUBDIVISIONS ) 
			sky_mins_subd[0] = HALF_SKY_SUBDIVISIONS;
		if ( sky_mins_subd[1] < -HALF_SKY_SUBDIVISIONS )
			sky_mins_subd[1] = -HALF_SKY_SUBDIVISIONS;
		else if ( sky_mins_subd[1] > HALF_SKY_SUBDIVISIONS ) 
			sky_mins_subd[1] = HALF_SKY_SUBDIVISIONS;

		if ( sky_maxs_subd[0] < -HALF_SKY_SUBDIVISIONS ) 
			sky_maxs_subd[0] = -HALF_SKY_SUBDIVISIONS;
		else if ( sky_maxs_subd[0] > HALF_SKY_SUBDIVISIONS ) 
			sky_maxs_subd[0] = HALF_SKY_SUBDIVISIONS;
		if ( sky_maxs_subd[1] < -HALF_SKY_SUBDIVISIONS ) 
			sky_maxs_subd[1] = -HALF_SKY_SUBDIVISIONS;
		else if ( sky_maxs_subd[1] > HALF_SKY_SUBDIVISIONS ) 
			sky_maxs_subd[1] = HALF_SKY_SUBDIVISIONS;

		//
		// iterate through the subdivisions
		//
		for ( t = sky_mins_subd[1]+HALF_SKY_SUBDIVISIONS; t <= sky_maxs_subd[1]+HALF_SKY_SUBDIVISIONS; t++ )
		{
			for ( s = sky_mins_subd[0]+HALF_SKY_SUBDIVISIONS; s <= sky_maxs_subd[0]+HALF_SKY_SUBDIVISIONS; s++ )
			{
				MakeSkyVec( ( s - HALF_SKY_SUBDIVISIONS ) / ( float ) HALF_SKY_SUBDIVISIONS, 
							( t - HALF_SKY_SUBDIVISIONS ) / ( float ) HALF_SKY_SUBDIVISIONS, 
							i, 
							s_skyTexCoords[t][s], 
							s_skyPoints[t][s] );
			}
		}

		DrawSkySide( shader->sky.outerbox[sky_texorder[i]],
			         sky_mins_subd,
					 sky_maxs_subd );
	}

}

/* Native pre-fix oracle: trusted bounds/counts only. */
static void StockFillCloudySkySide( const int mins[2], const int maxs[2], qboolean addIndexes )
{
	int s, t;
	int vertexStart = tess.numVertexes;
	int tHeight, sWidth;

	tHeight = maxs[1] - mins[1] + 1;
	sWidth = maxs[0] - mins[0] + 1;

	for ( t = mins[1]+HALF_SKY_SUBDIVISIONS; t <= maxs[1]+HALF_SKY_SUBDIVISIONS; t++ )
	{
		for ( s = mins[0]+HALF_SKY_SUBDIVISIONS; s <= maxs[0]+HALF_SKY_SUBDIVISIONS; s++ )
		{
			VectorAdd( s_skyPoints[t][s], backEnd.viewParms.or.origin, tess.xyz[tess.numVertexes] );
			tess.texCoords[tess.numVertexes][0][0] = s_skyTexCoords[t][s][0];
			tess.texCoords[tess.numVertexes][0][1] = s_skyTexCoords[t][s][1];

			tess.numVertexes++;

			if ( tess.numVertexes >= SHADER_MAX_VERTEXES )
			{
				ri.Error( ERR_DROP, "SHADER_MAX_VERTEXES hit in FillCloudySkySide()\n" );
			}
		}
	}

	// only add indexes for one pass, otherwise it would draw multiple times for each pass
	if ( addIndexes ) {
		for ( t = 0; t < tHeight-1; t++ )
		{	
			for ( s = 0; s < sWidth-1; s++ )
			{
				tess.indexes[tess.numIndexes] = vertexStart + s + t * ( sWidth );
				tess.numIndexes++;
				tess.indexes[tess.numIndexes] = vertexStart + s + ( t + 1 ) * ( sWidth );
				tess.numIndexes++;
				tess.indexes[tess.numIndexes] = vertexStart + s + 1 + t * ( sWidth );
				tess.numIndexes++;

				tess.indexes[tess.numIndexes] = vertexStart + s + ( t + 1 ) * ( sWidth );
				tess.numIndexes++;
				tess.indexes[tess.numIndexes] = vertexStart + s + 1 + ( t + 1 ) * ( sWidth );
				tess.numIndexes++;
				tess.indexes[tess.numIndexes] = vertexStart + s + 1 + t * ( sWidth );
				tess.numIndexes++;
			}
		}
	}
}

/* Native pre-fix oracle: trusted bounds/counts only. */
static void StockFillCloudBox( const shader_t *shader, int stage )
{
	int i;

	for ( i =0; i < 6; i++ )
	{
		int sky_mins_subd[2], sky_maxs_subd[2];
		int s, t;
		float MIN_T;

		if ( 1 ) // FIXME? shader->sky.fullClouds )
		{
			MIN_T = -HALF_SKY_SUBDIVISIONS;

			// still don't want to draw the bottom, even if fullClouds
			if ( i == 5 )
				continue;
		}
		else
		{
			switch( i )
			{
			case 0:
			case 1:
			case 2:
			case 3:
				MIN_T = -1;
				break;
			case 5:
				// don't draw clouds beneath you
				continue;
			case 4:		// top
			default:
				MIN_T = -HALF_SKY_SUBDIVISIONS;
				break;
			}
		}

		sky_mins[0][i] = floor( sky_mins[0][i] * HALF_SKY_SUBDIVISIONS ) / HALF_SKY_SUBDIVISIONS;
		sky_mins[1][i] = floor( sky_mins[1][i] * HALF_SKY_SUBDIVISIONS ) / HALF_SKY_SUBDIVISIONS;
		sky_maxs[0][i] = ceil( sky_maxs[0][i] * HALF_SKY_SUBDIVISIONS ) / HALF_SKY_SUBDIVISIONS;
		sky_maxs[1][i] = ceil( sky_maxs[1][i] * HALF_SKY_SUBDIVISIONS ) / HALF_SKY_SUBDIVISIONS;

		if ( ( sky_mins[0][i] >= sky_maxs[0][i] ) ||
			 ( sky_mins[1][i] >= sky_maxs[1][i] ) )
		{
			continue;
		}

		sky_mins_subd[0] = myftol( sky_mins[0][i] * HALF_SKY_SUBDIVISIONS );
		sky_mins_subd[1] = myftol( sky_mins[1][i] * HALF_SKY_SUBDIVISIONS );
		sky_maxs_subd[0] = myftol( sky_maxs[0][i] * HALF_SKY_SUBDIVISIONS );
		sky_maxs_subd[1] = myftol( sky_maxs[1][i] * HALF_SKY_SUBDIVISIONS );

		if ( sky_mins_subd[0] < -HALF_SKY_SUBDIVISIONS ) 
			sky_mins_subd[0] = -HALF_SKY_SUBDIVISIONS;
		else if ( sky_mins_subd[0] > HALF_SKY_SUBDIVISIONS ) 
			sky_mins_subd[0] = HALF_SKY_SUBDIVISIONS;
		if ( sky_mins_subd[1] < MIN_T )
			sky_mins_subd[1] = MIN_T;
		else if ( sky_mins_subd[1] > HALF_SKY_SUBDIVISIONS ) 
			sky_mins_subd[1] = HALF_SKY_SUBDIVISIONS;

		if ( sky_maxs_subd[0] < -HALF_SKY_SUBDIVISIONS ) 
			sky_maxs_subd[0] = -HALF_SKY_SUBDIVISIONS;
		else if ( sky_maxs_subd[0] > HALF_SKY_SUBDIVISIONS ) 
			sky_maxs_subd[0] = HALF_SKY_SUBDIVISIONS;
		if ( sky_maxs_subd[1] < MIN_T )
			sky_maxs_subd[1] = MIN_T;
		else if ( sky_maxs_subd[1] > HALF_SKY_SUBDIVISIONS ) 
			sky_maxs_subd[1] = HALF_SKY_SUBDIVISIONS;

		//
		// iterate through the subdivisions
		//
		for ( t = sky_mins_subd[1]+HALF_SKY_SUBDIVISIONS; t <= sky_maxs_subd[1]+HALF_SKY_SUBDIVISIONS; t++ )
		{
			for ( s = sky_mins_subd[0]+HALF_SKY_SUBDIVISIONS; s <= sky_maxs_subd[0]+HALF_SKY_SUBDIVISIONS; s++ )
			{
				MakeSkyVec( ( s - HALF_SKY_SUBDIVISIONS ) / ( float ) HALF_SKY_SUBDIVISIONS, 
							( t - HALF_SKY_SUBDIVISIONS ) / ( float ) HALF_SKY_SUBDIVISIONS, 
							i, 
							NULL,
							s_skyPoints[t][s] );

				s_skyTexCoords[t][s][0] = s_cloudTexCoords[i][t][s][0];
				s_skyTexCoords[t][s][1] = s_cloudTexCoords[i][t][s][1];
			}
		}

		// only add indexes for first stage
		StockFillCloudySkySide( sky_mins_subd, sky_maxs_subd, ( stage == 0 ) );
	}
}

static shaderCommands_t nativeGeometry;
static void CompareGeometry(int indexedVertices) {
    int count=indexedVertices>=0?indexedVertices:nativeGeometry.numVertexes;
    Check(tess.numVertexes==count&&tess.numIndexes==nativeGeometry.numIndexes,"native indexed geometry counts");
    Check(!memcmp(tess.xyz,nativeGeometry.xyz,count*sizeof(tess.xyz[0])),"native cloud vertices");
    Check(!memcmp(tess.texCoords,nativeGeometry.texCoords,count*sizeof(tess.texCoords[0])),"native cloud UVs");
    Check(!memcmp(tess.indexes,nativeGeometry.indexes,tess.numIndexes*sizeof(tess.indexes[0])),"native cloud index order");
}
static void NativeBounds(void) {
    int low,high,axis;
    for(axis=0;axis<2;axis++)for(low=0;low<=32;low++)for(high=low;high<=32;high++) {
        unsigned int nativeTrace;int nativeBinds,nativeStrips,nativeVertices;
        float first=-1+low/16.0f,last=-1+high/16.0f;
        Setup();sky_mins[axis][0]=first;sky_maxs[axis][0]=last;StockDrawSkyBox(&material);
        nativeTrace=trace;nativeBinds=binds;nativeStrips=strips;nativeVertices=vertices;
        Setup();sky_mins[axis][0]=first;sky_maxs[axis][0]=last;DrawSkyBox(&material);
        Check(trace==nativeTrace&&binds==nativeBinds&&strips==nativeStrips&&vertices==nativeVertices&&coordinates==vertices,"native complete sky graphics trace");
        Setup();sky_mins[axis][0]=first;sky_maxs[axis][0]=last;StockFillCloudBox(&material,0);nativeGeometry=tess;
        Setup();sky_mins[axis][0]=first;sky_maxs[axis][0]=last;FillCloudBox(&material,0);CompareGeometry(-1);
    }
}
static void StagesAndExtremeBounds(void) {
    int count,field;size_t i;const uint32_t invalid[]={0x7fc00000u,0xffc00000u,0x7f800001u,0xff800001u,0x7f800000u,0xff800000u};
    Setup();StockFillCloudBox(&material,0);StockFillCloudBox(&material,1);Check(tess.numVertexes==810,"original later cloud vertices were unindexed");nativeGeometry=tess;
    for(count=0;count<=MAX_SHADER_STAGES;count++) {
        Setup();memset(material.stages,0,sizeof(material.stages));for(field=0;field<count;field++)material.stages[field]=&stage;R_BuildCloudData(&tess);
        if(count)CompareGeometry(405);else Check(!tess.numVertexes&&!tess.numIndexes,"zero stages publish no cloud geometry");
    }
    Setup();tess.xstages=NULL;R_BuildCloudData(&tess);Check(!tess.numVertexes&&!tess.numIndexes,"absent stages publish no geometry");
    Setup();material.sky.cloudHeight=0;R_BuildCloudData(&tess);Check(!tess.numVertexes&&!tess.numIndexes,"native zero cloud height disables generation");
    Setup();sky_mins[0][0]=-FLT_MAX;sky_maxs[0][0]=FLT_MAX;DrawSkyBox(&material);Check(binds==6&&vertices==864,"finite extreme sky bounds saturate to visible cube");
    Setup();sky_mins[0][0]=-FLT_MAX;sky_maxs[0][0]=FLT_MAX;R_BuildCloudData(&tess);CompareGeometry(405);
    for(i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++)for(field=0;field<4;field++) {
        float value;memcpy(&value,&invalid[i],sizeof(value));Setup();if(field<2)sky_mins[field][0]=value;else sky_maxs[field-2][0]=value;
        DrawSkyBox(&material);Check(binds==5&&vertices==720,"nonfinite side skips before subdivision conversion");
        R_BuildCloudData(&tess);Check(tess.numVertexes==324&&tess.numIndexes==1536,"nonfinite cloud side skips before array access");
    }
}
static void RejectGeometry(const int mins[2],const int maxs[2],int vertexCount,int indexCount) {
    Setup();tess.numVertexes=vertexCount;tess.numIndexes=indexCount;nativeGeometry=tess;expectError=1;
    if(!setjmp(failure)){FillCloudySkySide(mins,maxs,qtrue);Check(0,"expected native bounds/capacity rejection");}
    Check(errors==1&&!memcmp(&tess,&nativeGeometry,sizeof(tess)),"rejection precedes every geometry/counter write");expectError=0;
}
static void CapacityBounds(void) {
    const int mins[2]={0,0},maxs[2]={1,1};const int badLow[2]={-5,0},badHigh[2]={5,1},reversed[2]={2,1};
    Setup();tess.numVertexes=SHADER_MAX_VERTEXES-5;tess.numIndexes=SHADER_MAX_INDEXES-7;FillCloudySkySide(mins,maxs,qtrue);
    Check(tess.numVertexes==SHADER_MAX_VERTEXES-1&&tess.numIndexes==SHADER_MAX_INDEXES-1,"last usable native vertex/index slots allowed");
    Check(tess.xyz[SHADER_MAX_VERTEXES-1][0]==0&&tess.indexes[SHADER_MAX_INDEXES-1]==0,"backend sentinels remain untouched");
    RejectGeometry(mins,maxs,SHADER_MAX_VERTEXES-4,0);RejectGeometry(mins,maxs,0,SHADER_MAX_INDEXES-6);
    RejectGeometry(mins,maxs,-1,0);RejectGeometry(mins,maxs,SHADER_MAX_VERTEXES,0);RejectGeometry(mins,maxs,INT_MAX,0);
    RejectGeometry(mins,maxs,0,-1);RejectGeometry(mins,maxs,0,SHADER_MAX_INDEXES);RejectGeometry(mins,maxs,0,INT_MAX);
    RejectGeometry(badLow,maxs,0,0);RejectGeometry(mins,badHigh,0,0);RejectGeometry(reversed,maxs,0,0);
}

static void StockInitSkyTexCoords( float heightCloud )
{
	int i, s, t;
	float radiusWorld = 4096;
	float p;
	float sRad, tRad;
	vec3_t skyVec;
	vec3_t v;

	// init zfar so MakeSkyVec works even though
	// a world hasn't been bounded
	backEnd.viewParms.zFar = 1024;

	for ( i = 0; i < 6; i++ )
	{
		for ( t = 0; t <= SKY_SUBDIVISIONS; t++ )
		{
			for ( s = 0; s <= SKY_SUBDIVISIONS; s++ )
			{
				// compute vector from view origin to sky side integral point
				MakeSkyVec( ( s - HALF_SKY_SUBDIVISIONS ) / ( float ) HALF_SKY_SUBDIVISIONS,
							( t - HALF_SKY_SUBDIVISIONS ) / ( float ) HALF_SKY_SUBDIVISIONS,
							i,
							NULL,
							skyVec );

				// compute parametric value 'p' that intersects with cloud layer
				p = ( 1.0f / ( 2 * DotProduct( skyVec, skyVec ) ) ) *
					( -2 * skyVec[2] * radiusWorld +
					   2 * sqrt( SQR( skyVec[2] ) * SQR( radiusWorld ) +
					             2 * SQR( skyVec[0] ) * radiusWorld * heightCloud +
								 SQR( skyVec[0] ) * SQR( heightCloud ) +
								 2 * SQR( skyVec[1] ) * radiusWorld * heightCloud +
								 SQR( skyVec[1] ) * SQR( heightCloud ) +
								 2 * SQR( skyVec[2] ) * radiusWorld * heightCloud +
								 SQR( skyVec[2] ) * SQR( heightCloud ) ) );

				s_cloudTexP[i][t][s] = p;

				// compute intersection point based on p
				VectorScale( skyVec, p, v );
				v[2] += radiusWorld;

				// compute vector from world origin to intersection point 'v'
				VectorNormalize( v );

				sRad = Q_acos( v[0] );
				tRad = Q_acos( v[1] );

				s_cloudTexCoords[i][t][s][0] = sRad;
				s_cloudTexCoords[i][t][s][1] = tRad;
			}
		}
	}
}

static float nativeCloudP[6][SKY_SUBDIVISIONS+1][SKY_SUBDIVISIONS+1];
static float nativeCloudUV[6][SKY_SUBDIVISIONS+1][SKY_SUBDIVISIONS+1][2];
static int CloudMatches(const float *actual,const float *native,size_t count,int angles) {
    size_t i;
    for(i=0;i<count;i++) {
#ifdef __FAST_MATH__
        double first=angles?cos((double)actual[i]):actual[i],second=angles?cos((double)native[i]):native[i];
        if(fabs(first-second)>(angles?4.0:8.0)*FLT_EPSILON*fmax(1.0,fabs(second)))return 0;
#else
        if(actual[i]!=native[i])return 0;
#endif
    }
    return 1;
}
static int NativeCloudMatches(void) {
    return CloudMatches((float*)s_cloudTexP,(float*)nativeCloudP,sizeof(nativeCloudP)/sizeof(float),0)&&CloudMatches((float*)s_cloudTexCoords,(float*)nativeCloudUV,sizeof(nativeCloudUV)/sizeof(float),1);
}

static void CheckCloudFinite(void) {
    int side,t,vertex,axis;
    for(side=0;side<6;side++)for(t=0;t<=SKY_SUBDIVISIONS;t++)for(vertex=0;vertex<=SKY_SUBDIVISIONS;vertex++) {
        Check(R_FiniteFloat(s_cloudTexP[side][t][vertex]),"finite complete cloud parameters");
        for(axis=0;axis<2;axis++)Check(R_FiniteFloat(s_cloudTexCoords[side][t][vertex][axis])&&s_cloudTexCoords[side][t][vertex][axis]>=0&&s_cloudTexCoords[side][t][vertex][axis]<=(float)M_PI,"finite bounded complete cloud UV");
    }
}
/* Independent geometry invariant, rather than repeating intersection formulas. */
static void CheckCloudSurface(float height) {
    int side,t,s,axis;
    for(side=0;side<6;side++)for(t=0;t<=SKY_SUBDIVISIONS;t++)for(s=0;s<=SKY_SUBDIVISIONS;s++) {
        vec3_t ray;double point[3],length,target=fabs(4096.0+height);
        MakeSkyVec((s-HALF_SKY_SUBDIVISIONS)/(float)HALF_SKY_SUBDIVISIONS,(t-HALF_SKY_SUBDIVISIONS)/(float)HALF_SKY_SUBDIVISIONS,side,NULL,ray);
        for(axis=0;axis<3;axis++)point[axis]=(double)ray[axis]*s_cloudTexP[side][t][s];point[2]+=4096;
        length=sqrt(point[0]*point[0]+point[1]*point[1]+point[2]*point[2]);
        Check(fabs(length-target)<=16.0*FLT_EPSILON*fmax(1.0,target),"wide cloud point lies on its intended sphere");
        if(length)for(axis=0;axis<2;axis++)Check(fabs(cos((double)s_cloudTexCoords[side][t][s][axis])-point[axis]/length)<=8.0*FLT_EPSILON,"wide cloud UV matches normalized intersection direction");
    }
}

static void CloudLayers(void) {
    const float valid[]={0,1,32,512,1024,4096,32768,100000,1e9f,-8192,-9000,-1e9f};
    const float extreme[]={FLT_MAX,-FLT_MAX,1e10f,-1e10f,1e20f,-1e20f};
    const float malformed[]={-1,-4096,-8191};
    const uint32_t invalid[]={0x7fc00000u,0xffc00000u,0x7f800001u,0xff800001u,0x7f800000u,0xff800000u};
    size_t i;
    for(i=0;i<sizeof(valid)/sizeof(valid[0]);i++) {
        Setup();StockInitSkyTexCoords(valid[i]);CheckCloudFinite();memcpy(nativeCloudP,s_cloudTexP,sizeof(nativeCloudP));memcpy(nativeCloudUV,s_cloudTexCoords,sizeof(nativeCloudUV));
        R_InitSkyTexCoords(valid[i]);CheckCloudFinite();Check(NativeCloudMatches(),"complete native stable cloud parameters and UV retained");
    }
    for(i=0;i<sizeof(extreme)/sizeof(extreme[0]);i++){Setup();R_InitSkyTexCoords(extreme[i]);CheckCloudFinite();CheckCloudSurface(extreme[i]);}
    Setup();StockInitSkyTexCoords(512);memcpy(nativeCloudP,s_cloudTexP,sizeof(nativeCloudP));memcpy(nativeCloudUV,s_cloudTexCoords,sizeof(nativeCloudUV));
    for(i=0;i<sizeof(malformed)/sizeof(malformed[0]);i++){R_InitSkyTexCoords(malformed[i]);CheckCloudFinite();Check(NativeCloudMatches(),"invalid cloud intersection uses complete native default layer");}
    for(i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++){float value;memcpy(&value,&invalid[i],sizeof(value));R_InitSkyTexCoords(value);CheckCloudFinite();Check(NativeCloudMatches(),"nonfinite cloud input uses complete default layer");}
}

int main(int argc,char **argv) {
    int proof=argc>1?atoi(argv[1]):-1;const int mins[2]={-4,-4},maxs[2]={4,4};
    Setup();
    if(proof==0){sky_mins[0][0]=-FLT_MAX;sky_maxs[0][0]=FLT_MAX;DrawSkyBox(&material);}
    else if(proof==1){sky_mins[0][0]=-FLT_MAX;sky_maxs[0][0]=FLT_MAX;FillCloudBox(&material,0);}
    else if(proof==2){tess.numVertexes=SHADER_MAX_VERTEXES;expectError=1;if(!setjmp(failure))FillCloudySkySide(mins,maxs,qtrue);Check(errors==1,"capacity rejected before writes");}
    else if(proof==3){tess.numIndexes=SHADER_MAX_INDEXES;FillCloudySkySide(mins,maxs,qtrue);}
    else if(proof==4){tess.xstages[1]=tess.xstages[2]=&stage;R_BuildCloudData(&tess);}
    else if(proof==6){R_InitSkyTexCoords(FLT_MAX);CheckCloudFinite();}
    else if(proof==7){R_InitSkyTexCoords(-1);CheckCloudFinite();}
    else if(proof==5){DrawSkyBox(&material);printf("Stock sky trace %08x binds %d strips %d vertices %d\n",trace,binds,strips,vertices);Setup();R_BuildCloudData(&tess);printf("Stock indexed cloud fingerprint %08x vertices %d indexes %d\n",CloudHash(0),tess.numVertexes,tess.numIndexes);}
    else {NativeBounds();StagesAndExtremeBounds();CapacityBounds();CloudLayers();puts("Native sky subdivision, complete graphics/indexed-geometry oracles, 0-8 stages and capacity checks passed (issue #46)");}
    return 0;
}
