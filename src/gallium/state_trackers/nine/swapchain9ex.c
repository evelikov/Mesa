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

#include "swapchain9ex.h"

#define DBG_CHANNEL DBG_SWAPCHAIN

HRESULT WINAPI
NineSwapChain9Ex_GetLastPresentCount( struct NineSwapChain9Ex *This,
                                      UINT *pLastPresentCount )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineSwapChain9Ex_GetPresentStats( struct NineSwapChain9Ex *This,
                                  D3DPRESENTSTATS *pPresentationStatistics )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineSwapChain9Ex_GetDisplayModeEx( struct NineSwapChain9Ex *This,
                                   D3DDISPLAYMODEEX *pMode,
                                   D3DDISPLAYROTATION *pRotation )
{
    STUB(D3DERR_INVALIDCALL);
}

IDirect3DSwapChain9ExVtbl NineSwapChain9Ex_vtable = {
    (void *)NineUnknown_QueryInterface,
    (void *)NineUnknown_AddRef,
    (void *)NineUnknown_Release,
    (void *)NineSwapChain9_Present,
    (void *)NineSwapChain9_GetFrontBufferData,
    (void *)NineSwapChain9_GetBackBuffer,
    (void *)NineSwapChain9_GetRasterStatus,
    (void *)NineSwapChain9_GetDisplayMode,
    (void *)NineSwapChain9_GetDevice,
    (void *)NineSwapChain9_GetPresentParameters,
    (void *)NineSwapChain9Ex_GetLastPresentCount,
    (void *)NineSwapChain9Ex_GetPresentStats,
    (void *)NineSwapChain9Ex_GetDisplayModeEx
};
