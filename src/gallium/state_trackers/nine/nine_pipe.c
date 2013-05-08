/*
 * Copyright 2011 Joakim Sindholt <opensource@zhasha.com>
 * Copyright 2013 Christoph Bumiller
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

static void
convert_dsa_state()
{
    struct pipe_depth_stencil_alpha_state dsa;

    dsa.depth.enabled = !!state[D3DRS_ZENABLE];
    dsa.depth.writemask = !!state[D3DRS_ZWRITEENABLE];
    dsa.depth.func = d3dcmpfunc_to_pipe_func(state[D3DRS_ZFUNC]);

    dsa.stencil[0].enabled = !!state[D3RDRS_STENCILENABLE];
    dsa.stencil[0].func = d3dcmpfunc_to_pipe_func(state[D3DRS_STENCILFUNC]);
    dsa.stencil[0].fail_op = d3dstencilop_to_pipe_stencil_op(state[D3DRS_STENCILFAIL]);
    dsa.stencil[0].zpass_op = d3dstencilop_to_pipe_stencil_op(state[D3DRS_STENCILPASS]);
    dsa.stencil[0].zfail_op = d3dstencilop_to_pipe_stencil_op(state[D3DRS_STENCILZFAIL]);
    dsa.stencil[0].valuemask = state[D3DRS_STENCILMASK];
    dsa.stencil[0].writemask = state[D3DRS_STENCILWRITEMASK];

    dsa.stencil[1].enabled = !!state[D3RDRS_TWOSIDEDSTENCILMODE];
    if (dsa.stencil[1].enabled) {
        dsa.stencil[1].func = d3dcmpfunc_to_pipe_func(state[D3DRS_CCW_STENCILFUNC]);
        dsa.stencil[1].fail_op = d3dstencilop_to_pipe_stencil_op(state[D3DRS_CCW_STENCILFAIL]);
        dsa.stencil[1].zpass_op = d3dstencilop_to_pipe_stencil_op(state[D3DRS_CCW_STENCILPASS]);
        dsa.stencil[1].zfail_op = d3dstencilop_to_pipe_stencil_op(state[D3DRS_CCW_STENCILZFAIL]);
        dsa.stencil[1].valuemask = dsa.stencil[0].valuemask;
        dsa.stencil[1].writemask = dsa.stencil[0].writemask;
    }

    dsa.alpha.enabled = !!state[D3DRS_ALPHATESTENABLE];
    dsa.alpha.func = d3dcmpfunc_to_pipe_func(state[D3DRS_ALPHAFUNC]);
    dsa.alpha.ref_value = (float)state[D3DRS_ALPHAREF] / 255.0f;

    cso_set_depth_stencil_alpha(cso, &dsa);
}

static void
convert_rasterizer_state()
{
    struct pipe_rasterizer_state rast;

    rast.flatshade = state[D3DRS_SHADEMODE] == D3DSHADE_FLAT;
    rast.light_twoside = 0;
    rast.clamp_vertex_color = 1;
    rast.clamp_fragment_color = 1; /* XXX */
    rast.front_ccw = 0;
    rast.cull_face = d3dcull_to_pipe_face(state[D3DRS_CULLMODE]);
    rast.fill_front = d3dfillmode_to_pipe_polygon_mode(state[D3DRS_FILLMODE]);
    rast.fill_back = rast.fill_front;
    rast.offset_point = 0; /* XXX */
    rast.offset_line = 0; /* XXX */
    rast.offset_tri = 1;
    rast.scissor = !!state[D3DRS_SCISSORTESTENABLE];
    rast.poly_smooth = 0;
    rast.poly_stipple_enable = 0;
    rast.point_smooth = 0;
    rast.sprite_coord_mode = PIPE_SPRITE_COORD_UPPER_LEFT;
    rast.point_quad_rasterization = !!state[D3DRS_POINTSPRITEENABLE];
    rast.point_size_per_vertex = ;
    rast.multisample = !!state[D3DRS_MULTISAMPLEANTIALIAS];
    rast.line_smooth = !!state[D3DRS_ANTIALIASEDLINEENABLE];
    rast.line_stipple_enable = 0;
    rast.line_last_pixel = !!state[D3DRS_LASTPIXEL];
    rast.flatshade_first = 1;
    rast.half_pixel_center = 0;
    rast.lower_left_origin = 0;
    rast.bottom_edge_rule = 0;
    rast.rasterizer_discard = 0;
    rast.depth_clip = 1;
    rast.clip_halfz = 1;
    rast.clip_plane_enable = state[D3DRS_CLIPPLANEENABLE];
    rast.line_stipple_factor = 0;
    rast.line_stipple_pattern = 0;
    rast.sprite_coord_enable = 0x00;
    rast.line_width = 1.0f;
    rast.point_size = asfloat(state[D3DRS_POINTSIZE]); /* XXX: D3DRS_POINTSIZE_MIN/MAX */
    rast.offset_units = asfloat(state[D3DRS_DEPTHBIAS]);
    rast.offset_scale = asfloat(state[D3DRS_SLOPESCALEDEPTHBIAS]);
    rast.offset_clamp = 0.0f;

    pipe->set_sample_mask(pipe, state[D3DRS_MULTISAMPLEMASK]);
}

