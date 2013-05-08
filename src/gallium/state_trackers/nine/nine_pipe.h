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

#ifndef _NINE_PIPE_H_
#define _NINE_PIPE_H_

#include "d3d9.h"
#include "pipe/p_format.h"

static inline float asfloat(DWORD value)
{
    union {
        float f;
        DWORD w;
    } u;
    u.w = value;
    return u.f;
}

D3DFORMAT pipe_to_d3d9_format(enum pipe_format format)
{
    static const D3DFORMAT map[PIPE_FORMAT_COUNT] =
    {
        [PIPE_FORMAT_NONE]               = D3DFMT_UNKNOWN,

     /* [PIPE_FORMAT_B8G8R8_UNORM]       = D3DFMT_R8G8B8, */
        [PIPE_FORMAT_B8G8R8A8_UNORM]     = D3DFMT_A8R8G8B8,
        [PIPE_FORMAT_B8G8R8X8_UNORM]     = D3DFMT_X8R8G8B8,
        [PIPE_FORMAT_B5G6R5_UNORM]       = D3DFMT_R5G6B5,
        [PIPE_FORMAT_B5G5R5X1_UNORM]     = D3DFMT_X1R5G5B5,
        [PIPE_FORMAT_B5G5R5A1_UNORM]     = D3DFMT_A1R5G5B5,
        [PIPE_FORMAT_B4G4R4A4_UNORM]     = D3DFMT_A4R4G4B4,
        [PIPE_FORMAT_B2G3R3_UNORM]       = D3DFMT_R3G3B2,
        [PIPE_FORMAT_A8_UNORM]           = D3DFMT_A8,
     /* [PIPE_FORMAT_B2G3R3A8_UNORM]     = D3DFMT_A8R3G3B2, */
        [PIPE_FORMAT_B4G4R4X4_UNORM]     = D3DFMT_X4R4G4B4,
        [PIPE_FORMAT_R10G10B10A2_UNORM]  = D3DFMT_A2B10G10R10,
        [PIPE_FORMAT_R8G8B8A8_UNORM]     = D3DFMT_A8B8G8R8,
        [PIPE_FORMAT_R8G8B8X8_UNORM]     = D3DFMT_X8B8G8R8,
        [PIPE_FORMAT_R16G16_UNORM]       = D3DFMT_G16R16,
        [PIPE_FORMAT_B10G10R10A2_UNORM]  = D3DFMT_A2R10G10B10,
        [PIPE_FORMAT_R16G16B16A16_UNORM] = D3DFMT_A16B16G16R16,

        [PIPE_FORMAT_R8_UINT]            = D3DFMT_P8,
        [PIPE_FORMAT_R8A8_UINT]          = D3DFMT_A8P8,

        [PIPE_FORMAT_L8_UNORM]           = D3DFMT_L8,
        [PIPE_FORMAT_L8A8_UNORM]         = D3DFMT_A8L8,
        [PIPE_FORMAT_L4A4_UNORM]         = D3DFMT_A4L4,

        [PIPE_FORMAT_R8G8_SNORM]           = D3DFMT_V8U8,
     /* [PIPE_FORMAT_?]                    = D3DFMT_L6V5U5, */
     /* [PIPE_FORMAT_?]                    = D3DFMT_X8L8V8U8, */
        [PIPE_FORMAT_R8G8B8A8_SNORM]       = D3DFMT_Q8W8V8U8,
        [PIPE_FORMAT_R16G16_SNORM]         = D3DFMT_V16U16,
        [PIPE_FORMAT_R10SG10SB10SA2U_NORM] = D3DFMT_A2W10V10U10,

        [PIPE_FORMAT_YUYV]               = D3DFMT_UYVY,
     /* [PIPE_FORMAT_YUY2]               = D3DFMT_YUY2, */
        [PIPE_FORMAT_DXT1_RGBA]          = D3DFMT_DXT1,
     /* [PIPE_FORMAT_DXT2_RGBA]          = D3DFMT_DXT2, */
        [PIPE_FORMAT_DXT3_RGBA]          = D3DFMT_DXT3,
     /* [PIPE_FORMAT_DXT4_RGBA]          = D3DFMT_DXT4, */
        [PIPE_FORMAT_DXT5_RGBA]          = D3DFMT_DXT5,
     /* [PIPE_FORMAT_?]                  = D3DFMT_MULTI2_ARGB8, (MET) */
        [PIPE_FORMAT_R8G8_B8G8_UNORM]    = D3DFMT_R8G8_B8G8, /* XXX: order */
        [PIPE_FORMAT_G8R8_G8B8_UNORM]    = D3DFMT_G8R8_G8B8,

        [PIPE_FORMAT_Z16_UNORM]          = D3DFMT_D16_LOCKABLE,
        [PIPE_FORMAT_Z32_UNORM]          = D3DFMT_D32,
     /* [PIPE_FORMAT_Z15_UNORM_S1_UINT]  = D3DFMT_D15S1, */
        [PIPE_FORMAT_Z24_UNORM_S8_UINT]  = D3DFMT_D24S8,
        [PIPE_FORMAT_Z24X8_UNORM]        = D3DFMT_D24X8,
        [PIPE_FORMAT_L16_UNORM]          = D3DFMT_L16,
        [PIPE_FORMAT_Z32_FLOAT]          = D3DFMT_D32F_LOCKABLE,
     /* [PIPE_FORMAT_Z24_FLOAT_S8_UINT]  = D3DFMT_D24FS8, */

        [PIPE_FORMAT_R16_UINT]           = D3DFMT_INDEX16,
        [PIPE_FORMAT_R32_UINT]           = D3DFMT_INDEX32,
        [PIPE_FORMAT_R16G16B16A16_SNORM] = D3DFMT_Q16W16V16U16,

        [PIPE_FORMAT_R16_FLOAT]          = D3DFMT_R16F,
        [PIPE_FORMAT_R32_FLOAT]          = D3DFMT_R32F,
        [PIPE_FORMAT_R16G16_FLOAT]       = D3DFMT_G16R16F,
        [PIPE_FORMAT_R32G32_FLOAT]       = D3DFMT_G32R32F,
        [PIPE_FORMAT_R16G16B16A16_FLOAT] = D3DFMT_A16B16G16R16F,
        [PIPE_FORMAT_R32G32B32A32_FLOAT] = D3DFMT_A32B32G32R32F,

     /* [PIPE_FORMAT_?]                  = D3DFMT_CxV8U8, */
	};
	return map[format];
}

