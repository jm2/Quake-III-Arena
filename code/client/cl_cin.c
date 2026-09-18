/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

/*****************************************************************************
 * name:		cl_cin.c
 *
 * desc:		video and cinematic playback
 *
 * $Archive: /MissionPack/code/client/cl_cin.c $
 *
 * cl_glconfig.hwtype trtypes 3dfx/ragepro need 256x256
 *
 *****************************************************************************/

#include "client.h"
#include "snd_local.h"

#define MAXSIZE				8
#define MINSIZE				4

#define DEFAULT_CIN_WIDTH	512
#define DEFAULT_CIN_HEIGHT	512

#define ROQ_QUAD			0x1000
#define ROQ_QUAD_INFO		0x1001
#define ROQ_CODEBOOK		0x1002
#define ROQ_QUAD_VQ			0x1011
#define ROQ_QUAD_JPEG		0x1012
#define ROQ_QUAD_HANG		0x1013
#define ROQ_PACKET			0x1030
#define ZA_SOUND_MONO		0x1020
#define ZA_SOUND_STEREO		0x1021

#define MAX_VIDEO_HANDLES	16

extern glconfig_t glConfig;
extern	int		s_paintedtime;
extern	int		s_rawend;


static qboolean RoQ_init( void );

/******************************************************************************
*
* Class:		trFMV
*
* Description:	RoQ/RnR manipulation routines
*				not entirely complete for first run
*
******************************************************************************/

static	long				ROQ_YY_tab[256];
static	long				ROQ_UB_tab[256];
static	long				ROQ_UG_tab[256];
static	long				ROQ_VG_tab[256];
static	long				ROQ_VR_tab[256];
static byte vq2[256][2*2*4];
static byte vq4[256][4*4*4];
static byte vq8[256][8*8*4];


typedef struct {
	byte				linbuf[DEFAULT_CIN_WIDTH*DEFAULT_CIN_HEIGHT*4*2];
	byte				file[65536];
	byte scaledFrame[DEFAULT_CIN_WIDTH*DEFAULT_CIN_HEIGHT*4];
	const void *scaledOwner;
	qboolean scaledValid;
	short				sqrTable[256];

	long				mcomp[256];
	byte				*qStatus[2][32768];

	long				oldXOff, oldYOff, oldysize, oldxsize;

	int					currentHandle;
} cinematics_t;

typedef struct {
	char				fileName[MAX_OSPATH];
	int					CIN_WIDTH, CIN_HEIGHT;
	int					xpos, ypos, width, height;
	qboolean			looping, holdAtEnd, dirty, alterGameState, silent, shader;
	fileHandle_t		iFile;
	e_status			status;
	unsigned int		startTime;
	unsigned int		lastTime;
	long				tfps;
	long				RoQPlayed;
	long				ROQSize;
	unsigned int		RoQFrameSize;
	long				onQuad;
	long				numQuads;
	long				samplesPerLine;
	unsigned int		roq_id;
	long				screenDelta;

	long				samplesPerPixel;				// defaults to 2
	byte*				gray;
	unsigned int		xsize, ysize, maxsize, minsize;

	qboolean			half, smootheddouble, hasChunk, streaming;
	long				normalBuffer0;
	long				roq_flags;
	long				roqF0;
	long				roqF1;
	long				t[2];
	long				roqFPS;
	int					playonwalls;
	byte*				buf;
	long				drawX, drawY;
} cin_cache;

static cinematics_t		cin;
static cin_cache		cinTable[MAX_VIDEO_HANDLES];
static int				currentHandle = -1;
static int				CL_handle = -1;

extern int				s_soundtime;		// sample PAIRS
extern int   			s_paintedtime; 		// sample PAIRS


void CIN_CloseAllVideos(void) {
	int		i;

	for ( i = 0 ; i < MAX_VIDEO_HANDLES ; i++ ) {
		if (cinTable[i].fileName[0] != 0 ) {
			CIN_StopCinematic(i);
		}
	}
}


static int CIN_HandleForVideo(void) {
	int		i;

	for ( i = 0 ; i < MAX_VIDEO_HANDLES ; i++ ) {
		if ( cinTable[i].fileName[0] == 0 ) {
			return i;
		}
	}
	Com_Error( ERR_DROP, "CIN_HandleForVideo: none free" );
	return -1;
}


extern int CL_ScaledMilliseconds(void);

//-----------------------------------------------------------------------------
// RllSetupTable
//
// Allocates and initializes the square table.
//
// Parameters:	None
//
// Returns:		Nothing
//-----------------------------------------------------------------------------
static void RllSetupTable()
{
	int z;

	for (z=0;z<128;z++) {
		cin.sqrTable[z] = (short)(z*z);
		cin.sqrTable[z+128] = (short)(-cin.sqrTable[z]);
	}
}



//-----------------------------------------------------------------------------
// RllDecodeMonoToMono
//
// Decode mono source data into a mono buffer.
//
// Parameters:	from -> buffer holding encoded data
//				to ->	buffer to hold decoded data
//				size =	number of bytes of input (= # of shorts of output)
//				signedOutput = 0 for unsigned output, non-zero for signed output
//				flag = flags from asset header
//
// Returns:		Number of samples placed in output buffer
//-----------------------------------------------------------------------------
long RllDecodeMonoToMono(unsigned char *from,short *to,unsigned int size,char signedOutput ,unsigned short flag)
{
	unsigned int z;
	int prev;
	
	if (signedOutput)	
		prev =  flag - 0x8000;
	else 
		prev = flag;

	for (z=0;z<size;z++) {
		prev = to[z] = (short)(prev + cin.sqrTable[from[z]]); 
	}
	return size;	//*sizeof(short));
}


//-----------------------------------------------------------------------------
// RllDecodeMonoToStereo
//
// Decode mono source data into a stereo buffer. Output is 4 times the number
// of bytes in the input.
//
// Parameters:	from -> buffer holding encoded data
//				to ->	buffer to hold decoded data
//				size =	number of bytes of input (= 1/4 # of bytes of output)
//				signedOutput = 0 for unsigned output, non-zero for signed output
//				flag = flags from asset header
//
// Returns:		Number of samples placed in output buffer
//-----------------------------------------------------------------------------
long RllDecodeMonoToStereo(unsigned char *from,short *to,unsigned int size,char signedOutput,unsigned short flag)
{
	unsigned int z;
	int prev;
	
	if (signedOutput)	
		prev =  flag - 0x8000;
	else 
		prev = flag;

	for (z = 0; z < size; z++) {
		prev = (short)(prev + cin.sqrTable[from[z]]);
		to[z*2+0] = to[z*2+1] = (short)(prev);
	}
	
	return size;	// * 2 * sizeof(short));
}


