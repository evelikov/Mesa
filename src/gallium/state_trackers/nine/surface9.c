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

#include "surface9.h"
#include "device9.h"

#include "nine_helpers.h"
#include "nine_pipe.h"

#include "pipe/p_context.h"
#include "pipe/p_screen.h"
#include "pipe/p_state.h"

#include "util/u_math.h"
#include "util/u_inlines.h"
#include "util/u_surface.h"

#define DBG_CHANNEL DBG_SURFACE

HRESULT
NineSurface9_ctor( struct NineSurface9 *This,
                   struct NineUnknownParams *pParams,
                   struct NineUnknown *pContainer,
                   struct NineDevice9 *pDevice,
                   struct pipe_resource *pResource,
                   unsigned Level,
                   unsigned Layer,
                   D3DSURFACE_DESC *pDesc )
{
    HRESULT hr = NineResource9_ctor(&This->base, pParams, pDevice, pResource,
                                    D3DRTYPE_SURFACE, pDesc->Pool);
    if (FAILED(hr))
        return hr;

    user_assert(!(pDesc->Usage & D3DUSAGE_DYNAMIC) ||
                (pDesc->Pool != D3DPOOL_MANAGED), D3DERR_INVALIDCALL);

    /* Reference the container so the data doesn't get deallocated in case
     * the user releases the container.
     */
    if (pContainer)
        NineUnknown_AddRef(pContainer);
    This->container = pContainer;

    This->pipe = NineDevice9_GetPipe(pDevice);
    This->screen = NineDevice9_GetScreen(pDevice);
    This->transfer = NULL;

    This->level = Level;
    This->layer = Layer;
    This->desc = *pDesc;

    if ((pDesc->Usage & (D3DUSAGE_DEPTHSTENCIL | D3DUSAGE_RENDERTARGET)) &&
        Level == 0) {
        NineSurface9_CreatePipeSurface(This);
        if (!This->surface)
            return D3DERR_DRIVERINTERNALERROR;
    }

    list_inithead(&This->dirty);

    return D3D_OK;
}

void
NineSurface9_dtor( struct NineSurface9 *This )
{
    if (This->transfer)
        NineSurface9_UnlockRect(This);
    NineSurface9_ClearDirtyRects(This);

    pipe_surface_reference(&This->surface, NULL);

    if (This->container)
        NineUnknown_Release(This->container);

    NineResource9_dtor(&This->base);
}

struct pipe_surface *
NineSurface9_CreatePipeSurface( struct NineSurface9 *This )
{
    struct pipe_context *pipe = This->pipe;
    struct pipe_resource *resource = This->base.resource;
    struct pipe_surface templ;

    templ.format = resource->format;
    templ.u.tex.level = This->level;
    templ.u.tex.first_layer = This->layer;
    templ.u.tex.last_layer = This->layer;

    This->surface = pipe->create_surface(pipe, resource, &templ);
    assert(This->surface);
    return This->surface;
}

HRESULT WINAPI
NineSurface9_GetContainer( struct NineSurface9 *This,
                           REFIID riid,
                           void **ppContainer )
{
    if (!This->container)
        return E_NOINTERFACE;
    return NineUnknown_QueryInterface(This->container, riid, ppContainer);
}

HRESULT WINAPI
NineSurface9_GetDesc( struct NineSurface9 *This,
                      D3DSURFACE_DESC *pDesc )
{
    user_assert(pDesc != NULL, E_POINTER);
    *pDesc = This->desc;
    return D3D_OK;
}

HRESULT WINAPI
NineSurface9_LockRect( struct NineSurface9 *This,
                       D3DLOCKED_RECT *pLockedRect,
                       const RECT *pRect,
                       DWORD Flags )
{
    struct pipe_resource *resource = This->base.resource;
    struct pipe_box box;
    unsigned usage;

    user_assert(!(Flags & ~(D3DLOCK_DISCARD |
                            D3DLOCK_DONOTWAIT |
                            D3DLOCK_NO_DIRTY_UPDATE |
                            D3DLOCK_NOSYSLOCK | /* ignored */
                            D3DLOCK_READONLY)), D3DERR_INVALIDCALL);
    user_assert(!((Flags & D3DLOCK_DISCARD) && (Flags & D3DLOCK_READONLY)),
                D3DERR_INVALIDCALL);

    /* check if it's already locked */
    user_assert(!This->transfer, D3DERR_INVALIDCALL);
    user_assert(pLockedRect, E_POINTER);

    if (Flags & D3DLOCK_DISCARD) {
        usage = PIPE_TRANSFER_WRITE | PIPE_TRANSFER_DISCARD_RANGE;
    } else {
        usage = (Flags & D3DLOCK_READONLY) ?
            PIPE_TRANSFER_READ : PIPE_TRANSFER_READ_WRITE;
    }
    if (Flags & D3DLOCK_DONOTWAIT)
        usage |= PIPE_TRANSFER_DONTBLOCK;

    if (pRect) {
        rect_to_pipe_box(&box, pRect);
        if (u_box_clip_2d(&box, This->desc.Width, This->desc.Height) < 0)
            return D3DERR_INVALIDCALL;
    } else {
        u_box_origin_2d(This->desc.Width, This->desc.Height, &box);
    }

    pLockedRect->pBits = This->pipe->transfer_map(This->pipe, resource,
                                                  This->level, usage, &box,
                                                  &This->transfer);
    if (!This->transfer) {
        if (Flags & D3DLOCK_DONOTWAIT)
            return D3DERR_WASSTILLDRAWING;
        return D3DERR_INVALIDCALL;
    }
    pLockedRect->Pitch = This->transfer->stride;

    if (!(Flags & D3DLOCK_NO_DIRTY_UPDATE)) {
        /* XXX: make this a pipe_box ? */
        struct u_rect rect;
        rect.x0 = box.x;
        rect.x1 = box.x + box.width;
        rect.y0 = box.y;
        rect.y1 = box.y + box.height;
        NineSurface9_AddDirtyRect(This, &rect, FALSE);
    }

    return D3D_OK;
}

