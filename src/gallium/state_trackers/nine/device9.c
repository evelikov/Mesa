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
#include "stateblock9.h"
#include "surface9.h"
#include "swapchain9.h"
#include "vertexbuffer9.h"
#include "vertexdeclaration9.h"
#include "vertexshader9.h"
#include "pixelshader9.h"
#include "query9.h"
#include "texture9.h"
#include "nine_helpers.h"
#include "nine_pipe.h"
#include "nine_ff.h"

#include "pipe/p_screen.h"
#include "pipe/p_context.h"
#include "util/u_math.h"
#include "util/u_inlines.h"
#include "util/u_format.h"
#include "util/u_gen_mipmap.h"

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
    struct pipe_context *pipe;
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
    pipe = This->pipe;

    This->cso = cso_create_context(This->pipe);
    if (!This->cso) { return E_OUTOFMEMORY; } /* also a guess */

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
                                          &This->state.rt[i]);
        if (FAILED(hr)) { return hr; }
    }

    /* Create constant buffers. */
    {
        struct pipe_constant_buffer cb;
        struct pipe_resource tmpl;
        unsigned max_const_vs, max_const_ps;

        max_const_vs = pScreen->get_shader_param(pScreen, PIPE_SHADER_VERTEX,
                                                 PIPE_SHADER_CAP_MAX_CONSTS);
        max_const_ps = pScreen->get_shader_param(pScreen, PIPE_SHADER_FRAGMENT,
                                                 PIPE_SHADER_CAP_MAX_CONSTS);

        max_const_vs = MIN2(max_const_vs, NINE_MAX_CONST_F);
        max_const_ps = MIN2(max_const_ps, NINE_MAX_CONST_F);

        tmpl.target = PIPE_BUFFER;
        tmpl.format = PIPE_FORMAT_R8_UNORM;
        tmpl.height0 = 1;
        tmpl.depth0 = 1;
        tmpl.array_size = 1;
        tmpl.last_level = 0;
        tmpl.nr_samples = 0;
        tmpl.usage = PIPE_USAGE_DYNAMIC;
        tmpl.bind = PIPE_BIND_CONSTANT_BUFFER;
        tmpl.flags = 0;

        tmpl.width0 = max_const_vs * 16;
        This->constbuf_vs = pScreen->resource_create(pScreen, &tmpl);

        tmpl.width0 = max_const_ps * 16;
        This->constbuf_ps = pScreen->resource_create(pScreen, &tmpl);

        if (!This->constbuf_vs || !This->constbuf_ps)
            return E_OUTOFMEMORY;

        cb.user_buffer = NULL; /* XXX: fix your drivers !!! */
        cb.buffer_offset = 0;
        cb.buffer = This->constbuf_vs;
        cb.buffer_size = This->constbuf_vs->width0;
        pipe->set_constant_buffer(pipe, PIPE_SHADER_VERTEX, 0, &cb);

        cb.buffer = This->constbuf_ps;
        cb.buffer_size = This->constbuf_ps->width0;
        pipe->set_constant_buffer(pipe, PIPE_SHADER_FRAGMENT, 0, &cb);
    }

    This->vs_bool_true = pScreen->get_shader_param(pScreen,
        PIPE_SHADER_VERTEX,
        PIPE_SHADER_CAP_INTEGERS) ? 0xFFFFFFFF : fui(1.0f);
    This->ps_bool_true = pScreen->get_shader_param(pScreen,
        PIPE_SHADER_FRAGMENT,
        PIPE_SHADER_CAP_INTEGERS) ? 0xFFFFFFFF : fui(1.0f);

    This->gen_mipmap = util_create_gen_mipmap(This->pipe, This->cso);
    if (!This->gen_mipmap)
        return E_OUTOFMEMORY;

    nine_state_set_defaults(&This->state, &This->caps);

    This->state.changed.group = NINE_STATE_FB;
    nine_update_state(This);

    ID3DPresentFactory_Release(This->present);

    return D3D_OK;
}

