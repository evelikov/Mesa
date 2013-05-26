
#include "nine_dump.h"
#include "nine_debug.h"

#include <stdio.h>
#include "util/u_memory.h"

const char *nine_D3DDEVTYPE_to_str(D3DDEVTYPE type)
{
    switch (type) {
    case D3DDEVTYPE_HAL: return "HAL";
    case D3DDEVTYPE_NULLREF: return "NULLREF";
    case D3DDEVTYPE_REF: return "REF";
    case D3DDEVTYPE_SW: return "SW";
    default:
       return "(D3DDEVTYPE:?)";
    }
}

const char *nine_D3DQUERYTYPE_to_str(D3DQUERYTYPE type)
{
    switch (type) {
    case D3DQUERYTYPE_VCACHE: return "VCACHE";
    case D3DQUERYTYPE_RESOURCEMANAGER: return "RESOURCEMANAGER";
    case D3DQUERYTYPE_VERTEXSTATS: return "VERTEXSTATS";
    case D3DQUERYTYPE_EVENT: return "EVENT";
    case D3DQUERYTYPE_OCCLUSION: return "OCCLUSION";
    case D3DQUERYTYPE_TIMESTAMP: return "TIMESTAMP";
    case D3DQUERYTYPE_TIMESTAMPDISJOINT: return "TIMESTAMPDISJOINT";
    case D3DQUERYTYPE_TIMESTAMPFREQ: return "TIMESTAMPFREQ";
    case D3DQUERYTYPE_PIPELINETIMINGS: return "PIPELINETIMINGS";
    case D3DQUERYTYPE_INTERFACETIMINGS: return "INTERFACETIMINGS";
    case D3DQUERYTYPE_VERTEXTIMINGS: return "VERTEXTIMINGS";
    case D3DQUERYTYPE_PIXELTIMINGS: return "PIXELTIMINGS";
    case D3DQUERYTYPE_BANDWIDTHTIMINGS: return "BANDWIDTHTIMINGS";
    case D3DQUERYTYPE_CACHEUTILIZATION: return "CACHEUTILIZATION";
    default:
        return "(D3DQUERYTYPE:?)";
    }
}

void
nine_dump_D3DADAPTER_IDENTIFIER9(unsigned ch, const D3DADAPTER_IDENTIFIER9 *id)
{
    DBG_FLAG(ch, "D3DADAPTER_IDENTIFIER9(%p):\n"
             "Driver: %s\n"
             "Description: %s\n"
             "DeviceName: %s\n"
             "DriverVersion: %08x.%08x\n"
             "VendorId: %x\n"
             "DeviceId: %x\n"
             "SubSysId: %x\n"
             "Revision: %u\n"
             "GUID: %08x.%04x.%04x.%02x.%02x.%02x.%02x.%02x.%02x.%02x.%02x\n"
             "WHQLLevel: %u\n", id, id->Driver, id->Description,
             id->DeviceName,
             id->DriverVersionLowPart, id->DriverVersionHighPart,
             id->VendorId, id->DeviceId, id->SubSysId,
             id->Revision,
             id->DeviceIdentifier.Data1,
             id->DeviceIdentifier.Data2,
             id->DeviceIdentifier.Data3,
             id->DeviceIdentifier.Data4[0],
             id->DeviceIdentifier.Data4[1],
             id->DeviceIdentifier.Data4[2],
             id->DeviceIdentifier.Data4[3],
             id->DeviceIdentifier.Data4[4],
             id->DeviceIdentifier.Data4[5],
             id->DeviceIdentifier.Data4[6],
             id->DeviceIdentifier.Data4[7],
             id->WHQLLevel);
}

#define C2S(args...) p += snprintf(&s[p],c-p,args)

