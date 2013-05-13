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

#include "iunknown.h"
#include "nine_helpers.h"

#define DBG_CHANNEL DBG_UNKNOWN

HRESULT
NineUnknown_ctor( struct NineUnknown *This,
                  struct NineUnknownParams *pParams )
{
    This->refs = 1;
    This->container = pParams->container;

    This->vtable = pParams->vtable;
    This->guids = pParams->guids;
    This->dtor = pParams->dtor;

    return D3D_OK;
}

void
NineUnknown_dtor( struct NineUnknown *This )
{
    FREE(This);
}

HRESULT WINAPI
NineUnknown_QueryInterface( struct NineUnknown *This,
                            REFIID riid,
                            void **ppvObject )
{
    unsigned i = 0;

    if (!ppvObject) { return E_POINTER; }

    do {
        if (GUID_equal(This->guids[i], riid)) {
            *ppvObject = This;
            return S_OK;
        }
    } while (This->guids[i++]);

    return E_NOINTERFACE;
}

ULONG WINAPI
NineUnknown_AddRef( struct NineUnknown *This )
{
    ULONG r = ++This->refs; /* TODO: make atomic */

    if (This->container && r == 2) {
        /* acquire a reference to the container so it doesn't go away, but only
         * when a reference is held outside the container itself */
        NineUnknown_AddRef(This->container);
    }
    return r;
}

ULONG WINAPI
NineUnknown_Release( struct NineUnknown *This )
{
    ULONG r = --This->refs; /* TODO: make atomic */

    /* This would signify implementation error */
    assert(r >= 0);

    if (r == 0) {
        This->dtor(This);
    } else if (r == 1 && This->container) {
        /* release the container when only the container holds a reference
         * to this object. This avoids circular referencing */
        NineUnknown_Release(This->container);
    }
    return r;
}