//-----------------------------------------------------------------------------
// RllDecodeStereoToStereo
//
// Decode stereo source data into a stereo buffer.
//
// Parameters:	from -> buffer holding encoded data
//				to ->	buffer to hold decoded data
//				size =	number of bytes of input (= 1/2 # of bytes of output)
//				signedOutput = 0 for unsigned output, non-zero for signed output
//				flag = flags from asset header
//
// Returns:		Number of samples placed in output buffer
//-----------------------------------------------------------------------------
long RllDecodeStereoToStereo(unsigned char *from,short *to,unsigned int size,char signedOutput, unsigned short flag)
{
	unsigned int z;
	unsigned char *zz = from;
	int	prevL, prevR;

	if (signedOutput) {
		prevL = (flag & 0xff00) - 0x8000;
		prevR = ((flag & 0x00ff) << 8) - 0x8000;
	} else {
		prevL = flag & 0xff00;
		prevR = (flag & 0x00ff) << 8;
	}

	for (z=0;z<size;z+=2) {
                prevL = (short)(prevL + cin.sqrTable[*zz++]); 
                prevR = (short)(prevR + cin.sqrTable[*zz++]);
                to[z+0] = (short)(prevL);
                to[z+1] = (short)(prevR);
	}
	
	return (size>>1);	//*sizeof(short));
}


//-----------------------------------------------------------------------------
// RllDecodeStereoToMono
//
// Decode stereo source data into a mono buffer.
//
// Parameters:	from -> buffer holding encoded data
//				to ->	buffer to hold decoded data
//				size =	number of bytes of input (= # of bytes of output)
//				signedOutput = 0 for unsigned output, non-zero for signed output
//				flag = flags from asset header
//
// Returns:		Number of samples placed in output buffer
//-----------------------------------------------------------------------------
long RllDecodeStereoToMono(unsigned char *from,short *to,unsigned int size,char signedOutput, unsigned short flag)
{
	unsigned int z;
	int prevL,prevR;
	
	if (signedOutput) {
		prevL = (flag & 0xff00) - 0x8000;
		prevR = ((flag & 0x00ff) << 8) -0x8000;
	} else {
		prevL = flag & 0xff00;
		prevR = (flag & 0x00ff) << 8;
	}

	for (z=0;z<size;z+=1) {
		prevL= prevL + cin.sqrTable[from[z*2]];
		prevR = prevR + cin.sqrTable[from[z*2+1]];
		to[z] = (short)((prevL + prevR)/2);
	}

	return size;
}

/******************************************************************************
*
* Function:		
*
* Description:	
*
******************************************************************************/

/** Copy complete RGBA rows without assuming native double alignment or pointer aliasing. */
static void RoQCopyBlock( const byte *source, byte *output, int size, int sourceStride, int outputStride ) {
	int row;
	for ( row = 0; row < size; row++ ) {
		memcpy( output + row * outputStride, source + row * sourceStride, size * 4 );
	}
}

typedef struct {
	byte *next, *end;
	unsigned short codes;
	int remaining;
} roqCursor_t;

/** Fetch a complete little-endian control word before consuming one of its eight codes. */
static qboolean RoQReadCode( roqCursor_t *cursor, unsigned int *code ) {
	if ( !cursor->remaining ) {
		if ( cursor->end - cursor->next < 2 ) return qfalse;
		cursor->codes = cursor->next[0] | (unsigned int)cursor->next[1] << 8;
		cursor->next += 2;
		cursor->remaining = 8;
	}
	*code = cursor->codes >> 14;
	cursor->codes <<= 2;
	cursor->remaining--;
	return qtrue;
}

/** Read one checked codebook or motion index. */
static qboolean RoQReadIndex( roqCursor_t *cursor, unsigned int *index ) {
	if ( cursor->next == cursor->end ) return qfalse;
	*index = *cursor->next++;
	return qtrue;
}

/** Check every row and column against one frame half, not merely the combined allocation. */
static qboolean RoQFrameBlock( long offset, int size ) {
	cin_cache *movie = &cinTable[currentHandle];
	long bytes = size * 4;
	return offset >= 0 && !(offset & 3) &&
	       offset % movie->samplesPerLine <= movie->samplesPerLine - bytes &&
	       offset + (size - 1) * movie->samplesPerLine + bytes <= movie->screenDelta;
}

/** Validate a block and all input indices, then optionally apply it after full-frame preflight. */
static qboolean RoQApplyBlock( roqCursor_t *cursor, byte *output, int size, unsigned int code, qboolean write ) {
	cin_cache *movie = &cinTable[currentHandle];
	long half = (movie->numQuads & 1) ? movie->screenDelta : 0;
	long local, sourceLocal;
	unsigned int index[4], i;
	byte *source;
	if ( !output ) return qfalse;
	local = output - cin.linbuf - half;
	if ( !RoQFrameBlock(local, size) ) return qfalse;
	if ( !code ) return qtrue;
	if ( code == 3 ) {
		if ( size != 4 ) return qfalse;
		for ( i = 0; i < 4; i++ ) if ( !RoQReadIndex(cursor, &index[i]) ) return qfalse;
		if ( write ) {
			for ( i = 0; i < 4; i++ ) {
				RoQCopyBlock( vq2[index[i]], output + (i / 2) * 2 * movie->samplesPerLine + (i % 2) * 8, 2, 8, movie->samplesPerLine );
			}
		}
		return qtrue;
	}
	if ( !RoQReadIndex(cursor, &index[0]) ) return qfalse;
	if ( code == 1 ) {
		sourceLocal = local + cin.mcomp[index[0]] - movie->normalBuffer0;
		if ( !RoQFrameBlock(sourceLocal, size) ) return qfalse;
		source = cin.linbuf + (movie->screenDelta - half) + sourceLocal;
		if ( write ) RoQCopyBlock( source, output, size, movie->samplesPerLine, movie->samplesPerLine );
	} else {
		source = size == 8 ? vq8[index[0]] : vq4[index[0]];
		if ( write ) RoQCopyBlock( source, output, size, size * 4, movie->samplesPerLine );
	}
	return qtrue;
}

