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

#include "stateblock9.h"
#include "device9.h"
#include "nine_helpers.h"

#define DBG_CHANNEL DBG_STATEBLOCK

HRESULT
NineStateBlock9_ctor( struct NineStateBlock9 *This,
                      struct NineUnknownParams *pParams,
                      struct NineDevice9 *pDevice,
                      enum nine_stateblock_type type )
{
    HRESULT hr = NineUnknown_ctor(&This->base, pParams);
    if (FAILED(hr))
        return hr;

    nine_reference_set(&This->device, pDevice);

    This->type = type;

    This->state.vs_const_f = MALLOC(pDevice->constbuf_vs->width0);
    This->state.ps_const_f = MALLOC(pDevice->constbuf_ps->width0);
    if (!This->state.vs_const_f || !This->state.ps_const_f)
        return E_OUTOFMEMORY;

    return D3D_OK;
}

void
NineStateBlock9_dtor( struct NineStateBlock9 *This )
{
    struct nine_state *state = &This->state;
    unsigned i;

    nine_reference(&state->idxbuf, NULL);

    if (state->changed.vtxbuf) {
        for (i = 0; i < PIPE_MAX_ATTRIBS; ++i)
            nine_reference(&state->stream[i], NULL);
    }

    nine_reference(&state->vs, NULL);
    nine_reference(&state->ps, NULL);

    nine_reference(&state->vdecl, NULL);

    if (state->vs_const_f)
        FREE(state->vs_const_f);
    if (state->ps_const_f)
        FREE(state->ps_const_f);

    nine_reference(&This->device, NULL);

    NineUnknown_dtor(&This->base);
}

HRESULT WINAPI
NineStateBlock9_GetDevice( struct NineStateBlock9 *This,
                           IDirect3DDevice9 **ppDevice )
{
    user_assert(ppDevice, E_POINTER);
    NineUnknown_AddRef(NineUnknown(This->device));
    *ppDevice = (IDirect3DDevice9 *)This->device;
    return D3D_OK;
}

/* Fast path for all state. */
static void
nine_state_transfer_all(struct nine_state *dst,
                        struct nine_state *src)
{
   /* TODO */
}

/* Copy state marked changed in @mask from @src to @dst.
 * If @apply is false, updating dst->changed can be omitted.
 * TODO: compare ?
 */
