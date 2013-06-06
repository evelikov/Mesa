
/* FF is big and ugly so feel free to write lines as long as you like.
 * Aieeeeeeeee !
 *
 * Let me make that clearer:
 * Aieeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee ! !! !!!
 */

#include "device9.h"
#include "basetexture9.h"
#include "vertexdeclaration9.h"
#include "vertexshader9.h"
#include "pixelshader9.h"
#include "nine_ff.h"
#include "nine_defines.h"
#include "nine_helpers.h"
#include "nine_pipe.h"
#include "nine_dump.h"

#include "pipe/p_context.h"
#include "tgsi/tgsi_ureg.h"
#include "tgsi/tgsi_dump.h"
#include "util/u_box.h"
#include "util/u_hash_table.h"

#define DBG_CHANNEL DBG_FF

#define NINED3DTSS_TCI_DISABLE                       0
#define NINED3DTSS_TCI_PASSTHRU                      1
#define NINED3DTSS_TCI_CAMERASPACENORMAL             2
#define NINED3DTSS_TCI_CAMERASPACEPOSITION           3
#define NINED3DTSS_TCI_CAMERASPACEREFLECTIONVECTOR   4
#define NINED3DTSS_TCI_SPHEREMAP                     5

struct nine_ff_vs_key
{
    union {
        struct {
            uint32_t position_t : 1;
            uint32_t lighting   : 1;
            uint32_t darkness   : 1; /* lighting enabled but no active lights */
            uint32_t localviewer : 1;
            uint32_t vertexpointsize : 1;
            uint32_t pointscale : 1;
            uint32_t vertexblend : 3;
            uint32_t vertexblend_indexed : 1;
            uint32_t vertextween : 1;
            uint32_t mtl_diffuse : 2; /* 0 = material, 1 = color1, 2 = color2 */
            uint32_t mtl_ambient : 2;
            uint32_t mtl_specular : 2;
            uint32_t mtl_emissive : 2;
            uint32_t fog_mode : 2;
            uint32_t fog_range : 1;
            uint32_t color0in_one : 1;
            uint32_t color1in_one : 1;
            uint32_t pad1 : 9;
            uint32_t tc_gen   : 24; /* 8 * 3 */
            uint32_t pad2 : 8;
            uint32_t tc_idx : 24;
        };
        uint64_t value64[3]; /* if u64 isn't enough, resize VertexShader9.ff_key */
        uint32_t value32[6];
    };
};

/* Texture stage state:
 *
 * COLOROP       D3DTOP 5 bit
 * ALPHAOP       D3DTOP 5 bit
 * COLORARG0     D3DTA  3 bit
 * COLORARG1     D3DTA  3 bit
 * COLORARG2     D3DTA  3 bit
 * ALPHAARG0     D3DTA  3 bit
 * ALPHAARG1     D3DTA  3 bit
 * ALPHAARG2     D3DTA  3 bit
 * RESULTARG     D3DTA  1 bit (CURRENT:0 or TEMP:1)
 * TEXCOORDINDEX 0 - 7  3 bit
 * ===========================
 *                     32 bit per stage
 */
struct nine_ff_ps_key
{
    union {
        struct {
            struct {
                uint32_t colorop   : 5;
                uint32_t alphaop   : 5;
                uint32_t colorarg0 : 3;
                uint32_t colorarg1 : 3;
                uint32_t colorarg2 : 3;
                uint32_t alphaarg0 : 3;
                uint32_t alphaarg1 : 3;
                uint32_t alphaarg2 : 3;
                uint32_t resultarg : 1; /* CURRENT:0 or TEMP:1 */
                uint32_t textarget : 2; /* 1D/2D/3D/CUBE */
                uint32_t projected : 1;
                /* that's 32 bit exactly */
            } ts[8];
            uint32_t fog : 1; /* for vFog with programmable VS */
            uint32_t fog_mode : 2;
            uint32_t specular : 1; /* 9 32-bit words with this */
            uint8_t colorarg_b4[3];
            uint8_t colorarg_b5[3];
            uint8_t alphaarg_b4[3]; /* 11 32-bit words plus a byte */
        };
        uint64_t value64[6]; /* if u64 isn't enough, resize PixelShader9.ff_key */
        uint32_t value32[12];
    };
};

static unsigned nine_ff_vs_key_hash(void *key)
{
    struct nine_ff_vs_key *vs = key;
    unsigned i;
    uint32_t hash = vs->value32[0];
    for (i = 1; i < Elements(vs->value32); ++i)
        hash ^= vs->value32[i];
    return hash;
}
static int nine_ff_vs_key_comp(void *key1, void *key2)
{
    struct nine_ff_vs_key *a = (struct nine_ff_vs_key *)key1;
    struct nine_ff_vs_key *b = (struct nine_ff_vs_key *)key2;

    return memcmp(a->value64, b->value64, sizeof(a->value64));
}
static unsigned nine_ff_ps_key_hash(void *key)
{
    struct nine_ff_ps_key *ps = key;
    unsigned i;
    uint32_t hash = ps->value32[0];
    for (i = 1; i < Elements(ps->value32); ++i)
        hash ^= ps->value32[i];
    return hash;
}
static int nine_ff_ps_key_comp(void *key1, void *key2)
{
    struct nine_ff_ps_key *a = (struct nine_ff_ps_key *)key1;
    struct nine_ff_ps_key *b = (struct nine_ff_ps_key *)key2;

    return memcmp(a->value64, b->value64, sizeof(a->value64));
}
static unsigned nine_ff_fvf_key_hash(void *key)
{
    return *(DWORD *)key;
}
static int nine_ff_fvf_key_comp(void *key1, void *key2)
{
    return *(DWORD *)key1 != *(DWORD *)key2;
}

static void nine_ff_prune_vs(struct NineDevice9 *);
static void nine_ff_prune_ps(struct NineDevice9 *);

static void nine_ureg_tgsi_dump(struct ureg_program *ureg)
{
    if (debug_get_bool_option("NINE_FF_DUMP", FALSE)) {
        unsigned count;
        const struct tgsi_token *toks = ureg_get_tokens(ureg, &count);
        tgsi_dump(toks, 0);
        ureg_free_tokens(toks);
    }
}

#define _X(r) ureg_scalar(ureg_src(r), TGSI_SWIZZLE_X)
#define _Y(r) ureg_scalar(ureg_src(r), TGSI_SWIZZLE_Y)
#define _Z(r) ureg_scalar(ureg_src(r), TGSI_SWIZZLE_Z)
#define _W(r) ureg_scalar(ureg_src(r), TGSI_SWIZZLE_W)

#define _XXXX(r) ureg_scalar(r, TGSI_SWIZZLE_X)
#define _YYYY(r) ureg_scalar(r, TGSI_SWIZZLE_Y)
#define _ZZZZ(r) ureg_scalar(r, TGSI_SWIZZLE_Z)
#define _WWWW(r) ureg_scalar(r, TGSI_SWIZZLE_W)

#define _XYZW(r) (r)

/* AL should contain base address of lights table. */
#define LIGHT_CONST(i)                                                \
    ureg_src_indirect(ureg_src_register(TGSI_FILE_CONSTANT, (i)), _X(AL))

#define MATERIAL_CONST(i) \
    ureg_src_register(TGSI_FILE_CONSTANT, 19 + (i))

#define MISC_CONST(i) \
    ureg_src_register(TGSI_FILE_CONSTANT, (i))

#define _CONST(n) ureg_DECL_constant(ureg, n)

/* VS FF constants layout:
 *
 * CONST[ 0.. 3] D3DTS_WORLD * D3DTS_VIEW * D3DTS_PROJECTION
 * CONST[ 4.. 7] D3DTS_WORLD * D3DTS_VIEW
 * CONST[ 8..11] D3DTS_VIEW * D3DTS_PROJECTION
 * CONST[12..15] D3DTS_VIEW
 * CONST[16..18] Normal matrix
 *
 * CONST[19]      MATERIAL.Emissive + Material.Ambient * RS.Ambient
 * CONST[20]      MATERIAL.Diffuse
 * CONST[21]      MATERIAL.Ambient
 * CONST[22]      MATERIAL.Specular
 * CONST[23].x___ MATERIAL.Power
 * CONST[24]      MATERIAL.Emissive
 * CONST[25]      RS.Ambient
 *
 * CONST[26].x___ RS.PointSizeMin
 * CONST[26]._y__ RS.PointSizeMax
 * CONST[26].__z_ RS.PointSize
 * CONST[26].___w RS.PointScaleA
 * CONST[27].x___ RS.PointScaleB
 * CONST[27]._y__ RS.PointScaleC
 *
 * CONST[28].x___ RS.FogEnd
 * CONST[28]._y__ 1.0f / (RS.FogEnd - RS.FogStart)
 * CONST[28].__z_ RS.FogDensity
 * CONST[29]      RS.FogColor
 *
 * CONST[32].x___ LIGHT[0].Type
 * CONST[32]._yzw LIGHT[0].Attenuation0,1,2
 * CONST[33]      LIGHT[0].Diffuse
 * CONST[34]      LIGHT[0].Specular
 * CONST[35]      LIGHT[0].Ambient
 * CONST[36].xyz_ LIGHT[0].Position
 * CONST[36].___w LIGHT[0].Range
 * CONST[37].xyz_ LIGHT[0].Direction
 * CONST[37].___w LIGHT[0].Falloff
 * CONST[38].x___ cos(LIGHT[0].Theta / 2)
 * CONST[38]._y__ cos(LIGHT[0].Phi / 2)
 * CONST[38].__z_ 1.0f / (cos(LIGHT[0].Theta / 2) - cos(Light[0].Phi / 2))
 * CONST[39].xyz_ LIGHT[0].HalfVector (for directional lights)
 * CONST[39].___w 1 if this is the last active light, 0 if not
 * CONST[40]      LIGHT[1]
 * CONST[48]      LIGHT[2]
 * CONST[56]      LIGHT[3]
 * CONST[64]      LIGHT[4]
 * CONST[72]      LIGHT[5]
 * CONST[80]      LIGHT[6]
 * CONST[88]      LIGHT[7]
 * NOTE: no lighting code is generated if there are no active lights
 *
 * CONST[128..131] D3DTS_TEXTURE0
 * CONST[132..135] D3DTS_TEXTURE1
 * CONST[136..139] D3DTS_TEXTURE2
 * CONST[140..143] D3DTS_TEXTURE3
 * CONST[144..147] D3DTS_TEXTURE4
 * CONST[148..151] D3DTS_TEXTURE5
 * CONST[152..155] D3DTS_TEXTURE6
 * CONST[156..159] D3DTS_TEXTURE7
 *
 * CONST[224] D3DTS_WORLDMATRIX[0]
 * CONST[228] D3DTS_WORLDMATRIX[1]
 * ...
 * CONST[252] D3DTS_WORLDMATRIX[7]
 */
struct vs_build_ctx
{
    struct ureg_program *ureg;
    const struct nine_ff_vs_key *key;

    unsigned input[PIPE_MAX_ATTRIBS];
    unsigned num_inputs;

    struct ureg_src aVtx;
    struct ureg_src aNrm;
    struct ureg_src aCol[2];
    struct ureg_src aTex[8];
    struct ureg_src aPsz;
    struct ureg_src aInd;
    struct ureg_src aWgt;

    struct ureg_src mtlA;
    struct ureg_src mtlD;
    struct ureg_src mtlS;
    struct ureg_src mtlE;
};

static INLINE struct ureg_src
build_vs_add_input(struct vs_build_ctx *vs, unsigned ndecl)
{
    const unsigned i = vs->num_inputs++;
    assert(i < PIPE_MAX_ATTRIBS);
    vs->input[i] = ndecl;
    return ureg_DECL_vs_input(vs->ureg, i);
}

