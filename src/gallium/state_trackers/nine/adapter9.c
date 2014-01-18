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

#include "adapter9.h"
#include "device9.h"
#include "nine_helpers.h"
#include "nine_pipe.h"

#include "pipe/p_screen.h"

#define DBG_CHANNEL DBG_ADAPTER

HRESULT
NineAdapter9_ctor( struct NineAdapter9 *This,
                   struct NineUnknownParams *pParams,
                   struct pipe_screen *pScreenHAL,
                   struct pipe_screen *pScreenREF,
                   D3DADAPTER_IDENTIFIER9 *pIdentifier,
                   PPRESENT_TO_RESOURCE pPTR,
                   void *pClosure,
                   void (*pDestructor)(void *) )
{
    HRESULT hr = NineUnknown_ctor(&This->base, pParams);
    if (FAILED(hr)) { return hr; }

    This->hal = pScreenHAL;
    This->ref = pScreenREF;
    This->identifier = *pIdentifier;
    This->ptrfunc = pPTR;
    This->closure = pClosure;
    This->dtor = pDestructor;

    return D3D_OK;
}

void
NineAdapter9_dtor( struct NineAdapter9 *This )
{
    void *closure = This->closure;
    void (*dtor)(void *) = This->dtor;

    if (This->ref) {
        if (This->ref->destroy) { This->ref->destroy(This->ref); }
    } else if (This->hal) {
        /* this assumes that if a ref exists, then the HAL screen will be
         * lying under it, or not exist. In which case the HAL screen will be
         * deleted by destroying the ref screen or never exist. */
        if (This->hal->destroy) { This->hal->destroy(This->hal); }
    }
    NineUnknown_dtor(&This->base);

    /* special case, call backend-specific dtor AFTER destroying this object
     * completely. */
    if (dtor) { dtor(closure); }
}

static HRESULT
NineAdapter9_GetScreen( struct NineAdapter9 *This,
                        D3DDEVTYPE DevType,
                        struct pipe_screen **ppScreen )
{
    switch (DevType) {
        case D3DDEVTYPE_HAL:
            *ppScreen = This->hal;
            break;

        case D3DDEVTYPE_REF:
        case D3DDEVTYPE_NULLREF:
            *ppScreen = This->ref;
            break;

        case D3DDEVTYPE_SW:
            return D3DERR_NOTAVAILABLE;

        default:
            user_assert(!"Invalid device type", D3DERR_INVALIDCALL);
    }

    if (!*ppScreen) { return D3DERR_NOTAVAILABLE; }

    return D3D_OK;
}

HRESULT WINAPI
NineAdapter9_GetAdapterIdentifier( struct NineAdapter9 *This,
                                   DWORD Flags,
                                   D3DADAPTER_IDENTIFIER9 *pIdentifier )
{
    /* regarding flags, MSDN has this to say:
     *  Flags sets the WHQLLevel member of D3DADAPTER_IDENTIFIER9. Flags can be
     *  set to either 0 or D3DENUM_WHQL_LEVEL. If D3DENUM_WHQL_LEVEL is
     *  specified, this call can connect to the Internet to download new
     *  Microsoft Windows Hardware Quality Labs (WHQL) certificates.
     * so let's just ignore it. */
    *pIdentifier = This->identifier;
    return D3D_OK;
}

static INLINE boolean
backbuffer_format( D3DFORMAT dfmt,
                   D3DFORMAT bfmt,
                   boolean win )
{
    if (dfmt == D3DFMT_A2R10G10B10 && win) { return FALSE; }

    if ((dfmt == D3DFMT_A2R10G10B10 && bfmt == dfmt) ||
        (dfmt == D3DFMT_X8R8G8B8 && (bfmt == dfmt ||
                                     bfmt == D3DFMT_A8R8G8B8)) ||
        (dfmt == D3DFMT_X1R5G5B5 && (bfmt == dfmt ||
                                     bfmt == D3DFMT_A1R5G5B5)) ||
        (dfmt == D3DFMT_R5G6B5 && bfmt == dfmt)) {
        return TRUE;
    }

    return FALSE;
}

HRESULT WINAPI
NineAdapter9_CheckDeviceType( struct NineAdapter9 *This,
                              D3DDEVTYPE DevType,
                              D3DFORMAT AdapterFormat,
                              D3DFORMAT BackBufferFormat,
                              BOOL bWindowed )
{
    struct pipe_screen *screen;
    enum pipe_format dfmt, bfmt;
    HRESULT hr;

    user_assert(backbuffer_format(AdapterFormat, BackBufferFormat, bWindowed),
                D3DERR_NOTAVAILABLE);

    hr = NineAdapter9_GetScreen(This, DevType, &screen);
    if (FAILED(hr)) { return hr; }

    dfmt = nine_pipe_format(AdapterFormat);
    bfmt = nine_pipe_format(BackBufferFormat);
    if (dfmt == PIPE_FORMAT_NONE || bfmt == PIPE_FORMAT_NONE) {
        return D3DERR_NOTAVAILABLE;
    }

    if (!screen->is_format_supported(screen, dfmt, PIPE_TEXTURE_2D, 1,
                                     PIPE_BIND_DISPLAY_TARGET |
                                     PIPE_BIND_SHARED) ||
        !screen->is_format_supported(screen, bfmt, PIPE_TEXTURE_2D, 1,
                                     PIPE_BIND_DISPLAY_TARGET |
                                     PIPE_BIND_SHARED)) {
        return D3DERR_NOTAVAILABLE;
    }

    return D3D_OK;
}