enum pipe_format d3d9_to_pipe_format(D3DFORMAT format)
{
    static const enum pipe_format map[] =
    {
        [D3DFMT_UNKNOWN] = PIPE_FORMAT_NONE,

        [D3DFMT_A8R8G8B8]     = PIPE_FORMAT_B8G8R8A8_UNORM,
        [D3DFMT_X8R8G8B8]     = PIPE_FORMAT_B8G8R8X8_UNORM,
        [D3DFMT_R5G6B5]       = PIPE_FORMAT_B5G6R5_UNORM,
        [D3DFMT_X1R5G5B5]     = PIPE_FORMAT_B5G5R5X1_UNORM,
        [D3DFMT_A1R5G5B5]     = PIPE_FORMAT_B5G5R5A1_UNORM,
        [D3DFMT_A4R4G4B4]     = PIPE_FORMAT_B4G4R4A4_UNORM,
        [D3DFMT_A8]           = PIPE_FORMAT_A8_UNORM,
        [D3DFMT_X4R4G4B4]     = PIPE_FORMAT_B4G4R4X4_UNORM,
        [D3DFMT_R3G3B2]       = PIPE_FORMAT_B2G3R3_UNORM,
        [D3DFMT_A2B10G10R10]  = PIPE_FORMAT_R10G10B10A2_UNORM,
        [D3DFMT_A8B8G8R8]     = PIPE_FORMAT_R8G8B8A8_UNORM,
        [D3DFMT_X8B8G8R8]     = PIPE_FORMAT_R8G8B8X8_UNORM,
        [D3DFMT_G16R16]       = PIPE_FORMAT_R16G16_UNORM,
        [D3DFMT_A2R10G10B10]  = PIPE_FORMAT_B10G10R10A2_UNORM,
        [D3DFMT_A16B16G16R16] = PIPE_FORMAT_R16G16B16A16_UNORM,

        [D3DFMT_P8] = PIPE_FORMAT_R8_UINT,

        [D3DFMT_L8]   = PIPE_FORMAT_L8_UNORM,
        [D3DFMT_A8L8] = PIPE_FORMAT_L8A8_UNORM,
        [D3DFMT_A4L4] = PIPE_FORMAT_L4A4_UNORM,

        [D3DFMT_V8U8]        = PIPE_FORMAT_R8G8_SNORM,
        [D3DFMT_Q8W8V8U8]    = PIPE_FORMAT_R8G8B8A8_SNORM,
        [D3DFMT_V16U16]      = PIPE_FORMAT_R16G16_SNORM,
        [D3DFMT_A2W10V10U10] = PIPE_FORMAT_R10SG10SB10SA2U_NORM,

        [D3DFMT_UYVY] = PIPE_FORMAT_YUYV,

        [D3DFMT_DXT1] = PIPE_FORMAT_DXT1_RGBA,
        [D3DFMT_DXT2] = PIPE_FORMAT_DXT3_RGBA, /* XXX */
        [D3DFMT_DXT3] = PIPE_FORMAT_DXT3_RGBA,
        [D3DFMT_DXT4] = PIPE_FORMAT_DXT5_RGBA, /* XXX */
        [D3DFMT_DXT5] = PIPE_FORMAT_DXT5_RGBA,

        [D3DFMT_G8R8_G8B8] = PIPE_FORMAT_G8R8_G8B8_UNORM, /* XXX: order ? */
        [D3DFMT_R8G8_B8G8] = PIPE_FORMAT_R8G8_B8G8_UNORM,

        [D3DFMT_D16_LOCKABLE]  = PIPE_FORMAT_Z16_UNORM,
        [D3DFMT_D32]           = PIPE_FORMAT_Z32_UNORM,
        [D3DFMT_D24S8]         = PIPE_FORMAT_Z24_UNORM_S8_UINT,
        [D3DFMT_D24X8]         = PIPE_FORMAT_Z24X8_UNORM,
        [D3DFMT_D16]           = PIPE_FORMAT_Z16_UNORM,
        [D3DFMT_L16]           = PIPE_FORMAT_L16_UNORM,
        [D3DFMT_D32F_LOCKABLE] = PIPE_FORMAT_Z32_FLOAT,

        [D3DFMT_INDEX16]      = PIPE_FORMAT_R16_UINT,
        [D3DFMT_INDEX32]      = PIPE_FORMAT_R32_UINT,
        [D3DFMT_Q16W16V16U16] = PIPE_FORMAT_R16G16B16A16_SNORM,

        [D3DFMT_R16F]          = PIPE_FORMAT_R16_FLOAT,
        [D3DFMT_R32F]          = PIPE_FORMAT_R32_FLOAT,
        [D3DFMT_G16R16F]       = PIPE_FORMAT_R16G16_FLOAT,
        [D3DFMT_G32R32F]       = PIPE_FORMAT_R32G32_FLOAT,
        [D3DFMT_A16B16G16R16F] = PIPE_FORMAT_R16G16B16A16_FLOAT,
        [D3DFMT_A32B32G32R32F] = PIPE_FORMAT_R32G32B32A32_FLOAT,

        /* non-1:1 formats */
        [D3DFMT_R8G8B8]   = PIPE_FORMAT_B8G8R8X8_UNORM,
        [D3DFMT_A8R3G3B2] = PIPE_FORMAT_B8G8R8A8_UNORM,

        [D3DFMT_A8P8]     = PIPE_FORMAT_R8A8_UINT,

        [D3DFMT_D15S1]    = PIPE_FORMAT_Z24_UNORM_S8_UINT,
        [D3DFMT_D24X4S4]  = PIPE_FORMAT_Z24_UNORM_S8_UINT,
        [D3DFMT_D24FS8]   = PIPE_FORMAT_Z32_FLOAT_S8X24_UINT,

        /* not really formats */
        [D3DFMT_VERTEXDATA]   = PIPE_FORMAT_NONE,
        [D3DFMT_BINARYBUFFER] = PIPE_FORMAT_NONE,

        /* unsupported formats */
        [D3DFMT_L6V5U5]      = PIPE_FORMAT_NONE,
        [D3DFMT_X8L8V8U8]    = PIPE_FORMAT_NONE,

        [D3DFMT_YUY2]        = PIPE_FORMAT_NONE, /* XXX: YUYV ? */
        [D3DFMT_MULTI_ARGB8] = PIPE_FORMAT_NONE, /* MET */

        [D3DFMT_CxV8U8]      = PIPE_FORMAT_NONE,

        [D3DFMT_A1]          = PIPE_FORMAT_NONE, /* add this ? */
    };
    return map[format];
}

