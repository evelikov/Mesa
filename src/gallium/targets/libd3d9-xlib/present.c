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
#include "d3dpresent.h"

#include "present.h"
#include "guid.h"
#include "debug.h"

#include <xcb/xproto.h>
#include <xcb/dri2.h>
#include <xcb/xfixes.h>

#include <d3ddrm.h>

#define HWND_TO_DRAWABLE(hwnd) ((xcb_drawable_t)((uintptr_t)(hwnd)))

struct NinePresentXlib
{
    /* COM vtable */
    void *vtable;
    /* IUnknown reference count */
    UINT refs;

    xcb_connection_t *c;
    xcb_drawable_t hwnd;
    xcb_xfixes_region_t region;
    xcb_void_cookie_t region_cookie;
    unsigned dri2_minor;

    D3DPRESENT_PARAMETERS params;
};

static boolean
create_drawable( struct NinePresentXlib *This,
                 xcb_drawable_t wnd )
{
    xcb_generic_error_t *err;
    xcb_void_cookie_t cookie = xcb_dri2_create_drawable_checked(This->c, wnd);
    err = xcb_request_check(This->c, cookie);
    if (err) {
        ERROR("DRI2CreateDrawable failed with error %hhu "
              "(major=%hhu, minor=%hu, drawable=0x%x)\n",
              err->error_code, err->major_code, err->minor_code, wnd);
        return FALSE;
    }
    return TRUE;
}

static void
destroy_drawable( struct NinePresentXlib *This,
                  xcb_drawable_t wnd )
{
    xcb_generic_error_t *err;
    xcb_void_cookie_t cookie = xcb_dri2_destroy_drawable_checked(This->c, wnd);
    err = xcb_request_check(This->c, cookie);
    if (err) {
        ERROR("DRI2DestroyDrawable failed with error %hhu "
              "(major=%hhu, minor=%hu, drawable=0x%x)\n",
              err->error_code, err->major_code, err->minor_code, wnd);
    }
}