static void
convert_blend_state()
{
    struct pipe_blend_state blend;
    struct pipe_blend_color blend_color;

    memset(&blend, 0, sizeof(blend));

    blend.dither = !!state[D3DRS_DITHERENABLE];

    blend.alpha_to_one = 0;
    blend.alpha_to_coverage = 0; /* XXX */

    blend.rt[0].blend_enable = !!state[D3DRS_ALPHABLENDENABLE];
    if (blend.rt[0].blend_enable) {
        blend.rt[0].rgb_func = d3dblendop_to_pipe_blend(state[D3DRS_BLENDOP]);
        blend.rt[0].rgb_src_factor = d3dblend_color_to_pipe_blendfactor(state[D3DRS_SRCBLEND]);
        blend.rt[0].rgb_dst_factor = d3dblend_color_to_pipe_blendfactor(state[D3DRS_DESTBLEND]);
        if (state[D3DRS_SEPARATEALPHABLENDENABLE]) {
            blend.rt[0].alpha_func = d3dblendop_to_pipe_blend(state[D3DRS_BLENDOPALPHA]);
            blend.rt[0].alpha_src_factor = d3dblend_alpha_to_pipe_blendfactor(state[D3DRS_SRCBLENDALPHA]);
            blend.rt[0].alpha_dst_factor = d3dblend_alpha_to_pipe_blendfactor(state[D3DRS_DESTBLENDALPHA]);
        } else {
            blend.rt[0].alpha_func = blend.rt[0].rgb_func;
            blend.rt[0].alpha_src_factor = d3dblend_alpha_to_pipe_blendfactor(state[D3DRS_SRCBLEND]);
            blend.rt[0].alpha_dst_factor = d3dblend_alpha_to_pipe_blendfactor(state[D3DRS_DESTBLEND]);
        }
    }
    blend.rt[0].colormask = state[D3DRS_COLORWRITEENABLE];

    if (state[D3DRS_COLORWRITEENABLE1] != state[D3DRS_COLORWRITEENABLE] ||
        state[D3DRS_COLORWRITEENABLE2] != state[D3DRS_COLORWRITEENABLE] ||
        state[D3DRS_COLORWRITEENABLE3] != state[D3DRS_COLORWRITEENABLE]) {
        unsigned i;
        blend.independent_blend_enable = TRUE;
        for (i = 1; i < 4; ++i)
            blend.rt[i] = blend.rt[0];
        blend.rt[1].colormask = state[D3DRS_COLORWRITEENABLE1];
        blend.rt[2].colormask = state[D3DRS_COLORWRITEENABLE2];
        blend.rt[3].colormask = state[D3DRS_COLORWRITEENABLE3];
    }

    blend.force_srgb = !!state[D3DRS_SRGBWRITEENABLE];

    if (dirty & GD3D9_UPDATE_BLEND_COLOR) {
        blend_color.color[0] = ((state[D3RDS_BLENDFACTOR] >>  0) & 0xff) / 255.0f;
        blend_color.color[1] = ((state[D3RDS_BLENDFACTOR] >>  8) & 0xff) / 255.0f;
        blend_color.color[2] = ((state[D3RDS_BLENDFACTOR] >> 16) & 0xff) / 255.0f;
        blend_color.color[3] = ((state[D3RDS_BLENDFACTOR] >> 24) & 0xff) / 255.0f;

        pipe->set_blend_color(pipe, &blend_color);
    }
}

static void
convert_sampler_state()
{
    struct pipe_sampler_state samp;

    samp.wrap_s = d3dtaddress_to_pipe_tex_wrap(state[D3DSAMP_ADDRESSU]);
    samp.wrap_t = d3dtaddress_to_pipe_tex_wrap(state[D3DSAMP_ADDRESSV]);
    samp.wrap_r = d3dtaddress_to_pipe_tex_wrap(state[D3DSAMP_ADDRESSW]);
    samp.min_mip_filter = d3dtexturefiltertype_to_pipe_tex_mipfilter(state[D3DSAMP_MIPFILTER]);
    samp.min_img_filter = d3dtexturefiltertype_to_pipe_tex_filter(state[D3DSAMP_MINFILTER]);
    samp.mag_img_filter = d3dtexturefiltertype_to_pipe_tex_filter(state[D3DSAMP_MAGFILTER]);
    samp.compare_mode = PIPE_TEX_COMPARE_NONE;
    samp.compare_func = PIPE_FUNC_NEVER;
    samp.normalized_coords = 1;
    samp.max_anisotropy = state[D3DSAMP_MAXANISOTROPY];
    samp.seamless_cube_map = 1;
    samp.lod_bias = asfloat(state[D3DSAMP_MIPMAPLODBIAS]);
    samp.min_lod = 0.0f;
    samp.max_lod = state[D3DSAMP_MAXMIPLEVEL];
    samp.border_color.f[0] = ((state[D3DSAMP_BORDERCOLOR] >>  0) & 0xff) / 255.0f;
    samp.border_color.f[1] = ((state[D3DSAMP_BORDERCOLOR] >>  8) & 0xff) / 255.0f;
    samp.border_color.f[2] = ((state[D3DSAMP_BORDERCOLOR] >> 16) & 0xff) / 255.0f;
    samp.border_color.f[3] = ((state[D3DSAMP_BORDERCOLOR] >> 24) & 0xff) / 255.0f;
}