static void *
nine_ff_build_vs(struct NineDevice9 *device, struct vs_build_ctx *vs)
{
    const struct nine_ff_vs_key *key = vs->key;
    struct ureg_program *ureg = ureg_create(TGSI_PROCESSOR_VERTEX);
    struct ureg_dst oPos, oCol[2], oTex[8], oPsz, oFog;
    struct ureg_dst rCol[2]; /* oCol if no fog, TEMP otherwise */
    struct ureg_dst rVtx, rNrm;
    struct ureg_dst r[8];
    struct ureg_dst AR;
    struct ureg_dst tmp, tmp_x, tmp_z;
    unsigned i;
    unsigned label[32], l = 0;
    unsigned num_r = 8;
    boolean need_rNrm = key->lighting || key->pointscale;
    boolean need_rVtx = key->lighting || key->fog_mode;

    vs->ureg = ureg;

    /* Check which inputs we should transform. */
    for (i = 0; i < 8 * 3; i += 3) {
        switch ((key->tc_gen >> i) & 0x3) {
        case NINED3DTSS_TCI_CAMERASPACENORMAL:
            need_rNrm = TRUE;
            break;
        case NINED3DTSS_TCI_CAMERASPACEPOSITION:
            need_rVtx = TRUE;
            break;
        case NINED3DTSS_TCI_CAMERASPACEREFLECTIONVECTOR:
            need_rVtx = need_rNrm = TRUE;
            break;
        default:
            break;
        }
    }

    /* Declare and record used inputs (needed for linkage with vertex format):
     * (texture coordinates handled later)
     */
    vs->aVtx = build_vs_add_input(vs,
        key->position_t ? NINE_DECLUSAGE_POSITIONT : NINE_DECLUSAGE_POSITION);

    if (need_rNrm)
        vs->aNrm = build_vs_add_input(vs, NINE_DECLUSAGE_NORMAL(0));

    vs->aCol[0] = ureg_imm1f(ureg, 1.0f);
    vs->aCol[1] = ureg_imm1f(ureg, 1.0f);

    if (key->lighting || key->darkness) {
        const unsigned mask = key->mtl_diffuse | key->mtl_specular |
                              key->mtl_ambient | key->mtl_emissive;
        if ((mask & 0x1) && !key->color0in_one)
            vs->aCol[0] = build_vs_add_input(vs, NINE_DECLUSAGE_COLOR(0));
        if ((mask & 0x2) && !key->color1in_one)
            vs->aCol[1] = build_vs_add_input(vs, NINE_DECLUSAGE_COLOR(1));

        vs->mtlD = MATERIAL_CONST(1);
        vs->mtlA = MATERIAL_CONST(2);
        vs->mtlS = MATERIAL_CONST(3);
        vs->mtlE = MATERIAL_CONST(5);
        if (key->mtl_diffuse  == 1) vs->mtlD = vs->aCol[0]; else
        if (key->mtl_diffuse  == 2) vs->mtlD = vs->aCol[1];
        if (key->mtl_ambient  == 1) vs->mtlA = vs->aCol[0]; else
        if (key->mtl_ambient  == 2) vs->mtlA = vs->aCol[1];
        if (key->mtl_specular == 1) vs->mtlS = vs->aCol[0]; else
        if (key->mtl_specular == 2) vs->mtlS = vs->aCol[1];
        if (key->mtl_emissive == 1) vs->mtlE = vs->aCol[0]; else
        if (key->mtl_emissive == 2) vs->mtlE = vs->aCol[1];
    } else {
        if (!key->color0in_one) vs->aCol[0] = build_vs_add_input(vs, NINE_DECLUSAGE_COLOR(0));
        if (!key->color1in_one) vs->aCol[1] = build_vs_add_input(vs, NINE_DECLUSAGE_COLOR(1));
    }

    if (key->vertexpointsize)
        vs->aPsz = build_vs_add_input(vs, NINE_DECLUSAGE_PSIZE);

    if (key->vertexblend_indexed)
        vs->aInd = build_vs_add_input(vs, NINE_DECLUSAGE_BLENDINDICES);
    if (key->vertexblend)
        vs->aWgt = build_vs_add_input(vs, NINE_DECLUSAGE_BLENDWEIGHT);

    /* Declare outputs:
     */
    oPos = ureg_DECL_output(ureg, TGSI_SEMANTIC_POSITION, 0); /* HPOS */
    oCol[0] = ureg_DECL_output(ureg, TGSI_SEMANTIC_COLOR, 0);
    oCol[1] = ureg_DECL_output(ureg, TGSI_SEMANTIC_COLOR, 1);

    if (key->vertexpointsize || key->pointscale) {
        oPsz = ureg_DECL_output_masked(ureg, TGSI_SEMANTIC_PSIZE, 0, TGSI_WRITEMASK_X);
        oPsz = ureg_writemask(oPsz, TGSI_WRITEMASK_X);
    }
    if (key->fog_mode) {
        /* We apply fog to the vertex colors, oFog is for programmable shaders only ?
         */
        oFog = ureg_DECL_output_masked(ureg, TGSI_SEMANTIC_FOG, 0, TGSI_WRITEMASK_X);
        oFog = ureg_writemask(oFog, TGSI_WRITEMASK_X);
    }

    /* Declare TEMPs:
     */
    for (i = 0; i < num_r; ++i)
        r[i] = ureg_DECL_local_temporary(ureg);
    tmp = r[0];
    tmp_x = ureg_writemask(tmp, TGSI_WRITEMASK_X);
    tmp_z = ureg_writemask(tmp, TGSI_WRITEMASK_Z);
    if (key->lighting || key->vertexblend)
        AR = ureg_DECL_address(ureg);

    if (key->fog_mode) {
        rCol[0] = r[2];
        rCol[1] = r[3];
    } else {
        rCol[0] = oCol[0];
        rCol[1] = oCol[1];
    }

    rVtx = ureg_writemask(r[1], TGSI_WRITEMASK_XYZ);
    rNrm = ureg_writemask(r[2], TGSI_WRITEMASK_XYZ);

    /* === Vertex transformation / vertex blending:
     */
    if (key->vertexblend) {
        struct ureg_src cWM[4];
        struct ureg_src aInd;
        struct ureg_src aWgt;

        for (i = 224; i <= 255; ++i)
            ureg_DECL_constant(ureg, i);
        for (i = 0; i < 4; ++i)
            cWM[i] = ureg_src_indirect(ureg_src_register(TGSI_FILE_CONSTANT, i), _X(AR));

        /* translate world matrix index to constant file index */
        if (key->vertexblend_indexed)
            ureg_MAD(ureg, tmp, aInd, ureg_imm1f(ureg, 4.0f), ureg_imm1f(ureg, 224.0f));
        else
            ureg_MOV(ureg, tmp, ureg_imm4f(ureg, 224.0f, 228.0f, 232.0f, 236.0f));
        for (i = 0; i < key->vertexblend; ++i) {
            ureg_ARL(ureg, AR, ureg_scalar(ureg_src(tmp), i));

            /* multiply by WORLD(index) */
            ureg_MUL(ureg, r[0], _XXXX(vs->aVtx), cWM[0]);
            ureg_MAD(ureg, r[0], _YYYY(vs->aVtx), cWM[1], ureg_src(r[0]));
            ureg_MAD(ureg, r[0], _ZZZZ(vs->aVtx), cWM[2], ureg_src(r[0]));
            ureg_MAD(ureg, r[0], _WWWW(vs->aVtx), cWM[3], ureg_src(r[0]));

            /* accumulate weighted position value */
            ureg_MAD(ureg, r[1], ureg_src(r[0]), ureg_scalar(aWgt, i), ureg_src(r[1]));
        }
        /* multiply by VIEW_PROJ */
        ureg_MUL(ureg, r[0], _X(r[1]), _CONST(8));
        ureg_MAD(ureg, r[0], _Y(r[1]), _CONST(9),  ureg_src(r[0]));
        ureg_MAD(ureg, r[0], _Z(r[1]), _CONST(10), ureg_src(r[0]));
        ureg_MAD(ureg, oPos, _W(r[1]), _CONST(11), ureg_src(r[0]));
    } else
    if (key->position_t) {
        ureg_MOV(ureg, oPos, vs->aVtx);
    } else {
        /* position = vertex * WORLD_VIEW_PROJ */
        ureg_MUL(ureg, r[0], _XXXX(vs->aVtx), _CONST(0));
        ureg_MAD(ureg, r[0], _YYYY(vs->aVtx), _CONST(1), ureg_src(r[0]));
        ureg_MAD(ureg, r[0], _ZZZZ(vs->aVtx), _CONST(2), ureg_src(r[0]));
        ureg_MAD(ureg, oPos, _WWWW(vs->aVtx), _CONST(3), ureg_src(r[0]));
    }

    if (need_rVtx) {
        ureg_MUL(ureg, rVtx, _XXXX(vs->aVtx), _CONST(4));
        ureg_MAD(ureg, rVtx, _YYYY(vs->aVtx), _CONST(5), ureg_src(rVtx));
        ureg_MAD(ureg, rVtx, _ZZZZ(vs->aVtx), _CONST(6), ureg_src(rVtx));
        ureg_MAD(ureg, rVtx, _WWWW(vs->aVtx), _CONST(7), ureg_src(rVtx));
    }
    if (need_rNrm) {
        ureg_MUL(ureg, rNrm, _XXXX(vs->aNrm), _CONST(16));
        ureg_MAD(ureg, rNrm, _YYYY(vs->aNrm), _CONST(17), ureg_src(rNrm));
        ureg_MAD(ureg, rNrm, _ZZZZ(vs->aNrm), _CONST(18), ureg_src(rNrm));
        ureg_NRM(ureg, rNrm, ureg_src(rNrm));
    }

    /* === Process point size:
     */
    if (key->vertexpointsize) {
        struct ureg_src cPsz1 = ureg_DECL_constant(ureg, 26);
        ureg_CLAMP(ureg, oPsz, vs->aPsz, _XXXX(cPsz1), _YYYY(cPsz1));
    } else
    if (key->pointscale) {
        struct ureg_dst tmp_x = ureg_writemask(tmp, TGSI_WRITEMASK_X);
        struct ureg_dst tmp_y = ureg_writemask(tmp, TGSI_WRITEMASK_Y);
        struct ureg_src cPsz1 = ureg_DECL_constant(ureg, 26);
        struct ureg_src cPsz2 = ureg_DECL_constant(ureg, 27);

        ureg_DP3(ureg, tmp_x, ureg_src(r[1]), ureg_src(r[1]));
        ureg_SQRT(ureg, tmp_y, _X(tmp));
        ureg_MAD(ureg, tmp_x, _Y(tmp), _YYYY(cPsz2), _XXXX(cPsz2));
        ureg_MAD(ureg, tmp_x, _Y(tmp), _X(tmp), _WWWW(cPsz1));
        ureg_RCP(ureg, tmp_x, ureg_src(tmp));
        ureg_MUL(ureg, tmp_x, ureg_src(tmp), _ZZZZ(cPsz1));
        ureg_CLAMP(ureg, oPsz, _X(tmp), _XXXX(cPsz1), _YYYY(cPsz1));
    }

    /* Texture coordinate generation:
     * XXX: D3DTTFF_PROJECTED, transform matrix
     */
    for (i = 0; i < 8; ++i) {
        const unsigned tci = (key->tc_gen >> (i * 3)) & 0x3;
        const unsigned idx = (key->tc_idx >> (i * 3)) & 0x3;

        if (tci == NINED3DTSS_TCI_DISABLE)
            continue;
        oTex[i] = ureg_DECL_output(ureg, TGSI_SEMANTIC_TEXCOORD, i);

        if (tci == NINED3DTSS_TCI_PASSTHRU)
            vs->aTex[idx] = build_vs_add_input(vs, NINE_DECLUSAGE_TEXCOORD(idx));

        switch (tci) {
        case NINED3DTSS_TCI_PASSTHRU:
            ureg_MOV(ureg, oTex[i], vs->aTex[idx]);
            break;
        case NINED3DTSS_TCI_CAMERASPACENORMAL:
            ureg_MOV(ureg, oTex[i], ureg_src(rNrm));
            break;
        case NINED3DTSS_TCI_CAMERASPACEPOSITION:
            ureg_MOV(ureg, oTex[i], ureg_src(rVtx));
            break;
        case NINED3DTSS_TCI_CAMERASPACEREFLECTIONVECTOR:
        case NINED3DTSS_TCI_SPHEREMAP:
            assert(!"TODO");
            break;
        default:
            break;
        }
    }

    /* === Lighting:
     *
     * DIRECTIONAL:  Light at infinite distance, parallel rays, no attenuation.
     * POINT: Finite distance to scene, divergent rays, isotropic, attenuation.
     * SPOT: Finite distance, divergent rays, angular dependence, attenuation.
     *
     * vec3 normal = normalize(in.Normal * NormalMatrix);
     * vec3 hitDir = light.direction;
     * float atten = 1.0;
     *
     * if (light.type != DIRECTIONAL)
     * {
     *     vec3 hitVec = light.position - eyeVertex;
     *     float d = length(hitVec);
     *     hitDir = hitVec / d;
     *     atten = 1 / ((light.atten2 * d + light.atten1) * d + light.atten0);
     * }
     *
     * if (light.type == SPOTLIGHT)
     * {
     *     float rho = dp3(-hitVec, light.direction);
     *     if (rho < cos(light.phi / 2))
     *         atten = 0;
     *     if (rho < cos(light.theta / 2))
     *         atten *= pow(some_func(rho), light.falloff);
     * }
     *
     * float nDotHit = dp3_sat(normal, hitVec);
     * float powFact = 0.0;
     *
     * if (nDotHit > 0.0)
     * {
     *     vec3 midVec = normalize(hitDir + eye);
     *     float nDotMid = dp3_sat(normal, midVec);
     *     pFact = pow(nDotMid, material.power);
     * }
     *
     * ambient += light.ambient * atten;
     * diffuse += light.diffuse * atten * nDotHit;
     * specular += light.specular * atten * powFact;
     */
    if (key->lighting) {
        struct ureg_dst tmp_y = ureg_writemask(tmp, TGSI_WRITEMASK_Y);

        struct ureg_dst rAtt = ureg_writemask(r[1], TGSI_WRITEMASK_W);
        struct ureg_dst rHit = ureg_writemask(r[3], TGSI_WRITEMASK_XYZ);
        struct ureg_dst rMid = ureg_writemask(r[4], TGSI_WRITEMASK_XYZ);

        struct ureg_dst rCtr = ureg_writemask(r[2], TGSI_WRITEMASK_W);

        struct ureg_dst AL = ureg_writemask(AR, TGSI_WRITEMASK_X);

        /* Light.*.Alpha is not used. */
        struct ureg_dst rD = ureg_writemask(r[5], TGSI_WRITEMASK_XYZ);
        struct ureg_dst rA = ureg_writemask(r[6], TGSI_WRITEMASK_XYZ);
        struct ureg_dst rS = ureg_writemask(r[7], TGSI_WRITEMASK_XYZ);

        struct ureg_src mtlP = _XXXX(MATERIAL_CONST(4));

        struct ureg_src cLKind = _XXXX(LIGHT_CONST(0));
        struct ureg_src cLAtt0 = _YYYY(LIGHT_CONST(0));
        struct ureg_src cLAtt1 = _ZZZZ(LIGHT_CONST(0));
        struct ureg_src cLAtt2 = _WWWW(LIGHT_CONST(0));
        struct ureg_src cLColD = _XYZW(LIGHT_CONST(1));
        struct ureg_src cLColS = _XYZW(LIGHT_CONST(2));
        struct ureg_src cLColA = _XYZW(LIGHT_CONST(3));
        struct ureg_src cLPos  = _XYZW(LIGHT_CONST(4));
        struct ureg_src cLRng  = _WWWW(LIGHT_CONST(4));
        struct ureg_src cLDir  = _XYZW(LIGHT_CONST(5));
        struct ureg_src cLFOff = _WWWW(LIGHT_CONST(5));
        struct ureg_src cLTht  = _XXXX(LIGHT_CONST(6));
        struct ureg_src cLPhi  = _YYYY(LIGHT_CONST(6));
        struct ureg_src cLSDiv = _ZZZZ(LIGHT_CONST(6));
        struct ureg_src cLLast = _WWWW(LIGHT_CONST(7));

        const unsigned loop_label = l++;

        ureg_MOV(ureg, rCtr, ureg_imm1f(ureg, 32.0f)); /* &lightconst(0) */
        ureg_MOV(ureg, rD, ureg_imm1f(ureg, 0.0f));
        ureg_MOV(ureg, rA, ureg_imm1f(ureg, 0.0f));
        ureg_MOV(ureg, rS, ureg_imm1f(ureg, 0.0f));
        rD = ureg_saturate(rD);
        rA = ureg_saturate(rA);
        rS = ureg_saturate(rS);


        /* loop management */
        ureg_BGNLOOP(ureg, &label[loop_label]);
        ureg_ARL(ureg, AL, _W(rCtr));

        /* if (not DIRECTIONAL light): */
        ureg_SNE(ureg, tmp_x, cLKind, ureg_imm1f(ureg, D3DLIGHT_DIRECTIONAL));
        ureg_MOV(ureg, rHit, ureg_negate(cLDir));
        ureg_MOV(ureg, rAtt, ureg_imm1f(ureg, 1.0f));
        ureg_IF(ureg, _X(tmp), &label[l++]);
        {
            /* hitDir = light.position - eyeVtx
             * d = length(hitDir)
             * hitDir /= d
             */
            ureg_SUB(ureg, rHit, cLPos, ureg_src(rVtx));
            ureg_DP3(ureg, tmp_x, ureg_src(rHit), ureg_src(rHit));
            ureg_RSQ(ureg, tmp_y, _X(tmp));
            ureg_MUL(ureg, rHit, ureg_src(rHit), _Y(tmp)); /* normalize */
            ureg_MUL(ureg, tmp_x, _X(tmp), _Y(tmp)); /* length */

            /* att = 1.0 / (light.att0 + (light.att1 + light.att2 * d) * d) */
            ureg_MAD(ureg, rAtt, _X(tmp), cLAtt2, cLAtt1);
            ureg_MAD(ureg, rAtt, _X(tmp), _W(rAtt), cLAtt0);
            ureg_RCP(ureg, rAtt, _W(rAtt));
            /* cut-off if distance exceeds Light.Range */
            ureg_SLT(ureg, tmp_x, _X(tmp), cLRng);
            ureg_MUL(ureg, rAtt, _W(rAtt), _X(tmp));
        }
        ureg_fixup_label(ureg, label[l-1], ureg_get_instruction_number(ureg));
        ureg_ENDIF(ureg);

        /* if (SPOT light) */
        ureg_SEQ(ureg, tmp_x, cLKind, ureg_imm1f(ureg, D3DLIGHT_SPOT));
        ureg_IF(ureg, _X(tmp), &label[l++]);
        {
            /* rho = dp3(-hitDir, light.spotDir)
             *
             * if (rho  > light.ctht2) NOTE: 0 <= phi <= pi, 0 <= theta <= phi
             *     spotAtt = 1
             * else
             * if (rho <= light.cphi2)
             *     spotAtt = 0
             * else
             *     spotAtt = (rho - light.cphi2) / (light.ctht2 - light.cphi2) ^ light.falloff
             */
            ureg_DP3(ureg, tmp_y, ureg_negate(ureg_src(rHit)), cLDir); /* rho */
            ureg_SUB(ureg, tmp_x, _Y(tmp), cLPhi);
            ureg_MUL(ureg, tmp_x, _X(tmp), cLSDiv);
            ureg_POW(ureg, tmp_x, _X(tmp), cLFOff); /* spotAtten */
            ureg_SGE(ureg, tmp_z, _Y(tmp), cLTht); /* if inside theta && phi */
            ureg_SGE(ureg, tmp_y, _Y(tmp), cLPhi); /* if inside phi */
            ureg_MAD(ureg, ureg_saturate(tmp_x), _X(tmp), _Y(tmp), _Z(tmp));
            ureg_MUL(ureg, rAtt, _W(rAtt), _X(tmp));
        }
        ureg_fixup_label(ureg, label[l-1], ureg_get_instruction_number(ureg));
        ureg_ENDIF(ureg);

        /* directional factors, let's not use LIT because of clarity */
        ureg_DP3(ureg, ureg_saturate(tmp_x), ureg_src(rNrm), ureg_src(rHit));
        ureg_MOV(ureg, tmp_y, ureg_imm1f(ureg, 0.0f));
        ureg_IF(ureg, _X(tmp), &label[l++]);
        {
            /* midVec = normalize(hitDir + eyeDir) */
            if (key->localviewer) {
                ureg_NRM(ureg, rMid, ureg_src(rVtx));
                ureg_ADD(ureg, rMid, ureg_src(rHit), ureg_negate(ureg_src(rMid)));
            } else {
                ureg_ADD(ureg, rMid, ureg_src(rHit), ureg_imm3f(ureg, 0.0f, 0.0f, 1.0f));
            }
            ureg_NRM(ureg, rMid, ureg_src(rMid));
            ureg_DP3(ureg, ureg_saturate(tmp_y), ureg_src(rNrm), ureg_src(rMid));
            ureg_POW(ureg, tmp_y, _Y(tmp), mtlP);

            ureg_MUL(ureg, tmp_x, _W(rAtt), _X(tmp)); /* dp3(normal,hitDir) * att */
            ureg_MUL(ureg, tmp_y, _W(rAtt), _Y(tmp)); /* power factor * att */
            ureg_MAD(ureg, rD, cLColD, _X(tmp), ureg_src(rD)); /* accumulate diffuse */
            ureg_MAD(ureg, rS, cLColS, _Y(tmp), ureg_src(rS)); /* accumulate specular */
        }
        ureg_fixup_label(ureg, label[l-1], ureg_get_instruction_number(ureg));
        ureg_ENDIF(ureg);

        ureg_MAD(ureg, rA, cLColA, _W(rAtt), ureg_src(rA)); /* accumulate ambient */

        /* break if this was the last light */
        ureg_IF(ureg, cLLast, &label[l++]);
        ureg_BRK(ureg);
        ureg_ENDIF(ureg);
        ureg_fixup_label(ureg, label[l-1], ureg_get_instruction_number(ureg));

        ureg_ADD(ureg, rCtr, _W(rCtr), ureg_imm1f(ureg, 8.0f));
        ureg_fixup_label(ureg, label[loop_label], ureg_get_instruction_number(ureg));
        ureg_ENDLOOP(ureg, &label[loop_label]);

        /* Set alpha factors of illumination to 1.0 for the multiplications. */
        ureg_MOV(ureg, ureg_writemask(rD, TGSI_WRITEMASK_W), ureg_imm1f(ureg, 1.0f));
        ureg_MOV(ureg, ureg_writemask(rS, TGSI_WRITEMASK_W), ureg_imm1f(ureg, 1.0f));

        /* Apply to material:
         *
         * oCol[0] = (material.emissive + material.ambient * rs.ambient) +
         *           material.ambient * ambient +
         *           material.diffuse * diffuse +
         * oCol[1] = material.specular * specular;
         */
        if (key->mtl_emissive == 0 && key->mtl_ambient == 0) {
            ureg_MOV(ureg, ureg_writemask(rA, TGSI_WRITEMASK_W), ureg_imm1f(ureg, 1.0f));
            ureg_MAD(ureg, tmp, ureg_src(rA), vs->mtlA, _CONST(19));
        } else {
            ureg_ADD(ureg, ureg_writemask(tmp, TGSI_WRITEMASK_XYZ), ureg_src(rA), _CONST(25));
            ureg_MAD(ureg, ureg_writemask(tmp, TGSI_WRITEMASK_XYZ), vs->mtlA, ureg_src(tmp), vs->mtlE);
            ureg_ADD(ureg, ureg_writemask(tmp, TGSI_WRITEMASK_W  ), vs->mtlA, vs->mtlE);
        }
        ureg_MAD(ureg, rCol[0], ureg_src(rD), vs->mtlD, ureg_src(tmp));
        ureg_MUL(ureg, rCol[1], ureg_src(rS), vs->mtlS);
    } else
    /* COLOR */
    if (key->darkness) {
        if (key->mtl_emissive == 0 && key->mtl_ambient == 0) {
            ureg_MAD(ureg, rCol[0], vs->mtlD, ureg_imm4f(ureg, 0.0f, 0.0f, 0.0f, 1.0f), _CONST(19));
        } else {
            ureg_MAD(ureg, ureg_writemask(rCol[0], TGSI_WRITEMASK_XYZ), vs->mtlA, _CONST(25), vs->mtlE);
            ureg_ADD(ureg, ureg_writemask(tmp,     TGSI_WRITEMASK_W), vs->mtlA, vs->mtlE);
            ureg_ADD(ureg, ureg_writemask(rCol[0], TGSI_WRITEMASK_W), vs->mtlD, _W(tmp));
        }
        ureg_MUL(ureg, rCol[1], ureg_imm4f(ureg, 0.0f, 0.0f, 0.0f, 1.0f), vs->mtlS);
    } else {
        ureg_MOV(ureg, rCol[0], vs->aCol[0]);
        ureg_MOV(ureg, rCol[1], vs->aCol[1]);
    }
    /* clamp_vertex_color: done by hardware */

    /* === Process fog.
     *
     * exp(x) = ex2(log2(e) * x)
     */
    if (key->fog_mode) {
        /* Fog doesn't affect alpha, TODO: combine with light code output */
        ureg_MOV(ureg, ureg_writemask(oCol[0], TGSI_WRITEMASK_W), _W(rCol[0]));
        ureg_MOV(ureg, ureg_writemask(oCol[1], TGSI_WRITEMASK_W), _W(rCol[1]));

        if (key->fog_range) {
            ureg_DP3(ureg, tmp_x, ureg_src(rVtx), ureg_src(rVtx));
            ureg_RSQ(ureg, tmp_z, _X(tmp));
            ureg_MUL(ureg, tmp_z, _Z(tmp), _X(tmp));
        } else {
            ureg_MOV(ureg, tmp_z, ureg_abs(_Z(rVtx)));
        }
        if (key->fog_mode == D3DFOG_EXP) {
            ureg_MUL(ureg, tmp_x, _Z(tmp), _ZZZZ(_CONST(28)));
            ureg_MUL(ureg, tmp_x, _X(tmp), ureg_imm1f(ureg, -1.442695f));
            ureg_EX2(ureg, tmp_x, _X(tmp));
        } else
        if (key->fog_mode == D3DFOG_EXP2) {
            ureg_MUL(ureg, tmp_x, _Z(tmp), _ZZZZ(_CONST(28)));
            ureg_MUL(ureg, tmp_x, _X(tmp), _X(tmp));
            ureg_MUL(ureg, tmp_x, _X(tmp), ureg_imm1f(ureg, -1.442695f));
            ureg_EX2(ureg, tmp_x, _X(tmp));
        } else
        if (key->fog_mode == D3DFOG_LINEAR) {
            ureg_SUB(ureg, tmp_x, _XXXX(_CONST(28)), _Z(tmp));
            ureg_MUL(ureg, ureg_saturate(tmp_x), _X(tmp), _YYYY(_CONST(28)));
        }
        ureg_MOV(ureg, oFog, _X(tmp));
        ureg_LRP(ureg, ureg_writemask(oCol[0], TGSI_WRITEMASK_XYZ), _X(tmp), ureg_src(rCol[0]), _CONST(29));
        ureg_LRP(ureg, ureg_writemask(oCol[1], TGSI_WRITEMASK_XYZ), _X(tmp), ureg_src(rCol[1]), _CONST(29));
    }

    ureg_END(ureg);
    nine_ureg_tgsi_dump(ureg);
    return ureg_create_shader_and_destroy(ureg, device->pipe);
}