HRESULT WINAPI
NineSurface9_UnlockRect( struct NineSurface9 *This )
{
    user_assert(This->transfer, D3DERR_INVALIDCALL);
    This->pipe->transfer_unmap(This->pipe, This->transfer);
    This->transfer = NULL;
    return D3D_OK;
}

HRESULT WINAPI
NineSurface9_GetDC( struct NineSurface9 *This,
                    HDC *phdc )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineSurface9_ReleaseDC( struct NineSurface9 *This,
                        HDC hdc )
{
    STUB(D3DERR_INVALIDCALL);
}

IDirect3DSurface9Vtbl NineSurface9_vtable = {
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
    (void *)NineSurface9_GetContainer,
    (void *)NineSurface9_GetDesc,
    (void *)NineSurface9_LockRect,
    (void *)NineSurface9_UnlockRect,
    (void *)NineSurface9_GetDC,
    (void *)NineSurface9_ReleaseDC
};


/* TODO: maybe switch to transfer_inline_write ? */
HRESULT
NineSurface9_UploadFromSurface( struct NineSurface9 *This,
                                struct NineSurface9 *From )
{
#if 0
    struct NineDirtyRect *dirty;
    enum pipe_format fmt = This->base.resource->format;
    HRESULT hr;
    D3DLOCKED_RECT dst, src;
    RECT rect;
    const unsigned width = From->desc.Width;
    const unsigned height = From->desc.Height;

    /* What about mip-maps ? */

    LIST_FOR_EACH_ENTRY(rect, &From->dirty, list) {
        rect.left = dirty->rect.x0;
        rect.top = dirty->rect.y0;
        rect.right = dirty->rect.x1;
        rect.bottom = dirty->rect.y1;

        hr = NineSurface9_Lock(This, &dst, &rect,
                               D3DLOCK_DISCARD | D3DLOCK_NO_DIRTY_UPDATE);
        if (FAILED(hr))
            return hr;
        hr = NineSurface9_Lock(From, &src, &rect,
                               D3DLOCK_READONLY);
        if (FAILED(hr)) {
            NineSurface9_Unlock(This);
            return hr;
        }

        if (dst.Pitch == src.Pitch) {
            memcpy(dst.pBits, src.pBits, size);
        } else {
            const unsigned size = util_format_get_stride(fmt, width);
            unsigned i;
            for (i = 0; i < height; ++i) {
                memcpy(dst.pBits, src.pBits, size);
                dst.pBits = (uint8_t *)dst.pBits + dst.Pitch;
                src.pBits = (uint8_t *)src.pBits + src.Pitch;
            }
        }

        NineSurface9_Unlock(From);
        NineSurface9_Unlock(This);
    }
    return D3D_OK;
#else
    STUB(D3DERR_INVALIDCALL);
#endif
}

HRESULT
NineSurface9_DownloadFromSurface( struct NineSurface9 *This,
                                  struct NineSurface9 *From )
{
    /* This works since we use NineSurface9_Lock. */
    return NineSurface9_UploadFromSurface(This, From);
}

HRESULT
NineSurface9_UploadSelf( struct NineSurface9 *This )
{
#if 0
    struct pipe_context *pipe = This->pipe;
    struct pipe_resource *resource = This->base.resource;
    struct pipe_box box;
    struct NineDirtyRect *dirty;
    void *data;
    const unsigned usage = PIPE_TRANSFER_WRITE | PIPE_TRANSFER_DISCARD;
    unsigned stride;
    const enum pipe_format fmt = resource->format;

    assert(This->desc.Pool == D3DPOOL_MANAGED);

    box.z = This->level;
    box.depth = 1;

    LIST_FOR_EACH_ENTRY(rect, &This->dirty, list) {
        box.x = dirty->rect.x0;
        box.y = dirty->rect.y0;
        box.width = dirty->rect.x1 - dirty->rect.x0;
        box.height = dirty->rect.y1 - dirty->rect.y0;

        data = This->base.data +
            This->base.sys.stride * box.y + util_format_get_stride(fmt, box.x);
        stride = This->base.sys.stride;

        DBG("Dirty: (%u.%u) (%ux%u) data(%p) stride(%u)\n",
            box.x, box.y, box.width, box.height, data, stride);

        pipe->transfer_inline_write(pipe, resource, This->level, usage, &box,
                                    data, stride, 0);
    }
    return D3D_OK;
#else
    STUB(D3DERR_INVALIDCALL);
#endif
}


static const GUID *NineSurface9_IIDs[] = {
    &IID_IDirect3DSurface9,
    &IID_IDirect3DResource9,
    &IID_IUnknown,
    NULL
};

HRESULT
NineSurface9_new( struct NineDevice9 *pDevice,
                  struct NineUnknown *pContainer,
                  struct pipe_resource *pResource,
                  unsigned Level,
                  unsigned Layer,
                  D3DSURFACE_DESC *pDesc,
                  struct NineSurface9 **ppOut )
{
    NINE_NEW(NineSurface9, ppOut, /* args */
             pContainer, pDevice, pResource, Level, Layer, pDesc);
}
