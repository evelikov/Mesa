/*
 * Copyright (C) 2013 Rob Clark <robclark@freedesktop.org>
 *
 * This code is derived from the following files.
 *
 * * src/glx/dri3_common.c
 * Copyright © 2013 Keith Packard
 *
 * * src/egl/drivers/dri2/common.c
 * * src/gbm/backends/dri/driver_name.c
 * Copyright © 2011 Intel Corporation
 *
 * Authors:
 *    Kristian Høgsberg <krh@bitplanet.net>
 *    Benjamin Franzke <benjaminfranzke@googlemail.com>
 *
 * * src/gallium/targets/egl-static/egl.c
 * Copyright (C) 2010-2011 LunarG Inc.
 *
 * Authors:
 *    Chia-I Wu <olv@lunarg.com>
 *
 * * src/gallium/state_trackers/egl/drm/native_drm.c
 * Copyright (C) 2010 Chia-I Wu <olv@0xlab.org>
 *
 * * src/egl/drivers/dri2/platform_android.c
 *
 * Copyright (C) 2010-2011 Chia-I Wu <olvaffe@gmail.com>
 * Copyright (C) 2010-2011 LunarG Inc.
 *
 * Based on platform_x11, which has
 *
 * Copyright © 2011 Intel Corporation
 *
 * * src/gallium/auxiliary/pipe-loader/pipe_loader_drm.c
 * Copyright 2011 Intel Corporation
 * Copyright 2012 Francisco Jerez
 * All Rights Reserved.
 *
 * Authors:
 *    Kristian Høgsberg <krh@bitplanet.net>
 *    Benjamin Franzke <benjaminfranzke@googlemail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Authors:
 *    Rob Clark <robclark@freedesktop.org>
 */

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef MAJOR_IN_MKDEV
#include <sys/mkdev.h>
#endif
#ifdef MAJOR_IN_SYSMACROS
#include <sys/sysmacros.h>
#endif
#include "loader.h"

#ifdef HAVE_LIBDRM
#include <stdlib.h>
#include <unistd.h>
#include <xf86drm.h>
#ifdef USE_DRICONF
#include "xmlconfig.h"
#include "xmlpool.h"
#endif
#endif

#define __IS_LOADER
#include "pci_id_driver_map.h"

static void default_logger(int level, const char *fmt, ...)
{
   if (level <= _LOADER_WARNING) {
      va_list args;
      va_start(args, fmt);
      vfprintf(stderr, fmt, args);
      va_end(args);
   }
}

static void (*log_)(int level, const char *fmt, ...) = default_logger;

int
loader_open_device(const char *device_name)
{
   int fd;
#ifdef O_CLOEXEC
   fd = open(device_name, O_RDWR | O_CLOEXEC);
   if (fd == -1 && errno == EINVAL)
#endif
   {
      fd = open(device_name, O_RDWR);
      if (fd != -1)
         fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) | FD_CLOEXEC);
   }
   return fd;
}

#if defined(HAVE_LIBDRM)
#ifdef USE_DRICONF
static const char __driConfigOptionsLoader[] =
DRI_CONF_BEGIN
    DRI_CONF_SECTION_INITIALIZATION
        DRI_CONF_DEVICE_ID_PATH_TAG()
    DRI_CONF_SECTION_END
DRI_CONF_END;

static char *loader_get_dri_config_device_id(void)
{
   driOptionCache defaultInitOptions;
   driOptionCache userInitOptions;
   char *prime = NULL;

   driParseOptionInfo(&defaultInitOptions, __driConfigOptionsLoader);
   driParseConfigFiles(&userInitOptions, &defaultInitOptions, 0, "loader");
   if (driCheckOption(&userInitOptions, "device_id", DRI_STRING))
      prime = strdup(driQueryOptionstr(&userInitOptions, "device_id"));
   driDestroyOptionCache(&userInitOptions);
   driDestroyOptionInfo(&defaultInitOptions);

   return prime;
}
#endif

static char *drm_construct_id_path_tag(drmDevicePtr device)
{
/* Length of "pci-xxxx_xx_xx_x\n" */
#define PCI_ID_PATH_TAG_LENGTH 17
   char *tag = NULL;

   if (device->bustype == DRM_BUS_PCI) {
        tag = calloc(PCI_ID_PATH_TAG_LENGTH, sizeof(char));
        if (tag == NULL)
            return NULL;

        sprintf(tag, "pci-%04x_%02x_%02x_%1u", device->businfo.pci->domain,
                device->businfo.pci->bus, device->businfo.pci->dev,
                device->businfo.pci->func);
   }
   return tag;
}

