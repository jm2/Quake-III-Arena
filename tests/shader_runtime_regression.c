/* Issue #46: actual shader math/noise bodies, without graphics imports. */
#include "renderer_image_gl_stub.h"
#include "../code/renderer/tr_image.c"
#include "../code/renderer/tr_shade_calc.c"
#include "../code/renderer/tr_noise.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <float.h>
#include <limits.h>
trGlobals_t tr;
glconfig_t glConfig;
backEndState_t backEnd;
shaderCommands_t tess;
refimport_t ri;
static trRefEntity_t entity;
static void Check(int condition,const char *message) {if(!condition){fprintf(stderr,"Shader runtime regression failed: %s\n",message);exit(1);}}
void QDECL Com_Error(int level,const char *format,...) {(void)level;(void)format;Check(0,"unexpected native error");exit(1);}
void QDECL Com_Printf(const char *format,...) {(void)format;}
#ifndef Com_Memcpy
void Com_Memcpy(void *out,const void *in,size_t n) {memcpy(out,in,n);}
#endif
#ifndef Com_Memset
void Com_Memset(void *out,int value,size_t n) {memset(out,value,n);}
#endif
static unsigned int Hash(unsigned int hash,const void *data,size_t n) {const byte *p=data;while(n--){hash^=*p++;hash*=16777619u;}return hash;}
static void Setup(void) {
    int i;memset(&tr,0,sizeof(tr));memset(&tess,0,sizeof(tess));memset(&backEnd,0,sizeof(backEnd));memset(&entity,0,sizeof(entity));
    ri.Error=Com_Error;backEnd.currentEntity=&entity;tess.numVertexes=4;tr.identityLight=1;
    for(i=0;i<FUNCTABLE_SIZE;i++){float value=(i-512)/512.0f;tr.sinTable[i]=value;tr.triangleTable[i]=value;tr.squareTable[i]=value;tr.sawToothTable[i]=value;tr.inverseSawToothTable[i]=value;}
    R_NoiseInit();R_InitFogTable();
}
/* Independent native pre-fix oracle: use only coordinates for which the
 * original casts/permutation arithmetic are defined. Same compiler and flags. */
static float StockNoiseGet4f( float x, float y, float z, float t )
{
	int i;
	int ix, iy, iz, it;
	float fx, fy, fz, ft;
	float front[4];
	float back[4];
	float fvalue, bvalue, value[2], finalvalue;

	ix = ( int ) floor( x );
	fx = x - ix;
	iy = ( int ) floor( y );
	fy = y - iy;
	iz = ( int ) floor( z );
	fz = z - iz;
	it = ( int ) floor( t );
	ft = t - it;

	for ( i = 0; i < 2; i++ )
	{
		front[0] = GetNoiseValue( ix, iy, iz, it + i );
		front[1] = GetNoiseValue( ix+1, iy, iz, it + i );
		front[2] = GetNoiseValue( ix, iy+1, iz, it + i );
		front[3] = GetNoiseValue( ix+1, iy+1, iz, it + i );

		back[0] = GetNoiseValue( ix, iy, iz + 1, it + i );
		back[1] = GetNoiseValue( ix+1, iy, iz + 1, it + i );
		back[2] = GetNoiseValue( ix, iy+1, iz + 1, it + i );
		back[3] = GetNoiseValue( ix+1, iy+1, iz + 1, it + i );

		fvalue = LERP( LERP( front[0], front[1], fx ), LERP( front[2], front[3], fx ), fy );
		bvalue = LERP( LERP( back[0], back[1], fx ), LERP( back[2], back[3], fx ), fy );

		value[i] = LERP( fvalue, bvalue, fz );
	}

	finalvalue = LERP( value[0], value[1], ft );

	return finalvalue;
}

static float StockWave(const waveForm_t *wave) {
    float *table=TableForFunc(wave->func);
    return wave->base+table[myftol((wave->phase+tess.shaderTime*wave->frequency)*FUNCTABLE_SIZE)&FUNCTABLE_MASK]*wave->amplitude;
}
static int NoiseMatches(float actual,float stock) {
#ifdef __FAST_MATH__
    /* Native release reassociation changes the last bits across code shapes. */
    return fabs((double)actual-stock)<=4.0*FLT_EPSILON;
#else
    return actual==stock;
#endif
}
static unsigned int StockSamples(void) {
    int mode,i;uint32_t colors[4];unsigned int hash=2166136261u;
    const genFunc_t funcs[]={GF_SIN,GF_TRIANGLE,GF_SQUARE,GF_SAWTOOTH,GF_INVERSE_SAWTOOTH};
    for(mode=0;mode<5;mode++)for(i=-64;i<=64;i++) {
        waveForm_t wave={funcs[mode],0.5f,0.25f,0.125f,(i+65)/32.0f};float value;tess.shaderTime=i/16.0f;
        value=EvalWaveForm(&wave);Check(value==StockWave(&wave),"native waveform result");hash=Hash(hash,&value,sizeof(value));
        memset(colors,0xa5,sizeof(colors));RB_CalcWaveColor(&wave,(byte*)colors);RB_CalcWaveAlpha(&wave,(byte*)colors);
        {int vertex;float glow=StockWave(&wave);if(glow<0)glow=0;else if(glow>1)glow=1;
            for(vertex=0;vertex<4;vertex++){byte *color=(byte *)&colors[vertex];Check(color[0]==myftol(255*glow)&&color[1]==color[0]&&color[2]==color[0]&&color[3]==(int)(255*glow),"native waveform colors and alpha");}}
        hash=Hash(hash,colors,sizeof(colors));
    }
    for(i=-256;i<=256;i++){float value=R_NoiseGet4f(i/8.0f,-i/16.0f,i/32.0f,i/64.0f);{float stock=StockNoiseGet4f(i/8.0f,-i/16.0f,i/32.0f,i/64.0f);Check(NoiseMatches(value,stock),"native noise interpolation result");}hash=Hash(hash,&value,sizeof(value));}
    return hash;
}

