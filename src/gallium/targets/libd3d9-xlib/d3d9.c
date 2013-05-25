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
#include "d3d9xlib.h"
#include "d3ddrm.h"
#include "d3dadapter9.h"

#include "present.h"
#include "guid.h"
#include "debug.h"

#include <X11/Xlib.h>
#include <X11/Xlib-xcb.h>
#include <xcb/xcb.h>
#include <xcb/dri2.h>
#include <xcb/randr.h>
#include <xcb/xfixes.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>

/* Only the header is needed. The library itself is never used */
#include <drm/drm.h>

#define DRIVER_MAJOR 0

struct display_mode
{
    D3DDISPLAYMODEEX mode;
    xcb_randr_mode_t xid;
};

/* this represents a snapshot taken at the moment of creation */
struct output
{
    D3DDISPLAYROTATION rotation; /* current rotation */
    struct display_mode *modes;
    unsigned nmodes;
    unsigned current; /* current mode num */

    xcb_randr_output_t xid; /* XID of output */

    union {
        xcb_randr_get_output_info_cookie_t info_cookie;
        xcb_randr_get_crtc_info_cookie_t crtc_cookie;
    } loader;
};

struct adapter_group
{
    struct output *outputs;
    unsigned noutputs;

    /* driver stuff */
    ID3DAdapter9 *adapter;
    void *handle;
    int fd;

    union {
        struct {
            xcb_dri2_connect_cookie_t cookie;
            xcb_window_t root;
        } dri2_con;
        struct {
            xcb_dri2_authenticate_cookie_t cookie;
            xcb_window_t root;
            struct D3DAdapter9DescriptorDRM *descriptor;
        } dri2_auth;
        struct {
            xcb_randr_get_screen_resources_current_cookie_t cookie;
        } randr;
    } loader;
};

struct adapter_map
{
    unsigned group;
    unsigned master;
};

struct Nine9Ex
{
    void *vtable; /* COM vtable */
    UINT refs; /* IUnknown reference count */

    /* adapter groups and mappings */
    struct adapter_group *groups;
    struct adapter_map *map;
    unsigned nadapters;
    unsigned ngroups;

    /* DRI2 stuff */
    xcb_connection_t *c;
    unsigned dri2_minor;

    boolean ex; /* true if this was created as a IDirect3D9Ex */
};

/* prototype */
static HRESULT WINAPI
Nine9Ex_CheckDeviceFormat( struct Nine9Ex *This,
                           UINT Adapter,
                           D3DDEVTYPE DeviceType,
                           D3DFORMAT AdapterFormat,
                           DWORD Usage,
                           D3DRESOURCETYPE RType,
                           D3DFORMAT CheckFormat );

/* convenience wrapper for calls into ID3D9Adapter */
#define ADAPTER_PROC(name, ...) \
    ID3DAdapter9_##name(This->groups[This->map[Adapter].group].adapter, \
                        ## __VA_ARGS__)

#define ADAPTER_OUTPUT \
    This->groups[This->map[Adapter].group].outputs[Adapter-This->map[Adapter].master]

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
    *ppvObject = NULL;
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
            free(This->map);
        }
        if (This->groups) {
            int i, j;
            for (i = 0; i < This->ngroups; ++i) {
                if (This->groups[i].outputs) {
                    for (j = 0; j < This->groups[i].noutputs; ++j) {
                        if (This->groups[i].outputs[j].modes) {
                            free(This->groups[i].outputs[j].modes);
                        }
                    }
                    free(This->groups[i].outputs);
                }

                if (This->groups[i].adapter) {
                    ID3DAdapter9_Release(This->groups[i].adapter);
                }
                if (This->groups[i].handle) {
                    dlclose(This->groups[i].handle);
                }
                if (This->groups[i].fd >= 0) {
                    close(This->groups[i].fd);
                }
            }
            free(This->groups);
        }
        free(This);
        return 0;
    }
    return This->refs;
}

static HRESULT WINAPI
Nine9Ex_RegisterSoftwareDevice( struct Nine9Ex *This,
                                void *pInitializeFunction )
{
    return D3DERR_INVALIDCALL;
}

static UINT WINAPI
Nine9Ex_GetAdapterCount( struct Nine9Ex *This )
{
    return This->nadapters;
}

