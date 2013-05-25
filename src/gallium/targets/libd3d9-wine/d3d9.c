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

#include "d3d9.h"
#include "d3ddrm.h"
#include "d3dadapter9.h"

#include "debug.h"
#include "driver.h"
#include "guid.h"

/* this represents a snapshot taken at the moment of creation */
struct output
{
    D3DDISPLAYROTATION rotation; /* current rotation */
    D3DDISPLAYMODEEX *modes;
    unsigned nmodes;
    unsigned nmodesalloc;
    unsigned current; /* current mode num */

    HMONITOR monitor;
};

struct adapter_group
{
    struct output *outputs;
    unsigned noutputs;
    unsigned noutputsalloc;

    /* override driver provided DeviceName with this to homogenize device names
     * with wine */
    WCHAR devname[32];

    /* driver stuff */
    ID3DAdapter9 *adapter;
};

struct adapter_map
{
    unsigned group;
    unsigned master;
};

struct Nine9Ex
{
    /* COM vtable */
    void *vtable;
    /* IUnknown reference count */
    UINT refs;

    /* adapter groups and mappings */
    struct adapter_group *groups;
    struct adapter_map *map;
    unsigned nadapters;
    unsigned ngroups;
    unsigned ngroupsalloc;

    /* Wine backend */
    ID3DWineDriver *driver;

    /* true if it implements IDirect3D9Ex */
    boolean ex;
};

/* convenience wrapper for calls into ID3D9Adapter */
#define ADAPTER_PROC(name, ...) \
    ID3DAdapter9_##name(This->groups[This->map[Adapter].group].adapter, \
                        ## __VA_ARGS__)

#define ADAPTER_OUTPUT \
    This->groups[This->map[Adapter].group].outputs[Adapter-This->map[Adapter].master]

static HRESULT WINAPI
Nine9Ex_CheckDeviceFormat( struct Nine9Ex *This,
                           UINT Adapter,
                           D3DDEVTYPE DeviceType,
                           D3DFORMAT AdapterFormat,
                           DWORD Usage,
                           D3DRESOURCETYPE RType,
                           D3DFORMAT CheckFormat );

static HRESULT WINAPI
Nine9Ex_QueryInterface( struct Nine9Ex *This,
                        REFIID riid,
                        void **ppvObject )
{
    if (!ppvObject) { return E_POINTER; }
    if ((GUID_equal(&IID_IDirect3D9Ex, riid) && This->ex) ||
         GUID_equal(&IID_IDirect3D9, riid) ||
         GUID_equal(&IID_IUnknown, riid)) {
        *ppvObject = This;
        return S_OK;
    }
    _WARNING("%s: QueryInterface failed.\n", __FUNCTION__);
    return E_NOINTERFACE;
}

static ULONG WINAPI
Nine9Ex_AddRef( struct Nine9Ex *This )
{
    return ++This->refs;
}

static ULONG WINAPI
Nine9Ex_Release( struct Nine9Ex *This )
{
    if (--This->refs == 0) {
        /* dtor */
        if (This->map) {
            HeapFree(GetProcessHeap(), 0, This->map);
        }
        if (This->groups) {
            int i, j;
            for (i = 0; i < This->ngroups; ++i) {
                if (This->groups[i].outputs) {
                    for (j = 0; j < This->groups[i].noutputs; ++j) {
                        if (This->groups[i].outputs[j].modes) {
                            HeapFree(GetProcessHeap(), 0,
                                     This->groups[i].outputs[j].modes);
                        }
                    }
                    HeapFree(GetProcessHeap(), 0, This->groups[i].outputs);
                }

                if (This->groups[i].adapter) {
                    ID3DAdapter9_Release(This->groups[i].adapter);
                }
            }
            HeapFree(GetProcessHeap(), 0, This->groups);
        }
        HeapFree(GetProcessHeap(), 0, This);
        return 0;
    }
    return This->refs;
}

static HRESULT WINAPI
Nine9Ex_RegisterSoftwareDevice( struct Nine9Ex *This,
                                void *pInitializeFunction )
{
    STUB(D3DERR_INVALIDCALL);
}

