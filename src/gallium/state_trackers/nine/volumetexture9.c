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

#include "volumetexture9.h"

#define DBG_CHANNEL DBG_VOLUMETEXTURE

HRESULT WINAPI
NineVolumeTexture9_GetLevelDesc( struct NineVolumeTexture9 *This,
                                 UINT Level,
                                 D3DVOLUME_DESC *pDesc )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineVolumeTexture9_GetVolumeLevel( struct NineVolumeTexture9 *This,
                                   UINT Level,
                                   IDirect3DVolume9 **ppVolumeLevel )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineVolumeTexture9_LockBox( struct NineVolumeTexture9 *This,
                            UINT Level,
                            D3DLOCKED_BOX *pLockedVolume,
                            const D3DBOX *pBox,
                            DWORD Flags )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineVolumeTexture9_UnlockBox( struct NineVolumeTexture9 *This,
                              UINT Level )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineVolumeTexture9_AddDirtyBox( struct NineVolumeTexture9 *This,
                                const D3DBOX *pDirtyBox )
{
    STUB(D3DERR_INVALIDCALL);
}

IDirect3DVolumeTexture9Vtbl NineVolumeTexture9_vtable = {
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
    (void *)NineVolumeTexture9_GetLevelDesc,
    (void *)NineVolumeTexture9_GetVolumeLevel,
    (void *)NineVolumeTexture9_LockBox,
    (void *)NineVolumeTexture9_UnlockBox,
    (void *)NineVolumeTexture9_AddDirtyBox
};
