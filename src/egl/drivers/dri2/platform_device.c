/*
 * Mesa 3-D graphics library
 *
 * Copyright 2018 Collabora
 *
 * Based on platform_surfaceless, which has:
 *
 * Copyright (c) 2014 The Chromium OS Authors.
 * Copyright © 2011 Intel Corporation
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

#ifdef HAVE_LIBDRM
#include <xf86drm.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "egl_dri2.h"
#include "egl_dri2_fallbacks.h"
#include "loader.h"

static __DRIimage*
device_alloc_image(struct dri2_egl_display *dri2_dpy,
                   struct dri2_egl_surface *dri2_surf)
{
   return dri2_dpy->image->createImage(
            dri2_dpy->dri_screen,
            dri2_surf->base.Width,
            dri2_surf->base.Height,
            dri2_surf->visual,
            0,
            NULL);
}

static void
device_free_images(struct dri2_egl_surface *dri2_surf)
{
   struct dri2_egl_display *dri2_dpy =
      dri2_egl_display(dri2_surf->base.Resource.Display);

   if (dri2_surf->front) {
      dri2_dpy->image->destroyImage(dri2_surf->front);
      dri2_surf->front = NULL;
   }
}

static int
device_image_get_buffers(__DRIdrawable *driDrawable,
                         unsigned int format,
                         uint32_t *stamp,
                         void *loaderPrivate,
                         uint32_t buffer_mask,
                         struct __DRIimageList *buffers)
{
   struct dri2_egl_surface *dri2_surf = loaderPrivate;
   struct dri2_egl_display *dri2_dpy =
      dri2_egl_display(dri2_surf->base.Resource.Display);

   buffers->image_mask = 0;
   buffers->front = NULL;
   buffers->back = NULL;

   /* The EGL 1.5 spec states that pbuffers are single-buffered. Specifically,
    * the spec states that they have a back buffer but no front buffer, in
    * contrast to pixmaps, which have a front buffer but no back buffer.
    *
    * Single-buffered surfaces with no front buffer confuse Mesa; so we deviate
    * from the spec, following the precedent of Mesa's EGL X11 platform. The
    * X11 platform correctly assigns pbuffers to single-buffered configs, but
    * assigns the pbuffer a front buffer instead of a back buffer.
    *
    * Pbuffers in the X11 platform mostly work today, so let's just copy its
    * behavior instead of trying to fix (and hence potentially breaking) the
    * world.
    */

   if (buffer_mask & __DRI_IMAGE_BUFFER_FRONT) {

      if (!dri2_surf->front)
         dri2_surf->front =
            device_alloc_image(dri2_dpy, dri2_surf);

      buffers->image_mask |= __DRI_IMAGE_BUFFER_FRONT;
      buffers->front = dri2_surf->front;
   }

   return 1;
}

static _EGLSurface *
dri2_device_create_surface(_EGLDriver *drv, _EGLDisplay *disp, EGLint type,
                           _EGLConfig *conf, const EGLint *attrib_list)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);
   struct dri2_egl_config *dri2_conf = dri2_egl_config(conf);
   struct dri2_egl_surface *dri2_surf;
   const __DRIconfig *config;

   /* Make sure to calloc so all pointers
    * are originally NULL.
    */
   dri2_surf = calloc(1, sizeof *dri2_surf);

   if (!dri2_surf) {
      _eglError(EGL_BAD_ALLOC, "eglCreatePbufferSurface");
      return NULL;
   }

   if (!dri2_init_surface(&dri2_surf->base, disp, type, conf, attrib_list, false))
      goto cleanup_surface;

   config = dri2_get_dri_config(dri2_conf, type,
                                dri2_surf->base.GLColorspace);

   if (!config) {
      _eglError(EGL_BAD_MATCH, "Unsupported surfacetype/colorspace configuration");
      goto cleanup_surface;
   }

   dri2_surf->dri_drawable =
      dri2_dpy->image_driver->createNewDrawable(dri2_dpy->dri_screen, config,
                                                dri2_surf);
   if (dri2_surf->dri_drawable == NULL) {
      _eglError(EGL_BAD_ALLOC, "image->createNewDrawable");
      goto cleanup_surface;
    }

   if (conf->RedSize == 5)
      dri2_surf->visual = __DRI_IMAGE_FORMAT_RGB565;
   else if (conf->AlphaSize == 0)
      dri2_surf->visual = __DRI_IMAGE_FORMAT_XRGB8888;
   else
      dri2_surf->visual = __DRI_IMAGE_FORMAT_ARGB8888;

   return &dri2_surf->base;

   cleanup_surface:
      free(dri2_surf);
      return NULL;
}