static UINT WINAPI
Nine9Ex_GetAdapterCount( struct Nine9Ex *This )
{
    _MESSAGE("%s: returning %u\n", __FUNCTION__, This->nadapters);
    return This->nadapters;
}

static HRESULT WINAPI
Nine9Ex_GetAdapterIdentifier( struct Nine9Ex *This,
                              UINT Adapter,
                              DWORD Flags,
                              D3DADAPTER_IDENTIFIER9 *pIdentifier )
{
    HRESULT hr;

    if (Adapter >= Nine9Ex_GetAdapterCount(This)) { return D3DERR_INVALIDCALL; }

    hr = ADAPTER_PROC(GetAdapterIdentifier, Flags, pIdentifier);
    if (SUCCEEDED(hr)) {
        struct adapter_group *group = &This->groups[This->map[Adapter].group];

        /* Override the driver provided DeviceName with what Wine provided */
        ZeroMemory(pIdentifier->DeviceName, sizeof(pIdentifier->DeviceName));
        if (!WideCharToMultiByte(CP_ACP, 0, group->devname, -1,
                                 pIdentifier->DeviceName,
                                 sizeof(pIdentifier->DeviceName),
                                 NULL, NULL)) {
            /* Wine does it */
            return D3DERR_INVALIDCALL;
        }
        _MESSAGE("%s: DeviceName overriden: %s\n", __FUNCTION__,
                 pIdentifier->DeviceName);
    }
    return hr;
}

static UINT WINAPI
Nine9Ex_GetAdapterModeCount( struct Nine9Ex *This,
                             UINT Adapter,
                             D3DFORMAT Format )
{
    if (Adapter >= Nine9Ex_GetAdapterCount(This)) {
        _WARNING("%s: Adapter %u does not exist.\n", __FUNCTION__, Adapter);
        return 0;
    }
    if (FAILED(Nine9Ex_CheckDeviceFormat(This, Adapter, D3DDEVTYPE_HAL,
                                         Format, D3DUSAGE_RENDERTARGET,
                                         D3DRTYPE_SURFACE, Format))) {
        _WARNING("%s: DeviceFormat not available.\n", __FUNCTION__);
        return 0;
    }

    _MESSAGE("%s: %u modes.\n", __FUNCTION__, ADAPTER_OUTPUT.nmodes);
    return ADAPTER_OUTPUT.nmodes;
}

static HRESULT WINAPI
Nine9Ex_EnumAdapterModes( struct Nine9Ex *This,
                          UINT Adapter,
                          D3DFORMAT Format,
                          UINT Mode,
                          D3DDISPLAYMODE *pMode )
{
    if (Adapter >= Nine9Ex_GetAdapterCount(This)) {
        _WARNING("%s: Adapter %u does not exist.\n", __FUNCTION__, Adapter);
        return D3DERR_INVALIDCALL;
    }
    if (FAILED(Nine9Ex_CheckDeviceFormat(This, Adapter, D3DDEVTYPE_HAL,
                                         Format, D3DUSAGE_RENDERTARGET,
                                         D3DRTYPE_SURFACE, Format))) {
        _WARNING("%s: DeviceFormat not available.\n", __FUNCTION__);
        return D3DERR_NOTAVAILABLE;
    }

    if (Mode >= ADAPTER_OUTPUT.nmodes) {
        _WARNING("%s: Mode %u does not exist.\n", __FUNCTION__, Mode);
        return D3DERR_INVALIDCALL;
    }

    pMode->Width = ADAPTER_OUTPUT.modes[Mode].Width;
    pMode->Height = ADAPTER_OUTPUT.modes[Mode].Height;
    pMode->RefreshRate = ADAPTER_OUTPUT.modes[Mode].RefreshRate;
    pMode->Format = Format;

    _MESSAGE("%s: returning mode Width=%u Height=%u RefreshRate=%u Format=%u\n",
             __FUNCTION__,
             pMode->Width, pMode->Height, pMode->RefreshRate, pMode->Format);

    return D3D_OK;
}

