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

#ifndef _NINE_STATEBLOCK9_H_
#define _NINE_STATEBLOCK9_H_

#include "iunknown.h"

struct NineStateBlock9
{
    struct NineUnknown base;
};
static INLINE struct NineStateBlock9 *
NineStateBlock9( void *data )
{
    return (struct NineStateBlock9 *)data;
}

HRESULT WINAPI
NineStateBlock9_GetDevice( struct NineStateBlock9 *This,
                           IDirect3DDevice9 **ppDevice );

HRESULT WINAPI
NineStateBlock9_Capture( struct NineStateBlock9 *This );

HRESULT WINAPI
NineStateBlock9_Apply( struct NineStateBlock9 *This );

#endif /* _NINE_STATEBLOCK9_H_ */
