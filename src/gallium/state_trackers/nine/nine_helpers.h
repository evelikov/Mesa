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

#ifndef _NINE_HELPERS_H_
#define _NINE_HELPERS_H_

#include "iunknown.h"

/* Sshhh ... */
#define nine_reference(a, b) _nine_reference((void **)(a), (b))

static inline void _nine_reference(void **ref, void *ptr)
{
    if (*ref != ptr) {
        if (*ref)
            NineUnknown_Release(*ref);
        if (ptr)
            NineUnknown_AddRef(ptr);
        *ref = ptr;
    }
}

#define nine_reference_set(a, b) _nine_reference_set((void *)(a), (b))

static inline void _nine_reference_set(void **ref, void *ptr)
{
    *ref = ptr;
    if (ptr)
        NineUnknown_AddRef(ptr);
}

#define NINE_NEW(nine, out, ...) \
    { \
        struct NineUnknownParams __params; \
        struct nine *__data; \
         \
        __data = CALLOC_STRUCT(nine); \
        if (!__data) { return E_OUTOFMEMORY; } \
         \
        __params.vtable = &nine##_vtable; \
        __params.guids = nine##_IIDs; \
        __params.dtor = (void *)nine##_dtor; \
        __params.container = NULL; \
        { \
            HRESULT __hr = nine##_ctor(__data, &__params, ## __VA_ARGS__); \
            if (FAILED(__hr)) { \
                nine##_dtor(__data); \
                return __hr; \
            } \
        } \
         \
        *(out) = __data; \
    } \
    return D3D_OK

static INLINE float asfloat(DWORD value)
{
    union {
        float f;
        DWORD w;
    } u;
    u.w = value;
    return u.f;
}

#endif /* _NINE_HELPERS_H_ */
