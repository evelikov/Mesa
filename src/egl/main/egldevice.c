/**************************************************************************
 *
 * Copyright 2015 Collabora
 * Copyright © 2016 Intel Corporation
 * Copyright 2016 Red Hat, Inc.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/

#ifdef HAVE_LIBDRM
#include <xf86drm.h>
#endif

<<<<<<< HEAD
=======
#include <assert.h>
#include <string.h>

#include "eglcurrent.h"
>>>>>>> e117682... egldevice: implement eglQueryDisplayAttribEXT
#include "egldevice.h"
#include "egldriver.h"
#include "eglglobals.h"
#include "egltypedefs.h"

struct _egl_device {
#ifdef HAVE_LIBDRM
   drmDevicePtr data;
#else
   void *data;
#endif
   const char *extensions;
};

#define MIN(x,y) (((x)<(y))?(x):(y))

static inline int
_eglGetDevicesCount(void)
{
#ifdef HAVE_LIBDRM
   return drmGetDevices(NULL, 0);
#else
   return 0;
#endif
}

static EGLBoolean
_eglFillDeviceData(_EGLDevice *devs, int num_devices)
{
#ifdef HAVE_LIBDRM
   drmDevicePtr data;

   data = calloc(num_devices, sizeof(drmDevicePtr));
   if (!data)
      return EGL_FALSE;

   // Rather than calling drmGetDevice num_devices times, we can use
   // drmGetDevices and point the EGL device private data to each instance.
   // We must be careful and use devs[0].data to free the data.
   if (drmGetDevices(data, num_devices) < 0) {
      free(data);
      return EGL_FALSE;
   }

   for (int i = 0; i < num_devices; i++) {
      devs[i].data = data[i];
      // Different devices can support different set of device extensions.
      devs[i].extensions = "";
   }

   return EGL_TRUE;
#else
   return EGL_FALSE;
#endif
}

void
_eglInitDevices(void)
{
   _EGLDevice *devs;

   mtx_lock(_eglGlobal.Mutex);

   devs = _eglGlobal.DeviceList.devs;
   if (!devs) {
      const int num_devices = _eglGetDevicesCount();

      if (num_devices <= 0)
         goto out;

      devs = calloc(num_devices, sizeof(_EGLDevice));
      if (!devs)
         goto out;

      if (_eglFillDeviceData(devs, num_devices) == EGL_FALSE) {
         free(devs);
         goto out;
      }

      _eglGlobal.DeviceList.devs = devs;
      _eglGlobal.DeviceList.num_devices = num_devices;
      _eglGlobal.ClientExtensionString = _egl_client_and_device_extensions;
   }

out:
   mtx_unlock(_eglGlobal.Mutex);
}

static void
_eglFreeDeviceData(_EGLDevice *devs, int num_devices)
{
#ifdef HAVE_LIBDRM
   // This looks strange/leaky. See the comment in _eglFillDeviceData() for
   // information why it's fine as-is.
   drmFreeDevices(devs[0].data);
#endif
}

/**
 * Finish device management.
 */
void
_eglFiniDevices(void)
{
   /* atexit function is called with global mutex locked */
   if (_eglGlobal.DeviceList.num_devices) {
      _eglFreeDeviceData(_eglGlobal.DeviceList.devs,
                         _eglGlobal.DeviceList.num_devices);
      free(_eglGlobal.DeviceList.devs);
      _eglGlobal.DeviceList.devs = NULL;
      _eglGlobal.DeviceList.num_devices = 0;
   }
}

/**
 * Return EGL_TRUE if the given handle is a valid handle to a display.
 */
EGLBoolean
_eglCheckDeviceHandle(EGLDeviceEXT device)
{
   EGLBoolean found = EGL_FALSE;

   mtx_lock(_eglGlobal.Mutex);

   for (int i = 0; i < _eglGlobal.DeviceList.num_devices; i++) {
      if (_eglGlobal.DeviceList.devs[i] == (_EGLDevice *) device) {
         found = EGL_TRUE;
         goto out;
      }
   }

out:
   mtx_unlock(_eglGlobal.Mutex);
   return found;
}

/**
 * Get attribute about specific device
 */
EGLBoolean
_eglQueryDeviceAttribEXT(_EGLDevice *device,
                         EGLint attribute,
                         EGLAttrib *value)
{
   if (!value)
      return _eglError(EGL_BAD_PARAMETER, "eglQueryDeviceAttribEXT");

   switch (attribute) {
   default:
      return _eglError(EGL_BAD_ATTRIBUTE, "eglQueryDeviceAttribEXT");
   }
}

/**
 * Get string about a specific device.
 */
