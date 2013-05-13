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

#include "vertexbuffer9.h"
#include "device9.h"
#include "nine_helpers.h"
#include "nine_pipe.h"

#include "pipe/p_screen.h"
#include "pipe/p_context.h"
#include "pipe/p_state.h"
#include "pipe/p_defines.h"
#include "pipe/p_format.h"

#define DBG_CHANNEL DBG_VERTEXBUFFER

HRESULT
NineVertexBuffer9_ctor( struct NineVertexBuffer9 *This,
                        struct NineUnknownParams *pParams,
                        struct NineDevice9 *pDevice,
                        D3DVERTEXBUFFER_DESC *pDesc )
{
    struct pipe_resource *resource;
    HRESULT hr, reshr;

    This->maps = MALLOC(sizeof(struct pipe_transfer *));
    if (!This->maps)
        return E_OUTOFMEMORY;
    This->nmaps = 0;
    This->maxmaps = 1;

    {
        /* create a vertex buffer */
        struct pipe_resource tmplt;

        This->screen = NineDevice9_GetScreen(pDevice);
        This->pipe = NineDevice9_GetPipe(pDevice);

        tmplt.target = PIPE_BUFFER;
        tmplt.format = PIPE_FORMAT_R8_UNORM;
        tmplt.width0 = This->desc.Size;
        tmplt.height0 = 1;
        tmplt.depth0 = 1;
        tmplt.array_size = 1;
        tmplt.last_level = 0;
        tmplt.nr_samples = 0;
        tmplt.usage = PIPE_USAGE_DEFAULT;

        if (This->desc.Usage & D3DUSAGE_DYNAMIC) {
            tmplt.usage = PIPE_USAGE_DYNAMIC;
        }
        /*if (This->desc.Usage & D3DUSAGE_DONOTCLIP) { }*/
        /*if (This->desc.Usage & D3DUSAGE_NONSECURE) { }*/
        /*if (This->desc.Usage & D3DUSAGE_NPATCHES) { }*/
        /*if (This->desc.Usage & D3DUSAGE_POINTS) { }*/
        /*if (This->desc.Usage & D3DUSAGE_RTPATCHES) { }*/
        /*if (This->desc.Usage & D3DUSAGE_SOFTWAREPROCESSING) { }*/
        /*if (This->desc.Usage & D3DUSAGE_TEXTAPI) { }*/

        tmplt.bind = PIPE_BIND_VERTEX_BUFFER | PIPE_BIND_TRANSFER_WRITE;
        if (!(This->desc.Usage & D3DUSAGE_WRITEONLY)) {
            tmplt.bind |= PIPE_BIND_TRANSFER_READ;
        }
        tmplt.flags = 0;

        resource = This->screen->resource_create(This->screen, &tmplt);
        if (!resource) {
            DBG("screen::resource_create failed\n"
                " format = %u\n"
                " width0 = %u\n"
                " usage = %u\n"
                " bind = %u\n",
                tmplt.format, tmplt.width0, tmplt.usage, tmplt.bind);
            /* XXX do we report E_OUTOFMEMORY on Pool==MANAGED? */
            reshr = D3DERR_OUTOFVIDEOMEMORY;
        }
    }
    hr = NineResource9_ctor(&This->base, pParams, pDevice, resource,
                            D3DRTYPE_VERTEXBUFFER, pDesc->Pool);
    if (FAILED(hr)) { return FAILED(reshr) ? reshr : hr; }

    pDesc->Type = D3DRTYPE_VERTEXBUFFER;
    pDesc->Format = D3DFMT_VERTEXDATA;
    This->desc = *pDesc;

    return D3D_OK;
}

void
NineVertexBuffer9_dtor( struct NineVertexBuffer9 *This )
{
    if (This->maps) {
        while (This->nmaps) {
            NineVertexBuffer9_Unlock(This);
        }
        FREE(This->maps);
    }

    NineResource9_dtor(&This->base);
}

HRESULT WINAPI
NineVertexBuffer9_Lock( struct NineVertexBuffer9 *This,
                        UINT OffsetToLock,
                        UINT SizeToLock,
                        void **ppbData,
                        DWORD Flags )
{
    struct pipe_box box;
    unsigned usage = 0;
    void *data;

    user_assert(ppbData, E_POINTER);
    
    if (This->nmaps == This->maxmaps) {
        struct pipe_transfer **newmaps =
            REALLOC(This->maps, sizeof(struct pipe_transfer *)*This->maxmaps,
                    sizeof(struct pipe_transfer *)*(This->maxmaps << 1));
        if (newmaps == NULL)
            return E_OUTOFMEMORY;
        
        This->maxmaps <<= 1;
        This->maps = newmaps;
    }

    if (Flags & D3DLOCK_DISCARD) { usage &= PIPE_TRANSFER_DISCARD_RANGE; }
    /* This flag indicates that no data used in the previous drawing call will
     * be touched in this update. How do we handle that? XXX
    if (Flags & D3DLOCK_NO_DIRTY_UPDATE) { }*/
    /* This should NOT lock the windowing system, which we already can't XXX
    if (Flags & D3DLOCK_NOSYSLOCK) { }*/
    usage &= PIPE_TRANSFER_READ;
    if (!(Flags & D3DLOCK_READONLY)) { usage &= PIPE_TRANSFER_WRITE; }
    if (Flags & D3DLOCK_NOOVERWRITE) { usage &= PIPE_TRANSFER_UNSYNCHRONIZED; }

    box.x = OffsetToLock;
    box.y = 0;
    box.z = 0;
    box.width = SizeToLock;
    box.height = 0;
    box.depth = 0;

    data = This->pipe->transfer_map(This->pipe, This->base.resource, 0,
                                    usage, &box, &This->maps[This->nmaps]);
    if (!data) {
        DBG("pipe::transfer_map failed\n"
            " usage = %u\n"
            " box.x = %u\n"
            " box.width = %u\n",
            usage, box.x, box.width);
        /* not sure what to return, msdn suggests this */
        return D3DERR_INVALIDCALL;
    }

    This->nmaps++;
    *ppbData = data;

    return D3D_OK;
}

HRESULT WINAPI
NineVertexBuffer9_Unlock( struct NineVertexBuffer9 *This )
{
    user_assert(This->nmaps > 0, D3DERR_INVALIDCALL);
    This->pipe->transfer_unmap(This->pipe, This->maps[--(This->nmaps)]);
    return D3D_OK;
}

HRESULT WINAPI
NineVertexBuffer9_GetDesc( struct NineVertexBuffer9 *This,
                           D3DVERTEXBUFFER_DESC *pDesc )
{
    user_assert(pDesc, E_POINTER);
    *pDesc = This->desc;
    return D3D_OK;
}

IDirect3DVertexBuffer9Vtbl NineVertexBuffer9_vtable = {
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
    (void *)NineVertexBuffer9_Lock,
    (void *)NineVertexBuffer9_Unlock,
    (void *)NineVertexBuffer9_GetDesc
};

static const GUID *NineVertexBuffer9_IIDs[] = {
    &IID_IDirect3DVertexBuffer9,
    &IID_IDirect3DResource9,
    &IID_IUnknown,
    NULL
};

HRESULT
NineVertexBuffer9_new( struct NineDevice9 *pDevice,
                       D3DVERTEXBUFFER_DESC *pDesc,
                       struct NineVertexBuffer9 **ppOut )
{
    NINE_NEW(NineVertexBuffer9, ppOut, /* args */ pDevice, pDesc);
}