static INLINE const char *
d3dformat_to_string(D3DFORMAT fmt)
{
    switch (fmt) {
    case D3DFMT_UNKNOWN: return "D3DFMT_UNKNOWN";
    case D3DFMT_R8G8B8: return "D3DFMT_R8G8B8";
    case D3DFMT_A8R8G8B8: return "D3DFMT_A8R8G8B8";
    case D3DFMT_X8R8G8B8: return "D3DFMT_X8R8G8B8";
    case D3DFMT_R5G6B5: return "D3DFMT_R5G6B5";
    case D3DFMT_X1R5G5B5: return "D3DFMT_X1R5G5B5";
    case D3DFMT_A1R5G5B5: return "D3DFMT_A1R5G5B5";
    case D3DFMT_A4R4G4B4: return "D3DFMT_A4R4G4B4";
    case D3DFMT_R3G3B2: return "D3DFMT_R3G3B2";
    case D3DFMT_A8: return "D3DFMT_A8";
    case D3DFMT_A8R3G3B2: return "D3DFMT_A8R3G3B2";
    case D3DFMT_X4R4G4B4: return "D3DFMT_X4R4G4B4";
    case D3DFMT_A2B10G10R10: return "D3DFMT_A2B10G10R10";
    case D3DFMT_A8B8G8R8: return "D3DFMT_A8B8G8R8";
    case D3DFMT_X8B8G8R8: return "D3DFMT_X8B8G8R8";
    case D3DFMT_G16R16: return "D3DFMT_G16R16";
    case D3DFMT_A2R10G10B10: return "D3DFMT_A2R10G10B10";
    case D3DFMT_A16B16G16R16: return "D3DFMT_A16B16G16R16";
    case D3DFMT_A8P8: return "D3DFMT_A8P8";
    case D3DFMT_P8: return "D3DFMT_P8";
    case D3DFMT_L8: return "D3DFMT_L8";
    case D3DFMT_A8L8: return "D3DFMT_A8L8";
    case D3DFMT_A4L4: return "D3DFMT_A4L4";
    case D3DFMT_V8U8: return "D3DFMT_V8U8";
    case D3DFMT_L6V5U5: return "D3DFMT_L6V5U5";
    case D3DFMT_X8L8V8U8: return "D3DFMT_X8L8V8U8";
    case D3DFMT_Q8W8V8U8: return "D3DFMT_Q8W8V8U8";
    case D3DFMT_V16U16: return "D3DFMT_V16U16";
    case D3DFMT_A2W10V10U10: return "D3DFMT_A2W10V10U10";
    case D3DFMT_UYVY: return "D3DFMT_UYVY";
    case D3DFMT_R8G8_B8G8: return "D3DFMT_R8G8_B8G8";
    case D3DFMT_YUY2: return "D3DFMT_YUY2";
    case D3DFMT_G8R8_G8B8: return "D3DFMT_G8R8_G8B8";
    case D3DFMT_DXT1: return "D3DFMT_DXT1";
    case D3DFMT_DXT2: return "D3DFMT_DXT2";
    case D3DFMT_DXT3: return "D3DFMT_DXT3";
    case D3DFMT_DXT4: return "D3DFMT_DXT4";
    case D3DFMT_DXT5: return "D3DFMT_DXT5";
    case D3DFMT_D16_LOCKABLE: return "D3DFMT_D16_LOCKABLE";
    case D3DFMT_D32: return "D3DFMT_D32";
    case D3DFMT_D15S1: return "D3DFMT_D15S1";
    case D3DFMT_D24S8: return "D3DFMT_D24S8";
    case D3DFMT_D24X8: return "D3DFMT_D24X8";
    case D3DFMT_D24X4S4: return "D3DFMT_D24X4S4";
    case D3DFMT_D16: return "D3DFMT_D16";
    case D3DFMT_D32F_LOCKABLE: return "D3DFMT_D32F_LOCKABLE";
    case D3DFMT_D24FS8: return "D3DFMT_D24FS8";
    case D3DFMT_D32_LOCKABLE: return "D3DFMT_D32_LOCKABLE";
    case D3DFMT_S8_LOCKABLE: return "D3DFMT_S8_LOCKABLE";
    case D3DFMT_L16: return "D3DFMT_L16";
    case D3DFMT_VERTEXDATA: return "D3DFMT_VERTEXDATA";
    case D3DFMT_INDEX16: return "D3DFMT_INDEX16";
    case D3DFMT_INDEX32: return "D3DFMT_INDEX32";
    case D3DFMT_Q16W16V16U16: return "D3DFMT_Q16W16V16U16";
    case D3DFMT_MULTI2_ARGB8: return "D3DFMT_MULTI2_ARGB8";
    case D3DFMT_R16F: return "D3DFMT_R16F";
    case D3DFMT_G16R16F: return "D3DFMT_G16R16F";
    case D3DFMT_A16B16G16R16F: return "D3DFMT_A16B16G16R16F";
    case D3DFMT_R32F: return "D3DFMT_R32F";
    case D3DFMT_G32R32F: return "D3DFMT_G32R32F";
    case D3DFMT_A32B32G32R32F: return "D3DFMT_A32B32G32R32F";
    case D3DFMT_CxV8U8: return "D3DFMT_CxV8U8";
    case D3DFMT_A1: return "D3DFMT_A1";
    case D3DFMT_A2B10G10R10_XR_BIAS: return "D3DFMT_A2B10G10R10_XR_BIAS";
    case D3DFMT_BINARYBUFFER: return "D3DFMT_BINARYBUFFER";
    default:
        break;
    }
    return "Unknown";
}

