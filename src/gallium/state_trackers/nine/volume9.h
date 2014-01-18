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

#ifndef _NINE_VOLUME9_H_
#define _NINE_VOLUME9_H_

#include "iunknown.h"

struct NineVolume9
{
    struct NineUnknown base;
};
static INLINE struct NineVolume9 *
NineVolume9( void *data )
{
    return (struct NineVolume9 *)data;
}

HRESULT WINAPI
NineVolume9_GetDevice( struct NineVolume9 *This,
                       IDirect3DDevice9 **ppDevice );

HRESULT WINAPI
NineVolume9_SetPrivateData( struct NineVolume9 *This,
                            REFGUID refguid,
                            const void *pData,
                            DWORD SizeOfData,
                            DWORD Flags );

HRESULT WINAPI
NineVolume9_GetPrivateData( struct NineVolume9 *This,
                            REFGUID refguid,
                            void *pData,
                            DWORD *pSizeOfData );

HRESULT WINAPI
NineVolume9_FreePrivateData( struct NineVolume9 *This,
                             REFGUID refguid );

HRESULT WINAPI
NineVolume9_GetContainer( struct NineVolume9 *This,
                          REFIID riid,
                          void **ppContainer );

HRESULT WINAPI
NineVolume9_GetDesc( struct NineVolume9 *This,
                     D3DVOLUME_DESC *pDesc );

HRESULT WINAPI
NineVolume9_LockBox( struct NineVolume9 *This,
                     D3DLOCKED_BOX *pLockedVolume,
                     const D3DBOX *pBox,
                     DWORD Flags );

HRESULT WINAPI
NineVolume9_UnlockBox( struct NineVolume9 *This );

#endif /* _NINE_VOLUME9_H_ */