static INLINE boolean
display_format( D3DFORMAT fmt,
                boolean win )
{
    /* http://msdn.microsoft.com/en-us/library/bb172558(v=VS.85).aspx#BackBuffer_or_Display_Formats */
    static const D3DFORMAT allowed[] = {
        D3DFMT_A2R10G10B10,
        D3DFMT_X8R8G8B8,
        D3DFMT_X1R5G5B5,
        D3DFMT_R5G6B5
    };
    unsigned i;

    if (fmt == D3DFMT_A2R10G10B10 && win) { return FALSE; }

    for (i = 0; i < sizeof(allowed)/sizeof(D3DFORMAT); i++) {
        if (fmt == allowed[i]) { return TRUE; }
    }
    return FALSE;
}

HRESULT WINAPI
NineAdapter9_CheckDeviceFormat( struct NineAdapter9 *This,
                                D3DDEVTYPE DeviceType,
                                D3DFORMAT AdapterFormat,
                                DWORD Usage,
                                D3DRESOURCETYPE RType,
                                D3DFORMAT CheckFormat )
{
    struct pipe_screen *screen;
    enum pipe_texture_target target;
    enum pipe_format fmt;
    unsigned bind = 0;
    unsigned usage = 0;
    HRESULT hr;
    boolean nomip = FALSE;

    user_assert(display_format(AdapterFormat, FALSE), D3DERR_INVALIDCALL);

    hr = NineAdapter9_GetScreen(This, DeviceType, &screen);
    if (FAILED(hr)) { return hr; }

    fmt = nine_pipe_format(AdapterFormat);
    if (fmt == PIPE_FORMAT_NONE) { return D3DERR_NOTAVAILABLE; }
    if (!screen->is_format_supported(screen, fmt, PIPE_TEXTURE_2D, 1,
                                     PIPE_BIND_DISPLAY_TARGET |
                                     PIPE_BIND_SHARED)) {
        return D3DERR_NOTAVAILABLE;
    }

    if (Usage & D3DUSAGE_DEPTHSTENCIL) { bind |= PIPE_BIND_DEPTH_STENCIL; }
    if (Usage & D3DUSAGE_DYNAMIC) { usage |= PIPE_USAGE_DYNAMIC; }
    if (Usage & D3DUSAGE_RENDERTARGET) { usage |= PIPE_BIND_RENDER_TARGET; }

    switch (RType) {
        case D3DRTYPE_SURFACE:
            nomip = TRUE;
            target = PIPE_TEXTURE_2D;
            break;

        case D3DRTYPE_VOLUME:
            nomip = TRUE;
            target = PIPE_TEXTURE_3D;
            break;

        case D3DRTYPE_TEXTURE:
            target = PIPE_TEXTURE_2D;
            break;

        case D3DRTYPE_VOLUMETEXTURE:
            target = PIPE_TEXTURE_3D;
            break;

        case D3DRTYPE_CUBETEXTURE:
            target = PIPE_TEXTURE_CUBE;
            break;

        case D3DRTYPE_VERTEXBUFFER:
            target = PIPE_BUFFER;
            bind |= PIPE_BIND_VERTEX_BUFFER;
            break;

        case D3DRTYPE_INDEXBUFFER:
            target = PIPE_BUFFER;
            bind |= PIPE_BIND_INDEX_BUFFER;
            break;

        default:
            return D3DERR_INVALIDCALL;
    }

    /* XXX displacement map */
    if (Usage & D3DUSAGE_DMAP) {
        return D3DERR_NOTAVAILABLE;
    }

    fmt = nine_pipe_format(CheckFormat);
    if (!screen->is_format_supported(screen, fmt, target, 1, bind)) {
        return D3DERR_NOTAVAILABLE;
    }

    /*if (Usage & D3DUSAGE_NONSECURE) { don't know the implication of this }*/
    /*if (Usage & D3DUSAGE_SOFTWAREPROCESSING) { we can always support this }*/

    if ((Usage & D3DUSAGE_AUTOGENMIPMAP) && nomip) {
        return D3DOK_NOAUTOGEN;
    }

    return D3D_OK;
}

HRESULT WINAPI
NineAdapter9_CheckDeviceMultiSampleType( struct NineAdapter9 *This,
                                         D3DDEVTYPE DeviceType,
                                         D3DFORMAT SurfaceFormat,
                                         BOOL Windowed,
                                         D3DMULTISAMPLE_TYPE MultiSampleType,
                                         DWORD *pQualityLevels )
{
    struct pipe_screen *screen;
    enum pipe_format fmt;
    HRESULT hr;

    hr = NineAdapter9_GetScreen(This, DeviceType, &screen);
    if (FAILED(hr)) { return hr; }

    fmt = nine_pipe_format(SurfaceFormat);
    if (fmt == PIPE_FORMAT_NONE) { return D3DERR_NOTAVAILABLE; }
    if (!screen->is_format_supported(screen, fmt, PIPE_TEXTURE_2D, 1, 0)) {
        return D3DERR_NOTAVAILABLE;
    }

    /* XXX check MSAA levels */
    if (pQualityLevels) { *pQualityLevels = 0; }

    return D3D_OK;
}