static INLINE unsigned
nine_fvf_stride( DWORD fvf )
{
    unsigned texcount, i, size = 0;

    switch (fvf & D3DFVF_POSITION_MASK) {
    case D3DFVF_XYZ:    size += 3*4; break;
    case D3DFVF_XYZRHW: size += 4*4; break;
    case D3DFVF_XYZB1:  size += 4*4; break;
    case D3DFVF_XYZB2:  size += 5*4; break;
    case D3DFVF_XYZB3:  size += 6*4; break;
    case D3DFVF_XYZB4:  size += 7*4; break;
    case D3DFVF_XYZB5:  size += 8*4; break;
    case D3DFVF_XYZW:   size += 4*4; break;
    default:
        user_error(!"Position doesn't match any known combination.");
        break;
    }

    if (fvf & D3DFVF_NORMAL)   { size += 3*4; }
    if (fvf & D3DFVF_PSIZE)    { size += 1*4; }
    if (fvf & D3DFVF_DIFFUSE)  { size += 1*4; }
    if (fvf & D3DFVF_SPECULAR) { size += 1*4; }

    texcount = (fvf >> D3DFVF_TEXCOUNT_SHIFT) & D3DFVF_TEXCOUNT_MASK;
    if (user_error(texcount <= 8))
        texcount = 8;

    for (i = 0; i < texcount; ++i) {
        unsigned texformat = (fvf>>(16+i*2))&0x3;
        /* texformats are defined having been shifted around so 1=3,2=0,3=1,4=2
         * meaning we can just do this instead of the switch below */
        size += (((texformat+1)&0x3)+1)*4;

        /*
        switch (texformat) {
        case D3DFVF_TEXTUREFORMAT1: size += 1*4;
        case D3DFVF_TEXTUREFORMAT2: size += 2*4;
        case D3DFVF_TEXTUREFORMAT3: size += 3*4;
        case D3DFVF_TEXTUREFORMAT4: size += 4*4;
        }
        */
    }

    return size;
}

