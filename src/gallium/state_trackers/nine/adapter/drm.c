/*
 * Copyright 2011 Joakim Sindholt <opensource@zhasha.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * on the rights to use, copy, modify, merge, publish, distribute, sub
 * license, and/or sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHOR(S) AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE. */

#include "../adapter9.h"

#include "pipe/p_screen.h"
#include "pipe/p_state.h"
#include "state_tracker/drm_driver.h"

#include "d3ddrm.h"

#include <drm/drm.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#define DBG_CHANNEL DBG_ADAPTER

#define VERSION_DWORD(hi, lo) \
    ((DWORD)( \
        ((DWORD)((hi) & 0xFFFF) << 16) | \
         (DWORD)((lo) & 0xFFFF) \
    ))

/* Regarding os versions, we should not define our own as that would simply be
 * weird. Defaulting to Win2k/XP seems sane considering the origin of D3D9. The
 * driver also defaults to being a generic D3D9 driver, which of course only
 * matters if you're actually using the DDI. */
#define VERSION_HIGH    VERSION_DWORD(0x0006, 0x000E) /* winxp, d3d9 */
#define VERSION_LOW     VERSION_DWORD(0x0000, 0x0001) /* version, build */

extern struct drm_driver_descriptor driver_descriptor;
extern struct pipe_screen *create_screen_ref(struct pipe_screen *hal);

/* read a DWORD in the form 0xnnnnnnnn, which is how sysfs pci id stuff is
 * formatted. */
static INLINE DWORD
read_file_dword( const char *name )
{
    char buf[32];
    int fd, r;

    fd = open(name, O_RDONLY);
    if (fd < 0) {
        DBG("Unable to get PCI information from `%s'\n", name);
        return 0;
    }

    r = read(fd, buf, 32);
    close(fd);

    return (r > 0) ? (DWORD)strtol(buf, NULL, 0) : 0;
}

/* sysfs doesn't expose the revision as its own file, so this function grabs a
 * dword at an offset in the raw PCI header. The reason this isn't used for all
 * data is that the kernel will make corrections but not expose them in the raw
 * header bytes. */
static INLINE DWORD
read_config_dword( int fd,
                   unsigned offset )
{
    DWORD r = 0;

    if (lseek(fd, offset, SEEK_SET) != offset) { return 0; }
    if (read(fd, &r, 4) != 4) { return 0; }

    return r;
}

static INLINE void
get_bus_info( int fd,
              DWORD *vendorid,
              DWORD *deviceid,
              DWORD *subsysid,
              DWORD *revision )
{
    drm_unique_t u;

    u.unique_len = 0;
    u.unique = NULL;

    if (ioctl(fd, DRM_IOCTL_GET_UNIQUE, &u)) { return; }
    u.unique = CALLOC(u.unique_len+1, 1);

    if (ioctl(fd, DRM_IOCTL_GET_UNIQUE, &u)) { return; }
    u.unique[u.unique_len] = '\0';

    DBG("DRM Device BusID: %s\n", u.unique);
    if (strncmp("pci:", u.unique, 4) == 0) {
        char fname[512]; /* this ought to be enough */
        int l = snprintf(fname, 512, "/sys/bus/pci/devices/%s/", u.unique+4);

        /* VendorId */
        snprintf(fname+l, 512-l, "vendor");
        *vendorid = read_file_dword(fname);
        /* DeviceId */
        snprintf(fname+l, 512-l, "device");
        *deviceid = read_file_dword(fname);
        /* SubSysId */
        snprintf(fname+l, 512-l, "subsystem_device");
        *subsysid = (read_file_dword(fname) << 16) & 0xFFFF0000;
        snprintf(fname+l, 512-l, "subsystem_vendor");
        *subsysid |= read_file_dword(fname) & 0x0000FFFF;
        /* Revision */
        {
            int cfgfd;

            snprintf(fname+l, 512-l, "config");
            cfgfd = open(fname, O_RDONLY);
            if (cfgfd >= 0) {
                *revision = read_config_dword(cfgfd, 0x8) & 0x000000FF;
                close(cfgfd);
            } else {
                DBG("Unable to get raw PCI information from `%s'\n", fname);
            }
        }
        DBG("PCI info: vendor=0x%04x, device=0x%04x, subsys=0x%08x, rev=%d\n",
            *vendorid, *deviceid, *subsysid, *revision);
    } else {
        DBG("Unsupported BusID type.\n");
    }

    FREE(u.unique);
}