/** Walk complete 8x8 groups and their four 4x4 children with the same checked cursor in both passes. */
static qboolean RoQVQPass( byte **status, byte *begin, byte *end, qboolean write ) {
	roqCursor_t cursor;
	long root;
	int child;
	unsigned int code;
	memset( &cursor, 0, sizeof(cursor) );
	cursor.next = begin;
	cursor.end = end;
	for ( root = 0; root < cinTable[currentHandle].onQuad; root += 5 ) {
		if ( !RoQReadCode(&cursor, &code) ) return qfalse;
		if ( code == 3 ) {
			for ( child = 1; child <= 4; child++ ) {
				if ( !RoQReadCode(&cursor, &code) || !RoQApplyBlock(&cursor, status[root + child], 4, code, write) ) return qfalse;
			}
		} else if ( !RoQApplyBlock(&cursor, status[root], 8, code, write) ) return qfalse;
	}
	return qtrue;
}

/** Reject the entire frame before writes if any code/index or motion-source rectangle is invalid. */
static qboolean blitVQQuad32fs( byte **status, byte *begin, byte *end ) {
	cin_cache *movie = &cinTable[currentHandle];
	if ( movie->onQuad <= 0 || movie->onQuad % 5 ||
	     movie->onQuad > sizeof(cin.qStatus[0]) / sizeof(cin.qStatus[0][0]) - 64 ||
	     movie->screenDelta <= 0 || movie->screenDelta > sizeof(cin.linbuf) / 2 ||
	     movie->samplesPerLine < 32 || movie->numQuads < 0 ) return qfalse;
	if ( !RoQVQPass(status, begin, end, qfalse) ) return qfalse;
	return RoQVQPass( status, begin, end, qtrue );
}

static void ROQ_GenYUVTables( void )
{
	float t_ub,t_vr,t_ug,t_vg;
	long i;

	t_ub = (1.77200f/2.0f) * (float)(1<<6) + 0.5f;
	t_vr = (1.40200f/2.0f) * (float)(1<<6) + 0.5f;
	t_ug = (0.34414f/2.0f) * (float)(1<<6) + 0.5f;
	t_vg = (0.71414f/2.0f) * (float)(1<<6) + 0.5f;
	for(i=0;i<256;i++) {
		float x = (float)(2 * i - 255);
	
		ROQ_UB_tab[i] = (long)( ( t_ub * x) + (1<<5));
		ROQ_VR_tab[i] = (long)( ( t_vr * x) + (1<<5));
		ROQ_UG_tab[i] = (long)( (-t_ug * x)		 );
		ROQ_VG_tab[i] = (long)( (-t_vg * x) + (1<<5));
		ROQ_YY_tab[i] = (long)( (i << 6) | (i >> 2) );
	}
}

#if defined(MACOS_X)

static inline unsigned int yuv_to_rgb24( long y, long u, long v )
{ 
	long r,g,b,YY;
        
        YY = (long)(ROQ_YY_tab[(y)]);

	r = (YY + ROQ_VR_tab[v]) >> 6;
	g = (YY + ROQ_UG_tab[u] + ROQ_VG_tab[v]) >> 6;
	b = (YY + ROQ_UB_tab[u]) >> 6;
	
	if (r<0) r = 0; if (g<0) g = 0; if (b<0) b = 0;
	if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255;
	
	return ((unsigned int)r<<24)|((unsigned int)g<<16)|((unsigned int)b<<8)|255u;	//+(255<<24));
}

#else
static unsigned int yuv_to_rgb24( long y, long u, long v )
{ 
	long r,g,b,YY = (long)(ROQ_YY_tab[(y)]);

	r = (YY + ROQ_VR_tab[v]) >> 6;
	g = (YY + ROQ_UG_tab[u] + ROQ_VG_tab[v]) >> 6;
	b = (YY + ROQ_UB_tab[u]) >> 6;
	
	if (r<0) r = 0; if (g<0) g = 0; if (b<0) b = 0;
	if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255;
	
	return LittleLong ((unsigned int)r|((unsigned int)g<<8)|((unsigned int)b<<16)|0xff000000u);
}
#endif

/******************************************************************************
*
* Function:		
*
* Description:	
*
******************************************************************************/

/** Validate all codebook input before updates, then build fixed-size byte-oriented RGBA tables. */
static qboolean decodeCodeBook( byte *input, byte *end, unsigned short flags ) {
	unsigned int two = flags >> 8, four = flags & 255, i, j, x, y, pixel;
	unsigned int index[4];
	if ( !two ) two = 256;
	if ( !flags ) four = 256;
	if ( end - input < two * 6 + four * 4 ) return qfalse;
	for ( i = 0; i < two; i++ ) {
		for ( j = 0; j < 4; j++ ) {
			pixel = yuv_to_rgb24( input[j], input[4], input[5] );
			memcpy( vq2[i] + j * 4, &pixel, 4 );
		}
		input += 6;
	}
	for ( i = 0; i < four; i++ ) {
		for ( j = 0; j < 4; j++ ) index[j] = *input++;
		for ( j = 0; j < 4; j++ ) {
			RoQCopyBlock( vq2[index[j]], vq4[i] + (j / 2) * 2 * 16 + (j % 2) * 8, 2, 8, 16 );
		}
		for ( y = 0; y < 8; y++ ) {
			for ( x = 0; x < 8; x++ ) memcpy( vq8[i] + (y * 8 + x) * 4, vq4[i] + ((y / 2) * 4 + x / 2) * 4, 4 );
		}
	}
	return qtrue;
}

static qboolean recurseQuad( long startX, long startY, long quadSize ) {
	cin_cache *movie = &cinTable[currentHandle];
	long offset, end;
	if ( startX + quadSize <= movie->xsize && startY + quadSize <= movie->ysize && quadSize <= MAXSIZE ) {
		offset = startY * movie->samplesPerLine + startX * movie->samplesPerPixel;
		end = offset + (quadSize - 1) * movie->samplesPerLine + quadSize * movie->samplesPerPixel;
		if ( movie->onQuad >= sizeof(cin.qStatus[0]) / sizeof(cin.qStatus[0][0]) - 64 ||
		     offset < 0 || end > movie->screenDelta ) return qfalse;
		cin.qStatus[0][movie->onQuad] = cin.linbuf + offset;
		cin.qStatus[1][movie->onQuad++] = cin.linbuf + movie->screenDelta + offset;
	}
	if ( quadSize == MINSIZE ) return qtrue;
	quadSize >>= 1;
	return recurseQuad(startX, startY, quadSize) &&
	       recurseQuad(startX + quadSize, startY, quadSize) &&
	       recurseQuad(startX, startY + quadSize, quadSize) &&
	       recurseQuad(startX + quadSize, startY + quadSize, quadSize);
}