static HRESULT WINAPI
Nine9Ex_GetAdapterIdentifier( struct Nine9Ex *This,
                              UINT Adapter,
                              DWORD Flags,
                              D3DADAPTER_IDENTIFIER9 *pIdentifier )
{
    if (Adapter >= Nine9Ex_GetAdapterCount(This)) { return D3DERR_INVALIDCALL; }
    return ADAPTER_PROC(GetAdapterIdentifier, Flags, pIdentifier);
}

static UINT WINAPI
Nine9Ex_GetAdapterModeCount( struct Nine9Ex *This,
                             UINT Adapter,
                             D3DFORMAT Format )
{
    if (Adapter >= Nine9Ex_GetAdapterCount(This)) { return 0; }
    if (FAILED(Nine9Ex_CheckDeviceFormat(This, Adapter, D3DDEVTYPE_HAL,
                                         Format, D3DUSAGE_RENDERTARGET,
                                         D3DRTYPE_SURFACE, Format))) {
        return 0;
    }

    return ADAPTER_OUTPUT.nmodes;
}

static HRESULT WINAPI
Nine9Ex_EnumAdapterModes( struct Nine9Ex *This,
                          UINT Adapter,
                          D3DFORMAT Format,
                          UINT Mode,
                          D3DDISPLAYMODE *pMode )
{
    if (Adapter >= Nine9Ex_GetAdapterCount(This)) { return D3DERR_INVALIDCALL; }
    if (FAILED(Nine9Ex_CheckDeviceFormat(This, Adapter, D3DDEVTYPE_HAL,
                                         Format, D3DUSAGE_RENDERTARGET,
                                         D3DRTYPE_SURFACE, Format))) {
        return D3DERR_NOTAVAILABLE;
    }

    if (Mode >= ADAPTER_OUTPUT.nmodes) { return D3DERR_INVALIDCALL; }

    pMode->Width = ADAPTER_OUTPUT.modes[Mode].mode.Width;
    pMode->Height = ADAPTER_OUTPUT.modes[Mode].mode.Height;
    pMode->RefreshRate = ADAPTER_OUTPUT.modes[Mode].mode.RefreshRate;
    pMode->Format = Format;

    return D3D_OK;
}

static HRESULT WINAPI
Nine9Ex_GetAdapterDisplayMode( struct Nine9Ex *This,
                               UINT Adapter,
                               D3DDISPLAYMODE *pMode )
{
    UINT Mode;

    if (Adapter >= Nine9Ex_GetAdapterCount(This)) { return D3DERR_INVALIDCALL; }

    Mode = ADAPTER_OUTPUT.current;
    pMode->Width = ADAPTER_OUTPUT.modes[Mode].mode.Width;
    pMode->Height = ADAPTER_OUTPUT.modes[Mode].mode.Height;
    pMode->RefreshRate = ADAPTER_OUTPUT.modes[Mode].mode.RefreshRate;
    pMode->Format = ADAPTER_OUTPUT.modes[Mode].mode.Format;

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
    return (HMONITOR)ADAPTER_OUTPUT.xid;
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

    if (Adapter >= Nine9Ex_GetAdapterCount(This)) { return D3DERR_INVALIDCALL; }

    nparams = (BehaviorFlags & D3DCREATE_ADAPTERGROUP_DEVICE) ?
              This->groups[This->map[Adapter].group].noutputs : 1;
    hr = NinePresentFactoryXlib_new(This->c, hFocusWindow,
                                    pPresentationParameters, nparams,
                                    This->dri2_minor, &present);
    if (FAILED(hr)) { return hr; }

    if (This->ex) {
        hr = ADAPTER_PROC(CreateDeviceEx, Adapter, DeviceType, hFocusWindow,
                          BehaviorFlags, (IDirect3D9Ex *)This, present,
                          (IDirect3DDevice9Ex **)ppReturnedDeviceInterface);
    } else {
        hr = ADAPTER_PROC(CreateDevice, Adapter, DeviceType, hFocusWindow,
                          BehaviorFlags, (IDirect3D9 *)This, present,
                          ppReturnedDeviceInterface);
    }
    if (FAILED(hr)) { ID3DPresentFactory_Release(present); }

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
    return D3DERR_INVALIDCALL;
}

static HRESULT WINAPI
Nine9Ex_GetAdapterDisplayModeEx( struct Nine9Ex *This,
                                 UINT Adapter,
                                 D3DDISPLAYMODEEX *pMode,
                                 D3DDISPLAYROTATION *pRotation )
{
    return D3DERR_INVALIDCALL;
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
    return D3DERR_INVALIDCALL;
}

