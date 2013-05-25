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
#include "d3dadapter9.h"
#include "d3ddrm.h"

#include "../debug.h"
#include "../driver.h"
#include "../guid.h"
#include "present.h"

#undef _WIN32
#include <X11/Xlib.h>
#include <X11/Xlib-xcb.h>
#include <xcb/xcb.h>
#include <xcb/dri2.h>
#include <xcb/xfixes.h>
#define _WIN32

#include <drm/drm.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define DRIVER_MAJOR 0

struct adapter
{
    int fd;
    void *handle;
};

struct NineWineDriverX11
{
    void *vtable;
    UINT refs;

    /* connection */
    xcb_connection_t *c;
    unsigned dri2_major, dri2_minor;

    /* fds and handles */
    struct adapter *adapters;
    unsigned nadapters;
    unsigned nadaptersalloc;
};

static HRESULT WINAPI
NineWineDriverX11_QueryInterface( struct NineWineDriverX11 *This,
                                  REFIID riid,
                                  void **ppvObject )
{
    if (!ppvObject) { return E_POINTER; }
    if (GUID_equal(&IID_ID3DWineDriver, riid) ||
        GUID_equal(&IID_IUnknown, riid)) {
        *ppvObject = This;
        This->refs++;
        return S_OK;
    }
    *ppvObject = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI
NineWineDriverX11_AddRef( struct NineWineDriverX11 *This )
{
    return ++This->refs;
}

static ULONG WINAPI
NineWineDriverX11_Release( struct NineWineDriverX11 *This )
{
    if (--This->refs == 0) {
        /* dtor */
        int i;
        for (i = 0; i < This->nadapters; ++i) {
            close(This->adapters[i].fd);
            dlclose(This->adapters[i].handle);
        }
        HeapFree(GetProcessHeap(), 0, This->adapters);
        HeapFree(GetProcessHeap(), 0, This);
        return 0;
    }
    return This->refs;
}

static HRESULT WINAPI
NineWineDriverX11_CreatePresentFactory( struct NineWineDriverX11 *This,
                                        HWND hFocusWnd,
                                        D3DPRESENT_PARAMETERS *pParams,
                                        unsigned nParams,
                                        ID3DPresentFactory **ppPresentFactory )
{
    return NineWinePresentFactoryX11_new(This->c, hFocusWnd, pParams, nParams,
                                         This->dri2_major, This->dri2_minor,
                                         ppPresentFactory);
}

static INLINE HRESULT
push_adapter( struct NineWineDriverX11 *This,
              int fd,
              void *handle )
{
    if (This->nadapters >= This->nadaptersalloc) {
        void *r;

        if (This->nadaptersalloc == 0) {
            This->nadaptersalloc = 2;
            r = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                          This->nadaptersalloc*sizeof(struct adapter));
        } else {
            This->nadaptersalloc <<= 1;
            r = HeapReAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, This->adapters,
                            This->nadaptersalloc*sizeof(struct adapter));
        }

        if (!r) { OOM(); }
        This->adapters = r;
    }

    This->adapters[This->nadapters].fd = fd;
    This->adapters[This->nadapters].handle = handle;
    This->nadapters++;

    return D3D_OK;
}

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
                        _WARNING("Error opening driver `%s': %s\n", buf, err);
                    }
                }

                if (driver) {
                    do {
                        /* validate it here */
                        struct D3DAdapter9DescriptorDRM *drm =
                            dlsym(driver, D3DAdapter9DescriptorDRMName);

                        if (!drm) {
                            _WARNING("Error opening driver `%s': %s\n",
                                     buf, dlerror());
                            break;
                        }

                        if (drm->major_version != DRIVER_MAJOR) {
                            _WARNING("Error opening driver `%s': Driver "
                                     "major version (%u) is not the same "
                                     "as library version (%u)\n",
                                     buf, drm->major_version, DRIVER_MAJOR);
                            break;
                        }

                        if (drm->create_adapter == NULL) {
                            _WARNING("Error opening driver `%s': "
                                     "create_adapter == NULL\n", buf);
                        }

                        *handle = driver;
                        *descriptor = drm;

                        _MESSAGE("Loaded driver `%.*s' from `%s'\n",
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

    _WARNING("Unable to locate driver named `%.*s'\n", namelen, name);

    return D3DERR_NOTAVAILABLE;
}

static HRESULT WINAPI
NineWineDriverX11_CreateAdapter9( struct NineWineDriverX11 *This,
                                  HDC hdc,
                                  ID3DAdapter9 **ppAdapter )
{
    struct D3DAdapter9DescriptorDRM *drm;
    xcb_generic_error_t *err = NULL;
    xcb_drawable_t drawable = 0;
    xcb_window_t root;
    drm_auth_t auth;
    void *handle;
    int fd;
    HRESULT hr;

    drawable = X11DRV_ExtEscape_GET_DRAWABLE(hdc);

    { /* XGetGeometry */
        xcb_get_geometry_cookie_t cookie;
        xcb_get_geometry_reply_t *reply;

        cookie = xcb_get_geometry(This->c, drawable);
        reply = xcb_get_geometry_reply(This->c, cookie, &err);
        if (err) {
            _WARNING("XGetGeometry failed with error %hhu (major=%hhu, "
                     "minor=%hu): Unable to get root window of drawable %u\n",
                     err->error_code, err->major_code, err->minor_code,
                     drawable);
            if (reply) { free(reply); }
            return D3DERR_DRIVERINTERNALERROR;
        }
        root = reply->root;
        free(reply);
    }

    { /* DRI2Connect */
        xcb_dri2_connect_cookie_t cookie;
        xcb_dri2_connect_reply_t *reply;
        char *path;

        cookie = xcb_dri2_connect(This->c, root, XCB_DRI2_DRIVER_TYPE_DRI);
        reply = xcb_dri2_connect_reply(This->c, cookie, &err);
        if (err) {
            _WARNING("DRI2Connect failed with error %hhu (major=%hhu, "
                     "minor=%hu): Unable to connect DRI2 on root %u\n",
                     err->error_code, err->major_code, err->minor_code, root);
            if (reply) { free(reply); }
            return D3DERR_DRIVERINTERNALERROR;
        }

        hr = load_path(xcb_dri2_connect_driver_name(reply),
                       xcb_dri2_connect_driver_name_length(reply),
                       &handle, &drm);
        if (FAILED(hr)) {
            free(reply);
            return hr;
        }

        path = _strndup(xcb_dri2_connect_device_name(reply),
                        xcb_dri2_connect_device_name_length(reply));

        fd = open(path, O_RDWR);
        if (fd < 0) {
            _WARNING("Failed to open drm fd: %s (%s)\n",
                      strerror(errno), path);
            dlclose(handle);
            free(path);
            free(reply);
            return D3DERR_DRIVERINTERNALERROR;
        }

        hr = push_adapter(This, fd, handle);
        if (FAILED(hr)) {
            close(fd);
            dlclose(handle);
            free(path);
            free(reply);
            return hr;
        }

        /* authenticate */
        if (ioctl(fd, DRM_IOCTL_GET_MAGIC, &auth) != 0) {
            _WARNING("DRM_IOCTL_GET_MAGIC failed: %s (%s)\n",
                     strerror(errno), path);
            free(path);
            free(reply);
            return D3DERR_DRIVERINTERNALERROR;
        }

        _MESSAGE("Associated `%.*s' with fd %d opened from `%s'\n",
                 xcb_dri2_connect_driver_name_length(reply),
                 xcb_dri2_connect_driver_name(reply), fd, path);

        free(path);
        free(reply);
    }

    { /* DRI2Authenticate */
        xcb_dri2_authenticate_cookie_t cookie;
        xcb_dri2_authenticate_reply_t *reply;

        cookie = xcb_dri2_authenticate(This->c, root, auth.magic);
        reply = xcb_dri2_authenticate_reply(This->c, cookie, &err);

        if (err) {
            _WARNING("DRI2Authenticate failed with error %hhu (major=%hhu, "
                     "minor=%hu): Error authenticating fd %d\n",
                     err->error_code, err->major_code, err->minor_code, fd);
            if (reply) { free(reply); }
            return D3DERR_DRIVERINTERNALERROR;
        }

        if (!reply->authenticated) {
            _WARNING("Unable to authenticate fd %d\n", fd);
            free(reply);
            return D3DERR_DRIVERINTERNALERROR;
        }
        free(reply);
    }

    hr = drm->create_adapter(fd, ppAdapter);
    if (FAILED(hr)) {
        _WARNING("Unable to create ID3DAdapter9 for fd %d\n", fd);
        return hr;
    }

    _MESSAGE("Created ID3DAdapter9 for fd %d\n", fd);

    return D3D_OK;
}

static INLINE HRESULT
check_x11_proto( struct NineWineDriverX11 *This )
{
    xcb_dri2_query_version_cookie_t dri2cookie;
    xcb_dri2_query_version_reply_t *dri2reply;
    xcb_xfixes_query_version_cookie_t fixcookie;
    xcb_xfixes_query_version_reply_t *fixreply;
    xcb_generic_error_t *err = NULL;

    /* query DRI2 */
    dri2cookie = xcb_dri2_query_version(This->c, 1, 3);
    dri2reply = xcb_dri2_query_version_reply(This->c, dri2cookie, &err);
    if (err) {
        _ERROR("Unable to query DRI2 extension.\nThis library requires DRI2. "
               "Please ensure your Xserver supports this extension.\n");
        return D3DERR_DRIVERINTERNALERROR;
    }
    _MESSAGE("Got DRI2 version %u.%u\n",
            dri2reply->major_version, dri2reply->minor_version);
    /* save version for feature checking */
    This->dri2_major = dri2reply->major_version;
    This->dri2_minor = dri2reply->minor_version;
    free(dri2reply);

    /* query XFixes for regions in blitting */
    fixcookie = xcb_xfixes_query_version(This->c, 2, 0);
    fixreply = xcb_xfixes_query_version_reply(This->c, fixcookie, &err);
    if (err) {
        _ERROR("Unable to query XFixes extension.\nThis library requires "
               "XFixes. Please ensure your Xserver supports this extension.\n");
        return D3DERR_DRIVERINTERNALERROR;
    }
    _MESSAGE("Got XFixes version %u.%u\n",
            fixreply->major_version, fixreply->minor_version);
    free(fixreply);

    return D3D_OK;
}

static ID3DWineDriverVtbl NineWineDriverX11_vtable = {
    (void *)NineWineDriverX11_QueryInterface,
    (void *)NineWineDriverX11_AddRef,
    (void *)NineWineDriverX11_Release,
    (void *)NineWineDriverX11_CreatePresentFactory,
    (void *)NineWineDriverX11_CreateAdapter9
};

HRESULT
D3DWineDriverCreate( ID3DWineDriver **ppDriver );

HRESULT
D3DWineDriverCreate( ID3DWineDriver **ppDriver )
{
    struct NineWineDriverX11 *This =
        HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                  sizeof(struct NineWineDriverX11));
    HRESULT hr;

    if (!This) { OOM(); }

    This->vtable = &NineWineDriverX11_vtable;
    This->refs = 1;

    This->c = xcb_connect(NULL, NULL); /* This is (almost) what Wine does */
    if (xcb_connection_has_error(This->c)) {
        _ERROR("Unable to get XCB connection from Xlib Display.\n");
        HeapFree(GetProcessHeap(), 0, This);
        return D3DERR_DRIVERINTERNALERROR;
    }

    hr = check_x11_proto(This);
    if (FAILED(hr)) {
        NineWineDriverX11_Release(This);
        return hr;
    }

    *ppDriver = (ID3DWineDriver *)This;

    return D3D_OK;
}