/** Build complete 8x8/4x4 groups and reserve termination entries without unchecked products. */
static qboolean setupQuad( void ) {
	cin_cache *movie = &cinTable[currentHandle];
	long count, i, x, y;
	count = (movie->xsize / 8) * (movie->ysize / 8) * 5;
	if ( count <= 0 || count > sizeof(cin.qStatus[0]) / sizeof(cin.qStatus[0][0]) - 64 ||
	     movie->screenDelta <= 0 || movie->screenDelta > sizeof(cin.linbuf) / 2 ) return qfalse;
	if ( movie->onQuad == count && movie->ysize == cin.oldysize && movie->xsize == cin.oldxsize ) return qtrue;
	movie->onQuad = 0;
	for ( y = 0; y < movie->ysize; y += 16 ) {
		for ( x = 0; x < movie->xsize; x += 16 ) {
			if ( !recurseQuad(x, y, 16) ) return qfalse;
		}
	}
	if ( movie->onQuad != count ) return qfalse;
	for ( i = count; i < count + 64; i++ ) cin.qStatus[0][i] = cin.qStatus[1][i] = NULL;
	cin.oldysize = movie->ysize;
	cin.oldxsize = movie->xsize;
	return qtrue;
}

/** Choose a supported power-of-two texture size within the source and hardware limits. */
static int RoQDrawSize( unsigned int source, int limit ) {
	int size = 1;
	while ( size <= source / 2 && size <= limit / 2 ) size <<= 1;
	return size;
}

/** Validate geometry before products, frame offsets, quad construction, or cache mutation. */
static qboolean readQuadInfo( byte *data, byte *end ) {
	cin_cache *movie = &cinTable[currentHandle];
	unsigned int width, height, maxsize, minsize, maxPixels;
	int limit;
	if ( end - data < 8 ) return qfalse;
	width = data[0] | (unsigned int)data[1] << 8;
	height = data[2] | (unsigned int)data[3] << 8;
	maxsize = data[4] | (unsigned int)data[5] << 8;
	minsize = data[6] | (unsigned int)data[7] << 8;
	maxPixels = sizeof(cin.linbuf) / (2 * 4);
	if ( width < 8 || height < 8 || (width & 7) || (height & 7) ||
	     height > maxPixels / width || maxsize != MAXSIZE || minsize != MINSIZE ||
	     movie->samplesPerPixel != 4 ) return qfalse;
	movie->xsize = movie->CIN_WIDTH = width;
	movie->ysize = movie->CIN_HEIGHT = height;
	movie->maxsize = maxsize;
	movie->minsize = minsize;
	movie->samplesPerLine = width * 4;
	movie->screenDelta = height * movie->samplesPerLine;
	movie->half = movie->smootheddouble = qfalse;
	movie->t[0] = movie->screenDelta;
	movie->t[1] = -movie->screenDelta;
	cin.scaledValid = qfalse;
	limit = glConfig.maxTextureSize > 0 ? glConfig.maxTextureSize : 256;
	if ( glConfig.hardwareType == GLHW_RAGEPRO && limit > 256 ) limit = 256;
	movie->drawX = RoQDrawSize( width, limit );
	movie->drawY = RoQDrawSize( height, limit );
#if defined(MACOS_X)
	if ( movie->drawX > 256 ) movie->drawX = 256;
	if ( movie->drawY > 256 ) movie->drawY = 256;
#endif
	return setupQuad();
}

static void RoQPrepMcomp( long xoff, long yoff ) 
{
	long i, j, x, y, temp, temp2;

	i=cinTable[currentHandle].samplesPerLine; j=cinTable[currentHandle].samplesPerPixel;
	if ( cinTable[currentHandle].xsize == (cinTable[currentHandle].ysize*4) && !cinTable[currentHandle].half ) { j = j+j; i = i+i; }
	
	for(y=0;y<16;y++) {
		temp2 = (y+yoff-8)*i;
		for(x=0;x<16;x++) {
			temp = (x+xoff-8)*j;
			cin.mcomp[(x*16)+y] = cinTable[currentHandle].normalBuffer0-(temp2+temp);
		}
	}
}

/******************************************************************************
*
* Function:		
*
* Description:	
*
******************************************************************************/

static void initRoQ() 
{
	if (currentHandle < 0) return;

	cinTable[currentHandle].samplesPerPixel = 4;
	ROQ_GenYUVTables();
	RllSetupTable();
}

/******************************************************************************
*
* Function:		
*
* Description:	
*
******************************************************************************/
/*
static byte* RoQFetchInterlaced( byte *source ) {
	int x, *src, *dst;

	if (currentHandle < 0) return NULL;

	src = (int *)source;
	dst = (int *)cinTable[currentHandle].buf2;

	for(x=0;x<256*256;x++) {
		*dst = *src;
		dst++; src += 2;
	}
	return cinTable[currentHandle].buf2;
}
*/
/** Stop malformed files without restarting the same invalid looping movie. */
static void RoQFail( void ) {
	cinTable[currentHandle].looping = qfalse;
	cinTable[currentHandle].holdAtEnd = qfalse;
	cinTable[currentHandle].status = FMV_EOF;
}

/** Close only a stream that was successfully started, including before the first frame. */
static void RoQCloseFile( void ) {
	cin_cache *movie = &cinTable[currentHandle];
	if ( movie->iFile ) {
		if ( movie->streaming ) Sys_EndStreamedFile( movie->iFile );
		FS_FCloseFile( movie->iFile );
		movie->iFile = 0;
	}
	movie->streaming = qfalse;
	cin.scaledValid = qfalse;
}