static HRESULT WINAPI
Nine9Ex_GetAdapterDisplayMode( struct Nine9Ex *This,
                               UINT Adapter,
                               D3DDISPLAYMODE *pMode )
{
    UINT Mode;

    if (Adapter >= Nine9Ex_GetAdapterCount(This)) {
        _WARNING("%s: Adapter %u does not exist.\n", __FUNCTION__, Adapter);
        return D3DERR_INVALIDCALL;
    }

    Mode = ADAPTER_OUTPUT.current;
    pMode->Width = ADAPTER_OUTPUT.modes[Mode].Width;
    pMode->Height = ADAPTER_OUTPUT.modes[Mode].Height;
    pMode->RefreshRate = ADAPTER_OUTPUT.modes[Mode].RefreshRate;
    pMode->Format = ADAPTER_OUTPUT.modes[Mode].Format;

    return D3D_OK;
}

static HRESULT WINAPI
Nine9Ex_CheckDeviceType( struct Nine9Ex *This,
                         UINT Adapter,
                         D3DDEVTYPE DevType,
                         D3DFORMAT AdapterFormat,
                         D3DFORMAT BackBufferFormat,
                         BOOL bWindowed )
{
    if (Adapter >= Nine9Ex_GetAdapterCount(This)) { return D3DERR_INVALIDCALL; }
    return ADAPTER_PROC(CheckDeviceType,
                        DevType, AdapterFormat, BackBufferFormat, bWindowed);
}

static HRESULT WINAPI
Nine9Ex_CheckDeviceFormat( struct Nine9Ex *This,
                           UINT Adapter,
                           D3DDEVTYPE DeviceType,
                           D3DFORMAT AdapterFormat,
                           DWORD Usage,
                           D3DRESOURCETYPE RType,
                           D3DFORMAT CheckFormat )
{
    if (Adapter >= Nine9Ex_GetAdapterCount(This)) { return D3DERR_INVALIDCALL; }
    return ADAPTER_PROC(CheckDeviceFormat,
                        DeviceType, AdapterFormat, Usage, RType, CheckFormat);
}

static HRESULT WINAPI
Nine9Ex_CheckDeviceMultiSampleType( struct Nine9Ex *This,
                                    UINT Adapter,
                                    D3DDEVTYPE DeviceType,
                                    D3DFORMAT SurfaceFormat,
                                    BOOL Windowed,
                                    D3DMULTISAMPLE_TYPE MultiSampleType,
                                    DWORD *pQualityLevels )
{
    if (Adapter >= Nine9Ex_GetAdapterCount(This)) { return D3DERR_INVALIDCALL; }
    return ADAPTER_PROC(CheckDeviceMultiSampleType, DeviceType, SurfaceFormat,
                        Windowed, MultiSampleType, pQualityLevels);
}

static HRESULT WINAPI
Nine9Ex_CheckDepthStencilMatch( struct Nine9Ex *This,
                                UINT Adapter,
                                D3DDEVTYPE DeviceType,
                                D3DFORMAT AdapterFormat,
                                D3DFORMAT RenderTargetFormat,
                                D3DFORMAT DepthStencilFormat )
{
    if (Adapter >= Nine9Ex_GetAdapterCount(This)) { return D3DERR_INVALIDCALL; }
    return ADAPTER_PROC(CheckDepthStencilMatch, DeviceType, AdapterFormat,
                        RenderTargetFormat, DepthStencilFormat);
}

static HRESULT WINAPI
Nine9Ex_CheckDeviceFormatConversion( struct Nine9Ex *This,
                                     UINT Adapter,
                                     D3DDEVTYPE DeviceType,
                                     D3DFORMAT SourceFormat,
                                     D3DFORMAT TargetFormat )
{
    if (Adapter >= Nine9Ex_GetAdapterCount(This)) { return D3DERR_INVALIDCALL; }
    return ADAPTER_PROC(CheckDeviceFormatConversion,
                        DeviceType, SourceFormat, TargetFormat);
}

