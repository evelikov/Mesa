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

static INLINE const char *
d3dformat_to_string( D3DFORMAT fmt )
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

static INLINE enum pipe_format
nine_pipe_format( D3DFORMAT fmt )
{
    switch (fmt) {
        case D3DFMT_UNKNOWN: return PIPE_FORMAT_NONE;
        /*case D3DFMT_R8G8B8: return PIPE_FORMAT_B8G8R8_UNORM;*/
        case D3DFMT_A8R8G8B8: return PIPE_FORMAT_B8G8R8A8_UNORM;
        case D3DFMT_X8R8G8B8: return PIPE_FORMAT_B8G8R8X8_UNORM;
        case D3DFMT_R5G6B5: return PIPE_FORMAT_B5G6R5_UNORM;
        case D3DFMT_X1R5G5B5: return PIPE_FORMAT_B5G5R5X1_UNORM;
        case D3DFMT_A1R5G5B5: return PIPE_FORMAT_B5G5R5A1_UNORM;
        case D3DFMT_A4R4G4B4: return PIPE_FORMAT_B4G4R4A4_UNORM;
        case D3DFMT_R3G3B2: return PIPE_FORMAT_B2G3R3_UNORM;
        case D3DFMT_A8: return PIPE_FORMAT_A8_UNORM;
        /*case D3DFMT_A8R3G3B2: return PIPE_FORMAT_B2G3R3A8_UNORM;*/
        case D3DFMT_X4R4G4B4: return PIPE_FORMAT_B4G4R4X4_UNORM;
        case D3DFMT_A2B10G10R10: return PIPE_FORMAT_R10G10B10A2_UNORM;
        case D3DFMT_A8B8G8R8: return PIPE_FORMAT_R8G8B8A8_UNORM;
        case D3DFMT_X8B8G8R8: return PIPE_FORMAT_R8G8B8X8_UNORM;
        case D3DFMT_G16R16: return PIPE_FORMAT_R16G16_UNORM;
        case D3DFMT_A2R10G10B10: return PIPE_FORMAT_B10G10R10A2_UNORM;
        case D3DFMT_A16B16G16R16: return PIPE_FORMAT_R16G16B16A16_UNORM;
        /*case D3DFMT_A8P8: return PIPE_FORMAT_?;*/
        /*case D3DFMT_P8: return PIPE_FORMAT_?;*/
        case D3DFMT_L8: return PIPE_FORMAT_L8_UNORM;
        case D3DFMT_A8L8: return PIPE_FORMAT_L8A8_UNORM;
        case D3DFMT_A4L4: return PIPE_FORMAT_L4A4_UNORM;
        /*case D3DFMT_V8U8: return PIPE_FORMAT_?;*/
        /*case D3DFMT_L6V5U5: return PIPE_FORMAT_?;*/
        /*case D3DFMT_X8L8V8U8: return PIPE_FORMAT_?;*/
        /*case D3DFMT_Q8W8V8U8: return PIPE_FORMAT_?;*/
        /*case D3DFMT_V16U16: return PIPE_FORMAT_?;*/
        /*case D3DFMT_A2W10V10U10: return PIPE_FORMAT_?;*/

        /* XXX check these 4 */
        case D3DFMT_UYVY: return PIPE_FORMAT_UYVY;
        case D3DFMT_R8G8_B8G8: return PIPE_FORMAT_R8G8_B8G8_UNORM;
        case D3DFMT_YUY2: return PIPE_FORMAT_YUYV;
        case D3DFMT_G8R8_G8B8: return PIPE_FORMAT_G8R8_G8B8_UNORM;

        case D3DFMT_DXT1: return PIPE_FORMAT_DXT1_RGB;
        case D3DFMT_DXT2: return PIPE_FORMAT_DXT1_RGBA;
        case D3DFMT_DXT3: return PIPE_FORMAT_DXT3_RGBA; /* XXX */
        case D3DFMT_DXT4: return PIPE_FORMAT_DXT3_RGBA;
        case D3DFMT_DXT5: return PIPE_FORMAT_DXT5_RGBA;

        case D3DFMT_D16_LOCKABLE: return PIPE_FORMAT_Z16_UNORM;
        case D3DFMT_D32: return PIPE_FORMAT_Z32_UNORM;
        /*case D3DFMT_D15S1: return PIPE_FORMAT_S1_UINT_Z15_UNORM;*/
        case D3DFMT_D24S8: return PIPE_FORMAT_S8_UINT_Z24_UNORM;
        case D3DFMT_D24X8: return PIPE_FORMAT_X8Z24_UNORM;
        /*case D3DFMT_D24X4S4: return PIPE_FORMAT_S4X4_UINT_Z24_UNORM;*/
        case D3DFMT_D16: return PIPE_FORMAT_Z16_UNORM;
        case D3DFMT_D32F_LOCKABLE: return PIPE_FORMAT_Z32_FLOAT;
        /*case D3DFMT_D24FS8: return PIPE_FORMAT_S8_UINT_Z24_FLOAT;*/
        case D3DFMT_D32_LOCKABLE: return PIPE_FORMAT_Z32_UNORM;
        case D3DFMT_S8_LOCKABLE: return PIPE_FORMAT_S8_UINT;

        case D3DFMT_L16: return PIPE_FORMAT_L16_UNORM;
        /*case D3DFMT_VERTEXDATA: return PIPE_FORMAT_?;*/
        /*case D3DFMT_INDEX16: return PIPE_FORMAT_?;*/
        /*case D3DFMT_INDEX32: return PIPE_FORMAT_?;*/
        /*case D3DFMT_Q16W16V16U16: return PIPE_FORMAT_?;*/
        /*case D3DFMT_MULTI2_ARGB8: return PIPE_FORMAT_?;*/
        case D3DFMT_R16F: return PIPE_FORMAT_R16_FLOAT;
        case D3DFMT_G16R16F: return PIPE_FORMAT_R16G16_FLOAT;
        case D3DFMT_A16B16G16R16F: return PIPE_FORMAT_R16G16B16A16_FLOAT;
        case D3DFMT_R32F: return PIPE_FORMAT_R32_FLOAT;
        case D3DFMT_G32R32F: return PIPE_FORMAT_R32G32_FLOAT;
        case D3DFMT_A32B32G32R32F: return PIPE_FORMAT_R32G32B32A32_FLOAT;
        /*case D3DFMT_CxV8U8: return PIPE_FORMAT_?;*/
        /*case D3DFMT_A1: return PIPE_FORMAT_?;*/
        /*case D3DFMT_A2B10G10R10_XR_BIAS: return PIPE_FORMAT_?;*/
        /*case D3DFMT_BINARYBUFFER: return PIPE_FORMAT_?;*/
        default:
            break;
    }
    return PIPE_FORMAT_NONE;
}

