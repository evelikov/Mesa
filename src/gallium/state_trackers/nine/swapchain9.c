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

#include "swapchain9.h"
#include "surface9.h"
#include "device9.h"

#include "nine_helpers.h"
#include "nine_pipe.h"

#include "util/u_inlines.h"
#include "util/u_blit.h"
#include "util/u_surface.h"

#define DBG_CHANNEL DBG_SWAPCHAIN

HRESULT
NineSwapChain9_ctor( struct NineSwapChain9 *This,
                     struct NineUnknownParams *pParams,
                     struct NineDevice9 *pDevice,
                     ID3DPresent *pPresent,
                     PPRESENT_TO_RESOURCE pPTR,
                     HWND hFocusWindow )
{
    unsigned i;
    struct pipe_resource *resource, tmplt;
    D3DSURFACE_DESC desc;

    HRESULT hr = NineUnknown_ctor(&This->base, pParams);
    if (FAILED(hr)) { return hr; }

    This->screen = NineDevice9_GetScreen(pDevice);
    This->pipe = NineDevice9_GetPipe(pDevice);
    This->cso = NineDevice9_GetCSO(pDevice);
    This->device = pDevice;
    This->ptrfunc = pPTR;
    This->present = pPresent;
    hr = ID3DPresent_GetPresentParameters(This->present, &This->params);
    if (FAILED(hr)) { return hr; }

    This->blitter = util_create_blit(This->pipe, This->cso);
    if (!This->blitter) { return E_OUTOFMEMORY; } /* guesswork */

    This->buffers = CALLOC(This->params.BackBufferCount+1,
                           sizeof(struct NineSurface9 *));
    if (!This->buffers) { return E_OUTOFMEMORY; }

    desc.Format = This->params.BackBufferFormat;
    desc.Pool = D3DPOOL_DEFAULT;
    desc.MultiSampleType = D3DMULTISAMPLE_NONE;
    desc.MultiSampleQuality = 0.0f;
    desc.Width = This->params.BackBufferWidth;
    desc.Height = This->params.BackBufferHeight;
    desc.Usage = D3DUSAGE_RENDERTARGET;

    tmplt.screen = This->screen;
    tmplt.target = PIPE_TEXTURE_2D;
    tmplt.format = nine_pipe_format(desc.Format);
    tmplt.width0 = desc.Width;
    tmplt.height0 = desc.Height;
    tmplt.depth0 = 1;
    tmplt.array_size = 1;
    tmplt.last_level = 0;
    tmplt.nr_samples = 0;
    tmplt.usage = PIPE_USAGE_DEFAULT; /* is there a better usage? */
    tmplt.bind = PIPE_BIND_RENDER_TARGET | PIPE_BIND_TRANSFER_WRITE |
                 PIPE_BIND_TRANSFER_READ;
    tmplt.flags = 0;

    for (i = 0; i < This->params.BackBufferCount; i++) {
        resource = This->screen->resource_create(This->screen, &tmplt);
        if (!resource) {
            DBG("screen::resource_create failed\n"
                " format = %u\n"
                " width0 = %u\n"
                " height0 = %u\n"
                " usage = %u\n"
                " bind = %u\n",
                tmplt.format, tmplt.width0,
                tmplt.height0, tmplt.usage, tmplt.bind);
            return D3DERR_OUTOFVIDEOMEMORY;
        }

        hr = NineSurface9_new(pDevice, NineUnknown(This), resource,
                              0, 0, &desc, &This->buffers[i]);
        pipe_resource_reference(&resource, NULL);
        if (FAILED(hr)) { return hr; }
    }

    return D3D_OK;
}