static INLINE void
d3dcolor_to_rgba(float *rgba, D3DCOLOR color)
{
    rgba[0] = (float)((color >> 16) & 0xFF) / 0xFF;
    rgba[1] = (float)((color >>  8) & 0xFF) / 0xFF;
    rgba[2] = (float)((color >>  0) & 0xFF) / 0xFF;
    rgba[3] = (float)((color >> 24) & 0xFF) / 0xFF;
}

static INLINE void
d3dcolor_to_pipe_color_union(union pipe_color_union *rgba, D3DCOLOR color)
{
    d3dcolor_to_rgba(&rgba->f[0], color);
}

static inline unsigned
d3dprimitivetype_to_pipe_prim(D3DPRIMITIVETYPE prim)
{
    switch (prim) {
    case D3DPT_POINTLIST:     return PIPE_PRIM_POINTS;
    case D3DPT_LINELIST:      return PIPE_PRIM_LINES;
    case D3DPT_LINESTRIP:     return PIPE_PRIM_LINE_STRIP;
    case D3DPT_TRIANGLELIST:  return PIPE_PRIM_TRIANGLES;
    case D3DPT_TRIANGLESTRIP: return PIPE_PRIM_TRIANGLE_STRIP;
    case D3DPT_TRIANGLEFAN:   return PIPE_PRIM_TRIANGLE_FAN;
    default:
        assert(0);
        return PIPE_PRIM_POINTS;
    }
}