/* PS FF constants layout:
 *
 * CONST[ 0.. 7]      stage[i].D3DTSS_CONSTANT
 * CONST[ 8..15].x___ stage[i].D3DTSS_BUMPENVMAT00
 * CONST[ 8..15]._y__ stage[i].D3DTSS_BUMPENVMAT01
 * CONST[ 8..15].__z_ stage[i].D3DTSS_BUMPENVMAT10
 * CONST[ 8..15].___w stage[i].D3DTSS_BUMPENVMAT11
 * CONST[16..19].x_z_ stage[i].D3DTSS_BUMPENVLSCALE
 * CONST[17..19]._y_w stage[i].D3DTSS_BUMPENVLOFFSET
 *
 * CONST[20] D3DRS_TEXTUREFACTOR
 * CONST[21] D3DRS_FOGCOLOR
 * CONST[22].x___ RS.FogEnd
 * CONST[22]._y__ 1.0f / (RS.FogEnd - RS.FogStart)
 * CONST[22].__z_ RS.FogDensity
 */
struct ps_build_ctx
{
    struct ureg_program *ureg;

    struct ureg_src vC[2]; /* DIFFUSE, SPECULAR */
    struct ureg_src vT[8]; /* TEXCOORD[i] */
    struct ureg_dst r[6];  /* TEMPs */
    struct ureg_dst rCur; /* D3DTA_CURRENT */
    struct ureg_dst rMod;
    struct ureg_src rCurSrc;
    struct ureg_dst rTmp; /* D3DTA_TEMP */
    struct ureg_src rTmpSrc;
    struct ureg_dst rTex;
    struct ureg_src rTexSrc;
    struct ureg_src cBEM[8];
    struct ureg_src s[8];