static float StockFogFactor( float s, float t ) {
	float	d;

	s -= 1.0/512;
	if ( s < 0 ) {
		return 0;
	}
	if ( t < 1.0/32 ) {
		return 0;
	}
	if ( t < 31.0/32 ) {
		s *= (t - 1.0f/32.0f) / (30.0f/32.0f);
	}

	// we need to leave a lot of clamp range
	s *= 8;

	if ( s > 1.0 ) {
		s = 1.0;
	}

	d = tr.fogTable[ (int)(s * (FOG_TABLE_SIZE-1)) ];

	return d;
}

static float FloatBits(uint32_t bits) {float value;memcpy(&value,&bits,sizeof(value));return value;}
static void ConversionBounds(void) {
    const float valid[]={-2147483648.0f,-2147483520.0f,-1.75f,-0.0f,0.0f,1.75f,255.0f,2147483520.0f};
    const uint32_t invalid[]={0x7fc00000u,0xffc00000u,0x7f800001u,0xff800001u,0x7f800000u,0xff800000u,0x7f7fffffu,0xff7fffffu,0x4f000000u,0xcf000001u};
    size_t i;
    for(i=0;i<sizeof(valid)/sizeof(valid[0]);i++) {
        Check(R_FiniteFloat(valid[i])&&R_FloatToIntValid(valid[i]),"representable native int input");
        Check(R_FloatToInt(valid[i])==(int)valid[i],"native C truncation retained");
        Check(R_Ftol(valid[i])==myftol(valid[i]),"native myftol rounding retained");
    }
    for(i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++) {
        float value=FloatBits(invalid[i]);
        Check(!R_FloatToIntValid(value)&&R_FloatToInt(value)==0&&R_Ftol(value)==0,"unsafe conversion zero fallback");
        Check(R_FiniteFloat(value)==((invalid[i]&0x7f800000u)!=0x7f800000u),"finite classification survives fast math");
    }
}
static void NoiseBounds(void) {
    const float extremes[]={FLT_MAX,-FLT_MAX,2147483648.0f,-2147483648.0f,2147483520.0f,-2147483520.0f,65536.25f,-65536.25f};
    const uint32_t invalid[]={0x7fc00000u,0xffc00000u,0x7f800001u,0xff800001u,0x7f800000u,0xff800000u};
    size_t i;int axis;
    for(i=0;i<sizeof(extremes)/sizeof(extremes[0]);i++)for(axis=0;axis<4;axis++) {
        float coords[]={0.25f,-0.5f,0.75f,-0.125f},reduced[4],actual,expected;
        coords[axis]=extremes[i];memcpy(reduced,coords,sizeof(coords));reduced[axis]=(float)fmod((double)coords[axis],NOISE_SIZE);
        actual=R_NoiseGet4f(coords[0],coords[1],coords[2],coords[3]);
        expected=StockNoiseGet4f(reduced[0],reduced[1],reduced[2],reduced[3]);
        Check(R_FiniteFloat(actual)&&NoiseMatches(actual,expected),"all-axis finite noise retains native periodic interpolation");
    }
    for(i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++)for(axis=0;axis<4;axis++) {
        float coords[]={0.25f,-0.5f,0.75f,-0.125f};coords[axis]=FloatBits(invalid[i]);
        Check(R_NoiseGet4f(coords[0],coords[1],coords[2],coords[3])==0,"nonfinite noise returns zero before cell conversion");
    }
}
static void FogBounds(void) {
    int x,y,axis;size_t i;const uint32_t invalid[]={0x7fc00000u,0xffc00000u,0x7f800001u,0xff800001u,0x7f800000u,0xff800000u};
    for(y=0;y<FOG_T;y++)for(x=0;x<FOG_S;x++) {
        float s=(x+0.5f)/FOG_S,t=(y+0.5f)/FOG_T;
        Check(R_FogFactor(s,t)==StockFogFactor(s,t),"all native fog texture samples retained");
    }
    Check(R_FogFactor(FLT_MAX,1)==StockFogFactor(FLT_MAX,1),"finite overflowing fog coordinate preserves native saturation");
    Check(R_FogFactor(-FLT_MAX,1)==0&&R_FogFactor(0.5f,-FLT_MAX)==0,"finite negative fog coordinates retain zero density");
    for(i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++)for(axis=0;axis<2;axis++) {
        float coords[]={0.5f,0.5f};coords[axis]=FloatBits(invalid[i]);
        Check(R_FogFactor(coords[0],coords[1])==0,"nonfinite fog coordinate returns zero before indexing");
    }
}