static HRESULT WINAPI
Nine9Ex_GetDeviceCaps( struct Nine9Ex *This,
                       UINT Adapter,
                       D3DDEVTYPE DeviceType,
                       D3DCAPS9 *pCaps )
{
    HRESULT hr;

    if (Adapter >= Nine9Ex_GetAdapterCount(This)) { return D3DERR_INVALIDCALL; }

    hr = ADAPTER_PROC(GetDeviceCaps, DeviceType, pCaps);
    if (FAILED(hr)) { return hr; }

    pCaps->MasterAdapterOrdinal = This->map[Adapter].master;
    pCaps->AdapterOrdinalInGroup = Adapter-This->map[Adapter].master;
    pCaps->NumberOfAdaptersInGroup =
        This->groups[This->map[Adapter].group].noutputs;

    return hr;
}

static HMONITOR WINAPI
Nine9Ex_GetAdapterMonitor( struct Nine9Ex *This,
                           UINT Adapter )
{
    if (Adapter >= Nine9Ex_GetAdapterCount(This)) { return (HMONITOR)0; }
    return (HMONITOR)ADAPTER_OUTPUT.monitor;
}

static HRESULT WINAPI
Nine9Ex_CreateDevice( struct Nine9Ex *This,
                      UINT Adapter,
                      D3DDEVTYPE DeviceType,
                      HWND hFocusWindow,
                      DWORD BehaviorFlags,
                      D3DPRESENT_PARAMETERS *pPresentationParameters,
                      IDirect3DDevice9 **ppReturnedDeviceInterface )
{
    ID3DPresentFactory *present;
    HRESULT hr;
    unsigned nparams;

    _MESSAGE("%s\n", __FUNCTION__);

    if (Adapter >= Nine9Ex_GetAdapterCount(This)) {
        _WARNING("%s: Adapter %u does not exist.\n", __FUNCTION__, Adapter);
        return D3DERR_INVALIDCALL;
    }

    nparams = (BehaviorFlags & D3DCREATE_ADAPTERGROUP_DEVICE) ?
              This->groups[This->map[Adapter].group].noutputs : 1;
    hr = ID3DWineDriver_CreatePresentFactory(This->driver, hFocusWindow,
                                             pPresentationParameters, nparams,
                                             &present);
    if (FAILED(hr)) {
        _WARNING("%s: Failed to create PresentFactory.\n", __FUNCTION__);
        return hr;
    }

    hr = ADAPTER_PROC(CreateDevice, Adapter, DeviceType, hFocusWindow,
                      BehaviorFlags, (IDirect3D9 *)This, present,
                      ppReturnedDeviceInterface);
    if (FAILED(hr)) {
        _WARNING("%s: ADAPTER_PROC failed.\n", __FUNCTION__);
        ID3DPresentFactory_Release(present);
    }

    return hr;
}

static UINT WINAPI
Nine9Ex_GetAdapterModeCountEx( struct Nine9Ex *This,
                               UINT Adapter,
                               const D3DDISPLAYMODEFILTER *pFilter )
{
    return 1;
}

static HRESULT WINAPI
Nine9Ex_EnumAdapterModesEx( struct Nine9Ex *This,
                            UINT Adapter,
                            const D3DDISPLAYMODEFILTER *pFilter,
                            UINT Mode,
                            D3DDISPLAYMODEEX *pMode )
{
    STUB(D3DERR_INVALIDCALL);
}

static HRESULT WINAPI
Nine9Ex_GetAdapterDisplayModeEx( struct Nine9Ex *This,
                                 UINT Adapter,
                                 D3DDISPLAYMODEEX *pMode,
                                 D3DDISPLAYROTATION *pRotation )
{
    STUB(D3DERR_INVALIDCALL);
}

static HRESULT WINAPI
Nine9Ex_CreateDeviceEx( struct Nine9Ex *This,
                        UINT Adapter,
                        D3DDEVTYPE DeviceType,
                        HWND hFocusWindow,
                        DWORD BehaviorFlags,
                        D3DPRESENT_PARAMETERS *pPresentationParameters,
                        D3DDISPLAYMODEEX *pFullscreenDisplayMode,
                        IDirect3DDevice9Ex **ppReturnedDeviceInterface )
{
    STUB(D3DERR_INVALIDCALL);
}