static INLINE unsigned
nine_fvf_stride( DWORD fvf )
{
    unsigned texcount, i, size = 0;

    switch (fvf & D3DFVF_POSITION_MASK) {
        case D3DFVF_XYZ: size += 3*4; break;
        case D3DFVF_XYZRHW: size += 4*4; break;
        case D3DFVF_XYZB1: size += 4*4; break;
        case D3DFVF_XYZB2: size += 5*4; break;
        case D3DFVF_XYZB3: size += 6*4; break;
        case D3DFVF_XYZB4: size += 7*4; break;
        case D3DFVF_XYZB5: size += 8*4; break;
        case D3DFVF_XYZW: size += 4*4; break;
        default:
            (void)user_error(!"Position doesn't match any known combination");
    }

    if (fvf & D3DFVF_NORMAL) { size += 3*4; }
    if (fvf & D3DFVF_PSIZE) { size += 1*4; }
    if (fvf & D3DFVF_DIFFUSE) { size += 1*4; }
    if (fvf & D3DFVF_SPECULAR) { size += 1*4; }

    texcount = (fvf >> D3DFVF_TEXCOUNT_SHIFT) & D3DFVF_TEXCOUNT_MASK;
    if (user_error(texcount <= 8)) { texcount = 8; }

    for (i = 0; i < texcount; ++i) {
        unsigned texformat = (fvf>>(16+i*2))&0x3;
        /* texformats are defined having been shifted around so 1=3,2=0,3=1,4=2
         * meaning we can just do this instead of the switch below */
        size += (((texformat+1)&0x3)+1)*4;

        /*switch (texformat) {
            case D3DFVF_TEXTUREFORMAT1: size += 1*4;
            case D3DFVF_TEXTUREFORMAT2: size += 2*4;
            case D3DFVF_TEXTUREFORMAT3: size += 3*4;
            case D3DFVF_TEXTUREFORMAT4: size += 4*4;
        }*/
    }

    return size;
}

static INLINE void
d3dcolor_to_rgba( D3DCOLOR color,
                  float *rgba )
{
    rgba[0] = (float)((color >> 16) & 0xFF) / 0xFF;
    rgba[1] = (float)((color >> 8) & 0xFF) / 0xFF;
    rgba[2] = (float)(color & 0xFF) / 0xFF;
    rgba[3] = (float)((color >> 24) & 0xFF) / 0xFF;
}

#endif /* _NINE_PIPE_H_ */
