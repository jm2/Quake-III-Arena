
// typedef int sysEventType_t;	// FIXME...
#include "../renderer/tr_local.h"
#include "mac_local.h"
#include <glm.h>
#include "MacGamma.h"

// MAX_DEVICES and macGlInfo moved to mac_local.h

cvar_t			*r_device;


cvar_t			*r_device;
cvar_t			*r_ext_transform_hint;
cvar_t *r_ext_texture_filter_anisotropic;

#ifndef GLM_PAGE_SIZE
#define GLM_PAGE_SIZE 1
#endif
GLint glmGetInteger( GLenum param ) { return 4096; }
glHardwareType_t		sys_hardwareType;
macGlInfo		sys_gl;

void GLimp_EndFrame( void );
static void GLimp_Extensions( void );


void CToPStr(char *cs, Str255 ps)
{
	GLint i, l;
	
	l = strlen(cs);
	if(l > 255) l = 255;
	ps[0] = l;
	for(i = 0; i < l; i++) ps[i + 1] = cs[i];
}

/*
============
CheckErrors
============
*/
void CheckErrors( void ) {		
	GLenum   err;

	err = aglGetError();
	if( err != AGL_NO_ERROR ) {
		ri.Error( ERR_FATAL, "aglGetError: %s", 
			aglErrorString( err ) );
	}
}

//=======================================================================

/*
=====================
GLimp_ResetDisplay
=====================
*/
void GLimp_ResetDisplay( void ) {
	if ( !glConfig.isFullscreen ) {
		return;
	}
	glConfig.isFullscreen = qfalse;
	
	// put the context into the inactive state
	DSpContext_SetState( sys_gl.DSpContext, kDSpContextState_Inactive );

	// release the context
	DSpContext_Release( sys_gl.DSpContext );

	// shutdown draw sprockets
	DSpShutdown();
}

/*
=====================
GLimp_ChangeDisplay
=====================
*/
void GLimp_ChangeDisplay( int *actualWidth, int *actualHeight ) {
	OSStatus				theError;
	DSpContextAttributes 	inAttributes;
	int						colorBits;
	
	// startup DrawSprocket
	theError = DSpStartup();
	if( theError ) {
		ri.Printf( PRINT_ALL, "DSpStartup() failed: %i\n", theError );
		*actualWidth = 640;
		*actualHeight = 480;
		return;
	}	
	
	if ( r_colorbits->integer == 24 || r_colorbits->integer == 32 ) {
		colorBits = kDSpDepthMask_32;
	} else {
		colorBits = kDSpDepthMask_16;
	}
	
	memset( &inAttributes, 0, sizeof( inAttributes ) );
	inAttributes.frequency					= 0;
	inAttributes.displayWidth				= glConfig.vidWidth;
	inAttributes.displayHeight				= glConfig.vidHeight;
	inAttributes.reserved1					= 0;
	inAttributes.reserved2					= 0;
	inAttributes.colorNeeds					= kDSpColorNeeds_Require;
	inAttributes.colorTable					= NULL;
	inAttributes.contextOptions				= 0;
	inAttributes.backBufferDepthMask		= colorBits;
	inAttributes.displayDepthMask			= colorBits;
	inAttributes.backBufferBestDepth		= colorBits;
	inAttributes.displayBestDepth			= colorBits;
	inAttributes.pageCount					= 1;
	inAttributes.gameMustConfirmSwitch		= false;
	inAttributes.reserved3[0]				= 0;
	inAttributes.reserved3[1]				= 0;
	inAttributes.reserved3[2]				= 0;
	inAttributes.reserved3[3]				= 0;

	theError = DSpFindBestContext( &inAttributes, &sys_gl.DSpContext );

	inAttributes.displayWidth				= glConfig.vidWidth;
	inAttributes.displayHeight				= glConfig.vidHeight;
	inAttributes.backBufferDepthMask		= colorBits;
	inAttributes.displayDepthMask			= colorBits;
	inAttributes.backBufferBestDepth		= colorBits;
	inAttributes.displayBestDepth			= colorBits;
	inAttributes.pageCount					= 1;

	theError = DSpContext_Reserve( sys_gl.DSpContext, &inAttributes );

	// find out what res we actually got
	theError = DSpContext_GetAttributes( sys_gl.DSpContext, &inAttributes );
	
	*actualWidth = inAttributes.displayWidth;
	*actualHeight = inAttributes.displayHeight;
	
	// put the context into the active state
	theError = DSpContext_SetState( sys_gl.DSpContext, kDSpContextState_Active );

	// fade back in
	theError = DSpContext_FadeGammaIn( NULL, NULL );

	glConfig.isFullscreen = qtrue;
}

