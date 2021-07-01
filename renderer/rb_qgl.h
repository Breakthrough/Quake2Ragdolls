/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

//
// rb_qgl.h
//

#ifndef __RB_QGL_H__
#define __RB_QGL_H__

#if defined(WIN32)
# include <windows.h>
#endif

#include <GL/gl.h>

#ifdef WIN32
# include "../win32/wglext.h"
#elif defined(__unix__)
# include <GL/glx.h>
#endif

#include "glext.h"

#ifndef APIENTRY
# define APIENTRY
#endif
#ifndef APIENTRYP
# define APIENTRYP APIENTRY *
#endif

#if defined(WIN32)

//
// win32
//

#ifndef WINAPI
# define WINAPI APIENTRY
#endif
#ifndef WINAPIP
# define WINAPIP WINAPI *
#endif

extern BOOL		(WINAPIP qwglSwapIntervalEXT) (int interval);

extern BOOL		(WINAPIP qwglGetDeviceGammaRamp3DFX) (HDC, WORD *);
extern BOOL		(WINAPIP qwglSetDeviceGammaRamp3DFX) (HDC, WORD *);

#endif // WIN32



#if defined(__unix__)

//
// linux
//

//GLX Functions
extern XVisualInfo * (*qglXChooseVisual) (Display *dpy, int screen, int *attribList);
extern GLXContext (*qglXCreateContext) (Display *dpy, XVisualInfo *vis, GLXContext shareList, Bool direct);
extern void		(*qglXDestroyContext) (Display *dpy, GLXContext ctx);
extern Bool		(*qglXMakeCurrent) (Display *dpy, GLXDrawable drawable, GLXContext ctx);
extern void		(*qglXCopyContext) (Display *dpy, GLXContext src, GLXContext dst, GLuint mask);
extern void		(*qglXSwapBuffers) (Display *dpy, GLXDrawable drawable);

#endif // linux



//
// extensions
//

// GL_SGIS_multitexture
#define GL_TEXTURE0_SGIS					0x835E
#define GL_TEXTURE1_SGIS					0x835F

extern void		(APIENTRYP qglSelectTextureSGIS) (GLenum texture);
extern void		(APIENTRYP qglActiveTextureARB) (GLenum texture);
extern void		(APIENTRYP qglClientActiveTextureARB) (GLenum texture);

// GL_EXT/SGI_compiled_vertex_array
extern void		(APIENTRYP qglLockArraysEXT) (int first, int count);
extern void		(APIENTRYP qglUnlockArraysEXT) ();

// GL_EXT_draw_range_elements
extern void		(APIENTRYP qglDrawRangeElementsEXT) (GLenum mode, GLuint count, GLuint start, GLsizei end, GLenum type, const GLvoid *indices);

// GL_ARB_vertex_buffer_object
extern void		(APIENTRYP qglBindBufferARB) (GLenum target, GLuint buffer);
extern void		(APIENTRYP qglDeleteBuffersARB) (GLsizei n, const GLuint *buffers);
extern void		(APIENTRYP qglGenBuffersARB) (GLsizei n, GLuint *buffers);
extern GLboolean (APIENTRYP qglIsBufferARB) (GLuint buffer);
extern void		*(APIENTRYP qglMapBufferARB) (GLenum target, GLenum access);
extern GLboolean (APIENTRYP qglUnmapBufferARB) (GLenum target);
extern void		(APIENTRYP qglBufferDataARB) (GLenum target, GLsizeiptrARB size, const GLvoid *data, GLenum usage);
extern void		(APIENTRYP qglBufferSubDataARB) (GLenum target, GLintptrARB offset, GLsizeiptrARB size, const GLvoid *data);

