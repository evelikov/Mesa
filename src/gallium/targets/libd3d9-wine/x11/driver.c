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

#include "d3d9.h"
#include "d3dadapter/d3dadapter9.h"
#include "d3dadapter/drm.h"

#include "../debug.h"
#include "../driver.h"
#include "../guid.h"
#include "present.h"

#undef _WIN32
#include <X11/Xlib.h>
#include <X11/extensions/Xfixes.h>
#include "dri2.h"
#define _WIN32

#include <libdrm/drm.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <fcntl.h>

struct NineWineDriverX11
{
    void *vtable;
    UINT refs;

    /* connection */
    Display *dpy;
    unsigned dri2_major, dri2_minor;
};

static HRESULT WINAPI
NineWineDriverX11_QueryInterface( struct NineWineDriverX11 *This,
                                  REFIID riid,
                                  void **ppvObject )
{
    if (!ppvObject) { return E_POINTER; }
    if (GUID_equal(&IID_ID3DWineDriver, riid) ||
        GUID_equal(&IID_IUnknown, riid)) {
        *ppvObject = This;
        This->refs++;
        return S_OK;
    }
    *ppvObject = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI
NineWineDriverX11_AddRef( struct NineWineDriverX11 *This )
{
    return ++This->refs;
}

static ULONG WINAPI
NineWineDriverX11_Release( struct NineWineDriverX11 *This )
{
    if (--This->refs == 0) {
        if (This->dpy) { XCloseDisplay(This->dpy); }
        HeapFree(GetProcessHeap(), 0, This);
        return 0;
    }
    return This->refs;
}

static HRESULT WINAPI
NineWineDriverX11_CreatePresentGroup( struct NineWineDriverX11 *This,
                                      const WCHAR *lpDeviceName,
                                      UINT Adapter,
                                      HWND hFocusWnd,
                                      D3DPRESENT_PARAMETERS *pParams,
                                      unsigned nParams,
                                      ID3DPresentGroup **ppPresentGroup )
{
    return NineWinePresentGroupX11_new(This->dpy, lpDeviceName, Adapter,
                                       hFocusWnd, pParams, nParams,
                                       This->dri2_major, This->dri2_minor,
                                       ppPresentGroup);
}

static HRESULT WINAPI
NineWineDriverX11_CreateAdapter9( struct NineWineDriverX11 *This,
                                  HDC hdc,
                                  ID3DAdapter9 **ppAdapter )
{
    const struct D3DAdapter9DRM *drm = D3DAdapter9GetProc(D3DADAPTER9DRM_NAME);
    Drawable drawable = 0;
    Window root;
    drm_auth_t auth;
    int fd;
    HRESULT hr;

    if (!drm) {
        _WARNING("DRM drivers are not supported on your system.\n");
        return D3DERR_DRIVERINTERNALERROR;
    }
    if (drm->major_version != D3DADAPTER9DRM_MAJOR) {
        _WARNING("D3DAdapter9DRM version %d.%d mismatch with expected %d.%d",
                 drm->major_version, drm->minor_version,
                 D3DADAPTER9DRM_MAJOR, D3DADAPTER9DRM_MINOR);
        return D3DERR_DRIVERINTERNALERROR;
    }

    drawable = X11DRV_ExtEscape_GET_DRAWABLE(hdc);

    { /* XGetGeometry */
        unsigned udummy;
        int dummy;

        if (!XGetGeometry(This->dpy, drawable, &root, &dummy, &dummy,
                          &udummy, &udummy, &udummy, &udummy)) {
            _WARNING("XGetGeometry failed: Unable to get root window of "
                     "drawable %u\n", drawable);
            return D3DERR_DRIVERINTERNALERROR;
        }
    }

    { /* DRI2Connect */
        char *driver, *device;

        if (!DRI2Connect(This->dpy, root, DRI2DriverDRI, &driver, &device)) {
            _WARNING("DRI2Connect failed: Unable to connect DRI2 on"
                     "window %u\n", root);
            return D3DERR_DRIVERINTERNALERROR;
        }

        fd = open(device, O_RDWR);
        if (fd < 0) {
            _WARNING("Failed to open drm fd: %s (%s)\n",
                      strerror(errno), device);
            free(driver);
            free(device);
            return D3DERR_DRIVERINTERNALERROR;
        }

        /* authenticate */
        if (ioctl(fd, DRM_IOCTL_GET_MAGIC, &auth) != 0) {
            _WARNING("DRM_IOCTL_GET_MAGIC failed: %s (%s)\n",
                     strerror(errno), device);
            free(driver);
            free(device);
            return D3DERR_DRIVERINTERNALERROR;
        }

        _MESSAGE("Associated `%s' with fd %d opened from `%s'\n",
                 driver, fd, device);

        free(driver);
        free(device);
    }

    { /* DRI2Authenticate */
        if (!DRI2Authenticate(This->dpy, root, auth.magic)) {
            _WARNING("DRI2Authenticate failed on fd %d\n", fd);
            return D3DERR_DRIVERINTERNALERROR;
        }
    }

    hr = drm->create_adapter(fd, ppAdapter);
    if (FAILED(hr)) {
        _WARNING("Unable to create ID3DAdapter9 for fd %d\n", fd);
        return hr;
    }

    _MESSAGE("Created ID3DAdapter9 for fd %d\n", fd);

    return D3D_OK;
}

static INLINE HRESULT
check_x11_proto( struct NineWineDriverX11 *This )
{
    int xfmaj, xfmin, dummy;

    /* query DRI2 */
    if (!DRI2QueryExtension(This->dpy)) {
        _ERROR("Xserver doesn't support DRI2.\n");
        return D3DERR_DRIVERINTERNALERROR;
    }
    if (!DRI2QueryVersion(This->dpy, &This->dri2_major, &This->dri2_minor)) {
        _ERROR("Unable to query DRI2 extension.\n");
        return D3DERR_DRIVERINTERNALERROR;
    }
    _MESSAGE("Got DRI2 version %u.%u\n", This->dri2_major, This->dri2_minor);

    /* query XFixes */
    if (!XFixesQueryExtension(This->dpy, &dummy, &dummy)) {
        _ERROR("Xserver doesn't support XFixes.\n");
        return D3DERR_DRIVERINTERNALERROR;
    }
    if (!XFixesQueryVersion(This->dpy, &xfmaj, &xfmin)) {
        _ERROR("Unable to query XFixes extension.\n");
        return D3DERR_DRIVERINTERNALERROR;
    }
    _MESSAGE("Got XFixes version %u.%u\n", xfmaj, xfmin);

    return D3D_OK;
}

static ID3DWineDriverVtbl NineWineDriverX11_vtable = {
    (void *)NineWineDriverX11_QueryInterface,
    (void *)NineWineDriverX11_AddRef,
    (void *)NineWineDriverX11_Release,
    (void *)NineWineDriverX11_CreatePresentGroup,
    (void *)NineWineDriverX11_CreateAdapter9
};

HRESULT
D3DWineDriverCreate( ID3DWineDriver **ppDriver );

HRESULT
D3DWineDriverCreate( ID3DWineDriver **ppDriver )
{
    struct NineWineDriverX11 *This =
        HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                  sizeof(struct NineWineDriverX11));
    HRESULT hr;

    if (!This) { OOM(); }

    This->vtable = &NineWineDriverX11_vtable;
    This->refs = 1;

    This->dpy = XOpenDisplay(NULL); /* This is what Wine does */
    if (!This->dpy) {
        _ERROR("Unable to connect to X11.\n");
        HeapFree(GetProcessHeap(), 0, This);
        return D3DERR_DRIVERINTERNALERROR;
    }

    hr = check_x11_proto(This);
    if (FAILED(hr)) {
        NineWineDriverX11_Release(This);
        return hr;
    }

    *ppDriver = (ID3DWineDriver *)This;

    return D3D_OK;
}
