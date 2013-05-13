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

#ifndef _NINE_IUNKNOWN_H_
#define _NINE_IUNKNOWN_H_

#include "pipe/p_compiler.h"

#include "util/u_memory.h"

#include "guid.h"
#include "nine_debug.h"
#include "nine_quirk.h"

#include "d3d9.h"

struct Nine9;

struct NineUnknown
{
    /* pointer to vtable  */
    void *vtable;

    /* reference count */
    ULONG refs;
    /* for which we can hold a special internal reference */
    struct NineUnknown *container;

    /* for QueryInterface */
    const GUID **guids;

    /* top-level dtor */
    void (*dtor)(void *data);
};
static INLINE struct NineUnknown *
NineUnknown( void *data )
{
    return (struct NineUnknown *)data;
}

/* Use this instead of a shitload of arguments */
struct NineUnknownParams
{
    void *vtable;
    const GUID **guids;
    void (*dtor)(void *data);
    struct NineUnknown *container;
};

/* ctor/dtor */
HRESULT
NineUnknown_ctor( struct NineUnknown *This,
                  struct NineUnknownParams *pParams );

void
NineUnknown_dtor( struct NineUnknown *This );

/* Methods */
HRESULT WINAPI
NineUnknown_QueryInterface( struct NineUnknown *This,
                            REFIID riid,
                            void **ppvObject );

ULONG WINAPI
NineUnknown_AddRef( struct NineUnknown *This );

ULONG WINAPI
NineUnknown_Release( struct NineUnknown *This );

#endif /* _NINE_IUNKNOWN_H_ */