static HRESULT WINAPI
Nine9Ex_GetAdapterLUID( struct Nine9Ex *This,
                        UINT Adapter,
                        LUID *pLUID )
{
    STUB(D3DERR_INVALIDCALL);
}

static INLINE struct adapter_group *
add_group( struct Nine9Ex *This )
{
    if (This->ngroups >= This->ngroupsalloc) {
        void *r;

        if (This->ngroupsalloc == 0) {
            This->ngroupsalloc = 2;
            r = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                          This->ngroupsalloc*sizeof(struct adapter_group));
        } else {
            This->ngroupsalloc <<= 1;
            r = HeapReAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, This->groups,
                        This->ngroupsalloc*sizeof(struct adapter_group));
        }

        if (!r) { return NULL; }
        This->groups = r;
    }

    return &This->groups[This->ngroups++];
}

static INLINE void
remove_group( struct Nine9Ex *This )
{
    struct adapter_group *group = &This->groups[This->ngroups-1];
    int i;

    for (i = 0; i < group->noutputs; ++i) {
        HeapFree(GetProcessHeap(), 0, group->outputs[i].modes);
    }
    HeapFree(GetProcessHeap(), 0, group->outputs);

    ZeroMemory(group, sizeof(struct adapter_group));
    This->ngroups--;
}

static INLINE struct output *
add_output( struct Nine9Ex *This )
{
    struct adapter_group *group = &This->groups[This->ngroups-1];

    if (group->noutputs >= group->noutputsalloc) {
        void *r;

        if (group->noutputsalloc == 0) {
            group->noutputsalloc = 2;
            r = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                          group->noutputsalloc*sizeof(struct output));
        } else {
            group->noutputsalloc <<= 1;
            r = HeapReAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, group->outputs,
                            group->noutputsalloc*sizeof(struct output));
        }

        if (!r) { return NULL; }
        group->outputs = r;
    }

    return &group->outputs[group->noutputs++];
}

static INLINE void
remove_output( struct Nine9Ex *This )
{
    struct adapter_group *group = &This->groups[This->ngroups-1];
    struct output *out = &group->outputs[group->noutputs-1];

    HeapFree(GetProcessHeap(), 0, out->modes);

    ZeroMemory(out, sizeof(struct output));
    group->noutputs--;
}

static INLINE D3DDISPLAYMODEEX *
add_mode( struct Nine9Ex *This )
{
    struct adapter_group *group = &This->groups[This->ngroups-1];
    struct output *out = &group->outputs[group->noutputs-1];

    if (out->nmodes >= out->nmodesalloc) {
        void *r;

        if (out->nmodesalloc == 0) {
            out->nmodesalloc = 8;
            r = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                          out->nmodesalloc*sizeof(struct D3DDISPLAYMODEEX));
        } else {
            out->nmodesalloc <<= 1;
            r = HeapReAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, out->modes,
                            out->nmodesalloc*sizeof(struct D3DDISPLAYMODEEX));
        }

        if (!r) { return NULL; }
        out->modes = r;
    }

    return &out->modes[out->nmodes++];
}

static INLINE void
remove_mode( struct Nine9Ex *This )
{
    struct adapter_group *group = &This->groups[This->ngroups-1];
    struct output *out = &group->outputs[group->noutputs-1];
    out->nmodes--;
}

#define DM_INTERLACED 2

