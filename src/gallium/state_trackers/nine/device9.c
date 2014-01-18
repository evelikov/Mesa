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

#include "device9.h"
#include "surface9.h"
#include "swapchain9.h"
#include "nine_helpers.h"
#include "nine_pipe.h"

#include "pipe/p_screen.h"
#include "pipe/p_context.h"

#include "cso_cache/cso_context.h"

#define DBG_CHANNEL DBG_DEVICE

HRESULT
NineDevice9_ctor( struct NineDevice9 *This,
                  struct NineUnknownParams *pParams,
                  struct pipe_screen *pScreen,
                  D3DDEVICE_CREATION_PARAMETERS *pCreationParameters,
                  D3DCAPS9 *pCaps,
                  IDirect3D9 *pD3D9,
                  ID3DPresentFactory *pPresentationFactory,
                  PPRESENT_TO_RESOURCE pPTR )
{
    unsigned i;
    HRESULT hr = NineUnknown_ctor(&This->base, pParams);
    if (FAILED(hr)) { return hr; }

    This->screen = pScreen;
    This->caps = *pCaps;
    This->d3d9 = pD3D9;
    This->params = *pCreationParameters;
    This->present = pPresentationFactory;
    IDirect3D9_AddRef(This->d3d9);
    ID3DPresentFactory_AddRef(This->present);

    This->pipe = This->screen->context_create(This->screen, NULL);
    if (!This->pipe) { return E_OUTOFMEMORY; } /* guess */

    This->cso = cso_create_context(This->pipe);
    if (!This->cso) { return E_OUTOFMEMORY; } /* also a guess */

    /* render targets */
    This->rendertargets = CALLOC(This->caps.NumSimultaneousRTs,
                                 sizeof(struct NineSurface9 *));
    if (!This->rendertargets) { return E_OUTOFMEMORY; }

    /* create implicit swapchains */
    This->nswapchains = ID3DPresentFactory_GetMultiheadCount(This->present);
    This->swapchains = CALLOC(This->nswapchains,
                              sizeof(struct NineSwapChain9 *));
    if (!This->swapchains) { return E_OUTOFMEMORY; }
    for (i = 0; i < This->nswapchains; ++i) {
        ID3DPresent *present;

        hr = ID3DPresentFactory_GetPresent(This->present, i, &present);
        if (FAILED(hr)) { return hr; }

        hr = NineSwapChain9_new(This, present, pPTR,
                                This->params.hFocusWindow,
                                &This->swapchains[i]);
        if (FAILED(hr)) { return hr; }

        hr = NineSwapChain9_GetBackBuffer(This->swapchains[i], 0,
                                          D3DBACKBUFFER_TYPE_MONO,
                                          (IDirect3DSurface9 **)
                                          &This->rendertargets[i]);
        if (FAILED(hr)) { return hr; }
    }

    NineDevice9_UpdateRenderTargets(This);
    ID3DPresentFactory_Release(This->present);

    return D3D_OK;
}

void
NineDevice9_dtor( struct NineDevice9 *This )
{
    int i;

    /* rendertargets */
    if (This->rendertargets) {
        for (i = 0; i < This->caps.NumSimultaneousRTs; i++) {
            if (This->rendertargets[i]) {
                NineUnknown_Release(NineUnknown(This->rendertargets[i]));
            }
        }
        FREE(This->rendertargets);
    }
    if (This->swapchains) {
        for (i = 0; i < This->nswapchains; ++i) {
            if (This->swapchains[i]) {
                NineUnknown_Release(NineUnknown(This->swapchains[i]));
            }
        }
        FREE(This->swapchains);
    }

    /* state stuff */
    if (This->pipe) {
        if (This->cso) {
            cso_release_all(This->cso);
            cso_destroy_context(This->cso);
        }
        if (This->pipe->destroy) { This->pipe->destroy(This->pipe); }
    }

    if (This->present) { ID3DPresentFactory_Release(This->present); }
    if (This->d3d9) { IDirect3D9_Release(This->d3d9); }

    NineUnknown_dtor(&This->base);
}

void
NineDevice9_UpdateRenderTargets( struct NineDevice9 *This )
{
    int i;

    This->fbstate.nr_cbufs = 0;
    This->fbstate.height = This->rendertargets[0]->surface->height;
    This->fbstate.width = This->rendertargets[0]->surface->width;
    This->fbstate.zsbuf = NULL; /* TODO */
    /* set all cbufs */
    for (i = 0; i < This->caps.NumSimultaneousRTs; i++) {
        if (This->rendertargets[i]) {
            This->fbstate.cbufs[This->fbstate.nr_cbufs++] =
                NineSurface9_GetSurface(This->rendertargets[i]);
        }
    }
    cso_set_framebuffer(This->cso, &This->fbstate);
}

struct pipe_screen *
NineDevice9_GetScreen( struct NineDevice9 *This )
{
    return This->screen;
}

