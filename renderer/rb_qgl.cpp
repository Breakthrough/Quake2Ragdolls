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
// rb_qgl.cpp
//

#ifdef WIN32

# include "rb_local.h"
# include "../win32/win_local.h"

#elif defined __unix__

# include <dlfcn.h>
# include <GL/gl.h>
# include <GL/glx.h>
# include "glext.h"
# include "rb_local.h"
# include "../unix/unix_glimp.h"

#endif

//
// win32
//

#ifdef WIN32
BOOL		(WINAPIP qwglSwapIntervalEXT) (int interval);

BOOL		(WINAPIP qwglGetDeviceGammaRamp3DFX) (HDC hDC, WORD *ramp);
BOOL		(WINAPIP qwglSetDeviceGammaRamp3DFX) (HDC hDC, WORD *ramp);
#endif

//
// unix
//

#ifdef __unix__
XVisualInfo	*(*qglXChooseVisual) (Display *dpy, int screen, int *attribList);
GLXContext	(*qglXCreateContext) (Display *dpy, XVisualInfo *vis, GLXContext shareList, Bool direct);
void		(*qglXDestroyContext) (Display *dpy, GLXContext ctx);
Bool	 	(*qglXMakeCurrent) (Display *dpy, GLXDrawable drawable, GLXContext ctx);
void		(*qglXCopyContext) (Display *dpy, GLXContext src, GLXContext dst, GLuint mask);
void		(*qglXSwapBuffers) (Display *dpy, GLXDrawable drawable);
#endif

//
// extensions
//

// GL_SGIS_multitexture
void		(APIENTRYP qglSelectTextureSGIS) (GLenum texture);
void		(APIENTRYP qglActiveTextureARB) (GLenum texture);
void		(APIENTRYP qglClientActiveTextureARB) (GLenum texture);

// GL_EXT/SGI_compiled_vertex_array
void		(APIENTRYP qglLockArraysEXT) (int first, int count);
void		(APIENTRYP qglUnlockArraysEXT) ();

// GL_EXT_draw_range_elements
void		(APIENTRYP qglDrawRangeElementsEXT) (GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices);

// GL_ARB_vertex_buffer_object
void		(APIENTRYP qglBindBufferARB) (GLenum target, GLuint buffer);
void		(APIENTRYP qglDeleteBuffersARB) (GLsizei n, const GLuint *buffers);
void		(APIENTRYP qglGenBuffersARB) (GLsizei n, GLuint *buffers);
GLboolean	(APIENTRYP qglIsBufferARB) (GLuint buffer);
GLvoid		*(APIENTRYP qglMapBufferARB) (GLenum target, GLenum access);
GLboolean	(APIENTRYP qglUnmapBufferARB) (GLenum target);
void		(APIENTRYP qglBufferDataARB) (GLenum target, GLsizeiptrARB size, const GLvoid *data, GLenum usage);
void		(APIENTRYP qglBufferSubDataARB) (GLenum target, GLintptrARB offset, GLsizeiptrARB size, const GLvoid *data);