static INLINE HRESULT
fill_groups( struct Nine9Ex *This )
{
    DISPLAY_DEVICEW dd;
    DEVMODEW dm;
    POINT pt;
    HDC hdc;
    HRESULT hr;
    int i, j, k;

    WCHAR wdisp[] = {'D','I','S','P','L','A','Y',0};

    ZeroMemory(&dd, sizeof(dd));
    ZeroMemory(&dm, sizeof(dm));
    dd.cb = sizeof(dd);
    dm.dmSize = sizeof(dm);

    for (i = 0; EnumDisplayDevicesW(NULL, i, &dd, 0); ++i) {
        struct adapter_group *group = add_group(This);
        if (!group) { OOM(); }

        hdc = CreateDCW(wdisp, dd.DeviceName, NULL, NULL);
        if (!hdc) {
            remove_group(This);
            _WARNING("Unable to create DC for display %d.\n", i);
            goto end_group;
        }

        hr = ID3DWineDriver_CreateAdapter9(This->driver, hdc, &group->adapter);
        DeleteDC(hdc);
        if (FAILED(hr)) {
            remove_group(This);
            goto end_group;
        }

        CopyMemory(group->devname, dd.DeviceName, sizeof(group->devname));
        for (j = 0; EnumDisplayDevicesW(group->devname, j, &dd, 0); ++j) {
            struct output *out = add_output(This);
            boolean orient = FALSE, monit = FALSE;
            if (!out) { OOM(); }

            for (k = 0; EnumDisplaySettingsExW(dd.DeviceName, k, &dm, 0); ++k) {
                D3DDISPLAYMODEEX *mode = add_mode(This);
                if (!out) { OOM(); }

                mode->Size = sizeof(D3DDISPLAYMODEEX);
                mode->Width = dm.dmPelsWidth;
                mode->Height = dm.dmPelsHeight;
                mode->RefreshRate = dm.dmDisplayFrequency;
                mode->ScanLineOrdering =
                    (dm.dmDisplayFlags & DM_INTERLACED) ?
                        D3DSCANLINEORDERING_INTERLACED :
                        D3DSCANLINEORDERING_PROGRESSIVE;

                switch (dm.dmBitsPerPel) {
                    case 32: mode->Format = D3DFMT_X8R8G8B8; break;
                    case 24: mode->Format = D3DFMT_R8G8B8; break;
                    case 16: mode->Format = D3DFMT_R5G6B5; break;
                    case 8:
                        remove_mode(This);
                        goto end_mode;

                    default:
                        remove_mode(This);
                        _WARNING("Unknown format (%u bpp) in display %d, "
                                 "monitor %d, mode %d.\n", dm.dmBitsPerPel,
                                 i, j, k);
                        goto end_mode;
                }

                if (!orient) {
                    switch (dm.dmDisplayOrientation) {
                        case DMDO_DEFAULT:
                            out->rotation = D3DDISPLAYROTATION_IDENTITY;
                            break;

                        case DMDO_90:
                            out->rotation = D3DDISPLAYROTATION_90;
                            break;

                        case DMDO_180:
                            out->rotation = D3DDISPLAYROTATION_180;
                            break;

                        case DMDO_270:
                            out->rotation = D3DDISPLAYROTATION_270;
                            break;

                        default:
                            remove_output(This);
                            _WARNING("Unknown display rotation in display %d, "
                                    "monitor %d\n", i, j, k);
                            goto end_output;
                    }
                    orient = TRUE;
                }

                if (!monit) {
                    pt.x = dm.dmPosition.x;
                    pt.y = dm.dmPosition.y;
                    out->monitor = MonitorFromPoint(pt, 0);
                    if (!out->monitor) {
                        remove_output(This);
                        _WARNING("Unable to get monitor handle for display %d, "
                                "monitor %d.\n", i, j);
                        goto end_output;
                    }
                    monit = TRUE;
                }

end_mode:
                ZeroMemory(&dm, sizeof(dm));
                dm.dmSize = sizeof(dm);
            }

end_output:
            ZeroMemory(&dd, sizeof(dd));
            dd.cb = sizeof(dd);
        }

end_group:
        ZeroMemory(&dd, sizeof(dd));
        dd.cb = sizeof(dd);
    }

    return D3D_OK;
}

static IDirect3D9ExVtbl Nine9Ex_vtable = {
    (void *)Nine9Ex_QueryInterface,
    (void *)Nine9Ex_AddRef,
    (void *)Nine9Ex_Release,
    (void *)Nine9Ex_RegisterSoftwareDevice,
    (void *)Nine9Ex_GetAdapterCount,
    (void *)Nine9Ex_GetAdapterIdentifier,
    (void *)Nine9Ex_GetAdapterModeCount,
    (void *)Nine9Ex_EnumAdapterModes,
    (void *)Nine9Ex_GetAdapterDisplayMode,
    (void *)Nine9Ex_CheckDeviceType,
    (void *)Nine9Ex_CheckDeviceFormat,
    (void *)Nine9Ex_CheckDeviceMultiSampleType,
    (void *)Nine9Ex_CheckDepthStencilMatch,
    (void *)Nine9Ex_CheckDeviceFormatConversion,
    (void *)Nine9Ex_GetDeviceCaps,
    (void *)Nine9Ex_GetAdapterMonitor,
    (void *)Nine9Ex_CreateDevice,
    (void *)Nine9Ex_GetAdapterModeCountEx,
    (void *)Nine9Ex_EnumAdapterModesEx,
    (void *)Nine9Ex_GetAdapterDisplayModeEx,
    (void *)Nine9Ex_CreateDeviceEx,
    (void *)Nine9Ex_GetAdapterLUID
};