void
NineDevice9_dtor( struct NineDevice9 *This )
{
    unsigned i;

    nine_reference(&This->record, NULL);

    for (i = 0; i < This->caps.NumSimultaneousRTs; ++i)
       nine_reference(&This->state.rt[i], NULL);
    nine_reference(&This->state.ds, NULL);

    pipe_resource_reference(&This->constbuf_vs, NULL);
    pipe_resource_reference(&This->constbuf_ps, NULL);

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
    struct NineTexture9 *tex;
    HRESULT hr;

    Usage &= D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_DEPTHSTENCIL | D3DUSAGE_DMAP |
             D3DUSAGE_DYNAMIC | D3DUSAGE_NONSECURE | D3DUSAGE_RENDERTARGET |
             D3DUSAGE_SOFTWAREPROCESSING | D3DUSAGE_TEXTAPI;

    user_assert(Width && Height, D3DERR_INVALIDCALL);
    user_assert(!pSharedHandle || Pool != D3DPOOL_SYSTEMMEM || Levels == 1,
                D3DERR_INVALIDCALL);

    hr = NineTexture9_new(This, Width, Height, Levels, Usage, Format, Pool,
                          &tex, pSharedHandle);
    if (SUCCEEDED(hr))
        *ppTexture = (IDirect3DTexture9 *)tex;

    return hr;
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

static HRESULT
create_zs_or_rt_surface(struct NineDevice9 *This,
                        boolean zs,
                        UINT Width, UINT Height,
                        D3DFORMAT Format,
                        D3DMULTISAMPLE_TYPE MultiSample,
                        DWORD MultisampleQuality,
                        BOOL Discard_or_Lockable,
                        IDirect3DSurface9 **ppSurface,
                        HANDLE *pSharedHandle)
{
    struct NineSurface9 *surface;
    struct pipe_screen *screen = This->screen;
    struct pipe_resource *resource = NULL;
    HRESULT hr;
    D3DSURFACE_DESC desc;
    struct pipe_resource templ;

    assert(!pSharedHandle);
    user_assert(Width && Height, D3DERR_INVALIDCALL);

    templ.target = PIPE_TEXTURE_2D;
    templ.format = d3d9_to_pipe_format(Format);
    templ.width0 = Width;
    templ.height0 = Height;
    templ.depth0 = 1;
    templ.array_size = 1;
    templ.last_level = 0;
    templ.nr_samples = (unsigned)MultiSample;
    templ.usage = PIPE_USAGE_STATIC;
    templ.bind = zs ? PIPE_BIND_DEPTH_STENCIL : PIPE_BIND_RENDER_TARGET;
    templ.flags = 0;

    /* since resource_create doesn't return an error code, check format here */
    user_assert(screen->is_format_supported(screen, templ.format, templ.target,
                    templ.nr_samples, templ.bind), D3DERR_INVALIDCALL);

    resource = screen->resource_create(screen, &templ);

    user_assert(resource, D3DERR_OUTOFVIDEOMEMORY);

    desc.Format = Format;
    desc.Type = D3DRTYPE_SURFACE;
    desc.Usage = zs ? D3DUSAGE_DEPTHSTENCIL : D3DUSAGE_RENDERTARGET;
    desc.Pool = D3DPOOL_DEFAULT;
    desc.MultiSampleType = MultiSample;
    desc.MultiSampleQuality = MultisampleQuality;
    desc.Width = Width;
    desc.Height = Height;

    hr = NineSurface9_new(This, NULL, resource, 0, 0, &desc, &surface);
    pipe_resource_reference(&resource, NULL);

    if (SUCCEEDED(hr))
        *ppSurface = (IDirect3DSurface9 *)surface;
    return hr;
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
    return create_zs_or_rt_surface(This, FALSE, Width, Height, Format,
                                   MultiSample, MultisampleQuality,
                                   Lockable, ppSurface, pSharedHandle);
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
    return create_zs_or_rt_surface(This, TRUE, Width, Height, Format,
                                   MultiSample, MultisampleQuality,
                                   Discard, ppSurface, pSharedHandle);
}

HRESULT WINAPI
NineDevice9_UpdateSurface( struct NineDevice9 *This,
                           IDirect3DSurface9 *pSourceSurface,
                           const RECT *pSourceRect,
                           IDirect3DSurface9 *pDestinationSurface,
                           const POINT *pDestPoint )
{
    struct NineSurface9 *dst = NineSurface9(pDestinationSurface);
    struct NineSurface9 *src = NineSurface9(pSourceSurface);

    user_assert(dst->base.pool == D3DPOOL_DEFAULT, D3DERR_INVALIDCALL);
    user_assert(src->base.pool == D3DPOOL_SYSTEMMEM, D3DERR_INVALIDCALL);
    user_assert(dst->desc.Format == src->desc.Format, D3DERR_INVALIDCALL);

    return NineSurface9_UploadFromSurface(dst, src);
}

HRESULT WINAPI
NineDevice9_UpdateTexture( struct NineDevice9 *This,
                           IDirect3DBaseTexture9 *pSourceTexture,
                           IDirect3DBaseTexture9 *pDestinationTexture )
{
    struct NineBaseTexture9 *dst = NineBaseTexture9(pDestinationTexture);
    struct NineBaseTexture9 *src = NineBaseTexture9(pSourceTexture);

    user_assert(pSourceTexture != pDestinationTexture, D3DERR_INVALIDCALL);
    user_assert(dst->base.type == src->base.type, D3DERR_INVALIDCALL);

    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineDevice9_GetRenderTargetData( struct NineDevice9 *This,
                                 IDirect3DSurface9 *pRenderTarget,
                                 IDirect3DSurface9 *pDestSurface )
{
    struct NineSurface9 *dst = NineSurface9(pDestSurface);
    struct NineSurface9 *src = NineSurface9(pRenderTarget);

    user_assert(dst->desc.Pool == D3DPOOL_DEFAULT, D3DERR_INVALIDCALL);
    user_assert(src->desc.Pool == D3DPOOL_SYSTEMMEM, D3DERR_INVALIDCALL);
    user_assert(src->desc.MultiSampleType < 2, D3DERR_INVALIDCALL);
    user_assert(dst->desc.Format == src->desc.Format, D3DERR_INVALIDCALL);

    return NineSurface9_DownloadFromSurface(dst, src);
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
    struct pipe_screen *screen = This->screen;
    struct pipe_context *pipe = This->pipe;
    struct NineSurface9 *dst = NineSurface9(pDestSurface);
    struct NineSurface9 *src = NineSurface9(pSourceSurface);
    struct pipe_resource *dst_res = NineSurface9_GetResource(dst);
    struct pipe_resource *src_res = NineSurface9_GetResource(src);
    const boolean zs = util_format_is_depth_or_stencil(dst_res->format);
    struct pipe_blit_info blit;

    user_assert(!zs || !This->in_scene, D3DERR_INVALIDCALL);
    user_assert(!zs || !pSourceRect ||
                (pSourceRect->left == 0 &&
                 pSourceRect->top == 0 &&
                 pSourceRect->right == src->desc.Width &&
                 pSourceRect->bottom == src->desc.Height), D3DERR_INVALIDCALL);
    user_assert(!zs || !pDestRect ||
                (pDestRect->left == 0 &&
                 pDestRect->top == 0 &&
                 pDestRect->right == dst->desc.Width &&
                 pDestRect->bottom == dst->desc.Height), D3DERR_INVALIDCALL);
    user_assert(screen->is_format_supported(screen, dst_res->format,
                                            dst_res->target,
                                            dst_res->nr_samples,
                                            zs ? PIPE_BIND_DEPTH_STENCIL :
                                            PIPE_BIND_RENDER_TARGET),
                D3DERR_INVALIDCALL);
    user_assert(screen->is_format_supported(screen, src_res->format,
                                            src_res->target,
                                            src_res->nr_samples,
                                            PIPE_BIND_SAMPLER_VIEW),
                D3DERR_INVALIDCALL);

    blit.dst.resource = dst_res;
    blit.dst.level = dst->level;
    blit.dst.box.z = dst->layer;
    blit.dst.box.depth = 1;
    blit.dst.format = dst_res->format;
    if (pDestRect) {
        rect_to_pipe_box_xy_only(&blit.dst.box, pDestRect);
    } else {
       blit.dst.box.x = 0;
       blit.dst.box.y = 0;
       blit.dst.box.width = dst->desc.Width;
       blit.dst.box.height = dst->desc.Height;
    }
    blit.src.resource = src_res;
    blit.src.level = src->level;
    blit.src.box.z = src->layer;
    blit.src.box.depth = 1;
    blit.src.format = src_res->format;
    if (pSourceRect) {
        rect_to_pipe_box_xy_only(&blit.src.box, pSourceRect);
    } else {
       blit.src.box.x = 0;
       blit.src.box.y = 0;
       blit.src.box.width = src->desc.Width;
       blit.src.box.height = src->desc.Height;
    }
    blit.mask = zs ? PIPE_MASK_ZS : PIPE_MASK_RGBA;
    blit.filter = Filter == D3DTEXF_LINEAR ?
       PIPE_TEX_FILTER_LINEAR : PIPE_TEX_FILTER_NEAREST;
    blit.scissor_enable = FALSE;

    pipe->blit(pipe, &blit);

    return D3D_OK;
}

HRESULT WINAPI
NineDevice9_ColorFill( struct NineDevice9 *This,
                       IDirect3DSurface9 *pSurface,
                       const RECT *pRect,
                       D3DCOLOR color )
{
    struct pipe_context *pipe = This->pipe;
    struct NineSurface9 *surf = NineSurface9(pSurface);
    unsigned x, y, w, h;
    union pipe_color_union rgba;

    user_assert(surf->base.pool == D3DPOOL_DEFAULT, D3DERR_INVALIDCALL);

    /* XXX: resource usage == rt, rt texture, or off-screen plain */

    if (pRect) {
        x = pRect->left;
        y = pRect->top;
        w = pRect->right - pRect->left;
        h = pRect->bottom - pRect->top;
    } else{
        x = 0;
        y = 0;
        w = surf->surface->width;
        h = surf->surface->height;
    }
    d3dcolor_to_pipe_color_union(&rgba, color);

    pipe->clear_render_target(pipe, NineSurface9_GetSurface(surf), &rgba,
                              x, y, w, h);

    return D3D_OK;
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
    const unsigned i = RenderTargetIndex;

    user_assert(i < This->caps.NumSimultaneousRTs, D3DERR_INVALIDCALL);

    if (This->state.rt[i] != NineSurface9(pRenderTarget)) {
       This->state.changed.group |= NINE_STATE_FB;
       nine_reference(&This->state.rt[i], pRenderTarget);
    }
    return D3D_OK;
}

HRESULT WINAPI
NineDevice9_GetRenderTarget( struct NineDevice9 *This,
                             DWORD RenderTargetIndex,
                             IDirect3DSurface9 **ppRenderTarget )
{
    const unsigned i = RenderTargetIndex;

    user_assert(i < This->caps.NumSimultaneousRTs, D3DERR_INVALIDCALL);

    nine_reference(ppRenderTarget, This->state.rt[i]);

    return D3D_OK;
}

HRESULT WINAPI
NineDevice9_SetDepthStencilSurface( struct NineDevice9 *This,
                                    IDirect3DSurface9 *pNewZStencil )
{
    if (This->state.ds != NineSurface9(pNewZStencil)) {
        This->state.changed.group |= NINE_STATE_FB;
        nine_reference(&This->state.ds, pNewZStencil);
    }
    return D3D_OK;
}

HRESULT WINAPI
NineDevice9_GetDepthStencilSurface( struct NineDevice9 *This,
                                    IDirect3DSurface9 **ppZStencilSurface )
{
    nine_reference(ppZStencilSurface, This->state.ds);
    return D3D_OK;
}

HRESULT WINAPI
NineDevice9_BeginScene( struct NineDevice9 *This )
{
    user_assert(!This->in_scene, D3DERR_INVALIDCALL);
    This->in_scene = TRUE;
    /* Do we want to do anything else here ? */
    return D3D_OK;
}

HRESULT WINAPI
NineDevice9_EndScene( struct NineDevice9 *This )
{
    user_assert(This->in_scene, D3DERR_INVALIDCALL);
    This->in_scene = FALSE;
    return D3D_OK;
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
        This->state.fb.cbufs[0]->width-1,
        This->state.fb.cbufs[0]->height-1
    };

    DBG("\n");

    user_assert((pRects && Count != 0) || (!pRects && Count == 0),
                D3DERR_INVALIDCALL);

    if (!pRects) {
        pRects = &fullrect;
        Count = 1;
    }

    d3dcolor_to_pipe_color_union(&rgba, Color);

    for (i = 0; i < This->state.fb.nr_cbufs; ++i) {
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
                if (x1 >= This->state.fb.width) { x1 = This->state.fb.width-1; }

                if (pRects[j].y2 >= pRects[j].y1) {
                    y0 = pRects[j].y1;
                    y1 = pRects[j].y2;
                } else {
                    y0 = pRects[j].y2;
                    y1 = pRects[j].y1;
                }
                if (y0 < 0) { y0 = 0; }
                if (y1 >= This->state.fb.height) { y1 = This->state.fb.height-1; }

                /* coordinates represent corners of a rectangle, meaning if
                 * they happen to be identical, it's still one pixel wide */
                width = x1-x0+1;
                height = y1-y0+1;

                This->pipe->clear_render_target(This->pipe,
                                                This->state.fb.cbufs[i],
                                                &rgba, x0, y0, width, height);
            }
        }
    }
    if (ds) {
        user_assert(This->state.fb.zsbuf, D3DERR_INVALIDCALL);
        This->pipe->clear_depth_stencil(This->pipe, This->state.fb.zsbuf,
                                        ds, Z, Stencil, 0, 0,
                                        This->state.fb.zsbuf->width,
                                        This->state.fb.zsbuf->height);
    }

    return D3D_OK;
}