// GL_ARB_vertex/fragment_program
void		(APIENTRYP qglVertexAttrib1dARB) (GLuint, GLdouble);
void		(APIENTRYP qglVertexAttrib1dvARB) (GLuint, const GLdouble *);
void		(APIENTRYP qglVertexAttrib1fARB) (GLuint, GLfloat);
void		(APIENTRYP qglVertexAttrib1fvARB) (GLuint, const GLfloat *);
void		(APIENTRYP qglVertexAttrib1sARB) (GLuint, GLshort);
void		(APIENTRYP qglVertexAttrib1svARB) (GLuint, const GLshort *);
void		(APIENTRYP qglVertexAttrib2dARB) (GLuint, GLdouble, GLdouble);
void		(APIENTRYP qglVertexAttrib2dvARB) (GLuint, const GLdouble *);
void		(APIENTRYP qglVertexAttrib2fARB) (GLuint, GLfloat, GLfloat);
void		(APIENTRYP qglVertexAttrib2fvARB) (GLuint, const GLfloat *);
void		(APIENTRYP qglVertexAttrib2sARB) (GLuint, GLshort, GLshort);
void		(APIENTRYP qglVertexAttrib2svARB) (GLuint, const GLshort *);
void		(APIENTRYP qglVertexAttrib3dARB) (GLuint, GLdouble, GLdouble, GLdouble);
void		(APIENTRYP qglVertexAttrib3dvARB) (GLuint, const GLdouble *);
void		(APIENTRYP qglVertexAttrib3fARB) (GLuint, GLfloat, GLfloat, GLfloat);
void		(APIENTRYP qglVertexAttrib3fvARB) (GLuint, const GLfloat *);
void		(APIENTRYP qglVertexAttrib3sARB) (GLuint, GLshort, GLshort, GLshort);
void		(APIENTRYP qglVertexAttrib3svARB) (GLuint, const GLshort *);
void		(APIENTRYP qglVertexAttrib4NbvARB) (GLuint, const GLbyte *);
void		(APIENTRYP qglVertexAttrib4NivARB) (GLuint, const GLint *);
void		(APIENTRYP qglVertexAttrib4NsvARB) (GLuint, const GLshort *);
void		(APIENTRYP qglVertexAttrib4NubARB) (GLuint, GLubyte, GLubyte, GLubyte, GLubyte);
void		(APIENTRYP qglVertexAttrib4NubvARB) (GLuint, const GLubyte *);
void		(APIENTRYP qglVertexAttrib4NuivARB) (GLuint, const GLuint *);
void		(APIENTRYP qglVertexAttrib4NusvARB) (GLuint, const GLushort *);
void		(APIENTRYP qglVertexAttrib4bvARB) (GLuint, const GLbyte *);
void		(APIENTRYP qglVertexAttrib4dARB) (GLuint, GLdouble, GLdouble, GLdouble, GLdouble);
void		(APIENTRYP qglVertexAttrib4dvARB) (GLuint, const GLdouble *);
void		(APIENTRYP qglVertexAttrib4fARB) (GLuint, GLfloat, GLfloat, GLfloat, GLfloat);
void		(APIENTRYP qglVertexAttrib4fvARB) (GLuint, const GLfloat *);
void		(APIENTRYP qglVertexAttrib4ivARB) (GLuint, const GLint *);
void		(APIENTRYP qglVertexAttrib4sARB) (GLuint, GLshort, GLshort, GLshort, GLshort);
void		(APIENTRYP qglVertexAttrib4svARB) (GLuint, const GLshort *);
void		(APIENTRYP qglVertexAttrib4ubvARB) (GLuint, const GLubyte *);
void		(APIENTRYP qglVertexAttrib4uivARB) (GLuint, const GLuint *);
void		(APIENTRYP qglVertexAttrib4usvARB) (GLuint, const GLushort *);
void		(APIENTRYP qglVertexAttribPointerARB) (GLuint, GLint, GLenum, GLboolean, GLsizei, const GLvoid *);
void		(APIENTRYP qglEnableVertexAttribArrayARB) (GLuint);
void		(APIENTRYP qglDisableVertexAttribArrayARB) (GLuint);
void		(APIENTRYP qglProgramStringARB) (GLenum, GLenum, GLsizei, const GLvoid *);
void		(APIENTRYP qglBindProgramARB) (GLenum, GLuint);
void		(APIENTRYP qglDeleteProgramsARB) (GLsizei, const GLuint *);
void		(APIENTRYP qglGenProgramsARB) (GLsizei, GLuint *);
void		(APIENTRYP qglProgramEnvParameter4dARB) (GLenum, GLuint, GLdouble, GLdouble, GLdouble, GLdouble);
void		(APIENTRYP qglProgramEnvParameter4dvARB) (GLenum, GLuint, const GLdouble *);
void		(APIENTRYP qglProgramEnvParameter4fARB) (GLenum, GLuint, GLfloat, GLfloat, GLfloat, GLfloat);
void		(APIENTRYP qglProgramEnvParameter4fvARB) (GLenum, GLuint, const GLfloat *);
void		(APIENTRYP qglProgramLocalParameter4dARB) (GLenum, GLuint, GLdouble, GLdouble, GLdouble, GLdouble);
void		(APIENTRYP qglProgramLocalParameter4dvARB) (GLenum, GLuint, const GLdouble *);
void		(APIENTRYP qglProgramLocalParameter4fARB) (GLenum, GLuint, GLfloat, GLfloat, GLfloat, GLfloat);
void		(APIENTRYP qglProgramLocalParameter4fvARB) (GLenum, GLuint, const GLfloat *);
void		(APIENTRYP qglGetProgramEnvParameterdvARB) (GLenum, GLuint, GLdouble *);
void		(APIENTRYP qglGetProgramEnvParameterfvARB) (GLenum, GLuint, GLfloat *);
void		(APIENTRYP qglGetProgramLocalParameterdvARB) (GLenum, GLuint, GLdouble *);
void		(APIENTRYP qglGetProgramLocalParameterfvARB) (GLenum, GLuint, GLfloat *);
void		(APIENTRYP qglGetProgramivARB) (GLenum, GLenum, GLint *);
void		(APIENTRYP qglGetProgramStringARB) (GLenum, GLenum, GLvoid *);
void		(APIENTRYP qglGetVertexAttribdvARB) (GLuint, GLenum, GLdouble *);
void		(APIENTRYP qglGetVertexAttribfvARB) (GLuint, GLenum, GLfloat *);
void		(APIENTRYP qglGetVertexAttribivARB) (GLuint, GLenum, GLint *);
void		(APIENTRYP qglGetVertexAttribPointervARB) (GLuint, GLenum, GLvoid* *);
GLboolean	(APIENTRYP qglIsProgramARB) (GLuint);