static inline unsigned
prim_count_to_vertex_count(D3DPRIMITIVETYPE prim, UINT count)
{
    switch (prim) {
    case D3DPT_POINTLIST:     return count;
    case D3DPT_LINELIST:      return count * 2;
    case D3DPT_LINESTRIP:     return count + 1;
    case D3DPT_TRIANGLELIST:  return count * 3;
    case D3DPT_TRIANGLESTRIP: return count + 2;
    case D3DPT_TRIANGLEFAN:   return count + 2;
    default:
        assert(0);
        return 0;
    }
}

static inline unsigned
d3dcmpfunc_to_pipe_func(D3DCMPFUNC func)
{
    switch (func) {
    case D3DCMP_NEVER:        return PIPE_FUNC_NEVER;
    case D3DCMP_LESS:         return PIPE_FUNC_LESS;
    case D3DCMP_EQUAL:        return PIPE_FUNC_EQUAL;
    case D3DCMP_LESSEQUAL:    return PIPE_FUNC_LEQUAL;
    case D3DCMP_GREATER:      return PIPE_FUNC_GREATER;
    case D3DCMP_NOTEQUAL:     return PIPE_FUNC_NOTEQUAL;
    case D3DCMP_GREATEREQUAL: return PIPE_FUNC_GEQUAL;
    case D3DCMP_ALWAYS:       return PIPE_FUNC_ALWAYS;
    default:
        assert(0);
        return PIPE_FUNC_NEVER;
    }
}

static inline unsigned
d3dstencilop_to_pipe_stencil_op(D3DSTENCILOP op)
{
    switch (op) {
    case D3DSTENCILOP_KEEP:    return PIPE_STENCIL_OP_KEEP;
    case D3DSTENCILOP_ZERO:    return PIPE_STENCIL_OP_ZERO;
    case D3DSTENCILOP_REPLACE: return PIPE_STENCIL_OP_REPLACE;
    case D3DSTENCILOP_INCRSAT: return PIPE_STENCIL_OP_INCR;
    case D3DSTENCILOP_DECRSAT: return PIPE_STENCIL_OP_DECR;
    case D3DSTENCILOP_INVERT:  return PIPE_STENCIL_OP_INVERT;
    case D3DSTENCILOP_INCR:    return PIPE_STENCIL_OP_INCR_WRAP;
    case D3DSTENCILOP_DECR:    return PIPE_STENCIL_OP_DECR_WRAP;
    default:
        return PIPE_STENCIL_OP_ZERO;
    }
}

