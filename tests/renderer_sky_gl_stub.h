/* Native sky fixture keeps graphics calls observable without a context. */
#ifndef Q3_SKY_GL_STUB_H
#define Q3_SKY_GL_STUB_H
#include "renderer_image_gl_stub.h"
#define GL_TRIANGLE_STRIP 0x0005
extern void (*qglBegin)(GLenum);
extern void (*qglEnd)(void);
extern void (*qglTexCoord2fv)(const GLfloat *);
extern void (*qglVertex3fv)(const GLfloat *);
extern void (*qglLoadMatrixf)(const GLfloat *);
extern void (*qglTranslatef)(GLfloat,GLfloat,GLfloat);
extern void (*qglDepthRange)(double,double);
extern void (*qglColor3f)(GLfloat,GLfloat,GLfloat);
extern void (*qglPushMatrix)(void);
extern void (*qglPopMatrix)(void);
#endif