/** Reopen and validate the current file before starting another playback pass. */
static void RoQReset( void ) {
	cin_cache *movie;
	if ( currentHandle < 0 ) return;
	movie = &cinTable[currentHandle];
	RoQCloseFile();
	movie->ROQSize = FS_FOpenFileRead( movie->fileName, &movie->iFile, qtrue );
	if ( !movie->iFile || movie->ROQSize < 16 ||
	     FS_Read( cin.file, 16, movie->iFile ) != 16 || !RoQ_init() ) {
		RoQFail();
		return;
	}
	Sys_BeginStreamedFile( movie->iFile, 0x10000 );
	movie->streaming = qtrue;
	movie->status = FMV_LOOPED;
}

/******************************************************************************
*
* Function:		
*
* Description:	
*
******************************************************************************/

#define ROQ_MAX_PACKET_DEPTH 16

typedef struct {
	unsigned int id, size;
	unsigned short flags;
	signed char f0, f1;
} roqChunk_t;

/** Decode all four size bytes and reserve room for a chunk header in the file buffer. */
static qboolean RoQChunkHeader( const byte *header, roqChunk_t *chunk ) {
	chunk->id = header[0] | (unsigned int)header[1] << 8;
	chunk->size = header[2] | (unsigned int)header[3] << 8 |
	              (unsigned int)header[4] << 16 | (unsigned int)header[5] << 24;
	chunk->flags = header[6] | (unsigned int)header[7] << 8;
	chunk->f0 = (signed char)header[7];
	chunk->f1 = (signed char)header[6];
	return chunk->size <= sizeof(cin.file) - 8 && chunk->id != 0x1084;
}

/** Keep decoder metadata separate from the number of bytes consumed from disk. */
static void RoQSetChunk( const roqChunk_t *chunk ) {
	cin_cache *movie = &cinTable[currentHandle];
	movie->roq_id = chunk->id;
	movie->RoQFrameSize = chunk->size;
	movie->roq_flags = chunk->flags;
	movie->roqF0 = chunk->f0;
	movie->roqF1 = chunk->f1;
}

/** Read a complete bounded payload; return 0 only at a clean chunk boundary. */
static int RoQReadChunk( roqChunk_t *chunk, byte **begin, byte **end ) {
	cin_cache *movie = &cinTable[currentHandle];
	byte header[8];
	long remaining;
	if ( movie->RoQPlayed < 0 || movie->RoQPlayed > movie->ROQSize ) return -1;
	remaining = movie->ROQSize - movie->RoQPlayed;
	if ( !movie->hasChunk ) {
		if ( !remaining ) return 0;
		if ( remaining < 8 || Sys_StreamedRead(header, 1, 8, movie->iFile) != 8 ||
		     !RoQChunkHeader(header, chunk) ) return -1;
		movie->RoQPlayed += 8;
		remaining -= 8;
		RoQSetChunk( chunk );
	}
	chunk->id = movie->roq_id;
	chunk->size = movie->RoQFrameSize;
	chunk->flags = movie->roq_flags;
	chunk->f0 = movie->roqF0;
	chunk->f1 = movie->roqF1;
	if ( chunk->size > sizeof(cin.file) - 8 || chunk->size > remaining ) return -1;
	/* size=1 makes both the synchronous byte-return and threaded item-return shims report bytes. */
	if ( chunk->size && Sys_StreamedRead(cin.file, 1, chunk->size, movie->iFile) != chunk->size ) return -1;
	movie->RoQPlayed += chunk->size;
	movie->hasChunk = qfalse;
	*begin = cin.file;
	*end = cin.file + chunk->size;
	return 1;
}

/** Check the entire embedded packet before dispatch, with bounded nesting and exact child counts. */
static qboolean RoQCheckPacket( byte *begin, byte *end, unsigned int count, unsigned int depth ) {
	unsigned int i;
	roqChunk_t child;
	if ( depth >= ROQ_MAX_PACKET_DEPTH ) return qfalse;
	for ( i = 0; i < count; i++ ) {
		if ( end - begin < 8 || !RoQChunkHeader(begin, &child) ) return qfalse;
		begin += 8;
		if ( child.size > (unsigned int)(end - begin) ) return qfalse;
		if ( child.id == ROQ_PACKET && !RoQCheckPacket(begin, begin + child.size, child.flags, depth + 1) ) return qfalse;
		begin += child.size;
	}
	return begin == end;
}

/** Derive output capacity before reading stereo pairs or expanding mono samples. */
static qboolean RoQDecodeAudio( byte *begin, byte *end, unsigned int id, unsigned short flags ) {
	short sbuf[32768];
	unsigned int size = end - begin;
	int samples;
	if ( id == ZA_SOUND_MONO ) {
		if ( size > sizeof(sbuf) / (2 * sizeof(sbuf[0])) ) return qfalse;
	} else if ( (size & 1) || size > sizeof(sbuf) / sizeof(sbuf[0]) ) {
		return qfalse;
	}
	if ( cinTable[currentHandle].silent || !size ) return qtrue;
	if ( id == ZA_SOUND_MONO ) {
		samples = RllDecodeMonoToStereo( begin, sbuf, size, 0, flags );
		S_RawSamples( samples, 22050, 2, 1, (byte *)sbuf, 1.0f );
	} else {
		if ( cinTable[currentHandle].numQuads == -1 ) {
			S_Update();
			s_rawend = s_soundtime;
		}
		samples = RllDecodeStereoToStereo( begin, sbuf, size, 0, flags );
		S_RawSamples( samples, 22050, 2, 2, (byte *)sbuf, 1.0f );
	}
	return qtrue;
}