//=======================================================================


/*
===================
GLimp_AglDescribe_f

===================
*/
void GLimp_AglDescribe_f( void ) {
	long		value;
	long		r,g,b,a;
	long		stencil, depth;
	
	ri.Printf( PRINT_ALL, "Selected pixel format 0x%x\n", (int)sys_gl.fmt );
	
	ri.Printf( PRINT_ALL, "TEXTURE_MEMORY: %i\n", sys_gl.textureMemory );
	ri.Printf( PRINT_ALL, "VIDEO_MEMORY: %i\n", sys_gl.videoMemory );
	
	aglDescribePixelFormat(sys_gl.fmt, AGL_RED_SIZE, &r);
	aglDescribePixelFormat(sys_gl.fmt, AGL_GREEN_SIZE, &g);
	aglDescribePixelFormat(sys_gl.fmt, AGL_BLUE_SIZE, &b);
	aglDescribePixelFormat(sys_gl.fmt, AGL_ALPHA_SIZE, &a);
	aglDescribePixelFormat(sys_gl.fmt, AGL_STENCIL_SIZE, &stencil);
	aglDescribePixelFormat(sys_gl.fmt, AGL_DEPTH_SIZE, &depth);
	ri.Printf( PRINT_ALL, "red:%i green:%i blue:%i alpha:%i depth:%i stencil:%i\n",
		r, g, b, a, depth, stencil );

	aglDescribePixelFormat(sys_gl.fmt, AGL_BUFFER_SIZE, &value);
	ri.Printf( PRINT_ALL, "BUFFER_SIZE: %i\n", value );

	aglDescribePixelFormat(sys_gl.fmt, AGL_PIXEL_SIZE, &value);
	ri.Printf( PRINT_ALL, "PIXEL_SIZE: %i\n", value );

	aglDescribePixelFormat(sys_gl.fmt, AGL_RENDERER_ID, &value);
	ri.Printf( PRINT_ALL, "RENDERER_ID: %i\n", value );

	// memory functions
	value = glmGetInteger( GLM_PAGE_SIZE );
	ri.Printf( PRINT_ALL, "GLM_PAGE_SIZE: %i\n", value );
	
	value = glmGetInteger( GLM_NUMBER_PAGES );
	ri.Printf( PRINT_ALL, "GLM_NUMBER_PAGES: %i\n", value );
	
	value = glmGetInteger( GLM_CURRENT_MEMORY );
	ri.Printf( PRINT_ALL, "GLM_CURRENT_MEMORY: %i\n", value );
	
	value = glmGetInteger( GLM_MAXIMUM_MEMORY );
	ri.Printf( PRINT_ALL, "GLM_MAXIMUM_MEMORY: %i\n", value );
	
}

/*
===================
GLimp_AglState_f

===================
*/
void GLimp_AglState_f( void ) {
	char	*cmd;
	int		state, value;
	
	if ( ri.Cmd_Argc() != 3 ) {
		ri.Printf( PRINT_ALL, "Usage: aglstate <parameter> <0/1>\n" );
		return;
	}

	cmd = ri.Cmd_Argv( 1 );
	if ( !Q_stricmp( cmd, "rasterization" ) ) {
		state = AGL_RASTERIZATION;
	} else {
		ri.Printf( PRINT_ALL, "Unknown agl state: %s\n", cmd );
		return;
	}
	
	cmd = ri.Cmd_Argv( 2 );
	value = atoi( cmd );
	
	if ( value ) {
		aglEnable( sys_gl.context, state );
	} else {
		aglDisable( sys_gl.context, state );
	}
}

