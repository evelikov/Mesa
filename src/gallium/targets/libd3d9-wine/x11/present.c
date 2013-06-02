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

#include "../debug.h"
#include "../guid.h"
#include "present.h"

#undef _WIN32
#include <X11/Xlib.h>
#include <X11/Xlib-xcb.h>
#include <xcb/xcb.h>
#include <xcb/dri2.h>
#include <xcb/xfixes.h>
#define _WIN32

#include <d3ddrm.h>

struct NineWinePresentX11
{
    /* COM vtable */
    void *vtable;
    /* IUnknown reference count */
    UINT refs;

    xcb_connection_t *c;

    xcb_xfixes_region_t region;
    xcb_void_cookie_t region_cookie;
    RGNDATA *rgndata;

    struct {
        xcb_drawable_t drawable;
        HWND ancestor;
        HWND real;
    } device_window;

    struct {
        xcb_drawable_t drawable;
        HWND ancestor;
        HWND real;
    } current_window;

    unsigned dri2_minor;

    D3DPRESENT_PARAMETERS params;
};

/* ripped from Wine */
#define X11DRV_ESCAPE 6789
enum x11drv_escape_codes
{
    X11DRV_SET_DRAWABLE,     /* set current drawable for a DC */
    X11DRV_GET_DRAWABLE,     /* get current drawable for a DC */
    X11DRV_START_EXPOSURES,  /* start graphics exposures */
    X11DRV_END_EXPOSURES,    /* end graphics exposures */
    X11DRV_FLUSH_GL_DRAWABLE /* flush changes made to the gl drawable */
};

struct x11drv_escape_get_drawable
{
    enum x11drv_escape_codes code;         /* escape code (X11DRV_GET_DRAWABLE) */
    Drawable                 drawable;     /* X drawable */
    Drawable                 gl_drawable;  /* GL drawable */
    int                      pixel_format; /* internal GL pixel format */
};

xcb_drawable_t
X11DRV_ExtEscape_GET_DRAWABLE( HDC hdc )
{
    struct x11drv_escape_get_drawable extesc = { X11DRV_GET_DRAWABLE };

    _MESSAGE("%s\n", __FUNCTION__);

    if (ExtEscape(hdc, X11DRV_ESCAPE, sizeof(extesc), (LPCSTR)&extesc,
                  sizeof(extesc), (LPSTR)&extesc) <= 0) {
        _WARNING("Unexpected error in X Drawable lookup (hdc = %p)\n", hdc);
    }

    return extesc.drawable;
}

static xcb_drawable_t
get_drawable( HWND hwnd )
{
    xcb_drawable_t drawable;
    HDC hdc;

    hdc = GetDC(hwnd);
    drawable = X11DRV_ExtEscape_GET_DRAWABLE(hdc);
    ReleaseDC(hwnd, hdc);

    return drawable;
}

static boolean
create_drawable( struct NineWinePresentX11 *This,
                 xcb_drawable_t wnd )
{
    xcb_generic_error_t *err;
    xcb_void_cookie_t cookie = xcb_dri2_create_drawable_checked(This->c, wnd);
    err = xcb_request_check(This->c, cookie);
    if (err) {
        _ERROR("DRI2CreateDrawable failed with error %hhu (major=%hhu, "
               "minor=%hu, drawable = %u)\n", err->error_code,
               err->major_code, err->minor_code, wnd);
        return FALSE;
    }
    _MESSAGE("Created DRI2 drawable for drawable %u.\n", wnd);
    return TRUE;
}

static void
destroy_drawable( struct NineWinePresentX11 *This,
                  xcb_drawable_t wnd )
{
    xcb_generic_error_t *err;
    xcb_void_cookie_t cookie = xcb_dri2_destroy_drawable_checked(This->c, wnd);
    err = xcb_request_check(This->c, cookie);
    if (err) {
        _ERROR("DRI2DestroyDrawable failed with error %hhu (major=%hhu, "
               "minor=%hu, drawable = %u)\n", err->error_code,
               err->major_code, err->minor_code, wnd);
    }
    _MESSAGE("Destroyed DRI2 drawable for drawable %u.\n", wnd);
}

