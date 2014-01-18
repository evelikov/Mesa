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

#include "volume9.h"

#define DBG_CHANNEL DBG_VOLUME

HRESULT WINAPI
NineVolume9_GetDevice( struct NineVolume9 *This,
                       IDirect3DDevice9 **ppDevice )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineVolume9_SetPrivateData( struct NineVolume9 *This,
                            REFGUID refguid,
                            const void *pData,
                            DWORD SizeOfData,
                            DWORD Flags )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineVolume9_GetPrivateData( struct NineVolume9 *This,
                            REFGUID refguid,
                            void *pData,
                            DWORD *pSizeOfData )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineVolume9_FreePrivateData( struct NineVolume9 *This,
                             REFGUID refguid )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineVolume9_GetContainer( struct NineVolume9 *This,
                          REFIID riid,
                          void **ppContainer )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineVolume9_GetDesc( struct NineVolume9 *This,
                     D3DVOLUME_DESC *pDesc )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineVolume9_LockBox( struct NineVolume9 *This,
                     D3DLOCKED_BOX *pLockedVolume,
                     const D3DBOX *pBox,
                     DWORD Flags )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineVolume9_UnlockBox( struct NineVolume9 *This )
{
    STUB(D3DERR_INVALIDCALL);
}

IDirect3DVolume9Vtbl NineVolume9_vtable = {
    (void *)NineUnknown_QueryInterface,
    (void *)NineUnknown_AddRef,
    (void *)NineUnknown_Release,
    (void *)NineVolume9_GetDevice,
    (void *)NineVolume9_SetPrivateData,
    (void *)NineVolume9_GetPrivateData,
    (void *)NineVolume9_FreePrivateData,
    (void *)NineVolume9_GetContainer,
    (void *)NineVolume9_GetDesc,
    (void *)NineVolume9_LockBox,
    (void *)NineVolume9_UnlockBox
};
