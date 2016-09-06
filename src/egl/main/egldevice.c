/**************************************************************************
 *
 * Copyright 2015 Collabora
 * Copyright © 2016 Intel Corporation
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

#include "egldevice.h"
#include "eglglobals.h"
#include "egltypedefs.h"

struct _egl_device {
   void *data;
};

void
_eglInitDevices(void)
{
   _EGLDevice *devs;

   mtx_lock(_eglGlobal.Mutex);

   devs = _eglGlobal.DeviceList.devs;
   if (!devs) {
      const int num_devices = 0;

      if (num_devices <= 0)
         goto out;

      devs = calloc(num_devices, sizeof(_EGLDevice));
      if (!devs)
         goto out;

      _eglGlobal.DeviceList.devs = devs;
      _eglGlobal.DeviceList.num_devices = num_devices;
      _eglGlobal.ClientExtensionString = _egl_client_and_device_extensions;
   }

out:
   mtx_unlock(_eglGlobal.Mutex);
}

/**
 * Finish device management.
 */
void
_eglFiniDevices(void)
{
   /* atexit function is called with global mutex locked */
   if (_eglGlobal.DeviceList.num_devices) {
      free(_eglGlobal.DeviceList.devs);
      _eglGlobal.DeviceList.devs = NULL;
      _eglGlobal.DeviceList.num_devices = 0;
   }
}
