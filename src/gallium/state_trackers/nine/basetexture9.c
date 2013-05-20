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

#include "basetexture9.h"
#include "device9.h"

#include "util/u_gen_mipmap.h"

#define DBG_CHANNEL DBG_BASETEXTURE

HRESULT
NineBaseTexture9_ctor( struct NineBaseTexture9 *This,
                       struct NineUnknownParams *pParams,
                       struct NineDevice9 *pDevice,
                       struct pipe_resource *pResource,
                       D3DRESOURCETYPE Type,
                       D3DPOOL Pool )
{
    HRESULT hr = NineResource9_ctor(&This->base, pParams, pDevice,
                                    pResource, Type, Pool);
    if (FAILED(hr))
        return hr;

    This->pipe = NineDevice9_GetPipe(pDevice);
    This->mipfilter = D3DTEXF_LINEAR;
    This->lod = 0;

    return D3D_OK;
}

void
NineBaseTexture9_dtor( struct NineBaseTexture9 *This )
{
    pipe_sampler_view_reference(&This->view, NULL);

    NineResource9_dtor(&This->base);
}

DWORD WINAPI
NineBaseTexture9_SetLOD( struct NineBaseTexture9 *This,
                         DWORD LODNew )
{
    DWORD old = This->lod;

    user_assert(This->base.pool == D3DPOOL_MANAGED, 0);

    This->lod = MIN2(LODNew, This->base.info.last_level);

    return old;
}

DWORD WINAPI
NineBaseTexture9_GetLOD( struct NineBaseTexture9 *This )
{
    return This->lod;
}

DWORD WINAPI
NineBaseTexture9_GetLevelCount( struct NineBaseTexture9 *This )
{
    if (This->base.usage & D3DUSAGE_AUTOGENMIPMAP)
        return 1;
    return This->base.info.last_level + 1;
}

HRESULT WINAPI
NineBaseTexture9_SetAutoGenFilterType( struct NineBaseTexture9 *This,
                                       D3DTEXTUREFILTERTYPE FilterType )
{
    user_assert(FilterType == D3DTEXF_POINT ||
                FilterType == D3DTEXF_LINEAR, D3DERR_INVALIDCALL);

    This->mipfilter = FilterType;

    return D3D_OK;
}

D3DTEXTUREFILTERTYPE WINAPI
NineBaseTexture9_GetAutoGenFilterType( struct NineBaseTexture9 *This )
{
    return This->mipfilter;
}

void WINAPI
NineBaseTexture9_GenerateMipSubLevels( struct NineBaseTexture9 *This )
{
    unsigned base_level = 0;
    unsigned last_level = This->base.info.last_level - This->lod;
    unsigned faces;
    unsigned filter = This->mipfilter == D3DTEXF_POINT ? PIPE_TEX_FILTER_NEAREST
                                                       : PIPE_TEX_FILTER_LINEAR;
    unsigned i;

    if (This->base.type == D3DRTYPE_CUBETEXTURE) {
        faces = 6;
        assert(This->base.info.array_size == 6);
    } else {
        faces = 1;
    }

    for (i = 0; i < faces; ++i)
        util_gen_mipmap(This->base.device->gen_mipmap,
                        This->view,
                        i, base_level, last_level, filter);
}

HRESULT
NineBaseTexture9_CreatePipeResource( struct NineBaseTexture9 *This )
{
    struct pipe_screen *screen = This->base.device->screen;
    struct pipe_resource templ;

    templ.target = This->base.info.target;
    templ.format = This->base.info.format;

    templ.width0 = u_minify(This->base.info.width0, This->lod);
    templ.height0 = u_minify(This->base.info.height0, This->lod);
    templ.depth0 = u_minify(This->base.info.depth0, This->lod);

    if (This->base.resource) {
        /* LOD might have changed. */
        if (This->base.resource->width0 == templ.width0 &&
            This->base.resource->height0 == templ.height0 &&
            This->base.resource->depth0 == templ.depth0)
            return D3D_OK;
        pipe_resource_reference(&This->base.resource, NULL);
    }

    templ.array_size = This->base.info.array_size;
    templ.last_level = This->base.info.last_level - This->lod;
    templ.nr_samples = This->base.info.nr_samples;

    templ.usage = This->base.info.usage;
    templ.bind = This->base.info.bind;
    templ.flags = This->base.info.flags;

    This->base.resource = screen->resource_create(screen, &templ);
    if (!This->base.resource)
        return D3DERR_OUTOFVIDEOMEMORY;
    return D3D_OK;
}
