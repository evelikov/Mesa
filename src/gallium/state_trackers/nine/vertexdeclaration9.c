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

#include "vertexdeclaration9.h"
#include "device9.h"
#include "nine_helpers.h"

#include "pipe/p_format.h"

#define DBG_CHANNEL DBG_VERTEXDECLARATION

static INLINE enum pipe_format decltype_format(BYTE type)
{
    switch (type) {
    case D3DDECLTYPE_FLOAT1:    return PIPE_FORMAT_R32_FLOAT;
    case D3DDECLTYPE_FLOAT2:    return PIPE_FORMAT_R32G32_FLOAT;
    case D3DDECLTYPE_FLOAT3:    return PIPE_FORMAT_R32G32B32_FLOAT;
    case D3DDECLTYPE_FLOAT4:    return PIPE_FORMAT_R32G32B32A32_FLOAT;
    case D3DDECLTYPE_D3DCOLOR:  return PIPE_FORMAT_B8G8R8A8_UNORM;
    case D3DDECLTYPE_UBYTE4:    return PIPE_FORMAT_R8G8B8A8_USCALED;
    case D3DDECLTYPE_SHORT2:    return PIPE_FORMAT_R16G16_SSCALED;
    case D3DDECLTYPE_SHORT4:    return PIPE_FORMAT_R16G16B16A16_SSCALED;
    case D3DDECLTYPE_UBYTE4N:   return PIPE_FORMAT_R8G8B8A8_UNORM;
    case D3DDECLTYPE_SHORT2N:   return PIPE_FORMAT_R16G16_SNORM;
    case D3DDECLTYPE_SHORT4N:   return PIPE_FORMAT_R16G16B16A16_SNORM;
    case D3DDECLTYPE_USHORT2N:  return PIPE_FORMAT_R16G16_UNORM;
    case D3DDECLTYPE_USHORT4N:  return PIPE_FORMAT_R16G16B16A16_UNORM;
    case D3DDECLTYPE_UDEC3:     return PIPE_FORMAT_R10G10B10X2_USCALED;
    case D3DDECLTYPE_DEC3N:     return PIPE_FORMAT_R10G10B10X2_SNORM;
    case D3DDECLTYPE_FLOAT16_2: return PIPE_FORMAT_R16G16_FLOAT;
    case D3DDECLTYPE_FLOAT16_4: return PIPE_FORMAT_R16G16B16A16_FLOAT;
    default:
        assert(!"Implementation error !");
    }
    return PIPE_FORMAT_NONE;
}

static INLINE unsigned decltype_size(BYTE type)
{
    switch (type) {
    case D3DDECLTYPE_FLOAT1: return 1 * sizeof(float);
    case D3DDECLTYPE_FLOAT2: return 2 * sizeof(float);
    case D3DDECLTYPE_FLOAT3: return 3 * sizeof(float);
    case D3DDECLTYPE_FLOAT4: return 4 * sizeof(float);
    case D3DDECLTYPE_D3DCOLOR: return 1 * sizeof(DWORD);
    case D3DDECLTYPE_UBYTE4: return 4 * sizeof(BYTE);
    case D3DDECLTYPE_SHORT2: return 2 * sizeof(short);
    case D3DDECLTYPE_SHORT4: return 4 * sizeof(short);
    case D3DDECLTYPE_UBYTE4N: return 4 * sizeof(BYTE);
    case D3DDECLTYPE_SHORT2N: return 2 * sizeof(short);
    case D3DDECLTYPE_SHORT4N: return 4 * sizeof(short);
    case D3DDECLTYPE_USHORT2N: return 2 * sizeof(short);
    case D3DDECLTYPE_USHORT4N: return 4 * sizeof(short);
    case D3DDECLTYPE_UDEC3: return 4;
    case D3DDECLTYPE_DEC3N: return 4;
    case D3DDECLTYPE_FLOAT16_2: return 2 * 2;
    case D3DDECLTYPE_FLOAT16_4: return 4 * 2;
    default:
        assert(!"Implementation error !");
    }
    return 0;
}

