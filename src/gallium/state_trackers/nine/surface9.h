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

#ifndef _NINE_SURFACE9_H_
#define _NINE_SURFACE9_H_

#include "resource9.h"

#include "pipe/p_state.h"
#include "util/u_double_list.h"
#include "util/u_rect.h"

struct NineDirtyRect
{
    struct list_head list;
    struct u_rect rect;
    boolean all_levels;
};

struct NineSurface9
{
    struct NineResource9 base;

    /* for GetContainer */
    struct NineUnknown *container;

    /* G3D state */
    struct pipe_context *pipe;
    struct pipe_screen *screen;
    struct pipe_transfer *transfer;
    struct pipe_surface *surface; /* created on-demand */

    /* resource description */
    unsigned level;
    unsigned layer;
    D3DSURFACE_DESC desc;

    struct list_head dirty;
};
static INLINE struct NineSurface9 *
NineSurface9( void *data )
{
    return (struct NineSurface9 *)data;
}

HRESULT
NineSurface9_new( struct NineDevice9 *pDevice,
                  struct NineUnknown *pContainer,
                  struct pipe_resource *pResource,
                  unsigned Level,
                  unsigned Layer,
                  D3DSURFACE_DESC *pDesc,
                  struct NineSurface9 **ppOut );

HRESULT
NineSurface9_ctor( struct NineSurface9 *This,
                   struct NineUnknownParams *pParams,
                   struct NineUnknown *pContainer,
                   struct NineDevice9 *pDevice,
                   struct pipe_resource *pResource,
                   unsigned Level,
                   unsigned Layer,
                   D3DSURFACE_DESC *pDesc );

void
NineSurface9_dtor( struct NineSurface9 *This );

/*** Nine private ***/

struct pipe_surface *
NineSurface9_CreatePipeSurface( struct NineSurface9 *This );

static INLINE struct pipe_surface *
NineSurface9_GetSurface( struct NineSurface9 *This )
{
    if (This->surface)
        return This->surface;
    return NineSurface9_CreatePipeSurface(This);
}

static INLINE struct pipe_resource *
NineSurface9_GetResource( struct NineSurface9 *This )
{
    return This->base.resource;
}

/*** Direct3D public ***/

HRESULT WINAPI
NineSurface9_GetContainer( struct NineSurface9 *This,
                           REFIID riid,
                           void **ppContainer );

HRESULT WINAPI
NineSurface9_GetDesc( struct NineSurface9 *This,
                      D3DSURFACE_DESC *pDesc );

HRESULT WINAPI
NineSurface9_LockRect( struct NineSurface9 *This,
                       D3DLOCKED_RECT *pLockedRect,
                       const RECT *pRect,
                       DWORD Flags );

HRESULT WINAPI
NineSurface9_UnlockRect( struct NineSurface9 *This );

HRESULT WINAPI
NineSurface9_GetDC( struct NineSurface9 *This,
                    HDC *phdc );

HRESULT WINAPI
NineSurface9_ReleaseDC( struct NineSurface9 *This,
                        HDC hdc );


static INLINE struct NineDirtyRect *
NineSurface9_AddDirtyRect( struct NineSurface9 *This,
                           const struct u_rect *rect, boolean all_levels )
{
    struct NineDirtyRect *drect;

    LIST_FOR_EACH_ENTRY(drect, &This->dirty, list) {
        if (drect->all_levels || !all_levels)
            if (u_rect_contained(rect, &drect->rect))
                return NULL;
        if (!drect->all_levels || all_levels) {
            if (u_rect_contained(&drect->rect, rect)) {
                drect->rect = *rect;
                drect->all_levels = all_levels;
                return drect;
            }
        }
    }
    drect = CALLOC_STRUCT(NineDirtyRect);
    if (drect) {
        list_inithead(&drect->list);
        list_addtail(&This->dirty, &drect->list);
        drect->rect = *rect;
        drect->all_levels = all_levels;
    }
    return drect;
}

static INLINE void
NineSurface9_ClearDirtyRects( struct NineSurface9 *This )
{
    struct NineDirtyRect *r, *s;

    LIST_FOR_EACH_ENTRY_SAFE(r, s, &This->dirty, list)
        FREE(r);
}

#endif /* _NINE_SURFACE9_H_ */