static HRESULT WINAPI
Nine9Ex_GetAdapterLUID( struct Nine9Ex *This,
                        UINT Adapter,
                        LUID *pLUID )
{
    return D3DERR_INVALIDCALL;
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

/* this function does NOT check if the string length is shorter than len */
static INLINE char *
_strndup( const char *str,
          unsigned len )
{
    char *buf = malloc(len+1);
    if (!buf) { return NULL; }

    memcpy(buf, str, len);
    buf[len] = '\0';

    return buf;
}

static INLINE HRESULT
load_path( const char *name,
           unsigned namelen,
           void **handle,
           struct D3DAdapter9DescriptorDRM **descriptor )
{
    const char *search_paths[] = {
        getenv("LIBD3D9_DRIVERS_PATH"),
        getenv("LIBD3D9_DRIVERS_DIR"),
        LIBD3D9_DEFAULT_SEARCH_PATH
    };

    int i, j;
    boolean escape;

    for (i = 0; i < sizeof(search_paths)/sizeof(*search_paths); ++i) {
        escape = FALSE;
        if (search_paths[i] == NULL) { continue; }

        for (j = 0; search_paths[i][j] != '\0'; ++j) {
            if (!escape && search_paths[i][j] == '\\') {
                escape = TRUE;
                continue;
            }

            if ((search_paths[i][j+1] == ':' ||
                 search_paths[i][j+1] == '\0') && !escape) {
                void *driver;
                char *buf = malloc(j+namelen+16);
                if (!buf) { OOM(); }

                snprintf(buf, j+namelen+16, "%s%s%.*s_nine.so",
                         search_paths[i], search_paths[i][j] == '/' ? "" : "/",
                         namelen, name);

                dlerror(); /* clear dlerror before loading */
                driver = dlopen(buf, RTLD_LAZY);

                {
                    char *err = dlerror();
                    if (err) {
                        WARNING("Error opening driver `%s': %s\n",
                                buf, err);
                    }
                }

                if (driver) {
                    do {
                        /* validate it here */
                        struct D3DAdapter9DescriptorDRM *drm =
                            dlsym(driver, D3DAdapter9DescriptorDRMName);

                        if (!drm) {
                            WARNING("Error opening driver `%s': %s\n",
                                    buf, dlerror());
                            break;
                        }

                        if (drm->major_version != DRIVER_MAJOR) {
                            WARNING("Error opening driver `%s': Driver "
                                    "major version (%u) is not the same "
                                    "as library version (%u)\n",
                                    buf, drm->major_version, DRIVER_MAJOR);
                            break;
                        }

                        if (drm->create_adapter == NULL) {
                            WARNING("Error opening driver `%s': "
                                    "create_adapter == NULL\n", buf);
                        }

                        *handle = driver;
                        *descriptor = drm;

                        MESSAGE("Loaded driver `%.*s' from `%s'\n",
                                namelen, name, buf);
                        free(buf);

                        return D3D_OK;
                    } while (0);

                    dlclose(driver);
                }
                free(buf);

                search_paths[i] += j+1;
                j = -1;

                escape = FALSE;
            }
        }
    }

    WARNING("Unable to locate driver named `%.*s'\n", namelen, name);

    return D3DERR_NOTAVAILABLE;
}

static INLINE HRESULT
fill_groups( struct Nine9Ex *This,
             int default_screen )
{
    const xcb_setup_t *setup = xcb_get_setup(This->c);
    xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);
    xcb_generic_error_t *err = NULL;
    int i, j, k;

    This->ngroups = iter.rem;
    This->groups = calloc(This->ngroups, sizeof(struct adapter_group));
    if (!This->groups) { OOM(); }

    for (i = 0; i < This->ngroups; ++i) {
        /* index to sort the default screen first */
        j = (i == default_screen) ? 0 : ((i < default_screen) ? i+1 : i);

        {
            xcb_screen_t *screen = iter.data;

            This->groups[j].loader.dri2_con.root = screen->root;
            This->groups[j].loader.dri2_con.cookie =
                xcb_dri2_connect(This->c, screen->root,
                                 XCB_DRI2_DRIVER_TYPE_DRI);
        }

        xcb_screen_next(&iter);
    }

    MESSAGE("Got %d adapter group(s)\n", This->ngroups);

    /* i is the real index, j is used to skip screens we can't connect to */
    for (i = j = 0; i < This->ngroups; ++i) {
        xcb_dri2_connect_reply_t *reply =
            xcb_dri2_connect_reply(This->c,
                                   This->groups[i].loader.dri2_con.cookie,
                                   &err);
        HRESULT hr;

        if (err) {
            WARNING("DRI2Connect failed with error %hhu (major=%hhu, "
                    "minor=%hu): Unable to connect DRI2 on screen %d%s\n",
                    err->error_code, err->major_code, err->minor_code,
                    i, (i == default_screen) ? " (default)" : "");
            if (reply) { free(reply); }
            continue;
        }

        /* try to load driver and skip if we can't */
        hr = load_path(xcb_dri2_connect_driver_name(reply),
                       xcb_dri2_connect_driver_name_length(reply),
                       &This->groups[j].handle,
                       &This->groups[j].loader.dri2_auth.descriptor);
        if (FAILED(hr)) {
            free(reply);
            if (hr == E_OUTOFMEMORY) {
                free(This->groups);
                return hr;
            }
            continue;
        }

        {
            /* try to open and authenticate and open a drm adapter */
            char *path = _strndup(xcb_dri2_connect_device_name(reply),
                                  xcb_dri2_connect_device_name_length(reply));
            drm_auth_t auth;
            int fd;

            fd = open(path, O_RDWR);
            if (fd < 0) {
                WARNING("Failed to open drm fd: %s (%s)\n",
                        strerror(errno), path);
                dlclose(This->groups[j].handle);
                free(path);
                free(reply);
                continue;
            }

            /* authenticate */
            if (ioctl(fd, DRM_IOCTL_GET_MAGIC, &auth) != 0) {
                WARNING("DRM_IOCTL_GET_MAGIC failed: %s (%s)\n",
                        strerror(errno), path);
                close(fd);
                dlclose(This->groups[j].handle);
                free(path);
                free(reply);
                continue;
            }

            MESSAGE("Associated `%.*s' with fd %d opened from `%s'\n",
                    xcb_dri2_connect_driver_name_length(reply),
                    xcb_dri2_connect_driver_name(reply), fd, path);

            free(path);
            free(reply);

            {
                xcb_window_t root = This->groups[i].loader.dri2_con.root;
                This->groups[j].fd = fd;
                This->groups[j].loader.dri2_auth.root = root;
                This->groups[j].loader.dri2_auth.cookie =
                    xcb_dri2_authenticate(This->c, root, auth.magic);
            }

            j++;
        }
    }

    MESSAGE("DRI2 connected, got %d group(s) of the previous %d\n",
            j, This->ngroups);
    This->ngroups = j;

    /* do the same trick as above, but this time we're requesting RandR info
     * for the screens we could authenticate */
    for (i = j = 0; i < This->ngroups; ++i) {
        xcb_window_t root = This->groups[i].loader.dri2_auth.root;
        xcb_dri2_authenticate_reply_t *reply =
            xcb_dri2_authenticate_reply(This->c,
                                        This->groups[i].loader.dri2_auth.cookie,
                                        &err);
        /* remember to move/destroy handle and fd as well */
        void *handle = This->groups[i].handle;
        struct D3DAdapter9DescriptorDRM *descriptor =
            This->groups[i].loader.dri2_auth.descriptor;
        int fd = This->groups[i].fd;
        HRESULT hr;

        if (err) {
            WARNING("DRI2Authenticate failed with error %hhu (major=%hhu, "
                    "minor=%hu): Error authenticating fd %d\n",
                    err->error_code, err->major_code, err->minor_code, fd);
            if (reply) { free(reply); }
            dlclose(handle);
            close(fd);
            continue;
        }

        if (!reply->authenticated) {
            WARNING("Unable to authenticate fd %d\n", fd);
            free(reply);
            dlclose(handle);
            close(fd);
            continue;
        }

        hr = descriptor->create_adapter(fd, &This->groups[j].adapter);
        if (FAILED(hr)) {
            WARNING("Unable to create ID3DAdapter9 for fd %d\n", fd);
            free(reply);
            dlclose(handle);
            close(fd);
            continue;
        }
        MESSAGE("Created ID3DAdapter9 for fd %d\n", fd);

        free(reply);

        This->groups[j].handle = handle;
        This->groups[j].fd = fd;
        This->groups[j].loader.randr.cookie =
            xcb_randr_get_screen_resources_current(This->c, root);
        j++;
    }

    MESSAGE("DRI2 authenticated, got %d group(s) of the previous %d\n",
            j, This->ngroups);
    This->ngroups = j;

    /* and lastly enumerate all outputs associated with authd adapters */
    for (i = j = 0; i < This->ngroups; ++i) {
        xcb_randr_get_screen_resources_current_cookie_t cookie =
            This->groups[i].loader.randr.cookie;
        xcb_randr_get_screen_resources_current_reply_t *reply =
            xcb_randr_get_screen_resources_current_reply(This->c, cookie, &err);
        xcb_randr_output_t *outputs;
        xcb_randr_mode_info_t *modeinfos;
        unsigned nmodeinfos;
        unsigned coutput = 0;
        int fd = This->groups[i].fd; /* debug stuff */

        if (err) {
            WARNING("XRRGetScreenResourcesCurrent failed with error %hhu "
                    "(major=%hhu, minor=%hu): Unable to get screen resources "
                    "for fd %d\n",
                    err->error_code, err->major_code, err->minor_code, fd);
            if (reply) { free(reply); }

            dlclose(This->groups[i].handle);
            This->groups[i].handle = NULL;

            close(This->groups[i].fd);
            This->groups[i].fd = -1;

            continue;
        }

        This->groups[j].noutputs =
            xcb_randr_get_screen_resources_current_outputs_length(reply);
        This->groups[j].outputs = calloc(This->groups[j].noutputs,
                                         sizeof(struct output));
        if (!This->groups[j].outputs) {
            free(reply);
            break;
        }
        outputs = xcb_randr_get_screen_resources_current_outputs(reply);

        for (k = 0; k < This->groups[j].noutputs; ++k) {
            This->groups[j].outputs[k].loader.info_cookie =
                xcb_randr_get_output_info(This->c, outputs[k],
                                          reply->config_timestamp);
        }

        modeinfos = xcb_randr_get_screen_resources_current_modes(reply);
        nmodeinfos = xcb_randr_get_screen_resources_current_modes_length(reply);

        for (k = 0; k < This->groups[j].noutputs; ++k) {
            xcb_randr_get_output_info_cookie_t icookie =
                This->groups[j].outputs[k].loader.info_cookie;
            xcb_randr_get_output_info_reply_t *ireply =
                xcb_randr_get_output_info_reply(This->c, icookie, &err);
            xcb_randr_mode_t *modes;
            unsigned cmode;

            if (err) {
                WARNING("XRRGetOutputInfo failed with error %hhu (major=%hhu, "
                        "minor=%hu): Unable to get output 0x%x information "
                        "for fd %d\n", err->error_code,
                        err->major_code, err->minor_code, outputs[k], fd);
                if (ireply) { free(ireply); }
                continue;
            }

            /*
            XXX Why does this return seemingly random values of 'status'?
            if (ireply->status != XCB_RANDR_SET_CONFIG_SUCCESS) {
                WARNING("Unable to get output %u information for "
                        "authenticated screen %d. This can be caused by a "
                        "screen changing while libd3d9-xlib is grabbing data "
                        "from X. Try restarting your application.\n",
                        outputs[k], i);
                free(ireply);
                continue;
            }*/

            if (ireply->connection != XCB_RANDR_CONNECTION_CONNECTED) {
                /* in the unknown case, we simply assume that it's connected */
                free(ireply);
                continue;
            }
            This->groups[j].outputs[coutput].nmodes = ireply->num_modes;
            This->groups[j].outputs[coutput].xid = outputs[k];
            This->groups[j].outputs[coutput].modes =
                calloc(ireply->num_modes, sizeof(struct display_mode));
            if (!This->groups[j].outputs[coutput].modes) {
                free(ireply);
                break;
            }

            This->groups[j].outputs[coutput].loader.crtc_cookie =
                xcb_randr_get_crtc_info(This->c, ireply->crtc,
                                        reply->config_timestamp);

            modes = xcb_randr_get_output_info_modes(ireply);
            for (cmode = 0; cmode < ireply->num_modes; ++cmode) {
                unsigned mi;

                This->groups[j].outputs[coutput].modes[cmode].xid =
                    modes[cmode];

                for (mi = 0; mi < nmodeinfos; ++mi) {
                    if (modeinfos[mi].id == modes[cmode]) {
                        D3DDISPLAYMODEEX *d3dmode =
                            &This->groups[j].outputs[coutput].modes[cmode].mode;
                        d3dmode->Size = sizeof(D3DDISPLAYMODEEX);
                        d3dmode->Width = modeinfos[mi].width;
                        d3dmode->Height = modeinfos[mi].height;
                        if (modeinfos[mi].htotal && modeinfos[mi].vtotal) {
                            /* D3D only recognizes integer refresh rates */
                            d3dmode->RefreshRate =
                                ((double)modeinfos[mi].dot_clock /
	                            (modeinfos[mi].htotal*modeinfos[mi].vtotal))+.5;
                        }
                        d3dmode->Format = D3DFMT_X8R8G8B8; /* XXX */
                        d3dmode->ScanLineOrdering =
                            (modeinfos[mi].mode_flags &
                             XCB_RANDR_MODE_FLAG_INTERLACE) ?
                                D3DSCANLINEORDERING_INTERLACED :
                                D3DSCANLINEORDERING_PROGRESSIVE;

                        break;
                    }
                }
            }

            free(ireply);

            coutput++;
        }

        MESSAGE("Got %d outputs for fd %d out of which %d are connected\n",
                This->groups[j].noutputs, fd, coutput);
        This->groups[j].noutputs = coutput;

        /* and the last little bit of information is rotation */
        for (k = 0; k < This->groups[j].noutputs; ++k) {
            xcb_randr_get_crtc_info_cookie_t ccookie =
                This->groups[j].outputs[k].loader.crtc_cookie;
            xcb_randr_get_crtc_info_reply_t *creply =
                xcb_randr_get_crtc_info_reply(This->c, ccookie, &err);
            int find;

            if (err) {
                WARNING("XRRGetCrtcInfo failed with error %hhu (major=%hhu, "
                        "minor=%hu): Unable to get rotation information for "
                        "output 0x%x in fd %d\n",
                        err->error_code, err->major_code, err->minor_code,
                        This->groups[j].outputs[k].xid, fd);
                if (creply) { free(creply); }
                continue;
            }

            /* remember the current mode */
            for (find = 0; find < This->groups[j].outputs[k].nmodes; ++find) {
                if (This->groups[j].outputs[k].modes[find].xid == creply->mode) {
                    This->groups[j].outputs[k].current = find;
                    break;
                }
            }

            This->groups[j].outputs[k].rotation = D3DDISPLAYROTATION_IDENTITY;
            switch (creply->rotation) {
                case XCB_RANDR_ROTATION_ROTATE_0:
                    This->groups[j].outputs[k].rotation =
                        D3DDISPLAYROTATION_IDENTITY;
                    break;

                case XCB_RANDR_ROTATION_ROTATE_90:
                    This->groups[j].outputs[k].rotation =
                        D3DDISPLAYROTATION_90;
                    break;

                case XCB_RANDR_ROTATION_ROTATE_180:
                    This->groups[j].outputs[k].rotation =
                        D3DDISPLAYROTATION_180;
                    break;

                case XCB_RANDR_ROTATION_ROTATE_270:
                    This->groups[j].outputs[k].rotation =
                        D3DDISPLAYROTATION_270;
                    break;

                case XCB_RANDR_ROTATION_REFLECT_X:
                case XCB_RANDR_ROTATION_REFLECT_Y:
                    /* XXX how do we represent this? */
                    break;

                default:
                    WARNING("Unknown crtc rotation %d received from Xserver\n",
                            creply->rotation);
            }

            free(creply);
        }

        /* move handle, descriptor and fd */
        if (i != j) {
            This->groups[j].handle = This->groups[i].handle;
            This->groups[j].adapter = This->groups[i].adapter;
            This->groups[j].fd = This->groups[i].fd;
            This->groups[i].handle = NULL;
            This->groups[i].adapter = NULL;
            This->groups[i].fd = -1;
        }

        free(reply);

        j++;
    }

    if (i != This->ngroups) {
        /* free resources in a desperate attempt to leak as little as possible
         * when running out of RAM */
        for (; j >= 0; --j) {
            if (This->groups[j].outputs) {
                for (k = 0; k < This->groups[j].noutputs; ++k) {
                    if (This->groups[j].outputs[k].modes) {
                        free(This->groups[j].outputs[k].modes);
                    }
                }
                free(This->groups[j].outputs);
            }
        }
        for (j = 0; j < This->ngroups; ++j) {
            if (This->groups[j].adapter) {
                ID3DAdapter9_Release(This->groups[j].adapter);
            }
            if (This->groups[j].handle) {
                dlclose(This->groups[j].handle);
            }
            if (This->groups[j].fd >= 0) {
                close(This->groups[j].fd);
            }
        }
        free(This->groups);
        OOM();
    }

    MESSAGE("XRandR info cached, got %d group(s) of the previous %d\n",
            j, This->ngroups);
    This->ngroups = j;

    return D3D_OK;
}

