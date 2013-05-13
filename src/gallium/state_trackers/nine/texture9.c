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
#include "texture9.h"
#include "nine_helpers.h"
#include "nine_pipe.h"

#include "pipe/p_state.h"
#include "pipe/p_context.h"
#include "pipe/p_screen.h"
#include "util/u_inlines.h"
#include "util/u_resource.h"

#define DBG_CHANNEL DBG_TEXTURE

static void
NineTexture9_UpdateSamplerView( struct NineTexture9 *This )
{
    struct pipe_context *pipe = This->base.pipe;
    struct pipe_resource *resource = This->base.base.resource;
    struct pipe_sampler_view templ;

    assert(resource);

    pipe_sampler_view_reference(&This->base.view, NULL);

    templ.format = resource->format;
    templ.u.tex.first_layer = 0;
    templ.u.tex.last_layer = 0;
    templ.u.tex.first_level = 0;
    templ.u.tex.last_level = resource->last_level;
    templ.swizzle_r = PIPE_SWIZZLE_RED;
    templ.swizzle_g = PIPE_SWIZZLE_GREEN;
    templ.swizzle_b = PIPE_SWIZZLE_BLUE;
    templ.swizzle_a = PIPE_SWIZZLE_ALPHA;

    This->base.view = pipe->create_sampler_view(pipe, resource, &templ);
}

static HRESULT
NineTexture9_ctor( struct NineTexture9 *This,
                   struct NineUnknownParams *pParams,
                   struct NineDevice9 *pDevice,
                   UINT Width, UINT Height, UINT Levels,
                   DWORD Usage,
                   D3DFORMAT Format,
                   D3DPOOL Pool,
                   HANDLE *pSharedHandle )
{
    struct pipe_screen *screen = pDevice->screen;
    struct pipe_resource *resource = NULL;
    HRESULT hr;
    D3DSURFACE_DESC sfdesc;
    unsigned l;
    struct pipe_resource templ;

    user_assert(!(Usage & (D3DUSAGE_RENDERTARGET | D3DUSAGE_DEPTHSTENCIL)) ||
                Pool == D3DPOOL_DEFAULT, D3DERR_INVALIDCALL);

    assert(!pSharedHandle); /* TODO */

    templ.target = PIPE_TEXTURE_2D;
    templ.format = d3d9_to_pipe_format(Format);
    templ.width0 = Width;
    templ.height0 = Height;
    templ.depth0 = 1;
    if (Levels)
        templ.last_level = Levels - 1;
    else
        templ.last_level = util_logbase2(MAX2(Width, Height));
    templ.array_size = 1;
    templ.nr_samples = 0;
    templ.flags = 0;

    This->surfaces = CALLOC(templ.last_level + 1, sizeof(*This->surfaces));
    if (!This->surfaces)
        return E_OUTOFMEMORY;

    if (Pool != D3DPOOL_SYSTEMMEM && Pool != D3DPOOL_SCRATCH) {
        /* create device-accessible texture */
        unsigned bind =
            PIPE_BIND_SAMPLER_VIEW |
            PIPE_BIND_TRANSFER_WRITE |
            PIPE_BIND_TRANSFER_READ;
        unsigned usage = PIPE_USAGE_STATIC;

        if (Usage & D3DUSAGE_RENDERTARGET)
            bind |= PIPE_BIND_RENDER_TARGET;
        if (Usage & D3DUSAGE_DEPTHSTENCIL)
            bind |= PIPE_BIND_DEPTH_STENCIL;

        if (Usage & D3DUSAGE_DYNAMIC)
            usage = PIPE_USAGE_DYNAMIC;

        templ.usage = usage;
        templ.bind = bind;

        if (!screen->is_format_supported(screen, templ.format, PIPE_TEXTURE_2D,
                                         0, bind))
            return D3DERR_WRONGTEXTUREFORMAT;

        resource = screen->resource_create(screen, &templ);
        if (!resource)
            return D3DERR_OUTOFVIDEOMEMORY;
        /* NOTE: Don't return without unreferencing the resource ! */
    }
    This->base.format = Format;
    This->base.width = Width;
    This->base.height = Height;
    This->base.layers = 1;
    This->base.last_level = templ.last_level;
    This->base.base.usage = Usage;

    hr = NineBaseTexture9_ctor(&This->base, pParams, pDevice,
                               resource,
                               D3DRTYPE_TEXTURE, Pool);
    /* BaseTexture9::Resource9 will hold the reference from here on. */
    pipe_resource_reference(&resource, NULL);
    if (FAILED(hr))
        return hr;

    /* Create all the surfaces right away, they manage backing storage
     * and transfers (LockRect) are deferred to them.
     */
    sfdesc.Format = Format;
    sfdesc.Type = D3DRTYPE_TEXTURE;
    sfdesc.Usage = Usage;
    sfdesc.Pool = Pool;
    sfdesc.MultiSampleType = D3DMULTISAMPLE_NONE;
    sfdesc.MultiSampleQuality = 0;
    for (l = 0; l <= This->base.last_level; ++l) {
        sfdesc.Width = u_minify(Width, l);
        sfdesc.Height = u_minify(Height, l);

        hr = NineSurface9_new(pDevice, NineUnknown(This), resource, l, 0,
                              &sfdesc, &This->surfaces[l]);
        if (FAILED(hr))
            return hr;
    }

    NineTexture9_UpdateSamplerView(This);

    return D3D_OK;
}

static void
NineTexture9_dtor( struct NineTexture9 *This )
{
    unsigned l;

    if (This->surfaces) {
        for (l = 0; l <= This->base.last_level; ++l)
            if (This->surfaces[l])
                NineUnknown_Release(NineUnknown(This->surfaces[l]));
        FREE(This->surfaces);
    }

    NineBaseTexture9_dtor(&This->base);
}