HRESULT WINAPI
NineDevice9_SetTransform( struct NineDevice9 *This,
                          D3DTRANSFORMSTATETYPE State,
                          const D3DMATRIX *pMatrix )
{
    NINESTATEPOINTER_SET(This);
    D3DMATRIX *M = nine_state_access_transform(state, State, TRUE);
    user_assert(M, D3DERR_INVALIDCALL);

    *M = *pMatrix;
    state->ff.changed.transform[State / 32] |= 1 << (State % 32);
    state->changed.group |= NINE_STATE_FF;

    return D3D_OK;
}

HRESULT WINAPI
NineDevice9_GetTransform( struct NineDevice9 *This,
                          D3DTRANSFORMSTATETYPE State,
                          D3DMATRIX *pMatrix )
{
    NINESTATEPOINTER_GET(This);
    D3DMATRIX *M = nine_state_access_transform(state, State, FALSE);
    user_assert(M, D3DERR_INVALIDCALL);
    *pMatrix = *M;
    return D3D_OK;
}

HRESULT WINAPI
NineDevice9_MultiplyTransform( struct NineDevice9 *This,
                               D3DTRANSFORMSTATETYPE State,
                               const D3DMATRIX *pMatrix )
{
    NINESTATEPOINTER_SET(This);
    D3DMATRIX T;
    D3DMATRIX *M = nine_state_access_transform(state, State, TRUE);
    user_assert(M, D3DERR_INVALIDCALL);

    nine_d3d_matrix_matrix_mul(&T, pMatrix, M);
    return NineDevice9_SetTransform(This, State, &T);
}

HRESULT WINAPI
NineDevice9_SetViewport( struct NineDevice9 *This,
                         const D3DVIEWPORT9 *pViewport )
{
    NINESTATEPOINTER_SET(This);
    state->viewport = *pViewport;
    state->changed.group |= NINE_STATE_VIEWPORT;
    return D3D_OK;
}