#define NINE_DECLUSAGE_CASE0(n) case D3DDECLUSAGE_##n: return NINE_DECLUSAGE_##n
#define NINE_DECLUSAGE_CASEi(n) case D3DDECLUSAGE_##n: return NINE_DECLUSAGE_##n(usage_idx)
INLINE unsigned
nine_d3d9_to_nine_declusage(unsigned usage, unsigned usage_idx)
{
    switch (usage) {
    NINE_DECLUSAGE_CASE0(POSITION);
    NINE_DECLUSAGE_CASE0(BLENDWEIGHT);
    NINE_DECLUSAGE_CASE0(BLENDINDICES);
    NINE_DECLUSAGE_CASEi(NORMAL);
    NINE_DECLUSAGE_CASE0(PSIZE);
    NINE_DECLUSAGE_CASEi(TEXCOORD);
    NINE_DECLUSAGE_CASE0(TANGENT);
    NINE_DECLUSAGE_CASE0(BINORMAL);
    NINE_DECLUSAGE_CASE0(TESSFACTOR);
    NINE_DECLUSAGE_CASE0(POSITIONT);
    NINE_DECLUSAGE_CASEi(COLOR);
    NINE_DECLUSAGE_CASE0(DEPTH);
    NINE_DECLUSAGE_CASE0(SAMPLE);
    default:
        assert(!"Invalid DECLUSAGE.");
        return NINE_DECLUSAGE_NONE;
    }
}

HRESULT
NineVertexDeclaration9_ctor( struct NineVertexDeclaration9 *This,
                             struct NineUnknownParams *pParams,
                             struct NineDevice9 *pDevice,
                             const D3DVERTEXELEMENT9 *pElements )
{
    const D3DCAPS9 *caps;
    unsigned i;

    HRESULT hr = NineUnknown_ctor(&This->base, pParams);
    if (FAILED(hr)) { return hr; }

    for (This->nelems = 0;
         pElements[This->nelems].Type != D3DDECLTYPE_UNUSED &&
         pElements[This->nelems].Stream != 0xFF; /* wine */
         ++This->nelems);
    --This->nelems;

    This->device = pDevice;
    caps = NineDevice9_GetCaps(This->device);
    user_assert(This->nelems > 0, D3DERR_INVALIDCALL);
    user_assert(This->nelems <= caps->MaxStreams, D3DERR_INVALIDCALL);

    This->decls = CALLOC(This->nelems+1, sizeof(D3DVERTEXELEMENT9));
    This->elems = CALLOC(This->nelems, sizeof(struct pipe_vertex_element));
    if (!This->decls || !This->elems) { return E_OUTOFMEMORY; }
    memcpy(This->decls, pElements, sizeof(D3DVERTEXELEMENT9)*(This->nelems+1));

    memset(This->usage_map, 0xff, sizeof(This->usage_map));

    for (i = 0; i < This->nelems; ++i) {
        uint8_t usage = nine_d3d9_to_nine_declusage(This->decls[i].Usage,
                                                    This->decls[i].UsageIndex);
        This->usage_map[usage] = i;

        This->elems[i].src_offset = This->decls[i].Offset;
        This->elems[i].instance_divisor = 0;
        This->elems[i].vertex_buffer_index = This->decls[i].Stream;
        This->elems[i].src_format = decltype_format(This->decls[i].Type);
        /* XXX Remember Method (tesselation), Usage, UsageIndex */
    }

    return D3D_OK;
}

void
NineVertexDeclaration9_dtor( struct NineVertexDeclaration9 *This )
{
    if (This->decls)
        FREE(This->decls);
    if (This->elems)
        FREE(This->elems);

    NineUnknown_dtor(&This->base);
}