void
NineSwapChain9_dtor( struct NineSwapChain9 *This )
{
    unsigned i;

    if (This->buffers) {
        for (i = 0; i <= This->params.BackBufferCount; i++) {
            if (This->buffers[i]) {
                NineUnknown_Release(NineUnknown(This->buffers[i]));
            }
        }
        FREE(This->buffers);
    }
    if (This->present) {
        ID3DPresent_Release(This->present);
    }
    if (This->blitter) {
        util_destroy_blit(This->blitter);
    }

    NineUnknown_dtor(&This->base);
}

static INLINE HRESULT
present( struct NineSwapChain9 *This,
         const RECT *pSourceRect,
         const RECT *pDestRect,
         HWND hDestWindowOverride,
         const RGNDATA *pDirtyRegion,
         DWORD dwFlags )
{
    struct pipe_resource *resource;
    struct pipe_surface templ, *emusurf;
    HRESULT hr;
    RGNDATA *rgndata;
    RECT rect;

    /* get a real backbuffer handle from the windowing system */
    hr = This->ptrfunc(This->present, hDestWindowOverride, &rect,
                       &rgndata, This->screen, &resource);
    if (FAILED(hr)) {
        return hr;
    } else if (hr == D3DOK_WINDOW_OCCLUDED) {
        /* if we present, nothing will show, so don't present */
        return D3D_OK;
    }

    memset(&templ, 0, sizeof(struct pipe_surface));
    u_surface_default_template(&templ, resource);

    emusurf = This->pipe->create_surface(This->pipe, resource, &templ);
    pipe_resource_reference(&resource, NULL);
    if (!emusurf) {
        DBG("Obtaining pipe_surface from ID3DPresent backend failed.\n");
        return D3DERR_DRIVERINTERNALERROR;
    }

    if (rgndata) {
        /* TODO */
    } else {
        /* blit (and possibly stretch/convert) pixels from This->buffers[0] to
         * emusurf using u_blit. Windows appears to use NEAREST */
        DBG("Blitting %ux%u surface to (%u, %u)-(%u, %u).\n",
            This->buffers[0]->surface->width, This->buffers[0]->surface->height,
            rect.left, rect.top, rect.right, rect.bottom);

        util_blit_pixels(This->blitter, This->buffers[0]->surface->texture,
                         This->buffers[0]->level, 0, 0,
                         This->buffers[0]->surface->width,
                         This->buffers[0]->surface->height, 0,
                         emusurf, rect.left, rect.top, rect.right, rect.bottom,
                         0.0f, PIPE_TEX_MIPFILTER_NEAREST, PIPE_MASK_RGBA, 0);
    }
    This->pipe->flush(This->pipe, NULL, 0);

    /* really present the frame */
    hr = ID3DPresent_Present(This->present, dwFlags);
    pipe_surface_reference(&emusurf, NULL);
    if (FAILED(hr)) { return hr; }

    return D3D_OK;
}

HRESULT WINAPI
NineSwapChain9_Present( struct NineSwapChain9 *This,
                        const RECT *pSourceRect,
                        const RECT *pDestRect,
                        HWND hDestWindowOverride,
                        const RGNDATA *pDirtyRegion,
                        DWORD dwFlags )
{
    int i;
    struct pipe_surface *temp;
    HRESULT hr = present(This, pSourceRect, pDestRect,
                         hDestWindowOverride, pDirtyRegion, dwFlags);

    switch (This->params.SwapEffect) {
        case D3DSWAPEFFECT_DISCARD:
            /* rotate the queue */
            temp = This->buffers[0]->surface;
            for (i = 1; i < This->params.BackBufferCount; i++) {
                This->buffers[i-1]->surface = This->buffers[i]->surface;
            }
            This->buffers[This->params.BackBufferCount-1]->surface = temp;
            break;

        case D3DSWAPEFFECT_FLIP:
            /* XXX not implemented */
            break;

        case D3DSWAPEFFECT_COPY:
            /* do nothing */
            break;

        case D3DSWAPEFFECT_OVERLAY:
            /* XXX not implemented */
            break;

        case D3DSWAPEFFECT_FLIPEX:
            /* XXX not implemented */
            break;
    }
    NineDevice9_UpdateRenderTargets(This->device);

    return hr;
}