// GL_ARB_vertex/fragment_shader
void		(APIENTRYP qglBindAttribLocationARB) (GLhandleARB, GLuint, const GLcharARB *);
void		(APIENTRYP qglGetActiveAttribARB) (GLhandleARB, GLuint, GLsizei, GLsizei *, GLint *, GLenum *, GLcharARB *);
GLint 		(APIENTRYP qglGetAttribLocationARB) (GLhandleARB, const GLcharARB *);

// GL_ARB_shader_objects
void		(APIENTRYP qglDeleteObjectARB) (GLhandleARB);
GLhandleARB	(APIENTRYP qglGetHandleARB) (GLenum);
void		(APIENTRYP qglDetachObjectARB) (GLhandleARB, GLhandleARB);
GLhandleARB	(APIENTRYP qglCreateShaderObjectARB) (GLenum);
void		(APIENTRYP qglShaderSourceARB) (GLhandleARB, GLsizei, const GLcharARB* *, const GLint *);
void		(APIENTRYP qglCompileShaderARB) (GLhandleARB);
GLhandleARB	(APIENTRYP qglCreateProgramObjectARB) ();
void		(APIENTRYP qglAttachObjectARB) (GLhandleARB, GLhandleARB);
void		(APIENTRYP qglLinkProgramARB) (GLhandleARB);
void		(APIENTRYP qglUseProgramObjectARB) (GLhandleARB);
void		(APIENTRYP qglValidateProgramARB) (GLhandleARB);
void		(APIENTRYP qglUniform1fARB) (GLint, GLfloat);
void		(APIENTRYP qglUniform2fARB) (GLint, GLfloat, GLfloat);
void		(APIENTRYP qglUniform3fARB) (GLint, GLfloat, GLfloat, GLfloat);
void		(APIENTRYP qglUniform4fARB) (GLint, GLfloat, GLfloat, GLfloat, GLfloat);
void		(APIENTRYP qglUniform1iARB) (GLint, GLint);
void		(APIENTRYP qglUniform2iARB) (GLint, GLint, GLint);
void		(APIENTRYP qglUniform3iARB) (GLint, GLint, GLint, GLint);
void		(APIENTRYP qglUniform4iARB) (GLint, GLint, GLint, GLint, GLint);
void		(APIENTRYP qglUniform1fvARB) (GLint, GLsizei, const GLfloat *);
void		(APIENTRYP qglUniform2fvARB) (GLint, GLsizei, const GLfloat *);
void		(APIENTRYP qglUniform3fvARB) (GLint, GLsizei, const GLfloat *);
void		(APIENTRYP qglUniform4fvARB) (GLint, GLsizei, const GLfloat *);
void		(APIENTRYP qglUniform1ivARB) (GLint, GLsizei, const GLint *);
void		(APIENTRYP qglUniform2ivARB) (GLint, GLsizei, const GLint *);
void		(APIENTRYP qglUniform3ivARB) (GLint, GLsizei, const GLint *);
void		(APIENTRYP qglUniform4ivARB) (GLint, GLsizei, const GLint *);
void		(APIENTRYP qglUniformMatrix2fvARB) (GLint, GLsizei, GLboolean, const GLfloat *);
void		(APIENTRYP qglUniformMatrix3fvARB) (GLint, GLsizei, GLboolean, const GLfloat *);
void		(APIENTRYP qglUniformMatrix4fvARB) (GLint, GLsizei, GLboolean, const GLfloat *);
void		(APIENTRYP qglGetObjectParameterfvARB) (GLhandleARB, GLenum, GLfloat *);
void		(APIENTRYP qglGetObjectParameterivARB) (GLhandleARB, GLenum, GLint *);
void		(APIENTRYP qglGetInfoLogARB) (GLhandleARB, GLsizei, GLsizei *, GLcharARB *);
void		(APIENTRYP qglGetAttachedObjectsARB) (GLhandleARB, GLsizei, GLsizei *, GLhandleARB *);
GLint		(APIENTRYP qglGetUniformLocationARB) (GLhandleARB, const GLcharARB *);
void		(APIENTRYP qglGetActiveUniformARB) (GLhandleARB, GLuint, GLsizei, GLsizei *, GLint *, GLenum *, GLcharARB *);
void		(APIENTRYP qglGetUniformfvARB) (GLhandleARB, GLint, GLfloat *);
void		(APIENTRYP qglGetUniformivARB) (GLhandleARB, GLint, GLint *);
void		(APIENTRYP qglGetShaderSourceARB) (GLhandleARB, GLsizei, GLsizei *, GLcharARB *);

