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
// win_glimp.c
// This file contains ALL Win32 specific stuff having to do with the OpenGL refresh
//

#include "../renderer/rb_local.h"
#include "win_local.h"
#include "resource.h"
#include "wglext.h"

/*
=============================================================================

	FRAME SETUP

=============================================================================
*/

/*
=================
GLimp_BeginFrame
=================
*/
void GLimp_BeginFrame()
{
	if (gl_bitdepth->modified)
	{
		if (gl_bitdepth->intVal > 0 && !sys_winInfo.bppChangeAllowed)
		{
			ri.config.vidBitDepth = sys_winInfo.desktopBPP;

			Cvar_VariableSetValue(gl_bitdepth, 0, true);
			Com_Printf(PRNT_WARNING, "gl_bitdepth requires Win95 OSR2.x or WinNT 4.x -- forcing to 0\n");
		}

		gl_bitdepth->modified = false;
	}

	if (ri.cameraSeparation < 0)
		glDrawBuffer(GL_BACK_LEFT);
	else if (ri.cameraSeparation > 0)
		glDrawBuffer(GL_BACK_RIGHT);
	else
		glDrawBuffer(GL_BACK);
}


/*
=================
GLimp_EndFrame

Responsible for doing a swapbuffers and possibly for other stuff as yet to be determined.
Probably better not to make this a GLimp function and instead do a call to GLimp_SwapBuffers.

Only error check if active, and don't swap if not active an you're in fullscreen
=================
*/
void GLimp_EndFrame()
{
	// Update the swap interval
	if (r_swapInterval->modified)
	{
		r_swapInterval->modified = false;
		if (!ri.config.stereoEnabled && ri.config.ext.bWinSwapInterval)
			qwglSwapIntervalEXT(r_swapInterval->intVal);
	}

	// Check for errors
	if (sys_winInfo.appActive)
	{
#ifndef _DEBUG
		if (gl_errorcheck->intVal)
#endif
			RB_CheckForError("GLimp_EndFrame");
	}
	else if (ri.config.vidFullScreen)
	{
		Sleep(0);
		return;
	}

	// Swap buffers
	if (Q_stricmp(gl_drawbuffer->string, "GL_BACK") == 0)
	{
		SwapBuffers(sys_winInfo.hDC);
	}

	// Conserve CPU
	Sleep(0);
}

/*
=============================================================================

	MISC

=============================================================================
*/

/*
=================
GLimp_AppActivate
=================
*/
void GLimp_AppActivate(const bool bActive)
{
	if (bActive)
	{
		SetForegroundWindow(sys_winInfo.hWnd);
		ShowWindow(sys_winInfo.hWnd, SW_SHOW);

		if (ri.config.vidFullScreen)
			ChangeDisplaySettings(&sys_winInfo.windowDM, CDS_FULLSCREEN);
	}
	else
	{
		if (ri.config.vidFullScreen)
		{
			ShowWindow(sys_winInfo.hWnd, SW_MINIMIZE);
			ChangeDisplaySettings(NULL, 0);
		}
	}
}


/*
=================
GLimp_GetGammaRamp
=================
*/
bool GLimp_GetGammaRamp(uint16 *ramp)
{
	if (qwglGetDeviceGammaRamp3DFX)
	{
		if (qwglGetDeviceGammaRamp3DFX(sys_winInfo.hDC, ramp))
			return true;
	}

	if (GetDeviceGammaRamp(sys_winInfo.hDC, ramp))
		return true;

	return false;
}


/*
=================
GLimp_SetGammaRamp
=================
*/
void GLimp_SetGammaRamp(uint16 *ramp)
{
	if (qwglSetDeviceGammaRamp3DFX)
		qwglSetDeviceGammaRamp3DFX(sys_winInfo.hDC, ramp);
	else
		SetDeviceGammaRamp(sys_winInfo.hDC, ramp);
}