/** Dispatch a checked disk or packet payload; frame/VQ cursor hardening follows separately. */
static qboolean RoQDecodeChunk( const roqChunk_t *chunk, byte *framedata, byte *end, unsigned int depth ) {
	unsigned int i;
	roqChunk_t child;
	if ( chunk->id == ROQ_PACKET ) {
		if ( !RoQCheckPacket(framedata, end, chunk->flags, depth) ) return qfalse;
		for ( i = 0; i < chunk->flags; i++ ) {
			RoQChunkHeader( framedata, &child );
			framedata += 8;
			if ( !RoQDecodeChunk(&child, framedata, framedata + child.size, depth + 1) ) return qfalse;
			framedata += child.size;
		}
		return qtrue;
	}
	RoQSetChunk( chunk );
	switch(cinTable[currentHandle].roq_id) 
	{
		case	ROQ_QUAD_VQ:
			if ( cinTable[currentHandle].numQuads < 0 || cinTable[currentHandle].screenDelta <= 0 || cinTable[currentHandle].onQuad <= 0 ) return qfalse;
			if ((cinTable[currentHandle].numQuads&1)) {
				cinTable[currentHandle].normalBuffer0 = cinTable[currentHandle].t[1];
				RoQPrepMcomp( cinTable[currentHandle].roqF0, cinTable[currentHandle].roqF1 );
				if ( !blitVQQuad32fs(cin.qStatus[1], framedata, end) ) return qfalse;
				cinTable[currentHandle].buf = 	cin.linbuf + cinTable[currentHandle].screenDelta;
			} else {
				cinTable[currentHandle].normalBuffer0 = cinTable[currentHandle].t[0];
				RoQPrepMcomp( cinTable[currentHandle].roqF0, cinTable[currentHandle].roqF1 );
				if ( !blitVQQuad32fs(cin.qStatus[0], framedata, end) ) return qfalse;
				cinTable[currentHandle].buf = 	cin.linbuf;
			}
			if (cinTable[currentHandle].numQuads == 0) {		// first frame
				Com_Memcpy(cin.linbuf+cinTable[currentHandle].screenDelta, cin.linbuf, cinTable[currentHandle].samplesPerLine*cinTable[currentHandle].ysize);
			}
			cinTable[currentHandle].numQuads++;
			cin.scaledValid = qfalse;
			cinTable[currentHandle].dirty = qtrue;
			break;
		case ROQ_CODEBOOK:
			return decodeCodeBook( framedata, end, chunk->flags );
		case ZA_SOUND_MONO:
		case ZA_SOUND_STEREO:
			return RoQDecodeAudio( framedata, end, chunk->id, chunk->flags );
		case	ROQ_QUAD_INFO:
			if ( end - framedata < 8 ) return qfalse;
			if (cinTable[currentHandle].numQuads == -1) {
				if ( !readQuadInfo(framedata, end) ) return qfalse;
				// we need to use CL_ScaledMilliseconds because of the smp mode calls from the renderer
				cinTable[currentHandle].startTime = cinTable[currentHandle].lastTime = CL_ScaledMilliseconds()*com_timescale->value;
			}
			if (cinTable[currentHandle].numQuads != 1) cinTable[currentHandle].numQuads = 0;
			break;
		case ROQ_QUAD_HANG:
			break;
		case	ROQ_QUAD_JPEG:
			break;
		default:
			return qfalse;
	}	
	return qtrue;
}

/** Advance one actual chunk, retaining the final payload and stopping failures without a reset loop. */
static void RoQInterrupt( void ) {
	roqChunk_t chunk;
	byte *begin, *end;
	int result;
	if ( currentHandle < 0 ) return;
	result = RoQReadChunk( &chunk, &begin, &end );
	if ( result < 0 || (result > 0 && !RoQDecodeChunk(&chunk, begin, end, 0)) ) {
		RoQFail();
	} else if ( !result ) {
		if ( cinTable[currentHandle].holdAtEnd ) cinTable[currentHandle].status = FMV_IDLE;
		else if ( cinTable[currentHandle].looping ) RoQReset();
		else cinTable[currentHandle].status = FMV_EOF;
	}
}

/******************************************************************************
*
* Function:		
*
* Description:	
*
******************************************************************************/

/** Validate the exact initial file/header read before exposing any decoder state. */
static qboolean RoQ_init( void ) {
	cin_cache *movie = &cinTable[currentHandle];
	roqChunk_t chunk;
	if ( movie->ROQSize < 16 || cin.file[0] != 0x84 || cin.file[1] != 0x10 ||
	     !RoQChunkHeader(cin.file + 8, &chunk) || chunk.size > movie->ROQSize - 16 ) return qfalse;
	movie->startTime = movie->lastTime = CL_ScaledMilliseconds() * com_timescale->value;
	movie->RoQPlayed = 16;
	movie->roqFPS = cin.file[6] | (unsigned int)cin.file[7] << 8;
	if ( !movie->roqFPS ) movie->roqFPS = 30;
	movie->numQuads = -1;
	movie->hasChunk = qtrue;
	cin.scaledValid = qfalse;
	RoQSetChunk( &chunk );
	return qtrue;
}

/******************************************************************************
*
* Function:		
*
* Description:	
*
******************************************************************************/

static void RoQShutdown( void ) {
	const char *s;

	if ( cinTable[currentHandle].status == FMV_IDLE ) {
		return;
	}
	Com_DPrintf("finished cinematic\n");
	cinTable[currentHandle].status = FMV_IDLE;

	RoQCloseFile();

	if (cinTable[currentHandle].alterGameState) {
		cls.state = CA_DISCONNECTED;
		// we can't just do a vstr nextmap, because
		// if we are aborting the intro cinematic with
		// a devmap command, nextmap would be valid by
		// the time it was referenced
		s = Cvar_VariableString( "nextmap" );
		if ( s[0] ) {
			Cbuf_ExecuteText( EXEC_APPEND, va("%s\n", s) );
			Cvar_Set( "nextmap", "" );
		}
		CL_handle = -1;
	}
	cinTable[currentHandle].fileName[0] = 0;
	currentHandle = -1;
}

/*
==================
SCR_StopCinematic
==================
*/
e_status CIN_StopCinematic(int handle) {
	
	if (handle < 0 || handle>= MAX_VIDEO_HANDLES || !cinTable[handle].fileName[0] || cinTable[handle].status == FMV_EOF) return FMV_EOF;
	currentHandle = handle;

	Com_DPrintf("trFMV::stop(), closing %s\n", cinTable[currentHandle].fileName);

	if (cinTable[currentHandle].alterGameState) {
		if ( cls.state != CA_CINEMATIC ) {
			return cinTable[currentHandle].status;
		}
	}
	cinTable[currentHandle].status = FMV_EOF;
	RoQShutdown();

	return FMV_EOF;
}

/*
==================
SCR_RunCinematic

Fetch and decompress the pending frame
==================
*/