static HRESULT
Nine9Ex_new( Display *dpy,
             boolean ex,
             IDirect3D9Ex **ppOut )
{
    xcb_generic_error_t *err = NULL;
    int i, j, k = 0;
    HRESULT hr;

    struct Nine9Ex *This = calloc(1, sizeof(struct Nine9Ex));
    if (!This) { OOM(); }

    /* use XCB */
    This->c = XGetXCBConnection(dpy);
    if (xcb_connection_has_error(This->c)) {
        ERROR("Unable to get XCB connection.\n");
        free(This);
        return D3DERR_DRIVERINTERNALERROR;
    }

    /* query the extensions we need */
    {
        xcb_dri2_query_version_cookie_t cookie;
        xcb_dri2_query_version_reply_t *reply;

        cookie = xcb_dri2_query_version(This->c, 1, 3);
        reply = xcb_dri2_query_version_reply(This->c, cookie, &err);
        if (err) {
            ERROR("Unable to query DRI2 extension.\n"
                  "This library requires DRI2. Please ensure your Xserver "
                  "supports this extension.\n");
            free(This);
            return D3DERR_DRIVERINTERNALERROR;
        }

        MESSAGE("Got DRI2 version %u.%u\n",
                reply->major_version, reply->minor_version);

        This->dri2_minor = reply->minor_version;
        free(reply);
    }

    {
        xcb_xfixes_query_version_cookie_t cookie;
        xcb_xfixes_query_version_reply_t *reply;

        cookie = xcb_xfixes_query_version(This->c, 2, 0);
        reply = xcb_xfixes_query_version_reply(This->c, cookie, &err);
        if (err) {
            ERROR("Unable to query XFixes extension.\n"
                  "This library requires XFixes. Please ensure your Xserver "
                  "supports this extension.\n");
            free(This);
            return D3DERR_DRIVERINTERNALERROR;
        }

        MESSAGE("Got XFixes version %u.%u\n",
                reply->major_version, reply->minor_version);

        free(reply);
    }

    {
        xcb_randr_query_version_cookie_t cookie;
        xcb_randr_query_version_reply_t *reply;

        cookie = xcb_randr_query_version(This->c, 1, 4);
        reply = xcb_randr_query_version_reply(This->c, cookie, &err);
        if (err) {
            ERROR("Unable to query XRandR extension.\n"
                  "This library requires XRandR. Please ensure your Xserver "
                  "supports this extension.\n");
            free(This);
            return D3DERR_DRIVERINTERNALERROR;
        }

        MESSAGE("Got XRandR version %u.%u\n",
                reply->major_version, reply->minor_version);

        free(reply);
    }

    /* create the real object */
    This->vtable = &Nine9Ex_vtable;
    This->refs = 1;
    This->ex = ex;

    /* query Xserver for adapter groups (screens) */
    hr = fill_groups(This, DefaultScreen(dpy));
    if (FAILED(hr)) {
        free(This);
        return hr;
    }

    /* count total number of adapters */
    for (i = j = 0; i < This->ngroups; ++i) {
        j += This->groups[i].noutputs;
    }
    This->nadapters = j;

    /* map absolute adapter IDs with internal outputs */
    This->map = calloc(This->nadapters, sizeof(struct adapter_map));
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

PUBLIC IDirect3D9 * WINAPI
XDirect3DCreate9( UINT SDKVersion,
                  Display *pDisplay )
{
    IDirect3D9Ex *d3d = NULL;
    HRESULT hr = Nine9Ex_new(pDisplay, FALSE, &d3d);
    assert((FAILED(hr) && d3d == NULL) || (SUCCEEDED(hr) && d3d != NULL));
    return (IDirect3D9 *)d3d;
}

PUBLIC HRESULT WINAPI
XDirect3DCreate9Ex( UINT SDKVersion,
                    Display *pDisplay,
                    IDirect3D9Ex **ppD3D )
{
    return Nine9Ex_new(pDisplay, TRUE, ppD3D);
}