/* XXX siphon into different lib */
HRESULT
D3DWineDriverCreate( ID3DWineDriver **ppDriver );

static HRESULT
Nine9Ex_new( boolean ex,
             IDirect3D9Ex **ppOut )
{
    struct Nine9Ex *This = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                                     sizeof(struct Nine9Ex));
    HRESULT hr;
    int i, j, k = 0;

    _MESSAGE("%s(ex=%i, ppOut=%p)\n", __FUNCTION__, ex, ppOut);

    if (!This) { OOM(); }

    This->vtable = &Nine9Ex_vtable;
    This->refs = 1;

    hr = D3DWineDriverCreate(&This->driver);
    if (FAILED(hr)) {
        Nine9Ex_Release(This);
        return hr;
    }

    hr = fill_groups(This);
    if (FAILED(hr)) {
        Nine9Ex_Release(This);
        return hr;
    }

    /* map absolute adapter IDs with internal adapters */
    for (i = k = 0; i < This->ngroups; ++i) {
        for (j = 0; j < This->groups[i].noutputs; ++j) {
            This->nadapters++;
        }
    }
    if (This->nadapters == 0) {
        _ERROR("No available adapters in system.\n");
        Nine9Ex_Release(This);
        return D3DERR_NOTAVAILABLE;
    }

    This->map = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                          This->nadapters*sizeof(struct adapter_map));
    if (!This->map) {
        Nine9Ex_Release(This);
        OOM();
    }
    for (i = k = 0; i < This->ngroups; ++i) {
        for (j = 0; j < This->groups[i].noutputs; ++j) {
            This->map[k].master = k-j;
            This->map[k].group = i;
            ++k;
        }
    }

    *ppOut = (IDirect3D9Ex *)This;
    return D3D_OK;
}

IDirect3D9 *WINAPI
Direct3DCreate9( UINT SDKVersion )
{
    IDirect3D9Ex *d3d = NULL;
    HRESULT hr;

    hr = Nine9Ex_new(FALSE, &d3d);
    return FAILED(hr) ? NULL : (IDirect3D9 *)d3d;
}

HRESULT WINAPI
Direct3DCreate9Ex( UINT SDKVersion,
                   IDirect3D9Ex **ppD3D9 )
{
    return Nine9Ex_new(TRUE, ppD3D9);
}

static int D3DPERF_event_level = 0;

void *WINAPI
Direct3DShaderValidatorCreate9( void )
{
    static boolean first = TRUE;

    _WARNING("%s: not implemented, returning NULL.\n", __FUNCTION__);

    if (first) {
        first = FALSE;
    }
    return NULL;
}

int WINAPI
D3DPERF_BeginEvent( D3DCOLOR color,
                    LPCWSTR name )
{
    return D3DPERF_event_level++;
}

int WINAPI
D3DPERF_EndEvent( void )
{
    return --D3DPERF_event_level;
}

DWORD WINAPI
D3DPERF_GetStatus( void )
{
    return 0;
}

void WINAPI
D3DPERF_SetOptions( DWORD options )
{
}

BOOL WINAPI
D3DPERF_QueryRepeatFrame( void )
{
    return FALSE;
}

void WINAPI
D3DPERF_SetMarker( D3DCOLOR color,
                   LPCWSTR name )
{
}

void WINAPI
D3DPERF_SetRegion( D3DCOLOR color,
                   LPCWSTR name )
{
}

void WINAPI
DebugSetMute( void )
{
}