static void
nine_state_transfer(struct nine_state *dst,
                    const struct nine_state *src,
                    struct nine_state *mask,
                    const boolean apply)
{
    unsigned i, j, s;

    if (mask->changed.group & NINE_STATE_VIEWPORT)
        dst->viewport = src->viewport;
    if (mask->changed.group & NINE_STATE_SCISSOR)
        dst->scissor = src->scissor;
    if (mask->changed.group & NINE_STATE_VS)
        nine_reference(&dst->vs, src->vs);
    if (mask->changed.group & NINE_STATE_PS)
        nine_reference(&dst->ps, src->ps);
    if ((mask->changed.group & NINE_STATE_VDECL) && (!apply || src->vdecl))
        nine_reference(&dst->vdecl, src->vdecl);
    if (mask->changed.group & NINE_STATE_IDXBUF)
        nine_reference(&dst->idxbuf, src->idxbuf);

    if (apply)
       dst->changed.group |= mask->changed.group;

    /* Vertex constants.
     *
     * Various possibilities for optimization here, like creating a per-SB
     * constant buffer, or memcmp'ing for changes.
     * Will do that later depending on what works best for specific apps.
     */
    if (mask->changed.group & NINE_STATE_VS_CONST) {
       for (i = 0; i < Elements(mask->changed.vs_const_f); ++i) {
          uint32_t m;
          if (!mask->changed.vs_const_f[i])
             continue;
          if (apply)
             dst->changed.vs_const_f[i] |= mask->changed.vs_const_f[i];
          m = mask->changed.vs_const_f[i];

          if (m == 0xFFFFFFFF) {
             memcpy(&dst->vs_const_f[i * 32 * 4],
                    &src->vs_const_f[i * 32 * 4], 32 * 4 * sizeof(float));
             continue;
          }
          for (j = ffs(m) - 1, m >>= j; m; ++j, m >>= 1)
             if (m & 1)
                memcpy(&dst->vs_const_f[(i * 32 + j) * 4],
                       &src->vs_const_f[(i * 32 + j) * 4], 4 * sizeof(float));
       }
    }
    if (mask->changed.vs_const_i) {
       uint16_t m = mask->changed.vs_const_i;
       for (i = ffs(m) - 1, m >>= i; m; ++i, m >>= 1)
          if (m & 1)
             memcpy(dst->vs_const_i[i], src->vs_const_i[i], 4 * sizeof(int));
       if (apply)
          dst->changed.vs_const_i |= mask->changed.vs_const_i;
    }
    if (mask->changed.vs_const_b) {
       uint16_t m = mask->changed.vs_const_b;
       for (i = ffs(m) - 1, m >>= i; m; ++i, m >>= 1)
          if (m & 1)
             dst->vs_const_b[i] = src->vs_const_b[i];
       if (apply)
          dst->changed.vs_const_b |= mask->changed.vs_const_b;
    }

    /* Pixel constants. */
    if (mask->changed.group & NINE_STATE_PS_CONST) {
       for (i = 0; i < Elements(mask->changed.ps_const_f); ++i) {
          uint32_t m;
          if (!mask->changed.ps_const_f[i])
             continue;
          if (apply)
             dst->changed.ps_const_f[i] |= mask->changed.ps_const_f[i];
          m = mask->changed.ps_const_f[i];

          if (m == 0xFFFFFFFF) {
             memcpy(&dst->ps_const_f[i * 32 * 4],
                    &src->ps_const_f[i * 32 * 4], 32 * 4 * sizeof(float));
             continue;
          }
          for (j = ffs(m) - 1, m >>= j; m; ++j, m >>= 1)
             if (m & 1)
                memcpy(&dst->ps_const_f[(i * 32 + j) * 4],
                       &src->ps_const_f[(i * 32 + j) * 4], 4 * sizeof(float));
       }
    }
    if (mask->changed.ps_const_i) {
       uint16_t m = mask->changed.ps_const_i;
       for (i = ffs(m) - 1, m >>= i; m; ++i, m >>= 1)
          if (m & 1)
             memcpy(dst->ps_const_i[i], src->ps_const_i[i], 4 * sizeof(int));
       if (apply)
          dst->changed.ps_const_i |= mask->changed.ps_const_i;
    }
    if (mask->changed.ps_const_b) {
       uint16_t m = mask->changed.ps_const_b;
       for (i = ffs(m) - 1, m >>= i; m; ++i, m >>= 1)
          if (m & 1)
             dst->ps_const_b[i] = src->ps_const_b[i];
       if (apply)
          dst->changed.ps_const_b |= mask->changed.ps_const_b;
    }

    /* Render states. */
    /* TODO: speed this up */
    for (i = 0; i <= NINED3DRS_LAST; ++i) {
        if (!mask->changed.rs[i / 32]) {
            i += 31;
            continue;
        }
        if (i % 32 == 0)
           i += ffs(mask->changed.rs[i / 32]) - 1;
        if (mask->changed.rs[i / 32] & (1 << (i % 32)))
            dst->rs[i] = src->rs[i];
    }
    if (apply) {
       for (i = 0; i < Elements(dst->changed.rs); ++i)
          dst->changed.rs[i] |= mask->changed.rs[i];
    }

    /* Vertex streams. */
    if (mask->changed.vtxbuf | mask->changed.stream_freq) {
        for (i = 0; i < PIPE_MAX_ATTRIBS; ++i) {
            if (mask->changed.vtxbuf & (1 << i))
                nine_reference(&dst->stream[i], src->stream[i]);
            if (mask->changed.stream_freq & (1 << i))
                dst->stream_freq[i] = src->stream_freq[i];
        }
        dst->stream_instancedata_mask &= ~mask->changed.stream_freq;
        dst->stream_instancedata_mask |=
            src->stream_instancedata_mask & mask->changed.stream_freq;
        if (apply) {
           dst->changed.vtxbuf      |= mask->changed.vtxbuf;
           dst->changed.stream_freq |= mask->changed.stream_freq;
        }
    }

    /* Clip planes. */
    if (mask->changed.ucp) {
        for (i = 0; i < PIPE_MAX_CLIP_PLANES; ++i)
            if (mask->changed.ucp & (1 << i))
                memcpy(dst->clip.ucp[i],
                       src->clip.ucp[i], sizeof(src->clip.ucp[0]));
        if (apply)
           dst->changed.ucp |= mask->changed.ucp;
    }

    /* Texture and sampler state. */
    if (mask->changed.texture) {
        for (s = 0; s < NINE_MAX_SAMPLERS; ++s)
            if (mask->changed.texture & (1 << s))
                nine_reference(&dst->texture[s], src->texture[s]);
        if (apply)
            dst->changed.texture |= mask->changed.texture;
    }
    if (mask->changed.group & NINE_STATE_SAMPLER) {
        for (s = 0; s < NINE_MAX_SAMPLERS; ++s) {
            if (mask->changed.sampler[s] == 0x3ffe) {
                memcpy(&dst->samp[s], &src->samp[s], sizeof(dst->samp[s]));
            } else {
                for (i = 0; i <= NINED3DSAMP_LAST; ++i)
                    if (mask->changed.sampler[s] & (1 << i))
                        dst->samp[s][i] = src->samp[s][i];
            }
            if (apply)
                dst->changed.sampler[s] |= mask->changed.sampler[s];
        }
    }

    if (!(mask->changed.group & NINE_STATE_FF))
        return;

    /* Fixed function state. */
    if (mask->changed.group & NINE_STATE_FF_MATERIAL) {
        dst->ff.material = src->ff.material;
        if (apply)
            dst->changed.group |= NINE_STATE_FF_MATERIAL;
    }
    if (mask->changed.group & NINE_STATE_FF_PSSTAGES) {
        for (s = 0; s < NINE_MAX_SAMPLERS; ++s) {
            for (i = 0; i < NINED3DTSS_COUNT; ++i)
                if (mask->ff.changed.tex_stage[s][i / 32] & (1 << (i % 32)))
                    dst->ff.tex_stage[s][i] = src->ff.tex_stage[s][i];
            if (apply) {
                /* TODO: it's 32 exactly, just offset by 1 as 0 is unused */
                dst->ff.changed.tex_stage[s][0] |=
                    mask->ff.changed.tex_stage[s][0];
                dst->ff.changed.tex_stage[s][1] |=
                    mask->ff.changed.tex_stage[s][1];
            }
        }
    }
    if (mask->changed.group & NINE_STATE_FF_LIGHTING) {
        if (dst->ff.num_lights < mask->ff.num_lights) {
            dst->ff.light = REALLOC(dst->ff.light,
                                    dst->ff.num_lights * sizeof(D3DLIGHT9),
                                    mask->ff.num_lights * sizeof(D3DLIGHT9));
            dst->ff.num_lights = mask->ff.num_lights;
        }
        for (i = 0; i < mask->ff.num_lights; ++i)
            if (mask->ff.light[i].Type != NINED3DLIGHT_INVALID)
                dst->ff.light[i] = src->ff.light[i];

        DBG("TODO: active lights\n");
    }
    if (mask->changed.group & NINE_STATE_FF_VSTRANSF) {
        for (i = 0; i < Elements(mask->ff.changed.transform); ++i) {
            if (!mask->ff.changed.transform[i])
                continue;
            for (s = i * 32; s < (i * 32 + 32); ++s) {
                if (!(mask->ff.changed.transform[i] & (1 << (s % 32))))
                    continue;
                *nine_state_access_transform(dst, s, TRUE) =
                    *nine_state_access_transform( /* const because !alloc */
                        (struct nine_state *)src, s, FALSE);
            }
            if (apply)
                dst->ff.changed.transform[i] |= mask->ff.changed.transform[i];
        }
    }
}