static INLINE enum pipe_format
choose_format( struct pipe_screen *screen,
               unsigned cpp )
{
    int i, count = 0;
    enum pipe_format formats[4];

    switch (cpp) {
    case 4:
        formats[count++] = PIPE_FORMAT_B8G8R8A8_UNORM;
        formats[count++] = PIPE_FORMAT_A8R8G8B8_UNORM;
        break;
    case 3:
        formats[count++] = PIPE_FORMAT_B8G8R8X8_UNORM;
        formats[count++] = PIPE_FORMAT_X8R8G8B8_UNORM;
        formats[count++] = PIPE_FORMAT_B8G8R8A8_UNORM;
        formats[count++] = PIPE_FORMAT_A8R8G8B8_UNORM;
        break;
    case 2:
        formats[count++] = PIPE_FORMAT_B5G6R5_UNORM;
        break;
    default:
        break;
    }
    for (i = 0; i < count; ++i) {
        if (screen->is_format_supported(screen, formats[i], PIPE_TEXTURE_2D, 0,
                                        PIPE_BIND_RENDER_TARGET)) {
            return formats[i];
        }
    }
    return PIPE_FORMAT_NONE;
}

static HRESULT
winsys_to_resource( ID3DPresent *present,
                    HWND hwnd,
                    RECT *rect,
                    RGNDATA **rgn,
                    struct pipe_screen *screen,
                    struct pipe_resource **res )
{
    struct pipe_resource templ;
    struct winsys_handle handle;
    D3DDRM_BUFFER drmbuf;
    HRESULT hr;
    
    /* get a real backbuffer handle from the windowing system */
    hr = ID3DPresent_GetBuffer(present, hwnd, &drmbuf, rect, rgn);
    if (FAILED(hr)) {
        DBG("Unable to get presentation backend display buffer.\n");
        return hr;
    } else if (hr == D3DOK_WINDOW_OCCLUDED) {
        return hr;
    }

    handle.type = DRM_API_HANDLE_TYPE_SHARED;
    handle.handle = drmbuf.iName;
    handle.stride = drmbuf.dwStride;

    memset(&templ, 0, sizeof(templ));
    templ.target = PIPE_TEXTURE_2D;
    templ.last_level = 0;
    templ.width0 = drmbuf.dwWidth;
    templ.height0 = drmbuf.dwHeight;
    templ.depth0 = 1;
    templ.array_size = 1;
    templ.format = choose_format(screen, drmbuf.dwCPP);
    templ.bind = PIPE_BIND_DISPLAY_TARGET | PIPE_BIND_RENDER_TARGET;

    *res = screen->resource_from_handle(screen, &templ, &handle);
    if (!*res) {
        DBG("Invalid resource obtained from ID3DPresent backend.\n");
        return D3DERR_DRIVERINTERNALERROR;
    }
    
    return D3D_OK;
}