/*
===================
GLimp_Extensions

===================
*/
static void GLimp_Extensions( void ) {
	const char	*extensions;

	// get our config strings
	Q_strncpyz( glConfig.vendor_string, (const char *)qglGetString (GL_VENDOR), sizeof( glConfig.vendor_string ) );
	Q_strncpyz( glConfig.renderer_string, (const char *)qglGetString (GL_RENDERER), sizeof( glConfig.renderer_string ) );
	Q_strncpyz( glConfig.version_string, (const char *)qglGetString (GL_VERSION), sizeof( glConfig.version_string ) );
	Q_strncpyz( glConfig.extensions_string, (const char *)qglGetString (GL_EXTENSIONS), sizeof( glConfig.extensions_string ) );

	extensions = glConfig.extensions_string;

	// GL_ARB_multitexture
	qglMultiTexCoord2fARB = NULL;
	qglActiveTextureARB = NULL;
	qglClientActiveTextureARB = NULL;

	if ( strstr( extensions, "GL_ARB_multitexture" )  ) {
		if ( r_ext_multitexture->integer && r_allowExtensions->integer ) {
			// qglMultiTexCoord2fARB = glMultiTexCoord2fARB;
			// qglActiveTextureARB = glActiveTextureARB;
			// qglClientActiveTextureARB = glClientActiveTextureARB;

			ri.Printf( PRINT_ALL, "...using GL_ARB_multitexture (Disabled for compatibility)\n" );
		} else {
			ri.Printf( PRINT_ALL, "...ignoring GL_ARB_multitexture\n" );
		}
	} else {
		ri.Printf( PRINT_ALL, "...GL_ARB_multitexture not found\n" );
	}

	// GL_EXT_compiled_vertex_array
	qglLockArraysEXT = NULL;
	qglUnlockArraysEXT = NULL;

	if ( strstr( extensions, "GL_EXT_compiled_vertex_array" ) ) {
		if ( r_ext_compiled_vertex_array->integer && r_allowExtensions->integer ) {
			// qglLockArraysEXT = glLockArraysEXT;
			// qglUnlockArraysEXT = glUnlockArraysEXT;

			ri.Printf( PRINT_ALL, "...using GL_EXT_compiled_vertex_array (Disabled for compatibility)\n" );
		} else {
			ri.Printf( PRINT_ALL, "...ignoring GL_EXT_compiled_vertex_array\n" );
		}
	} else {
		ri.Printf( PRINT_ALL, "...GL_EXT_compiled_vertex_array not found\n" );
	}
	
	// GL_EXT_texture_env_add
	glConfig.textureEnvAddAvailable = qfalse;
	if ( strstr( glConfig.extensions_string, "EXT_texture_env_add" ) ) {
		if ( r_ext_texture_env_add->integer ) {
			glConfig.textureEnvAddAvailable = qtrue;
			ri.Printf( PRINT_ALL, "...using GL_EXT_texture_env_add\n" );
		} else {
			glConfig.textureEnvAddAvailable = qfalse;
			ri.Printf( PRINT_ALL, "...ignoring GL_EXT_texture_env_add\n" );
		}
	} else {
		ri.Printf( PRINT_ALL, "...GL_EXT_texture_env_add not found\n" );
	}

	// GL_EXT_texture_filter_anisotropic
	glConfig.textureFilterAnisotropicAvailable = qfalse;
	if ( strstr( glConfig.extensions_string, "EXT_texture_filter_anisotropic" ) )
	{
		glConfig.textureFilterAnisotropicAvailable = qtrue;
		ri.Printf( PRINT_ALL, "...GL_EXT_texture_filter_anisotropic available\n" );

		if ( r_ext_texture_filter_anisotropic->integer )
		{
			ri.Printf( PRINT_ALL, "...using GL_EXT_texture_filter_anisotropic\n" );
		}
		else
		{
			ri.Printf( PRINT_ALL, "...ignoring GL_EXT_texture_filter_anisotropic\n" );
		}
		ri.Cvar_Set( "r_ext_texture_filter_anisotropic_avail", "1" );
	}
	else
	{
		ri.Printf( PRINT_ALL, "...GL_EXT_texture_filter_anisotropic not found\n" );
		ri.Cvar_Set( "r_ext_texture_filter_anisotropic_avail", "0" );
	}

	// apple transform hint
	if ( strstr( extensions, "GL_APPLE_transform_hint" ) ) {
		if ( r_ext_compiled_vertex_array->integer && r_allowExtensions->integer ) {
			glHint( GL_TRANSFORM_HINT_APPLE, GL_FASTEST );
			ri.Printf( PRINT_ALL, "...using GL_APPLE_transform_hint\n" );
		} else {
			ri.Printf( PRINT_ALL, "...ignoring GL_APPLE_transform_hint\n" );
		}
	} else {
		ri.Printf( PRINT_ALL, "...GL_APPLE_transform_hint not found\n" );
	}
}