static INLINE boolean
depth_stencil_format( D3DFORMAT fmt )
{
    static D3DFORMAT allowed[] = {
        D3DFMT_D16_LOCKABLE,
        D3DFMT_D32,
        D3DFMT_D15S1,
        D3DFMT_D24S8,
        D3DFMT_D24X8,
        D3DFMT_D24X4S4,
        D3DFMT_D16,
        D3DFMT_D32F_LOCKABLE,
        D3DFMT_D24FS8,
        D3DFMT_D32_LOCKABLE
    };
    unsigned i;

    for (i = 0; i < sizeof(allowed)/sizeof(D3DFORMAT); i++) {
        if (fmt == allowed[i]) { return TRUE; }
    }
    return FALSE;
}

HRESULT WINAPI
NineAdapter9_CheckDepthStencilMatch( struct NineAdapter9 *This,
                                     D3DDEVTYPE DeviceType,
                                     D3DFORMAT AdapterFormat,
                                     D3DFORMAT RenderTargetFormat,
                                     D3DFORMAT DepthStencilFormat )
{
    struct pipe_screen *screen;
    enum pipe_format dfmt, bfmt, zsfmt;
    HRESULT hr;

    user_assert(display_format(AdapterFormat, FALSE), D3DERR_NOTAVAILABLE);
    user_assert(depth_stencil_format(DepthStencilFormat), D3DERR_NOTAVAILABLE);

    hr = NineAdapter9_GetScreen(This, DeviceType, &screen);
    if (FAILED(hr)) { return hr; }

    dfmt = nine_pipe_format(AdapterFormat);
    bfmt = nine_pipe_format(RenderTargetFormat);
    zsfmt = nine_pipe_format(DepthStencilFormat);
    if (dfmt == PIPE_FORMAT_NONE ||
        bfmt == PIPE_FORMAT_NONE ||
        zsfmt == PIPE_FORMAT_NONE) {
        return D3DERR_NOTAVAILABLE;
    }

    if (!screen->is_format_supported(screen, dfmt, PIPE_TEXTURE_2D, 1,
                                     PIPE_BIND_DISPLAY_TARGET |
                                     PIPE_BIND_SHARED) ||
        !screen->is_format_supported(screen, bfmt, PIPE_TEXTURE_2D, 1,
                                     PIPE_BIND_RENDER_TARGET) ||
        !screen->is_format_supported(screen, zsfmt, PIPE_TEXTURE_2D, 1,
                                     PIPE_BIND_DEPTH_STENCIL)) {
        return D3DERR_NOTAVAILABLE;
    }

    return D3D_OK;
}

HRESULT WINAPI
NineAdapter9_CheckDeviceFormatConversion( struct NineAdapter9 *This,
                                          D3DDEVTYPE DeviceType,
                                          D3DFORMAT SourceFormat,
                                          D3DFORMAT TargetFormat )
{
    /* MSDN says this tests whether a certain backbuffer format can be used in
     * conjunction with a certain front buffer format. It's a little confusing
     * but some one wiser might be able to figure this one out. XXX */
    struct pipe_screen *screen;
    enum pipe_format dfmt, bfmt;
    HRESULT hr;

    user_assert(backbuffer_format(TargetFormat, SourceFormat, FALSE),
                D3DERR_NOTAVAILABLE);

    hr = NineAdapter9_GetScreen(This, DeviceType, &screen);
    if (FAILED(hr)) { return hr; }

    dfmt = nine_pipe_format(TargetFormat);
    bfmt = nine_pipe_format(SourceFormat);
    if (dfmt == PIPE_FORMAT_NONE || bfmt == PIPE_FORMAT_NONE) {
        return D3DERR_NOTAVAILABLE;
    }
    if (!screen->is_format_supported(screen, dfmt, PIPE_TEXTURE_2D, 1,
                                     PIPE_BIND_DISPLAY_TARGET |
                                     PIPE_BIND_SHARED) ||
        !screen->is_format_supported(screen, bfmt, PIPE_TEXTURE_2D, 1,
                                     PIPE_BIND_DISPLAY_TARGET |
                                     PIPE_BIND_SHARED)) {
        return D3DERR_NOTAVAILABLE;
    }

    return D3D_OK;
}