e_status CIN_RunCinematic (int handle)
{
        // bk001204 - init
	int	start = 0;
	int     thisTime = 0;

	if (handle < 0 || handle>= MAX_VIDEO_HANDLES || !cinTable[handle].fileName[0] || cinTable[handle].status == FMV_EOF) return FMV_EOF;

	if (cin.currentHandle != handle) {
		currentHandle = handle;
		cin.currentHandle = currentHandle;
		cinTable[currentHandle].status = FMV_EOF;
		RoQReset();
	}

	if (cinTable[handle].playonwalls < -1)
	{
		return cinTable[handle].status;
	}

	currentHandle = handle;

	if (cinTable[currentHandle].alterGameState) {
		if ( cls.state != CA_CINEMATIC ) {
			return cinTable[currentHandle].status;
		}
	}

	if (cinTable[currentHandle].status == FMV_IDLE) {
		return cinTable[currentHandle].status;
	}

	// we need to use CL_ScaledMilliseconds because of the smp mode calls from the renderer
	thisTime = CL_ScaledMilliseconds()*com_timescale->value;
	if (cinTable[currentHandle].shader && (abs(thisTime - cinTable[currentHandle].lastTime))>100) {
		cinTable[currentHandle].startTime += thisTime - cinTable[currentHandle].lastTime;
	}
	// we need to use CL_ScaledMilliseconds because of the smp mode calls from the renderer
	cinTable[currentHandle].tfps = ((((CL_ScaledMilliseconds()*com_timescale->value) - cinTable[currentHandle].startTime)*3)/100);

	start = cinTable[currentHandle].startTime;
	while(  (cinTable[currentHandle].tfps != cinTable[currentHandle].numQuads)
		&& (cinTable[currentHandle].status == FMV_PLAY) ) 
	{
		RoQInterrupt();
		if (start != cinTable[currentHandle].startTime) {
			// we need to use CL_ScaledMilliseconds because of the smp mode calls from the renderer
		  cinTable[currentHandle].tfps = ((((CL_ScaledMilliseconds()*com_timescale->value)
							  - cinTable[currentHandle].startTime)*3)/100);
			start = cinTable[currentHandle].startTime;
		}
	}

	cinTable[currentHandle].lastTime = thisTime;

	if (cinTable[currentHandle].status == FMV_LOOPED) {
		cinTable[currentHandle].status = FMV_PLAY;
	}

	if (cinTable[currentHandle].status == FMV_EOF) {
	  if (cinTable[currentHandle].looping) {
		RoQReset();
	  } else {
		RoQShutdown();
		return FMV_EOF;
	  }
	}

	return cinTable[handle].status;
}

/*
==================
CL_PlayCinematic

==================
*/
int CIN_PlayCinematic( const char *arg, int x, int y, int w, int h, int systemBits ) {
	char	name[MAX_OSPATH];
	int		i;

	if (strstr(arg, "/") == NULL && strstr(arg, "\\") == NULL) {
		Com_sprintf (name, sizeof(name), "video/%s", arg);
	} else {
		Com_sprintf (name, sizeof(name), "%s", arg);
	}

	if (!(systemBits & CIN_system)) {
		for ( i = 0 ; i < MAX_VIDEO_HANDLES ; i++ ) {
			if (!strcmp(cinTable[i].fileName, name) ) {
				return i;
			}
		}
	}

	// DEBUG: Bypass intro cinematics to fix Mac OS 9 white screen/freeze
	if (strstr(arg, "idlogo") || strstr(arg, "intro")) {
		Sys_LogPrintf("CIN_PlayCinematic: Bypassing intro cinematic '%s' to prevent freeze\n", arg);
		return -1;
	}

	Com_DPrintf("SCR_PlayCinematic( %s )\n", arg);

	Com_Memset(&cin, 0, sizeof(cinematics_t) );
	currentHandle = CIN_HandleForVideo();
	Com_Memset( &cinTable[currentHandle], 0, sizeof(cinTable[currentHandle]) );

	cin.currentHandle = currentHandle;

	strcpy(cinTable[currentHandle].fileName, name);

	cinTable[currentHandle].ROQSize = 0;
	cinTable[currentHandle].ROQSize = FS_FOpenFileRead (cinTable[currentHandle].fileName, &cinTable[currentHandle].iFile, qtrue);

	if ( !cinTable[currentHandle].iFile || cinTable[currentHandle].ROQSize < 16 ) {
		RoQFail();
		RoQShutdown();
		return -1;
	}

	CIN_SetExtents(currentHandle, x, y, w, h);
	CIN_SetLooping(currentHandle, (systemBits & CIN_loop)!=0);

	cinTable[currentHandle].CIN_HEIGHT = DEFAULT_CIN_HEIGHT;
	cinTable[currentHandle].CIN_WIDTH  =  DEFAULT_CIN_WIDTH;
	cinTable[currentHandle].holdAtEnd = (systemBits & CIN_hold) != 0;
	cinTable[currentHandle].alterGameState = (systemBits & CIN_system) != 0;
	cinTable[currentHandle].playonwalls = 1;
	cinTable[currentHandle].silent = (systemBits & CIN_silent) != 0;
	cinTable[currentHandle].shader = (systemBits & CIN_shader) != 0;


	initRoQ();

	if ( FS_Read(cin.file, 16, cinTable[currentHandle].iFile) != 16 || !RoQ_init() ) {
		RoQFail();
		cinTable[currentHandle].alterGameState = qfalse;
		RoQShutdown();
		return -1;
	}
	if (cinTable[currentHandle].alterGameState) {
		// close the menu
		if ( uivm ) {
			VM_Call( uivm, UI_SET_ACTIVE_MENU, UIMENU_NONE );
		}
	} else {
		cinTable[currentHandle].playonwalls = cl_inGameVideo->integer;
	}

	Sys_BeginStreamedFile( cinTable[currentHandle].iFile, 0x10000 );
	cinTable[currentHandle].streaming = qtrue;
	cinTable[currentHandle].status = FMV_PLAY;
	Com_DPrintf( "trFMV::play(), playing %s\n", arg );
	if ( cinTable[currentHandle].alterGameState ) cls.state = CA_CINEMATIC;
	Con_Close();
	s_rawend = s_soundtime;
	return currentHandle;
}

void CIN_SetExtents (int handle, int x, int y, int w, int h) {
	if (handle < 0 || handle>= MAX_VIDEO_HANDLES || cinTable[handle].status == FMV_EOF) return;
	cinTable[handle].xpos = x;
	cinTable[handle].ypos = y;
	cinTable[handle].width = w;
	cinTable[handle].height = h;
	cinTable[handle].dirty = qtrue;
}