    struct {
        unsigned index;
        unsigned index_pre_mod;
        unsigned num_regs;
    } stage;
};

static struct ureg_src
ps_get_ts_arg(struct ps_build_ctx *ps, unsigned ta)
{
    struct ureg_src reg;

    switch (ta & D3DTA_SELECTMASK) {
    case D3DTA_CONSTANT:
        reg = ureg_DECL_constant(ps->ureg, ps->stage.index);
        break;
    case D3DTA_CURRENT:
        reg = (ps->stage.index == ps->stage.index_pre_mod) ? ureg_src(ps->rMod) : ps->rCurSrc;
        break;
    case D3DTA_DIFFUSE:
        reg = ureg_DECL_fs_input(ps->ureg, TGSI_SEMANTIC_COLOR, 0, TGSI_INTERPOLATE_PERSPECTIVE);
        break;
    case D3DTA_SPECULAR:
        reg = ureg_DECL_fs_input(ps->ureg, TGSI_SEMANTIC_COLOR, 1, TGSI_INTERPOLATE_PERSPECTIVE);
        break;
    case D3DTA_TEMP:
        reg = ps->rTmpSrc;
        break;
    case D3DTA_TEXTURE:
        reg = ps->rTexSrc;
        break;
    case D3DTA_TFACTOR:
        reg = ureg_DECL_constant(ps->ureg, 20);
        break;
    default:
        assert(0);
        reg = ureg_src_undef();
        break;
    }
    if (ta & D3DTA_COMPLEMENT) {
        struct ureg_dst dst = ps->r[ps->stage.num_regs++];
        ureg_SUB(ps->ureg, dst, ureg_imm1f(ps->ureg, 1.0f), reg);
        reg = ureg_src(dst);
    }
    if (ta & D3DTA_ALPHAREPLICATE)
        reg = _WWWW(reg);
    return reg;
}

static struct ureg_dst
ps_get_ts_dst(struct ps_build_ctx *ps, unsigned ta)
{
    assert(!(ta & (D3DTA_COMPLEMENT | D3DTA_ALPHAREPLICATE)));

    switch (ta & D3DTA_SELECTMASK) {
    case D3DTA_CURRENT:
        return ps->rCur;
    case D3DTA_TEMP:
        return ps->rTmp;
    default:
        assert(0);
        return ureg_dst_undef();
    }
}

static uint8_t ps_d3dtop_args_mask(D3DTEXTUREOP top)
{
    switch (top) {
    case D3DTOP_DISABLE:
        return 0x0;
    case D3DTOP_SELECTARG1:
    case D3DTOP_PREMODULATE:
        return 0x2;
    case D3DTOP_SELECTARG2:
        return 0x4;
    case D3DTOP_MULTIPLYADD:
    case D3DTOP_LERP:
        return 0x7;
    default:
        return 0x6;
    }
}

static void
ps_do_ts_op(struct ps_build_ctx *ps, unsigned top, struct ureg_dst dst, struct ureg_src *arg)
{
    struct ureg_program *ureg = ps->ureg;
    struct ureg_dst tmp = ps->r[ps->stage.num_regs];
    struct ureg_dst tmp_x = ureg_writemask(tmp, TGSI_WRITEMASK_X);

    tmp.WriteMask = dst.WriteMask;

    if (top != D3DTOP_SELECTARG1 && top != D3DTOP_SELECTARG2 &&
        top != D3DTOP_MODULATE && top != D3DTOP_PREMODULATE &&
        top != D3DTOP_BLENDDIFFUSEALPHA && top != D3DTOP_BLENDTEXTUREALPHA &&
        top != D3DTOP_BLENDFACTORALPHA && top != D3DTOP_BLENDCURRENTALPHA &&
        top != D3DTOP_BUMPENVMAP && top != D3DTOP_BUMPENVMAPLUMINANCE &&
        top != D3DTOP_LERP)
        dst = ureg_saturate(dst);

    switch (top) {
    case D3DTOP_SELECTARG1:
        ureg_MOV(ureg, dst, arg[1]);
        break;
    case D3DTOP_SELECTARG2:
        ureg_MOV(ureg, dst, arg[2]);
        break;
    case D3DTOP_MODULATE:
        ureg_MUL(ureg, dst, arg[1], arg[2]);
        break;
    case D3DTOP_MODULATE2X:
        ureg_MUL(ureg, tmp, arg[1], arg[2]);
        ureg_ADD(ureg, dst, ureg_src(tmp), ureg_src(tmp));
        break;
    case D3DTOP_MODULATE4X:
        ureg_MUL(ureg, tmp, arg[1], arg[2]);
        ureg_MUL(ureg, dst, ureg_src(tmp), ureg_imm1f(ureg, 4.0f));
        break;
    case D3DTOP_ADD:
        ureg_ADD(ureg, dst, arg[1], arg[2]);
        break;
    case D3DTOP_ADDSIGNED:
        ureg_ADD(ureg, tmp, arg[1], arg[2]);
        ureg_SUB(ureg, dst, ureg_src(tmp), ureg_imm1f(ureg, 0.5f));
        break;
    case D3DTOP_ADDSIGNED2X:
        ureg_ADD(ureg, tmp, arg[1], arg[2]);
        ureg_MAD(ureg, dst, ureg_src(tmp), ureg_imm1f(ureg, 2.0f), ureg_imm1f(ureg, -1.0f));
        break;
    case D3DTOP_SUBTRACT:
        ureg_SUB(ureg, dst, arg[1], arg[2]);
        break;
    case D3DTOP_ADDSMOOTH:
        ureg_SUB(ureg, tmp, ureg_imm1f(ureg, 1.0f), arg[1]);
        ureg_MAD(ureg, dst, ureg_src(tmp), arg[2], arg[1]);
        break;
    case D3DTOP_BLENDDIFFUSEALPHA:
        ureg_LRP(ureg, dst, _WWWW(ps->vC[0]), arg[1], arg[2]);
        break;
    case D3DTOP_BLENDTEXTUREALPHA:
        /* XXX: alpha taken from previous stage, texture or result ? */
        ureg_LRP(ureg, dst, _W(ps->rTex), arg[1], arg[2]);
        break;
    case D3DTOP_BLENDFACTORALPHA:
        ureg_LRP(ureg, dst, _WWWW(_CONST(20)), arg[1], arg[2]);
        break;
    case D3DTOP_BLENDTEXTUREALPHAPM:
        ureg_SUB(ureg, tmp_x, ureg_imm1f(ureg, 1.0f), _W(ps->rTex));
        ureg_MAD(ureg, dst, arg[2], _X(tmp), arg[1]);
        break;
    case D3DTOP_BLENDCURRENTALPHA:
        ureg_LRP(ureg, dst, _WWWW(ps->rCurSrc), arg[1], arg[2]);
        break;
    case D3DTOP_PREMODULATE:
        ureg_MOV(ureg, dst, arg[1]);
        ps->stage.index_pre_mod = ps->stage.index + 1;
        break;
    case D3DTOP_MODULATEALPHA_ADDCOLOR:
        ureg_MAD(ureg, dst, _WWWW(arg[1]), arg[2], arg[1]);
        break;
    case D3DTOP_MODULATECOLOR_ADDALPHA:
        ureg_MAD(ureg, dst, arg[1], arg[2], _WWWW(arg[1]));
        break;
    case D3DTOP_MODULATEINVALPHA_ADDCOLOR:
        ureg_SUB(ureg, tmp_x, ureg_imm1f(ureg, 1.0f), _WWWW(arg[1]));
        ureg_MAD(ureg, dst, _X(tmp), arg[2], arg[1]);
        break;
    case D3DTOP_MODULATEINVCOLOR_ADDALPHA:
        ureg_SUB(ureg, tmp, ureg_imm1f(ureg, 1.0f), arg[1]);
        ureg_MAD(ureg, dst, ureg_src(tmp), arg[2], _WWWW(arg[1]));
        break;
    case D3DTOP_BUMPENVMAP:
        break;
    case D3DTOP_BUMPENVMAPLUMINANCE:
        break;
    case D3DTOP_DOTPRODUCT3:
        ureg_DP3(ureg, dst, arg[1], arg[2]);
        break;
    case D3DTOP_MULTIPLYADD:
        ureg_MAD(ureg, dst, arg[2], arg[0], arg[1]);
        break;
    case D3DTOP_LERP:
        ureg_LRP(ureg, dst, arg[1], arg[2], arg[0]);
        break;
    case D3DTOP_DISABLE:
        /* no-op ? */
        break;
    default:
        assert(!"invalid D3DTOP");
        break;
    }
}