HRESULT WINAPI
NineVertexDeclaration9_GetDevice( struct NineVertexDeclaration9 *This,
                                  IDirect3DDevice9 **ppDevice )
{
    user_assert(ppDevice, E_POINTER);
    NineUnknown_AddRef(NineUnknown(This->device));
    *ppDevice = (IDirect3DDevice9 *)This->device;
    return D3D_OK;
}

HRESULT WINAPI
NineVertexDeclaration9_GetDeclaration( struct NineVertexDeclaration9 *This,
                                       D3DVERTEXELEMENT9 *pElement,
                                       UINT *pNumElements )
{
    if (!pElement) {
        user_assert(pNumElements, D3DERR_INVALIDCALL);
        *pNumElements = This->nelems+1;
        return D3D_OK;
    }
    if (pNumElements) { *pNumElements = This->nelems+1; }
    memcpy(pElement, This->decls, sizeof(D3DVERTEXELEMENT9)*(This->nelems+1));
    return D3D_OK;
}

IDirect3DVertexDeclaration9Vtbl NineVertexDeclaration9_vtable = {
    (void *)NineUnknown_QueryInterface,
    (void *)NineUnknown_AddRef,
    (void *)NineUnknown_Release,
    (void *)NineVertexDeclaration9_GetDevice,
    (void *)NineVertexDeclaration9_GetDeclaration
};

static const GUID *NineVertexDeclaration9_IIDs[] = {
    &IID_IDirect3DVertexDeclaration9,
    &IID_IUnknown,
    NULL
};

HRESULT
NineVertexDeclaration9_new( struct NineDevice9 *pDevice,
                            const D3DVERTEXELEMENT9 *pElements,
                            struct NineVertexDeclaration9 **ppOut )
{
    NINE_NEW(NineVertexDeclaration9, ppOut, /* args */ pDevice, pElements);
}

HRESULT
NineVertexDeclaration9_new_from_fvf( struct NineDevice9 *pDevice,
                                     DWORD FVF,
                                     struct NineVertexDeclaration9 **ppOut )
{
    D3DVERTEXELEMENT9 elems[16], decl_end = D3DDECL_END();
    unsigned texcount, i, betas, nelems = 0;
    BYTE beta_index = 0xFF;

    switch (FVF & D3DFVF_POSITION_MASK) {
        case D3DFVF_XYZ: /* simple XYZ */
        case D3DFVF_XYZB1:
        case D3DFVF_XYZB2:
        case D3DFVF_XYZB3:
        case D3DFVF_XYZB4:
        case D3DFVF_XYZB5: /* XYZ with beta values */
            elems[nelems].Type = D3DDECLTYPE_FLOAT3;
            elems[nelems].Usage = D3DDECLUSAGE_POSITION;
            elems[nelems].UsageIndex = 0;
            ++nelems;
            /* simple XYZ has no beta values. break. */
            if ((FVF & D3DFVF_POSITION_MASK) == D3DFVF_XYZ) { break; }

            betas = (((FVF & D3DFVF_XYZB5)-D3DFVF_XYZB1)>>1)+1;
            if (FVF & D3DFVF_LASTBETA_D3DCOLOR) {
                beta_index = D3DDECLTYPE_D3DCOLOR;
            } else if (FVF & D3DFVF_LASTBETA_UBYTE4) {
                beta_index = D3DDECLTYPE_UBYTE4;
            } else if ((FVF & D3DFVF_XYZB5) == D3DFVF_XYZB5) {
                beta_index = D3DDECLTYPE_FLOAT1;
            }
            if (beta_index != 0xFF) { --betas; }

            if (betas > 0) {
                switch (betas) {
                    case 1: elems[nelems].Type = D3DDECLTYPE_FLOAT1; break;
                    case 2: elems[nelems].Type = D3DDECLTYPE_FLOAT2; break;
                    case 3: elems[nelems].Type = D3DDECLTYPE_FLOAT3; break;
                    case 4: elems[nelems].Type = D3DDECLTYPE_FLOAT4; break;
                    default:
                        assert(!"Implementation error!");
                }
                elems[nelems].Usage = D3DDECLUSAGE_BLENDWEIGHT;
                elems[nelems].UsageIndex = 0;
                ++nelems;
            }

            if (beta_index != 0xFF) {
                elems[nelems].Type = beta_index;
                elems[nelems].Usage = D3DDECLUSAGE_BLENDINDICES;
                elems[nelems].UsageIndex = 0;
                ++nelems;
            }
            break;

        case D3DFVF_XYZW: /* simple XYZW */
        case D3DFVF_XYZRHW: /* pretransformed XYZW */
            elems[nelems].Type = D3DDECLTYPE_FLOAT4;
            elems[nelems].Usage =
                ((FVF & D3DFVF_POSITION_MASK) == D3DFVF_XYZW) ?
                D3DDECLUSAGE_POSITION : D3DDECLUSAGE_POSITIONT;
            elems[nelems].UsageIndex = 0;
            ++nelems;
            break;

        default:
            (void)user_error(!"Position doesn't match any known combination");
    }