/*
============================
GLimp_SufficientVideoMemory
============================
*/
#if 0

qboolean	GLimp_SufficientVideoMemory( void ) {
	AGLRendererInfo head_info, info;
	GLint 		accelerated;
	AGLDevice 	*device;
	GLint 		i, ndevs;
		
	device = aglDevicesOfPixelFormat( sys_gl.fmt, &ndevs);
	if (!device || ndevs < 1) {
		ri.Printf( PRINT_ALL, "aglDevicesOfPixelFormat failed.\n" );
		return 0;
	}
	
	ri.Printf( PRINT_ALL, "%i rendering devices\n", ndevs );
	
	head_info =  aglQueryRendererInfo( device, 1 );
	info = head_info;
	if (!info) {
		ri.Printf( PRINT_ALL, "aglQueryRendererInfo failed.\n" );
		return 0;
	}
	
	for ( i = 0 ; i < ndevs ; i++ ) {
		// ignore the software renderer listing
		aglDescribeRenderer( info, AGL_ACCELERATED, &accelerated );
		if ( accelerated ) {		
			aglDescribeRenderer( info, AGL_TEXTURE_MEMORY, &sys_gl.textureMemory );
			aglDescribeRenderer( info, AGL_VIDEO_MEMORY, &sys_gl.videoMemory );
		}
		info = aglNextRendererInfo(info);
	}

	aglDestroyRendererInfo(head_info);
#if 0
	if ( sys_gl.videoMemory < 16000000 ) {
		glConfig.hardwareType = GLHW_RAGEPRO;		// FIXME when voodoo is available
	} else {
		glConfig.isRagePro = GLHW_GENERIC;		// FIXME when voodoo is available
	}
#endif
	ri.Printf( PRINT_ALL, "%i texture memory, %i video memory\n", sys_gl.textureMemory, sys_gl.videoMemory );

	return qtrue;
}

#endif

/*
=======================
CheckDeviceRenderers

========================
*/
/*
=======================
CheckDeviceRenderers

========================
*/
static void CheckDeviceRenderers( GDHandle device ) {
	AGLRendererInfo info, head_info;
	GLint inum;
	GLint	accelerated;
	GLint	textureMemory, videoMemory;
	
    Com_FlightRecord("CheckDeviceRenderers: Start\n");
    Debug_Breadcrumb(410); // CheckDeviceRenderers Entry

	head_info =  aglQueryRendererInfo(&device, 1);
	if( !head_info ) {
		ri.Printf( PRINT_ALL, "aglQueryRendererInfo : Info Error\n");
        Com_FlightRecord("CheckDeviceRenderers: aglQueryRendererInfo failed\n");
		return;
	}
	
	info = head_info;
	inum = 0;
	while( info ) {
        Com_FlightRecord("CheckDeviceRenderers: Loop inum=%d\n", inum);
        Debug_Breadcrumb(411); // CheckDeviceRenderers Loop start

		ri.Printf( PRINT_ALL, "  Renderer : %d\n", inum);
		
		aglDescribeRenderer( info, AGL_ACCELERATED, &accelerated );
		
		if ( accelerated ) {		
			aglDescribeRenderer( info, AGL_TEXTURE_MEMORY, &textureMemory );
			aglDescribeRenderer( info, AGL_VIDEO_MEMORY, &videoMemory );
			ri.Printf( PRINT_ALL, "    AGL_VIDEO_MEMORY: %i\n", textureMemory );
			ri.Printf( PRINT_ALL, "    AGL_TEXTURE_MEMORY: %i\n", videoMemory );
			
			// save the device with the most texture memory
			if ( sys_gl.textureMemory < textureMemory ) {
				sys_gl.textureMemory = textureMemory;
				sys_gl.device = device;
			}
		} else {
			ri.Printf( PRINT_ALL, "    Not accelerated.\n" );
		}

		info = aglNextRendererInfo(info);
		inum++;
        
        // Safety Break
        if (inum > 100) {
            Com_FlightRecord("CheckDeviceRenderers: Loop runaway! Breaking.\n");
            break;
        }
	}
	
	aglDestroyRendererInfo(head_info);
    Com_FlightRecord("CheckDeviceRenderers: End\n");
    Debug_Breadcrumb(412); // CheckDeviceRenderers Exit
}