static inline unsigned
d3dcull_to_pipe_face(D3DCULL cull)
{
    switch (cull) {
    case D3DCULL_NONE: return PIPE_FACE_NONE;
    case D3DCULL_CW:   return PIPE_FACE_FRONT;
    case D3DCULL_CCW:  return PIPE_FACE_BACK;
    default:
        assert(0);
        return PIPE_FACE_NONE;
    }
}

static inline unsigned
d3dfillmode_to_pipe_polygon_mode(D3DFILLMODE mode)
{
    switch (mode) {
    case D3DFILLMODE_POINT:     return PIPE_POLYGON_MODE_POINT;
    case D3DFILLMODE_WIREFRAME: return PIPE_POLYGON_MODE_LINE;
    case D3DFILLMODE_SOLID:     return PIPE_POLYGON_MODE_FILL;
    default:
        assert(0);
        return PIPE_POLYGON_MODE_FILL;
    }
}

static inline unsigned
d3dblendop_to_pipe_blend(D3DBLENDOP op)
{
    switch (op) {
    case D3DBLENDOP_ADD:         return PIPE_BLEND_ADD;
    case D3DBLENDOP_SUBTRACT:    return PIPE_BLEND_SUBTRACT;
    case D3DBLENDOP_REVSUBTRACT: return PIPE_BLEND_REVERSE_SUBTRACT;
    case D3DBLENDOP_MIN:         return PIPE_BLEND_MIN;
    case D3DBLENDOP_MAX:         return PIPE_BLEND_MAX;
    default:
        assert(0);
        return PIPE_BLEND_ADD;
    }
}

static inline unsigned
d3dblend_alpha_to_pipe_blendfactor(D3DBLEND b)
{
    switch (b) {
    case D3DBLEND_ZERO:            return PIPE_BLENDFACTOR_ZERO;
    case D3DBLEND_ONE:             return PIPE_BLENDFACTOR_ONE;
    case D3DBLEND_SRCCOLOR:        return PIPE_BLENDFACTOR_SRC_COLOR;
    case D3DBLEND_INVSRCCOLOR:     return PIPE_BLENDFACTOR_INV_SRC_COLOR;
    case D3DBLEND_SRCALPHA:        return PIPE_BLENDFACTOR_SRC_ALPHA;
    case D3DBLEND_INVSRCALPHA:     return PIPE_BLENDFACTOR_INV_SRC_ALPHA;
    case D3DBLEND_DESTALPHA:       return PIPE_BLENDFACTOR_DST_ALPHA;
    case D3DBLEND_INVDESTALPHA:    return PIPE_BLENDFACTOR_INV_DST_ALPHA;
    case D3DBLEND_DESTCOLOR:       return PIPE_BLENDFACTOR_DST_COLOR;
    case D3DBLEND_INVDESTCOLOR:    return PIPE_BLENDFACTOR_INV_DST_COLOR;
    case D3DBLEND_SRCALPHASAT:     return PIPE_BLENDFACTOR_SRC_ALPHA_SATURATE;
    case D3DBLEND_BOTHSRCALPHA:    return PIPE_BLENDFACTOR_SRC_ALPHA;
    case D3DBLEND_BOTHINVSRCALPHA: return PIPE_BLENDFACTOR_INV_SRC_ALPHA;
    case D3DBLEND_BLENDFACTOR:     return PIPE_BLENDFACTOR_CONST_COLOR;
    case D3DBLEND_INVBLENDFACTOR:  return PIPE_BLENDFACTOR_INV_CONST_COLOR;
    case D3DBLEND_SRCCOLOR2:       return PIPE_BLENDFACTOR_SRC1_COLOR;
    case D3DBLEND_INVSRCCOLOR2:    return PIPE_BLENDFACTOR_INV_SRC1_COLOR;
    }
}