// GL_EXT_framebuffer_object
GLboolean	(APIENTRYP qglIsRenderbufferEXT) (GLuint);
void		(APIENTRYP qglBindRenderbufferEXT) (GLenum, GLuint);
void		(APIENTRYP qglDeleteRenderbuffersEXT) (GLsizei, const GLuint *);
void		(APIENTRYP qglGenRenderbuffersEXT) (GLsizei, GLuint *);
void		(APIENTRYP qglRenderbufferStorageEXT) (GLenum, GLenum, GLsizei, GLsizei);
void		(APIENTRYP qglGetRenderbufferParameterivEXT) (GLenum, GLenum, GLint *);
GLboolean	(APIENTRYP qglIsFramebufferEXT) (GLuint);
void		(APIENTRYP qglBindFramebufferEXT) (GLenum, GLuint);
void		(APIENTRYP qglDeleteFramebuffersEXT) (GLsizei, const GLuint *);
void		(APIENTRYP qglGenFramebuffersEXT) (GLsizei, GLuint *);
GLenum		(APIENTRYP qglCheckFramebufferStatusEXT) (GLenum);
void		(APIENTRYP qglFramebufferTexture1DEXT) (GLenum, GLenum, GLenum, GLuint, GLint);
void		(APIENTRYP qglFramebufferTexture2DEXT) (GLenum, GLenum, GLenum, GLuint, GLint);
void		(APIENTRYP qglFramebufferTexture3DEXT) (GLenum, GLenum, GLenum, GLuint, GLint, GLint);
void		(APIENTRYP qglFramebufferRenderbufferEXT) (GLenum, GLenum, GLenum, GLuint);
void		(APIENTRYP qglGetFramebufferAttachmentParameterivEXT) (GLenum, GLenum, GLenum, GLint *);
void		(APIENTRYP qglGenerateMipmapEXT) (GLenum);

// GL_EXT_texture3D
void		(APIENTRYP qglTexImage3D) (GLenum target, GLint level, GLenum internalFormat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
void		(APIENTRYP qglTexSubImage3D) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const GLvoid *pixels);

// GL_EXT_stencil_two_side
void		(APIENTRYP qglActiveStencilFaceEXT) (GLenum face);