static bool drm_device_matches_tag(drmDevicePtr device, const char *prime_tag)
{
   char *tag = drm_construct_id_path_tag(device);
   int ret;

   if (tag == NULL)
      return false;

   ret = strcmp(tag, prime_tag);

   free(tag);
   return ret == 0;
}

static char *drm_get_id_path_tag_for_fd(int fd)
{
   drmDevicePtr device;
   char *tag;

   if (drmGetDevice(fd, &device) != 0)
       return NULL;

   tag = drm_construct_id_path_tag(device);
   drmFreeDevice(&device);
   return tag;
}

int loader_get_user_preferred_fd(int default_fd, int *different_device)
{
/* Arbitrary "maximum" value of drm devices. */
#define MAX_DRM_DEVICES 32
   const char *dri_prime = getenv("DRI_PRIME");
   char *default_tag, *prime = NULL;
   drmDevicePtr devices[MAX_DRM_DEVICES];
   int i, num_devices, fd;
   bool found = false;

   if (dri_prime)
      prime = strdup(dri_prime);
#ifdef USE_DRICONF
   else
      prime = loader_get_dri_config_device_id();
#endif

   if (prime == NULL) {
      *different_device = 0;
      return default_fd;
   }

   default_tag = drm_get_id_path_tag_for_fd(default_fd);
   if (default_tag == NULL)
      goto err;

   num_devices = drmGetDevices(devices, MAX_DRM_DEVICES);
   if (num_devices < 0)
      goto err;

   /* two format are supported:
    * "1": choose any other card than the card used by default.
    * id_path_tag: (for example "pci-0000_02_00_0") choose the card
    * with this id_path_tag.
    */
   if (!strcmp(prime,"1")) {
      /* Hmm... detection for 2-7 seems to be broken. Oh well ...
       * Pick the first render device that is not our own.
       */
      printf("PRIME\n");
      for (i = 0; i < num_devices; i++) {
         printf("%s\n", devices[i]->nodes[DRM_NODE_RENDER]);

         if (devices[i]->available_nodes & 1 << DRM_NODE_RENDER &&
             !drm_device_matches_tag(devices[i], default_tag)) {

            found = true;
            break;
         }
      }
   } else {
      printf("DEVICE ID %s\n", prime);
      for (i = 0; i < num_devices; i++) {
         printf("%s\n", devices[i]->nodes[DRM_NODE_RENDER]);
         if (devices[i]->available_nodes & 1 << DRM_NODE_RENDER &&
            drm_device_matches_tag(devices[i], prime)) {

            found = true;
            break;
         }
      }
   }

   if (!found) {
      drmFreeDevices(devices, num_devices);
      goto err;
   }

   printf("OPENING %s\n", devices[i]->nodes[DRM_NODE_RENDER]);
   fd = loader_open_device(devices[i]->nodes[DRM_NODE_RENDER]);
   drmFreeDevices(devices, num_devices);
   if (fd < 0)
      goto err;

   close(default_fd);

   *different_device = !!strcmp(default_tag, prime);

   free(default_tag);
   free(prime);
   return fd;

 err:
   *different_device = 0;

   free(default_tag);
   free(prime);
   return default_fd;
}
#else
int loader_get_user_preferred_fd(int default_fd, int *different_device)
{
   *different_device = 0;
   return default_fd;
}
#endif

#if defined(HAVE_SYSFS) || defined(HAVE_LIBDRM)
static int
dev_node_from_fd(int fd, unsigned int *maj, unsigned int *min)
{
   struct stat buf;

   if (fstat(fd, &buf) < 0) {
      log_(_LOADER_WARNING, "MESA-LOADER: failed to stat fd %d\n", fd);
      return -1;
   }

   if (!S_ISCHR(buf.st_mode)) {
      log_(_LOADER_WARNING, "MESA-LOADER: fd %d not a character device\n", fd);
      return -1;
   }

   *maj = major(buf.st_rdev);
   *min = minor(buf.st_rdev);

   return 0;
}
#endif

#if defined(HAVE_LIBDRM)