/*
===========
GLimp_GetProcAddress
===========
*/
void *GLimp_GetProcAddress(const char *procName)
{
	return wglGetProcAddress(procName);
}

/*
=============================================================================

	INIT / SHUTDOWN

=============================================================================
*/

static bool WGLisExtensionSupported(const char *extension)
{
	const size_t extlen = strlen(extension);
	const char *supported = NULL;

	// Try To Use wglGetExtensionStringARB On Current DC, If Possible
	PROC wglGetExtString = wglGetProcAddress("wglGetExtensionsStringARB");

	if (!wglGetExtString)
		wglGetExtString = wglGetProcAddress("wglGetExtensionsStringEXT");

	if (!wglGetExtString)
		wglGetExtString = wglGetProcAddress("wglGetExtensionsString");

	if (wglGetExtString)
		supported = ((char*(__stdcall*)(HDC))wglGetExtString)(wglGetCurrentDC());

	// If That Failed, Try Standard Opengl Extensions String
	if (supported == NULL)
		supported = (const char*)glGetString(GL_EXTENSIONS);

	// If That Failed Too, Must Be No Extensions Supported
	if (supported == NULL)
		return false;

	// Begin Examination At Start Of String, Increment By 1 On False Match
	for (const char* p = supported; ; p++)
	{
		// Advance p Up To The Next Possible Match
		p = strstr(p, extension);

		if (p == NULL)
			return false;						// No Match

		// Make Sure That Match Is At The Start Of The String Or That
		// The Previous Char Is A Space, Or Else We Could Accidentally
		// Match "wglFunkywglExtension" With "wglExtension"

		// Also, Make Sure That The Following Character Is Space Or NULL
		// Or Else "wglExtensionTwo" Might Match "wglExtension"
		if ((p==supported || p[-1]==' ') && (p[extlen]=='\0' || p[extlen]==' '))
			return true;						// Match
	}
}

static bool arbMultisampleSupported = false;
static int arbMultisampleFormat = -1;
static bool testedForMultisampling = false;
static int lastSamples = -1, passedSamples;

static bool InitMultisample(HINSTANCE hInstance,HWND hWnd,PIXELFORMATDESCRIPTOR &pfd,
	const int colorBits, const int depthBits, const int alphaBits, const int stencilBits)
{  
	testedForMultisampling = true;
	passedSamples = 0;

	// See If The String Exists In WGL!
	if (!WGLisExtensionSupported("WGL_ARB_multisample"))
	{
		arbMultisampleSupported=false;
		return false;
	}

	// Get Our Pixel Format
	PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB =
		(PFNWGLCHOOSEPIXELFORMATARBPROC)wglGetProcAddress("wglChoosePixelFormatARB");

	if (!wglChoosePixelFormatARB)
	{
		// We Didn't Find Support For Multisampling, Set Our Flag And Exit Out.
		arbMultisampleSupported=false;
		return false;
	}

	// Get Our Current Device Context. We Need This In Order To Ask The OpenGL Window What Attributes We Have
	HDC hDC = GetDC(hWnd);

	int pixelFormat;
	bool valid;
	UINT numFormats;
	float fAttributes[] = {0,0};
	int samples = Q_NearestPow<int>(r_multisamples->intVal, true);
	lastSamples = r_multisamples->intVal;
	passedSamples = lastSamples;

	// These Attributes Are The Bits We Want To Test For In Our Sample
	// Everything Is Pretty Standard, The Only One We Want To 
	// Really Focus On Is The SAMPLE BUFFERS ARB And WGL SAMPLES
	// These Two Are Going To Do The Main Testing For Whether Or Not
	// We Support Multisampling On This Hardware
	int iAttributes[] = { WGL_DRAW_TO_WINDOW_ARB,GL_TRUE,
		WGL_SUPPORT_OPENGL_ARB,GL_TRUE,
		WGL_ACCELERATION_ARB,WGL_FULL_ACCELERATION_ARB,
		WGL_COLOR_BITS_ARB,colorBits,
		WGL_ALPHA_BITS_ARB,alphaBits,
		WGL_DEPTH_BITS_ARB,depthBits,
		WGL_STENCIL_BITS_ARB,stencilBits,
		WGL_DOUBLE_BUFFER_ARB,GL_TRUE,
		WGL_SAMPLE_BUFFERS_ARB,GL_TRUE,
		WGL_SAMPLES_ARB, samples ,						// Check For 4x Multisampling
		0,0};

	// First We Check To See If We Can Get A Pixel Format For 4 Samples
	valid = wglChoosePixelFormatARB(hDC,iAttributes,fAttributes,1,&pixelFormat,&numFormats) == 1;

	// if We Returned True, And Our Format Count Is Greater Than 1
	if (valid && numFormats >= 1)
	{
		arbMultisampleSupported	= true;
		arbMultisampleFormat	= pixelFormat;	
		return arbMultisampleSupported;
	}

	// Our Pixel Format With 4 Samples Failed, Test For 2 Samples
	iAttributes[19] = 2;
	valid = wglChoosePixelFormatARB(hDC,iAttributes,fAttributes,1,&pixelFormat,&numFormats) == 1;
	if (valid && numFormats >= 1)
	{
		passedSamples = 2;
	arbMultisampleSupported	= true;
		arbMultisampleFormat	= pixelFormat;	 
		return arbMultisampleSupported;
	}

	// Return The Valid Format
	return  arbMultisampleSupported;
}