struct pipe_context *
NineDevice9_GetPipe( struct NineDevice9 *This )
{
    return This->pipe;
}

struct cso_context *
NineDevice9_GetCSO( struct NineDevice9 *This )
{
    return This->cso;
}

const D3DCAPS9 *
NineDevice9_GetCaps( struct NineDevice9 *This )
{
    return &This->caps;
}

HRESULT WINAPI
NineDevice9_TestCooperativeLevel( struct NineDevice9 *This )
{
    STUB(D3DERR_INVALIDCALL);
}

UINT WINAPI
NineDevice9_GetAvailableTextureMem( struct NineDevice9 *This )
{
    STUB(0);
}

HRESULT WINAPI
NineDevice9_EvictManagedResources( struct NineDevice9 *This )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_GetDirect3D( struct NineDevice9 *This,
                         IDirect3D9 **ppD3D9 )
{
    user_assert(ppD3D9 != NULL, E_POINTER);
    *ppD3D9 = This->d3d9;
    return D3D_OK;
}

HRESULT WINAPI
NineDevice9_GetDeviceCaps( struct NineDevice9 *This,
                           D3DCAPS9 *pCaps )
{
    user_assert(pCaps != NULL, D3DERR_INVALIDCALL);
    *pCaps = This->caps;
    return D3D_OK;
}

HRESULT WINAPI
NineDevice9_GetDisplayMode( struct NineDevice9 *This,
                            UINT iSwapChain,
                            D3DDISPLAYMODE *pMode )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_GetCreationParameters( struct NineDevice9 *This,
                                   D3DDEVICE_CREATION_PARAMETERS *pParameters )
{
    user_assert(pParameters != NULL, D3DERR_INVALIDCALL);
    *pParameters = This->params;
    return D3D_OK;
}

HRESULT WINAPI
NineDevice9_SetCursorProperties( struct NineDevice9 *This,
                                 UINT XHotSpot,
                                 UINT YHotSpot,
                                 IDirect3DSurface9 *pCursorBitmap )
{
    STUB(D3DERR_INVALIDCALL);
}

void WINAPI
NineDevice9_SetCursorPosition( struct NineDevice9 *This,
                               int X,
                               int Y,
                               DWORD Flags )
{
    STUB();
}

BOOL WINAPI
NineDevice9_ShowCursor( struct NineDevice9 *This,
                        BOOL bShow )
{
    STUB(0);
}

HRESULT WINAPI
NineDevice9_CreateAdditionalSwapChain( struct NineDevice9 *This,
                                       D3DPRESENT_PARAMETERS *pPresentationParameters,
                                       IDirect3DSwapChain9 **pSwapChain )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_GetSwapChain( struct NineDevice9 *This,
                          UINT iSwapChain,
                          IDirect3DSwapChain9 **pSwapChain )
{
    user_assert(pSwapChain != NULL, D3DERR_INVALIDCALL);
    user_assert(iSwapChain < This->nswapchains, D3DERR_INVALIDCALL);

    *pSwapChain = (IDirect3DSwapChain9 *)This->swapchains[iSwapChain];

    return D3D_OK;
}

UINT WINAPI
NineDevice9_GetNumberOfSwapChains( struct NineDevice9 *This )
{
    return This->nswapchains;
}

HRESULT WINAPI
NineDevice9_Reset( struct NineDevice9 *This,
                   D3DPRESENT_PARAMETERS *pPresentationParameters )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_Present( struct NineDevice9 *This,
                     const RECT *pSourceRect,
                     const RECT *pDestRect,
                     HWND hDestWindowOverride,
                     const RGNDATA *pDirtyRegion )
{
    unsigned i;
    HRESULT hr;

    /* XXX is this right? */
    for (i = 0; i < This->nswapchains; ++i) {
        hr = NineSwapChain9_Present(This->swapchains[i], pSourceRect, pDestRect,
                                    hDestWindowOverride, pDirtyRegion, 0);
        if (FAILED(hr)) { return hr; }
    }

    return D3D_OK;
}

HRESULT WINAPI
NineDevice9_GetBackBuffer( struct NineDevice9 *This,
                           UINT iSwapChain,
                           UINT iBackBuffer,
                           D3DBACKBUFFER_TYPE Type,
                           IDirect3DSurface9 **ppBackBuffer )
{
    user_assert(ppBackBuffer != NULL, D3DERR_INVALIDCALL);
    user_assert(iSwapChain < This->nswapchains, D3DERR_INVALIDCALL);

    return NineSwapChain9_GetBackBuffer(This->swapchains[iSwapChain],
                                        iBackBuffer, Type, ppBackBuffer);
}