static int
drm_get_pci_id_for_fd(int fd, int *vendor_id, int *chip_id)
{
   drmDevicePtr device;
   int ret;

   if (drmGetDevice(fd, &device) == 0) {
      if (device->bustype == DRM_BUS_PCI) {
         *vendor_id = device->deviceinfo.pci->vendor_id;
         *chip_id = device->deviceinfo.pci->device_id;
         ret = 1;
      }
      else {
         log_(_LOADER_WARNING, "MESA-LOADER: device is not located on the PCI bus\n");
         ret = 0;
      }
      drmFreeDevice(&device);
   }
   else {
      log_(_LOADER_WARNING, "MESA-LOADER: device is not located on the PCI bus\n");
      ret = 0;
   }

   return ret;
}
#endif


int
loader_get_pci_id_for_fd(int fd, int *vendor_id, int *chip_id)
{
#if HAVE_LIBDRM
   if (drm_get_pci_id_for_fd(fd, vendor_id, chip_id))
      return 1;
#endif
   return 0;
}


#if HAVE_SYSFS
static char *
sysfs_get_device_name_for_fd(int fd)
{
   char *device_name = NULL;
   unsigned int maj, min;
   FILE *f;
   char buf[0x40];
   static const char match[9] = "\nDEVNAME=";
   int expected = 1;

   if (dev_node_from_fd(fd, &maj, &min) < 0)
      return NULL;

   snprintf(buf, sizeof(buf), "/sys/dev/char/%d:%d/uevent", maj, min);
   if (!(f = fopen(buf, "r")))
       return NULL;

   while (expected < sizeof(match)) {
      int c = getc(f);

      if (c == EOF) {
         fclose(f);
         return NULL;
      } else if (c == match[expected] )
         expected++;
      else
         expected = 0;
   }

   strcpy(buf, "/dev/");
   if (fgets(buf + 5, sizeof(buf) - 5, f)) {
      buf[strcspn(buf, "\n")] = '\0';
      device_name = strdup(buf);
   }

   fclose(f);
   return device_name;
}
#endif

#if defined(HAVE_LIBDRM)
static char *
drm_get_device_name_for_fd(int fd)
{
   unsigned int maj, min;
   char buf[0x40];
   int n;

   if (dev_node_from_fd(fd, &maj, &min) < 0)
      return NULL;

   n = snprintf(buf, sizeof(buf), DRM_DEV_NAME, DRM_DIR_NAME, min);
   if (n == -1 || n >= sizeof(buf))
      return NULL;

   return strdup(buf);
}
#endif

char *
loader_get_device_name_for_fd(int fd)
{
   char *result = NULL;

#if HAVE_SYSFS
   if ((result = sysfs_get_device_name_for_fd(fd)))
      return result;
#endif
#if HAVE_LIBDRM
   if ((result = drm_get_device_name_for_fd(fd)))
      return result;
#endif
   return result;
}

char *
loader_get_driver_for_fd(int fd, unsigned driver_types)
{
   int vendor_id, chip_id, i, j;
   char *driver = NULL;

   if (!driver_types)
      driver_types = _LOADER_GALLIUM | _LOADER_DRI;

   if (!loader_get_pci_id_for_fd(fd, &vendor_id, &chip_id)) {

#if HAVE_LIBDRM
      /* fallback to drmGetVersion(): */
      drmVersionPtr version = drmGetVersion(fd);

      if (!version) {
         log_(_LOADER_WARNING, "failed to get driver name for fd %d\n", fd);
         return NULL;
      }

      driver = strndup(version->name, version->name_len);
      log_(_LOADER_INFO, "using driver %s for %d\n", driver, fd);

      drmFreeVersion(version);
#endif

      return driver;
   }

   for (i = 0; driver_map[i].driver; i++) {
      if (vendor_id != driver_map[i].vendor_id)
         continue;

      if (!(driver_types & driver_map[i].driver_types))
         continue;

      if (driver_map[i].predicate && !driver_map[i].predicate(fd))
         continue;

      if (driver_map[i].num_chips_ids == -1) {
         driver = strdup(driver_map[i].driver);
         goto out;
      }

      for (j = 0; j < driver_map[i].num_chips_ids; j++)
         if (driver_map[i].chip_ids[j] == chip_id) {
            driver = strdup(driver_map[i].driver);
            goto out;
         }
   }

out:
   log_(driver ? _LOADER_DEBUG : _LOADER_WARNING,
         "pci id for fd %d: %04x:%04x, driver %s\n",
         fd, vendor_id, chip_id, driver);
   return driver;
}

void
loader_set_logger(void (*logger)(int level, const char *fmt, ...))
{
   log_ = logger;
}