/*
=======================
CheckDevices

Make sure there is a device with enough video memory to play
=======================
*/
static void CheckDevices( void ) {
	GDHandle device;
	static	qboolean	checkedFullscreen;
	
    Com_FlightRecord("CheckDevices: Start\n");
    Debug_Breadcrumb(400); // CheckDevices Entry

	if ( checkedFullscreen ) {
		return;
	}
	if ( glConfig.isFullscreen ) {
		checkedFullscreen = qtrue;
	}

	device = GetDeviceList();
	sys_gl.numDevices = 0;
    Com_FlightRecord("CheckDevices: GetDeviceList returned %p\n", device);

	while( device && sys_gl.numDevices < MAX_DEVICES ) {
        Com_FlightRecord("CheckDevices: Loop device=%p numDevices=%d\n", device, sys_gl.numDevices);
        Debug_Breadcrumb(401); // CheckDevices Loop

		sys_gl.devices[ sys_gl.numDevices ] = device;

		ri.Printf( PRINT_ALL, "Device : %d\n", sys_gl.numDevices);
		CheckDeviceRenderers(device);
		
		device = GetNextDevice(device);
		
		sys_gl.numDevices++;
	}

    Com_FlightRecord("CheckDevices: Loop finished/Devices checked.\n");
    Debug_Breadcrumb(402); // CheckDevices Loop End

	CheckErrors();		
	
	if ( sys_gl.textureMemory < 4000000 ) {
		ri.Error( ERR_FATAL, "You must have at least four megs of video memory to play" );
	}

	if ( sys_gl.textureMemory < 16000000 ) {
		sys_hardwareType = GLHW_RAGEPRO;		// this will have to change with voodoo
	} else {
		sys_hardwareType = GLHW_GENERIC;
	}
    Com_FlightRecord("CheckDevices: End\n");
}