/*
=================
GLimp_SetupPFD
=================
*/
static int GLimp_SetupPFD(PIXELFORMATDESCRIPTOR *pfd, const int colorBits, const int depthBits, const int alphaBits, const int stencilBits)
{
	// Fill out the PFD
	pfd->nSize			= sizeof(PIXELFORMATDESCRIPTOR);
	pfd->nVersion		= 1;
	pfd->dwFlags		= PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd->iPixelType		= PFD_TYPE_RGBA;

	pfd->cColorBits		= colorBits;
	pfd->cRedBits		= 0;
	pfd->cRedShift		= 0;
	pfd->cGreenBits		= 0;
	pfd->cGreenShift	= 0;
	pfd->cBlueBits		= 0;
	pfd->cBlueShift		= 0;

	pfd->cAlphaBits		= alphaBits;
	pfd->cAlphaShift	= 0;

	pfd->cAccumBits		= 0;
	pfd->cAccumRedBits	= 0;
	pfd->cAccumGreenBits= 0;
	pfd->cAccumBlueBits	= 0;
	pfd->cAccumAlphaBits= 0;

	pfd->cDepthBits		= depthBits;
	pfd->cStencilBits	= stencilBits;

	pfd->cAuxBuffers	= 0;
	pfd->iLayerType		= PFD_MAIN_PLANE;
	pfd->bReserved		= 0;

	pfd->dwLayerMask	= 0;
	pfd->dwVisibleMask	= 0;
	pfd->dwDamageMask	= 0;

	Com_Printf(0, "...PFD(c%d a%d z%d s%d):\n",
		colorBits, alphaBits, depthBits, stencilBits);

	// Set PFD_STEREO if necessary
	if (cl_stereo->intVal)
	{
		Com_DevPrintf(0, "...attempting to use stereo pfd\n");
		pfd->dwFlags |= PFD_STEREO;
		ri.config.stereoEnabled = true;
	}
	else
	{
		Com_DevPrintf(0, "...not attempting to use stereo pfd\n");
		ri.config.stereoEnabled = false;
	}

	// Choose a pixel format
	int iPixelFormat = (arbMultisampleFormat != -1) ? arbMultisampleFormat : ChoosePixelFormat(sys_winInfo.hDC, pfd);
	if (!iPixelFormat)
	{
		Com_Printf(PRNT_ERROR, "...ChoosePixelFormat failed\n");
		return 0;
	}
	else
	{
		Com_DevPrintf(0, "...ChoosePixelFormat succeeded\n");
	}

	if (SetPixelFormat(sys_winInfo.hDC, iPixelFormat, pfd) == false)
	{
		Com_Printf(PRNT_ERROR, "...SetPixelFormat failed\n");
		return 0;
	}
	else
	{
		Com_DevPrintf(0, "...SetPixelFormat succeeded\n");
	}

	DescribePixelFormat(sys_winInfo.hDC, iPixelFormat, sizeof(PIXELFORMATDESCRIPTOR), pfd);

	// Check for hardware acceleration
	if (pfd->dwFlags & PFD_GENERIC_FORMAT)
	{
		if (!gl_allow_software->intVal)
		{
			Com_Printf(PRNT_ERROR, "...no hardware acceleration detected\n");
			return 0;
		}
		else
		{
			Com_Printf(0, "...using software emulation\n");
		}
	}
	else if (pfd->dwFlags & PFD_GENERIC_ACCELERATED)
	{
		Com_Printf(0, "...MCD acceleration found\n");
	}
	else
	{
		Com_Printf(0, "...hardware acceleration detected\n");
	}

	return iPixelFormat;
}


