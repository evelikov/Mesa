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
    struct pipe_resource *info = &This->base.base.info;
    unsigned l;
    D3DSURFACE_DESC sfdesc;
    HRESULT hr;

    DBG("(%p) Width=%u Height=%u Levels=%u Usage=%x Format=%s Pool=%u\n",
        This,
        Width, Height, Levels, Usage, d3dformat_to_string(Format), Pool);

    user_assert(!(Usage & (D3DUSAGE_RENDERTARGET | D3DUSAGE_DEPTHSTENCIL)) ||
                Pool == D3DPOOL_DEFAULT, D3DERR_INVALIDCALL);
    user_assert(!(Usage & D3DUSAGE_AUTOGENMIPMAP) ||
                Pool != D3DPOOL_SYSTEMMEM, D3DERR_INVALIDCALL);

    assert(!pSharedHandle); /* TODO */

    This->base.format = Format;

    info->screen = pDevice->screen;
    info->target = PIPE_TEXTURE_2D;
    info->format = d3d9_to_pipe_format(Format);
    info->width0 = Width;
    info->height0 = Height;
    info->depth0 = 1;
    if (Levels)
        info->last_level = Levels - 1;
    else
        info->last_level = util_logbase2(MAX2(Width, Height));
    info->array_size = 1;
    info->nr_samples = 0;
    info->bind = PIPE_BIND_SAMPLER_VIEW;
    info->usage = PIPE_USAGE_DEFAULT;
    info->flags = 0;

    if (Usage & D3DUSAGE_RENDERTARGET)
        info->bind |= PIPE_BIND_RENDER_TARGET;
    if (Usage & D3DUSAGE_DEPTHSTENCIL)
        info->bind |= PIPE_BIND_DEPTH_STENCIL;

    if (Usage & D3DUSAGE_DYNAMIC) {
        info->usage = PIPE_USAGE_DYNAMIC;
        info->bind |=
            PIPE_BIND_TRANSFER_READ |
            PIPE_BIND_TRANSFER_WRITE;
    }

    if (Pool == D3DPOOL_SYSTEMMEM)
        info->usage = PIPE_USAGE_STAGING;

    This->surfaces = CALLOC(info->last_level + 1, sizeof(*This->surfaces));
    if (!This->surfaces)
        return E_OUTOFMEMORY;

    hr = NineBaseTexture9_ctor(&This->base, pParams, pDevice,
                               D3DRTYPE_TEXTURE, Pool);
    if (FAILED(hr))
        return hr;

    /* Create all the surfaces right away.
     * They manage backing storage, and transfers (LockRect) are deferred
     * to them.
     */
    sfdesc.Format = Format;
    sfdesc.Type = D3DRTYPE_TEXTURE;
    sfdesc.Usage = Usage;
    sfdesc.Pool = Pool;
    sfdesc.MultiSampleType = D3DMULTISAMPLE_NONE;
    sfdesc.MultiSampleQuality = 0;
    for (l = 0; l <= info->last_level; ++l) {
        sfdesc.Width = u_minify(Width, l);
        sfdesc.Height = u_minify(Height, l);

        hr = NineSurface9_new(pDevice, NineUnknown(This),
                              This->base.base.resource, l, 0,
                              &sfdesc, &This->surfaces[l]);
        if (FAILED(hr))
            return hr;
    }

    return D3D_OK;
}

static void
NineTexture9_dtor( struct NineTexture9 *This )
{
    unsigned l;

    if (This->surfaces) {
        for (l = 0; l <= This->base.base.info.last_level; ++l)
            nine_reference(&This->surfaces[l], NULL);
        FREE(This->surfaces);
    }

    NineBaseTexture9_dtor(&This->base);
}

HRESULT WINAPI
NineTexture9_GetLevelDesc( struct NineTexture9 *This,
                           UINT Level,
                           D3DSURFACE_DESC *pDesc )
{
    user_assert(Level <= This->base.base.info.last_level, D3DERR_INVALIDCALL);

    *pDesc = This->surfaces[Level]->desc;

    return D3D_OK;
}

HRESULT WINAPI
NineTexture9_GetSurfaceLevel( struct NineTexture9 *This,
                              UINT Level,
                              IDirect3DSurface9 **ppSurfaceLevel )
{
    user_assert(Level <= This->base.base.info.last_level, D3DERR_INVALIDCALL);
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
    user_assert(Level <= This->base.base.info.last_level, D3DERR_INVALIDCALL);
    user_assert(Level == 0 || !(This->base.base.usage & D3DUSAGE_AUTOGENMIPMAP),
                D3DERR_INVALIDCALL);

    return NineSurface9_LockRect(This->surfaces[Level], pLockedRect,
                                 pRect, Flags);
}

HRESULT WINAPI
NineTexture9_UnlockRect( struct NineTexture9 *This,
                         UINT Level )
{
    user_assert(Level <= This->base.base.info.last_level, D3DERR_INVALIDCALL);

    return NineSurface9_UnlockRect(This->surfaces[Level]);
}

HRESULT WINAPI
NineTexture9_AddDirtyRect( struct NineTexture9 *This,
                           const RECT *pDirtyRect )
{
    DBG("This=%p pDirtyRect=%p[(%u,%u)-(%u,%u)]\n", This, pDirtyRect,
        pDirtyRect ? pDirtyRect->left : 0, pDirtyRect ? pDirtyRect->top : 0,
        pDirtyRect ? pDirtyRect->right : 0, pDirtyRect ? pDirtyRect->bottom : 0);

    /* Tracking dirty regions on DEFAULT or SYSTEMMEM resources is pointless,
     * because we always write to the final storage.
     */
    if (This->base.base.pool != D3DPOOL_MANAGED)
        return D3D_OK;

    if (!pDirtyRect) {
        NineSurface9_AddDirtyRect(This->surfaces[0], NULL);
    } else {
        struct pipe_box box;
        rect_to_pipe_box(&box, pDirtyRect);    
        NineSurface9_AddDirtyRect(This->surfaces[0], &box);
    }
    return D3D_OK;
}

static void WINAPI
NineTexture9_PreLoad( struct NineTexture9 *This )
{
    unsigned l;

    if (This->base.base.pool != D3DPOOL_MANAGED)
        return;

    for (l = 0; l <= This->base.base.info.last_level; ++l)
        NineSurface9_UpdateSelf(This->surfaces[l]);
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
    (void *)NineTexture9_PreLoad,
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