static void DerivedConsumers(void) {
    const genFunc_t funcs[]={GF_SIN,GF_TRIANGLE,GF_SQUARE,GF_SAWTOOTH,GF_INVERSE_SAWTOOTH};
    uint32_t colors[4];int i,vertex;waveForm_t wave={GF_SIN,0.5f,0.25f,0,FLT_MAX};deformStage_t bulge;
    for(i=0;i<5;i++) {
        wave.func=funcs[i];tess.shaderTime=2;
        Check(EvalWaveForm(&wave)==0.25f,"overflow waveform phase uses table index zero");
        wave.frequency=FloatBits(0x7fc00000u);
        Check(EvalWaveForm(&wave)==0.25f,"nonfinite waveform phase uses table index zero");wave.frequency=FLT_MAX;
    }
    wave.func=GF_SIN;wave.frequency=0;wave.base=FloatBits(0x7fc00000u);tess.shaderTime=0;
    memset(colors,0xa5,sizeof(colors));RB_CalcWaveColor(&wave,(byte*)colors);RB_CalcWaveAlpha(&wave,(byte*)colors);
    for(vertex=0;vertex<4;vertex++){byte *c=(byte*)&colors[vertex];Check(c[0]==0&&c[1]==0&&c[2]==0&&c[3]==0,"nonfinite wave color/alpha conversions return zero");}
    memset(&bulge,0,sizeof(bulge));bulge.bulgeWidth=FLT_MAX;bulge.bulgeHeight=2;
    memset(tess.xyz,0,sizeof(tess.xyz));memset(tess.normal,0,sizeof(tess.normal));memset(tess.texCoords,0,sizeof(tess.texCoords));
    for(vertex=0;vertex<4;vertex++){tess.texCoords[vertex][0][0]=2;tess.normal[vertex][0]=1;}
    RB_CalcBulgeVertexes(&bulge);
    for(vertex=0;vertex<4;vertex++)Check(tess.xyz[vertex][0]==-2&&tess.xyz[vertex][1]==0,"overflow bulge phase uses native zero-index scale");
    VectorSet(entity.lightDir,1,0,0);VectorSet(entity.ambientLight,FLT_MAX,FLT_MAX,FLT_MAX);VectorSet(entity.directedLight,FLT_MAX,FLT_MAX,FLT_MAX);
    memset(colors,0xa5,sizeof(colors));RB_CalcDiffuseColor((byte*)colors);
    for(vertex=0;vertex<4;vertex++){byte *c=(byte*)&colors[vertex];Check(c[0]==0&&c[1]==0&&c[2]==0&&c[3]==255,"overflow diffuse channels use zero conversion");}
}

int main(int argc,char **argv) {
    int proof=argc>1?atoi(argv[1]):-1;waveForm_t wave={GF_SIN,0.5f,0.25f,0,FLT_MAX};float value;
    Setup();
    if(proof==0){tess.shaderTime=2;value=EvalWaveForm(&wave);Check(value==0.25f,"overflow phase has defined zero-index fallback");}
    else if(proof==1){value=R_NoiseGet4f(FLT_MAX,0,0,0);Check(value==R_NoiseGet4f(0,0,0,0),"extreme finite noise retains periodic coordinates");}
    else if(proof==2){uint32_t colors[4];{uint32_t nanBits=0x7fc00000;memcpy(&wave.base,&nanBits,sizeof(wave.base));}wave.frequency=0;tess.shaderTime=0;RB_CalcWaveAlpha(&wave,(byte*)colors);}
    else if(proof==4){value=R_FogFactor(FloatBits(0x7fc00000u),0.5f);Check(value==0,"nonfinite fog returns zero");}
    else if(proof==3)printf("Stock runtime/noise fingerprint %08x\n",StockSamples());
    else {
        (void)StockSamples();
        ConversionBounds();
        NoiseBounds();
        FogBounds();
        DerivedConsumers();
        tess.shaderTime=2;value=EvalWaveForm(&wave);Check(value==0.25f,"overflow phase has defined zero-index fallback");
        Check(R_NoiseGet4f(FLT_MAX,0,0,0)==R_NoiseGet4f(0,0,0,0),"extreme finite noise remains periodic");
        puts("Native shader math/noise stock samples and overflow checks passed (issue #46)");
    }
    return 0;
}