/*
=================
CreateGameWindow
=================
*/
static qboolean CreateGameWindow( void ) {
	cvar_t		*vid_xpos;
	cvar_t		*vid_ypos;
	int			mode;
	int			x, y;
	Str255    	pstr;

	Rect		windowRect;
	
	Sys_LogPrintf("DEBUG: CreateGameWindow: Starting\n");
	
	vid_xpos = ri.Cvar_Get( "vid_xpos", "30", 0 );
	vid_ypos = ri.Cvar_Get( "vid_ypos", "30", 0 );
	
	// get mode info
	mode = r_mode->integer;
    ri.Printf( PRINT_ALL, "...setting mode %d:", mode );
	Sys_LogPrintf("DEBUG: CreateGameWindow: mode=%d\n", mode);

    if ( !R_GetModeInfo( &glConfig.vidWidth, &glConfig.vidHeight, &glConfig.windowAspect, mode ) )  {
        ri.Printf( PRINT_ALL, " invalid mode\n" );
		Sys_LogPrintf("DEBUG: CreateGameWindow: R_GetModeInfo failed (invalid mode)\n");
        return false;
    }
    ri.Printf( PRINT_ALL, " %d %d\n", glConfig.vidWidth, glConfig.vidHeight );
	Sys_LogPrintf("DEBUG: CreateGameWindow: vidWidth=%d, vidHeight=%d\n", glConfig.vidWidth, glConfig.vidHeight);

	/* Create window - using NewCWindow instead of GetNewCWindow (no resources needed) */
	if ( r_fullscreen->integer ) {
		int		actualWidth, actualHeight;
		
		Sys_LogPrintf("DEBUG: CreateGameWindow: Fullscreen mode, calling GLimp_ChangeDisplay\n");
		
		// change display resolution
		GLimp_ChangeDisplay( &actualWidth, &actualHeight );
		
		x = ( actualWidth - glConfig.vidWidth ) / 2;
		y = ( actualHeight - glConfig.vidHeight ) / 2;
		
		Sys_LogPrintf("DEBUG: CreateGameWindow: actualWidth=%d, actualHeight=%d, x=%d, y=%d\n", actualWidth, actualHeight, x, y);
	} else {
		// Position window in upper-right corner to keep console visible
		x = 320;  // Right side of screen
		y = 30;   // Near top
		// Force smaller size for debugging
		glConfig.vidWidth = 320;
		glConfig.vidHeight = 240;
		Sys_LogPrintf("DEBUG: Windowed mode, FORCED small window at x=%d, y=%d, size=%dx%d\n", x, y, glConfig.vidWidth, glConfig.vidHeight);
	}
	
	// Create window rect
	windowRect.left = x;
	windowRect.top = y;
	windowRect.right = x + glConfig.vidWidth;
	windowRect.bottom = y + glConfig.vidHeight;
	
	Sys_LogPrintf("DEBUG: CreateGameWindow: Calling NewCWindow, rect=(%d,%d,%d,%d)\n", 
		   windowRect.left, windowRect.top, windowRect.right, windowRect.bottom);
	
	// Use NewCWindow instead of GetNewCWindow to avoid resource dependency
	// Window style: 5 = noGrowDocProc (standard document window without grow box)
	// For fullscreen, use 2 = plainDBox (plain box)
	{
		short windowStyle = r_fullscreen->integer ? 2 : 5;
		CToPStr("Quake3: Arena", pstr);
		sys_gl.drawable = (AGLDrawable) NewCWindow(nil, &windowRect, pstr, true, windowStyle, (WindowPtr)-1L, false, 0);
	}
	
	if( !sys_gl.drawable ) {
		printf("DEBUG: CreateGameWindow: NewCWindow FAILED!\n"); fflush(stdout);
		return qfalse;
	}
	
	printf("DEBUG: CreateGameWindow: NewCWindow succeeded, drawable=%p\n", sys_gl.drawable); fflush(stdout);
	
	SizeWindow((GrafPort *) sys_gl.drawable, glConfig.vidWidth, glConfig.vidHeight,GL_FALSE);
	MoveWindow((GrafPort *) sys_gl.drawable,x, y, GL_FALSE);
	ShowWindow((GrafPort *) sys_gl.drawable);
	SetPort((GrafPort *) sys_gl.drawable);
	HiliteWindow((GrafPort *) sys_gl.drawable, 1);
	SelectWindow((WindowPtr)sys_gl.drawable);
	
	printf("DEBUG: CreateGameWindow: Window setup complete\n"); fflush(stdout);
	
	return qtrue;
}


