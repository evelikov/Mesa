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

#ifndef _D3DPRESENT_H_
#define _D3DPRESENT_H_

#include <d3d9.h>

#define D3DOK_WINDOW_OCCLUDED MAKE_D3DSTATUS(2531)

#ifndef __cplusplus
typedef struct ID3DPresent ID3DPresent;

/* Presentation backend for drivers to display their brilliant work */
typedef struct ID3DPresentVtbl
{
    /* IUnknown */
    HRESULT (WINAPI *QueryInterface)(ID3DPresent *This, REFIID riid, void **ppvObject);
    ULONG (WINAPI *AddRef)(ID3DPresent *This);
    ULONG (WINAPI *Release)(ID3DPresent *This);

    /* ID3DPresent */
    /* This function initializes the screen and window provided at creation.
     * Hence why this should always be called as the one of first things a new
     * swap chain does */
    HRESULT (WINAPI *GetPresentParameters)(ID3DPresent *This, D3DPRESENT_PARAMETERS *pPresentationParameters);
    /* In alignment with d3d1x, always draw to a window system provided instead
     * of going the optimum way. The issue here of course is that it requires
     * an extra blit. See d3d1x/dxgi/src/dxgi_native.cpp for a long discussion
     * of how to optimally implement presentation */
    HRESULT (WINAPI *GetBuffer)(ID3DPresent *This, HWND hWndOverride, void *pBuffer, RECT *pRect, RGNDATA **ppRegion);
    /* Get a handle to the front buffer, or a copy of the front buffer */
    HRESULT (WINAPI *GetFrontBuffer)(ID3DPresent *This, void *pBuffer);
    HRESULT (WINAPI *Present)(ID3DPresent *This, DWORD Flags);
    HRESULT (WINAPI *GetRasterStatus)(ID3DPresent *This, D3DRASTER_STATUS *pRasterStatus);
    HRESULT (WINAPI *GetDisplayMode)(ID3DPresent *This, D3DDISPLAYMODEEX *pMode);
    HRESULT (WINAPI *GetPresentStats)(ID3DPresent *This, D3DPRESENTSTATS *pStats);
    HRESULT (WINAPI *GetCursorPos)(ID3DPresent *This, POINT *pPoint);
    HRESULT (WINAPI *SetCursorPos)(ID3DPresent *This, POINT *pPoint);
    /* Cursor size is always 32x32. pBitmap and pHotspot can be NULL. */
    HRESULT (WINAPI *SetCursor)(ID3DPresent *This, void *pBitmap, POINT *pHotspot, BOOL bShow);
    HRESULT (WINAPI *SetGammaRamp)(ID3DPresent *This, const D3DGAMMARAMP *pRamp, HWND hWndOverride);
    HRESULT (WINAPI *GetWindowRect)(ID3DPresent *This, HWND hWnd, LPRECT pRect);
} ID3DPresentVtbl;

struct ID3DPresent
{
    ID3DPresentVtbl *lpVtbl;
};

/* IUnknown macros */
#define ID3DPresent_QueryInterface(p,a,b) (p)->lpVtbl->QueryInterface(p,a,b)
#define ID3DPresent_AddRef(p) (p)->lpVtbl->AddRef(p)
#define ID3DPresent_Release(p) (p)->lpVtbl->Release(p)
/* ID3DPresent macros */
#define ID3DPresent_GetPresentParameters(p,a) (p)->lpVtbl->GetPresentParameters(p,a)
#define ID3DPresent_GetBuffer(p,a,b,c,d) (p)->lpVtbl->GetBuffer(p,a,b,c,d)
#define ID3DPresent_GetFrontBuffer(p,a) (p)->lpVtbl->GetFrontBuffer(p,a)
#define ID3DPresent_Present(p,a) (p)->lpVtbl->Present(p,a)
#define ID3DPresent_GetRasterStatus(p,a) (p)->lpVtbl->GetRasterStatus(p,a)
#define ID3DPresent_GetDisplayMode(p,a) (p)->lpVtbl->GetDisplayMode(p,a)
#define ID3DPresent_GetPresentStats(p,a) (p)->lpVtbl->GetPresentStats(p,a)
#define ID3DPresent_GetCursorPos(p,a) (p)->lpVtbl->GetCursorPos(p,a)
#define ID3DPresent_SetCursorPos(p,a) (p)->lpVtbl->SetCursorPos(p,a)
#define ID3DPresent_SetCursor(p,a,b,c) (p)->lpVtbl->SetCursor(p,a,b,c)
#define ID3DPresent_SetGammaRamp(p,a,b) (p)->lpVtbl->SetGammaRamp(p,a,b)
#define ID3DPresent_GetWindowRect(p,a,b) (p)->lpVtbl->GetWindowRect(p,a,b)