HRESULT WINAPI
NineDevice9_GetRasterStatus( struct NineDevice9 *This,
                             UINT iSwapChain,
                             D3DRASTER_STATUS *pRasterStatus )
{
    user_assert(pRasterStatus != NULL, D3DERR_INVALIDCALL);
    user_assert(iSwapChain < This->nswapchains, D3DERR_INVALIDCALL);

    return NineSwapChain9_GetRasterStatus(This->swapchains[iSwapChain],
                                          pRasterStatus);
}

HRESULT WINAPI
NineDevice9_SetDialogBoxMode( struct NineDevice9 *This,
                              BOOL bEnableDialogs )
{
    STUB(D3DERR_INVALIDCALL);
}

void WINAPI
NineDevice9_SetGammaRamp( struct NineDevice9 *This,
                          UINT iSwapChain,
                          DWORD Flags,
                          const D3DGAMMARAMP *pRamp )
{
    STUB();
}

void WINAPI
NineDevice9_GetGammaRamp( struct NineDevice9 *This,
                          UINT iSwapChain,
                          D3DGAMMARAMP *pRamp )
{
    STUB();
}

HRESULT WINAPI
NineDevice9_CreateTexture( struct NineDevice9 *This,
                           UINT Width,
                           UINT Height,
                           UINT Levels,
                           DWORD Usage,
                           D3DFORMAT Format,
                           D3DPOOL Pool,
                           IDirect3DTexture9 **ppTexture,
                           HANDLE *pSharedHandle )
{
    Usage &= D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_DEPTHSTENCIL | D3DUSAGE_DMAP |
             D3DUSAGE_DYNAMIC | D3DUSAGE_NONSECURE | D3DUSAGE_RENDERTARGET |
             D3DUSAGE_SOFTWAREPROCESSING | D3DUSAGE_TEXTAPI;
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_CreateVolumeTexture( struct NineDevice9 *This,
                                 UINT Width,
                                 UINT Height,
                                 UINT Depth,
                                 UINT Levels,
                                 DWORD Usage,
                                 D3DFORMAT Format,
                                 D3DPOOL Pool,
                                 IDirect3DVolumeTexture9 **ppVolumeTexture,
                                 HANDLE *pSharedHandle )
{
    Usage &= D3DUSAGE_DYNAMIC | D3DUSAGE_NONSECURE |
             D3DUSAGE_SOFTWAREPROCESSING;
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_CreateCubeTexture( struct NineDevice9 *This,
                               UINT EdgeLength,
                               UINT Levels,
                               DWORD Usage,
                               D3DFORMAT Format,
                               D3DPOOL Pool,
                               IDirect3DCubeTexture9 **ppCubeTexture,
                               HANDLE *pSharedHandle )
{
    Usage &= D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_DEPTHSTENCIL | D3DUSAGE_DYNAMIC |
             D3DUSAGE_NONSECURE | D3DUSAGE_RENDERTARGET |
             D3DUSAGE_SOFTWAREPROCESSING;
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_CreateVertexBuffer( struct NineDevice9 *This,
                                UINT Length,
                                DWORD Usage,
                                DWORD FVF,
                                D3DPOOL Pool,
                                IDirect3DVertexBuffer9 **ppVertexBuffer,
                                HANDLE *pSharedHandle )
{
    Usage &= D3DUSAGE_DONOTCLIP | D3DUSAGE_DYNAMIC | D3DUSAGE_NONSECURE |
             D3DUSAGE_NPATCHES | D3DUSAGE_POINTS | D3DUSAGE_RTPATCHES |
             D3DUSAGE_SOFTWAREPROCESSING | D3DUSAGE_TEXTAPI |
             D3DUSAGE_WRITEONLY;
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_CreateIndexBuffer( struct NineDevice9 *This,
                               UINT Length,
                               DWORD Usage,
                               D3DFORMAT Format,
                               D3DPOOL Pool,
                               IDirect3DIndexBuffer9 **ppIndexBuffer,
                               HANDLE *pSharedHandle )
{
    Usage &= D3DUSAGE_DONOTCLIP | D3DUSAGE_DYNAMIC | D3DUSAGE_NONSECURE |
             D3DUSAGE_NPATCHES | D3DUSAGE_POINTS | D3DUSAGE_RTPATCHES |
             D3DUSAGE_SOFTWAREPROCESSING | D3DUSAGE_WRITEONLY;
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_CreateRenderTarget( struct NineDevice9 *This,
                                UINT Width,
                                UINT Height,
                                D3DFORMAT Format,
                                D3DMULTISAMPLE_TYPE MultiSample,
                                DWORD MultisampleQuality,
                                BOOL Lockable,
                                IDirect3DSurface9 **ppSurface,
                                HANDLE *pSharedHandle )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_CreateDepthStencilSurface( struct NineDevice9 *This,
                                       UINT Width,
                                       UINT Height,
                                       D3DFORMAT Format,
                                       D3DMULTISAMPLE_TYPE MultiSample,
                                       DWORD MultisampleQuality,
                                       BOOL Discard,
                                       IDirect3DSurface9 **ppSurface,
                                       HANDLE *pSharedHandle )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_UpdateSurface( struct NineDevice9 *This,
                           IDirect3DSurface9 *pSourceSurface,
                           const RECT *pSourceRect,
                           IDirect3DSurface9 *pDestinationSurface,
                           const POINT *pDestPoint )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_UpdateTexture( struct NineDevice9 *This,
                           IDirect3DBaseTexture9 *pSourceTexture,
                           IDirect3DBaseTexture9 *pDestinationTexture )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_GetRenderTargetData( struct NineDevice9 *This,
                                 IDirect3DSurface9 *pRenderTarget,
                                 IDirect3DSurface9 *pDestSurface )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_GetFrontBufferData( struct NineDevice9 *This,
                                UINT iSwapChain,
                                IDirect3DSurface9 *pDestSurface )
{
    user_assert(pDestSurface != NULL, D3DERR_INVALIDCALL);
    user_assert(iSwapChain < This->nswapchains, D3DERR_INVALIDCALL);

    return NineSwapChain9_GetFrontBufferData(This->swapchains[iSwapChain],
                                             pDestSurface);
}

HRESULT WINAPI
NineDevice9_StretchRect( struct NineDevice9 *This,
                         IDirect3DSurface9 *pSourceSurface,
                         const RECT *pSourceRect,
                         IDirect3DSurface9 *pDestSurface,
                         const RECT *pDestRect,
                         D3DTEXTUREFILTERTYPE Filter )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_ColorFill( struct NineDevice9 *This,
                       IDirect3DSurface9 *pSurface,
                       const RECT *pRect,
                       D3DCOLOR color )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_CreateOffscreenPlainSurface( struct NineDevice9 *This,
                                         UINT Width,
                                         UINT Height,
                                         D3DFORMAT Format,
                                         D3DPOOL Pool,
                                         IDirect3DSurface9 **ppSurface,
                                         HANDLE *pSharedHandle )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_SetRenderTarget( struct NineDevice9 *This,
                             DWORD RenderTargetIndex,
                             IDirect3DSurface9 *pRenderTarget )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_GetRenderTarget( struct NineDevice9 *This,
                             DWORD RenderTargetIndex,
                             IDirect3DSurface9 **ppRenderTarget )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_SetDepthStencilSurface( struct NineDevice9 *This,
                                    IDirect3DSurface9 *pNewZStencil )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_GetDepthStencilSurface( struct NineDevice9 *This,
                                    IDirect3DSurface9 **ppZStencilSurface )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_BeginScene( struct NineDevice9 *This )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_EndScene( struct NineDevice9 *This )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_Clear( struct NineDevice9 *This,
                   DWORD Count,
                   const D3DRECT *pRects,
                   DWORD Flags,
                   D3DCOLOR Color,
                   float Z,
                   DWORD Stencil )
{
    int i, j;
    unsigned ds = ((Flags & D3DCLEAR_ZBUFFER) ? PIPE_CLEAR_DEPTH : 0) |
                  ((Flags & D3DCLEAR_STENCIL) ? PIPE_CLEAR_STENCIL : 0);
    union pipe_color_union rgba;
    const D3DRECT fullrect = {
        0, 0,
        This->fbstate.cbufs[0]->width-1,
        This->fbstate.cbufs[0]->height-1
    };

    user_assert((pRects && Count != 0) || (!pRects && Count == 0),
                D3DERR_INVALIDCALL);

    if (!pRects) {
        pRects = &fullrect;
        Count = 1;
    }

    d3dcolor_to_rgba(Color, rgba.f);

    for (i = 0; i < This->fbstate.nr_cbufs; ++i) {
        for (j = 0; j < Count; ++j) {
            if (Flags & D3DCLEAR_TARGET) {
                unsigned x0, x1, y0, y1, width, height;

                /* users are idiots, and for sure someone will manage to pass
                 * the coordinates in in the wrong order, or out of bounds */
                if (pRects[j].x2 >= pRects[j].x1) {
                    x0 = pRects[j].x1;
                    x1 = pRects[j].x2;
                } else {
                    x0 = pRects[j].x2;
                    x1 = pRects[j].x1;
                }
                if (x0 < 0) { x0 = 0; }
                if (x1 >= This->fbstate.width) { x1 = This->fbstate.width-1; }

                if (pRects[j].y2 >= pRects[j].y1) {
                    y0 = pRects[j].y1;
                    y1 = pRects[j].y2;
                } else {
                    y0 = pRects[j].y2;
                    y1 = pRects[j].y1;
                }
                if (y0 < 0) { y0 = 0; }
                if (y1 >= This->fbstate.height) { y1 = This->fbstate.height-1; }

                /* coordinates represent corners of a rectangle, meaning if
                 * they happen to be identical, it's still one pixel wide */
                width = x1-x0+1;
                height = y1-y0+1;

                This->pipe->clear_render_target(This->pipe,
                                                This->fbstate.cbufs[i],
                                                &rgba, x0, y0, width, height);
            }
        }
    }
    if (ds) {
        user_assert(This->fbstate.zsbuf, D3DERR_INVALIDCALL);
        This->pipe->clear_depth_stencil(This->pipe, This->fbstate.zsbuf,
                                        ds, Z, Stencil, 0, 0,
                                        This->fbstate.zsbuf->width,
                                        This->fbstate.zsbuf->height);
    }

    return D3D_OK;
}

HRESULT WINAPI
NineDevice9_SetTransform( struct NineDevice9 *This,
                          D3DTRANSFORMSTATETYPE State,
                          const D3DMATRIX *pMatrix )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_GetTransform( struct NineDevice9 *This,
                          D3DTRANSFORMSTATETYPE State,
                          D3DMATRIX *pMatrix )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_MultiplyTransform( struct NineDevice9 *This,
                               D3DTRANSFORMSTATETYPE State,
                               const D3DMATRIX *pMatrix )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_SetViewport( struct NineDevice9 *This,
                         const D3DVIEWPORT9 *pViewport )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_GetViewport( struct NineDevice9 *This,
                         D3DVIEWPORT9 *pViewport )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_SetMaterial( struct NineDevice9 *This,
                         const D3DMATERIAL9 *pMaterial )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_GetMaterial( struct NineDevice9 *This,
                         D3DMATERIAL9 *pMaterial )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_SetLight( struct NineDevice9 *This,
                      DWORD Index,
                      const D3DLIGHT9 *pLight )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_GetLight( struct NineDevice9 *This,
                      DWORD Index,
                      D3DLIGHT9 *pLight )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_LightEnable( struct NineDevice9 *This,
                         DWORD Index,
                         BOOL Enable )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_GetLightEnable( struct NineDevice9 *This,
                            DWORD Index,
                            BOOL *pEnable )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_SetClipPlane( struct NineDevice9 *This,
                          DWORD Index,
                          const float *pPlane )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_GetClipPlane( struct NineDevice9 *This,
                          DWORD Index,
                          float *pPlane )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_SetRenderState( struct NineDevice9 *This,
                            D3DRENDERSTATETYPE State,
                            DWORD Value )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_GetRenderState( struct NineDevice9 *This,
                            D3DRENDERSTATETYPE State,
                            DWORD *pValue )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_CreateStateBlock( struct NineDevice9 *This,
                              D3DSTATEBLOCKTYPE Type,
                              IDirect3DStateBlock9 **ppSB )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_BeginStateBlock( struct NineDevice9 *This )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_EndStateBlock( struct NineDevice9 *This,
                           IDirect3DStateBlock9 **ppSB )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_SetClipStatus( struct NineDevice9 *This,
                           const D3DCLIPSTATUS9 *pClipStatus )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_GetClipStatus( struct NineDevice9 *This,
                           D3DCLIPSTATUS9 *pClipStatus )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_GetTexture( struct NineDevice9 *This,
                        DWORD Stage,
                        IDirect3DBaseTexture9 **ppTexture )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_SetTexture( struct NineDevice9 *This,
                        DWORD Stage,
                        IDirect3DBaseTexture9 *pTexture )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_GetTextureStageState( struct NineDevice9 *This,
                                  DWORD Stage,
                                  D3DTEXTURESTAGESTATETYPE Type,
                                  DWORD *pValue )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_SetTextureStageState( struct NineDevice9 *This,
                                  DWORD Stage,
                                  D3DTEXTURESTAGESTATETYPE Type,
                                  DWORD Value )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_GetSamplerState( struct NineDevice9 *This,
                             DWORD Sampler,
                             D3DSAMPLERSTATETYPE Type,
                             DWORD *pValue )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_SetSamplerState( struct NineDevice9 *This,
                             DWORD Sampler,
                             D3DSAMPLERSTATETYPE Type,
                             DWORD Value )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_ValidateDevice( struct NineDevice9 *This,
                            DWORD *pNumPasses )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_SetPaletteEntries( struct NineDevice9 *This,
                               UINT PaletteNumber,
                               const PALETTEENTRY *pEntries )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_GetPaletteEntries( struct NineDevice9 *This,
                               UINT PaletteNumber,
                               PALETTEENTRY *pEntries )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_SetCurrentTexturePalette( struct NineDevice9 *This,
                                      UINT PaletteNumber )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_GetCurrentTexturePalette( struct NineDevice9 *This,
                                      UINT *PaletteNumber )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_SetScissorRect( struct NineDevice9 *This,
                            const RECT *pRect )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_GetScissorRect( struct NineDevice9 *This,
                            RECT *pRect )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_SetSoftwareVertexProcessing( struct NineDevice9 *This,
                                         BOOL bSoftware )
{
    STUB(D3DERR_INVALIDCALL);
}

BOOL WINAPI
NineDevice9_GetSoftwareVertexProcessing( struct NineDevice9 *This )
{
    return (This->params.BehaviorFlags & D3DCREATE_SOFTWARE_VERTEXPROCESSING)
               ? TRUE : FALSE;
}

HRESULT WINAPI
NineDevice9_SetNPatchMode( struct NineDevice9 *This,
                           float nSegments )
{
    STUB(D3DERR_INVALIDCALL);
}

float WINAPI
NineDevice9_GetNPatchMode( struct NineDevice9 *This )
{
    STUB(0);
}

HRESULT WINAPI
NineDevice9_DrawPrimitive( struct NineDevice9 *This,
                           D3DPRIMITIVETYPE PrimitiveType,
                           UINT StartVertex,
                           UINT PrimitiveCount )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_DrawIndexedPrimitive( struct NineDevice9 *This,
                                  D3DPRIMITIVETYPE PrimitiveType,
                                  INT BaseVertexIndex,
                                  UINT MinVertexIndex,
                                  UINT NumVertices,
                                  UINT startIndex,
                                  UINT primCount )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_DrawPrimitiveUP( struct NineDevice9 *This,
                             D3DPRIMITIVETYPE PrimitiveType,
                             UINT PrimitiveCount,
                             const void *pVertexStreamZeroData,
                             UINT VertexStreamZeroStride )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_DrawIndexedPrimitiveUP( struct NineDevice9 *This,
                                    D3DPRIMITIVETYPE PrimitiveType,
                                    UINT MinVertexIndex,
                                    UINT NumVertices,
                                    UINT PrimitiveCount,
                                    const void *pIndexData,
                                    D3DFORMAT IndexDataFormat,
                                    const void *pVertexStreamZeroData,
                                    UINT VertexStreamZeroStride )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_ProcessVertices( struct NineDevice9 *This,
                             UINT SrcStartIndex,
                             UINT DestIndex,
                             UINT VertexCount,
                             IDirect3DVertexBuffer9 *pDestBuffer,
                             IDirect3DVertexDeclaration9 *pVertexDecl,
                             DWORD Flags )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_CreateVertexDeclaration( struct NineDevice9 *This,
                                     const D3DVERTEXELEMENT9 *pVertexElements,
                                     IDirect3DVertexDeclaration9 **ppDecl )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_SetVertexDeclaration( struct NineDevice9 *This,
                                  IDirect3DVertexDeclaration9 *pDecl )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_GetVertexDeclaration( struct NineDevice9 *This,
                                  IDirect3DVertexDeclaration9 **ppDecl )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_SetFVF( struct NineDevice9 *This,
                    DWORD FVF )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_GetFVF( struct NineDevice9 *This,
                    DWORD *pFVF )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_CreateVertexShader( struct NineDevice9 *This,
                                const DWORD *pFunction,
                                IDirect3DVertexShader9 **ppShader )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_SetVertexShader( struct NineDevice9 *This,
                             IDirect3DVertexShader9 *pShader )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_GetVertexShader( struct NineDevice9 *This,
                             IDirect3DVertexShader9 **ppShader )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_SetVertexShaderConstantF( struct NineDevice9 *This,
                                      UINT StartRegister,
                                      const float *pConstantData,
                                      UINT Vector4fCount )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_GetVertexShaderConstantF( struct NineDevice9 *This,
                                      UINT StartRegister,
                                      float *pConstantData,
                                      UINT Vector4fCount )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_SetVertexShaderConstantI( struct NineDevice9 *This,
                                      UINT StartRegister,
                                      const int *pConstantData,
                                      UINT Vector4iCount )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_GetVertexShaderConstantI( struct NineDevice9 *This,
                                      UINT StartRegister,
                                      int *pConstantData,
                                      UINT Vector4iCount )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_SetVertexShaderConstantB( struct NineDevice9 *This,
                                      UINT StartRegister,
                                      const BOOL *pConstantData,
                                      UINT BoolCount )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_GetVertexShaderConstantB( struct NineDevice9 *This,
                                      UINT StartRegister,
                                      BOOL *pConstantData,
                                      UINT BoolCount )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_SetStreamSource( struct NineDevice9 *This,
                             UINT StreamNumber,
                             IDirect3DVertexBuffer9 *pStreamData,
                             UINT OffsetInBytes,
                             UINT Stride )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_GetStreamSource( struct NineDevice9 *This,
                             UINT StreamNumber,
                             IDirect3DVertexBuffer9 **ppStreamData,
                             UINT *pOffsetInBytes,
                             UINT *pStride )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_SetStreamSourceFreq( struct NineDevice9 *This,
                                 UINT StreamNumber,
                                 UINT Setting )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_GetStreamSourceFreq( struct NineDevice9 *This,
                                 UINT StreamNumber,
                                 UINT *pSetting )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_SetIndices( struct NineDevice9 *This,
                        IDirect3DIndexBuffer9 *pIndexData )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_GetIndices( struct NineDevice9 *This,
                        IDirect3DIndexBuffer9 **ppIndexData,
                        UINT *pBaseVertexIndex )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_CreatePixelShader( struct NineDevice9 *This,
                               const DWORD *pFunction,
                               IDirect3DPixelShader9 **ppShader )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_SetPixelShader( struct NineDevice9 *This,
                            IDirect3DPixelShader9 *pShader )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_GetPixelShader( struct NineDevice9 *This,
                            IDirect3DPixelShader9 **ppShader )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_SetPixelShaderConstantF( struct NineDevice9 *This,
                                     UINT StartRegister,
                                     const float *pConstantData,
                                     UINT Vector4fCount )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_GetPixelShaderConstantF( struct NineDevice9 *This,
                                     UINT StartRegister,
                                     float *pConstantData,
                                     UINT Vector4fCount )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_SetPixelShaderConstantI( struct NineDevice9 *This,
                                     UINT StartRegister,
                                     const int *pConstantData,
                                     UINT Vector4iCount )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_GetPixelShaderConstantI( struct NineDevice9 *This,
                                     UINT StartRegister,
                                     int *pConstantData,
                                     UINT Vector4iCount )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_SetPixelShaderConstantB( struct NineDevice9 *This,
                                     UINT StartRegister,
                                     const BOOL *pConstantData,
                                     UINT BoolCount )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_GetPixelShaderConstantB( struct NineDevice9 *This,
                                     UINT StartRegister,
                                     BOOL *pConstantData,
                                     UINT BoolCount )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_DrawRectPatch( struct NineDevice9 *This,
                           UINT Handle,
                           const float *pNumSegs,
                           const D3DRECTPATCH_INFO *pRectPatchInfo )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_DrawTriPatch( struct NineDevice9 *This,
                          UINT Handle,
                          const float *pNumSegs,
                          const D3DTRIPATCH_INFO *pTriPatchInfo )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_DeletePatch( struct NineDevice9 *This,
                         UINT Handle )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_CreateQuery( struct NineDevice9 *This,
                         D3DQUERYTYPE Type,
                         IDirect3DQuery9 **ppQuery )
{
    STUB(D3DERR_INVALIDCALL);
}

IDirect3DDevice9Vtbl NineDevice9_vtable = {
    (void *)NineUnknown_QueryInterface,
    (void *)NineUnknown_AddRef,
    (void *)NineUnknown_Release,
    (void *)NineDevice9_TestCooperativeLevel,
    (void *)NineDevice9_GetAvailableTextureMem,
    (void *)NineDevice9_EvictManagedResources,
    (void *)NineDevice9_GetDirect3D,
    (void *)NineDevice9_GetDeviceCaps,
    (void *)NineDevice9_GetDisplayMode,
    (void *)NineDevice9_GetCreationParameters,
    (void *)NineDevice9_SetCursorProperties,
    (void *)NineDevice9_SetCursorPosition,
    (void *)NineDevice9_ShowCursor,
    (void *)NineDevice9_CreateAdditionalSwapChain,
    (void *)NineDevice9_GetSwapChain,
    (void *)NineDevice9_GetNumberOfSwapChains,
    (void *)NineDevice9_Reset,
    (void *)NineDevice9_Present,
    (void *)NineDevice9_GetBackBuffer,
    (void *)NineDevice9_GetRasterStatus,
    (void *)NineDevice9_SetDialogBoxMode,
    (void *)NineDevice9_SetGammaRamp,
    (void *)NineDevice9_GetGammaRamp,
    (void *)NineDevice9_CreateTexture,
    (void *)NineDevice9_CreateVolumeTexture,
    (void *)NineDevice9_CreateCubeTexture,
    (void *)NineDevice9_CreateVertexBuffer,
    (void *)NineDevice9_CreateIndexBuffer,
    (void *)NineDevice9_CreateRenderTarget,
    (void *)NineDevice9_CreateDepthStencilSurface,
    (void *)NineDevice9_UpdateSurface,
    (void *)NineDevice9_UpdateTexture,
    (void *)NineDevice9_GetRenderTargetData,
    (void *)NineDevice9_GetFrontBufferData,
    (void *)NineDevice9_StretchRect,
    (void *)NineDevice9_ColorFill,
    (void *)NineDevice9_CreateOffscreenPlainSurface,
    (void *)NineDevice9_SetRenderTarget,
    (void *)NineDevice9_GetRenderTarget,
    (void *)NineDevice9_SetDepthStencilSurface,
    (void *)NineDevice9_GetDepthStencilSurface,
    (void *)NineDevice9_BeginScene,
    (void *)NineDevice9_EndScene,
    (void *)NineDevice9_Clear,
    (void *)NineDevice9_SetTransform,
    (void *)NineDevice9_GetTransform,
    (void *)NineDevice9_MultiplyTransform,
    (void *)NineDevice9_SetViewport,
    (void *)NineDevice9_GetViewport,
    (void *)NineDevice9_SetMaterial,
    (void *)NineDevice9_GetMaterial,
    (void *)NineDevice9_SetLight,
    (void *)NineDevice9_GetLight,
    (void *)NineDevice9_LightEnable,
    (void *)NineDevice9_GetLightEnable,
    (void *)NineDevice9_SetClipPlane,
    (void *)NineDevice9_GetClipPlane,
    (void *)NineDevice9_SetRenderState,
    (void *)NineDevice9_GetRenderState,
    (void *)NineDevice9_CreateStateBlock,
    (void *)NineDevice9_BeginStateBlock,
    (void *)NineDevice9_EndStateBlock,
    (void *)NineDevice9_SetClipStatus,
    (void *)NineDevice9_GetClipStatus,
    (void *)NineDevice9_GetTexture,
    (void *)NineDevice9_SetTexture,
    (void *)NineDevice9_GetTextureStageState,
    (void *)NineDevice9_SetTextureStageState,
    (void *)NineDevice9_GetSamplerState,
    (void *)NineDevice9_SetSamplerState,
    (void *)NineDevice9_ValidateDevice,
    (void *)NineDevice9_SetPaletteEntries,
    (void *)NineDevice9_GetPaletteEntries,
    (void *)NineDevice9_SetCurrentTexturePalette,
    (void *)NineDevice9_GetCurrentTexturePalette,
    (void *)NineDevice9_SetScissorRect,
    (void *)NineDevice9_GetScissorRect,
    (void *)NineDevice9_SetSoftwareVertexProcessing,
    (void *)NineDevice9_GetSoftwareVertexProcessing,
    (void *)NineDevice9_SetNPatchMode,
    (void *)NineDevice9_GetNPatchMode,
    (void *)NineDevice9_DrawPrimitive,
    (void *)NineDevice9_DrawIndexedPrimitive,
    (void *)NineDevice9_DrawPrimitiveUP,
    (void *)NineDevice9_DrawIndexedPrimitiveUP,
    (void *)NineDevice9_ProcessVertices,
    (void *)NineDevice9_CreateVertexDeclaration,
    (void *)NineDevice9_SetVertexDeclaration,
    (void *)NineDevice9_GetVertexDeclaration,
    (void *)NineDevice9_SetFVF,
    (void *)NineDevice9_GetFVF,
    (void *)NineDevice9_CreateVertexShader,
    (void *)NineDevice9_SetVertexShader,
    (void *)NineDevice9_GetVertexShader,
    (void *)NineDevice9_SetVertexShaderConstantF,
    (void *)NineDevice9_GetVertexShaderConstantF,
    (void *)NineDevice9_SetVertexShaderConstantI,
    (void *)NineDevice9_GetVertexShaderConstantI,
    (void *)NineDevice9_SetVertexShaderConstantB,
    (void *)NineDevice9_GetVertexShaderConstantB,
    (void *)NineDevice9_SetStreamSource,
    (void *)NineDevice9_GetStreamSource,
    (void *)NineDevice9_SetStreamSourceFreq,
    (void *)NineDevice9_GetStreamSourceFreq,
    (void *)NineDevice9_SetIndices,
    (void *)NineDevice9_GetIndices,
    (void *)NineDevice9_CreatePixelShader,
    (void *)NineDevice9_SetPixelShader,
    (void *)NineDevice9_GetPixelShader,
    (void *)NineDevice9_SetPixelShaderConstantF,
    (void *)NineDevice9_GetPixelShaderConstantF,
    (void *)NineDevice9_SetPixelShaderConstantI,
    (void *)NineDevice9_GetPixelShaderConstantI,
    (void *)NineDevice9_SetPixelShaderConstantB,
    (void *)NineDevice9_GetPixelShaderConstantB,
    (void *)NineDevice9_DrawRectPatch,
    (void *)NineDevice9_DrawTriPatch,
    (void *)NineDevice9_DeletePatch,
    (void *)NineDevice9_CreateQuery
};

static const GUID *NineDevice9_IIDs[] = {
    &IID_IDirect3DDevice9,
    &IID_IUnknown,
    NULL
};

HRESULT
NineDevice9_new( struct pipe_screen *pScreen,
                 D3DDEVICE_CREATION_PARAMETERS *pCreationParameters,
                 D3DCAPS9 *pCaps,
                 IDirect3D9 *pD3D9,
                 ID3DPresentFactory *pPresentationFactory,
                 PPRESENT_TO_RESOURCE pPTR,
                 struct NineDevice9 **ppOut )
{
    NINE_NEW(NineDevice9, ppOut, /* args */
             pScreen, pCreationParameters, pCaps,
             pD3D9, pPresentationFactory, pPTR);
}
