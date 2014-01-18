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

#ifndef _D3DDRM_H_
#define _D3DDRM_H_

#include "d3dadapter9.h"

/* NOTE: fd is owned by the calling process, not the driver. The calling
 * process opened it, and it will close it. */
#define D3DAdapter9DescriptorDRMName "D3DAdapter9DRMEntryPoint"
typedef HRESULT (*D3DCreateAdapter9DRMProc)(int fd, ID3DAdapter9 **ppAdapter);

struct D3DAdapter9DescriptorDRM
{
    unsigned major_version; /* ABI break */
    unsigned minor_version; /* backwards compatible feature additions */

    D3DCreateAdapter9DRMProc create_adapter;
};

/* presentation buffer */
typedef struct _D3DDRM_BUFFER
{
    INT iName;
    DWORD dwWidth;
    DWORD dwHeight;
    DWORD dwStride;
    DWORD dwCPP;
} D3DDRM_BUFFER;

#endif /* _D3DDRM_H_ */