/*
=================
GLimp_GLInit
=================
*/
PIXELFORMATDESCRIPTOR iPFD;
int colorBits, alphaBits, depthBits, stencilBits;
static bool GLimp_GLInit()
{
	Com_DevPrintf(0, "OpenGL init\n");

	// This shouldn't happen
	if (sys_winInfo.hDC)
	{
		Com_Printf(PRNT_ERROR, "...non-NULL hDC exists!\n");
		if (ReleaseDC(sys_winInfo.hWnd, sys_winInfo.hDC))
			Com_Printf(0, "...hDC release succeeded\n");
		else
			Com_Printf(PRNT_ERROR, "...hDC release failed\n");
	}

	// Get a DC for the specified window
	if ((sys_winInfo.hDC = GetDC(sys_winInfo.hWnd)) == NULL)
	{
		Com_Printf(PRNT_ERROR, "...GetDC failed\n");
		return false;
	}
	else
	{
		Com_DevPrintf(0, "...GetDC succeeded\n");
	}

	// Alpha bits
	alphaBits = r_alphabits->intVal;

	// Color bits
	colorBits = r_colorbits->intVal;
	if (colorBits == 0)
		colorBits = sys_winInfo.desktopBPP;

	// Depth bits
	depthBits;
	if (r_depthbits->intVal == 0)
	{
		if (colorBits > 16)
			depthBits = 24;
		else
			depthBits = 16;
	}
	else
	{
		depthBits = r_depthbits->intVal;
	}

	// Stencil bits
	stencilBits = r_stencilbits->intVal;
	if (!gl_stencilbuffer->intVal)
		stencilBits = 0;
	else if (depthBits < 24)
		stencilBits = 0;

	// Setup the PFD
	int iPixelFormat = GLimp_SetupPFD(&iPFD, colorBits, depthBits, alphaBits, stencilBits);
	if (!iPixelFormat)
	{
		// Don't needlessly try again
		if (colorBits == sys_winInfo.desktopBPP && alphaBits == 0 && stencilBits == 0)
		{
			Com_Printf(PRNT_ERROR, "...failed to find a decent pixel format\n");
			return false;
		}

		// Attempt two, no alpha and no stencil bits
		Com_Printf(PRNT_ERROR, "...first attempt failed, trying again\n");

		colorBits = sys_winInfo.desktopBPP;
		if (r_depthbits->intVal == 0)
		{
			if (colorBits > 16)
				depthBits = 24;
			else
				depthBits = 16;
		}
		else
		{
			depthBits = r_depthbits->intVal;
		}
		alphaBits = 0;
		stencilBits = 0;

		iPixelFormat = GLimp_SetupPFD(&iPFD, colorBits, depthBits, alphaBits, stencilBits);
		if (!iPixelFormat)
		{
			Com_Printf(PRNT_ERROR, "...failed to find a decent pixel format\n");
			return false;
		}
	}

	// Report if stereo is desired but unavailable
	if (cl_stereo->intVal)
	{
		if (iPFD.dwFlags & PFD_STEREO)
		{
			Com_DevPrintf(0, "...stereo pfd succeeded\n");
		}
		else
		{
			Com_Printf(PRNT_ERROR, "...stereo pfd failed\n");
			Cvar_VariableSetValue(cl_stereo, 0, true);
			ri.config.stereoEnabled = false;
		}
	}

	// Startup the OpenGL subsystem by creating a context
	if ((sys_winInfo.hGLRC = wglCreateContext(sys_winInfo.hDC)) == 0)
	{
		Com_Printf(PRNT_ERROR, "...wglCreateContext failed\n");
		return false;
	}
	else
	{
		Com_DevPrintf(0, "...wglCreateContext succeeded\n");
	}

	// Make the new context current
	if (!wglMakeCurrent(sys_winInfo.hDC, sys_winInfo.hGLRC))
	{
		Com_Printf(PRNT_ERROR, "...wglMakeCurrent failed\n");
		return false;
	}
	else
	{
		Com_DevPrintf(0, "...wglMakeCurrent succeeded\n");
	}

	ri.cMultiSamples = passedSamples;
	ri.cColorBits = iPFD.cColorBits;
	ri.cAlphaBits = iPFD.cAlphaBits;
	ri.cDepthBits = iPFD.cDepthBits;
	ri.cStencilBits = iPFD.cStencilBits;

	// Print out PFD specifics
	Com_Printf(0, "----------------------------------------\n");
	Com_Printf(0, "GL_PFD #%i: c(%d-bits) a(%d-bits) z(%d-bit) s(%d-bit) samp(%d)\n",
				iPixelFormat,
				ri.cColorBits, ri.cAlphaBits, ri.cDepthBits, ri.cStencilBits, ri.cMultiSamples);

	return true;
}


