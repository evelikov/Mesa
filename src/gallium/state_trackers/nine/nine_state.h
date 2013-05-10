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

#ifndef _NINE_STATE_H_
#define _NINE_STATE_H_

#include "d3d9.h"
#include "pipe/p_state.h"

#define NINED3DRS_LAST D3DRS_BLENDOPALPHA

#define NINED3DSAMP_LAST D3DSAMP_DMAPOFFSET

#define NINE_STATE_FB          (1 <<  0)
#define NINE_STATE_VIEWPORT    (1 <<  1)
#define NINE_STATE_SCISSOR     (1 <<  2)
#define NINE_STATE_RASTERIZER  (1 <<  3)
#define NINE_STATE_BLEND       (1 <<  4)
#define NINE_STATE_DSA         (1 <<  5)
#define NINE_STATE_VS          (1 <<  6)
#define NINE_STATE_VS_CONST    (1 <<  7)
#define NINE_STATE_PS          (1 <<  8)
#define NINE_STATE_PS_CONST    (1 <<  9)
#define NINE_STATE_TEXTURE     (1 << 10)
#define NINE_STATE_SAMPLER     (1 << 11)
#define NINE_STATE_VDECL       (1 << 12)
#define NINE_STATE_IDXBUF      (1 << 13)
#define NINE_STATE_PRIM        (1 << 14)
#define NINE_STATE_MATERIAL    (1 << 15)
#define NINE_STATE_BLEND_COLOR (1 << 16)
#define NINE_STATE_SAMPLE_MASK (1 << 17)
#define NINE_STATE_FF          (1 << 18)
#define NINE_STATE_MISC_CONST  (1 << 19)
#define NINE_STATE_ALL          0xfffff

#define NINE_MAX_SIMULTANEOUS_RENDERTARGETS 4
#define NINE_MAX_CONST_F 256
#define NINE_MAX_CONST_I 16
#define NINE_MAX_CONST_B 16

#define NINE_CONST_F_BASE_IDX  0
#define NINE_CONST_I_BASE_IDX  NINE_MAX_CONST_F
#define NINE_CONST_B_BASE_IDX (NINE_MAX_CONST_F + NINE_MAX_CONST_I)

#define NINE_MAX_SAMPLERS 16
#define NINE_MAX_TEXTURES 16

struct nine_state
{
    struct {
        uint32_t group;
        uint32_t rs[(NINED3DRS_LAST + 1 + 31) / 32];
        uint32_t vtxbuf;
        uint32_t stream_freq;
        uint32_t texture_state[NINE_MAX_TEXTURES];
        uint16_t sampler_state[NINE_MAX_SAMPLERS];
        uint32_t vs_const_f[(NINE_MAX_CONST_F + 31) / 32]; /* ref: 224 in PS */
        uint32_t ps_const_f[(NINE_MAX_CONST_F + 31) / 32];
        uint16_t vs_const_i; /* NINE_MAX_CONST_I == 16 */
        uint16_t ps_const_i;
        uint16_t vs_const_b; /* NINE_MAX_CONST_B == 16 */
        uint16_t ps_const_b;
        uint8_t ucp;
    } changed;

    struct NineSurface9 *rt[NINE_MAX_SIMULTANEOUS_RENDERTARGETS];
    struct NineSurface9 *ds;

    D3DVIEWPORT9 viewport;

    struct pipe_scissor_state scissor;

    struct NineVertexShader9 *vs;
    float *vs_const_f;
    int    vs_const_i[NINE_MAX_CONST_I][4];
    BOOL   vs_const_b[NINE_MAX_CONST_B];

    struct NinePixelShader9 *ps;
    float *ps_const_f;
    int    ps_const_i[NINE_MAX_CONST_I][4];
    BOOL   ps_const_b[NINE_MAX_CONST_B];

    struct {
        void *rast;
        void *dsa;
        void *blend;
        void *samp[PIPE_MAX_SAMPLERS];
    } cso;

    struct NineVertexDeclaration9 *vdecl;

    struct NineIndexBuffer9   *idxbuf;
    struct NineVertexBuffer9  *stream[PIPE_MAX_ATTRIBS];
    struct pipe_vertex_buffer  vtxbuf[PIPE_MAX_ATTRIBS];
    UINT stream_freq[PIPE_MAX_ATTRIBS];
    uint32_t stream_instancedata_mask; /* derived from stream_freq */
    uint32_t stream_usage_mask; /* derived from VS and vdecl */

    struct pipe_clip_state clip;
    struct pipe_framebuffer_state fb;

    DWORD rs[NINED3DRS_LAST + 1];

    DWORD samp[NINED3DSAMP_LAST + 1];
};

/* map D3DRS -> log2(NINE_STATE_x) (do we need this ?)
 */
const uint32_t nine_render_state_group[NINED3DRS_LAST + 1];

/* for D3DSBT_PIXEL/VERTEX:
 */
const uint32_t nine_render_states_pixel[(NINED3DRS_LAST + 31) / 32];
const uint32_t nine_render_states_vertex[(NINED3DRS_LAST + 31) / 32];

struct NineDevice9;

boolean nine_update_state(struct NineDevice9 *);

void nine_state_set_defaults(struct nine_state *, D3DCAPS9 *);


#endif /* _NINE_STATE_H_ */