const char *
_eglQueryDeviceStringEXT(_EGLDevice *device, EGLint name)
{
   switch (name) {
   case EGL_EXTENSIONS:
      return device->extensions;

   default:
      _eglError(EGL_BAD_PARAMETER, "eglQueryDeviceStringEXT");
      return NULL;
   };
}

<<<<<<< HEAD
=======
static EGLBoolean
_eglQueryDeviceFromDisplay(_EGLDeviceInfo *info,
                           _EGLDriver *drv,
                           _EGLDisplay *disp,
                           EGLAttrib *value)
{
#ifdef HAVE_LIBUDEV
   const char *device_name = NULL;
   _EGLDevice *dev;
   UDEV_SYMBOL(const char *, udev_device_get_property_value,
               (struct udev_device *, const char *));

   if (dlsym_failed)
      return EGL_FALSE;

   if (!drv->QueryDeviceName)
      return EGL_FALSE;

   device_name = drv->QueryDeviceName(disp);

   mtx_lock(_eglGlobal.Mutex);

   assert(info->got_devices);

   for (dev = info->devices; dev; dev = dev->Next) {
      const char *devname = udev_device_get_property_value(
         dev->Info, "DEVNAME");

      if (!devname)
         continue;

      if (!strcmp(devname, device_name))
         break;
   }

   mtx_unlock(_eglGlobal.Mutex);

   *value = (EGLAttrib) dev;

   return (dev) ? EGL_TRUE : EGL_FALSE;
#else
   return EGL_FALSE;
#endif
}

EGLBoolean
_eglQueryDisplayAttribEXT(_EGLDriver *drv,
                          _EGLDisplay *disp,
                          EGLint attribute,
                          EGLAttrib *value)
{
   _EGLDeviceInfo *info;

   info = _eglEnsureDeviceInfo(EGL_TRUE);
   if (!info)
      return _eglError(EGL_BAD_ALLOC, "eglQueryDisplayAttribEXT");

   if (!value)
      return _eglError(EGL_BAD_PARAMETER, "eglQueryDisplayAttribEXT");

   switch (attribute) {
   case EGL_DEVICE_EXT:
      return _eglQueryDeviceFromDisplay(info, drv, disp, value);
   default:
      return _eglError(EGL_BAD_ATTRIBUTE, "eglQueryDisplayAttribEXT");
   }
}

static EGLBoolean
_eglFillDeviceList(_EGLDeviceInfo *info)
{
#ifdef HAVE_LIBDRM
   info->num_drm_devices = drmGetDevices(NULL, 0);

   if (info->num_drm_devices < 0) {
      info->num_drm_devices = 0;
      return EGL_FALSE;
   }

   info->drm_devices = calloc(info->num_drm_devices, sizeof(drmDevicePtr));
   if (!info->drm_devices) {
      info->num_drm_devices = 0;
      return EGL_FALSE;
   }

   if (drmGetDevices(info->drm_devices, info->num_drm_devices) < 0) {
      free(info->drm_devices);
      info->num_drm_devices = 0;
      return EGL_FALSE;
   }

   info->got_devices = EGL_TRUE;
   return EGL_TRUE;
#else
   return EGL_FALSE;
#endif
}

>>>>>>> 98aa78d... egldevice: implement eglQueryDeviceStringEXT
>>>>>>> 30af732... egldevice: implement eglQueryDisplayAttribEXT
/**
 * Enumerate EGL devices.
 */
EGLBoolean
_eglQueryDevicesEXT(EGLint max_devices,
                    _EGLDevice **devices,
                    EGLint *num_devices)
{
   _EGLDevice *devs;
   EGLBoolean ret = EGL_TRUE;

   /* max_devices can only be bad if devices is non-NULL. num_devices must
    * always be present. */
   if ((devices && max_devices < 1) || !num_devices)
      return _eglError(EGL_BAD_PARAMETER, "eglQueryDevicesEXT");

   mtx_lock(_eglGlobal.Mutex);

   if (!_eglGlobal.DeviceList.num_devices) {
      ret = _eglError(EGL_BAD_PARAMETER, "eglQueryDevicesEXT");
      goto out;

   /* bail early if devices is NULL */
   if (!devices) {
      *num_devices = _eglGlobal.DeviceList.num_devices;
      goto out;
   }

   /* fill devices array */
   *num_devices = MIN(_eglGlobal.DeviceList.num_devices, max_devices);
   devs = _eglGlobal.DeviceList.devs;

   for (int i = 0; i < *num_devices; i++)
      devices[i] = devs[i];

out:
   mtx_unlock(_eglGlobal.Mutex);

   return ret;
}