    /* normals, psize and colors */
    if (FVF & D3DFVF_NORMAL) {
        elems[nelems].Type = D3DDECLTYPE_FLOAT3;
        elems[nelems].Usage = D3DDECLUSAGE_NORMAL;
        elems[nelems].UsageIndex = 0;
        ++nelems;
    }
    if (FVF & D3DFVF_PSIZE) {
        elems[nelems].Type = D3DDECLTYPE_FLOAT1;
        elems[nelems].Usage = D3DDECLUSAGE_PSIZE;
        elems[nelems].UsageIndex = 0;
        ++nelems;
    }
    if (FVF & D3DFVF_DIFFUSE) {
        elems[nelems].Type = D3DDECLTYPE_D3DCOLOR;
        elems[nelems].Usage = D3DDECLUSAGE_COLOR;
        elems[nelems].UsageIndex = 0;
        ++nelems;
    }
    if (FVF & D3DFVF_SPECULAR) {
        elems[nelems].Type = D3DDECLTYPE_D3DCOLOR;
        elems[nelems].Usage = D3DDECLUSAGE_COLOR;
        elems[nelems].UsageIndex = 1;
        ++nelems;
    }

    /* textures */
    texcount = (FVF >> D3DFVF_TEXCOUNT_SHIFT) & D3DFVF_TEXCOUNT_MASK;
    if (user_error(texcount <= 8)) { texcount = 8; }

    for (i = 0; i < texcount; ++i) {
        switch ((FVF >> (16+i*2)) & 0x3) {
            case D3DFVF_TEXTUREFORMAT1:
                elems[nelems].Type = D3DDECLTYPE_FLOAT1;
                break;

            case D3DFVF_TEXTUREFORMAT2:
                elems[nelems].Type = D3DDECLTYPE_FLOAT2;
                break;

            case D3DFVF_TEXTUREFORMAT3:
                elems[nelems].Type = D3DDECLTYPE_FLOAT3;
                break;

            case D3DFVF_TEXTUREFORMAT4:
                elems[nelems].Type = D3DDECLTYPE_FLOAT4;
                break;

            default:
                assert(!"Implementation error!");
        }
        elems[nelems].Usage = D3DDECLUSAGE_TEXCOORD;
        elems[nelems].UsageIndex = i;
        ++nelems;
    }

    /* fill out remaining data */
    for (i = 0; i < nelems; ++i) {
        elems[i].Stream = 0;
        elems[i].Offset = (i == 0) ? 0 : (elems[i-1].Offset +
                                          decltype_size(elems[i-1].Type));
        elems[i].Method = D3DDECLMETHOD_DEFAULT;
    }
    elems[nelems++] = decl_end;

    NINE_NEW(NineVertexDeclaration9, ppOut, /* args */ pDevice, elems);
}