static HRESULT
create_adapter_drm( int fd,
                    ID3DAdapter9 **ppAdapter )
{
    struct pipe_screen *ref, *hal;
    D3DADAPTER_IDENTIFIER9 drvid;
    HRESULT hr;

    DBG("fd=%i ppAdapter=%p\n", fd, ppAdapter);

    hal = driver_descriptor.create_screen(fd);
    if (!hal) {
        DBG("Unable to create drm screen.\n");
        return D3DERR_DRIVERINTERNALERROR; /* guess */
    }

    /* Ref screen on DRM adapters */
    ref = create_screen_ref(hal);
    if (!ref) {
        DBG("Unable to create ref screen. D3DDEVTYPE_REF and "
            "D3DDEVTYPE_NULLREF will not be available.\n");
    }

    {
        int i, len = strlen(driver_descriptor.name);

        memset(&drvid, 0, sizeof(drvid));
        get_bus_info(fd, &drvid.VendorId, &drvid.DeviceId,
                     &drvid.SubSysId, &drvid.Revision);

        strncpy(drvid.Driver, driver_descriptor.driver_name, 512);
        strncpy(drvid.DeviceName, hal->get_name(hal), 32);
        snprintf(drvid.Description, 512, "Gallium 0.4 with %s",
                 hal->get_vendor(hal));

        drvid.DriverVersionLowPart = VERSION_LOW;
        drvid.DriverVersionHighPart = VERSION_HIGH;

        /* To make a pseudo-real GUID we put the entire version in first, and
         * add as much of the device name as possible in the byte array. */
        drvid.DeviceIdentifier.Data1 = drvid.DriverVersionHighPart;
        drvid.DeviceIdentifier.Data2 = drvid.DriverVersionLowPart >> 16;
        drvid.DeviceIdentifier.Data3 = drvid.DriverVersionLowPart & 0xFFFF;
        for (i = 0; i < 8; ++i) {
            drvid.DeviceIdentifier.Data4[i] =
                (i < len) ? driver_descriptor.name[i] : 0;
        }

        drvid.WHQLLevel = 1; /* This fakes WHQL validaion */
    }

    /* Fake NVIDIA binary driver on Windows.
     *
     * OS version: 4=95/98/NT4, 5=2000, 6=2000/XP, 7=Vista, 8=Win7
     */
    {
        strncpy(drvid.Driver, "nvd3dum.dll", sizeof(drvid.Driver));
        strncpy(drvid.Description, "NVIDIA GeForce GTX 680", sizeof(drvid.Description));
        drvid.DriverVersionLowPart = VERSION_DWORD(12, 6658); /* minor, build */
        drvid.DriverVersionHighPart = VERSION_DWORD(6, 15); /* OS, major */
        drvid.SubSysId = 0;
        drvid.Revision = 0;
        drvid.DeviceIdentifier.Data1 = 0xaeb2cdd4;
        drvid.DeviceIdentifier.Data2 = 0x6e41;
        drvid.DeviceIdentifier.Data3 = 0x43ea;
        drvid.DeviceIdentifier.Data4[0] = 0x94;
        drvid.DeviceIdentifier.Data4[1] = 0x1c;
        drvid.DeviceIdentifier.Data4[2] = 0x83;
        drvid.DeviceIdentifier.Data4[3] = 0x61;
        drvid.DeviceIdentifier.Data4[4] = 0xcc;
        drvid.DeviceIdentifier.Data4[5] = 0x76;
        drvid.DeviceIdentifier.Data4[6] = 0x07;
        drvid.DeviceIdentifier.Data4[7] = 0x81;
        drvid.WHQLLevel = 0;
    }

    hr = NineAdapter9_new(hal, ref, &drvid, winsys_to_resource, NULL, NULL,
                          (struct NineAdapter9 **)ppAdapter);
    if (FAILED(hr)) {
        if (ref) {
            if (ref->destroy) {
                ref->destroy(ref);
            } else if (hal->destroy) {
                hal->destroy(hal);
            }
        } else {
            if (hal->destroy) { hal->destroy(hal); }
        }
    }
    return hr;
}

/* drm driver entry point */
PUBLIC const struct D3DAdapter9DescriptorDRM D3DAdapter9DRMEntryPoint = {
    .major_version = 0,
    .minor_version = 0,
    .create_adapter = create_adapter_drm
};
