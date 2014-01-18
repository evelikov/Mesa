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

#include "query9.h"

#define DBG_CHANNEL DBG_QUERY

HRESULT WINAPI
NineQuery9_GetDevice( struct NineQuery9 *This,
                      IDirect3DDevice9 **ppDevice )
{
    STUB(D3DERR_INVALIDCALL);
}

D3DQUERYTYPE WINAPI
NineQuery9_GetType( struct NineQuery9 *This )
{
    STUB(0);
}

DWORD WINAPI
NineQuery9_GetDataSize( struct NineQuery9 *This )
{
    STUB(0);
}

HRESULT WINAPI
NineQuery9_Issue( struct NineQuery9 *This,
                  DWORD dwIssueFlags )
{
    STUB(D3DERR_INVALIDCALL);
}

HRESULT WINAPI
NineQuery9_GetData( struct NineQuery9 *This,
                    void *pData,
                    DWORD dwSize,
                    DWORD dwGetDataFlags )
{
    STUB(D3DERR_INVALIDCALL);
}

IDirect3DQuery9Vtbl NineQuery9_vtable = {
    (void *)NineUnknown_QueryInterface,
    (void *)NineUnknown_AddRef,
    (void *)NineUnknown_Release,
    (void *)NineQuery9_GetDevice,
    (void *)NineQuery9_GetType,
    (void *)NineQuery9_GetDataSize,
    (void *)NineQuery9_Issue,
    (void *)NineQuery9_GetData
};