static EGLBoolean
device_destroy_surface(_EGLDriver *drv, _EGLDisplay *disp, _EGLSurface *surf)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(disp);
   struct dri2_egl_surface *dri2_surf = dri2_egl_surface(surf);

   device_free_images(dri2_surf);

   dri2_dpy->core->destroyDrawable(dri2_surf->dri_drawable);

   dri2_fini_surface(surf);
   free(dri2_surf);
   return EGL_TRUE;
}

static _EGLSurface *
dri2_device_create_pbuffer_surface(_EGLDriver *drv, _EGLDisplay *disp,
                                   _EGLConfig *conf, const EGLint *attrib_list)
{
   return dri2_device_create_surface(drv, disp, EGL_PBUFFER_BIT, conf,
                                     attrib_list);
}

static EGLBoolean
device_swap_buffers(_EGLDriver *drv, _EGLDisplay *disp, _EGLSurface *surf)
{
   assert(!surf || surf->Type == EGL_PBUFFER_BIT);

   /* From the EGL 1.5 spec:
    *    If surface is a [...] pbuffer surface, eglSwapBuffers has no effect.
    */
   return EGL_TRUE;
}

static EGLBoolean
device_add_configs_for_visuals(_EGLDriver *drv, _EGLDisplay *dpy)
{
   struct dri2_egl_display *dri2_dpy = dri2_egl_display(dpy);
   static const struct {
      const char *format_name;
      unsigned int rgba_masks[4];
   } visuals[] = {
      { "ARGB8888", { 0xff0000, 0xff00, 0xff, 0xff000000 } },
      { "RGB888",   { 0xff0000, 0xff00, 0xff, 0x0 } },
      { "RGB565",   { 0x00f800, 0x07e0, 0x1f, 0x0 } },
   };
   unsigned int format_count[ARRAY_SIZE(visuals)] = { 0 };
   unsigned int config_count = 0;

   for (unsigned i = 0; dri2_dpy->driver_configs[i] != NULL; i++) {
      for (unsigned j = 0; j < ARRAY_SIZE(visuals); j++) {
         struct dri2_egl_config *dri2_conf;

         dri2_conf = dri2_add_config(dpy, dri2_dpy->driver_configs[i],
               config_count + 1, EGL_PBUFFER_BIT, NULL,
               visuals[j].rgba_masks);

         if (dri2_conf) {
            if (dri2_conf->base.ConfigID == config_count + 1)
               config_count++;
            format_count[j]++;
         }
      }
   }

   for (unsigned i = 0; i < ARRAY_SIZE(format_count); i++) {
      if (!format_count[i]) {
         _eglLog(_EGL_DEBUG, "No DRI config supports native format %s",
               visuals[i].format_name);
      }
   }

   return (config_count != 0);
}

static const struct dri2_egl_display_vtbl dri2_device_display_vtbl = {
   .create_pixmap_surface = dri2_fallback_create_pixmap_surface,
   .create_pbuffer_surface = dri2_device_create_pbuffer_surface,
   .destroy_surface = device_destroy_surface,
   .create_image = dri2_create_image_khr,
   .swap_buffers = device_swap_buffers,
   .swap_buffers_with_damage = dri2_fallback_swap_buffers_with_damage,
   .swap_buffers_region = dri2_fallback_swap_buffers_region,
   .set_damage_region = dri2_fallback_set_damage_region,
   .post_sub_buffer = dri2_fallback_post_sub_buffer,
   .copy_buffers = dri2_fallback_copy_buffers,
   .query_buffer_age = dri2_fallback_query_buffer_age,
   .create_wayland_buffer_from_image = dri2_fallback_create_wayland_buffer_from_image,
   .get_sync_values = dri2_fallback_get_sync_values,
   .get_dri_drawable = dri2_surface_get_dri_drawable,
};