static HRESULT WINAPI
NinePresentXlib_QueryInterface( struct NinePresentXlib *This,
                                REFIID riid,
                                void **ppvObject )
{
    if (!ppvObject) { return E_POINTER; }
    if (GUID_equal(&IID_ID3DPresent, riid) ||
        GUID_equal(&IID_IUnknown, riid)) {
        *ppvObject = This;
        This->refs++;
        return S_OK;
    }
    *ppvObject = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI
NinePresentXlib_AddRef( struct NinePresentXlib *This )
{
    return ++This->refs;
}

static ULONG WINAPI
NinePresentXlib_Release( struct NinePresentXlib *This )
{
    if (--This->refs == 0) {
        /* dtor */
        if (This->region) {
            xcb_xfixes_destroy_region(This->c, This->region);
        }

        destroy_drawable(This, HWND_TO_DRAWABLE(This->params.hDeviceWindow));
        if (This->hwnd &&
            This->hwnd != HWND_TO_DRAWABLE(This->params.hDeviceWindow)) {
            destroy_drawable(This, This->hwnd);
        }
        free(This);
        return 0;
    }
    return This->refs;
}

static HRESULT WINAPI
NinePresentXlib_GetPresentParameters( struct NinePresentXlib *This,
                                      D3DPRESENT_PARAMETERS *pPresentationParameters )
{
    *pPresentationParameters = This->params;
    return D3D_OK;
}

static HRESULT WINAPI
NinePresentXlib_GetBuffer( struct NinePresentXlib *This,
                           HWND hWndOverride,
                           void *pBuffer,
                           RECT *pRect,
                           RGNDATA **ppRegion )
{
    xcb_dri2_get_buffers_cookie_t cookie;
    xcb_dri2_get_buffers_reply_t *reply;
    xcb_dri2_dri2_buffer_t *buffers;
    xcb_get_geometry_cookie_t geomcookie;
    xcb_get_geometry_reply_t *geomreply;
    xcb_generic_error_t *err = NULL;
    D3DDRM_BUFFER *drmbuf = pBuffer;
    static const uint32_t attachments[] = {
        XCB_DRI2_ATTACHMENT_BUFFER_BACK_LEFT
    };

    if (hWndOverride && hWndOverride != This->params.hDeviceWindow) {
        xcb_drawable_t wnd_override = HWND_TO_DRAWABLE(hWndOverride);
        if (wnd_override != This->hwnd) {
            if (This->hwnd != HWND_TO_DRAWABLE(This->params.hDeviceWindow) &&
                This->hwnd != 0) {
                destroy_drawable(This, This->hwnd);
            }
            This->hwnd = wnd_override;
            if (!create_drawable(This, This->hwnd)) {
                This->hwnd = 0;
                ERROR("Unable to create DRI2 drawable for override window "
                      "(XID: 0x%x)\n", wnd_override);
                return D3DERR_DRIVERINTERNALERROR;
            }
        }
    } else {
        This->hwnd = HWND_TO_DRAWABLE(This->params.hDeviceWindow);
    }

    /* XXX base this on events instead of calling every single frame */
    geomcookie = xcb_get_geometry(This->c, This->hwnd);
    cookie = xcb_dri2_get_buffers(This->c, This->hwnd, 1, 1, attachments);

    geomreply = xcb_get_geometry_reply(This->c, geomcookie, &err);
    if (err) {
        WARNING("XGetGeometry failed with error %hhu (major=%hhu, minor=%hu)\n",
                err->error_code, err->major_code, err->minor_code);
        if (geomreply) { free(geomreply); }
        return D3DERR_DRIVERINTERNALERROR;
    } else {
        pRect->right = geomreply->width;
        pRect->bottom = geomreply->height;
        free(geomreply);
    }
    pRect->left = 0;
    pRect->top = 0;
    *ppRegion = NULL;

    reply = xcb_dri2_get_buffers_reply(This->c, cookie, &err);
    if (err) {
        ERROR("DRI2GetBuffers failed with error %hhu (major=%hhu, minor=%hu)\n",
              err->error_code, err->major_code, err->minor_code);
        return D3DERR_DRIVERINTERNALERROR;
    }
    buffers = xcb_dri2_get_buffers_buffers(reply);

    drmbuf->iName = buffers[0].name;
    drmbuf->dwWidth = reply->width;
    drmbuf->dwHeight = reply->height;
    drmbuf->dwStride = buffers[0].pitch;
    drmbuf->dwCPP = buffers[0].cpp;
    free(reply);

    if (This->region) {
        xcb_xfixes_destroy_region(This->c, This->region);
        This->region = 0;
    }
    if (1/*This->dri2_minor < 3*/) {
        This->region = xcb_generate_id(This->c);
        This->region_cookie =
            xcb_xfixes_create_region_from_window_checked(This->c, This->region,
                                                         This->hwnd, 0);
    }

    return D3D_OK;
}

static HRESULT WINAPI
NinePresentXlib_GetFrontBuffer( struct NinePresentXlib *This,
                                void *pBuffer )
{
    /* TODO: implement */
    return D3DERR_INVALIDCALL;
}

static HRESULT WINAPI
NinePresentXlib_Present( struct NinePresentXlib *This,
                         DWORD Flags )
{
    xcb_generic_error_t *err = NULL;

    if (1/*This->dri2_minor < 3*/) {
        xcb_dri2_copy_region_cookie_t cookie;
        xcb_dri2_copy_region_reply_t *reply;

        err = xcb_request_check(This->c, This->region_cookie);
        if (err) {
            ERROR("XXFixesCreateRegionFromWindow failed with error %hhu "
                  "(major=%hhu, minor=%hu)\n", err->error_code,
                  err->major_code, err->minor_code);
            This->region = 0;
            return D3DERR_DRIVERINTERNALERROR;
        }

        cookie = xcb_dri2_copy_region(This->c, This->hwnd, This->region,
                                      XCB_DRI2_ATTACHMENT_BUFFER_FRONT_LEFT,
                                      XCB_DRI2_ATTACHMENT_BUFFER_BACK_LEFT);
        if (!(Flags & D3DPRESENT_DONOTWAIT)) {
            reply = xcb_dri2_copy_region_reply(This->c, cookie, &err);
            if (err) {
                ERROR("DRI2CopyRegion failed with error %hhu "
                      "(major=%hhu, minor=%hu)\n", err->error_code,
                      err->major_code, err->minor_code);
                return D3DERR_DRIVERINTERNALERROR;
            }
            free(reply);
        }
    }

    return D3D_OK;
}

static HRESULT WINAPI
NinePresentXlib_GetRasterStatus( struct NinePresentXlib *This,
                                 D3DRASTER_STATUS *pRasterStatus )
{
    /* TODO: implement */
    return D3DERR_INVALIDCALL;
}

static HRESULT WINAPI
NinePresentXlib_GetDisplayMode( struct NinePresentXlib *This,
                                D3DDISPLAYMODEEX *pMode )
{
    /* TODO: implement */
    return D3DERR_INVALIDCALL;
}

static HRESULT WINAPI
NinePresentXlib_GetPresentStats( struct NinePresentXlib *This,
                                 D3DPRESENTSTATS *pStats )
{
    /* TODO: implement */
    return D3DERR_INVALIDCALL;
}

static HRESULT WINAPI
NinePresentXlib_GetCursorPos( struct NinePresentXlib *This,
                              POINT *pPoint )
{
   /* TODO: implement */
    return D3DERR_INVALIDCALL;
}

static HRESULT WINAPI
NinePresentXlib_SetGammaRamp( struct NinePresentXlib *This,
                              const D3DGAMMARAMP *pRamp,
                              HWND hWndOverride )
{
   /* TODO: implement */
   return D3DERR_INVALIDCALL;
}

static ID3DPresentVtbl NinePresentXlib_vtable = {
    (void *)NinePresentXlib_QueryInterface,
    (void *)NinePresentXlib_AddRef,
    (void *)NinePresentXlib_Release,
    (void *)NinePresentXlib_GetPresentParameters,
    (void *)NinePresentXlib_GetBuffer,
    (void *)NinePresentXlib_GetFrontBuffer,
    (void *)NinePresentXlib_Present,
    (void *)NinePresentXlib_GetRasterStatus,
    (void *)NinePresentXlib_GetDisplayMode,
    (void *)NinePresentXlib_GetPresentStats,
    (void *)NinePresentXlib_GetCursorPos,
    (void *)NinePresentXlib_SetGammaRamp
};

static HRESULT
NinePresentXlib_new( xcb_connection_t *c,
                     D3DPRESENT_PARAMETERS *params,
                     HWND focus_wnd,
                     unsigned dri2_minor,
                     struct NinePresentXlib **out )
{
    struct NinePresentXlib *This;
    xcb_get_geometry_cookie_t cookie;
    xcb_get_geometry_reply_t *reply;
    xcb_generic_error_t *err = NULL;
    unsigned xwidth, xheight;

    if (params->hDeviceWindow) { focus_wnd = params->hDeviceWindow; }
    if (!focus_wnd) {
        ERROR("No HWND specified for presentation backend.\n");
        return D3DERR_INVALIDCALL;
    }
    cookie = xcb_get_geometry(c, HWND_TO_DRAWABLE(focus_wnd));

    This = malloc(sizeof(struct NinePresentXlib));
    if (!This) { OOM(); }

    This->vtable = &NinePresentXlib_vtable;
    This->refs = 1;
    This->c = c;
    This->hwnd = 0;
    This->region = 0;
    This->dri2_minor = dri2_minor;

    /* sanitize presentation parameters */
    if (params->SwapEffect == D3DSWAPEFFECT_COPY &&
        params->BackBufferCount > 1) {
        WARNING("present: BackBufferCount > 1 when SwapEffect == "
                "D3DSWAPEFFECT_COPY.\n");
        params->BackBufferCount = 1;
    }

    /* XXX 30 for Ex */
    if (params->BackBufferCount > 3) {
        WARNING("present: BackBufferCount > 3.\n");
        params->BackBufferCount = 3;
    }

    if (params->BackBufferCount == 0) {
        params->BackBufferCount = 1;
    }

    if (params->BackBufferFormat == D3DFMT_UNKNOWN) {
        params->BackBufferFormat = D3DFMT_A8R8G8B8;
    }

    reply = xcb_get_geometry_reply(This->c, cookie, &err);
    if (err) {
        WARNING("XGetGeometry failed with error %hhu (major=%hhu, minor=%hu)\n",
                err->error_code, err->major_code, err->minor_code);
        xwidth = 640;
        xheight = 480;
    } else {
        xwidth = reply->width;
        xheight = reply->height;
        free(reply);
    }

    if (params->BackBufferWidth == 0) { params->BackBufferWidth = xwidth; }
    if (params->BackBufferHeight == 0) { params->BackBufferHeight = xheight; }
    This->params = *params;
    This->params.hDeviceWindow = focus_wnd;

    if (!create_drawable(This, HWND_TO_DRAWABLE(This->params.hDeviceWindow))) {
        free(This);
        return D3DERR_DRIVERINTERNALERROR;
    }
    *out = This;

    return D3D_OK;
}

struct NinePresentFactoryXlib
{
    /* COM vtable */
    void *vtable;
    /* IUnknown reference count */
    UINT refs;

    struct NinePresentXlib **present_backends;
    unsigned npresent_backends;

    /* X11 info */
    xcb_connection_t *c;
    unsigned dri2_minor;
};

static HRESULT WINAPI
NinePresentFactoryXlib_QueryInterface( struct NinePresentFactoryXlib *This,
                                       REFIID riid,
                                       void **ppvObject )
{
    if (!ppvObject) { return E_POINTER; }
    if (GUID_equal(&IID_ID3DPresentFactory, riid) ||
        GUID_equal(&IID_IUnknown, riid)) {
        *ppvObject = This;
        This->refs++;
        return S_OK;
    }
    *ppvObject = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI
NinePresentFactoryXlib_AddRef( struct NinePresentFactoryXlib *This )
{
    return ++This->refs;
}

static ULONG WINAPI
NinePresentFactoryXlib_Release( struct NinePresentFactoryXlib *This )
{
    if (--This->refs == 0) {
        unsigned i;
        if (This->present_backends) {
            for (i = 0; i < This->npresent_backends; ++i) {
                NinePresentXlib_Release(This->present_backends[i]);
            }
            free(This->present_backends);
        }
        free(This);
        return 0;
    }
    return This->refs;
}

static UINT WINAPI
NinePresentFactoryXlib_GetMultiheadCount( struct NinePresentFactoryXlib *This )
{
    /* XXX get this from cached XRandR info */
    return 1;
}

static HRESULT WINAPI
NinePresentFactoryXlib_GetPresent( struct NinePresentFactoryXlib *This,
                                   UINT Index,
                                   ID3DPresent **ppPresent )
{
    if (Index >= NinePresentFactoryXlib_GetMultiheadCount(This)) {
        return D3DERR_INVALIDCALL;
    }
    NinePresentXlib_AddRef(This->present_backends[Index]);
    *ppPresent = (ID3DPresent *)This->present_backends[Index];

    return D3D_OK;
}

static HRESULT WINAPI
NinePresentFactoryXlib_CreateAdditionalPresent( struct NinePresentFactoryXlib *This,
                                                D3DPRESENT_PARAMETERS *pPresentationParameters,
                                                ID3DPresent **ppPresent )
{
    return D3DERR_INVALIDCALL;
}

static ID3DPresentFactoryVtbl NinePresentFactoryXlib_vtable = {
    (void *)NinePresentFactoryXlib_QueryInterface,
    (void *)NinePresentFactoryXlib_AddRef,
    (void *)NinePresentFactoryXlib_Release,
    (void *)NinePresentFactoryXlib_GetMultiheadCount,
    (void *)NinePresentFactoryXlib_GetPresent,
    (void *)NinePresentFactoryXlib_CreateAdditionalPresent
};

HRESULT
NinePresentFactoryXlib_new( xcb_connection_t *c,
                            HWND focus_wnd,
                            D3DPRESENT_PARAMETERS *params,
                            unsigned nparams,
                            unsigned dri2_minor,
                            ID3DPresentFactory **out )
{
    struct NinePresentFactoryXlib *This =
        calloc(1, sizeof(struct NinePresentFactoryXlib));
    HRESULT hr;
    unsigned i;

    if (!This) { return E_OUTOFMEMORY; }

    This->vtable = &NinePresentFactoryXlib_vtable;
    This->refs = 1;
    This->c = c;
    This->dri2_minor = dri2_minor;
    This->npresent_backends = nparams;
    This->present_backends = calloc(This->npresent_backends,
                                    sizeof(struct NinePresentXlib *));
    if (!This->present_backends) {
        NinePresentFactoryXlib_Release(This);
        return E_OUTOFMEMORY;
    }

    for (i = 0; i < This->npresent_backends; ++i) {
        hr = NinePresentXlib_new(c, &params[i], focus_wnd,
                                 dri2_minor, &This->present_backends[i]);
        if (FAILED(hr)) {
            NinePresentFactoryXlib_Release(This);
            return hr;
        }
    }
    *out = (ID3DPresentFactory *)This;

    return D3D_OK;
}