typedef struct ID3DPresentFactory ID3DPresentFactory;

typedef struct ID3DPresentFactoryVtbl
{
    /* IUnknown */
    HRESULT (WINAPI *QueryInterface)(ID3DPresentFactory *This, REFIID riid, void **ppvObject);
    ULONG (WINAPI *AddRef)(ID3DPresentFactory *This);
    ULONG (WINAPI *Release)(ID3DPresentFactory *This);

    /* ID3DPresentFactory */
    /* When creating a device, it's relevant for the driver to know how many
     * implicit swap chains to create. It has to create one per monitor in a
     * multi-monitor setup */
    UINT (WINAPI *GetMultiheadCount)(ID3DPresentFactory *This);
    /* returns only the implicit present interfaces */
    HRESULT (WINAPI *GetPresent)(ID3DPresentFactory *This, UINT Index, ID3DPresent **ppPresent);
    /* used to create additional presentation interfaces along the way */
    HRESULT (WINAPI *CreateAdditionalPresent)(ID3DPresentFactory *This, D3DPRESENT_PARAMETERS *pPresentationParameters, ID3DPresent **ppPresent);
} ID3DPresentFactoryVtbl;

struct ID3DPresentFactory
{
    ID3DPresentFactoryVtbl *lpVtbl;
};

/* IUnknown macros */
#define ID3DPresentFactory_QueryInterface(p,a,b) (p)->lpVtbl->QueryInterface(p,a,b)
#define ID3DPresentFactory_AddRef(p) (p)->lpVtbl->AddRef(p)
#define ID3DPresentFactory_Release(p) (p)->lpVtbl->Release(p)
/* ID3DPresentFactory */
#define ID3DPresentFactory_GetMultiheadCount(p) (p)->lpVtbl->GetMultiheadCount(p)
#define ID3DPresentFactory_GetPresent(p,a,b) (p)->lpVtbl->GetPresent(p,a,b)
#define ID3DPresentFactory_CreateAdditionalPresent(p,a,b) (p)->lpVtbl->CreateAdditionalPresent(p,a,b)

#else /* __cplusplus */

struct ID3DPresent : public IUnknown
{
    HRESULT WINAPI GetPresentParameters(D3DPRESENT_PARAMETERS *pPresentationParameters);
    HRESULT WINAPI GetBuffer(HWND hWndOverride, void *pBuffer, RECT *pRect, RGNDATA **ppRegion);
    HRESULT WINAPI GetFrontBuffer(void *pBuffer);
    HRESULT WINAPI Present(DWORD Flags);
    HRESULT WINAPI GetRasterStatus(D3DRASTER_STATUS *pRasterStatus);
    HRESULT WINAPI GetDisplayMode(D3DDISPLAYMODEEX *pMode);
    HRESULT WINAPI GetPresentStats(D3DPRESENTSTATS *pStats);
};

struct ID3DPresentFactory : public IUnknown
{
    UINT WINAPI GetMultiheadCount();
    HRESULT WINAPI GetPresent(UINT Index, ID3DPresent **ppPresent);
    HRESULT WINAPI CreateAdditionalPresent(D3DPRESENT_PARAMETERS *pPresentationParameters, ID3DPresent **ppPresent);
};

#endif /* __cplusplus */

#endif /* _D3DPRESENT_H_ */