/*
=================
GLimp_CreateWindow
=================
*/
LRESULT CALLBACK MainWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static bool GLimp_CreateWindow(const bool fullScreen, const int width, const int height)
{
	bool isMultisampled = false;
	arbMultisampleFormat = -1;

doItAgain:
	// Register the window class if needed
	if (!sys_winInfo.classRegistered)
	{
		WNDCLASS		wClass;

		memset (&wClass, 0, sizeof(wClass));

		wClass.style			= CS_HREDRAW|CS_VREDRAW;
		wClass.lpfnWndProc		= (WNDPROC)MainWndProc;
		wClass.cbClsExtra		= 0;
		wClass.cbWndExtra		= 0;
		wClass.hInstance		= sys_winInfo.hInstance;
		wClass.hIcon			= LoadIcon(sys_winInfo.hInstance, MAKEINTRESOURCE(IDI_ICON1));
		wClass.hCursor			= LoadCursor(NULL, IDC_ARROW);
		wClass.hbrBackground	= (HBRUSH)GetStockObject(WHITE_BRUSH);
		wClass.lpszMenuName		= 0;
		wClass.lpszClassName	= WINDOW_CLASS_NAME;

		if (!RegisterClass (&wClass))
		{
			Com_Error(ERR_FATAL, "Couldn't register window class");
			return false;
		}
		Com_DevPrintf(0, "...RegisterClass succeeded\n");
		sys_winInfo.classRegistered = true;
	}

	// Adjust the window
	RECT rect;
	rect.left = 0;
	rect.top = 0;
	rect.right = width;
	rect.bottom = height;

	DWORD exStyle = 0;
	DWORD dwStyle = 0;
	if (fullScreen)
	{
		exStyle |= WS_EX_TOPMOST;
		dwStyle |= WS_POPUP|WS_SYSMENU;
	}
	else
	{
		dwStyle |= WS_TILEDWINDOW|WS_CLIPCHILDREN|WS_CLIPSIBLINGS;

		AdjustWindowRect(&rect, dwStyle, FALSE);
	}

	int outWidth = rect.right - rect.left;
	int outHeight = rect.bottom - rect.top;
	int outXPos = 0;
	int outYPos = 0;

	if (!fullScreen)
	{
		outXPos = vid_xpos->intVal;
		outYPos = vid_ypos->intVal;

		// Clamp
		if (outXPos < 0)
			outXPos = 0;
		if (outYPos < 0)
			outYPos = 0;
		if (outWidth < sys_winInfo.desktopWidth && outHeight < sys_winInfo.desktopHeight)
		{
			if (outXPos+outWidth > sys_winInfo.desktopWidth)
				outXPos = sys_winInfo.desktopWidth - outWidth;
			if (outYPos+outHeight > sys_winInfo.desktopHeight)
				outYPos = sys_winInfo.desktopHeight - outHeight;
		}
	}

	// Create the window
	sys_winInfo.hWnd = CreateWindowEx(
		exStyle,
		WINDOW_CLASS_NAME,		// class name
		WINDOW_APP_NAME,		// app name
		dwStyle | WS_VISIBLE,	// windows style
		outXPos, outYPos,		// x y pos
		outWidth, outHeight,	// width height
		NULL,					// handle to parent
		NULL,					// handle to menu
		sys_winInfo.hInstance,	// app instance
		NULL					// no extra params
	);

	if (!sys_winInfo.hWnd)
	{
		char *buf = NULL;

		UnregisterClass(WINDOW_CLASS_NAME, sys_winInfo.hInstance);
		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
						NULL,
						GetLastError (),
						MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT),
						(LPTSTR) &buf,
						0,
						NULL);
		Com_Error(ERR_FATAL, "Couldn't create window\nGetLastError: %s", buf);
		return false;
	}
	Com_DevPrintf(0, "...CreateWindow succeeded\n");

	// Init all the OpenGL stuff for the window
	if (!GLimp_GLInit())
	{
		if (sys_winInfo.hGLRC)
		{
			wglDeleteContext(sys_winInfo.hGLRC);
			sys_winInfo.hGLRC = NULL;
		}

		if (sys_winInfo.hDC)
		{
			ReleaseDC(sys_winInfo.hWnd, sys_winInfo.hDC);
			sys_winInfo.hDC = NULL;
		}

		Com_Printf(PRNT_ERROR, "OpenGL initialization failed!\n");
		return false;
	}

	// see if we wanted multisampling
	if (r_multisamples->intVal != 0 && ((lastSamples != r_multisamples->intVal) || !testedForMultisampling))
		InitMultisample(sys_winInfo.hInstance, sys_winInfo.hWnd, iPFD, colorBits, depthBits, alphaBits, stencilBits);

	if (!isMultisampled && arbMultisampleSupported && r_multisamples->intVal)
	{
		wglMakeCurrent (sys_winInfo.hDC, 0);
		wglDeleteContext(sys_winInfo.hGLRC);
		sys_winInfo.hGLRC = 0;

		ReleaseDC(sys_winInfo.hWnd, sys_winInfo.hDC);
		sys_winInfo.hDC = 0;
		
		DestroyWindow(sys_winInfo.hWnd);
		sys_winInfo.hWnd = 0;

		isMultisampled = true;
		goto doItAgain;
	}

	// Show the window
	ShowWindow(sys_winInfo.hWnd, SW_SHOWNORMAL);
	UpdateWindow(sys_winInfo.hWnd);

	SetForegroundWindow(sys_winInfo.hWnd);
	SetFocus(sys_winInfo.hWnd);
	return true;
}