static void *
nine_ff_build_ps(struct NineDevice9 *device, struct nine_ff_ps_key *key)
{
    struct ps_build_ctx ps;
    struct ureg_program *ureg = ureg_create(TGSI_PROCESSOR_FRAGMENT);
    struct ureg_dst oCol;
    unsigned i, s;

    memset(&ps, 0, sizeof(ps));
    ps.ureg = ureg;
    ps.stage.index_pre_mod = -1;

    ps.vC[0] = ureg_DECL_fs_input(ureg, TGSI_SEMANTIC_COLOR, 0, TGSI_INTERPOLATE_PERSPECTIVE);

    /* Declare all TEMPs we might need, serious drivers have a register allocator. */
    for (i = 0; i < Elements(ps.r); ++i)
        ps.r[i] = ureg_DECL_local_temporary(ureg);
    ps.rCur = ps.r[0];
    ps.rTmp = ps.r[1];
    ps.rTex = ps.r[2];
    ps.rCurSrc = ureg_src(ps.rCur);
    ps.rTmpSrc = ureg_src(ps.rTmp);
    ps.rTexSrc = ureg_src(ps.rTex);

    for (s = 0; s < 8; ++s) {
        ps.s[s] = ureg_src_undef();

        if (key->ts[s].colorop != D3DTOP_DISABLE) {
            if (key->ts[s].colorarg0 == D3DTA_SPECULAR ||
                key->ts[s].colorarg1 == D3DTA_SPECULAR ||
                key->ts[s].colorarg2 == D3DTA_SPECULAR)
                ps.vC[1] = ureg_DECL_fs_input(ureg, TGSI_SEMANTIC_COLOR, 1, TGSI_INTERPOLATE_PERSPECTIVE);

            if (key->ts[s].colorarg0 == D3DTA_TEXTURE ||
                key->ts[s].colorarg1 == D3DTA_TEXTURE ||
                key->ts[s].colorarg2 == D3DTA_TEXTURE) {
                ps.s[s] = ureg_DECL_sampler(ureg, s);
                ps.vT[s] = ureg_DECL_fs_input(ureg, TGSI_SEMANTIC_TEXCOORD, s, TGSI_INTERPOLATE_PERSPECTIVE);
            }
            if (s && (key->ts[s - 1].colorop == D3DTOP_PREMODULATE ||
                      key->ts[s - 1].alphaop == D3DTOP_PREMODULATE))
                ps.s[s] = ureg_DECL_sampler(ureg, s);
        }

        if (key->ts[s].alphaop != D3DTOP_DISABLE) {
            if (key->ts[s].alphaarg0 == D3DTA_SPECULAR ||
                key->ts[s].alphaarg1 == D3DTA_SPECULAR ||
                key->ts[s].alphaarg2 == D3DTA_SPECULAR)
                ps.vC[1] = ureg_DECL_fs_input(ureg, TGSI_SEMANTIC_COLOR, 1, TGSI_INTERPOLATE_PERSPECTIVE);

            if (key->ts[s].alphaarg0 == D3DTA_TEXTURE ||
                key->ts[s].alphaarg1 == D3DTA_TEXTURE ||
                key->ts[s].alphaarg2 == D3DTA_TEXTURE) {
                ps.s[s] = ureg_DECL_sampler(ureg, s);
                ps.vT[s] = ureg_DECL_fs_input(ureg, TGSI_SEMANTIC_TEXCOORD, s, TGSI_INTERPOLATE_PERSPECTIVE);
            }
        }
    }
    if (key->specular)
        ps.vC[1] = ureg_DECL_fs_input(ureg, TGSI_SEMANTIC_COLOR, 1, TGSI_INTERPOLATE_PERSPECTIVE);

    oCol = ureg_DECL_output(ureg, TGSI_SEMANTIC_COLOR, 0);

    if (key->ts[0].colorop == D3DTOP_DISABLE &&
        key->ts[0].alphaop == D3DTOP_DISABLE)
        ureg_MOV(ureg, ps.rCur, ps.vC[0]);
    /* Or is it undefined then ? */

    /* Run stages.
     */
    for (s = 0; s < 8; ++s) {
        unsigned colorarg[3];
        unsigned alphaarg[3];
        const uint8_t used_c = ps_d3dtop_args_mask(key->ts[s].colorop);
        const uint8_t used_a = ps_d3dtop_args_mask(key->ts[s].alphaop);
        struct ureg_dst dst;
        struct ureg_src arg[3];

        if (key->ts[s].colorop == D3DTOP_DISABLE &&
            key->ts[s].alphaop == D3DTOP_DISABLE)
            continue;
        ps.stage.index = s;
        ps.stage.num_regs = 3;

        DBG("STAGE[%u]: colorop=%s alphaop=%s\n", s,
            nine_D3DTOP_to_str(key->ts[s].colorop),
            nine_D3DTOP_to_str(key->ts[s].alphaop));

        if (!ureg_src_is_undef(ps.s[s])) {
            unsigned target;
            switch (key->ts[s].textarget) {
            case 0: target = TGSI_TEXTURE_1D; break;
            case 1: target = TGSI_TEXTURE_2D; break;
            case 2: target = TGSI_TEXTURE_3D; break;
            case 3: target = TGSI_TEXTURE_CUBE; break;
            /* this is a 2 bit bitfield, do I really need a default case ? */
            }

            /* sample the texture */
            if (key->ts[s].colorop == D3DTOP_BUMPENVMAP ||
                key->ts[s].colorop == D3DTOP_BUMPENVMAPLUMINANCE) {
            }
            if (key->ts[s].projected)
                ureg_TXP(ureg, ps.rTex, target, ps.vT[s], ps.s[s]);
            else
                ureg_TEX(ureg, ps.rTex, target, ps.vT[s], ps.s[s]);
        }

        if (s == 0 &&
            (key->ts[0].resultarg != 0 /* not current */ ||
             key->ts[0].colorop == D3DTOP_DISABLE ||
             key->ts[0].alphaop == D3DTOP_DISABLE ||
             key->ts[0].colorarg0 == D3DTA_CURRENT ||
             key->ts[0].colorarg1 == D3DTA_CURRENT ||
             key->ts[0].colorarg2 == D3DTA_CURRENT ||
             key->ts[0].alphaarg0 == D3DTA_CURRENT ||
             key->ts[0].alphaarg1 == D3DTA_CURRENT ||
             key->ts[0].alphaarg2 == D3DTA_CURRENT)
           ) {
            /* Initialize D3DTA_CURRENT.
             * (Yes we can do this before the loop but not until
             *  NVE4 has an instruction scheduling pass.)
             */
            ureg_MOV(ureg, ps.rCur, ps.vC[0]);
        }

        dst = ps_get_ts_dst(&ps, key->ts[s].resultarg ? D3DTA_TEMP : D3DTA_CURRENT);

        if (ps.stage.index_pre_mod == ps.stage.index) {
            ps.rMod = ps.r[ps.stage.num_regs++];
            ureg_MUL(ureg, ps.rMod, ps.rCurSrc, ps.rTexSrc);
        }

        colorarg[0] = (key->ts[s].colorarg0 | ((key->colorarg_b4[0] >> s) << 4) | ((key->colorarg_b5[0] >> s) << 5)) & 0x3f;
        colorarg[1] = (key->ts[s].colorarg1 | ((key->colorarg_b4[1] >> s) << 4) | ((key->colorarg_b5[1] >> s) << 5)) & 0x3f;
        colorarg[2] = (key->ts[s].colorarg2 | ((key->colorarg_b4[2] >> s) << 4) | ((key->colorarg_b5[2] >> s) << 5)) & 0x3f;
        alphaarg[0] = (key->ts[s].alphaarg0 | ((key->alphaarg_b4[0] >> s) << 4)) & 0x1f;
        alphaarg[1] = (key->ts[s].alphaarg1 | ((key->alphaarg_b4[1] >> s) << 4)) & 0x1f;
        alphaarg[2] = (key->ts[s].alphaarg2 | ((key->alphaarg_b4[2] >> s) << 4)) & 0x1f;

        if (key->ts[s].colorop != key->ts[s].alphaop ||
            colorarg[0] != alphaarg[0] ||
            colorarg[1] != alphaarg[1] ||
            colorarg[2] != alphaarg[2])
            dst.WriteMask = TGSI_WRITEMASK_XYZ;

        if (used_c & 0x1) arg[0] = ps_get_ts_arg(&ps, colorarg[0]);
        if (used_c & 0x2) arg[1] = ps_get_ts_arg(&ps, colorarg[1]);
        if (used_c & 0x4) arg[2] = ps_get_ts_arg(&ps, colorarg[2]);
        ps_do_ts_op(&ps, key->ts[s].colorop, dst, arg);

        if (dst.WriteMask != TGSI_WRITEMASK_XYZW) {
            dst.WriteMask = TGSI_WRITEMASK_W;

            if (used_a & 0x1) arg[0] = ps_get_ts_arg(&ps, alphaarg[0]);
            if (used_a & 0x2) arg[1] = ps_get_ts_arg(&ps, alphaarg[1]);
            if (used_a & 0x4) arg[2] = ps_get_ts_arg(&ps, alphaarg[2]);
            ps_do_ts_op(&ps, key->ts[s].alphaop, dst, arg);
        }
    }

    if (key->specular)
        ureg_ADD(ureg, ps.rCur, ps.rCurSrc, ps.vC[1]);

    /* Fog.
     */
    if (key->fog_mode) {
        struct ureg_src vPos = ureg_DECL_fs_input(ureg, TGSI_SEMANTIC_POSITION, 0, TGSI_INTERPOLATE_LINEAR);
        struct ureg_dst rFog = ureg_writemask(ps.rTmp, TGSI_WRITEMASK_X);
        if (key->fog_mode == D3DFOG_EXP) {
            ureg_MUL(ureg, rFog, _ZZZZ(vPos), _ZZZZ(_CONST(22)));
            ureg_MUL(ureg, rFog, _X(rFog), ureg_imm1f(ureg, -1.442695f));
            ureg_EX2(ureg, rFog, _X(rFog));
        } else
        if (key->fog_mode == D3DFOG_EXP2) {
            ureg_MUL(ureg, rFog, _ZZZZ(vPos), _ZZZZ(_CONST(22)));
            ureg_MUL(ureg, rFog, _X(rFog), _X(rFog));
            ureg_MUL(ureg, rFog, _X(rFog), ureg_imm1f(ureg, -1.442695f));
            ureg_EX2(ureg, rFog, _X(rFog));
        } else
        if (key->fog_mode == D3DFOG_LINEAR) {
            ureg_SUB(ureg, rFog, _XXXX(_CONST(22)), _ZZZZ(vPos));
            ureg_MUL(ureg, ureg_saturate(rFog), _X(rFog), _YYYY(_CONST(22)));
        }
        ureg_LRP(ureg, ureg_writemask(oCol, TGSI_WRITEMASK_XYZ), _X(rFog), ps.rCurSrc, _CONST(21));
        ureg_MOV(ureg, ureg_writemask(oCol, TGSI_WRITEMASK_W), ps.rCurSrc);
    } else
    if (key->fog) {
        struct ureg_src vFog = ureg_DECL_fs_input(ureg, TGSI_SEMANTIC_FOG, 0, TGSI_INTERPOLATE_PERSPECTIVE);
        ureg_LRP(ureg, ureg_writemask(oCol, TGSI_WRITEMASK_XYZ), _XXXX(vFog), ps.rCurSrc, _CONST(21));
        ureg_MOV(ureg, ureg_writemask(oCol, TGSI_WRITEMASK_W), ps.rCurSrc);
    } else {
        ureg_MOV(ureg, oCol, ps.rCurSrc);
    }

    ureg_END(ureg);
    nine_ureg_tgsi_dump(ureg);
    return ureg_create_shader_and_destroy(ureg, device->pipe);
}

static struct NineVertexShader9 *
nine_ff_get_vs(struct NineDevice9 *device)
{
    const struct nine_state *state = &device->state;
    struct NineVertexShader9 *vs;
    enum pipe_error err;
    struct vs_build_ctx bld;
    struct nine_ff_vs_key key;
    unsigned s;

    memset(&key, 0, sizeof(key));
    memset(&bld, 0, sizeof(bld));

    bld.key = &key;

    /* FIXME: this shouldn't be NULL, but it is on init */
    if (state->vdecl) {
        if (state->vdecl->usage_map[NINE_DECLUSAGE_POSITIONT] != 0xff)
            key.position_t = 1;
        if (state->vdecl->usage_map[NINE_DECLUSAGE_COLOR(0)] == 0xff)
            key.color0in_one = 1;
        if (state->vdecl->usage_map[NINE_DECLUSAGE_COLOR(1)] == 0xff)
            key.color1in_one = 1;
    }

    if (state->vdecl &&
        state->vdecl->usage_map[NINE_DECLUSAGE_PSIZE] != 0xff)
        key.vertexpointsize = 1;
    if (!key.vertexpointsize)
        key.pointscale = !!state->rs[D3DRS_POINTSCALEENABLE];

    key.lighting = !!state->rs[D3DRS_LIGHTING] &&  state->ff.num_lights_active;
    key.darkness = !!state->rs[D3DRS_LIGHTING] && !state->ff.num_lights_active;
    if (key.position_t) {
        key.darkness = 0; /* |= key.lighting; */ /* XXX ? */
        key.lighting = 0;
    }
    if ((key.lighting | key.darkness) && state->rs[D3DRS_COLORVERTEX]) {
        key.mtl_diffuse = state->rs[D3DRS_DIFFUSEMATERIALSOURCE];
        key.mtl_ambient = state->rs[D3DRS_AMBIENTMATERIALSOURCE];
        key.mtl_specular = state->rs[D3DRS_SPECULARMATERIALSOURCE];
        key.mtl_emissive = state->rs[D3DRS_EMISSIVEMATERIALSOURCE];
    }
    if (!key.position_t) {
        key.fog_mode = state->rs[D3DRS_FOGENABLE] ? state->rs[D3DRS_FOGVERTEXMODE] : 0;
        if (key.fog_mode)
            key.fog_range = !!state->rs[D3DRS_RANGEFOGENABLE];
    }

    if (state->rs[D3DRS_VERTEXBLEND] != D3DVBF_DISABLE) {
        key.vertexblend_indexed = !!state->rs[D3DRS_INDEXEDVERTEXBLENDENABLE];

        switch (state->rs[D3DRS_VERTEXBLEND]) {
        case D3DVBF_0WEIGHTS: key.vertexblend = key.vertexblend_indexed; break;
        case D3DVBF_1WEIGHTS: key.vertexblend = 2; break;
        case D3DVBF_2WEIGHTS: key.vertexblend = 3; break;
        case D3DVBF_3WEIGHTS: key.vertexblend = 4; break;
        case D3DVBF_TWEENING: key.vertextween = 1; break;
        default:
            assert(!"invalid D3DVBF");
            break;
        }
    }

    for (s = 0; s < 8; ++s) {
        if (state->ff.tex_stage[s][D3DTSS_COLOROP] == D3DTOP_DISABLE &&
            state->ff.tex_stage[s][D3DTSS_ALPHAOP] == D3DTOP_DISABLE)
            break; /* XXX continue ? */
        key.tc_idx |= ((state->ff.tex_stage[s][D3DTSS_TEXCOORDINDEX] >>  0) & 7) << (s * 3);
        key.tc_gen |= ((state->ff.tex_stage[s][D3DTSS_TEXCOORDINDEX] >> 16) + 1) << (s * 3);
    }

    vs = util_hash_table_get(device->ff.ht_vs, &key);
    if (vs)
        return vs;
    NineVertexShader9_new(device, &vs, NULL, nine_ff_build_vs(device, &bld));

    nine_ff_prune_vs(device);
    if (vs) {
        unsigned n;

        memcpy(&vs->ff_key, &key, sizeof(vs->ff_key));

        err = util_hash_table_set(device->ff.ht_vs, &vs->ff_key, vs);
        assert(err == PIPE_OK);
        device->ff.num_vs++;

        vs->num_inputs = bld.num_inputs;
        for (n = 0; n < bld.num_inputs; ++n)
            vs->input_map[n].ndecl = bld.input[n];

        vs->position_t = key.position_t;
    }
    return vs;
}