// GL_ARB_vertex/fragment_program
extern void		(APIENTRYP qglVertexAttrib1dARB) (GLuint, GLdouble);
extern void		(APIENTRYP qglVertexAttrib1dvARB) (GLuint, const GLdouble *);
extern void		(APIENTRYP qglVertexAttrib1fARB) (GLuint, GLfloat);
extern void		(APIENTRYP qglVertexAttrib1fvARB) (GLuint, const GLfloat *);
extern void		(APIENTRYP qglVertexAttrib1sARB) (GLuint, GLshort);
extern void		(APIENTRYP qglVertexAttrib1svARB) (GLuint, const GLshort *);
extern void		(APIENTRYP qglVertexAttrib2dARB) (GLuint, GLdouble, GLdouble);
extern void		(APIENTRYP qglVertexAttrib2dvARB) (GLuint, const GLdouble *);
extern void		(APIENTRYP qglVertexAttrib2fARB) (GLuint, GLfloat, GLfloat);
extern void		(APIENTRYP qglVertexAttrib2fvARB) (GLuint, const GLfloat *);
extern void		(APIENTRYP qglVertexAttrib2sARB) (GLuint, GLshort, GLshort);
extern void		(APIENTRYP qglVertexAttrib2svARB) (GLuint, const GLshort *);
extern void		(APIENTRYP qglVertexAttrib3dARB) (GLuint, GLdouble, GLdouble, GLdouble);
extern void		(APIENTRYP qglVertexAttrib3dvARB) (GLuint, const GLdouble *);
extern void		(APIENTRYP qglVertexAttrib3fARB) (GLuint, GLfloat, GLfloat, GLfloat);
extern void		(APIENTRYP qglVertexAttrib3fvARB) (GLuint, const GLfloat *);
extern void		(APIENTRYP qglVertexAttrib3sARB) (GLuint, GLshort, GLshort, GLshort);
extern void		(APIENTRYP qglVertexAttrib3svARB) (GLuint, const GLshort *);
extern void		(APIENTRYP qglVertexAttrib4NbvARB) (GLuint, const GLbyte *);
extern void		(APIENTRYP qglVertexAttrib4NivARB) (GLuint, const GLint *);
extern void		(APIENTRYP qglVertexAttrib4NsvARB) (GLuint, const GLshort *);
extern void		(APIENTRYP qglVertexAttrib4NubARB) (GLuint, GLubyte, GLubyte, GLubyte, GLubyte);
extern void		(APIENTRYP qglVertexAttrib4NubvARB) (GLuint, const GLubyte *);
extern void		(APIENTRYP qglVertexAttrib4NuivARB) (GLuint, const GLuint *);
extern void		(APIENTRYP qglVertexAttrib4NusvARB) (GLuint, const GLushort *);
extern void		(APIENTRYP qglVertexAttrib4bvARB) (GLuint, const GLbyte *);
extern void		(APIENTRYP qglVertexAttrib4dARB) (GLuint, GLdouble, GLdouble, GLdouble, GLdouble);
extern void		(APIENTRYP qglVertexAttrib4dvARB) (GLuint, const GLdouble *);
extern void		(APIENTRYP qglVertexAttrib4fARB) (GLuint, GLfloat, GLfloat, GLfloat, GLfloat);
extern void		(APIENTRYP qglVertexAttrib4fvARB) (GLuint, const GLfloat *);
extern void		(APIENTRYP qglVertexAttrib4ivARB) (GLuint, const GLint *);
extern void		(APIENTRYP qglVertexAttrib4sARB) (GLuint, GLshort, GLshort, GLshort, GLshort);
extern void		(APIENTRYP qglVertexAttrib4svARB) (GLuint, const GLshort *);
extern void		(APIENTRYP qglVertexAttrib4ubvARB) (GLuint, const GLubyte *);
extern void		(APIENTRYP qglVertexAttrib4uivARB) (GLuint, const GLuint *);
extern void		(APIENTRYP qglVertexAttrib4usvARB) (GLuint, const GLushort *);
extern void		(APIENTRYP qglVertexAttribPointerARB) (GLuint, GLint, GLenum, GLboolean, GLsizei, const GLvoid *);
extern void		(APIENTRYP qglEnableVertexAttribArrayARB) (GLuint);
extern void		(APIENTRYP qglDisableVertexAttribArrayARB) (GLuint);
extern void		(APIENTRYP qglProgramStringARB) (GLenum, GLenum, GLsizei, const GLvoid *);
extern void		(APIENTRYP qglBindProgramARB) (GLenum, GLuint);
extern void		(APIENTRYP qglDeleteProgramsARB) (GLsizei, const GLuint *);
extern void		(APIENTRYP qglGenProgramsARB) (GLsizei, GLuint *);
extern void		(APIENTRYP qglProgramEnvParameter4dARB) (GLenum, GLuint, GLdouble, GLdouble, GLdouble, GLdouble);
extern void		(APIENTRYP qglProgramEnvParameter4dvARB) (GLenum, GLuint, const GLdouble *);
extern void		(APIENTRYP qglProgramEnvParameter4fARB) (GLenum, GLuint, GLfloat, GLfloat, GLfloat, GLfloat);
extern void		(APIENTRYP qglProgramEnvParameter4fvARB) (GLenum, GLuint, const GLfloat *);
extern void		(APIENTRYP qglProgramLocalParameter4dARB) (GLenum, GLuint, GLdouble, GLdouble, GLdouble, GLdouble);
extern void		(APIENTRYP qglProgramLocalParameter4dvARB) (GLenum, GLuint, const GLdouble *);
extern void		(APIENTRYP qglProgramLocalParameter4fARB) (GLenum, GLuint, GLfloat, GLfloat, GLfloat, GLfloat);
extern void		(APIENTRYP qglProgramLocalParameter4fvARB) (GLenum, GLuint, const GLfloat *);
extern void		(APIENTRYP qglGetProgramEnvParameterdvARB) (GLenum, GLuint, GLdouble *);
extern void		(APIENTRYP qglGetProgramEnvParameterfvARB) (GLenum, GLuint, GLfloat *);
extern void		(APIENTRYP qglGetProgramLocalParameterdvARB) (GLenum, GLuint, GLdouble *);
extern void		(APIENTRYP qglGetProgramLocalParameterfvARB) (GLenum, GLuint, GLfloat *);
extern void		(APIENTRYP qglGetProgramivARB) (GLenum, GLenum, GLint *);
extern void		(APIENTRYP qglGetProgramStringARB) (GLenum, GLenum, GLvoid *);
extern void		(APIENTRYP qglGetVertexAttribdvARB) (GLuint, GLenum, GLdouble *);
extern void		(APIENTRYP qglGetVertexAttribfvARB) (GLuint, GLenum, GLfloat *);
extern void		(APIENTRYP qglGetVertexAttribivARB) (GLuint, GLenum, GLint *);
extern void		(APIENTRYP qglGetVertexAttribPointervARB) (GLuint, GLenum, GLvoid* *);
extern GLboolean (APIENTRYP qglIsProgramARB) (GLuint);