HRESULT WINAPI
NineDevice9_GetViewport( struct NineDevice9 *This,
                         D3DVIEWPORT9 *pViewport )
{
    NINESTATEPOINTER_GET(This);
    *pViewport = state->viewport;
    return D3D_OK;
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
    NINESTATEPOINTER_SET(This);
    user_assert(Index < PIPE_MAX_CLIP_PLANES, D3DERR_INVALIDCALL);

    memcpy(&state->clip.ucp[Index][0], pPlane, sizeof(state->clip.ucp[0]));
    state->changed.ucp |= 1 << Index;

    return D3D_OK;
}

HRESULT WINAPI
NineDevice9_GetClipPlane( struct NineDevice9 *This,
                          DWORD Index,
                          float *pPlane )
{
    NINESTATEPOINTER_GET(This);
    user_assert(Index < PIPE_MAX_CLIP_PLANES, D3DERR_INVALIDCALL);

    memcpy(pPlane, &state->clip.ucp[Index][0], sizeof(state->clip.ucp[0]));
    return D3D_OK;
}

HRESULT WINAPI
NineDevice9_SetRenderState( struct NineDevice9 *This,
                            D3DRENDERSTATETYPE State,
                            DWORD Value )
{
    NINESTATEPOINTER_SET(This);
    user_assert(State < Elements(state->rs), D3DERR_INVALIDCALL);

    state->rs[State] = Value;
    state->changed.rs[State / 32] |= 1 << (State % 32);
    state->changed.group |= nine_render_state_group[State];

    return D3D_OK;
}

HRESULT WINAPI
NineDevice9_GetRenderState( struct NineDevice9 *This,
                            D3DRENDERSTATETYPE State,
                            DWORD *pValue )
{
    NINESTATEPOINTER_GET(This);
    user_assert(State < Elements(state->rs), D3DERR_INVALIDCALL);

    *pValue = state->rs[State];
    return D3D_OK;
}

HRESULT WINAPI
NineDevice9_CreateStateBlock( struct NineDevice9 *This,
                              D3DSTATEBLOCKTYPE Type,
                              IDirect3DStateBlock9 **ppSB )
{
    struct NineStateBlock9 *nsb;
    struct nine_state *dst;
    HRESULT hr;
    enum nine_stateblock_type type;

    user_assert(Type == D3DSBT_ALL ||
                Type == D3DSBT_VERTEXSTATE ||
                Type == D3DSBT_PIXELSTATE, D3DERR_INVALIDCALL);

    switch (Type) {
    case D3DSBT_VERTEXSTATE: type = NINESBT_VERTEXSTATE; break;
    case D3DSBT_PIXELSTATE:  type = NINESBT_PIXELSTATE; break;
    default:
       type = NINESBT_ALL;
       break;
    }

    hr = NineStateBlock9_new(This, &nsb, type);
    if (FAILED(hr))
       return hr;
    *ppSB = (IDirect3DStateBlock9 *)nsb;
    dst = &nsb->state;

    dst->changed.group =
       NINE_STATE_TEXTURE |
       NINE_STATE_SAMPLER;

    if (Type == D3DSBT_ALL || Type == D3DSBT_VERTEXSTATE) {
       dst->changed.group |=
          NINE_STATE_VS | NINE_STATE_VS_CONST |
          NINE_STATE_VDECL;
       /* TODO: texture/sampler state */
       memcpy(dst->changed.rs,
              nine_render_states_vertex, sizeof(dst->changed.rs));
       memset(dst->changed.vs_const_f, ~0, sizeof(dst->vs_const_f));
       dst->changed.vs_const_i = 0xffff;
       dst->changed.vs_const_b = 0xffff;
    }
    if (Type == D3DSBT_ALL || Type == D3DSBT_PIXELSTATE) {
       dst->changed.group |=
          NINE_STATE_PS | NINE_STATE_PS_CONST;
       /* TODO: texture/sampler state */
       memcpy(dst->changed.rs,
              nine_render_states_pixel, sizeof(dst->changed.rs));
       memset(dst->changed.ps_const_f, ~0, sizeof(dst->ps_const_f));
       dst->changed.ps_const_i = 0xffff;
       dst->changed.ps_const_b = 0xffff;
    }
    if (Type == D3DSBT_ALL) {
       dst->changed.group |=
          NINE_STATE_VIEWPORT |
          NINE_STATE_SCISSOR |
          NINE_STATE_RASTERIZER |
          NINE_STATE_BLEND |
          NINE_STATE_DSA |
          NINE_STATE_IDXBUF |
          NINE_STATE_MATERIAL |
          NINE_STATE_BLEND_COLOR |
          NINE_STATE_SAMPLE_MASK;
       dst->changed.vtxbuf = (1ULL << This->caps.MaxStreams) - 1;
       dst->changed.stream_freq = dst->changed.vtxbuf;
       dst->changed.ucp = (1 << PIPE_MAX_CLIP_PLANES) - 1;
       memset(dst->changed.rs, ~0, sizeof(dst->changed.rs));
    }
    NineStateBlock9_Capture(NineStateBlock9(*ppSB));

    /* TODO: fixed function state */

    return D3D_OK;
}

HRESULT WINAPI
NineDevice9_BeginStateBlock( struct NineDevice9 *This )
{
    HRESULT hr;

    user_assert(!This->record, D3DERR_INVALIDCALL);

    hr = NineStateBlock9_new(This, &This->record, NINESBT_CUSTOM);
    if (FAILED(hr))
        return hr;

    This->update = &This->record->state;
    return D3D_OK;
}

HRESULT WINAPI
NineDevice9_EndStateBlock( struct NineDevice9 *This,
                           IDirect3DStateBlock9 **ppSB )
{
    user_assert(This->record, D3DERR_INVALIDCALL);

    nine_reference(ppSB, This->record);
    nine_reference(&This->record, NULL);

    This->update = &This->state;
    return D3D_OK;
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
    NINESTATEPOINTER_GET(This);
    user_assert(Stage < This->caps.MaxSimultaneousTextures, D3DERR_INVALIDCALL);
    nine_reference(ppTexture, state->texture[Stage]);
    return D3D_OK;
}

HRESULT WINAPI
NineDevice9_SetTexture( struct NineDevice9 *This,
                        DWORD Stage,
                        IDirect3DBaseTexture9 *pTexture )
{
    NINESTATEPOINTER_SET(This);
    user_assert(Stage < This->caps.MaxSimultaneousTextures, D3DERR_INVALIDCALL);

    nine_reference(&state->texture[Stage], pTexture);
    state->changed.texture |= 1 << Stage;
    state->changed.group |= NINE_STATE_TEXTURE;

    return D3D_OK;
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
    NINESTATEPOINTER_GET(This);
    user_assert(Sampler < This->caps.MaxSimultaneousTextures, D3DERR_INVALIDCALL);
    *pValue = state->samp[Sampler][Type];
    return D3D_OK;
}