static struct NinePixelShader9 *
nine_ff_get_ps(struct NineDevice9 *device)
{
    struct nine_state *state = &device->state;
    struct NinePixelShader9 *ps;
    enum pipe_error err;
    struct nine_ff_ps_key key;
    unsigned s;

    memset(&key, 0, sizeof(key));
    for (s = 0; s < 8; ++s) {
        key.ts[s].colorop = state->ff.tex_stage[s][D3DTSS_COLOROP];
        key.ts[s].alphaop = state->ff.tex_stage[s][D3DTSS_ALPHAOP];
        /* MSDN says D3DTOP_DISABLE disables this and all subsequent stages. */
        if (key.ts[s].colorop == D3DTOP_DISABLE) {
            /* And that alphaop cannot be disabled when colorop isn't. */
            key.ts[s].alphaop = D3DTOP_DISABLE;
            break;
        }
        if (!state->texture[s] &&
            state->ff.tex_stage[s][D3DTSS_COLORARG1] == D3DTA_TEXTURE) {
            /* This should also disable the stage. */
            key.ts[s].colorop = key.ts[s].alphaop = D3DTOP_DISABLE;
            break;
        }
        if (key.ts[s].colorop != D3DTOP_DISABLE) {
            uint8_t used_c = ps_d3dtop_args_mask(key.ts[s].colorop);
            if (used_c & 0x1) key.ts[s].colorarg0 = state->ff.tex_stage[s][D3DTSS_COLORARG0];
            if (used_c & 0x2) key.ts[s].colorarg1 = state->ff.tex_stage[s][D3DTSS_COLORARG1];
            if (used_c & 0x4) key.ts[s].colorarg2 = state->ff.tex_stage[s][D3DTSS_COLORARG2];
            if (used_c & 0x1) key.colorarg_b4[0] |= (state->ff.tex_stage[s][D3DTSS_COLORARG0] >> 4) << s;
            if (used_c & 0x1) key.colorarg_b5[0] |= (state->ff.tex_stage[s][D3DTSS_COLORARG0] >> 5) << s;
            if (used_c & 0x2) key.colorarg_b4[1] |= (state->ff.tex_stage[s][D3DTSS_COLORARG1] >> 4) << s;
            if (used_c & 0x2) key.colorarg_b5[1] |= (state->ff.tex_stage[s][D3DTSS_COLORARG1] >> 5) << s;
            if (used_c & 0x4) key.colorarg_b4[2] |= (state->ff.tex_stage[s][D3DTSS_COLORARG2] >> 4) << s;
            if (used_c & 0x4) key.colorarg_b5[2] |= (state->ff.tex_stage[s][D3DTSS_COLORARG2] >> 5) << s;
        }
        if (key.ts[s].alphaop != D3DTOP_DISABLE) {
            uint8_t used_a = ps_d3dtop_args_mask(key.ts[s].alphaop);
            if (used_a & 0x1) key.ts[s].alphaarg0 = state->ff.tex_stage[s][D3DTSS_ALPHAARG0];
            if (used_a & 0x2) key.ts[s].alphaarg1 = state->ff.tex_stage[s][D3DTSS_ALPHAARG1];
            if (used_a & 0x4) key.ts[s].alphaarg2 = state->ff.tex_stage[s][D3DTSS_ALPHAARG2];
            if (used_a & 0x1) key.alphaarg_b4[0] |= (state->ff.tex_stage[s][D3DTSS_ALPHAARG0] >> 4) << s;
            if (used_a & 0x2) key.alphaarg_b4[1] |= (state->ff.tex_stage[s][D3DTSS_ALPHAARG1] >> 4) << s;
            if (used_a & 0x4) key.alphaarg_b4[2] |= (state->ff.tex_stage[s][D3DTSS_ALPHAARG2] >> 4) << s;
        }
        key.ts[s].resultarg = state->ff.tex_stage[s][D3DTSS_RESULTARG] == D3DTA_TEMP;

        if (state->texture[s]) {
            switch (state->texture[s]->base.type) {
            case D3DRTYPE_TEXTURE:       key.ts[s].textarget = 1; break;
            case D3DRTYPE_VOLUMETEXTURE: key.ts[s].textarget = 2; break;
            case D3DRTYPE_CUBETEXTURE:   key.ts[s].textarget = 3; break;
            default:
                assert(!"unexpected texture type");
                break;
            }
        } else {
            key.ts[s].textarget = 1;
        }
    }
    for (; s < 8; ++s)
        key.ts[s].colorop = key.ts[s].alphaop = D3DTOP_DISABLE;
    if (state->rs[D3DRS_FOGENABLE])
        key.fog_mode = state->rs[D3DRS_FOGTABLEMODE];

    ps = util_hash_table_get(device->ff.ht_ps, &key);
    if (ps)
        return ps;
    NinePixelShader9_new(device, &ps, NULL, nine_ff_build_ps(device, &key));

    nine_ff_prune_ps(device);
    if (ps) {
        memcpy(&ps->ff_key, &key, sizeof(ps->ff_key));

        err = util_hash_table_set(device->ff.ht_ps, &ps->ff_key, ps);
        assert(err == PIPE_OK);
        device->ff.num_ps++;
    }
    return ps;
}

#define GET_D3DTS(n) nine_state_access_transform(state, D3DTS_##n, FALSE)
#define IS_D3DTS_DIRTY(s,n) ((s)->ff.changed.transform[(D3DTS_##n) / 32] & (1 << ((D3DTS_##n) % 32)))
static void
nine_ff_upload_vs_transforms(struct NineDevice9 *device)
{
    struct pipe_context *pipe = device->pipe;
    struct nine_state *state = &device->state;
    D3DMATRIX M[8];
    struct pipe_box box;
    const unsigned usage = PIPE_TRANSFER_WRITE | PIPE_TRANSFER_DISCARD_RANGE;
    unsigned i;

    /* TODO: make this nicer, and only upload the ones we need */

    if (IS_D3DTS_DIRTY(state, WORLD) ||
        IS_D3DTS_DIRTY(state, VIEW) ||
        IS_D3DTS_DIRTY(state, PROJECTION)) {
        /* upload MVP, MV matrices */
        nine_d3d_matrix_matrix_mul(&M[1], GET_D3DTS(WORLD), GET_D3DTS(VIEW));
        nine_d3d_matrix_matrix_mul(&M[0], &M[1], GET_D3DTS(PROJECTION));
        u_box_1d(0, 2 * sizeof(M[0]), &box);
        pipe->transfer_inline_write(pipe, device->constbuf_vs, 0, usage,
                                    &box,
                                    &M[0], 0, 0);
        /* upload normal matrix == transpose(inverse(MV)) */
        nine_d3d_matrix_inverse_3x3(&M[0], &M[1]);
        nine_d3d_matrix_transpose(&M[1], &M[0]);
        box.width = sizeof(M[0]) - sizeof(M[0].m[3]);
        box.x = 4 * sizeof(M[0]);
        pipe->transfer_inline_write(pipe, device->constbuf_vs, 0, usage,
                                    &box,
                                    &M[1].m[0][0], 0, 0);

        /* upload VP matrix */
        nine_d3d_matrix_matrix_mul(&M[0], GET_D3DTS(VIEW), GET_D3DTS(PROJECTION));
        box.x = 2 * sizeof(M[0]);
        box.width = sizeof(M[0]);
        pipe->transfer_inline_write(pipe, device->constbuf_vs, 0, usage,
                                    &box,
                                    &M[0], 0, 0);
        /* upload V matrix */
        box.x = 3 * sizeof(M[0]);
        pipe->transfer_inline_write(pipe, device->constbuf_vs, 0, usage,
                                    &box,
                                    GET_D3DTS(VIEW), 0, 0);

        /* upload W matrix */
        box.x = (224 / 4) * sizeof(M[0]);
        pipe->transfer_inline_write(pipe, device->constbuf_vs, 0, usage,
                                    &box,
                                    GET_D3DTS(WORLD), 0, 0);
    }

    if (state->rs[D3DRS_VERTEXBLEND] != D3DVBF_DISABLE) {
        /* upload other world matrices */
        box.x = (228 / 4) * sizeof(M[0]);
        box.width = 7 * sizeof(M[0]);
        for (i = 1; i <= 7; ++i)
            M[i - 1] = *GET_D3DTS(WORLDMATRIX(i));
        pipe->transfer_inline_write(pipe, device->constbuf_vs, 0, usage, &box, &M[0], 0, 0);
    }
}

static void
nine_ff_upload_lights(struct NineDevice9 *device)
{
    struct pipe_context *pipe = device->pipe;
    struct nine_state *state = &device->state;
    float data[8][4];
    union pipe_color_union color;
    unsigned l;
    struct pipe_box box;
    const unsigned usage = PIPE_TRANSFER_WRITE | PIPE_TRANSFER_DISCARD_RANGE;

    u_box_1d(0, sizeof(data), &box);

    if (state->changed.group & NINE_STATE_FF_MATERIAL) {
        const D3DMATERIAL9 *mtl = &state->ff.material;

        memcpy(data[1], &mtl->Diffuse, sizeof(data[1]));
        memcpy(data[2], &mtl->Ambient, sizeof(data[2]));
        memcpy(data[3], &mtl->Specular, sizeof(data[3]));
        memcpy(data[4], &mtl->Emissive, sizeof(data[4]));

        data[5][0] = mtl->Power;
        data[5][1] = 0.0f;
        data[5][2] = 0.0f;
        data[5][3] = 0.0f;

        d3dcolor_to_rgba(&color.f[0], state->rs[D3DRS_AMBIENT]);
        data[0][0] = color.f[0] * mtl->Ambient.r + mtl->Emissive.r;
        data[0][1] = color.f[1] * mtl->Ambient.g + mtl->Emissive.g;
        data[0][2] = color.f[2] * mtl->Ambient.b + mtl->Emissive.b;
        data[0][3] = mtl->Ambient.a + mtl->Emissive.a;

        box.width = 6 * 4 * sizeof(float);
        box.x = 19 * 4 * sizeof(float);
        pipe->transfer_inline_write(pipe, device->constbuf_vs, 0, usage, &box,
                                    data, 0, 0);
    }

    if (!(state->changed.group & NINE_STATE_FF_LIGHTING))
        return;

    /* initialize unused fields */
    data[6][3] = 0.0f;
    data[7][0] = 0.0f;
    data[7][1] = 0.0f;
    data[7][2] = 0.0f;
    for (l = 0; l < state->ff.num_lights_active; ++l) {
        const D3DLIGHT9 *light = &state->ff.light[state->ff.active_light[l]];

        data[0][0] = light->Type;
        data[0][1] = light->Attenuation0;
        data[0][2] = light->Attenuation1;
        data[0][3] = light->Attenuation2;
        memcpy(&data[1], &light->Diffuse, sizeof(data[1]));
        memcpy(&data[2], &light->Specular, sizeof(data[2]));
        memcpy(&data[3], &light->Ambient, sizeof(data[3]));
        nine_d3d_vector4_matrix_mul((D3DVECTOR *)data[4], &light->Position, GET_D3DTS(VIEW));
        nine_d3d_vector3_matrix_mul((D3DVECTOR *)data[5], &light->Direction, GET_D3DTS(VIEW));
        data[4][3] = light->Type == D3DLIGHT_DIRECTIONAL ? 1e9f : light->Range;
        data[5][3] = light->Falloff;
        data[6][0] = cosf(light->Theta * 0.5f);
        data[6][1] = cosf(light->Phi * 0.5f);
        data[6][2] = 1.0f / (data[6][0] - data[6][1]);
        data[7][3] = (l + 1) == state->ff.num_lights_active;

        box.width = 8 * 4 * sizeof(float);
        box.x = (32 + l * 8) * 4 * sizeof(float);
        pipe->transfer_inline_write(pipe, device->constbuf_vs, 0, usage, &box, data, 0, 0);
    }
}

