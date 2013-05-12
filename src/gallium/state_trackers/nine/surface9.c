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
    struct pipe_surface tmplt;
    HRESULT hr = NineResource9_ctor(&This->base, pParams, pDevice, pResource,
                                    D3DRTYPE_SURFACE, pDesc->Pool);
    if (FAILED(hr)) { return hr; }

    /* Reference the container so the data doesn't get deallocated in case
     * the user releases the container.
     */
    NineUnknown_AddRef(pContainer);
    This->container = pContainer;
    This->pipe = NineDevice9_GetPipe(pDevice);
    This->screen = NineDevice9_GetScreen(pDevice);
    This->transfer = NULL;

    memset(&tmplt, 0, sizeof(struct pipe_surface));
    u_surface_default_template(&tmplt, pResource);
    tmplt.u.tex.level = Level;
    tmplt.u.tex.first_layer = Layer;
    tmplt.u.tex.last_layer = Layer;
    This->surface = This->pipe->create_surface(This->pipe, pResource, &tmplt);

    This->level = Level;
    This->desc = *pDesc;
    if (!This->surface) { return D3DERR_DRIVERINTERNALERROR; }

    return D3D_OK;
}

void
NineSurface9_dtor( struct NineSurface9 *This )
{
    if (This->surface) { pipe_surface_reference(&This->surface, NULL); }
    if (This->transfer) { NineSurface9_UnlockRect(This); }

    NineUnknown_Release(This->container);

    NineResource9_dtor(&This->base);
}

struct pipe_surface *
NineSurface9_GetSurface( struct NineSurface9 *This )
{
    return This->surface;
}

HRESULT WINAPI
NineSurface9_GetContainer( struct NineSurface9 *This,
                           REFIID riid,
                           void **ppContainer )
{
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
    struct pipe_box box;
    unsigned usage = 0;

    /* check if it's already locked */
    user_assert(!This->transfer, D3DERR_INVALIDCALL);
    user_assert(pLockedRect, E_POINTER);

    if (Flags & D3DLOCK_DISCARD) {
        usage |= PIPE_TRANSFER_DISCARD_RANGE;
    } else {
        usage |= PIPE_TRANSFER_READ;
    }

    if (!(Flags & D3DLOCK_READONLY)) { usage |= PIPE_TRANSFER_WRITE; }
    if (Flags & D3DLOCK_DONOTWAIT) { usage |= PIPE_TRANSFER_DONTBLOCK; }
    if (Flags & D3DLOCK_NOOVERWRITE) { usage |= PIPE_TRANSFER_UNSYNCHRONIZED; }
    /* XXX prevent dirty state updates?
    if (Flags & D3DLOCK_NO_DIRTY_UPDATE) { } */
    /* XXX we don't have a system-wide lock for this, so ignore
    if (Flags & D3DLOCK_NOSYSLOCK) { } */

    if (pRect) {
        box.x = pRect->left;
        box.y = pRect->top;
        box.z = 0;
        box.width = pRect->right-pRect->left;
        box.height = pRect->bottom-pRect->top;
        box.depth = 0;
    } else {
        box.x = 0;
        box.y = 0;
        box.z = 0;
        box.width = This->desc.Width;
        box.height = This->desc.Height;
        box.depth = 0;
    }
    box.width = MIN2(box.width-box.x, This->desc.Width-box.x);
    box.height = MIN2(box.height-box.y, This->desc.Height-box.y);

    pLockedRect->pBits = This->pipe->transfer_map(This->pipe,
                                                  This->surface->texture,
                                                  This->level, usage, &box,
                                                  &This->transfer);
    if (!This->transfer) {
        if (Flags & D3DLOCK_DONOTWAIT) {
            return D3DERR_WASSTILLDRAWING;
        }
        return D3DERR_INVALIDCALL;
    }
    pLockedRect->Pitch = This->transfer->stride;

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