HRESULT WINAPI
NineDevice9_SetSamplerState( struct NineDevice9 *This,
                             DWORD Sampler,
                             D3DSAMPLERSTATETYPE Type,
                             DWORD Value )
{
    NINESTATEPOINTER_SET(This);
    user_assert(Sampler < This->caps.MaxSimultaneousTextures, D3DERR_INVALIDCALL);

    state->samp[Sampler][Type] = Value;
    state->changed.group |= NINE_STATE_SAMPLER;
    state->changed.sampler[Sampler] |= 1 << Type;

    return D3D_OK;
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
    NINESTATEPOINTER_SET(This);

    state->scissor.minx = pRect->left;
    state->scissor.miny = pRect->top;
    state->scissor.maxx = pRect->right;
    state->scissor.maxy = pRect->bottom;

    state->changed.group |= NINE_STATE_SCISSOR;

    return D3D_OK;
}

HRESULT WINAPI
NineDevice9_GetScissorRect( struct NineDevice9 *This,
                            RECT *pRect )
{
    NINESTATEPOINTER_GET(This);

    pRect->left   = state->scissor.minx;
    pRect->top    = state->scissor.miny;
    pRect->right  = state->scissor.maxx;
    pRect->bottom = state->scissor.maxy;

    return D3D_OK;
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

static INLINE void
init_draw_info(struct pipe_draw_info *info,
               struct NineDevice9 *dev, D3DPRIMITIVETYPE type, UINT count)
{
    info->mode = d3dprimitivetype_to_pipe_prim(type);
    info->count = prim_count_to_vertex_count(type, count);
    info->start_instance = 0;
    info->instance_count = 1;
    if (dev->state.stream_instancedata_mask & dev->state.stream_usage_mask)
        info->instance_count = MAX2(dev->state.stream_freq[0] & 0x7FFFFF, 1);
    info->primitive_restart = FALSE;
    info->count_from_stream_output = NULL;
    /* info->indirect = NULL; */
}

HRESULT WINAPI
NineDevice9_DrawPrimitive( struct NineDevice9 *This,
                           D3DPRIMITIVETYPE PrimitiveType,
                           UINT StartVertex,
                           UINT PrimitiveCount )
{
    struct pipe_draw_info info;

    DBG("iface %p, PrimitiveType %u, StartVertex %u, PrimitiveCount %u\n",
        This, PrimitiveType, StartVertex, PrimitiveCount);

    nine_update_state(This);

    init_draw_info(&info, This, PrimitiveType, PrimitiveCount);
    info.indexed = FALSE;
    info.start = StartVertex;
    info.index_bias = 0;
    info.min_index = info.start;
    info.max_index = info.count - 1;

    This->pipe->draw_vbo(This->pipe, &info);

    return D3D_OK;
}

HRESULT WINAPI
NineDevice9_DrawIndexedPrimitive( struct NineDevice9 *This,
                                  D3DPRIMITIVETYPE PrimitiveType,
                                  INT BaseVertexIndex,
                                  UINT MinVertexIndex,
                                  UINT NumVertices,
                                  UINT StartIndex,
                                  UINT PrimitiveCount )
{
    struct pipe_draw_info info;

    DBG("iface %p, PrimitiveType %u, BaseVertexIndex %u, MinVertexIndex %u "
        "NumVertices %u, StartIndex %u, PrimitiveCount %u\n",
        This, PrimitiveType, BaseVertexIndex, MinVertexIndex, NumVertices,
        StartIndex, PrimitiveCount);

    nine_update_state(This);

    init_draw_info(&info, This, PrimitiveType, PrimitiveCount);
    info.indexed = TRUE;
    info.start = StartIndex;
    info.index_bias = BaseVertexIndex;
    info.min_index = BaseVertexIndex + MinVertexIndex;
    info.max_index = BaseVertexIndex + NumVertices;

    This->pipe->draw_vbo(This->pipe, &info);

    return D3D_OK;
}

HRESULT WINAPI
NineDevice9_DrawPrimitiveUP( struct NineDevice9 *This,
                             D3DPRIMITIVETYPE PrimitiveType,
                             UINT PrimitiveCount,
                             const void *pVertexStreamZeroData,
                             UINT VertexStreamZeroStride )
{
    struct pipe_vertex_buffer vtxbuf;
    struct pipe_draw_info info;

    DBG("iface %p, PrimitiveType %u, PrimitiveCount %u, data %p, stride %u\n",
        This, PrimitiveType, PrimitiveCount,
        pVertexStreamZeroData, VertexStreamZeroStride);

    nine_update_state(This);

    init_draw_info(&info, This, PrimitiveType, PrimitiveCount);
    info.indexed = FALSE;
    info.start = 0;
    info.index_bias = 0;
    info.min_index = 0;
    info.max_index = info.count - 1;

    /* TODO: stop hating drivers that don't support user buffers */
    vtxbuf.stride = VertexStreamZeroStride;
    vtxbuf.buffer_offset = 0;
    vtxbuf.buffer = NULL;
    vtxbuf.user_buffer = pVertexStreamZeroData;

    This->pipe->set_vertex_buffers(This->pipe, 0, 1, &vtxbuf);
    This->state.changed.vtxbuf |= 1;

    This->pipe->draw_vbo(This->pipe, &info);

    return D3D_OK;
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
    struct pipe_draw_info info;
    struct pipe_vertex_buffer vbuf;
    struct pipe_index_buffer ibuf;

    DBG("iface %p, PrimitiveType %u, MinVertexIndex %u, NumVertices %u "
        "PrimitiveCount %u, pIndexData %p, IndexDataFormat %u "
        "pVertexStreamZeroData %p, VertexStreamZeroStride %u\n",
        This, PrimitiveType, MinVertexIndex, NumVertices, PrimitiveCount,
        pIndexData, IndexDataFormat,
        pVertexStreamZeroData, VertexStreamZeroStride);

    nine_update_state(This);

    init_draw_info(&info, This, PrimitiveType, PrimitiveCount);
    info.indexed = TRUE;
    info.start = 0;
    info.index_bias = 0;
    info.min_index = MinVertexIndex;
    info.max_index = MinVertexIndex + NumVertices - 1;

    vbuf.stride = VertexStreamZeroStride;
    vbuf.buffer_offset = 0;
    vbuf.buffer = NULL;
    vbuf.user_buffer = pVertexStreamZeroData;

    ibuf.index_size = (IndexDataFormat == D3DFMT_INDEX16) ? 2 : 4;
    ibuf.offset = 0;
    ibuf.buffer = NULL;
    ibuf.user_buffer = pIndexData;

    This->pipe->set_vertex_buffers(This->pipe, 0, 1, &vbuf);
    This->pipe->set_index_buffer(This->pipe, &ibuf);
    This->state.changed.vtxbuf |= 1;
    This->state.changed.group |= NINE_STATE_IDXBUF;

    This->pipe->draw_vbo(This->pipe, &info);

    return D3D_OK;
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
    struct NineVertexDeclaration9 *vdecl;

    HRESULT hr = NineVertexDeclaration9_new(This, pVertexElements, &vdecl);
    if (SUCCEEDED(hr))
        *ppDecl = (IDirect3DVertexDeclaration9 *)vdecl;

    return hr;
}

HRESULT WINAPI
NineDevice9_SetVertexDeclaration( struct NineDevice9 *This,
                                  IDirect3DVertexDeclaration9 *pDecl )
{
    NINESTATEPOINTER_SET(This);
    nine_reference(&state->vdecl, pDecl);
    state->changed.group |= NINE_STATE_VDECL;
    /* XXX: should this really change the result of GetFVF ? */
    return D3D_OK;
}

HRESULT WINAPI
NineDevice9_GetVertexDeclaration( struct NineDevice9 *This,
                                  IDirect3DVertexDeclaration9 **ppDecl )
{
    NINESTATEPOINTER_GET(This);
    nine_reference(ppDecl, state->vdecl);
    return D3D_OK;
}

HRESULT WINAPI
NineDevice9_SetFVF( struct NineDevice9 *This,
                    DWORD FVF )
{
    NINESTATEPOINTER_SET(This);
    struct NineVertexDeclaration9 *vdecl;
    HRESULT hr;

    if (!FVF) {
        /* XXX: is this correct ? */
        if (state->vdecl && state->vdecl->fvf)
            nine_reference(&state->vdecl, NULL);
        return D3D_OK;
    }

    /* TODO: cache FVF vdecls */
    hr = NineVertexDeclaration9_new_from_fvf(This, FVF, &vdecl);
    if (FAILED(hr))
        return hr;
    vdecl->fvf = FVF;

    nine_reference(&state->vdecl, NULL);
    state->vdecl = vdecl; /* don't increase refcount */
    state->changed.group |= NINE_STATE_VDECL;

    return D3D_OK;
}

HRESULT WINAPI
NineDevice9_GetFVF( struct NineDevice9 *This,
                    DWORD *pFVF )
{
    NINESTATEPOINTER_GET(This);
    *pFVF = state->vdecl ? state->vdecl->fvf : 0;
    return D3D_OK;
}

HRESULT WINAPI
NineDevice9_CreateVertexShader( struct NineDevice9 *This,
                                const DWORD *pFunction,
                                IDirect3DVertexShader9 **ppShader )
{
    struct NineVertexShader9 *vs;
    HRESULT hr;

    hr = NineVertexShader9_new(This, &vs, pFunction, NULL);
    if (FAILED(hr))
        return hr;
    *ppShader = (IDirect3DVertexShader9 *)vs;
    return D3D_OK;
}

HRESULT WINAPI
NineDevice9_SetVertexShader( struct NineDevice9 *This,
                             IDirect3DVertexShader9 *pShader )
{
    NINESTATEPOINTER_SET(This);
    nine_reference(&state->vs, pShader);
    state->changed.group |= NINE_STATE_VS;
    return D3D_OK;
}

HRESULT WINAPI
NineDevice9_GetVertexShader( struct NineDevice9 *This,
                             IDirect3DVertexShader9 **ppShader )
{
    NINESTATEPOINTER_GET(This);
    nine_reference(ppShader, state->vs);
    return D3D_OK;
}

HRESULT WINAPI
NineDevice9_SetVertexShaderConstantF( struct NineDevice9 *This,
                                      UINT StartRegister,
                                      const float *pConstantData,
                                      UINT Vector4fCount )
{
    NINESTATEPOINTER_SET(This);
    uint32_t mask;
    unsigned i;
    unsigned c;

    user_assert(StartRegister                  < This->caps.MaxVertexShaderConst, D3DERR_INVALIDCALL);
    user_assert(StartRegister + Vector4fCount <= This->caps.MaxVertexShaderConst, D3DERR_INVALIDCALL);

    if (!Vector4fCount)
       return D3D_OK;
    user_assert(pConstantData, D3DERR_INVALIDCALL);

    memcpy(&state->vs_const_f[StartRegister * 4],
           pConstantData,
           Vector4fCount * 4 * sizeof(state->vs_const_f[0]));

    /* set dirty bitmask */
    i = StartRegister / 32;
    c = MIN2(Vector4fCount, 32 - (StartRegister % 32));
    mask = 0xFFFFFFFF;
    if (Vector4fCount < 32)
       mask >>= 32 - Vector4fCount;

    state->changed.vs_const_f[i] |= mask << (StartRegister % 32);
    for (++i; i < (StartRegister + Vector4fCount) / 32; ++i)
       state->changed.vs_const_f[i] = 0xFFFFFFFF;
    c = (Vector4fCount - c) - (i - 1) * 32;
    if (c)
       state->changed.vs_const_f[i] |= (1 << c) - 1;

    state->changed.group |= NINE_STATE_VS_CONST;

    return D3D_OK;
}

HRESULT WINAPI
NineDevice9_GetVertexShaderConstantF( struct NineDevice9 *This,
                                      UINT StartRegister,
                                      float *pConstantData,
                                      UINT Vector4fCount )
{
    NINESTATEPOINTER_GET(This);
    user_assert(StartRegister                  < This->caps.MaxVertexShaderConst, D3DERR_INVALIDCALL);
    user_assert(StartRegister + Vector4fCount <= This->caps.MaxVertexShaderConst, D3DERR_INVALIDCALL);
    user_assert(pConstantData, D3DERR_INVALIDCALL);

    memcpy(pConstantData,
           &state->vs_const_f[StartRegister * 4],
           Vector4fCount * 4 * sizeof(state->vs_const_f[0]));

    return D3D_OK;
}

HRESULT WINAPI
NineDevice9_SetVertexShaderConstantI( struct NineDevice9 *This,
                                      UINT StartRegister,
                                      const int *pConstantData,
                                      UINT Vector4iCount )
{
    NINESTATEPOINTER_SET(This);
    user_assert(StartRegister                  < NINE_MAX_CONST_I, D3DERR_INVALIDCALL);
    user_assert(StartRegister + Vector4iCount <= NINE_MAX_CONST_I, D3DERR_INVALIDCALL);
    user_assert(pConstantData, D3DERR_INVALIDCALL);

    memcpy(&state->vs_const_i[StartRegister][0],
           pConstantData,
           Vector4iCount * sizeof(state->vs_const_i[0]));

    state->changed.vs_const_i |= ((1 << Vector4iCount) - 1) << StartRegister;

    return D3D_OK;
}

HRESULT WINAPI
NineDevice9_GetVertexShaderConstantI( struct NineDevice9 *This,
                                      UINT StartRegister,
                                      int *pConstantData,
                                      UINT Vector4iCount )
{
    NINESTATEPOINTER_GET(This);
    user_assert(StartRegister                  < NINE_MAX_CONST_I, D3DERR_INVALIDCALL);
    user_assert(StartRegister + Vector4iCount <= NINE_MAX_CONST_I, D3DERR_INVALIDCALL);
    user_assert(pConstantData, D3DERR_INVALIDCALL);

    memcpy(pConstantData,
           &state->vs_const_i[StartRegister][0],
           Vector4iCount * sizeof(state->vs_const_i[0]));

    return D3D_OK;
}

HRESULT WINAPI
NineDevice9_SetVertexShaderConstantB( struct NineDevice9 *This,
                                      UINT StartRegister,
                                      const BOOL *pConstantData,
                                      UINT BoolCount )
{
    NINESTATEPOINTER_SET(This);
    user_assert(StartRegister              < NINE_MAX_CONST_B, D3DERR_INVALIDCALL);
    user_assert(StartRegister + BoolCount <= NINE_MAX_CONST_B, D3DERR_INVALIDCALL);
    user_assert(pConstantData, D3DERR_INVALIDCALL);

    memcpy(&state->vs_const_b[StartRegister],
           pConstantData,
           BoolCount * sizeof(state->vs_const_b[0]));

    state->changed.vs_const_b |= ((1 << BoolCount) - 1) << StartRegister;

    return D3D_OK;
}

HRESULT WINAPI
NineDevice9_GetVertexShaderConstantB( struct NineDevice9 *This,
                                      UINT StartRegister,
                                      BOOL *pConstantData,
                                      UINT BoolCount )
{
    NINESTATEPOINTER_GET(This);
    user_assert(StartRegister              < NINE_MAX_CONST_B, D3DERR_INVALIDCALL);
    user_assert(StartRegister + BoolCount <= NINE_MAX_CONST_B, D3DERR_INVALIDCALL);
    user_assert(pConstantData, D3DERR_INVALIDCALL);

    memcpy(pConstantData,
           &state->vs_const_b[StartRegister],
           BoolCount * sizeof(state->vs_const_b[0]));

    return D3D_OK;
}

HRESULT WINAPI
NineDevice9_SetStreamSource( struct NineDevice9 *This,
                             UINT StreamNumber,
                             IDirect3DVertexBuffer9 *pStreamData,
                             UINT OffsetInBytes,
                             UINT Stride )
{
    NINESTATEPOINTER_SET(This);
    struct NineVertexBuffer9 *pVBuf9 = NineVertexBuffer9(pStreamData);
    const unsigned i = StreamNumber;

    user_assert(StreamNumber < This->caps.MaxStreams, D3DERR_INVALIDCALL);
    user_assert(Stride <= This->caps.MaxStreamStride, D3DERR_INVALIDCALL);

    state->vtxbuf[i].stride = Stride;
    state->vtxbuf[i].buffer_offset = OffsetInBytes;
    state->vtxbuf[i].buffer = pStreamData ? pVBuf9->base.resource : NULL;

    nine_reference(&state->stream[i], pStreamData);

    state->changed.vtxbuf |= 1 << StreamNumber;
    return D3D_OK;
}

HRESULT WINAPI
NineDevice9_GetStreamSource( struct NineDevice9 *This,
                             UINT StreamNumber,
                             IDirect3DVertexBuffer9 **ppStreamData,
                             UINT *pOffsetInBytes,
                             UINT *pStride )
{
    NINESTATEPOINTER_GET(This);
    const unsigned i = StreamNumber;

    user_assert(StreamNumber < This->caps.MaxStreams, D3DERR_INVALIDCALL);
    user_assert(ppStreamData, D3DERR_INVALIDCALL);

    nine_reference(ppStreamData, state->stream[i]);
    *pStride = state->vtxbuf[i].stride;
    *pOffsetInBytes = state->vtxbuf[i].buffer_offset;

    return D3D_OK;
}

HRESULT WINAPI
NineDevice9_SetStreamSourceFreq( struct NineDevice9 *This,
                                 UINT StreamNumber,
                                 UINT Setting )
{
    NINESTATEPOINTER_SET(This);
    /* const UINT freq = Setting & 0x7FFFFF; */

    user_assert(StreamNumber < This->caps.MaxStreams, D3DERR_INVALIDCALL);
    user_assert(StreamNumber != 0 || (Setting & D3DSTREAMSOURCE_INDEXEDDATA),
                D3DERR_INVALIDCALL);
    user_assert(!(Setting & D3DSTREAMSOURCE_INSTANCEDATA) ^
                !(Setting & D3DSTREAMSOURCE_INDEXEDDATA), D3DERR_INVALIDCALL);

    state->stream_freq[StreamNumber] = Setting;

    if (Setting & D3DSTREAMSOURCE_INSTANCEDATA)
        state->stream_instancedata_mask |= 1 << StreamNumber;
    else
        state->stream_instancedata_mask &= ~(1 << StreamNumber);

    state->changed.stream_freq |= 1 << StreamNumber;
    return D3D_OK;
}

HRESULT WINAPI
NineDevice9_GetStreamSourceFreq( struct NineDevice9 *This,
                                 UINT StreamNumber,
                                 UINT *pSetting )
{
    NINESTATEPOINTER_GET(This);
    user_assert(StreamNumber < This->caps.MaxStreams, D3DERR_INVALIDCALL);

    *pSetting = state->stream_freq[StreamNumber];

    return D3D_OK;
}

HRESULT WINAPI
NineDevice9_SetIndices( struct NineDevice9 *This,
                        IDirect3DIndexBuffer9 *pIndexData )
{
    NINESTATEPOINTER_SET(This);

    nine_reference(&state->idxbuf, pIndexData);

    state->changed.group |= NINE_STATE_IDXBUF;
    return D3D_OK;
}

/* XXX: wine/d3d9 doesn't have pBaseVertexIndex, and it doesn't make sense
 * here because it's an argument passed to the Draw calls.
 */
HRESULT WINAPI
NineDevice9_GetIndices( struct NineDevice9 *This,
                        IDirect3DIndexBuffer9 **ppIndexData /*,
                        UINT *pBaseVertexIndex */ )
{
    NINESTATEPOINTER_GET(This);
    user_assert(ppIndexData, D3DERR_INVALIDCALL);

    nine_reference(ppIndexData, state->idxbuf);
    return D3D_OK;
}

HRESULT WINAPI
NineDevice9_CreatePixelShader( struct NineDevice9 *This,
                               const DWORD *pFunction,
                               IDirect3DPixelShader9 **ppShader )
{
    struct NinePixelShader9 *ps;
    HRESULT hr;

    hr = NinePixelShader9_new(This, &ps, pFunction, NULL);
    if (FAILED(hr))
        return hr;
    *ppShader = (IDirect3DPixelShader9 *)ps;
    return D3D_OK;
}

HRESULT WINAPI
NineDevice9_SetPixelShader( struct NineDevice9 *This,
                            IDirect3DPixelShader9 *pShader )
{
    NINESTATEPOINTER_SET(This);
    nine_reference(&state->vs, pShader);
    state->changed.group |= NINE_STATE_PS;
    return D3D_OK;
}

HRESULT WINAPI
NineDevice9_GetPixelShader( struct NineDevice9 *This,
                            IDirect3DPixelShader9 **ppShader )
{
    NINESTATEPOINTER_GET(This);
    nine_reference(ppShader, state->ps);
    return D3D_OK;
}

HRESULT WINAPI
NineDevice9_SetPixelShaderConstantF( struct NineDevice9 *This,
                                     UINT StartRegister,
                                     const float *pConstantData,
                                     UINT Vector4fCount )
{
    NINESTATEPOINTER_SET(This);
    uint32_t mask;
    unsigned i;
    unsigned c;

    user_assert(StartRegister                  < NINE_MAX_CONST_F, D3DERR_INVALIDCALL);
    user_assert(StartRegister + Vector4fCount <= NINE_MAX_CONST_F, D3DERR_INVALIDCALL);

    if (!Vector4fCount)
       return D3D_OK;
    user_assert(pConstantData, D3DERR_INVALIDCALL);

    memcpy(&state->ps_const_f[StartRegister * 4],
           pConstantData,
           Vector4fCount * 4 * sizeof(state->ps_const_f[0]));

    /* set dirty bitmask */
    i = StartRegister / 32;
    c = MIN2(Vector4fCount, 32 - (StartRegister % 32));
    mask = 0xFFFFFFFF;
    if (Vector4fCount < 32)
       mask >>= 32 - Vector4fCount;

    state->changed.ps_const_f[i] |= mask << (StartRegister % 32);
    for (++i; i < (StartRegister + Vector4fCount) / 32; ++i)
       state->changed.ps_const_f[i] = 0xFFFFFFFF;
    c = (Vector4fCount - c) - (i - 1) * 32;
    if (c)
       state->changed.ps_const_f[i] |= (1 << c) - 1;

    state->changed.group |= NINE_STATE_PS_CONST;

    return D3D_OK;
}

HRESULT WINAPI
NineDevice9_GetPixelShaderConstantF( struct NineDevice9 *This,
                                     UINT StartRegister,
                                     float *pConstantData,
                                     UINT Vector4fCount )
{
    NINESTATEPOINTER_GET(This);
    user_assert(StartRegister                  < NINE_MAX_CONST_F, D3DERR_INVALIDCALL);
    user_assert(StartRegister + Vector4fCount <= NINE_MAX_CONST_F, D3DERR_INVALIDCALL);
    user_assert(pConstantData, D3DERR_INVALIDCALL);

    memcpy(pConstantData,
           &state->ps_const_f[StartRegister * 4],
           Vector4fCount * 4 * sizeof(state->ps_const_f[0]));

    return D3D_OK;
}

HRESULT WINAPI
NineDevice9_SetPixelShaderConstantI( struct NineDevice9 *This,
                                     UINT StartRegister,
                                     const int *pConstantData,
                                     UINT Vector4iCount )
{
    NINESTATEPOINTER_SET(This);
    user_assert(StartRegister                  < NINE_MAX_CONST_I, D3DERR_INVALIDCALL);
    user_assert(StartRegister + Vector4iCount <= NINE_MAX_CONST_I, D3DERR_INVALIDCALL);
    user_assert(pConstantData, D3DERR_INVALIDCALL);

    memcpy(&state->ps_const_i[StartRegister][0],
           pConstantData,
           Vector4iCount * sizeof(state->ps_const_i[0]));

    state->changed.ps_const_i |= ((1 << Vector4iCount) - 1) << StartRegister;

    return D3D_OK;
}

HRESULT WINAPI
NineDevice9_GetPixelShaderConstantI( struct NineDevice9 *This,
                                     UINT StartRegister,
                                     int *pConstantData,
                                     UINT Vector4iCount )
{
    NINESTATEPOINTER_GET(This);
    user_assert(StartRegister                  < NINE_MAX_CONST_I, D3DERR_INVALIDCALL);
    user_assert(StartRegister + Vector4iCount <= NINE_MAX_CONST_I, D3DERR_INVALIDCALL);
    user_assert(pConstantData, D3DERR_INVALIDCALL);

    memcpy(pConstantData,
           &state->ps_const_i[StartRegister][0],
           Vector4iCount * sizeof(state->ps_const_i[0]));

    return D3D_OK;
}

HRESULT WINAPI
NineDevice9_SetPixelShaderConstantB( struct NineDevice9 *This,
                                     UINT StartRegister,
                                     const BOOL *pConstantData,
                                     UINT BoolCount )
{
    NINESTATEPOINTER_SET(This);
    user_assert(StartRegister              < NINE_MAX_CONST_B, D3DERR_INVALIDCALL);
    user_assert(StartRegister + BoolCount <= NINE_MAX_CONST_B, D3DERR_INVALIDCALL);
    user_assert(pConstantData, D3DERR_INVALIDCALL);

    memcpy(&state->ps_const_b[StartRegister],
           pConstantData,
           BoolCount * sizeof(state->ps_const_b[0]));

    state->changed.ps_const_b |= ((1 << BoolCount) - 1) << StartRegister;

    return D3D_OK;
}

HRESULT WINAPI
NineDevice9_GetPixelShaderConstantB( struct NineDevice9 *This,
                                     UINT StartRegister,
                                     BOOL *pConstantData,
                                     UINT BoolCount )
{
    NINESTATEPOINTER_GET(This);
    user_assert(StartRegister              < NINE_MAX_CONST_B, D3DERR_INVALIDCALL);
    user_assert(StartRegister + BoolCount <= NINE_MAX_CONST_B, D3DERR_INVALIDCALL);
    user_assert(pConstantData, D3DERR_INVALIDCALL);

    memcpy(pConstantData,
           &state->ps_const_b[StartRegister],
           BoolCount * sizeof(state->ps_const_b[0]));

    return D3D_OK;
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
    struct NineQuery9 *query;
    HRESULT hr;

    hr = NineQuery9_new(This, &query, Type);
    if (FAILED(hr))
        return hr;
    *ppQuery = (IDirect3DQuery9 *)query;
    return D3D_OK;
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