/*
=================
GLimp_Shutdown

This routine does all OS specific shutdown procedures for the OpenGL
subsystem. Under OpenGL this means NULLing out the current DC and
HGLRC, deleting the rendering context, and releasing the DC acquired
for the window. The state structure is also nulled out.
=================
*/
void GLimp_Shutdown(const bool full)
{
	Com_Printf(0, "OpenGL shutdown:\n");

	// Set the current context to NULL
	if (wglMakeCurrent(NULL, NULL))
		Com_Printf(0, "...wglMakeCurrent(0, 0) succeeded\n");
	else
		Com_Printf(PRNT_ERROR, "...wglMakeCurrent(0, 0) failed\n");

	// Delete the OpenGL context
	if (sys_winInfo.hGLRC)
	{
		if (wglDeleteContext(sys_winInfo.hGLRC))
			Com_Printf(0, "...context deletion succeeded\n");
		else
			Com_Printf(PRNT_ERROR, "...context deletion failed\n");
		sys_winInfo.hGLRC = NULL;
	}

	// Release the hDC
	if (sys_winInfo.hDC)
	{
		if (ReleaseDC(sys_winInfo.hWnd, sys_winInfo.hDC))
			Com_Printf(0, "...hDC release succeeded\n");
		else
			Com_Printf(PRNT_ERROR, "...hDC release failed\n");
		sys_winInfo.hDC = NULL;
	}

	// Destroy the window
	if (sys_winInfo.hWnd)
	{
		Com_Printf(0, "...destroying the window\n");
		ShowWindow(sys_winInfo.hWnd, SW_HIDE);
		DestroyWindow(sys_winInfo.hWnd);
		sys_winInfo.hWnd = NULL;
	}

	// Reset display settings
	if (sys_winInfo.cdsFS)
	{
		Com_Printf(0, "...resetting display settings\n");
		ChangeDisplaySettings(NULL, 0);
		sys_winInfo.cdsFS = false;
	}

	// Unregister the window
	if (full)
	{
		Com_Printf(0, "...unregistering the window\n");
		UnregisterClass(WINDOW_CLASS_NAME, sys_winInfo.hInstance);
		sys_winInfo.classRegistered = false;
	}
}