HRESULT WINAPI
NineAdapter9_GetDeviceCaps( struct NineAdapter9 *This,
                            D3DDEVTYPE DeviceType,
                            D3DCAPS9 *pCaps )
{
    struct pipe_screen *screen;
    boolean sm3, vs;
    HRESULT hr;

    user_assert(pCaps, D3DERR_INVALIDCALL);

    hr = NineAdapter9_GetScreen(This, DeviceType, &screen);
    if (FAILED(hr)) { return hr; }

#define D3DPIPECAP(pcap, d3dcap) \
    (screen->get_param(screen, PIPE_CAP_##pcap) ? (d3dcap) : 0)

#define D3DNPIPECAP(pcap, d3dcap) \
    (screen->get_param(screen, PIPE_CAP_##pcap) ? 0 : (d3dcap))

    sm3 = screen->get_param(screen, PIPE_CAP_SM3);
    vs = !!(screen->get_shader_param(screen, PIPE_SHADER_VERTEX,
                                     PIPE_SHADER_CAP_MAX_INSTRUCTIONS));

    pCaps->AdapterOrdinal = 0;
    pCaps->Caps = 0;
    pCaps->Caps2 = D3DCAPS2_CANMANAGERESOURCE |
                   D3DCAPS2_DYNAMICTEXTURES |
                   D3DCAPS2_CANAUTOGENMIPMAP;
    pCaps->Caps3 = D3DCAPS3_COPY_TO_VIDMEM | D3DCAPS3_COPY_TO_SYSTEMMEM;
    pCaps->PresentationIntervals = D3DPRESENT_INTERVAL_DEFAULT;
    pCaps->CursorCaps = D3DCURSORCAPS_COLOR | D3DCURSORCAPS_LOWRES;
    pCaps->DevCaps = D3DDEVCAPS_CANBLTSYSTONONLOCAL |
                     D3DDEVCAPS_CANRENDERAFTERFLIP |
                     D3DDEVCAPS_DRAWPRIMITIVES2 |
                     D3DDEVCAPS_DRAWPRIMITIVES2EX |
                     D3DDEVCAPS_DRAWPRIMTLVERTEX |
                     /*D3DDEVCAPS_EXECUTESYSTEMMEMORY |*/
                     /*D3DDEVCAPS_EXECUTEVIDEOMEMORY |*/
                     D3DDEVCAPS_HWRASTERIZATION |
                     D3DDEVCAPS_HWTRANSFORMANDLIGHT |
                     /*D3DDEVCAPS_NPATCHES |*/
                     D3DDEVCAPS_PUREDEVICE |
                     /*D3DDEVCAPS_QUINTICRTPATCHES |*/
                     /*D3DDEVCAPS_RTPATCHES |*/
                     /*D3DDEVCAPS_RTPATCHHANDLEZERO |*/
                     /*D3DDEVCAPS_SEPARATETEXTUREMEMORIES |*/
                     /*D3DDEVCAPS_TEXTURENONLOCALVIDMEM |*/
                     D3DDEVCAPS_TEXTURESYSTEMMEMORY |
                     D3DDEVCAPS_TEXTUREVIDEOMEMORY |
                     D3DDEVCAPS_TLVERTEXSYSTEMMEMORY |
                     D3DDEVCAPS_TLVERTEXVIDEOMEMORY;

    pCaps->PrimitiveMiscCaps = D3DPMISCCAPS_MASKZ |
                               D3DPMISCCAPS_CULLCW |
                               D3DPMISCCAPS_CULLCCW |
                               D3DPMISCCAPS_COLORWRITEENABLE |
                               D3DPMISCCAPS_CLIPPLANESCALEDPOINTS |
                               D3DPMISCCAPS_CLIPTLVERTS |
                               /*D3DPMISCCAPS_TSSARGTEMP |*/
                               D3DPMISCCAPS_BLENDOP |
                               /*D3DPMISCCAPS_INDEPENDENTWRITEMASKS |*/
                               /*D3DPMISCCAPS_PERSTAGECONSTANT |*/
                               /*D3DPMISCCAPS_POSTBLENDSRGBCONVERT |*/
                               D3DPMISCCAPS_FOGANDSPECULARALPHA |
                               /*D3DPMISCCAPS_SEPARATEALPHABLEND |*/
                               /*D3DPMISCCAPS_MRTINDEPENDENTBITDEPTHS |*/
                               /*D3DPMISCCAPS_MRTPOSTPIXELSHADERBLENDING |*/
                               /*D3DPMISCCAPS_FOGVERTEXCLAMPED*/0;
    pCaps->RasterCaps =
        D3DPIPECAP(ANISOTROPIC_FILTER, D3DPRASTERCAPS_ANISOTROPY) |
        /*D3DPRASTERCAPS_COLORPERSPECTIVE |*/
        /*D3DPRASTERCAPS_DITHER |*/
        /*D3DPRASTERCAPS_DEPTHBIAS |*/
        /*D3DPRASTERCAPS_FOGRANGE |*/
        /*D3DPRASTERCAPS_FOGTABLE |*/
        /*D3DPRASTERCAPS_FOGVERTEX |*/
        /*D3DPRASTERCAPS_MIPMAPLODBIAS |*/
        /*D3DPRASTERCAPS_MULTISAMPLE_TOGGLE |*/
        /*D3DPRASTERCAPS_SCISSORTEST |*/
        /*D3DPRASTERCAPS_SLOPESCALEDEPTHBIAS |*/
        /*D3DPRASTERCAPS_WBUFFER |*/
        /*D3DPRASTERCAPS_WFOG |*/
        /*D3DPRASTERCAPS_ZBUFFERLESSHSR |*/
        /*D3DPRASTERCAPS_ZFOG |*/
        D3DPRASTERCAPS_ZTEST;

    pCaps->ZCmpCaps = D3DPCMPCAPS_NEVER |
                      D3DPCMPCAPS_LESS |
                      D3DPCMPCAPS_EQUAL |
                      D3DPCMPCAPS_LESSEQUAL |
                      D3DPCMPCAPS_GREATER |
                      D3DPCMPCAPS_NOTEQUAL |
                      D3DPCMPCAPS_GREATEREQUAL |
                      D3DPCMPCAPS_ALWAYS;

    pCaps->SrcBlendCaps = D3DPBLENDCAPS_ZERO |
                          D3DPBLENDCAPS_ONE |
                          D3DPBLENDCAPS_SRCCOLOR |
                          D3DPBLENDCAPS_INVSRCCOLOR |
                          D3DPBLENDCAPS_SRCALPHA |
                          D3DPBLENDCAPS_INVSRCALPHA |
                          D3DPBLENDCAPS_DESTALPHA |
                          D3DPBLENDCAPS_INVDESTALPHA |
                          D3DPBLENDCAPS_DESTCOLOR |
                          D3DPBLENDCAPS_INVDESTCOLOR |
                          D3DPBLENDCAPS_SRCALPHASAT |
                          D3DPBLENDCAPS_BOTHSRCALPHA |
                          D3DPBLENDCAPS_BOTHINVSRCALPHA |
                          D3DPBLENDCAPS_BLENDFACTOR;

    pCaps->DestBlendCaps = D3DPBLENDCAPS_ZERO |
                           D3DPBLENDCAPS_ONE |
                           D3DPBLENDCAPS_SRCCOLOR |
                           D3DPBLENDCAPS_INVSRCCOLOR |
                           D3DPBLENDCAPS_SRCALPHA |
                           D3DPBLENDCAPS_INVSRCALPHA |
                           D3DPBLENDCAPS_DESTALPHA |
                           D3DPBLENDCAPS_INVDESTALPHA |
                           D3DPBLENDCAPS_DESTCOLOR |
                           D3DPBLENDCAPS_INVDESTCOLOR |
                           D3DPBLENDCAPS_SRCALPHASAT |
                           D3DPBLENDCAPS_BOTHSRCALPHA |
                           D3DPBLENDCAPS_BOTHINVSRCALPHA |
                           D3DPBLENDCAPS_BLENDFACTOR;

    pCaps->AlphaCmpCaps = D3DPCMPCAPS_ALWAYS;

    pCaps->ShadeCaps = D3DPSHADECAPS_COLORGOURAUDRGB |
                       D3DPSHADECAPS_SPECULARGOURAUDRGB |
                       D3DPSHADECAPS_ALPHAGOURAUDBLEND |
                       D3DPSHADECAPS_FOGGOURAUD;

    pCaps->TextureCaps =
        D3DPTEXTURECAPS_PERSPECTIVE |
        D3DNPIPECAP(NPOT_TEXTURES, D3DPTEXTURECAPS_POW2) |
        D3DPTEXTURECAPS_ALPHA |
        /*D3DPTEXTURECAPS_TEXREPEATNOTSCALEDBYSIZE |*/
        D3DPTEXTURECAPS_ALPHAPALETTE |
        D3DNPIPECAP(NPOT_TEXTURES, D3DPTEXTURECAPS_NONPOW2CONDITIONAL) |
        D3DPTEXTURECAPS_PROJECTED |
        D3DPTEXTURECAPS_CUBEMAP |
        D3DPTEXTURECAPS_VOLUMEMAP |
        D3DPIPECAP(MAX_TEXTURE_2D_LEVELS, D3DPTEXTURECAPS_MIPMAP) |
        D3DPIPECAP(MAX_TEXTURE_3D_LEVELS, D3DPTEXTURECAPS_MIPVOLUMEMAP) |
        D3DPIPECAP(MAX_TEXTURE_CUBE_LEVELS, D3DPTEXTURECAPS_MIPCUBEMAP) |
        D3DNPIPECAP(NPOT_TEXTURES, D3DPTEXTURECAPS_CUBEMAP_POW2) |
        D3DNPIPECAP(NPOT_TEXTURES, D3DPTEXTURECAPS_VOLUMEMAP_POW2);

    pCaps->TextureFilterCaps =
        D3DPTFILTERCAPS_MINFPOINT |
        D3DPTFILTERCAPS_MINFLINEAR |
        D3DPIPECAP(ANISOTROPIC_FILTER, D3DPTFILTERCAPS_MINFANISOTROPIC) |
        /*D3DPTFILTERCAPS_MINFPYRAMIDALQUAD |*/
        /*D3DPTFILTERCAPS_MINFGAUSSIANQUAD |*/
        D3DPTFILTERCAPS_MIPFPOINT |
        D3DPTFILTERCAPS_MIPFLINEAR |
        D3DPTFILTERCAPS_MAGFPOINT |
        D3DPTFILTERCAPS_MAGFLINEAR |
        D3DPIPECAP(ANISOTROPIC_FILTER, D3DPTFILTERCAPS_MAGFANISOTROPIC) |
        /*D3DPTFILTERCAPS_MAGFPYRAMIDALQUAD |*/
        /*D3DPTFILTERCAPS_MAGFGAUSSIANQUAD*/0;

    pCaps->CubeTextureFilterCaps = pCaps->TextureFilterCaps;
    pCaps->VolumeTextureFilterCaps = pCaps->TextureFilterCaps;

    pCaps->TextureAddressCaps =
        /*D3DPTADDRESSCAPS_WRAP |*/
        D3DPIPECAP(TEXTURE_MIRROR_CLAMP, D3DPTADDRESSCAPS_MIRROR) |
        D3DPIPECAP(TEXTURE_MIRROR_CLAMP, D3DPTADDRESSCAPS_CLAMP) |
        /*D3DPTADDRESSCAPS_BORDER |*/
        /*D3DPTADDRESSCAPS_INDEPENDENTUV |*/
        D3DNPIPECAP(TEXTURE_MIRROR_CLAMP, D3DPTADDRESSCAPS_MIRRORONCE);

    pCaps->VolumeTextureAddressCaps = pCaps->TextureAddressCaps;

    pCaps->LineCaps =
        D3DLINECAPS_TEXTURE |
        D3DLINECAPS_ZTEST |
        D3DLINECAPS_BLEND |
        /*D3DLINECAPS_ALPHACMP |*/
        D3DLINECAPS_FOG;
    if (screen->get_paramf(screen, PIPE_CAPF_MAX_LINE_WIDTH_AA) > 0.0) {
        pCaps->LineCaps |= D3DLINECAPS_ANTIALIAS;
    }

    pCaps->MaxTextureWidth =
        1<<(screen->get_param(screen, PIPE_CAP_MAX_TEXTURE_2D_LEVELS)-1);
    pCaps->MaxTextureHeight = pCaps->MaxTextureWidth;
    pCaps->MaxVolumeExtent =
        1<<(screen->get_param(screen, PIPE_CAP_MAX_TEXTURE_3D_LEVELS)-1);
    /* XXX: Yeah... figure these out. */
    pCaps->MaxTextureRepeat = 0;
    pCaps->MaxTextureAspectRatio = 1;

    pCaps->MaxAnisotropy =
        (DWORD)screen->get_paramf(screen, PIPE_CAPF_MAX_TEXTURE_ANISOTROPY);

    /* XXX: More to figure out. */
    pCaps->MaxVertexW = 0.0f;
    pCaps->GuardBandLeft = screen->get_paramf(screen,
                                              PIPE_CAPF_GUARD_BAND_LEFT);
    pCaps->GuardBandTop = screen->get_paramf(screen,
                                             PIPE_CAPF_GUARD_BAND_TOP);
    pCaps->GuardBandRight = screen->get_paramf(screen,
                                               PIPE_CAPF_GUARD_BAND_RIGHT);
    pCaps->GuardBandBottom = screen->get_paramf(screen,
                                                PIPE_CAPF_GUARD_BAND_BOTTOM);
    pCaps->ExtentsAdjust = 0.0f;

    pCaps->StencilCaps =
        D3DSTENCILCAPS_KEEP |
        D3DSTENCILCAPS_ZERO |
        D3DSTENCILCAPS_REPLACE |
        D3DSTENCILCAPS_INCRSAT |
        D3DSTENCILCAPS_DECRSAT |
        D3DSTENCILCAPS_INVERT |
        D3DSTENCILCAPS_INCR |
        D3DSTENCILCAPS_DECR |
        D3DPIPECAP(TWO_SIDED_STENCIL, D3DSTENCILCAPS_TWOSIDED);

    pCaps->FVFCaps =
        (D3DFVFCAPS_TEXCOORDCOUNTMASK & 1) |
        /*D3DFVFCAPS_DONOTSTRIPELEMENTS |*/
        D3DFVFCAPS_PSIZE;

    /* XXX: Some of these are probably not in SM2.0 so cap them when I figure
     * them out. For now leave them all enabled. */
    pCaps->TextureOpCaps = D3DTEXOPCAPS_DISABLE |
                           D3DTEXOPCAPS_SELECTARG1 |
                           D3DTEXOPCAPS_SELECTARG2 |
                           D3DTEXOPCAPS_MODULATE |
                           D3DTEXOPCAPS_MODULATE2X |
                           D3DTEXOPCAPS_MODULATE4X |
                           D3DTEXOPCAPS_ADD |
                           D3DTEXOPCAPS_ADDSIGNED |
                           D3DTEXOPCAPS_ADDSIGNED2X |
                           D3DTEXOPCAPS_SUBTRACT |
                           D3DTEXOPCAPS_ADDSMOOTH |
                           D3DTEXOPCAPS_BLENDDIFFUSEALPHA |
                           D3DTEXOPCAPS_BLENDTEXTUREALPHA |
                           D3DTEXOPCAPS_BLENDFACTORALPHA |
                           D3DTEXOPCAPS_BLENDTEXTUREALPHAPM |
                           D3DTEXOPCAPS_BLENDCURRENTALPHA |
                           D3DTEXOPCAPS_PREMODULATE |
                           D3DTEXOPCAPS_MODULATEALPHA_ADDCOLOR |
                           D3DTEXOPCAPS_MODULATECOLOR_ADDALPHA |
                           D3DTEXOPCAPS_MODULATEINVALPHA_ADDCOLOR |
                           D3DTEXOPCAPS_MODULATEINVCOLOR_ADDALPHA |
                           D3DTEXOPCAPS_BUMPENVMAP |
                           D3DTEXOPCAPS_BUMPENVMAPLUMINANCE |
                           D3DTEXOPCAPS_DOTPRODUCT3 |
                           D3DTEXOPCAPS_MULTIPLYADD |
                           D3DTEXOPCAPS_LERP;

    /* XXX: Is this right? */
    pCaps->MaxTextureBlendStages =
        (DWORD)screen->get_param(screen, PIPE_CAP_BLEND_EQUATION_SEPARATE);
    pCaps->MaxSimultaneousTextures = pCaps->MaxTextureBlendStages;

    pCaps->VertexProcessingCaps = D3DVTXPCAPS_TEXGEN |
                                  D3DVTXPCAPS_MATERIALSOURCE7 |
                                  D3DVTXPCAPS_DIRECTIONALLIGHTS |
                                  D3DVTXPCAPS_POSITIONALLIGHTS |
                                  /*D3DVTXPCAPS_LOCALVIEWER |*/
                                  /*D3DVTXPCAPS_TWEENING |*/
                                  /*D3DVTXPCAPS_TEXGEN_SPHEREMAP |*/
                                  /*D3DVTXPCAPS_NO_TEXGEN_NONLOCALVIEWER*/0;

    /* No idea and no PIPE_CAP. */
    pCaps->MaxActiveLights = 100;
    pCaps->MaxUserClipPlanes = 100;
    pCaps->MaxVertexBlendMatrices = 4;
    pCaps->MaxVertexBlendMatrixIndex = 4;

    pCaps->MaxPointSize = screen->get_paramf(screen, PIPE_CAPF_MAX_POINT_WIDTH);

    pCaps->MaxPrimitiveCount = 0xFFFFFFFF;
    pCaps->MaxVertexIndex = 0xFFFFFFFF;
    pCaps->MaxStreams = 16;

    /* XXX: No valid clue. */
    pCaps->MaxStreamStride = 0xFFFF;

    pCaps->VertexShaderVersion = sm3 ? 0xFFFE0300 : 0xFFFE0200;
    if (vs) {
        /* VS 2 as well as 3.0 supports a minimum of 256 consts, no matter how
         * much our architecture moans about it. The problem is that D3D9
         * expects access to 16 int consts (i#), containing 3 components and
         * 16 booleans (b#), containing only 1 component. This should be packed
         * into 20 float vectors (16 for i# and 16/4 for b#), since gallium has
         * removed support for the loop counter/boolean files. */
        pCaps->MaxVertexShaderConst =
            screen->get_shader_param(screen, PIPE_SHADER_VERTEX,
                                     PIPE_SHADER_CAP_MAX_CONSTS)-20;

        /* Fake the minimum cap for Windows */
        if (QUIRK(FAKE_CAPS)) {
            pCaps->MaxVertexShaderConst = 256;
        }
    } else {
        pCaps->MaxVertexShaderConst = 0;
    }

    pCaps->PixelShaderVersion = sm3 ? 0xFFFF0300 : 0xFFFF0200;
    pCaps->PixelShader1xMaxValue = 256.0f; /* I haven't a clue. */

    pCaps->DevCaps2 = /*D3DDEVCAPS2_STREAMOFFSET |*/
                      /*D3DDEVCAPS2_DMAPNPATCH |*/
                      /*D3DDEVCAPS2_ADAPTIVETESSRTPATCH |*/
                      /*D3DDEVCAPS2_ADAPTIVETESSNPATCH |*/
                      /*D3DDEVCAPS2_CAN_STRETCHRECT_FROM_TEXTURES |*/
                      /*D3DDEVCAPS2_PRESAMPLEDDMAPNPATCH |*/
                      /*D3DDEVCAPS2_VERTEXELEMENTSCANSHARESTREAMOFFSET*/0;

    /* Undocumented? */
    pCaps->MaxNpatchTessellationLevel = 0.0f;
    pCaps->Reserved5 = 0;

    pCaps->DeclTypes = D3DDTCAPS_UBYTE4 |
                       D3DDTCAPS_UBYTE4N |
                       D3DDTCAPS_SHORT2N |
                       D3DDTCAPS_SHORT4N |
                       D3DDTCAPS_USHORT2N |
                       D3DDTCAPS_USHORT4N |
                       D3DDTCAPS_UDEC3 |
                       D3DDTCAPS_DEC3N |
                       D3DDTCAPS_FLOAT16_2 |
                       D3DDTCAPS_FLOAT16_4;

    pCaps->NumSimultaneousRTs =
        screen->get_param(screen, PIPE_CAP_MAX_RENDER_TARGETS);

    pCaps->StretchRectFilterCaps = D3DPTFILTERCAPS_MINFPOINT |
                                   D3DPTFILTERCAPS_MINFLINEAR |
                                   D3DPTFILTERCAPS_MAGFPOINT |
                                   D3DPTFILTERCAPS_MAGFLINEAR;

    pCaps->VS20Caps.Caps = 1;
    pCaps->VS20Caps.DynamicFlowControlDepth = /* XXX is this dynamic? */
        screen->get_shader_param(screen, PIPE_SHADER_VERTEX,
                                 PIPE_SHADER_CAP_MAX_CONTROL_FLOW_DEPTH);
    pCaps->VS20Caps.NumTemps =
        screen->get_shader_param(screen, PIPE_SHADER_VERTEX,
                                 PIPE_SHADER_CAP_MAX_TEMPS);
    pCaps->VS20Caps.StaticFlowControlDepth = /* XXX is this static? */
        screen->get_shader_param(screen, PIPE_SHADER_VERTEX,
                                 PIPE_SHADER_CAP_MAX_CONTROL_FLOW_DEPTH);

    pCaps->PS20Caps.Caps = 1;
    pCaps->PS20Caps.DynamicFlowControlDepth = /* XXX is this dynamic? */
        screen->get_shader_param(screen, PIPE_SHADER_FRAGMENT,
                                 PIPE_SHADER_CAP_MAX_CONTROL_FLOW_DEPTH);;
    pCaps->PS20Caps.NumTemps =
        screen->get_shader_param(screen, PIPE_SHADER_FRAGMENT,
                                 PIPE_SHADER_CAP_MAX_TEMPS);
    pCaps->PS20Caps.StaticFlowControlDepth =  /* XXX is this static? */
        screen->get_shader_param(screen, PIPE_SHADER_FRAGMENT,
                                 PIPE_SHADER_CAP_MAX_CONTROL_FLOW_DEPTH);
    pCaps->PS20Caps.NumInstructionSlots =
        screen->get_shader_param(screen, PIPE_SHADER_FRAGMENT,
                                 PIPE_SHADER_CAP_MAX_INSTRUCTIONS);

    pCaps->VertexTextureFilterCaps = pCaps->TextureFilterCaps;

    pCaps->MaxVShaderInstructionsExecuted = 0xFFFF;
    pCaps->MaxPShaderInstructionsExecuted = 0xFFFF;

    pCaps->MaxVertexShader30InstructionSlots =
        sm3 ? screen->get_shader_param(screen, PIPE_SHADER_VERTEX,
                                       PIPE_SHADER_CAP_MAX_INSTRUCTIONS) : 0;
    pCaps->MaxPixelShader30InstructionSlots =
        sm3 ? screen->get_shader_param(screen, PIPE_SHADER_FRAGMENT,
                                       PIPE_SHADER_CAP_MAX_INSTRUCTIONS) : 0;;

    return D3D_OK;
}

HRESULT WINAPI
NineAdapter9_CreateDevice( struct NineAdapter9 *This,
                           UINT RealAdapter,
                           D3DDEVTYPE DeviceType,
                           HWND hFocusWindow,
                           DWORD BehaviorFlags,
                           IDirect3D9 *pD3D9,
                           ID3DPresentFactory *pPresentationFactory,
                           IDirect3DDevice9 **ppReturnedDeviceInterface )
{
    struct pipe_screen *screen;
    D3DDEVICE_CREATION_PARAMETERS params;
    D3DCAPS9 caps;
    HRESULT hr;

    hr = NineAdapter9_GetScreen(This, DeviceType, &screen);
    if (FAILED(hr)) { return hr; }

    hr = NineAdapter9_GetDeviceCaps(This, DeviceType, &caps);
    if (FAILED(hr)) { return hr; }

    params.AdapterOrdinal = RealAdapter;
    params.DeviceType = DeviceType;
    params.hFocusWindow = hFocusWindow;
    params.BehaviorFlags = BehaviorFlags;

    hr = NineDevice9_new(screen, &params, &caps, pD3D9,
                         pPresentationFactory, This->ptrfunc,
                         (struct NineDevice9 **)ppReturnedDeviceInterface);
    if (FAILED(hr)) { return hr; }

    return D3D_OK;
}

HRESULT WINAPI
NineAdapter9_CreateDeviceEx( struct NineAdapter9 *This,
                             UINT RealAdapter,
                             D3DDEVTYPE DeviceType,
                             HWND hFocusWindow,
                             DWORD BehaviorFlags,
                             IDirect3D9Ex *pD3D9Ex,
                             ID3DPresentFactory *pPresentationFactory,
                             IDirect3DDevice9Ex **ppReturnedDeviceInterface )
{
    STUB(D3DERR_INVALIDCALL);
}

ID3DAdapter9Vtbl NineAdapter9_vtable = {
    (void *)NineUnknown_QueryInterface,
    (void *)NineUnknown_AddRef,
    (void *)NineUnknown_Release,
    (void *)NineAdapter9_GetAdapterIdentifier,
    (void *)NineAdapter9_CheckDeviceType,
    (void *)NineAdapter9_CheckDeviceFormat,
    (void *)NineAdapter9_CheckDeviceMultiSampleType,
    (void *)NineAdapter9_CheckDepthStencilMatch,
    (void *)NineAdapter9_CheckDeviceFormatConversion,
    (void *)NineAdapter9_GetDeviceCaps,
    (void *)NineAdapter9_CreateDevice,
    (void *)NineAdapter9_CreateDeviceEx
};

static const GUID *NineAdapter9_IIDs[] = {
    &IID_ID3D9Adapter,
    &IID_IUnknown,
    NULL
};

HRESULT
NineAdapter9_new( struct pipe_screen *pScreenHAL,
                  struct pipe_screen *pScreenREF,
                  D3DADAPTER_IDENTIFIER9 *pIdentifier,
                  PPRESENT_TO_RESOURCE pPTR,
                  void *pClosure,
                  void (*pDestructor)(void *),
                  struct NineAdapter9 **ppOut )
{
    NINE_NEW(NineAdapter9, ppOut,
             /* args */ pScreenHAL, pScreenREF, pIdentifier,
                        pPTR, pClosure, pDestructor);
}
