/*
 * Mesa 3-D graphics library
 *
 * Copyright (C) 2014 Adrián Arroyo Calle <adrian.arroyocalle@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
 
#include <errno.h>
#include <dlfcn.h>
#include <stdio.h>

#include "loader.h"
#include "egl_dri2.h"
#include "egl_dri2_fallbacks.h"

#include <InterfaceKit.h>

static void
haiku_log(EGLint level, const char *msg)
{
   switch (level) {
   case _EGL_DEBUG:
      printf("%s", msg);
      break;
   case _EGL_INFO:
      printf("%s", msg);
      break;
   case _EGL_WARNING:
      printf("%s", msg);
      break;
   case _EGL_FATAL:
      fprintf(stderr,"%s", msg);
      break;
   default:
      break;
   }
}

BWindow* 
haiku_create_window()
{
	BWindow* win=new BWindow(BRect(100,100,500,500),"EGL Test",B_TITLED_WINDOW,0);
	return win;
}

static EGLBoolean
dri2_haiku_add_configs_for_visuals(struct dri2_egl_display *dri2_dpy, _EGLDisplay *disp)
{
	if (!_eglGetArraySize(disp->Configs)) {
      _eglLog(_EGL_WARNING, "DRI2: failed to create any config");
      return EGL_FALSE;
   }

   return EGL_TRUE;
}

extern "C"
EGLBoolean
dri2_initialize_haiku(_EGLDriver *drv, _EGLDisplay *dpy)
{
	printf("INITIALIZING HAIKU");
	struct dri2_egl_display_vtbl dri2_haiku_display_vtbl;
	dri2_haiku_display_vtbl={};
	dri2_haiku_display_vtbl.authenticate = NULL;
   dri2_haiku_display_vtbl.create_window_surface = NULL;
   dri2_haiku_display_vtbl.create_pixmap_surface = NULL;
   dri2_haiku_display_vtbl.create_pbuffer_surface = NULL;
   dri2_haiku_display_vtbl.destroy_surface = NULL;
   dri2_haiku_display_vtbl.create_image = NULL;
   dri2_haiku_display_vtbl.swap_interval = dri2_fallback_swap_interval;
   dri2_haiku_display_vtbl.swap_buffers = NULL;
   dri2_haiku_display_vtbl.swap_buffers_region = dri2_fallback_swap_buffers_region;
   dri2_haiku_display_vtbl.post_sub_buffer = dri2_fallback_post_sub_buffer;
   dri2_haiku_display_vtbl.copy_buffers = NULL;
   dri2_haiku_display_vtbl.query_buffer_age = dri2_fallback_query_buffer_age,
   dri2_haiku_display_vtbl.create_wayland_buffer_from_image = dri2_fallback_create_wayland_buffer_from_image;
   dri2_haiku_display_vtbl.get_sync_values = dri2_fallback_get_sync_values;
	
	struct dri2_egl_display *dri2_dpy;
	const char *err;
	
	_eglSetLogProc(haiku_log);
	
	//loader_set_logger(_eglLog);
	
	dri2_dpy =(struct dri2_egl_display*) calloc(1, sizeof(*dri2_dpy));
	if (!dri2_dpy)
		return _eglError(EGL_BAD_ALLOC, "eglInitialize");
	dpy->DriverData=(void*) dri2_dpy;
	if(dpy->PlatformDisplay == NULL)
	{
		/* OPEN DEVICE */
		dri2_dpy->bwindow=(void*)haiku_create_window();
		dri2_dpy->own_device=true;	
	}else{
		BWindow* win=(BWindow*)dpy->PlatformDisplay;
		dri2_dpy->bwindow=win;
	}
	
	dri2_dpy->driver_name = strdup("Haiku OpenGL");
	if (!dri2_load_driver_swrast(dpy))
		return EGL_FALSE;
	
	dri2_dpy->swrast_loader_extension.base.name = __DRI_SWRAST_LOADER;
   dri2_dpy->swrast_loader_extension.base.version = __DRI_SWRAST_LOADER_VERSION;
   //dri2_dpy->swrast_loader_extension.getDrawableInfo = swrastGetDrawableInfo;
   //dri2_dpy->swrast_loader_extension.putImage = swrastPutImage;
   //dri2_dpy->swrast_loader_extension.getImage = swrastGetImage;
   
      dri2_dpy->extensions[0] = &dri2_dpy->swrast_loader_extension.base;
   dri2_dpy->extensions[1] = NULL;
   dri2_dpy->extensions[2] = NULL;
	
	if (dri2_dpy->bwindow) {
      if (!dri2_haiku_add_configs_for_visuals(dri2_dpy, dpy))
      	return EGL_FALSE;
    }
	
	
	dpy->VersionMajor=1;
	dpy->VersionMinor=4;
	
	dri2_dpy->vtbl = &dri2_haiku_display_vtbl;
	
	return EGL_TRUE;
	
}