/* Capture those bits of current device state that have been changed between
 * BeginStateBlock and EndStateBlock.
 */
HRESULT WINAPI
NineStateBlock9_Capture( struct NineStateBlock9 *This )
{
    DBG("This=%p\n", This);

    nine_state_transfer(&This->state, &This->device->state, &This->state,
                        FALSE);
    return D3D_OK;
}

/* Set state managed by this StateBlock as current device state. */
HRESULT WINAPI
NineStateBlock9_Apply( struct NineStateBlock9 *This )
{
    DBG("This=%p\n", This);

    if (This->type == NINESBT_ALL && 0) /* TODO */
       nine_state_transfer_all(&This->device->state, &This->state);
    nine_state_transfer(&This->device->state, &This->state, &This->state, TRUE);
    return D3D_OK;
}

IDirect3DStateBlock9Vtbl NineStateBlock9_vtable = {
    (void *)NineUnknown_QueryInterface,
    (void *)NineUnknown_AddRef,
    (void *)NineUnknown_Release,
    (void *)NineStateBlock9_GetDevice,
    (void *)NineStateBlock9_Capture,
    (void *)NineStateBlock9_Apply
};

static const GUID *NineStateBlock9_IIDs[] = {
    &IID_IDirect3DStateBlock9,
    &IID_IUnknown,
    NULL
};

HRESULT
NineStateBlock9_new( struct NineDevice9 *pDevice,
                     struct NineStateBlock9 **ppOut,
                     enum nine_stateblock_type type)
{
    NINE_NEW(NineStateBlock9, ppOut, pDevice, type);
}
