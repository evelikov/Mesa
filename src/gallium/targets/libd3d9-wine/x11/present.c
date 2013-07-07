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
#include "adjust.h"

#undef _WIN32
#include <X11/Xlib.h>
#include <X11/extensions/Xfixes.h>
#include "dri2.h"
#define _WIN32

#include <d3dadapter/drm.h>

struct NineWinePresentX11
{
    /* COM vtable */
    void *vtable;
    /* IUnknown reference count */
    UINT refs;

    Display *dpy;

    XserverRegion region;
    RGNDATA *rgndata;

    struct {
        Drawable drawable;
        HWND ancestor;
        HWND real;
    } device_window;

    struct {
        Drawable drawable;
        HWND ancestor;
        HWND real;
    } current_window;

    unsigned dri2_minor;

    D3DPRESENT_PARAMETERS params;

    WCHAR devname[32];
    HCURSOR hCursor;
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

Drawable
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

static Drawable
get_drawable( HWND hwnd )
{
    Drawable drawable;
    HDC hdc;

    hdc = GetDC(hwnd);
    drawable = X11DRV_ExtEscape_GET_DRAWABLE(hdc);
    ReleaseDC(hwnd, hdc);

    return drawable;
}

static boolean
create_drawable( struct NineWinePresentX11 *This,
                 Drawable wnd )
{
    DRI2CreateDrawable(This->dpy, wnd);
    _MESSAGE("Created DRI2 drawable for drawable %u.\n", wnd);
    return TRUE;
}

static void
destroy_drawable( struct NineWinePresentX11 *This,
                  Drawable wnd )
{
    DRI2DestroyDrawable(This->dpy, wnd);
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
            XFixesDestroyRegion(This->dpy, This->region);
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
                              const RECT *pDestRect,
                              RECT *pRect,
                              RGNDATA **ppRegion )
{
    static const unsigned attachments[] = { DRI2BufferBackLeft };

    DRI2Buffer *buffers;
    Drawable drawable;
    XRectangle xrect;
    D3DDRM_BUFFER *drmbuf = pBuffer;
    HWND ancestor;
    RECT dest;
    unsigned width, height, n;

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
    if (!get_winex11_hwnd_offset(This->current_window.real,
                                 This->current_window.ancestor, pRect)) {
        return D3DERR_DRIVERINTERNALERROR;
    }
    _MESSAGE("pRect=(%u..%u)x(%u..%u)\n",
             pRect->left, pRect->right, pRect->top, pRect->bottom);

    /* XXX base this on events instead of calling every single frame */
    if ((n = DRI2GetBuffers(This->dpy, This->current_window.drawable,
                            attachments, 1, &width, &height, &buffers)) < 1) {
        _ERROR("DRI2GetBuffers failed (drawable = %u, n = %u)\n",
               This->current_window.drawable, n);
        return D3DERR_DRIVERINTERNALERROR;
    }

    drmbuf->iName = buffers[0].name;
    drmbuf->dwWidth = width;
    drmbuf->dwHeight = height;
    drmbuf->dwStride = buffers[0].pitch;
    drmbuf->dwCPP = buffers[0].cpp;
    free(buffers);

    if (This->region) {
        XFixesDestroyRegion(This->dpy, This->region);
        This->region = 0;
    }

    /* set a destrect covering the entire window if none given */
    if (!pDestRect) {
        dest.left = 0;
        dest.top = 0;
        dest.right = pRect->right-pRect->left;
        dest.bottom = pRect->top-pRect->bottom;
        pDestRect = &dest;
    }
    _MESSAGE("pDestRect=(%u..%u)x(%u..%u)\n", pDestRect->left,
             pDestRect->right, pDestRect->top, pDestRect->bottom);

    xrect.x = pRect->left+pDestRect->left;
    xrect.y = pRect->top+pDestRect->top;
    xrect.width = pDestRect->right-pDestRect->left;
    xrect.height = pDestRect->bottom-pDestRect->top;
    _MESSAGE("XFixes rect (x=%u, y=%u, w=%u, h=%u)\n",
             xrect.x, xrect.y, xrect.width, xrect.height);

    This->region = XFixesCreateRegion(This->dpy, &xrect, 1);

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
    _MESSAGE("%s(This=%p, Flags=%x)\n", __FUNCTION__, This, Flags);

    if (1/*This->dri2_minor < 3*/) {
        if (!DRI2CopyRegion(This->dpy, This->current_window.drawable,
                            This->region, DRI2BufferFrontLeft,
                            DRI2BufferBackLeft)) {
            _ERROR("DRI2CopyRegion failed (drawable = %u, region = %u)\n",
                   This->current_window.drawable, This->region);
            return D3DERR_DRIVERINTERNALERROR;
        }
        /* XXX if (!(Flags & D3DPRESENT_DONOTWAIT)) { */
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
                                   D3DDISPLAYMODEEX *pMode,
                                   D3DDISPLAYROTATION *pRotation )
{
    DEVMODEW dm;

    ZeroMemory(&dm, sizeof(dm));
    dm.dmSize = sizeof(dm);

    EnumDisplaySettingsExW(This->devname, ENUM_CURRENT_SETTINGS, &dm, 0);
    pMode->Width = dm.dmPelsWidth;
    pMode->Height = dm.dmPelsHeight;
    pMode->RefreshRate = dm.dmDisplayFrequency;
    pMode->ScanLineOrdering = (dm.dmDisplayFlags & DM_INTERLACED) ?
                                  D3DSCANLINEORDERING_INTERLACED :
                                  D3DSCANLINEORDERING_PROGRESSIVE;

    /* XXX This is called "guessing" */
    switch (dm.dmBitsPerPel) {
        case 32: pMode->Format = D3DFMT_X8R8G8B8; break;
        case 24: pMode->Format = D3DFMT_R8G8B8; break;
        case 16: pMode->Format = D3DFMT_R5G6B5; break;
        default:
            _WARNING("Unknown display format with %u bpp.\n", dm.dmBitsPerPel);
            pMode->Format = D3DFMT_UNKNOWN;
    }

    switch (dm.dmDisplayOrientation) {
        case DMDO_DEFAULT: *pRotation = D3DDISPLAYROTATION_IDENTITY; break;
        case DMDO_90:      *pRotation = D3DDISPLAYROTATION_90; break;
        case DMDO_180:     *pRotation = D3DDISPLAYROTATION_180; break;
        case DMDO_270:     *pRotation = D3DDISPLAYROTATION_270; break;
        default:
            _WARNING("Unknown display rotation %u.\n", dm.dmDisplayOrientation);
            *pRotation = D3DDISPLAYROTATION_IDENTITY;
    }

    return D3D_OK;
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

static HRESULT WINAPI
NineWinePresentX11_SetCursorPos( struct NineWinePresentX11 *This,
                                 POINT *pPoint )
{
    if (!pPoint)
        return D3DERR_INVALIDCALL;
    return SetCursorPos(pPoint->x, pPoint->y);
}

static HRESULT WINAPI
NineWinePresentX11_SetCursor( struct NineWinePresentX11 *This,
                              void *pBitmap,
                              POINT *pHotspot,
                              BOOL bShow )
{
   if (pBitmap) {
      ICONINFO info;
      HCURSOR cursor;

      DWORD mask[32];
      memset(mask, ~0, sizeof(mask));

      if (!pHotspot)
         return D3DERR_INVALIDCALL;
      info.fIcon = FALSE;
      info.xHotspot = pHotspot->x;
      info.yHotspot = pHotspot->y;
      info.hbmMask = CreateBitmap(32, 32, 1, 1, mask);
      info.hbmColor = CreateBitmap(32, 32, 1, 32, pBitmap);

      cursor = CreateIconIndirect(&info);
      if (info.hbmMask) DeleteObject(info.hbmMask);
      if (info.hbmColor) DeleteObject(info.hbmColor);
      if (cursor)
         DestroyCursor(This->hCursor);
      This->hCursor = cursor;
   }
   SetCursor(bShow ? This->hCursor : NULL);

   return D3D_OK;
}

static HRESULT WINAPI
NineWinePresentX11_SetGammaRamp( struct NineWinePresentX11 *This,
                                 const D3DGAMMARAMP *pRamp,
                                 HWND hWndOverride )
{
   HWND hWnd = hWndOverride ? hWndOverride : This->current_window.real;
   HDC hdc;
   BOOL ok;
   if (!pRamp)
      return D3DERR_INVALIDCALL;
   hdc = GetDC(hWnd);
   ok = SetDeviceGammaRamp(hdc, (void *)pRamp);
   ReleaseDC(hWnd, hdc);
   return ok ? D3D_OK : D3DERR_DRIVERINTERNALERROR;
}

static HRESULT WINAPI
NineWinePresentX11_GetWindowRect( struct NineWinePresentX11 *This,
                                  HWND hWnd,
                                  LPRECT pRect )
{
   if (!hWnd)
      hWnd = This->current_window.real;
   return GetClientRect(hWnd, pRect) ? D3D_OK : D3DERR_INVALIDCALL;
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
    (void *)NineWinePresentX11_GetCursorPos,
    (void *)NineWinePresentX11_SetCursorPos,
    (void *)NineWinePresentX11_SetCursor,
    (void *)NineWinePresentX11_SetGammaRamp,
    (void *)NineWinePresentX11_GetWindowRect
};

static HRESULT
NineWinePresentX11_new( Display *dpy,
                        const WCHAR *devname,
                        unsigned ordinal,
                        D3DPRESENT_PARAMETERS *params,
                        HWND focus_wnd,
                        unsigned dri2_minor,
                        struct NineWinePresentX11 **out )
{
    struct NineWinePresentX11 *This;
    RECT rect;
    int i;

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
    This->dpy = dpy;
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
    /* XXX strcpyW(This->devname, devname); */
    for (i = 0; i < sizeof(This->devname)-1 && devname[i]; ++i) {
        This->devname[i] = devname[i];
    }
    This->devname[i] = '\0';

    if (!create_drawable(This, This->device_window.drawable)) {
        _ERROR("Unable to create DRI2 drawable.\n");
        HeapFree(GetProcessHeap(), 0, This);
        return D3DERR_DRIVERINTERNALERROR;
    }
    *out = This;

    return D3D_OK;
}

struct NineWinePresentGroupX11
{
    /* COM vtable */
    void *vtable;
    /* IUnknown reference count */
    UINT refs;

    struct NineWinePresentX11 **present_backends;
    unsigned npresent_backends;

    /* X11 info */
    Display *dpy;
    unsigned dri2_minor;
};

static HRESULT WINAPI
NineWinePresentGroupX11_QueryInterface( struct NineWinePresentGroupX11 *This,
                                        REFIID riid,
                                        void **ppvObject )
{
    _MESSAGE("%s\n", __FUNCTION__);
    if (!ppvObject) { return E_POINTER; }
    if (GUID_equal(&IID_ID3DPresentGroup, riid) ||
        GUID_equal(&IID_IUnknown, riid)) {
        *ppvObject = This;
        return S_OK;
    }
    _MESSAGE("E_NOINTERFACE\n");
    *ppvObject = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI
NineWinePresentGroupX11_AddRef( struct NineWinePresentGroupX11 *This )
{
    _MESSAGE("%s(%p): %li+=1\n", __FUNCTION__, This, This->refs);
    return ++This->refs;
}

static ULONG WINAPI
NineWinePresentGroupX11_Release( struct NineWinePresentGroupX11 *This )
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
NineWinePresentGroupX11_GetMultiheadCount( struct NineWinePresentGroupX11 *This )
{
    _MESSAGE("%s\n", __FUNCTION__);
    return 1;
}

static HRESULT WINAPI
NineWinePresentGroupX11_GetPresent( struct NineWinePresentGroupX11 *This,
                                    UINT Index,
                                    ID3DPresent **ppPresent )
{
    _MESSAGE("%s(This=%p Index=%u ppPresent=%p)\n", __FUNCTION__, This, Index, ppPresent);

    if (Index >= NineWinePresentGroupX11_GetMultiheadCount(This)) {
        _ERROR("Index >= MultiHeadCount\n");
        return D3DERR_INVALIDCALL;
    }
    NineWinePresentX11_AddRef(This->present_backends[Index]);
    *ppPresent = (ID3DPresent *)This->present_backends[Index];

    _MESSAGE("%s: *ppPresent = %p\n", __FUNCTION__, ppPresent ? *ppPresent : NULL);
    return D3D_OK;
}

static HRESULT WINAPI
NineWinePresentGroupX11_CreateAdditionalPresent( struct NineWinePresentGroupX11 *This,
                                                 D3DPRESENT_PARAMETERS *pPresentationParameters,
                                                 ID3DPresent **ppPresent )
{
    STUB(D3DERR_INVALIDCALL);
}

static ID3DPresentGroupVtbl NineWinePresentGroupX11_vtable = {
    (void *)NineWinePresentGroupX11_QueryInterface,
    (void *)NineWinePresentGroupX11_AddRef,
    (void *)NineWinePresentGroupX11_Release,
    (void *)NineWinePresentGroupX11_GetMultiheadCount,
    (void *)NineWinePresentGroupX11_GetPresent,
    (void *)NineWinePresentGroupX11_CreateAdditionalPresent
};

HRESULT
NineWinePresentGroupX11_new( Display *dpy,
                             const WCHAR *devname,
                             UINT adapter,
                             HWND focus_wnd,
                             D3DPRESENT_PARAMETERS *params,
                             unsigned nparams,
                             unsigned dri2_major,
                             unsigned dri2_minor,
                             ID3DPresentGroup **out )
{
    struct NineWinePresentGroupX11 *This =
        HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                  sizeof(struct NineWinePresentGroupX11));
    HRESULT hr;
    unsigned i;

    _MESSAGE("%s\n", __FUNCTION__);

    if (!This) { OOM(); }

    This->vtable = &NineWinePresentGroupX11_vtable;
    This->refs = 1;
    This->dpy = dpy;
    This->dri2_minor = dri2_minor;
    This->npresent_backends = nparams;
    This->present_backends = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                                       This->npresent_backends *
                                       sizeof(struct NineWinePresentX11 *));
    if (!This->present_backends) {
        NineWinePresentGroupX11_Release(This);
        OOM();
    }

    if (nparams != 1) { adapter = 0; }
    for (i = 0; i < This->npresent_backends; ++i) {
        hr = NineWinePresentX11_new(dpy, devname, i+adapter, &params[i],
                                    focus_wnd, dri2_minor,
                                    &This->present_backends[i]);
        if (FAILED(hr)) {
            _ERROR("NineWinePresentX11_new failed.\n");
            NineWinePresentGroupX11_Release(This);
            return hr;
        }
    }

    *out = (ID3DPresentGroup *)This;

    _MESSAGE("*out = %p\n", out ? *out : NULL);
    return D3D_OK;
}