/*
===================
GLimp_SetMode

Returns false if the mode / fullscrenn / options combination failed,
so another fallback can be tried
===================
*/
qboolean GLimp_SetMode( void ) {
	GLint     	value;
	GLint		attrib[64];
	int			i;

	printf("DEBUG: GLimp_SetMode: Starting\n"); fflush(stdout);

	if ( !CreateGameWindow() ) {
		printf("DEBUG: GLimp_SetMode: CreateGameWindow FAILED\n"); fflush(stdout);
		ri.Printf( PRINT_ALL, "GLimp_Init: window could not be created" );
		return qfalse;
	}
	
	printf("DEBUG: GLimp_SetMode: CreateGameWindow succeeded\n"); fflush(stdout);
	
	// check devices now that the game has set the display mode,
	// because RAVE devices don't get reported if in an 8 bit desktop
	printf("DEBUG: GLimp_SetMode: Calling CheckDevices\n"); fflush(stdout);
	CheckDevices();
	
	printf("DEBUG: GLimp_SetMode: CheckDevices complete, numDevices=%d\n", sys_gl.numDevices); fflush(stdout);
	
	// set up the attribute list
	i = 0;
	attrib[i++] = AGL_RGBA;
	attrib[i++] = AGL_DOUBLEBUFFER;
	attrib[i++] = AGL_NO_RECOVERY;
	attrib[i++] = AGL_ACCELERATED;

	if ( r_colorbits->integer >= 16 ) {
		attrib[i++] = AGL_RED_SIZE;
		attrib[i++] = 8;
		attrib[i++] = AGL_GREEN_SIZE;
		attrib[i++] = 8;
		attrib[i++] = AGL_BLUE_SIZE;
		attrib[i++] = 8;
		attrib[i++] = AGL_ALPHA_SIZE;
		attrib[i++] = 0;
	} else {
		attrib[i++] = AGL_RED_SIZE;
		attrib[i++] = 5;
		attrib[i++] = AGL_GREEN_SIZE;
		attrib[i++] = 5;
		attrib[i++] = AGL_BLUE_SIZE;
		attrib[i++] = 5;
		attrib[i++] = AGL_ALPHA_SIZE;
		attrib[i++] = 0;
	}
	
	attrib[i++] = AGL_STENCIL_SIZE;
	if ( r_stencilbits->integer ) {
		attrib[i++] = r_stencilbits->integer;		
	} else {
		attrib[i++] = 0;
	}

	attrib[i++] = AGL_DEPTH_SIZE;
	if ( r_depthbits->integer ) {
		attrib[i++] = r_depthbits->integer;
	} else {
		attrib[i++] = 16;
	}
	
	attrib[i++] = 0;
	
	/* Choose pixel format */
	printf("DEBUG: GLimp_SetMode: Calling aglChoosePixelFormat\n"); fflush(stdout);
	ri.Printf( PRINT_ALL, "aglChoosePixelFormat\n" );
	if ( r_device->integer < 0 || r_device->integer >= sys_gl.numDevices ) {
		ri.Cvar_Set( "r_device", "0" );
	}
	sys_gl.fmt = aglChoosePixelFormat( &sys_gl.devices[ r_device->integer ], 1, attrib);
	if(!sys_gl.fmt) {
		printf("DEBUG: GLimp_SetMode: aglChoosePixelFormat FAILED\n"); fflush(stdout);
		ri.Printf( PRINT_ALL, "GLimp_Init: Pixel format could not be achieved\n");
		return qfalse;
	}
	printf("DEBUG: GLimp_SetMode: aglChoosePixelFormat succeeded, fmt=0x%x\n", (int)sys_gl.fmt); fflush(stdout);
	ri.Printf( PRINT_ALL, "Selected pixel format 0x%x\n", (int)sys_gl.fmt );
	
	aglDescribePixelFormat(sys_gl.fmt, AGL_RED_SIZE, &value);
	glConfig.colorBits = value * 3;
	aglDescribePixelFormat(sys_gl.fmt, AGL_STENCIL_SIZE, &value);
	glConfig.stencilBits = value;
	aglDescribePixelFormat(sys_gl.fmt, AGL_DEPTH_SIZE, &value);
	glConfig.depthBits = value;

	CheckErrors();
	
	/* Create context */
	sys_gl.context = aglCreateContext(sys_gl.fmt, NULL);
	if(!sys_gl.context) {
		ri.Printf( PRINT_ALL, "GLimp_init: Context could not be created\n");
		return qfalse;
	}
	
	CheckErrors();
	
	/* Make context current */

	if(!aglSetDrawable(sys_gl.context, sys_gl.drawable)) {
		ri.Printf( PRINT_ALL, "GLimp_Init: Could not attach to context\n" );
		return qfalse;
	}
	
	CheckErrors();
	
	if( !aglSetCurrentContext(sys_gl.context) ) {
		ri.Printf( PRINT_ALL, "GLimp_Init: Could not attach to context");
		return qfalse;
	}

	CheckErrors();
	
	// check video memory and determine ragePro status
#if 0
	if ( !GLimp_SufficientVideoMemory() ) {
		return qfalse;
	}
#endif
	glConfig.hardwareType = sys_hardwareType;		// FIXME: this isn't really right
	
	// draw something to show that GL is alive	
	qglClearColor( 1, 0.5, 0.2, 0 );
	qglClear( GL_COLOR_BUFFER_BIT );
	GLimp_EndFrame();

	CheckErrors();

	// get the extensions	
	GLimp_Extensions();

	CheckErrors();
	
	return qtrue;
}



