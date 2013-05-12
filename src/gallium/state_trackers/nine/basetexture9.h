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

#ifndef _NINE_BASETEXTURE9_H_
#define _NINE_BASETEXTURE9_H_

#include "resource9.h"
#include "util/u_inlines.h"
#include "util/u_double_list.h"

#define NINE_MAX_TEXTURE_LEVELS 16

struct NineDirtyRegion
{
    struct list_head list;
    struct pipe_box box;
};

struct NineBaseTexture9
{
    struct NineResource9 base;

    /* g3d */
    struct pipe_context *pipe;
    struct pipe_sampler_view *view;

    D3DFORMAT format;
    UINT width;
    UINT height;
    UINT layers;
    BYTE last_level;

    D3DTEXTUREFILTERTYPE mipfilter;
    DWORD lod;

    struct pipe_transfer *transfer[NINE_MAX_TEXTURE_LEVELS];
    struct list_head dirty;
};
static INLINE struct NineBaseTexture9 *
NineBaseTexture9( void *data )
{
    return (struct NineBaseTexture9 *)data;
}

HRESULT
NineBaseTexture9_ctor( struct NineBaseTexture9 *This,
                       struct NineUnknownParams *pParams,
                       struct NineDevice9 *pDevice,
                       struct pipe_resource *pResource,
                       D3DRESOURCETYPE Type,
                       D3DPOOL Pool );

void
NineBaseTexture9_dtor( struct NineBaseTexture9 *This );

DWORD WINAPI
NineBaseTexture9_SetLOD( struct NineBaseTexture9 *This,
                         DWORD LODNew );

DWORD WINAPI
NineBaseTexture9_GetLOD( struct NineBaseTexture9 *This );

DWORD WINAPI
NineBaseTexture9_GetLevelCount( struct NineBaseTexture9 *This );

HRESULT WINAPI
NineBaseTexture9_SetAutoGenFilterType( struct NineBaseTexture9 *This,
                                       D3DTEXTUREFILTERTYPE FilterType );

D3DTEXTUREFILTERTYPE WINAPI
NineBaseTexture9_GetAutoGenFilterType( struct NineBaseTexture9 *This );

void WINAPI
NineBaseTexture9_GenerateMipSubLevels( struct NineBaseTexture9 *This );


static INLINE void
NineBaseTexture9_GetPipeBox2D( struct NineBaseTexture9 *This,
                              struct pipe_box *dst,
                              UINT Level,
                              UINT Face,
                              const RECT *pRect )
{
    if (pRect) {
        dst->x = pRect->left;
        dst->y = pRect->top;
        dst->z = Face;
        dst->width = pRect->right - pRect->left;
        dst->height = pRect->bottom - pRect->top;
        dst->depth = 1;
    } else {
        dst->x = 0;
        dst->y = 0;
        dst->z = Face;
        dst->width = u_minify(This->width, Level);
        dst->height = u_minify(This->height, Level);
        dst->depth = 1;
    }
}

static INLINE void
NineBaseTexture9_GetPipeBox3D( struct NineBaseTexture9 *This,
                              struct pipe_box *dst,
                              UINT Level,
                              const D3DBOX *pBox )
{
    if (pBox) {
        dst->x = pBox->Left;
        dst->y = pBox->Top;
        dst->z = pBox->Front;
        dst->width = pBox->Right - pBox->Left;
        dst->height = pBox->Bottom - pBox->Top;
        dst->depth = pBox->Back - pBox->Front;
    } else {
        dst->x = 0;
        dst->y = 0;
        dst->z = 0;
        dst->width = u_minify(This->width, Level);
        dst->height = u_minify(This->height, Level);
        dst->depth = u_minify(This->layers, Level);
    }
}

static INLINE struct NineDirtyRegion *
NineBaseTexture9_AddDirtyRegion( struct NineBaseTexture9 *This,
                                 const struct pipe_box *box )
{
    struct NineDirtyRegion *region = CALLOC_STRUCT(NineDirtyRegion);
    region->box = *box;
    list_inithead(&region->list);
    list_addtail(&This->dirty, &region->list);
    return region;
}

static INLINE void
NineBaseTexture9_ClearDirtyRegions( struct NineBaseTexture9 *This )
{
    struct NineDirtyRegion *r, *s;

    LIST_FOR_EACH_ENTRY_SAFE(r, s, &This->dirty, list)
        FREE(r);
}

#endif /* _NINE_BASETEXTURE9_H_ */
