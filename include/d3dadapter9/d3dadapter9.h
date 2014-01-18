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

#ifndef _D3DADAPTER9_H_
#define _D3DADAPTER9_H_

#include <d3d9.h>

#ifndef D3DOK_WINDOW_OCCLUDED
#define D3DOK_WINDOW_OCCLUDED MAKE_D3DSTATUS(2531)
#endif /* D3DOK_WINDOW_OCCLUDED */

#ifndef __cplusplus
typedef struct ID3DPresent ID3DPresent;
typedef struct ID3DPresentGroup ID3DPresentGroup;
typedef struct ID3DAdapter9 ID3DAdapter9;

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
    HRESULT (WINAPI *GetBuffer)(ID3DPresent *This, HWND hWndOverride, void *pBuffer, const RECT *pDestRect, RECT *pRect, RGNDATA **ppRegion);
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
#define ID3DPresent_GetBuffer(p,a,b,c,d,e) (p)->lpVtbl->GetBuffer(p,a,b,c,d,e)
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

typedef struct ID3DPresentGroupVtbl
{
    /* IUnknown */
    HRESULT (WINAPI *QueryInterface)(ID3DPresentGroup *This, REFIID riid, void **ppvObject);
    ULONG (WINAPI *AddRef)(ID3DPresentGroup *This);
    ULONG (WINAPI *Release)(ID3DPresentGroup *This);

    /* ID3DPresentGroup */
    /* When creating a device, it's relevant for the driver to know how many
     * implicit swap chains to create. It has to create one per monitor in a
     * multi-monitor setup */
    UINT (WINAPI *GetMultiheadCount)(ID3DPresentGroup *This);
    /* returns only the implicit present interfaces */
    HRESULT (WINAPI *GetPresent)(ID3DPresentGroup *This, UINT Index, ID3DPresent **ppPresent);
    /* used to create additional presentation interfaces along the way */
    HRESULT (WINAPI *CreateAdditionalPresent)(ID3DPresentGroup *This, D3DPRESENT_PARAMETERS *pPresentationParameters, ID3DPresent **ppPresent);
} ID3DPresentGroupVtbl;

struct ID3DPresentGroup
{
    ID3DPresentGroupVtbl *lpVtbl;
};

/* IUnknown macros */
#define ID3DPresentGroup_QueryInterface(p,a,b) (p)->lpVtbl->QueryInterface(p,a,b)
#define ID3DPresentGroup_AddRef(p) (p)->lpVtbl->AddRef(p)
#define ID3DPresentGroup_Release(p) (p)->lpVtbl->Release(p)
/* ID3DPresentGroup */
#define ID3DPresentGroup_GetMultiheadCount(p) (p)->lpVtbl->GetMultiheadCount(p)
#define ID3DPresentGroup_GetPresent(p,a,b) (p)->lpVtbl->GetPresent(p,a,b)
#define ID3DPresentGroup_CreateAdditionalPresent(p,a,b) (p)->lpVtbl->CreateAdditionalPresent(p,a,b)

/* Representation of an adapter group, although since this is implemented by
 * the driver, it knows nothing about the windowing system it's on */
typedef struct ID3DAdapter9Vtbl
{
    /* IUnknown */
    HRESULT (WINAPI *QueryInterface)(ID3DAdapter9 *This, REFIID riid, void **ppvObject);
    ULONG (WINAPI *AddRef)(ID3DAdapter9 *This);
    ULONG (WINAPI *Release)(ID3DAdapter9 *This);

    /* ID3DAdapter9 */
    HRESULT (WINAPI *GetAdapterIdentifier)(ID3DAdapter9 *This, DWORD Flags, D3DADAPTER_IDENTIFIER9 *pIdentifier);
    HRESULT (WINAPI *CheckDeviceType)(ID3DAdapter9 *This, D3DDEVTYPE DevType, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat, BOOL bWindowed);
    HRESULT (WINAPI *CheckDeviceFormat)(ID3DAdapter9 *This, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, DWORD Usage, D3DRESOURCETYPE RType, D3DFORMAT CheckFormat);
    HRESULT (WINAPI *CheckDeviceMultiSampleType)(ID3DAdapter9 *This, D3DDEVTYPE DeviceType, D3DFORMAT SurfaceFormat, BOOL Windowed, D3DMULTISAMPLE_TYPE MultiSampleType, DWORD *pQualityLevels);
    HRESULT (WINAPI *CheckDepthStencilMatch)(ID3DAdapter9 *This, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, D3DFORMAT RenderTargetFormat, D3DFORMAT DepthStencilFormat);
    HRESULT (WINAPI *CheckDeviceFormatConversion)(ID3DAdapter9 *This, D3DDEVTYPE DeviceType, D3DFORMAT SourceFormat, D3DFORMAT TargetFormat);
    HRESULT (WINAPI *GetDeviceCaps)(ID3DAdapter9 *This, D3DDEVTYPE DeviceType, D3DCAPS9 *pCaps);
    HRESULT (WINAPI *CreateDevice)(ID3DAdapter9 *This, UINT RealAdapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, IDirect3D9 *pD3D9, ID3DPresentGroup *pPresentationFactory, IDirect3DDevice9 **ppReturnedDeviceInterface);
    HRESULT (WINAPI *CreateDeviceEx)(ID3DAdapter9 *This, UINT RealAdapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, IDirect3D9Ex *pD3D9Ex, ID3DPresentGroup *pPresentationFactory, IDirect3DDevice9Ex **ppReturnedDeviceInterface);
} ID3DAdapter9Vtbl;