static HRESULT WINAPI
NineWinePresentX11_QueryInterface( struct NineWinePresentX11 *This,
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
    _WARNING("%s: E_NOINTERFACE\n", __FUNCTION__);
    *ppvObject = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI
NineWinePresentX11_AddRef( struct NineWinePresentX11 *This )
{
    _MESSAGE("%s(%p): %li+=1\n", __FUNCTION__, This, This->refs);
    return ++This->refs;
}

static ULONG WINAPI
NineWinePresentX11_Release( struct NineWinePresentX11 *This )
{
    _MESSAGE("%s(%p): %li-=1\n", __FUNCTION__, This, This->refs);

    if (--This->refs == 0) {
        /* dtor */
        if (This->region) {
            xcb_xfixes_destroy_region(This->c, This->region);
        }
        if (This->rgndata) {
            HeapFree(GetProcessHeap(), 0, This->rgndata);
        }

        destroy_drawable(This, This->device_window.drawable);
        if (This->current_window.drawable &&
            This->current_window.drawable != This->device_window.drawable) {
            destroy_drawable(This, This->current_window.drawable);
        }

        HeapFree(GetProcessHeap(), 0, This);
        return 0;
    }
    return This->refs;
}

static HRESULT WINAPI
NineWinePresentX11_GetPresentParameters( struct NineWinePresentX11 *This,
                                         D3DPRESENT_PARAMETERS *pPresentationParameters )
{
    *pPresentationParameters = This->params;
    return D3D_OK;
}

static HRESULT WINAPI
NineWinePresentX11_GetBuffer( struct NineWinePresentX11 *This,
                              HWND hWndOverride,
                              void *pBuffer,
                              RECT *pRect,
                              RGNDATA **ppRegion )
{
    xcb_dri2_get_buffers_cookie_t cookie;
    xcb_dri2_get_buffers_reply_t *reply;
    xcb_dri2_dri2_buffer_t *buffers;
    xcb_generic_error_t *err = NULL;
    xcb_drawable_t drawable;
    xcb_rectangle_t xrect;
    D3DDRM_BUFFER *drmbuf = pBuffer;
    HWND ancestor;
    static const uint32_t attachments[] = {
        XCB_DRI2_ATTACHMENT_BUFFER_BACK_LEFT
    };

    _MESSAGE("%s(This=%p hWndOverride=%p pBuffer=%p pRect=%p ppRegion=%p)\n",
             __FUNCTION__, This, hWndOverride, pBuffer, pRect, ppRegion);

    if (This->rgndata) {
        HeapFree(GetProcessHeap(), 0, This->rgndata);
        This->rgndata = NULL;
    }

    if (hWndOverride) {
        ancestor = GetAncestor(hWndOverride, GA_ROOT);
        if (ancestor != This->device_window.ancestor &&
            ancestor != This->current_window.ancestor) {
            if (This->current_window.ancestor) {
                destroy_drawable(This, This->current_window.drawable);
                This->current_window.real = NULL;
                This->current_window.ancestor = NULL;
                This->current_window.drawable = 0;
            }
            drawable = get_drawable(ancestor);
            if (!drawable) {
                _ERROR("DRIVERINTERNALERROR\n");
                return D3DERR_DRIVERINTERNALERROR;
            }

            if (!create_drawable(This, drawable)) {
                _ERROR("DRIVERINTERNALERROR\n");
                return D3DERR_DRIVERINTERNALERROR;
            }
            This->current_window.drawable = drawable;
        }
        This->current_window.real = hWndOverride;
        This->current_window.ancestor = ancestor;
    } else {
        This->current_window.drawable = This->device_window.drawable;
        This->current_window.real = This->device_window.real;
        This->current_window.ancestor = This->device_window.ancestor;
    }

    {
        /* TODO: don't pass rgndata to the driver, but use it for DRI2CopyRegion
        DWORD rgn_size;
        HRGN hrgn = CreateRectRgn(0, 0, 0, 0);
        if (GetWindowRgn(This->current_window.real, hrgn) != _ERROR) {
            rgn_size = GetRegionData(hrgn, 0, NULL);
            This->rgndata = HeapAlloc(GetProcessHeap(), 0, rgn_size);
            GetRegionData(hrgn, rgn_size, This->rgndata);
        }
        DeleteObject(hrgn);
        if (!This->rgndata) {
            return D3DERR_DRIVERINTERNALERROR;
        }*/
        *ppRegion = NULL;
    }
    if (This->current_window.ancestor != This->current_window.real) {
        POINT panc, pwnd;
        RECT rwnd;
        if (!GetDCOrgEx(GetDC(This->current_window.ancestor), &panc) ||
            !GetDCOrgEx(GetDC(This->current_window.real), &pwnd) ||
            !GetClientRect(This->current_window.real, &rwnd)) {
            _ERROR("Unable to get fake subwindow coordinates.\n");
            return D3DERR_DRIVERINTERNALERROR;
        }

        _MESSAGE("pAnc=%u,%u\n", panc.x, panc.y);
        _MESSAGE("pWnd=%u,%u\n", pwnd.x, pwnd.y);
        _MESSAGE("rWnd=(%u..%u)x(%u..%u)\n", rwnd.left, rwnd.right,
                 rwnd.top, rwnd.bottom);

        pRect->left = pwnd.x-panc.x;
        pRect->top = pwnd.y-panc.y;
        pRect->right = pRect->left+rwnd.right;
        pRect->bottom = pRect->top+rwnd.bottom;
    } else {
        if (!GetClientRect(This->current_window.real, pRect)) {
            _ERROR("Unable to get window size.\n");
            return D3DERR_DRIVERINTERNALERROR;
        }
    }

    /* XXX base this on events instead of calling every single frame */
    cookie = xcb_dri2_get_buffers(This->c, This->current_window.drawable,
                                  1, 1, attachments);
    reply = xcb_dri2_get_buffers_reply(This->c, cookie, &err);
    if (err) {
        _ERROR("DRI2GetBuffers failed with error %hhu (major=%hhu, "
               "minor=%hu)\n", err->error_code,
               err->major_code, err->minor_code);
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
    xrect.x = pRect->left;
    xrect.y = pRect->top;
    xrect.width = pRect->right-pRect->left;
    xrect.height = pRect->bottom-pRect->top;
    This->region = xcb_generate_id(This->c);
    This->region_cookie =
        xcb_xfixes_create_region_checked(This->c, This->region, 1, &xrect);

    _MESSAGE("%s: OK, *ppRegion=%p\n", __FUNCTION__, ppRegion ? *ppRegion : NULL);
    return D3D_OK;
}

static HRESULT WINAPI
NineWinePresentX11_GetFrontBuffer( struct NineWinePresentX11 *This,
                                   void *pBuffer )
{
    STUB(D3DERR_INVALIDCALL); /* TODO: implement */
}

static HRESULT WINAPI
NineWinePresentX11_Present( struct NineWinePresentX11 *This,
                            DWORD Flags )
{
    xcb_generic_error_t *err = NULL;

    _MESSAGE("%s(This=%p, Flags=%x)\n", __FUNCTION__, This, Flags);

    if (1/*This->dri2_minor < 3*/) {
        xcb_dri2_copy_region_cookie_t cookie;
        xcb_dri2_copy_region_reply_t *reply;

        err = xcb_request_check(This->c, This->region_cookie);
        if (err) {
            _ERROR("XXFixesCreateRegion failed with error %hhu (major=%hhu, "
                   "minor=%hu)\n", err->error_code,
                   err->major_code, err->minor_code);
            This->region = 0;
            return D3DERR_DRIVERINTERNALERROR;
        }

        cookie = xcb_dri2_copy_region(This->c,
                                      This->current_window.drawable,
                                      This->region,
                                      XCB_DRI2_ATTACHMENT_BUFFER_FRONT_LEFT,
                                      XCB_DRI2_ATTACHMENT_BUFFER_BACK_LEFT);
        if (!(Flags & D3DPRESENT_DONOTWAIT)) {
            reply = xcb_dri2_copy_region_reply(This->c, cookie, &err);
            if (err) {
                _ERROR("DRI2CopyRegion failed with error %hhu (major=%hhu, "
                       "minor=%hu)\n", err->error_code,
                       err->major_code, err->minor_code);
                return D3DERR_DRIVERINTERNALERROR;
            }
            free(reply);
        }
    }

    return D3D_OK;
}

static HRESULT WINAPI
NineWinePresentX11_GetRasterStatus( struct NineWinePresentX11 *This,
                                    D3DRASTER_STATUS *pRasterStatus )
{
    STUB(D3DERR_INVALIDCALL);
}

static HRESULT WINAPI
NineWinePresentX11_GetDisplayMode( struct NineWinePresentX11 *This,
                                   D3DDISPLAYMODEEX *pMode )
{
    STUB(D3DERR_INVALIDCALL); /* TODO: implement */
}

static HRESULT WINAPI
NineWinePresentX11_GetPresentStats( struct NineWinePresentX11 *This,
                                    D3DPRESENTSTATS *pStats )
{
    STUB(D3DERR_INVALIDCALL); /* TODO: implement */
}

static HRESULT WINAPI
NineWinePresentX11_GetCursorPos( struct NineWinePresentX11 *This,
                              POINT *pPoint )
{
    BOOL ok;
    if (!pPoint)
        return D3DERR_INVALIDCALL;
    ok = GetCursorPos(pPoint);
    ok = ok && ScreenToClient(This->current_window.real, pPoint);
    return ok ? S_OK : D3DERR_DRIVERINTERNALERROR;
}

static ID3DPresentVtbl NineWinePresentX11_vtable = {
    (void *)NineWinePresentX11_QueryInterface,
    (void *)NineWinePresentX11_AddRef,
    (void *)NineWinePresentX11_Release,
    (void *)NineWinePresentX11_GetPresentParameters,
    (void *)NineWinePresentX11_GetBuffer,
    (void *)NineWinePresentX11_GetFrontBuffer,
    (void *)NineWinePresentX11_Present,
    (void *)NineWinePresentX11_GetRasterStatus,
    (void *)NineWinePresentX11_GetDisplayMode,
    (void *)NineWinePresentX11_GetPresentStats,
    (void *)NineWinePresentX11_GetCursorPos
};

static HRESULT
NineWinePresentX11_new( xcb_connection_t *c,
                        D3DPRESENT_PARAMETERS *params,
                        HWND focus_wnd,
                        unsigned dri2_minor,
                        struct NineWinePresentX11 **out )
{
    struct NineWinePresentX11 *This;
    RECT rect;

    _MESSAGE("%s(params=%p)\n", __FUNCTION__, params);

    if (params->hDeviceWindow) { focus_wnd = params->hDeviceWindow; }
    if (!focus_wnd) {
        _ERROR("No HWND specified for presentation backend.\n");
        return D3DERR_INVALIDCALL;
    }

    This = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                     sizeof(struct NineWinePresentX11));
    if (!This) { OOM(); }

    This->vtable = &NineWinePresentX11_vtable;
    This->refs = 1;
    This->c = c;
    This->dri2_minor = dri2_minor;

    /* sanitize presentation parameters */
    if (params->SwapEffect == D3DSWAPEFFECT_COPY &&
        params->BackBufferCount > 1) {
        _WARNING("present: BackBufferCount > 1 when SwapEffect == "
                 "D3DSWAPEFFECT_COPY.\n");
        params->BackBufferCount = 1;
    }

    /* XXX 30 for Ex */
    if (params->BackBufferCount > 3) {
        _WARNING("present: BackBufferCount > 3.\n");
        params->BackBufferCount = 3;
    }

    if (params->BackBufferCount == 0) {
        params->BackBufferCount = 1;
    }

    if (params->BackBufferFormat == D3DFMT_UNKNOWN) {
        params->BackBufferFormat = D3DFMT_A8R8G8B8;
    }

    This->current_window.real = NULL;
    This->current_window.ancestor = NULL;
    This->current_window.drawable = 0;

    This->device_window.real = focus_wnd;
    This->device_window.ancestor = GetAncestor(focus_wnd, GA_ROOT);
    This->device_window.drawable = get_drawable(This->device_window.ancestor);

    if (!GetClientRect(focus_wnd, &rect)) {
        _WARNING("GetClientRect failed.\n");
        rect.right = 640;
        rect.bottom = 480;
    }

    if (params->BackBufferWidth == 0) {
        params->BackBufferWidth = rect.right;
    }
    if (params->BackBufferHeight == 0) {
        params->BackBufferHeight = rect.bottom;
    }
    This->params = *params;

    if (!create_drawable(This, This->device_window.drawable)) {
        _ERROR("Unable to create DRI2 drawable.\n");
        HeapFree(GetProcessHeap(), 0, This);
        return D3DERR_DRIVERINTERNALERROR;
    }
    *out = This;

    return D3D_OK;
}

struct NineWinePresentFactoryX11
{
    /* COM vtable */
    void *vtable;
    /* IUnknown reference count */
    UINT refs;

    struct NineWinePresentX11 **present_backends;
    unsigned npresent_backends;

    /* X11 info */
    xcb_connection_t *c;
    unsigned dri2_minor;
};

static HRESULT WINAPI
NineWinePresentFactoryX11_QueryInterface( struct NineWinePresentFactoryX11 *This,
                                          REFIID riid,
                                          void **ppvObject )
{
    _MESSAGE("%s\n", __FUNCTION__);
    if (!ppvObject) { return E_POINTER; }
    if (GUID_equal(&IID_ID3DPresentFactory, riid) ||
        GUID_equal(&IID_IUnknown, riid)) {
        *ppvObject = This;
        return S_OK;
    }
    _MESSAGE("E_NOINTERFACE\n");
    *ppvObject = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI
NineWinePresentFactoryX11_AddRef( struct NineWinePresentFactoryX11 *This )
{
    _MESSAGE("%s(%p): %li+=1\n", __FUNCTION__, This, This->refs);
    return ++This->refs;
}

static ULONG WINAPI
NineWinePresentFactoryX11_Release( struct NineWinePresentFactoryX11 *This )
{
    _MESSAGE("%s(%p): %li-=1\n", __FUNCTION__, This, This->refs);
    if (--This->refs == 0) {
        unsigned i;
        if (This->present_backends) {
            for (i = 0; i < This->npresent_backends; ++i) {
                NineWinePresentX11_Release(This->present_backends[i]);
            }
            HeapFree(GetProcessHeap(), 0, This->present_backends);
        }
        HeapFree(GetProcessHeap(), 0, This);
        return 0;
    }
    return This->refs;
}

static UINT WINAPI
NineWinePresentFactoryX11_GetMultiheadCount( struct NineWinePresentFactoryX11 *This )
{
    _MESSAGE("%s\n", __FUNCTION__);
    return 1;
}

static HRESULT WINAPI
NineWinePresentFactoryX11_GetPresent( struct NineWinePresentFactoryX11 *This,
                                   UINT Index,
                                   ID3DPresent **ppPresent )
{
    _MESSAGE("%s(This=%p Index=%u ppPresent=%p)\n", __FUNCTION__, This, Index, ppPresent);

    if (Index >= NineWinePresentFactoryX11_GetMultiheadCount(This)) {
        _ERROR("Index >= MultiHeadCount\n");
        return D3DERR_INVALIDCALL;
    }
    NineWinePresentX11_AddRef(This->present_backends[Index]);
    *ppPresent = (ID3DPresent *)This->present_backends[Index];

    _MESSAGE("%s: *ppPresent = %p\n", __FUNCTION__, ppPresent ? *ppPresent : NULL);
    return D3D_OK;
}

static HRESULT WINAPI
NineWinePresentFactoryX11_CreateAdditionalPresent( struct NineWinePresentFactoryX11 *This,
                                                   D3DPRESENT_PARAMETERS *pPresentationParameters,
                                                   ID3DPresent **ppPresent )
{
    STUB(D3DERR_INVALIDCALL);
}

static ID3DPresentFactoryVtbl NineWinePresentFactoryX11_vtable = {
    (void *)NineWinePresentFactoryX11_QueryInterface,
    (void *)NineWinePresentFactoryX11_AddRef,
    (void *)NineWinePresentFactoryX11_Release,
    (void *)NineWinePresentFactoryX11_GetMultiheadCount,
    (void *)NineWinePresentFactoryX11_GetPresent,
    (void *)NineWinePresentFactoryX11_CreateAdditionalPresent
};

HRESULT
NineWinePresentFactoryX11_new( xcb_connection_t *c,
                               HWND focus_wnd,
                               D3DPRESENT_PARAMETERS *params,
                               unsigned nparams,
                               unsigned dri2_major,
                               unsigned dri2_minor,
                               ID3DPresentFactory **out )
{
    struct NineWinePresentFactoryX11 *This =
        HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                  sizeof(struct NineWinePresentFactoryX11));
    HRESULT hr;
    unsigned i;

    _MESSAGE("%s\n", __FUNCTION__);

    if (!This) { OOM(); }

    This->vtable = &NineWinePresentFactoryX11_vtable;
    This->refs = 1;
    This->c = c;
    This->dri2_minor = dri2_minor;
    This->npresent_backends = nparams;
    This->present_backends = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                                       This->npresent_backends *
                                       sizeof(struct NineWinePresentX11 *));
    if (!This->present_backends) {
        NineWinePresentFactoryX11_Release(This);
        OOM();
    }

    for (i = 0; i < This->npresent_backends; ++i) {
        hr = NineWinePresentX11_new(c, &params[i], focus_wnd,
                                    dri2_minor, &This->present_backends[i]);
        if (FAILED(hr)) {
            _ERROR("NineWinePresentX11_new failed.\n");
            NineWinePresentFactoryX11_Release(This);
            return hr;
        }
    }

    *out = (ID3DPresentFactory *)This;

    _MESSAGE("*out = %p\n", out ? *out : NULL);
    return D3D_OK;
}