static void
device_flush_front_buffer(__DRIdrawable *driDrawable, void *loaderPrivate)
{
}

static const __DRIimageLoaderExtension image_loader_extension = {
   .base             = { __DRI_IMAGE_LOADER, 1 },
   .getBuffers       = device_image_get_buffers,
   .flushFrontBuffer = device_flush_front_buffer,
};

static const __DRIextension *image_loader_extensions[] = {
   &image_loader_extension.base,
   &image_lookup_extension.base,
   &use_invalidate.base,
   NULL,
};

static int
device_get_fd(_EGLDisplay *disp, _EGLDevice *dev)
{
#ifdef HAVE_LIBDRM
   int fd = ((_EGLDeviceDisplayOptions *)disp->Options.Platform)->internal_fd;
   if (fd != -1) {
      /* According to the spec - if the FD does not match the EGLDevice
       * behaviour is undefined.
       *
       * Add a trivial sanity check since it doesn't cost us anything.
       */
      if (dev != _eglAddDevice(fd, false))
         return -1;

      /* No EGL_EXT_output* extensions are supported, hence no master perms
       * are needed. Get the render one - otherwise drivers might error out.
       */
      char *node = drmGetRenderDeviceNameFromFd(fd);

      close(fd);
      fd = loader_open_device(node);
      free(node);
      return fd;
   }
   const char *node = _eglGetDRMDeviceRenderNode(dev);
   return loader_open_device(node);
#else
   _eglLog(_EGL_FATAL, "Driver bug: Built without libdrm, yet using a HW device");
   return -1;
#endif
}

static EGLBoolean
device_probe_device(_EGLDisplay *disp)
{
   _EGLDevice *dev;
   struct dri2_egl_display *dri2_dpy = disp->DriverData;
   EGLBoolean swrast;
   int fd;

   /* Extension requires a PlatformDisplay - the EGLDevice. */
   dev = disp->PlatformDisplay;
   if (_eglDeviceSupports(dev, _EGL_DEVICE_DRM)) {
      swrast = EGL_FALSE;
      fd = device_get_fd(disp, dev);
      if (fd < 0)
         return EGL_FALSE;

      dri2_dpy->driver_name = loader_get_driver_for_fd(fd);
   } else if (_eglDeviceSupports(dev, _EGL_DEVICE_SOFTWARE)) {
      swrast = EGL_TRUE;
      fd = -1;

      /* XXX: Should really be a "null" version of kms_swrast */
      dri2_dpy->driver_name = strdup("swrast");
   } else {
      assert(0);
      return EGL_FALSE;
   }

   if (!dri2_dpy->driver_name)
      return EGL_FALSE;

   dri2_dpy->fd = fd;
   disp->Device = dev;

   if (swrast)
      return dri2_load_driver_swrast(disp);

   return dri2_load_driver_dri3(disp);
}

EGLBoolean
dri2_initialize_device(_EGLDriver *drv, _EGLDisplay *disp)
{
   struct dri2_egl_display *dri2_dpy;
   const char* err;

   loader_set_logger(_eglLog);

   dri2_dpy = calloc(1, sizeof *dri2_dpy);
   if (!dri2_dpy)
      return _eglError(EGL_BAD_ALLOC, "eglInitialize");

   dri2_dpy->fd = -1;
   disp->DriverData = (void *) dri2_dpy;

   if (!device_probe_device(disp)) {
      err = "DRI2: failed to load driver";
      goto cleanup;
   }

   dri2_dpy->loader_extensions = image_loader_extensions;

   if (!dri2_create_screen(disp)) {
      err = "DRI2: failed to create screen";
      goto cleanup;
   }

   if (!dri2_setup_extensions(disp)) {
      err = "DRI2: failed to find required DRI extensions";
      goto cleanup;
   }

   dri2_setup_screen(disp);

   if (!device_add_configs_for_visuals(drv, disp)) {
      err = "DRI2: failed to add configs";
      goto cleanup;
   }

   /* Fill vtbl last to prevent accidentally calling virtual function during
    * initialization.
    */
   dri2_dpy->vtbl = &dri2_device_display_vtbl;

   return EGL_TRUE;

cleanup:
   dri2_display_destroy(disp);
   return _eglError(EGL_NOT_INITIALIZED, err);
}