#define CAP_CASE(m,p,n) \
    do {                     \
        if (caps->m & p##_##n) \
            C2S(" "#n); \
        else \
            C2S(" ("#n")"); \
    } while(0)

void
nine_dump_D3DCAPS9(unsigned ch, const D3DCAPS9 *caps)
{
    const int c = 1 << 17;
    int p = 0;
    char *s = (char *)MALLOC(c);

    if (!s) {
        DBG_FLAG(ch, "D3DCAPS9(%p): (out of memory)\n", caps);
        return;
    }

    C2S("DeviceType: %s\n", nine_D3DDEVTYPE_to_str(caps->DeviceType));

    C2S("AdapterOrdinal: %u\nCaps:", caps->AdapterOrdinal);
    if (caps->Caps & 0x20000)
        C2S(" READ_SCANLINE");
    if (caps->Caps & ~0x20000)
        C2S(" %x", caps->Caps & ~0x20000);

    C2S("\nCaps2:");
    CAP_CASE(Caps2, D3DCAPS2, CANAUTOGENMIPMAP);
    CAP_CASE(Caps2, D3DCAPS2, CANCALIBRATEGAMMA);
    CAP_CASE(Caps2, D3DCAPS2, CANSHARERESOURCE);
    CAP_CASE(Caps2, D3DCAPS2, CANMANAGERESOURCE);
    CAP_CASE(Caps2, D3DCAPS2, DYNAMICTEXTURES);
    CAP_CASE(Caps2, D3DCAPS2, FULLSCREENGAMMA);

    C2S("\nCaps3:");
    CAP_CASE(Caps3, D3DCAPS3, ALPHA_FULLSCREEN_FLIP_OR_DISCARD);
    CAP_CASE(Caps3, D3DCAPS3, COPY_TO_VIDMEM);
    CAP_CASE(Caps3, D3DCAPS3, COPY_TO_SYSTEMMEM);
    CAP_CASE(Caps3, D3DCAPS3, DXVAHD);
    CAP_CASE(Caps3, D3DCAPS3, LINEAR_TO_SRGB_PRESENTATION);

    C2S("\nPresentationIntervals:");
    CAP_CASE(PresentationIntervals, D3DPRESENT_INTERVAL, ONE);
    CAP_CASE(PresentationIntervals, D3DPRESENT_INTERVAL, TWO);
    CAP_CASE(PresentationIntervals, D3DPRESENT_INTERVAL, THREE);
    CAP_CASE(PresentationIntervals, D3DPRESENT_INTERVAL, FOUR);
    CAP_CASE(PresentationIntervals, D3DPRESENT_INTERVAL, IMMEDIATE);

    C2S("\nCursorCaps:");
    CAP_CASE(CursorCaps, D3DCURSORCAPS, COLOR);
    CAP_CASE(CursorCaps, D3DCURSORCAPS, LOWRES);

    C2S("\nDevCaps:");
    CAP_CASE(DevCaps, D3DDEVCAPS, CANBLTSYSTONONLOCAL);
    CAP_CASE(DevCaps, D3DDEVCAPS, CANRENDERAFTERFLIP);
    CAP_CASE(DevCaps, D3DDEVCAPS, DRAWPRIMITIVES2);
    CAP_CASE(DevCaps, D3DDEVCAPS, DRAWPRIMITIVES2EX);
    CAP_CASE(DevCaps, D3DDEVCAPS, DRAWPRIMTLVERTEX);
    CAP_CASE(DevCaps, D3DDEVCAPS, EXECUTESYSTEMMEMORY);
    CAP_CASE(DevCaps, D3DDEVCAPS, EXECUTEVIDEOMEMORY);
    CAP_CASE(DevCaps, D3DDEVCAPS, HWRASTERIZATION);
    CAP_CASE(DevCaps, D3DDEVCAPS, HWTRANSFORMANDLIGHT);
    CAP_CASE(DevCaps, D3DDEVCAPS, NPATCHES);
    CAP_CASE(DevCaps, D3DDEVCAPS, PUREDEVICE);
    CAP_CASE(DevCaps, D3DDEVCAPS, QUINTICRTPATCHES);
    CAP_CASE(DevCaps, D3DDEVCAPS, RTPATCHES);
    CAP_CASE(DevCaps, D3DDEVCAPS, RTPATCHHANDLEZERO);
    CAP_CASE(DevCaps, D3DDEVCAPS, SEPARATETEXTUREMEMORIES);
    CAP_CASE(DevCaps, D3DDEVCAPS, TEXTURENONLOCALVIDMEM);
    CAP_CASE(DevCaps, D3DDEVCAPS, TEXTURESYSTEMMEMORY);
    CAP_CASE(DevCaps, D3DDEVCAPS, TEXTUREVIDEOMEMORY);
    CAP_CASE(DevCaps, D3DDEVCAPS, TLVERTEXSYSTEMMEMORY);
    CAP_CASE(DevCaps, D3DDEVCAPS, TLVERTEXVIDEOMEMORY);

    C2S("\nPrimitiveMiscCaps:");
    CAP_CASE(PrimitiveMiscCaps, D3DPMISCCAPS, MASKZ);
    CAP_CASE(PrimitiveMiscCaps, D3DPMISCCAPS, CULLNONE);
    CAP_CASE(PrimitiveMiscCaps, D3DPMISCCAPS, CULLCW);
    CAP_CASE(PrimitiveMiscCaps, D3DPMISCCAPS, CULLCCW);
    CAP_CASE(PrimitiveMiscCaps, D3DPMISCCAPS, COLORWRITEENABLE);
    CAP_CASE(PrimitiveMiscCaps, D3DPMISCCAPS, CLIPPLANESCALEDPOINTS);
    CAP_CASE(PrimitiveMiscCaps, D3DPMISCCAPS, CLIPTLVERTS);
    CAP_CASE(PrimitiveMiscCaps, D3DPMISCCAPS, TSSARGTEMP);
    CAP_CASE(PrimitiveMiscCaps, D3DPMISCCAPS, BLENDOP);
    CAP_CASE(PrimitiveMiscCaps, D3DPMISCCAPS, NULLREFERENCE);
    CAP_CASE(PrimitiveMiscCaps, D3DPMISCCAPS, INDEPENDENTWRITEMASKS);
    CAP_CASE(PrimitiveMiscCaps, D3DPMISCCAPS, PERSTAGECONSTANT);
    CAP_CASE(PrimitiveMiscCaps, D3DPMISCCAPS, POSTBLENDSRGBCONVERT);
    CAP_CASE(PrimitiveMiscCaps, D3DPMISCCAPS, FOGANDSPECULARALPHA);
    CAP_CASE(PrimitiveMiscCaps, D3DPMISCCAPS, SEPARATEALPHABLEND);
    CAP_CASE(PrimitiveMiscCaps, D3DPMISCCAPS, MRTINDEPENDENTBITDEPTHS);
    CAP_CASE(PrimitiveMiscCaps, D3DPMISCCAPS, MRTPOSTPIXELSHADERBLENDING);
    CAP_CASE(PrimitiveMiscCaps, D3DPMISCCAPS, FOGVERTEXCLAMPED);

    C2S("\nRasterCaps:");
    CAP_CASE(RasterCaps, D3DPRASTERCAPS, ANISOTROPY);
    CAP_CASE(RasterCaps, D3DPRASTERCAPS, COLORPERSPECTIVE);
    CAP_CASE(RasterCaps, D3DPRASTERCAPS, DITHER);
    CAP_CASE(RasterCaps, D3DPRASTERCAPS, DEPTHBIAS);
    CAP_CASE(RasterCaps, D3DPRASTERCAPS, FOGRANGE);
    CAP_CASE(RasterCaps, D3DPRASTERCAPS, FOGTABLE);
    CAP_CASE(RasterCaps, D3DPRASTERCAPS, FOGVERTEX);
    CAP_CASE(RasterCaps, D3DPRASTERCAPS, MIPMAPLODBIAS);
    CAP_CASE(RasterCaps, D3DPRASTERCAPS, MULTISAMPLE_TOGGLE);
    CAP_CASE(RasterCaps, D3DPRASTERCAPS, SCISSORTEST);
    CAP_CASE(RasterCaps, D3DPRASTERCAPS, SLOPESCALEDEPTHBIAS);
    CAP_CASE(RasterCaps, D3DPRASTERCAPS, WBUFFER);
    CAP_CASE(RasterCaps, D3DPRASTERCAPS, WFOG);
    CAP_CASE(RasterCaps, D3DPRASTERCAPS, ZBUFFERLESSHSR);
    CAP_CASE(RasterCaps, D3DPRASTERCAPS, ZFOG);
    CAP_CASE(RasterCaps, D3DPRASTERCAPS, ZTEST);

    C2S("\nZCmpCaps:");
    CAP_CASE(ZCmpCaps, D3DPCMPCAPS, ALWAYS);
    CAP_CASE(ZCmpCaps, D3DPCMPCAPS, EQUAL);
    CAP_CASE(ZCmpCaps, D3DPCMPCAPS, GREATER);
    CAP_CASE(ZCmpCaps, D3DPCMPCAPS, GREATEREQUAL);
    CAP_CASE(ZCmpCaps, D3DPCMPCAPS, LESS);
    CAP_CASE(ZCmpCaps, D3DPCMPCAPS, LESSEQUAL);
    CAP_CASE(ZCmpCaps, D3DPCMPCAPS, NEVER);
    CAP_CASE(ZCmpCaps, D3DPCMPCAPS, NOTEQUAL);

    C2S("\nSrcBlendCaps");
    CAP_CASE(SrcBlendCaps, D3DPBLENDCAPS, BLENDFACTOR);
    CAP_CASE(SrcBlendCaps, D3DPBLENDCAPS, BOTHINVSRCALPHA);
    CAP_CASE(SrcBlendCaps, D3DPBLENDCAPS, BOTHSRCALPHA);
    CAP_CASE(SrcBlendCaps, D3DPBLENDCAPS, DESTALPHA);
    CAP_CASE(SrcBlendCaps, D3DPBLENDCAPS, DESTCOLOR);
    CAP_CASE(SrcBlendCaps, D3DPBLENDCAPS, INVDESTALPHA);
    CAP_CASE(SrcBlendCaps, D3DPBLENDCAPS, INVDESTCOLOR);
    CAP_CASE(SrcBlendCaps, D3DPBLENDCAPS, INVSRCALPHA);
    CAP_CASE(SrcBlendCaps, D3DPBLENDCAPS, INVSRCCOLOR);
    CAP_CASE(SrcBlendCaps, D3DPBLENDCAPS, INVSRCCOLOR2);
    CAP_CASE(SrcBlendCaps, D3DPBLENDCAPS, ONE);
    CAP_CASE(SrcBlendCaps, D3DPBLENDCAPS, SRCALPHA);
    CAP_CASE(SrcBlendCaps, D3DPBLENDCAPS, SRCALPHASAT);
    CAP_CASE(SrcBlendCaps, D3DPBLENDCAPS, SRCCOLOR);
    CAP_CASE(SrcBlendCaps, D3DPBLENDCAPS, SRCCOLOR2);
    CAP_CASE(SrcBlendCaps, D3DPBLENDCAPS, ZERO);

    C2S("\nDestBlendCaps");
    CAP_CASE(DestBlendCaps, D3DPBLENDCAPS, BLENDFACTOR);
    CAP_CASE(DestBlendCaps, D3DPBLENDCAPS, BOTHINVSRCALPHA);
    CAP_CASE(DestBlendCaps, D3DPBLENDCAPS, BOTHSRCALPHA);
    CAP_CASE(DestBlendCaps, D3DPBLENDCAPS, DESTALPHA);
    CAP_CASE(DestBlendCaps, D3DPBLENDCAPS, DESTCOLOR);
    CAP_CASE(DestBlendCaps, D3DPBLENDCAPS, INVDESTALPHA);
    CAP_CASE(DestBlendCaps, D3DPBLENDCAPS, INVDESTCOLOR);
    CAP_CASE(DestBlendCaps, D3DPBLENDCAPS, INVSRCALPHA);
    CAP_CASE(DestBlendCaps, D3DPBLENDCAPS, INVSRCCOLOR);
    CAP_CASE(DestBlendCaps, D3DPBLENDCAPS, INVSRCCOLOR2);
    CAP_CASE(DestBlendCaps, D3DPBLENDCAPS, ONE);
    CAP_CASE(DestBlendCaps, D3DPBLENDCAPS, SRCALPHA);
    CAP_CASE(DestBlendCaps, D3DPBLENDCAPS, SRCALPHASAT);
    CAP_CASE(DestBlendCaps, D3DPBLENDCAPS, SRCCOLOR);
    CAP_CASE(DestBlendCaps, D3DPBLENDCAPS, SRCCOLOR2);
    CAP_CASE(DestBlendCaps, D3DPBLENDCAPS, ZERO);

    C2S("\nAlphaCmpCaps:");
    CAP_CASE(AlphaCmpCaps, D3DPCMPCAPS, ALWAYS);
    CAP_CASE(AlphaCmpCaps, D3DPCMPCAPS, EQUAL);
    CAP_CASE(AlphaCmpCaps, D3DPCMPCAPS, GREATER);
    CAP_CASE(AlphaCmpCaps, D3DPCMPCAPS, GREATEREQUAL);
    CAP_CASE(AlphaCmpCaps, D3DPCMPCAPS, LESS);
    CAP_CASE(AlphaCmpCaps, D3DPCMPCAPS, LESSEQUAL);
    CAP_CASE(AlphaCmpCaps, D3DPCMPCAPS, NEVER);
    CAP_CASE(AlphaCmpCaps, D3DPCMPCAPS, NOTEQUAL);

    C2S("\nShadeCaps:");
    CAP_CASE(ShadeCaps, D3DPSHADECAPS, ALPHAGOURAUDBLEND);
    CAP_CASE(ShadeCaps, D3DPSHADECAPS, COLORGOURAUDRGB);
    CAP_CASE(ShadeCaps, D3DPSHADECAPS, FOGGOURAUD);
    CAP_CASE(ShadeCaps, D3DPSHADECAPS, SPECULARGOURAUDRGB);

    C2S("\nTextureCaps:");
    CAP_CASE(TextureCaps, D3DPTEXTURECAPS, ALPHA);
    CAP_CASE(TextureCaps, D3DPTEXTURECAPS, ALPHAPALETTE);
    CAP_CASE(TextureCaps, D3DPTEXTURECAPS, CUBEMAP);
    CAP_CASE(TextureCaps, D3DPTEXTURECAPS, CUBEMAP_POW2);
    CAP_CASE(TextureCaps, D3DPTEXTURECAPS, MIPCUBEMAP);
    CAP_CASE(TextureCaps, D3DPTEXTURECAPS, MIPMAP);
    CAP_CASE(TextureCaps, D3DPTEXTURECAPS, MIPVOLUMEMAP);
    CAP_CASE(TextureCaps, D3DPTEXTURECAPS, NONPOW2CONDITIONAL);
    CAP_CASE(TextureCaps, D3DPTEXTURECAPS, NOPROJECTEDBUMPENV);
    CAP_CASE(TextureCaps, D3DPTEXTURECAPS, PERSPECTIVE);
    CAP_CASE(TextureCaps, D3DPTEXTURECAPS, POW2);
    CAP_CASE(TextureCaps, D3DPTEXTURECAPS, PROJECTED);
    CAP_CASE(TextureCaps, D3DPTEXTURECAPS, SQUAREONLY);
    CAP_CASE(TextureCaps, D3DPTEXTURECAPS, TEXREPEATNOTSCALEDBYSIZE);
    CAP_CASE(TextureCaps, D3DPTEXTURECAPS, VOLUMEMAP);
    CAP_CASE(TextureCaps, D3DPTEXTURECAPS, VOLUMEMAP_POW2);

    C2S("\nTextureFilterCaps:");
 /* CAP_CASE(TextureFilterCaps, D3DPTFILTERCAPS, CONVOLUTIONMONO); */
    CAP_CASE(TextureFilterCaps, D3DPTFILTERCAPS, MAGFPOINT);
    CAP_CASE(TextureFilterCaps, D3DPTFILTERCAPS, MAGFLINEAR);
    CAP_CASE(TextureFilterCaps, D3DPTFILTERCAPS, MAGFANISOTROPIC);
    CAP_CASE(TextureFilterCaps, D3DPTFILTERCAPS, MAGFPYRAMIDALQUAD);
    CAP_CASE(TextureFilterCaps, D3DPTFILTERCAPS, MAGFGAUSSIANQUAD);
    CAP_CASE(TextureFilterCaps, D3DPTFILTERCAPS, MINFPOINT);
    CAP_CASE(TextureFilterCaps, D3DPTFILTERCAPS, MINFLINEAR);
    CAP_CASE(TextureFilterCaps, D3DPTFILTERCAPS, MINFANISOTROPIC);
    CAP_CASE(TextureFilterCaps, D3DPTFILTERCAPS, MINFPYRAMIDALQUAD);
    CAP_CASE(TextureFilterCaps, D3DPTFILTERCAPS, MINFGAUSSIANQUAD);
    CAP_CASE(TextureFilterCaps, D3DPTFILTERCAPS, MIPFPOINT);
    CAP_CASE(TextureFilterCaps, D3DPTFILTERCAPS, MIPFLINEAR);

    C2S("\nCubeTextureFilterCaps:");
 /* CAP_CASE(CubeTextureFilterCaps, D3DPTFILTERCAPS, CONVOLUTIONMONO); */
    CAP_CASE(CubeTextureFilterCaps, D3DPTFILTERCAPS, MAGFPOINT);
    CAP_CASE(CubeTextureFilterCaps, D3DPTFILTERCAPS, MAGFLINEAR);
    CAP_CASE(CubeTextureFilterCaps, D3DPTFILTERCAPS, MAGFANISOTROPIC);
    CAP_CASE(CubeTextureFilterCaps, D3DPTFILTERCAPS, MAGFPYRAMIDALQUAD);
    CAP_CASE(CubeTextureFilterCaps, D3DPTFILTERCAPS, MAGFGAUSSIANQUAD);
    CAP_CASE(CubeTextureFilterCaps, D3DPTFILTERCAPS, MINFPOINT);
    CAP_CASE(CubeTextureFilterCaps, D3DPTFILTERCAPS, MINFLINEAR);
    CAP_CASE(CubeTextureFilterCaps, D3DPTFILTERCAPS, MINFANISOTROPIC);
    CAP_CASE(CubeTextureFilterCaps, D3DPTFILTERCAPS, MINFPYRAMIDALQUAD);
    CAP_CASE(CubeTextureFilterCaps, D3DPTFILTERCAPS, MINFGAUSSIANQUAD);
    CAP_CASE(CubeTextureFilterCaps, D3DPTFILTERCAPS, MIPFPOINT);
    CAP_CASE(CubeTextureFilterCaps, D3DPTFILTERCAPS, MIPFLINEAR);

    C2S("\nVolumeTextureFilterCaps:");
 /* CAP_CASE(VolumeTextureFilterCaps, D3DPTFILTERCAPS, CONVOLUTIONMONO); */
    CAP_CASE(VolumeTextureFilterCaps, D3DPTFILTERCAPS, MAGFPOINT);
    CAP_CASE(VolumeTextureFilterCaps, D3DPTFILTERCAPS, MAGFLINEAR);
    CAP_CASE(VolumeTextureFilterCaps, D3DPTFILTERCAPS, MAGFANISOTROPIC);
    CAP_CASE(VolumeTextureFilterCaps, D3DPTFILTERCAPS, MAGFPYRAMIDALQUAD);
    CAP_CASE(VolumeTextureFilterCaps, D3DPTFILTERCAPS, MAGFGAUSSIANQUAD);
    CAP_CASE(VolumeTextureFilterCaps, D3DPTFILTERCAPS, MINFPOINT);
    CAP_CASE(VolumeTextureFilterCaps, D3DPTFILTERCAPS, MINFLINEAR);
    CAP_CASE(VolumeTextureFilterCaps, D3DPTFILTERCAPS, MINFANISOTROPIC);
    CAP_CASE(VolumeTextureFilterCaps, D3DPTFILTERCAPS, MINFPYRAMIDALQUAD);
    CAP_CASE(VolumeTextureFilterCaps, D3DPTFILTERCAPS, MINFGAUSSIANQUAD);
    CAP_CASE(VolumeTextureFilterCaps, D3DPTFILTERCAPS, MIPFPOINT);
    CAP_CASE(VolumeTextureFilterCaps, D3DPTFILTERCAPS, MIPFLINEAR);

    C2S("\nTextureAddressCaps:");
    CAP_CASE(TextureAddressCaps, D3DPTADDRESSCAPS, BORDER);
    CAP_CASE(TextureAddressCaps, D3DPTADDRESSCAPS, CLAMP);
    CAP_CASE(TextureAddressCaps, D3DPTADDRESSCAPS, INDEPENDENTUV);
    CAP_CASE(TextureAddressCaps, D3DPTADDRESSCAPS, MIRROR);
    CAP_CASE(TextureAddressCaps, D3DPTADDRESSCAPS, MIRRORONCE);
    CAP_CASE(TextureAddressCaps, D3DPTADDRESSCAPS, WRAP);

    C2S("\nVolumeTextureAddressCaps:");
    CAP_CASE(VolumeTextureAddressCaps, D3DPTADDRESSCAPS, BORDER);
    CAP_CASE(VolumeTextureAddressCaps, D3DPTADDRESSCAPS, CLAMP);
    CAP_CASE(VolumeTextureAddressCaps, D3DPTADDRESSCAPS, INDEPENDENTUV);
    CAP_CASE(VolumeTextureAddressCaps, D3DPTADDRESSCAPS, MIRROR);
    CAP_CASE(VolumeTextureAddressCaps, D3DPTADDRESSCAPS, MIRRORONCE);
    CAP_CASE(VolumeTextureAddressCaps, D3DPTADDRESSCAPS, WRAP);

    C2S("\nLineCaps:");
    CAP_CASE(LineCaps, D3DLINECAPS, ALPHACMP);
    CAP_CASE(LineCaps, D3DLINECAPS, ANTIALIAS);
    CAP_CASE(LineCaps, D3DLINECAPS, BLEND);
    CAP_CASE(LineCaps, D3DLINECAPS, FOG);
    CAP_CASE(LineCaps, D3DLINECAPS, TEXTURE);
    CAP_CASE(LineCaps, D3DLINECAPS, ZTEST);

    C2S("\nMaxTextureWidth: %u", caps->MaxTextureWidth);
    C2S("\nMaxTextureHeight: %u", caps->MaxTextureHeight);
    C2S("\nMaxVolumeExtent: %u", caps->MaxVolumeExtent);
    C2S("\nMaxTextureRepeat: %u", caps->MaxTextureRepeat);
    C2S("\nMaxTextureAspectRatio: %u", caps->MaxTextureAspectRatio);
    C2S("\nMaxAnisotropy: %u", caps->MaxAnisotropy);
    C2S("\nMaxVertexW: %f", caps->MaxVertexW);

    C2S("\nGuardBandLef,Top,Right,Bottom: %f %f %f %f",
        caps->GuardBandLeft, caps->GuardBandTop,
        caps->GuardBandRight, caps->GuardBandBottom);

    C2S("\nExtentsAdjust: %f", caps->ExtentsAdjust);

    C2S("\nStencilCaps:");
    CAP_CASE(StencilCaps, D3DSTENCILCAPS, KEEP);
    CAP_CASE(StencilCaps, D3DSTENCILCAPS, ZERO);
    CAP_CASE(StencilCaps, D3DSTENCILCAPS, REPLACE);
    CAP_CASE(StencilCaps, D3DSTENCILCAPS, INCRSAT);
    CAP_CASE(StencilCaps, D3DSTENCILCAPS, DECRSAT);
    CAP_CASE(StencilCaps, D3DSTENCILCAPS, INVERT);
    CAP_CASE(StencilCaps, D3DSTENCILCAPS, INCR);
    CAP_CASE(StencilCaps, D3DSTENCILCAPS, DECR);
    CAP_CASE(StencilCaps, D3DSTENCILCAPS, TWOSIDED);

    C2S("\nFVFCaps:");
    CAP_CASE(FVFCaps, D3DFVFCAPS, DONOTSTRIPELEMENTS);
    CAP_CASE(FVFCaps, D3DFVFCAPS, PSIZE);
    CAP_CASE(FVFCaps, D3DFVFCAPS, TEXCOORDCOUNTMASK);

    C2S("\nTextureOpCaps:");
    CAP_CASE(TextureOpCaps, D3DTEXOPCAPS, ADD);
    CAP_CASE(TextureOpCaps, D3DTEXOPCAPS, ADDSIGNED);
    C2S(" ...");

    C2S("\nMaxTextureBlendStages: %u", caps->MaxTextureBlendStages);
    C2S("\nMaxSimultaneousTextures: %u", caps->MaxTextureBlendStages);

    C2S("\nVertexProcessingCaps:");
    CAP_CASE(VertexProcessingCaps, D3DVTXPCAPS, DIRECTIONALLIGHTS);
    CAP_CASE(VertexProcessingCaps, D3DVTXPCAPS, LOCALVIEWER);
    CAP_CASE(VertexProcessingCaps, D3DVTXPCAPS, MATERIALSOURCE7);
    CAP_CASE(VertexProcessingCaps, D3DVTXPCAPS, NO_TEXGEN_NONLOCALVIEWER);
    CAP_CASE(VertexProcessingCaps, D3DVTXPCAPS, POSITIONALLIGHTS);
    CAP_CASE(VertexProcessingCaps, D3DVTXPCAPS, TEXGEN);
    CAP_CASE(VertexProcessingCaps, D3DVTXPCAPS, TEXGEN_SPHEREMAP);
    CAP_CASE(VertexProcessingCaps, D3DVTXPCAPS, TWEENING);

    C2S("\nMaxActiveLights: %u", caps->MaxActiveLights);
    C2S("\nMaxUserClipPlanes: %u", caps->MaxUserClipPlanes);
    C2S("\nMaxVertexBlendMatrices: %u", caps->MaxVertexBlendMatrices);
    C2S("\nMaxVertexBlendMatrixIndex: %u", caps->MaxVertexBlendMatrixIndex);
    C2S("\nMaxPointSize: %f", caps->MaxPointSize);
    C2S("\nMaxPrimitiveCount: 0x%x", caps->MaxPrimitiveCount);
    C2S("\nMaxVertexIndex: 0x%x", caps->MaxVertexIndex);
    C2S("\nMaxStreams: %u", caps->MaxStreams);
    C2S("\nMaxStreamStride: 0x%x", caps->MaxStreamStride);

    C2S("\nVertexShaderVersion: %08x", caps->VertexShaderVersion);
    C2S("\nMaxVertexShaderConst: %u", caps->MaxVertexShaderConst);
    C2S("\nPixelShaderVersion: %08x", caps->PixelShaderVersion);
    C2S("\nPixelShader1xMaxValue: %f", caps->PixelShader1xMaxValue);

    DBG_FLAG(ch, "D3DCAPS9(%p) part 1:\n%s\n", caps, s);
    p = 0;

    C2S("DevCaps2:");
    CAP_CASE(DevCaps2, D3DDEVCAPS2, ADAPTIVETESSRTPATCH);
    CAP_CASE(DevCaps2, D3DDEVCAPS2, ADAPTIVETESSNPATCH);
    CAP_CASE(DevCaps2, D3DDEVCAPS2, CAN_STRETCHRECT_FROM_TEXTURES);
    CAP_CASE(DevCaps2, D3DDEVCAPS2, DMAPNPATCH);
    CAP_CASE(DevCaps2, D3DDEVCAPS2, PRESAMPLEDDMAPNPATCH);
    CAP_CASE(DevCaps2, D3DDEVCAPS2, STREAMOFFSET);
    CAP_CASE(DevCaps2, D3DDEVCAPS2, VERTEXELEMENTSCANSHARESTREAMOFFSET);

    C2S("\nMasterAdapterOrdinal: %u", caps->MasterAdapterOrdinal);
    C2S("\nAdapterOrdinalInGroup: %u", caps->AdapterOrdinalInGroup);
    C2S("\nNumberOfAdaptersInGroup: %u", caps->NumberOfAdaptersInGroup);

    C2S("\nDeclTypes:");
    CAP_CASE(DeclTypes, D3DDTCAPS, UBYTE4);
    CAP_CASE(DeclTypes, D3DDTCAPS, UBYTE4N);
    CAP_CASE(DeclTypes, D3DDTCAPS, SHORT2N);
    CAP_CASE(DeclTypes, D3DDTCAPS, SHORT4N);
    CAP_CASE(DeclTypes, D3DDTCAPS, USHORT2N);
    CAP_CASE(DeclTypes, D3DDTCAPS, USHORT4N);
    CAP_CASE(DeclTypes, D3DDTCAPS, UDEC3);
    CAP_CASE(DeclTypes, D3DDTCAPS, DEC3N);
    CAP_CASE(DeclTypes, D3DDTCAPS, FLOAT16_2);
    CAP_CASE(DeclTypes, D3DDTCAPS, FLOAT16_4);

    C2S("\nNumSimultaneousRTs: %u", caps->NumSimultaneousRTs);

    C2S("\nStretchRectFilterCaps:");
    CAP_CASE(StretchRectFilterCaps, D3DPTFILTERCAPS, MINFPOINT);
    CAP_CASE(StretchRectFilterCaps, D3DPTFILTERCAPS, MINFLINEAR);
    CAP_CASE(StretchRectFilterCaps, D3DPTFILTERCAPS, MAGFPOINT);
    CAP_CASE(StretchRectFilterCaps, D3DPTFILTERCAPS, MAGFLINEAR);

    C2S("\nVS20Caps.Caps: Predication=%s", caps->VS20Caps.Caps ? "yes" : "no");
    C2S("\nVS20Caps.DynamicFlowControlDepth: %u", caps->VS20Caps.DynamicFlowControlDepth);
    C2S("\nVS20Caps.NumTemps: %u", caps->VS20Caps.NumTemps);
    C2S("\nVS20Caps.StaticFlowControlDepth: %u", caps->VS20Caps.StaticFlowControlDepth);

    C2S("\nPS20Caps.Caps: Predication=%s", caps->VS20Caps.Caps ? "yes" : "no");
    C2S("\nPS20Caps.DynamicFlowControlDepth: %u", caps->PS20Caps.DynamicFlowControlDepth);
    C2S("\nPS20Caps.NumTemps: %u", caps->PS20Caps.NumTemps);
    C2S("\nPS20Caps.StaticFlowControlDepth: %u", caps->PS20Caps.StaticFlowControlDepth);
    C2S("\nPS20Caps.NumInstructionSlots: %u", caps->PS20Caps.NumInstructionSlots);

    C2S("\nVertexTextureFilterCaps");
 /* CAP_CASE(VertexTextureFilterCaps, D3DPTFILTERCAPS, CONVOLUTIONMONO); */
    CAP_CASE(VertexTextureFilterCaps, D3DPTFILTERCAPS, MAGFPOINT);
    CAP_CASE(VertexTextureFilterCaps, D3DPTFILTERCAPS, MAGFLINEAR);
    CAP_CASE(VertexTextureFilterCaps, D3DPTFILTERCAPS, MAGFANISOTROPIC);
    CAP_CASE(VertexTextureFilterCaps, D3DPTFILTERCAPS, MAGFPYRAMIDALQUAD);
    CAP_CASE(VertexTextureFilterCaps, D3DPTFILTERCAPS, MAGFGAUSSIANQUAD);
    CAP_CASE(VertexTextureFilterCaps, D3DPTFILTERCAPS, MINFPOINT);
    CAP_CASE(VertexTextureFilterCaps, D3DPTFILTERCAPS, MINFLINEAR);
    CAP_CASE(VertexTextureFilterCaps, D3DPTFILTERCAPS, MINFANISOTROPIC);
    CAP_CASE(VertexTextureFilterCaps, D3DPTFILTERCAPS, MINFPYRAMIDALQUAD);
    CAP_CASE(VertexTextureFilterCaps, D3DPTFILTERCAPS, MINFGAUSSIANQUAD);
    CAP_CASE(VertexTextureFilterCaps, D3DPTFILTERCAPS, MIPFPOINT);
    CAP_CASE(VertexTextureFilterCaps, D3DPTFILTERCAPS, MIPFLINEAR);

    C2S("\nMaxVShaderInstructionsExecuted: %u", caps->MaxVShaderInstructionsExecuted);
    C2S("\nMaxPShaderInstructionsExecuted: %u", caps->MaxPShaderInstructionsExecuted);
    C2S("\nMaxVertexShader30InstructionSlots: %u >= 512", caps->MaxVertexShader30InstructionSlots);
    C2S("\nMaxPixelShader30InstructionSlots: %u >= 512", caps->MaxPixelShader30InstructionSlots);

    DBG_FLAG(ch, "D3DCAPS9(%p) part 2:\n%s\n", caps, s);

    FREE(s);
}
