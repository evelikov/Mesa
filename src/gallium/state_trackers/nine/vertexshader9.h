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

#ifndef _NINE_VERTEXSHADER9_H_
#define _NINE_VERTEXSHADER9_H_

#include "iunknown.h"
#include "pipe/p_state.h" /* PIPE_MAX_ATTRIBS */

struct NineVertexShader9
{
    struct NineUnknown base;
    void *cso;
    struct {
    } input_map[PIPE_MAX_ATTRIBS];
    unsigned num_inputs;
    struct {
        const DWORD *tokens;
        DWORD size;
    } byte_code;
    struct NineDevice9 *device;
};
static INLINE struct NineVertexShader9 *
NineVertexShader9( void *data )
{
    return (struct NineVertexShader9 *)data;
}

HRESULT
NineVertexShader9_new( struct NineDevice9 *pDevice,
                       struct NineVertexShader9 **ppOut,
                       const DWORD *pFunction );

HRESULT
NineVertexShader9_ctor( struct NineVertexShader9 *,
                        struct NineUnknownParams *pParams,
                        struct NineDevice9 *pDevice,
                        const DWORD *pFunction );

void
NineVertexShader9_dtor( struct NineVertexShader9 * );

HRESULT WINAPI
NineVertexShader9_GetDevice( struct NineVertexShader9 *This,
                             IDirect3DDevice9 **ppDevice );

HRESULT WINAPI
NineVertexShader9_GetFunction( struct NineVertexShader9 *This,
                               void *pData,
                               UINT *pSizeOfData );

#endif /* _NINE_VERTEXSHADER9_H_ */