HRESULT WINAPI
NineSwapChain9_GetFrontBufferData( struct NineSwapChain9 *This,
                                   IDirect3DSurface9 *pDestSurface )
{
    /* TODO: GetFrontBuffer() and copy the contents */
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineSwapChain9_GetBackBuffer( struct NineSwapChain9 *This,
                              UINT iBackBuffer,
                              D3DBACKBUFFER_TYPE Type,
                              IDirect3DSurface9 **ppBackBuffer )
{
    struct NineSurface9 *surf;

    (void)user_error(Type == D3DBACKBUFFER_TYPE_MONO);
    user_assert(iBackBuffer < This->params.BackBufferCount, D3DERR_INVALIDCALL);
    user_assert(ppBackBuffer != NULL, E_POINTER);

    surf = This->buffers[iBackBuffer];
    NineUnknown_AddRef(NineUnknown(surf));
    *ppBackBuffer = (IDirect3DSurface9 *)surf;
    return D3D_OK;
}

HRESULT WINAPI
NineSwapChain9_GetRasterStatus( struct NineSwapChain9 *This,
                                D3DRASTER_STATUS *pRasterStatus )
{
    user_assert(pRasterStatus != NULL, E_POINTER);
    return ID3DPresent_GetRasterStatus(This->present, pRasterStatus);
}

HRESULT WINAPI
NineSwapChain9_GetDisplayMode( struct NineSwapChain9 *This,
                               D3DDISPLAYMODE *pMode )
{
    D3DDISPLAYMODEEX mode;
    HRESULT hr;

    user_assert(pMode != NULL, E_POINTER);

    hr = ID3DPresent_GetDisplayMode(This->present, &mode);
    if (SUCCEEDED(hr)) {
        pMode->Width = mode.Width;
        pMode->Height = mode.Height;
        pMode->RefreshRate = mode.RefreshRate;
        pMode->Format = mode.Format;
    }
    return hr;
}

HRESULT WINAPI
NineSwapChain9_GetDevice( struct NineSwapChain9 *This,
                          IDirect3DDevice9 **ppDevice )
{
    user_assert(ppDevice != NULL, E_POINTER);
    *ppDevice = (IDirect3DDevice9 *)This->device;
    return D3D_OK;
}

HRESULT WINAPI
NineSwapChain9_GetPresentParameters( struct NineSwapChain9 *This,
                                     D3DPRESENT_PARAMETERS *pPresentationParameters )
{
    user_assert(pPresentationParameters != NULL, E_POINTER);
    *pPresentationParameters = This->params;
    return D3D_OK;
}

IDirect3DSwapChain9Vtbl NineSwapChain9_vtable = {
    (void *)NineUnknown_QueryInterface,
    (void *)NineUnknown_AddRef,
    (void *)NineUnknown_Release,
    (void *)NineSwapChain9_Present,
    (void *)NineSwapChain9_GetFrontBufferData,
    (void *)NineSwapChain9_GetBackBuffer,
    (void *)NineSwapChain9_GetRasterStatus,
    (void *)NineSwapChain9_GetDisplayMode,
    (void *)NineSwapChain9_GetDevice,
    (void *)NineSwapChain9_GetPresentParameters
};

static const GUID *NineSwapChain9_IIDs[] = {
    &IID_IDirect3DSwapChain9,
    &IID_IUnknown,
    NULL
};

HRESULT
NineSwapChain9_new( struct NineDevice9 *pDevice,
                    ID3DPresent *pPresent,
                    PPRESENT_TO_RESOURCE pPTR,
                    HWND hFocusWindow,
                    struct NineSwapChain9 **ppOut )
{
    NINE_NEW(NineSwapChain9, ppOut, /* args */
             pDevice, pPresent, pPTR, hFocusWindow);
}