/*
=================
GLimp_Init

This routine is responsible for initializing the OS specific portions of Opengl under Win32
this means dealing with the pixelformats and doing the wgl interface stuff.
=================
*/
#define OSR2_BUILD_NUMBER 1111
bool GLimp_Init()
{
	sys_winInfo.bppChangeAllowed = false;

	OSVERSIONINFO vinfo;
	vinfo.dwOSVersionInfoSize = sizeof(vinfo);
	if (GetVersionEx(&vinfo))
	{
		if (vinfo.dwMajorVersion > 4)
		{
			sys_winInfo.bppChangeAllowed = true;
		}
		else if (vinfo.dwMajorVersion == 4)
		{
			if (vinfo.dwPlatformId == VER_PLATFORM_WIN32_NT)
			{
				sys_winInfo.bppChangeAllowed = true;
			}
			else if (vinfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS)
			{
				if (LOWORD (vinfo.dwBuildNumber) >= OSR2_BUILD_NUMBER)
					sys_winInfo.bppChangeAllowed = true;
			}
		}
	}
	else
	{
		Com_Printf(PRNT_ERROR, "GLimp_Init: - GetVersionEx failed\n");
		return false;
	}

	return true;
}


/*
=================
GLimp_AttemptMode

Returns true when the a mode change was successful
=================
*/
bool GLimp_AttemptMode(const bool fullScreen, const int width, const int height)
{
	// Get desktop properties
	HDC hdc = GetDC(GetDesktopWindow());
	sys_winInfo.desktopBPP = GetDeviceCaps(hdc, BITSPIXEL);
	sys_winInfo.desktopWidth = GetDeviceCaps(hdc, HORZRES);
	sys_winInfo.desktopHeight = GetDeviceCaps(hdc, VERTRES);
	sys_winInfo.desktopHZ = GetDeviceCaps(hdc, VREFRESH);
	ReleaseDC(GetDesktopWindow(), hdc);

	Com_Printf(0, "Mode: %d x %d %s\n", width, height, fullScreen ? "(fullscreen)" : "(windowed)");

	// Set before calling GLimp_CreateWindow because it can trigger WM_MOVE which may
	// act incorrectly when switching between fullscreen and windowed
	ri.config.vidFullScreen = fullScreen;
	ri.config.vidWidth = width;
	ri.config.vidHeight = height;

	// Attempt fullscreen
	if (fullScreen)
	{
		Com_Printf(0, "...attempting fullscreen mode\n");

		memset(&sys_winInfo.windowDM, 0, sizeof(sys_winInfo.windowDM));
		sys_winInfo.windowDM.dmSize			= sizeof(sys_winInfo.windowDM);
		sys_winInfo.windowDM.dmPelsWidth	= width;
		sys_winInfo.windowDM.dmPelsHeight	= height;
		sys_winInfo.windowDM.dmFields		= DM_PELSWIDTH | DM_PELSHEIGHT;

		// Set display frequency
		if (r_displayFreq->intVal > 0)
		{
			sys_winInfo.windowDM.dmFields |= DM_DISPLAYFREQUENCY;
			sys_winInfo.windowDM.dmDisplayFrequency = r_displayFreq->intVal;

			Com_Printf(0, "...using r_displayFreq of %d\n", r_displayFreq->intVal);
			ri.config.vidFrequency = r_displayFreq->intVal;
		}
		else
		{
			ri.config.vidFrequency = sys_winInfo.desktopHZ;
			Com_Printf(0, "...using desktop frequency: %d\n", ri.config.vidFrequency);
		}

		// Set bit depth
		if (gl_bitdepth->intVal > 0)
		{
			sys_winInfo.windowDM.dmBitsPerPel = gl_bitdepth->intVal;
			sys_winInfo.windowDM.dmFields |= DM_BITSPERPEL;

			Com_Printf(0, "...using gl_bitdepth of %d\n", gl_bitdepth->intVal);
			ri.config.vidBitDepth = gl_bitdepth->intVal;
		}
		else
		{
			ri.config.vidBitDepth = sys_winInfo.desktopBPP;
			Com_Printf(0, "...using desktop depth: %d\n", ri.config.vidBitDepth);
		}

		// ChangeDisplaySettings
		Com_Printf(0, "...calling to ChangeDisplaySettings\n");
		if (ChangeDisplaySettings(&sys_winInfo.windowDM, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
		{
			Com_Printf(PRNT_ERROR, "...fullscreen mode failed\n");
			Com_Printf(0, "...resetting display\n");
			ChangeDisplaySettings(NULL, 0);
			sys_winInfo.cdsFS = false;
			return false;
		}

		// Create the window
		if (!GLimp_CreateWindow(fullScreen, width, height))
		{
			Com_Printf(0, "...resetting display\n");
			ChangeDisplaySettings(NULL, 0);
			sys_winInfo.cdsFS = false;
			return false;
		}

		sys_winInfo.cdsFS = true;
		return true;
	}

	// Create a window (not fullscreen)
	ri.config.vidBitDepth = sys_winInfo.desktopBPP;
	ri.config.vidFrequency = sys_winInfo.desktopHZ;

	// Reset the display
	if (sys_winInfo.cdsFS)
	{
		Com_Printf(0, "...resetting display\n");
		ChangeDisplaySettings(NULL, 0);
		sys_winInfo.cdsFS = false;
	}

	// Create the window
	if (!GLimp_CreateWindow(false, width, height))
	{
		Com_Printf (PRNT_ERROR, "...windowed mode failed\n");
		return false;
	}

	return true;
}
