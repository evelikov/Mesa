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

struct NineBaseTexture9
{
    struct NineResource9 base;
    struct list_head list;

    /* g3d */
    struct pipe_context *pipe;
    struct pipe_sampler_view *view;

    D3DFORMAT format;

    D3DTEXTUREFILTERTYPE mipfilter;
    DWORD lod;
    DWORD lod_resident;

    boolean dirty;
    boolean dirty_mip;
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

void WINAPI
NineBaseTexture9_PreLoad( struct NineBaseTexture9 *This );

/* For D3DPOOL_MANAGED only (after SetLOD change): */
HRESULT
NineBaseTexture9_CreatePipeResource( struct NineBaseTexture9 *This );

/* For D3DPOOL_MANAGED only: */
HRESULT
NineBaseTexture9_UploadSelf( struct NineBaseTexture9 *This );

HRESULT
NineBaseTexture9_UpdateSamplerView( struct NineBaseTexture9 *This );

static INLINE void
NineBaseTexture9_Validate( struct NineBaseTexture9 *This )
{
    DBG_FLAG(DBG_BASETEXTURE, "This=%p dirty=%i dirty_mip=%i\n",
             This, This->dirty, This->dirty_mip);
    if (This->dirty && This->base.pool == D3DPOOL_MANAGED)
        NineBaseTexture9_UploadSelf(This);
    if (This->dirty_mip)
        NineBaseTexture9_GenerateMipSubLevels(This);
}

static INLINE struct pipe_sampler_view *
NineBaseTexture9_GetSamplerView( struct NineBaseTexture9 *This )
{
    if (!This->view)
        NineBaseTexture9_UpdateSamplerView(This);
    return This->view;
}

#ifdef DEBUG
void
NineBaseTexture9_Dump( struct NineBaseTexture9 *This );
#else
static INLINE void
NineBaseTexture9_Dump( struct NineBaseTexture9 *This ) { }
#endif

#endif /* _NINE_BASETEXTURE9_H_ */
