/* Isolated FreeType API imports for native ownership tests; not a rasterizer. */
#ifndef Q3_FONT_FREETYPE_STUB_H
#define Q3_FONT_FREETYPE_STUB_H
#include <stddef.h>
typedef struct { int unused; } FT_Outline;
typedef struct { long horiBearingX,width,horiBearingY,height,horiAdvance; } FT_Glyph_Metrics;
typedef struct { FT_Glyph_Metrics metrics;int format;FT_Outline outline; } FT_GlyphSlotRec,*FT_GlyphSlot;
typedef struct { FT_GlyphSlot glyph; } FT_FaceRec,*FT_Face;
typedef void *FT_Library;
typedef struct { unsigned int rows,width;int pitch;unsigned char *buffer;unsigned short num_grays;unsigned char pixel_mode; } FT_Bitmap;
#define ft_glyph_format_outline 1
#define ft_pixel_mode_grays 2
#define ft_pixel_mode_mono 1
#define FT_LOAD_DEFAULT 0
int FT_Init_FreeType(FT_Library *library);
int FT_Done_FreeType(FT_Library library);
int FT_New_Memory_Face(FT_Library library,const void *data,long length,long index,FT_Face *face);
int FT_Set_Char_Size(FT_Face face,long width,long height,unsigned int horizontal,unsigned int vertical);
int FT_Done_Face(FT_Face face);
unsigned int FT_Get_Char_Index(FT_Face face,unsigned long character);
int FT_Load_Glyph(FT_Face face,unsigned int index,int flags);
void FT_Outline_Translate(FT_Outline *outline,long x,long y);
int FT_Outline_Get_Bitmap(FT_Library library,FT_Outline *outline,FT_Bitmap *bitmap);
#endif
