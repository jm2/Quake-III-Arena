/* Compile unused image code while native skin tests isolate graphics imports. */
#ifndef Q3_IMAGE_GL_STUB_H
#define Q3_IMAGE_GL_STUB_H
#define __QGL_H__
typedef unsigned int GLuint,GLenum;
typedef int GLint,GLsizei;
typedef float GLfloat;
#define GL_NEAREST 0x2600
#define GL_LINEAR 0x2601
#define GL_NEAREST_MIPMAP_NEAREST 0x2700
#define GL_LINEAR_MIPMAP_NEAREST 0x2701
#define GL_NEAREST_MIPMAP_LINEAR 0x2702
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#define GL_RGB 0x1907
#define GL_RGBA 0x1908
#define GL_RGB4_S3TC 0x83a1
#define GL_RGB5 0x8050
#define GL_RGB8 0x8051
#define GL_RGBA4 0x8056
#define GL_RGBA8 0x8058
#define GL_REPEAT 0x2901
#define GL_CLAMP 0x2900
#define GL_TEXTURE_2D 0x0de1
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_TEXTURE_BORDER_COLOR 0x1004
#define GL_UNSIGNED_BYTE 0x1401
extern void (*qglActiveTextureARB)(GLenum);
extern void (*qglBindTexture)(GLenum,GLuint);
extern void (*qglDeleteTextures)(GLsizei,const GLuint *);
extern void (*qglTexImage2D)(GLenum,GLint,GLint,GLsizei,GLsizei,GLint,GLenum,GLenum,const void *);
extern void (*qglTexParameterf)(GLenum,GLenum,GLfloat);
extern void (*qglTexParameterfv)(GLenum,GLenum,const GLfloat *);
#endif