// GL_ARB_vertex/fragment_shader
extern void		(APIENTRYP qglBindAttribLocationARB) (GLhandleARB, GLuint, const GLcharARB *);
extern void		(APIENTRYP qglGetActiveAttribARB) (GLhandleARB, GLuint, GLsizei, GLsizei *, GLint *, GLenum *, GLcharARB *);
extern GLint 	(APIENTRYP qglGetAttribLocationARB) (GLhandleARB, const GLcharARB *);

// GL_ARB_shader_objects
extern void		(APIENTRYP qglDeleteObjectARB) (GLhandleARB);
extern GLhandleARB (APIENTRYP qglGetHandleARB) (GLenum);
extern void		(APIENTRYP qglDetachObjectARB) (GLhandleARB, GLhandleARB);
extern GLhandleARB (APIENTRYP qglCreateShaderObjectARB) (GLenum);
extern void		(APIENTRYP qglShaderSourceARB) (GLhandleARB, GLsizei, const GLcharARB* *, const GLint *);
extern void		(APIENTRYP qglCompileShaderARB) (GLhandleARB);
extern GLhandleARB (APIENTRYP qglCreateProgramObjectARB) ();
extern void		(APIENTRYP qglAttachObjectARB) (GLhandleARB, GLhandleARB);
extern void		(APIENTRYP qglLinkProgramARB) (GLhandleARB);
extern void		(APIENTRYP qglUseProgramObjectARB) (GLhandleARB);
extern void		(APIENTRYP qglValidateProgramARB) (GLhandleARB);
extern void		(APIENTRYP qglUniform1fARB) (GLint, GLfloat);
extern void		(APIENTRYP qglUniform2fARB) (GLint, GLfloat, GLfloat);
extern void		(APIENTRYP qglUniform3fARB) (GLint, GLfloat, GLfloat, GLfloat);
extern void		(APIENTRYP qglUniform4fARB) (GLint, GLfloat, GLfloat, GLfloat, GLfloat);
extern void		(APIENTRYP qglUniform1iARB) (GLint, GLint);
extern void		(APIENTRYP qglUniform2iARB) (GLint, GLint, GLint);
extern void		(APIENTRYP qglUniform3iARB) (GLint, GLint, GLint, GLint);
extern void		(APIENTRYP qglUniform4iARB) (GLint, GLint, GLint, GLint, GLint);
extern void		(APIENTRYP qglUniform1fvARB) (GLint, GLsizei, const GLfloat *);
extern void		(APIENTRYP qglUniform2fvARB) (GLint, GLsizei, const GLfloat *);
extern void		(APIENTRYP qglUniform3fvARB) (GLint, GLsizei, const GLfloat *);
extern void		(APIENTRYP qglUniform4fvARB) (GLint, GLsizei, const GLfloat *);
extern void		(APIENTRYP qglUniform1ivARB) (GLint, GLsizei, const GLint *);
extern void		(APIENTRYP qglUniform2ivARB) (GLint, GLsizei, const GLint *);
extern void		(APIENTRYP qglUniform3ivARB) (GLint, GLsizei, const GLint *);
extern void		(APIENTRYP qglUniform4ivARB) (GLint, GLsizei, const GLint *);
extern void		(APIENTRYP qglUniformMatrix2fvARB) (GLint, GLsizei, GLboolean, const GLfloat *);
extern void		(APIENTRYP qglUniformMatrix3fvARB) (GLint, GLsizei, GLboolean, const GLfloat *);
extern void		(APIENTRYP qglUniformMatrix4fvARB) (GLint, GLsizei, GLboolean, const GLfloat *);
extern void		(APIENTRYP qglGetObjectParameterfvARB) (GLhandleARB, GLenum, GLfloat *);
extern void		(APIENTRYP qglGetObjectParameterivARB) (GLhandleARB, GLenum, GLint *);
extern void		(APIENTRYP qglGetInfoLogARB) (GLhandleARB, GLsizei, GLsizei *, GLcharARB *);
extern void		(APIENTRYP qglGetAttachedObjectsARB) (GLhandleARB, GLsizei, GLsizei *, GLhandleARB *);
extern GLint	(APIENTRYP qglGetUniformLocationARB) (GLhandleARB, const GLcharARB *);
extern void		(APIENTRYP qglGetActiveUniformARB) (GLhandleARB, GLuint, GLsizei, GLsizei *, GLint *, GLenum *, GLcharARB *);
extern void		(APIENTRYP qglGetUniformfvARB) (GLhandleARB, GLint, GLfloat *);
extern void		(APIENTRYP qglGetUniformivARB) (GLhandleARB, GLint, GLint *);
extern void		(APIENTRYP qglGetShaderSourceARB) (GLhandleARB, GLsizei, GLsizei *, GLcharARB *);

