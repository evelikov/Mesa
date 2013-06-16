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

#ifndef _NINE_SHADER_H_
#define _NINE_SHADER_H_

#include "d3d9types.h"
#include "d3d9caps.h"
#include "nine_defines.h"
#include "pipe/p_state.h" /* PIPE_MAX_ATTRIBS */

struct NineDevice9;

struct nine_lconstf
{
    int *locations;
    float *data;
    unsigned num;
};

struct nine_shader_info
{
    unsigned type; /* in, PIPE_SHADER_x */

    const DWORD *byte_code; /* in, pointer to shader tokens */
    DWORD        byte_size; /* out, size of data at byte_code */

    void *cso; /* out, pipe cso for bind_vs,fs_state */

    uint8_t input_map[PIPE_MAX_ATTRIBS]; /* VS input -> NINE_DECLUSAGE_x */
    uint8_t num_inputs; /* there may be unused inputs (NINE_DECLUSAGE_NONE) */

    boolean position_t; /* out, true if VP writes pre-transformed position */
    boolean point_size; /* out, true if VP writes point size */

    uint16_t sampler_mask; /* out, which samplers are being used */
    uint16_t sampler_mask_shadow; /* in, which samplers use depth compare */
    uint8_t rt_mask; /* out, which render targets are being written */

    struct nine_lconstf lconstf; /* out, NOTE: members to be free'd by user */
};

HRESULT
nine_translate_shader(struct NineDevice9 *device, struct nine_shader_info *);

#endif /* _NINE_SHADER_H_ */
