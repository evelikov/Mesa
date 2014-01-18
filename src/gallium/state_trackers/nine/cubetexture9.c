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

#include "cubetexture9.h"

#define DBG_CHANNEL DBG_CUBETEXTURE

HRESULT WINAPI
NineCubeTexture9_GetLevelDesc( struct NineCubeTexture9 *This,
                               UINT Level,
                               D3DSURFACE_DESC *pDesc )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineCubeTexture9_GetCubeMapSurface( struct NineCubeTexture9 *This,
                                    D3DCUBEMAP_FACES FaceType,
                                    UINT Level,
                                    IDirect3DSurface9 **ppCubeMapSurface )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineCubeTexture9_LockRect( struct NineCubeTexture9 *This,
                           D3DCUBEMAP_FACES FaceType,
                           UINT Level,
                           D3DLOCKED_RECT *pLockedRect,
                           const RECT *pRect,
                           DWORD Flags )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineCubeTexture9_UnlockRect( struct NineCubeTexture9 *This,
                             D3DCUBEMAP_FACES FaceType,
                             UINT Level )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineCubeTexture9_AddDirtyRect( struct NineCubeTexture9 *This,
                               D3DCUBEMAP_FACES FaceType,
                               const RECT *pDirtyRect )
{
    STUB(D3DERR_INVALIDCALL);
}

IDirect3DCubeTexture9Vtbl NineCubeTexture9_vtable = {
    (void *)NineUnknown_QueryInterface,
    (void *)NineUnknown_AddRef,
    (void *)NineUnknown_Release,
    (void *)NineResource9_GetDevice,
    (void *)NineResource9_SetPrivateData,
    (void *)NineResource9_GetPrivateData,
    (void *)NineResource9_FreePrivateData,
    (void *)NineResource9_SetPriority,
    (void *)NineResource9_GetPriority,
    (void *)NineResource9_PreLoad,
    (void *)NineResource9_GetType,
    (void *)NineBaseTexture9_SetLOD,
    (void *)NineBaseTexture9_GetLOD,
    (void *)NineBaseTexture9_GetLevelCount,
    (void *)NineBaseTexture9_SetAutoGenFilterType,
    (void *)NineBaseTexture9_GetAutoGenFilterType,
    (void *)NineBaseTexture9_GenerateMipSubLevels,
    (void *)NineCubeTexture9_GetLevelDesc,
    (void *)NineCubeTexture9_GetCubeMapSurface,
    (void *)NineCubeTexture9_LockRect,
    (void *)NineCubeTexture9_UnlockRect,
    (void *)NineCubeTexture9_AddDirtyRect
};