// GL_EXT_framebuffer_object
extern GLboolean (APIENTRYP qglIsRenderbufferEXT) (GLuint);
extern void		(APIENTRYP qglBindRenderbufferEXT) (GLenum, GLuint);
extern void		(APIENTRYP qglDeleteRenderbuffersEXT) (GLsizei, const GLuint *);
extern void		(APIENTRYP qglGenRenderbuffersEXT) (GLsizei, GLuint *);
extern void		(APIENTRYP qglRenderbufferStorageEXT) (GLenum, GLenum, GLsizei, GLsizei);
extern void		(APIENTRYP qglGetRenderbufferParameterivEXT) (GLenum, GLenum, GLint *);
extern GLboolean (APIENTRYP qglIsFramebufferEXT) (GLuint);
extern void		(APIENTRYP qglBindFramebufferEXT) (GLenum, GLuint);
extern void		(APIENTRYP qglDeleteFramebuffersEXT) (GLsizei, const GLuint *);
extern void		(APIENTRYP qglGenFramebuffersEXT) (GLsizei, GLuint *);
extern GLenum	(APIENTRYP qglCheckFramebufferStatusEXT) (GLenum);
extern void		(APIENTRYP qglFramebufferTexture1DEXT) (GLenum, GLenum, GLenum, GLuint, GLint);
extern void		(APIENTRYP qglFramebufferTexture2DEXT) (GLenum, GLenum, GLenum, GLuint, GLint);
extern void		(APIENTRYP qglFramebufferTexture3DEXT) (GLenum, GLenum, GLenum, GLuint, GLint, GLint);
extern void		(APIENTRYP qglFramebufferRenderbufferEXT) (GLenum, GLenum, GLenum, GLuint);
extern void		(APIENTRYP qglGetFramebufferAttachmentParameterivEXT) (GLenum, GLenum, GLenum, GLint *);
extern void		(APIENTRYP qglGenerateMipmapEXT) (GLenum);

// GL_EXT_texture3D
extern void		(APIENTRYP qglTexImage3D) (GLenum target, GLint level, GLenum internalFormat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
extern void		(APIENTRYP qglTexSubImage3D) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const GLvoid *pixels);

// GL_EXT_stencil_two_side
extern void		(APIENTRYP qglActiveStencilFaceEXT) (GLenum face);

#endif // __RB_QGL_H__