static void
nine_ff_upload_point_and_fog_params(struct NineDevice9 *device)
{
    struct pipe_context *pipe = device->pipe;
    struct nine_state *state = &device->state;
    struct pipe_box box;
    float data[16];
    const unsigned usage = PIPE_TRANSFER_WRITE | PIPE_TRANSFER_DISCARD_RANGE;

    if (!(state->changed.group & NINE_STATE_FF_OTHER))
        return;

    u_box_1d(26 * 4 * sizeof(float), 15 * sizeof(float), &box);

    data[0] = asfloat(state->rs[D3DRS_POINTSIZE_MIN]);
    data[1] = asfloat(state->rs[D3DRS_POINTSIZE_MAX]);
    data[2] = asfloat(state->rs[D3DRS_POINTSIZE]);
    data[3] = asfloat(state->rs[D3DRS_POINTSCALE_A]);
    data[4] = asfloat(state->rs[D3DRS_POINTSCALE_B]);
    data[5] = asfloat(state->rs[D3DRS_POINTSCALE_C]);
    data[6] = 0;
    data[7] = 0;
    data[8] = asfloat(state->rs[D3DRS_FOGEND]);
    data[9] = 1.0f / (asfloat(state->rs[D3DRS_FOGEND]) - asfloat(state->rs[D3DRS_FOGSTART]));
    data[10] = asfloat(state->rs[D3DRS_FOGDENSITY]);
    data[11] = 0.0f;
    d3dcolor_to_rgba(&data[12], state->rs[D3DRS_FOGCOLOR]);

    pipe->transfer_inline_write(pipe, device->constbuf_vs, 0, usage, &box,
                                data, 0, 0);

    DBG("VS Fog: end=%f 1/(end-start)=%f density=%f color=(%f %f %f)\n",
        data[8],data[9],data[10],data[12],data[13],data[14]);
}

static void
nine_ff_upload_tex_matrices(struct NineDevice9 *device)
{
    struct pipe_context *pipe = device->pipe;
    struct nine_state *state = &device->state;
    struct pipe_box box;
    const unsigned usage = PIPE_TRANSFER_WRITE | PIPE_TRANSFER_DISCARD_RANGE;
    unsigned s;

    if (!(state->ff.changed.transform[0] & 0xff0000))
        return;

    u_box_1d(128 * 4 * sizeof(float), 16 * sizeof(float), &box);

    for (s = 0; s < 8; ++s, box.x += 16 * sizeof(float)) {
        if (!IS_D3DTS_DIRTY(state, TEXTURE0 + s))
            continue;
        pipe->transfer_inline_write(pipe, device->constbuf_vs, 0, usage, &box,
                                    nine_state_access_transform(state, D3DTS_TEXTURE0 + s, FALSE),
                                    0, 0);
    }
}

static void
nine_ff_upload_ps_params(struct NineDevice9 *device)
{
    struct pipe_context *pipe = device->pipe;
    struct nine_state *state = &device->state;
    struct pipe_box box;
    unsigned s;
    float data[12][4];
    const unsigned usage = PIPE_TRANSFER_WRITE | PIPE_TRANSFER_DISCARD_RANGE;

    if (!(state->changed.group & (NINE_STATE_FF_PSSTAGES | NINE_STATE_FF_OTHER)))
        return;

    u_box_1d(0, 8 * 4 * sizeof(float), &box);

    for (s = 0; s < 8; ++s)
        d3dcolor_to_rgba(data[s], state->ff.tex_stage[s][D3DTSS_CONSTANT]);
    pipe->transfer_inline_write(pipe, device->constbuf_ps, 0, usage, &box,
                                data, 0, 0);

    for (s = 0; s < 8; ++s) {
        data[s][0] = asfloat(state->ff.tex_stage[s][D3DTSS_BUMPENVMAT00]);
        data[s][1] = asfloat(state->ff.tex_stage[s][D3DTSS_BUMPENVMAT01]);
        data[s][2] = asfloat(state->ff.tex_stage[s][D3DTSS_BUMPENVMAT10]);
        data[s][3] = asfloat(state->ff.tex_stage[s][D3DTSS_BUMPENVMAT11]);
        data[8 + s/2][2*(s%2) + 0] = asfloat(state->ff.tex_stage[s][D3DTSS_BUMPENVLSCALE]);
        data[8 + s/2][2*(s%2) + 1] = asfloat(state->ff.tex_stage[s][D3DTSS_BUMPENVLOFFSET]);
    }
    box.width = 12 * 4 * sizeof(float);
    box.x = 8 * 4 * sizeof(float);
    pipe->transfer_inline_write(pipe, device->constbuf_ps, 0, usage, &box,
                                data, 0, 0);
    
    d3dcolor_to_rgba(data[0], state->rs[D3DRS_TEXTUREFACTOR]);
    d3dcolor_to_rgba(data[1], state->rs[D3DRS_FOGCOLOR]);
    data[2][0] = asfloat(state->rs[D3DRS_FOGEND]);
    data[2][1] = 1.0f / (asfloat(state->rs[D3DRS_FOGEND]) - asfloat(state->rs[D3DRS_FOGSTART]));
    data[2][2] = asfloat(state->rs[D3DRS_FOGDENSITY]);
    data[2][3] = 0.0f;
    box.width = 3 * 4 * sizeof(float);
    box.x = 20 * 4 * sizeof(float);
    pipe->transfer_inline_write(pipe, device->constbuf_ps, 0, usage, &box,
                                data, 0, 0);

    DBG("PS Fog: color=(%f %f %f %f) end=%f 1/(end-start)=%f density=%f\n",
        data[1][0],data[1][1],data[1][2],data[1][3],
        data[2][0],data[2][1],data[2][2]);
}

void
nine_ff_update(struct NineDevice9 *device)
{
    struct nine_state *state = &device->state;

    DBG("vs=%p ps=%p\n", device->state.vs, device->state.ps);

    /* NOTE: the only reference belongs to the hash table */
    if (!device->state.vs)
        device->ff.vs = nine_ff_get_vs(device);
    if (!device->state.ps)
        device->ff.ps = nine_ff_get_ps(device);

    if (!device->state.vs) {
        if (device->state.ff.clobber.vs_const) {
            device->state.ff.clobber.vs_const = FALSE;
            device->state.changed.group |=
                NINE_STATE_FF_VSTRANSF |
                NINE_STATE_FF_MATERIAL |
                NINE_STATE_FF_LIGHTING |
                NINE_STATE_FF_OTHER;
            device->state.ff.changed.transform[0] |= 0xff000c;
            device->state.ff.changed.transform[8] |= 0xff;
        }
        nine_ff_upload_vs_transforms(device);
        nine_ff_upload_tex_matrices(device);
        nine_ff_upload_lights(device);
        nine_ff_upload_point_and_fog_params(device);

        memset(state->ff.changed.transform, 0, sizeof(state->ff.changed.transform));

        device->state.changed.group |= NINE_STATE_VS_CONST;
        memset(device->state.changed.vs_const_f, ~0, 256 / 8);
    }

    if (!device->state.ps) {
        if (device->state.ff.clobber.ps_const) {
            device->state.ff.clobber.ps_const = FALSE;
            device->state.changed.group |=
                NINE_STATE_FF_PSSTAGES |
                NINE_STATE_FF_OTHER;
        }
        nine_ff_upload_ps_params(device);

        device->state.changed.group |= NINE_STATE_PS_CONST;
        memset(device->state.changed.ps_const_f, ~0, 24 / 8);
    }

    device->state.changed.group &= ~NINE_STATE_FF;
    device->state.changed.group |= NINE_STATE_VS | NINE_STATE_PS;
}


boolean
nine_ff_init(struct NineDevice9 *device)
{
    device->ff.ht_vs = util_hash_table_create(nine_ff_vs_key_hash,
                                              nine_ff_vs_key_comp);
    device->ff.ht_ps = util_hash_table_create(nine_ff_ps_key_hash,
                                              nine_ff_ps_key_comp);

    device->ff.ht_fvf = util_hash_table_create(nine_ff_fvf_key_hash,
                                               nine_ff_fvf_key_comp);

    return device->ff.ht_vs && device->ff.ht_ps && device->ff.ht_fvf;
}

static enum pipe_error nine_ff_ht_delete_cb(void *key, void *value, void *data)
{
    NineUnknown_Release(NineUnknown(value));
    return PIPE_OK;
}

void
nine_ff_fini(struct NineDevice9 *device)
{
    if (device->ff.ht_vs) {
        util_hash_table_foreach(device->ff.ht_vs, nine_ff_ht_delete_cb, NULL);
        util_hash_table_destroy(device->ff.ht_vs);
    }
    if (device->ff.ht_ps) {
        util_hash_table_foreach(device->ff.ht_ps, nine_ff_ht_delete_cb, NULL);
        util_hash_table_destroy(device->ff.ht_ps);
    }
    if (device->ff.ht_fvf) {
        util_hash_table_foreach(device->ff.ht_fvf, nine_ff_ht_delete_cb, NULL);
        util_hash_table_destroy(device->ff.ht_fvf);
    }
}

static void
nine_ff_prune_vs(struct NineDevice9 *device)
{
    if (device->ff.num_vs > 100) {
        /* could destroy the bound one here, so unbind */
        device->pipe->bind_vs_state(device->pipe, NULL);
        util_hash_table_foreach(device->ff.ht_vs, nine_ff_ht_delete_cb, NULL);
        util_hash_table_clear(device->ff.ht_vs);
        device->ff.num_vs = 0;
        device->state.changed.group |= NINE_STATE_VS;
    }
}
static void
nine_ff_prune_ps(struct NineDevice9 *device)
{
    if (device->ff.num_ps > 100) {
        /* could destroy the bound one here, so unbind */
        device->pipe->bind_fs_state(device->pipe, NULL);
        util_hash_table_foreach(device->ff.ht_ps, nine_ff_ht_delete_cb, NULL);
        util_hash_table_clear(device->ff.ht_ps);
        device->ff.num_ps = 0;
        device->state.changed.group |= NINE_STATE_PS;
    }
}

/* ========================================================================== */

/* Matrix multiplication:
 *
 * in memory: 0 1 2 3 (row major)
 *            4 5 6 7
 *            8 9 a b
 *            c d e f
 *
 *    cA cB cC cD
 * r0             = (r0 * cA) (r0 * cB) . .
 * r1             = (r1 * cA) (r1 * cB)
 * r2             = (r2 * cA) .
 * r3             = (r3 * cA) .
 *
 *               r: (11) (12) (13) (14)
 *                  (21) (22) (23) (24)
 *                  (31) (32) (33) (34)
 *                  (41) (42) (43) (44)
 * l: (11 12 13 14)
 *    (21 22 23 24)
 *    (31 32 33 34)
 *    (41 42 43 44)
 *
 * v: (x  y  z  1 )
 *
 * t.xyzw = MUL(v.xxxx, r[0]);
 * t.xyzw = MAD(v.yyyy, r[1], t.xyzw);
 * t.xyzw = MAD(v.zzzz, r[2], t.xyzw);
 * v.xyzw = MAD(v.wwww, r[3], t.xyzw);
 *
 * v.x = DP4(v, c[0]);
 * v.y = DP4(v, c[1]);
 * v.z = DP4(v, c[2]);
 * v.w = DP4(v, c[3]) = 1
 */

/*
static void
nine_D3DMATRIX_print(const D3DMATRIX *M)
{
    DBG("\n(%f %f %f %f)\n"
        "(%f %f %f %f)\n"
        "(%f %f %f %f)\n"
        "(%f %f %f %f)\n",
        M->m[0][0], M->m[0][1], M->m[0][2], M->m[0][3],
        M->m[1][0], M->m[1][1], M->m[1][2], M->m[1][3],
        M->m[2][0], M->m[2][1], M->m[2][2], M->m[2][3],
        M->m[3][0], M->m[3][1], M->m[3][2], M->m[3][3]);
}
*/

static INLINE float
nine_DP4_row_col(const D3DMATRIX *A, int r, const D3DMATRIX *B, int c)
{
    return A->m[r][0] * B->m[0][c] +
           A->m[r][1] * B->m[1][c] +
           A->m[r][2] * B->m[2][c] +
           A->m[r][3] * B->m[3][c];
}

static INLINE float
nine_DP4_vec_col(const D3DVECTOR *v, const D3DMATRIX *M, int c)
{
    return v->x * M->m[0][c] +
           v->y * M->m[1][c] +
           v->z * M->m[2][c] +
           1.0f * M->m[3][c];
}

static INLINE float
nine_DP3_vec_col(const D3DVECTOR *v, const D3DMATRIX *M, int c)
{
    return v->x * M->m[0][c] +
           v->y * M->m[1][c] +
           v->z * M->m[2][c];
}

void
nine_d3d_matrix_matrix_mul(D3DMATRIX *D, const D3DMATRIX *L, const D3DMATRIX *R)
{
    D->_11 = nine_DP4_row_col(L, 0, R, 0);
    D->_12 = nine_DP4_row_col(L, 0, R, 1);
    D->_13 = nine_DP4_row_col(L, 0, R, 2);
    D->_14 = nine_DP4_row_col(L, 0, R, 3);

    D->_21 = nine_DP4_row_col(L, 1, R, 0);
    D->_22 = nine_DP4_row_col(L, 1, R, 1);
    D->_23 = nine_DP4_row_col(L, 1, R, 2);
    D->_24 = nine_DP4_row_col(L, 1, R, 3);

    D->_31 = nine_DP4_row_col(L, 2, R, 0);
    D->_32 = nine_DP4_row_col(L, 2, R, 1);
    D->_33 = nine_DP4_row_col(L, 2, R, 2);
    D->_34 = nine_DP4_row_col(L, 2, R, 3);

    D->_41 = nine_DP4_row_col(L, 3, R, 0);
    D->_42 = nine_DP4_row_col(L, 3, R, 1);
    D->_43 = nine_DP4_row_col(L, 3, R, 2);
    D->_44 = nine_DP4_row_col(L, 3, R, 3);
}