void CIN_SetLooping(int handle, qboolean loop) {
	if (handle < 0 || handle>= MAX_VIDEO_HANDLES || cinTable[handle].status == FMV_EOF) return;
	cinTable[handle].looping = loop;
}

/*
==================
SCR_DrawCinematic

==================
*/
/** Cache bounded texture pixels until a new frame, geometry, or playback pass invalidates them. */
static byte *RoQResampleFrame( const cin_cache *movie ) {
	byte *output;
	int x, y, c, sourceX, sourceY, offset;
	qboolean averageFour, averageTwo;
	if ( movie->CIN_WIDTH == movie->drawX && movie->CIN_HEIGHT == movie->drawY ) return NULL;
	if ( cin.scaledValid && cin.scaledOwner == movie ) return cin.scaledFrame;
	output = cin.scaledFrame;
	averageFour = movie->CIN_WIDTH == 512 && movie->CIN_HEIGHT == 512 && movie->drawX == 256 && movie->drawY == 256;
	averageTwo = movie->CIN_WIDTH == 512 && movie->CIN_HEIGHT == 256 && movie->drawX == 256 && movie->drawY == 256;
	for ( y = 0; y < movie->drawY; y++ ) {
		sourceY = (y * movie->CIN_HEIGHT) / movie->drawY;
		for ( x = 0; x < movie->drawX; x++ ) {
			sourceX = (x * movie->CIN_WIDTH) / movie->drawX;
			offset = sourceY * movie->samplesPerLine + sourceX * 4;
			for ( c = 0; c < 4; c++ ) {
				int value = movie->buf[offset + c];
				if ( averageFour ) {
					value = (value + movie->buf[offset + 4 + c] + movie->buf[offset + movie->samplesPerLine + c] + movie->buf[offset + movie->samplesPerLine + 4 + c]) >> 2;
				} else if ( averageTwo ) {
					value = (value + movie->buf[offset + 4 + c]) >> 1;
				}
				output[(y * movie->drawX + x) * 4 + c] = value;
			}
		}
	}
	cin.scaledOwner = movie;
	cin.scaledValid = qtrue;
	return output;
}

/** Draw only a live movie with complete geometry and a texture that matches its source bytes. */
void CIN_DrawCinematic( int handle ) {
	float x, y, w, h;
	byte *scaled;
	cin_cache *movie;
	if ( handle < 0 || handle >= MAX_VIDEO_HANDLES || !cinTable[handle].fileName[0] ) return;
	movie = &cinTable[handle];
	if ( movie->status == FMV_EOF || !movie->buf || movie->screenDelta <= 0 ) return;
	x = movie->xpos; y = movie->ypos; w = movie->width; h = movie->height;
	SCR_AdjustFrom640( &x, &y, &w, &h );
	scaled = RoQResampleFrame( movie );
	re.DrawStretchRaw( x, y, w, h, movie->drawX, movie->drawY, scaled ? scaled : movie->buf, handle, movie->dirty );
	movie->dirty = qfalse;
}

void CL_PlayCinematic_f(void) {
	char	*arg, *s;
	qboolean	holdatend;
	int bits = CIN_system;

	Com_DPrintf("CL_PlayCinematic_f\n");
	if (cls.state == CA_CINEMATIC) {
		SCR_StopCinematic();
	}

	arg = Cmd_Argv( 1 );
	s = Cmd_Argv(2);

	holdatend = qfalse;
	if ((s && s[0] == '1') || Q_stricmp(arg,"demoend.roq")==0 || Q_stricmp(arg,"end.roq")==0) {
		bits |= CIN_hold;
	}
	if (s && s[0] == '2') {
		bits |= CIN_loop;
	}

	S_StopAllSounds ();

	CL_handle = CIN_PlayCinematic( arg, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, bits );
	if (CL_handle >= 0) {
		do {
			SCR_RunCinematic();
		} while (CL_handle >= 0 && CL_handle < MAX_VIDEO_HANDLES &&
		         cinTable[CL_handle].buf == NULL && cinTable[CL_handle].status == FMV_PLAY);		// wait for first frame (load codebook and sound)
	}
}


void SCR_DrawCinematic (void) {
	if (CL_handle >= 0 && CL_handle < MAX_VIDEO_HANDLES) {
        static int cinLogCount = 0;
        if (cinLogCount < 20) {
            Sys_LogPrintf("SCR_DrawCinematic: Drawing cinematic handle=%d\n", CL_handle);
            cinLogCount++;
        }
		CIN_DrawCinematic(CL_handle);
	} else {
        static int cinFailLogCount = 0;
        if (cinFailLogCount < 20) {
            Sys_LogPrintf("SCR_DrawCinematic: Invalid handle %d\n", CL_handle);
            cinFailLogCount++;
        }
    }
}

void SCR_RunCinematic (void)
{
	if (CL_handle >= 0 && CL_handle < MAX_VIDEO_HANDLES) {
		CIN_RunCinematic(CL_handle);
	}
}

void SCR_StopCinematic(void) {
	if (CL_handle >= 0 && CL_handle < MAX_VIDEO_HANDLES) {
		CIN_StopCinematic(CL_handle);
		S_StopAllSounds ();
		CL_handle = -1;
	}
}

/** Upload the same bounded texture pixels and dimensions used by cinematic previews. */
void CIN_UploadCinematic( int handle ) {
	cin_cache *movie;
	byte *scaled;
	if ( handle < 0 || handle >= MAX_VIDEO_HANDLES || !cinTable[handle].fileName[0] ) return;
	movie = &cinTable[handle];
	if ( movie->status == FMV_EOF || !movie->buf || movie->screenDelta <= 0 ) return;
	if ( movie->playonwalls <= 0 && movie->dirty ) {
		if ( movie->playonwalls == 0 ) movie->playonwalls = -1;
		else if ( movie->playonwalls == -1 ) movie->playonwalls = -2;
		else movie->dirty = qfalse;
	}
	scaled = RoQResampleFrame( movie );
	re.UploadCinematic( movie->drawX, movie->drawY, movie->drawX, movie->drawY, scaled ? scaled : movie->buf, handle, movie->dirty );
	if ( cl_inGameVideo->integer == 0 && movie->playonwalls == 1 ) movie->playonwalls--;
}