/*
===================
GLimp_Init

Don't return unless OpenGL has been properly initialized
===================
*/
void GLimp_Init( void ) {
	GLint		major, minor;
	static		qboolean	registered;
	
	ri.Printf( PRINT_ALL, "--- GLimp_Init ---\n" );
	printf("DEBUG: GLimp_Init: Starting\n"); fflush(stdout);

	aglGetVersion( &major, &minor );
	ri.Printf( PRINT_ALL, "aglVersion: %i.%i\n", (int)major, (int)minor );
	printf("DEBUG: GLimp_Init: aglVersion %d.%d\n", (int)major, (int)minor); fflush(stdout);
	
	r_device = ri.Cvar_Get( "r_device", "0", CVAR_LATCH | CVAR_ARCHIVE );
	r_ext_transform_hint = ri.Cvar_Get( "r_ext_transform_hint", "1", CVAR_LATCH | CVAR_ARCHIVE );
	
	// DEBUG: Force windowed mode for console visibility
	ri.Cvar_Set( "r_fullscreen", "0" );
	ri.Printf( PRINT_ALL, "^3DEBUG: Forced windowed mode for debugging\n" );
	
	if ( !registered ) {
		ri.Cmd_AddCommand( "aglDescribe", GLimp_AglDescribe_f );
		ri.Cmd_AddCommand( "aglState", GLimp_AglState_f );
	}
	
	memset( &glConfig, 0, sizeof( glConfig ) );


	r_swapInterval->modified = qtrue;	// force a set next frame


	glConfig.deviceSupportsGamma = qtrue;
	
	// FIXME: try for a voodoo first
	sys_gl.systemGammas = GetSystemGammas();

	if ( GLimp_SetMode() ) {
		ri.Printf( PRINT_ALL, "------------------\n" );
		return;
	}

	// fall back to the known-good mode
	ri.Cvar_Set( "r_mode", "3" );
	ri.Cvar_Set( "r_stereo", "0" );
	ri.Cvar_Set( "r_depthBits", "16" );
	ri.Cvar_Set( "r_colorBits", "16" );
	ri.Cvar_Set( "r_stencilBits", "0" );

	if ( GLimp_SetMode() ) {
		ri.Printf( PRINT_ALL, "------------------\n" );
		return;
	}

	printf("DEBUG: GLimp_Init: Both GLimp_SetMode attempts failed!\n"); fflush(stdout);
	ri.Error( ERR_FATAL, "Could not initialize OpenGL" );
}


/*
===============
GLimp_EndFrame

===============
*/
void GLimp_EndFrame( void ) {
	// check for variable changes
	if ( r_swapInterval->modified ) {
		r_swapInterval->modified = qfalse;
		ri.Printf( PRINT_ALL, "Changing AGL_SWAP_INTERVAL\n" );
		aglSetInteger( sys_gl.context, AGL_SWAP_INTERVAL, (long *)&r_swapInterval->integer );
	}

	// make sure the event loop is pumped
	Sys_SendKeyEvents();
	
	aglSwapBuffers( sys_gl.context );
}

/*
===============
GLimp_Shutdown

===============
*/
void GLimp_Shutdown( void ) {	
	if ( sys_gl.systemGammas ) {
		RestoreSystemGammas( sys_gl.systemGammas );
		DisposeSystemGammas( &sys_gl.systemGammas );
		sys_gl.systemGammas = 0;
	}
	
	if ( sys_gl.context ) {
		aglDestroyContext(sys_gl.context);
		sys_gl.context = 0;
	}
	if ( sys_gl.fmt ) {
	    aglDestroyPixelFormat(sys_gl.fmt);
		sys_gl.fmt = 0;
	}
	if ( sys_gl.drawable ) {
		DisposeWindow((GrafPort *) sys_gl.drawable);
		sys_gl.drawable = 0;
	}
	GLimp_ResetDisplay();
	
	memset( &glConfig, 0, sizeof( glConfig ) );
}


void		GLimp_LogComment( char *comment ) {
}
qboolean	GLimp_SpawnRenderThread( void (*function)( void ) ) {
}
void *GLimp_RendererSleep( void ) {

}

void GLimp_FrontEndSleep( void ) {

}

void GLimp_WakeRenderer( void * data ) {

}



/*
===============
GLimp_SetGamma

===============
*/
void		GLimp_SetGamma( unsigned char red[256], 
						    unsigned char green[256],
							unsigned char blue[256] ) {
	char	color[3][256];
	int		i;

	for ( i = 0 ; i < 256 ; i++ ) {
		color[0][i] = red[i];
		color[1][i] = green[i];
		color[2][i] = blue[i];	
	}
	SetDeviceGammaRampGD( sys_gl.device, color[0] );
}