void
nine_d3d_vector4_matrix_mul(D3DVECTOR *d, const D3DVECTOR *v, const D3DMATRIX *M)
{
    d->x = nine_DP4_vec_col(v, M, 0);
    d->y = nine_DP4_vec_col(v, M, 1);
    d->z = nine_DP4_vec_col(v, M, 2);
}

void
nine_d3d_vector3_matrix_mul(D3DVECTOR *d, const D3DVECTOR *v, const D3DMATRIX *M)
{
    d->x = nine_DP3_vec_col(v, M, 0);
    d->y = nine_DP3_vec_col(v, M, 1);
    d->z = nine_DP3_vec_col(v, M, 2);
}

void
nine_d3d_matrix_transpose(D3DMATRIX *D, const D3DMATRIX *M)
{
    unsigned i, j;
    for (i = 0; i < 4; ++i)
    for (j = 0; j < 4; ++j)
        D->m[i][j] = M->m[j][i];
}

#define _M_ADD_PROD_1i_2j_3k_4l(i,j,k,l) do {            \
    float t = M->_1##i * M->_2##j * M->_3##k * M->_4##l; \
    if (t > 0.0f) pos += t; else neg += t; } while(0)

#define _M_SUB_PROD_1i_2j_3k_4l(i,j,k,l) do {            \
    float t = M->_1##i * M->_2##j * M->_3##k * M->_4##l; \
    if (t > 0.0f) neg -= t; else pos -= t; } while(0)
float
nine_d3d_matrix_det(const D3DMATRIX *M)
{
    float pos = 0.0f;
    float neg = 0.0f;

    _M_ADD_PROD_1i_2j_3k_4l(1, 2, 3, 4);
    _M_ADD_PROD_1i_2j_3k_4l(1, 3, 4, 2);
    _M_ADD_PROD_1i_2j_3k_4l(1, 4, 2, 3);

    _M_ADD_PROD_1i_2j_3k_4l(2, 1, 4, 3);
    _M_ADD_PROD_1i_2j_3k_4l(2, 3, 1, 4);
    _M_ADD_PROD_1i_2j_3k_4l(2, 4, 3, 1);

    _M_ADD_PROD_1i_2j_3k_4l(3, 1, 2, 4);
    _M_ADD_PROD_1i_2j_3k_4l(3, 2, 4, 1);
    _M_ADD_PROD_1i_2j_3k_4l(3, 4, 1, 2);

    _M_ADD_PROD_1i_2j_3k_4l(4, 1, 3, 2);
    _M_ADD_PROD_1i_2j_3k_4l(4, 2, 1, 3);
    _M_ADD_PROD_1i_2j_3k_4l(4, 3, 2, 1);

    _M_SUB_PROD_1i_2j_3k_4l(1, 2, 4, 3);
    _M_SUB_PROD_1i_2j_3k_4l(1, 3, 2, 4);
    _M_SUB_PROD_1i_2j_3k_4l(1, 4, 3, 2);

    _M_SUB_PROD_1i_2j_3k_4l(2, 1, 3, 4);
    _M_SUB_PROD_1i_2j_3k_4l(2, 3, 4, 1);
    _M_SUB_PROD_1i_2j_3k_4l(2, 4, 1, 3);

    _M_SUB_PROD_1i_2j_3k_4l(3, 1, 4, 2);
    _M_SUB_PROD_1i_2j_3k_4l(3, 2, 1, 4);
    _M_SUB_PROD_1i_2j_3k_4l(3, 4, 2, 1);

    _M_SUB_PROD_1i_2j_3k_4l(4, 1, 2, 3);
    _M_SUB_PROD_1i_2j_3k_4l(4, 2, 3, 1);
    _M_SUB_PROD_1i_2j_3k_4l(4, 3, 1, 2);

    return pos + neg;
}

/* XXX: Probably better to just use src/mesa/math/m_matrix.c because
 * I have no idea where this code came from.
 */
void
nine_d3d_matrix_inverse(D3DMATRIX *D, const D3DMATRIX *M)
{
    int i, k;
    float det;

    D->m[0][0] =
        M->m[1][1] * M->m[2][2] * M->m[3][3] -
        M->m[1][1] * M->m[3][2] * M->m[2][3] -
        M->m[1][2] * M->m[2][1] * M->m[3][3] +
        M->m[1][2] * M->m[3][1] * M->m[2][3] +
        M->m[1][3] * M->m[2][1] * M->m[3][2] -
        M->m[1][3] * M->m[3][1] * M->m[2][2];

    D->m[0][1] =
       -M->m[0][1] * M->m[2][2] * M->m[3][3] +
        M->m[0][1] * M->m[3][2] * M->m[2][3] +
        M->m[0][2] * M->m[2][1] * M->m[3][3] -
        M->m[0][2] * M->m[3][1] * M->m[2][3] -
        M->m[0][3] * M->m[2][1] * M->m[3][2] +
        M->m[0][3] * M->m[3][1] * M->m[2][2];

    D->m[0][2] =
        M->m[0][1] * M->m[1][2] * M->m[3][3] -
        M->m[0][1] * M->m[3][2] * M->m[1][3] -
        M->m[0][2] * M->m[1][1] * M->m[3][3] +
        M->m[0][2] * M->m[3][1] * M->m[1][3] +
        M->m[0][3] * M->m[1][1] * M->m[3][2] -
        M->m[0][3] * M->m[3][1] * M->m[1][2];

    D->m[0][3] =
       -M->m[0][1] * M->m[1][2] * M->m[2][3] +
        M->m[0][1] * M->m[2][2] * M->m[1][3] +
        M->m[0][2] * M->m[1][1] * M->m[2][3] -
        M->m[0][2] * M->m[2][1] * M->m[1][3] -
        M->m[0][3] * M->m[1][1] * M->m[2][2] +
        M->m[0][3] * M->m[2][1] * M->m[1][2];

    D->m[1][0] =
       -M->m[1][0] * M->m[2][2] * M->m[3][3] +
        M->m[1][0] * M->m[3][2] * M->m[2][3] +
        M->m[1][2] * M->m[2][0] * M->m[3][3] -
        M->m[1][2] * M->m[3][0] * M->m[2][3] -
        M->m[1][3] * M->m[2][0] * M->m[3][2] +
        M->m[1][3] * M->m[3][0] * M->m[2][2];

    D->m[1][1] =
        M->m[0][0] * M->m[2][2] * M->m[3][3] -
        M->m[0][0] * M->m[3][2] * M->m[2][3] -
        M->m[0][2] * M->m[2][0] * M->m[3][3] +
        M->m[0][2] * M->m[3][0] * M->m[2][3] +
        M->m[0][3] * M->m[2][0] * M->m[3][2] -
        M->m[0][3] * M->m[3][0] * M->m[2][2];

    D->m[1][2] =
       -M->m[0][0] * M->m[1][2] * M->m[3][3] +
        M->m[0][0] * M->m[3][2] * M->m[1][3] +
        M->m[0][2] * M->m[1][0] * M->m[3][3] -
        M->m[0][2] * M->m[3][0] * M->m[1][3] -
        M->m[0][3] * M->m[1][0] * M->m[3][2] +
        M->m[0][3] * M->m[3][0] * M->m[1][2];

    D->m[1][3] =
        M->m[0][0] * M->m[1][2] * M->m[2][3] -
        M->m[0][0] * M->m[2][2] * M->m[1][3] -
        M->m[0][2] * M->m[1][0] * M->m[2][3] +
        M->m[0][2] * M->m[2][0] * M->m[1][3] +
        M->m[0][3] * M->m[1][0] * M->m[2][2] -
        M->m[0][3] * M->m[2][0] * M->m[1][2];

    D->m[2][0] =
        M->m[1][0] * M->m[2][1] * M->m[3][3] -
        M->m[1][0] * M->m[3][1] * M->m[2][3] -
        M->m[1][1] * M->m[2][0] * M->m[3][3] +
        M->m[1][1] * M->m[3][0] * M->m[2][3] +
        M->m[1][3] * M->m[2][0] * M->m[3][1] -
        M->m[1][3] * M->m[3][0] * M->m[2][1];

    D->m[2][1] =
       -M->m[0][0] * M->m[2][1] * M->m[3][3] +
        M->m[0][0] * M->m[3][1] * M->m[2][3] +
        M->m[0][1] * M->m[2][0] * M->m[3][3] -
        M->m[0][1] * M->m[3][0] * M->m[2][3] -
        M->m[0][3] * M->m[2][0] * M->m[3][1] +
        M->m[0][3] * M->m[3][0] * M->m[2][1];

    D->m[2][2] =
        M->m[0][0] * M->m[1][1] * M->m[3][3] -
        M->m[0][0] * M->m[3][1] * M->m[1][3] -
        M->m[0][1] * M->m[1][0] * M->m[3][3] +
        M->m[0][1] * M->m[3][0] * M->m[1][3] +
        M->m[0][3] * M->m[1][0] * M->m[3][1] -
        M->m[0][3] * M->m[3][0] * M->m[1][1];

    D->m[2][3] =
       -M->m[0][0] * M->m[1][1] * M->m[2][3] +
        M->m[0][0] * M->m[2][1] * M->m[1][3] +
        M->m[0][1] * M->m[1][0] * M->m[2][3] -
        M->m[0][1] * M->m[2][0] * M->m[1][3] -
        M->m[0][3] * M->m[1][0] * M->m[2][1] +
        M->m[0][3] * M->m[2][0] * M->m[1][1];

    D->m[3][0] =
       -M->m[1][0] * M->m[2][1] * M->m[3][2] +
        M->m[1][0] * M->m[3][1] * M->m[2][2] +
        M->m[1][1] * M->m[2][0] * M->m[3][2] -
        M->m[1][1] * M->m[3][0] * M->m[2][2] -
        M->m[1][2] * M->m[2][0] * M->m[3][1] +
        M->m[1][2] * M->m[3][0] * M->m[2][1];

    D->m[3][1] =
        M->m[0][0] * M->m[2][1] * M->m[3][2] -
        M->m[0][0] * M->m[3][1] * M->m[2][2] -
        M->m[0][1] * M->m[2][0] * M->m[3][2] +
        M->m[0][1] * M->m[3][0] * M->m[2][2] +
        M->m[0][2] * M->m[2][0] * M->m[3][1] -
        M->m[0][2] * M->m[3][0] * M->m[2][1];

    D->m[3][2] =
       -M->m[0][0] * M->m[1][1] * M->m[3][2] +
        M->m[0][0] * M->m[3][1] * M->m[1][2] +
        M->m[0][1] * M->m[1][0] * M->m[3][2] -
        M->m[0][1] * M->m[3][0] * M->m[1][2] -
        M->m[0][2] * M->m[1][0] * M->m[3][1] +
        M->m[0][2] * M->m[3][0] * M->m[1][1];

    D->m[3][3] =
        M->m[0][0] * M->m[1][1] * M->m[2][2] -
        M->m[0][0] * M->m[2][1] * M->m[1][2] -
        M->m[0][1] * M->m[1][0] * M->m[2][2] +
        M->m[0][1] * M->m[2][0] * M->m[1][2] +
        M->m[0][2] * M->m[1][0] * M->m[2][1] -
        M->m[0][2] * M->m[2][0] * M->m[1][1];

    det =
        M->m[0][0] * D->m[0][0] +
        M->m[1][0] * D->m[0][1] +
        M->m[2][0] * D->m[0][2] +
        M->m[3][0] * D->m[0][3];

    det = 1.0 / det;

    for (i = 0; i < 4; i++)
    for (k = 0; k < 4; k++)
        D->m[i][k] *= det;

#ifdef DEBUG
    {
        D3DMATRIX I;

        nine_d3d_matrix_matrix_mul(&I, D, M);

        for (i = 0; i < 4; ++i)
        for (k = 0; k < 4; ++k)
            if (fabsf(I.m[i][k] - (float)(i == k)) > 1e-3)
                DBG("Matrix inversion check FAILED !\n");
    }
#endif
}

/* TODO: don't use 4x4 inverse, unless this gets all nicely inlined ? */
void
nine_d3d_matrix_inverse_3x3(D3DMATRIX *D, const D3DMATRIX *M)
{
    D3DMATRIX T;
    unsigned i, j;

    for (i = 0; i < 3; ++i)
    for (j = 0; j < 3; ++j)
        T.m[i][j] = M->m[i][j];
    for (i = 0; i < 3; ++i) {
        T.m[i][3] = 0.0f;
        T.m[3][i] = 0.0f;
    }
    T.m[3][3] = 1.0f;

    nine_d3d_matrix_inverse(D, &T);
}
