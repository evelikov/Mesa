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

#ifndef _NINE_SWAPCHAIN9_H_
#define _NINE_SWAPCHAIN9_H_

#include "iunknown.h"
#include "adapter9.h"

#include "d3dpresent.h"

struct NineDevice9;
struct NineSurface9;
struct nine_winsys_swapchain;
struct blit_state;

struct NineSwapChain9
{
    struct NineUnknown base;

    /* G3D stuff */
    struct pipe_screen *screen;
    struct pipe_context *pipe;
    struct cso_context *cso;
    struct blit_state *blitter;

    /* presentation backend */
    ID3DPresent *present;
    D3DPRESENT_PARAMETERS params;
    PPRESENT_TO_RESOURCE ptrfunc;

    /* parent device */
    struct NineDevice9 *device;

    /* buffer handles */
    struct NineSurface9 **buffers; /* [0] is frontbuffer */
};
static INLINE struct NineSwapChain9 *
NineSwapChain9( void *data )
{
    return (struct NineSwapChain9 *)data;
}

HRESULT
NineSwapChain9_new( struct NineDevice9 *pDevice,
                    ID3DPresent *pPresent,
                    PPRESENT_TO_RESOURCE pPTR,
                    HWND hFocusWindow,
                    struct NineSwapChain9 **ppOut );

HRESULT
NineSwapChain9_ctor( struct NineSwapChain9 *This,
                     struct NineUnknownParams *pParams,
                     struct NineDevice9 *pDevice,
                     ID3DPresent *pPresent,
                     PPRESENT_TO_RESOURCE pPTR,
                     HWND hFocusWindow );

void
NineSwapChain9_dtor( struct NineSwapChain9 *This );

HRESULT WINAPI
NineSwapChain9_Present( struct NineSwapChain9 *This,
                        const RECT *pSourceRect,
                        const RECT *pDestRect,
                        HWND hDestWindowOverride,
                        const RGNDATA *pDirtyRegion,
                        DWORD dwFlags );

HRESULT WINAPI
NineSwapChain9_GetFrontBufferData( struct NineSwapChain9 *This,
                                   IDirect3DSurface9 *pDestSurface );

HRESULT WINAPI
NineSwapChain9_GetBackBuffer( struct NineSwapChain9 *This,
                              UINT iBackBuffer,
                              D3DBACKBUFFER_TYPE Type,
                              IDirect3DSurface9 **ppBackBuffer );

HRESULT WINAPI
NineSwapChain9_GetRasterStatus( struct NineSwapChain9 *This,
                                D3DRASTER_STATUS *pRasterStatus );

HRESULT WINAPI
NineSwapChain9_GetDisplayMode( struct NineSwapChain9 *This,
                               D3DDISPLAYMODE *pMode );

HRESULT WINAPI
NineSwapChain9_GetDevice( struct NineSwapChain9 *This,
                          IDirect3DDevice9 **ppDevice );

HRESULT WINAPI
NineSwapChain9_GetPresentParameters( struct NineSwapChain9 *This,
                                     D3DPRESENT_PARAMETERS *pPresentationParameters );

#endif /* _NINE_SWAPCHAIN9_H_ */