struct ID3DAdapter9
{
    ID3DAdapter9Vtbl *lpVtbl;
};

/* IUnknown macros */
#define ID3DAdapter9_QueryInterface(p,a,b) (p)->lpVtbl->QueryInterface(p,a,b)
#define ID3DAdapter9_AddRef(p) (p)->lpVtbl->AddRef(p)
#define ID3DAdapter9_Release(p) (p)->lpVtbl->Release(p)
/* ID3DAdapter9 macros */
#define ID3DAdapter9_GetAdapterIdentifier(p,a,b) (p)->lpVtbl->GetAdapterIdentifier(p,a,b)
#define ID3DAdapter9_CheckDeviceType(p,a,b,c,d) (p)->lpVtbl->CheckDeviceType(p,a,b,c,d)
#define ID3DAdapter9_CheckDeviceFormat(p,a,b,c,d,e) (p)->lpVtbl->CheckDeviceFormat(p,a,b,c,d,e)
#define ID3DAdapter9_CheckDeviceMultiSampleType(p,a,b,c,d,e) (p)->lpVtbl->CheckDeviceMultiSampleType(p,a,b,c,d,e)
#define ID3DAdapter9_CheckDepthStencilMatch(p,a,b,c,d) (p)->lpVtbl->CheckDepthStencilMatch(p,a,b,c,d)
#define ID3DAdapter9_CheckDeviceFormatConversion(p,a,b,c) (p)->lpVtbl->CheckDeviceFormatConversion(p,a,b,c)
#define ID3DAdapter9_GetDeviceCaps(p,a,b) (p)->lpVtbl->GetDeviceCaps(p,a,b)
#define ID3DAdapter9_CreateDevice(p,a,b,c,d,e,f,g) (p)->lpVtbl->CreateDevice(p,a,b,c,d,e,f,g)
#define ID3DAdapter9_CreateDeviceEx(p,a,b,c,d,e,f,g) (p)->lpVtbl->CreateDeviceEx(p,a,b,c,d,e,f,g)

#else /* __cplusplus */

struct ID3DPresent : public IUnknown
{
    HRESULT WINAPI GetPresentParameters(D3DPRESENT_PARAMETERS *pPresentationParameters);
    HRESULT WINAPI GetBuffer(HWND hWndOverride, void *pBuffer, const RECT *pDestRect, RECT *pRect, RGNDATA **ppRegion);
    HRESULT WINAPI GetFrontBuffer(void *pBuffer);
    HRESULT WINAPI Present(DWORD Flags);
    HRESULT WINAPI GetRasterStatus(D3DRASTER_STATUS *pRasterStatus);
    HRESULT WINAPI GetDisplayMode(D3DDISPLAYMODEEX *pMode);
    HRESULT WINAPI GetPresentStats(D3DPRESENTSTATS *pStats);
};

struct ID3DPresentGroup : public IUnknown
{
    UINT WINAPI GetMultiheadCount();
    HRESULT WINAPI GetPresent(UINT Index, ID3DPresent **ppPresent);
    HRESULT WINAPI CreateAdditionalPresent(D3DPRESENT_PARAMETERS *pPresentationParameters, ID3DPresent **ppPresent);
};

struct ID3DAdapter9 : public IUnknown
{
    HRESULT WINAPI GetAdapterIdentifier(DWORD Flags, D3DADAPTER_IDENTIFIER9 *pIdentifier);
    HRESULT WINAPI CheckDeviceType(D3DDEVTYPE DevType, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat, BOOL bWindowed);
    HRESULT WINAPI CheckDeviceFormat(D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, DWORD Usage, D3DRESOURCETYPE RType, D3DFORMAT CheckFormat);
    HRESULT WINAPI CheckDeviceMultiSampleType(D3DDEVTYPE DeviceType, D3DFORMAT SurfaceFormat, BOOL Windowed, D3DMULTISAMPLE_TYPE MultiSampleType, DWORD *pQualityLevels);
    HRESULT WINAPI CheckDepthStencilMatch(D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, D3DFORMAT RenderTargetFormat, D3DFORMAT DepthStencilFormat);
    HRESULT WINAPI CheckDeviceFormatConversion(D3DDEVTYPE DeviceType, D3DFORMAT SourceFormat, D3DFORMAT TargetFormat);
    HRESULT WINAPI GetDeviceCaps(D3DDEVTYPE DeviceType, D3DCAPS9 *pCaps);
    HRESULT WINAPI CreateDevice(UINT RealAdapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, IDirect3D9 *pD3D9, ID3DPresentGroup *pPresentationFactory, IDirect3DDevice9 **ppReturnedDeviceInterface);
    HRESULT WINAPI CreateDeviceEx(UINT RealAdapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, IDirect3D9Ex *pD3D9Ex, ID3DPresentGroup *pPresentationFactory, IDirect3DDevice9Ex **ppReturnedDeviceInterface);
};

#endif /* __cplusplus */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* acquire a const struct D3DAdapter9* structure describing the interface
 * queried. See  */
const void * WINAPI
D3DAdapter9GetProc( const char *name );

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _D3DADAPTER9_H_ */