static inline unsigned
d3dblend_color_to_pipe_blendfactor(D3DBLEND b)
{
    switch (b) {
    case D3DBLEND_ZERO:            return PIPE_BLENDFACTOR_ZERO;
    case D3DBLEND_ONE:             return PIPE_BLENDFACTOR_ONE;
    case D3DBLEND_SRCCOLOR:        return PIPE_BLENDFACTOR_SRC_COLOR;
    case D3DBLEND_INVSRCCOLOR:     return PIPE_BLENDFACTOR_INV_SRC_COLOR;
    case D3DBLEND_SRCALPHA:        return PIPE_BLENDFACTOR_SRC_ALPHA;
    case D3DBLEND_INVSRCALPHA:     return PIPE_BLENDFACTOR_INV_SRC_ALPHA;
    case D3DBLEND_DESTALPHA:       return PIPE_BLENDFACTOR_DST_ALPHA;
    case D3DBLEND_INVDESTALPHA:    return PIPE_BLENDFACTOR_INV_DST_ALPHA;
    case D3DBLEND_DESTCOLOR:       return PIPE_BLENDFACTOR_DST_COLOR;
    case D3DBLEND_INVDESTCOLOR:    return PIPE_BLENDFACTOR_INV_DST_COLOR;
    case D3DBLEND_SRCALPHASAT:     return PIPE_BLENDFACTOR_SRC_ALPHA_SATURATE;
    case D3DBLEND_BOTHSRCALPHA:    return PIPE_BLENDFACTOR_SRC_ALPHA;
    case D3DBLEND_BOTHINVSRCALPHA: return PIPE_BLENDFACTOR_INV_SRC_ALPHA;
    case D3DBLEND_BLENDFACTOR:     return PIPE_BLENDFACTOR_CONST_ALPHA;
    case D3DBLEND_INVBLENDFACTOR:  return PIPE_BLENDFACTOR_INV_CONST_ALPHA;
    case D3DBLEND_SRCCOLOR2:       return PIPE_BLENDFACTOR_ONE; /* XXX */
    case D3DBLEND_INVSRCCOLOR2:    return PIPE_BLENDFACTOR_ZERO; /* XXX */
    }
}

static unsigned
d3dtaddress_to_pipe_tex_wrap(D3DTADDRESS addr)
{
    switch (addr) {
    case D3DTADDRESS_WRAP:
    case D3DTADDRESS_MIRROR:
    case D3DTADDRESS_CLAMP:
    case D3DTADDRESS_BORDER:
    case D3DTADDRESS_MIRRORONCE:
    default:
        assert(0);
        return PIPE_TEX_WRAP;
    }
}

static unsigned
d3dtexturefiltertype_to_pipe_tex_filter(D3DTEXTUREFILTERTYPE filter)
{
    switch (filter) {
    case D3DTEXF_POINT:       return PIPE_TEX_FILTER_NEAREST;
    case D3DTEXF_LINEAR:      return PIPE_TEX_FILTER_LINEAR;
    case D3DTEXF_ANISOTROPIC: return PIPE_TEX_FILTER_LINEAR;

    case D3DTEXF_NONE:
    case D3DTEXF_PYRAMIDALQUAD:
    case D3DTEXF_GAUSSIANQUAD:
    case D3DTEXF_CONVOLUTIONMONO:
    default:
        assert(0);
        return PIPE_TEX_FILTER_NEAREST;
    }
}

static unsigned
d3dtexturefiltertype_to_pipe_tex_mipfilter(D3DTEXTUREFILTERTYPE filter)
{
    switch (filter) {
    case D3DTEXF_NONE:        return PIPE_TEX_MIPFILTER_NONE;
    case D3DTEXF_POINT:       return PIPE_TEX_MIPFILTER_NEAREST;
    case D3DTEXF_LINEAR:      return PIPE_TEX_MIPFILTER_LINEAR;
    case D3DTEXF_ANISOTROPIC: return PIPE_TEX_MIPFILTER_LINEAR;

    case D3DTEXF_PYRAMIDALQUAD:
    case D3DTEXF_GAUSSIANQUAD:
    case D3DTEXF_CONVOLUTIONMONO:
    default:
        assert(0);
        return PIPE_TEX_MIPFILTER_NONE;
    }
}

#endif /* _NINE_PIPE_H_ */