HRESULT WINAPI
NineTexture9_GetLevelDesc( struct NineTexture9 *This,
                           UINT Level,
                           D3DSURFACE_DESC *pDesc )
{
    user_assert(Level <= This->base.last_level, D3DERR_INVALIDCALL);

    *pDesc = This->surfaces[Level]->desc;

    return D3D_OK;
}

HRESULT WINAPI
NineTexture9_GetSurfaceLevel( struct NineTexture9 *This,
                              UINT Level,
                              IDirect3DSurface9 **ppSurfaceLevel )
{
    user_assert(Level <= This->base.last_level, D3DERR_INVALIDCALL);
    user_assert(Level == 0 || !(This->base.base.usage & D3DUSAGE_AUTOGENMIPMAP),
                D3DERR_INVALIDCALL);

    NineUnknown_AddRef(NineUnknown(This->surfaces[Level]));
    *ppSurfaceLevel = (IDirect3DSurface9 *)This->surfaces[Level];

    return D3D_OK;
}

HRESULT WINAPI
NineTexture9_LockRect( struct NineTexture9 *This,
                       UINT Level,
                       D3DLOCKED_RECT *pLockedRect,
                       const RECT *pRect,
                       DWORD Flags )
{
    user_assert(Level <= This->base.last_level, D3DERR_INVALIDCALL);
    user_assert(Level == 0 || !(This->base.base.usage & D3DUSAGE_AUTOGENMIPMAP),
                D3DERR_INVALIDCALL);

    return NineSurface9_LockRect(This->surfaces[Level], pLockedRect,
                                 pRect, Flags);

#if 0
    struct pipe_context *pipe = This->base.pipe;
    struct pipe_resource *resource = This->base.base.resource;
    struct pipe_box box;
    unsigned usage;

    NineBaseTexture9_GetPipeBox2D(&This->base, &box, Level, 0, pRect);

    usage = (Flags & D3DLOCK_READONLY) ?
        PIPE_TRANSFER_READ : PIPE_TRANSFER_WRITE;
    usage |= (Flags & D3DLOCK_DISCARD) ?
        PIPE_TRANSFER_DISCARD_RANGE : PIPE_TRANSFER_READ;

    if (This->base.base.pool == D3DPOOL_DEFAULT) {
        pLockedRect->pBits =
            pipe->transfer_map(pipe, resource, Level, usage, &box,
                               &This->base.transfer[Level]);
        pLockedRect->Pitch = This->base.transfer[Level]->stride;
    } else {
        /* we should probably remember these ... */
        unsigned offset = 0;
        unsigned stride = 0;
        pLockedRect->pBits = This->base.base.sys.data + offset;
        pLockedRect->Pitch = stride;

        if (!(Flags & D3DLOCK_NO_DIRTY_UPDATE))
            NineBaseTexture9_AddDirtyRegion(&This->base, &box);
    }
    return D3D_OK;
#endif
}

HRESULT WINAPI
NineTexture9_UnlockRect( struct NineTexture9 *This,
                         UINT Level )
{
    user_assert(Level <= This->base.last_level, D3DERR_INVALIDCALL);

    return NineSurface9_UnlockRect(This->surfaces[Level]);
}

HRESULT WINAPI
NineTexture9_AddDirtyRect( struct NineTexture9 *This,
                           const RECT *pDirtyRect )
{
    struct NineDirtyRect *entry;
    struct u_rect rect;
    rect_to_g3d_u_rect(&rect, pDirtyRect);

    entry = NineSurface9_AddDirtyRect(This->surfaces[0], &rect, TRUE);
    if (!entry)
        return E_OUTOFMEMORY;
    return D3D_OK;
}

IDirect3DTexture9Vtbl NineTexture9_vtable = {
    (void *)NineUnknown_QueryInterface,
    (void *)NineUnknown_AddRef,
    (void *)NineUnknown_Release,
    (void *)NineResource9_GetDevice,
    (void *)NineResource9_SetPrivateData,
    (void *)NineResource9_GetPrivateData,
    (void *)NineResource9_FreePrivateData,
    (void *)NineResource9_SetPriority,
    (void *)NineResource9_GetPriority,
    (void *)NineResource9_PreLoad,
    (void *)NineResource9_GetType,
    (void *)NineBaseTexture9_SetLOD,
    (void *)NineBaseTexture9_GetLOD,
    (void *)NineBaseTexture9_GetLevelCount,
    (void *)NineBaseTexture9_SetAutoGenFilterType,
    (void *)NineBaseTexture9_GetAutoGenFilterType,
    (void *)NineBaseTexture9_GenerateMipSubLevels,
    (void *)NineTexture9_GetLevelDesc,
    (void *)NineTexture9_GetSurfaceLevel,
    (void *)NineTexture9_LockRect,
    (void *)NineTexture9_UnlockRect,
    (void *)NineTexture9_AddDirtyRect
};

static const GUID *NineTexture9_IIDs[] = {
    &IID_IDirect3DTexture9,
    &IID_IDirect3DBaseTexture9,
    &IID_IDirect3DResource9,
    &IID_IUnknown,
    NULL
};

HRESULT
NineTexture9_new( struct NineDevice9 *pDevice,
                  UINT Width, UINT Height, UINT Levels,
                  DWORD Usage,
                  D3DFORMAT Format,
                  D3DPOOL Pool,
                  struct NineTexture9 **ppOut,
                  HANDLE *pSharedHandle )
{
    NINE_NEW(NineTexture9, ppOut, pDevice,
             Width, Height, Levels,
             Usage, Format, Pool, pSharedHandle);
}
