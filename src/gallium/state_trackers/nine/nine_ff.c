
#include "device9.h"
#include "vertexdeclaration9.h"
#include "vertexshader9.h"
#include "pixelshader9.h"
#include "nine_ff.h"
#include "nine_defines.h"
#include "nine_helpers.h"

#include "pipe/p_context.h"
#include "tgsi/tgsi_ureg.h"
#include "util/u_box.h"
#include "util/u_hash_table.h"

#define DBG_CHANNEL DBG_FF

struct nine_ff_vs_key
{
    union {
        struct {
            uint32_t position_t : 1;
            uint32_t lighting   : 1;
        };
        uint64_t value; /* if u64 isn't enough, resize VertexShader9.ff_key */
    };
};
struct nine_ff_ps_key
{
    union {
        struct {
            uint32_t aaa : 1;
        };
        uint64_t value; /* if u64 isn't enough, resize PixelShader9.ff_key */
    };
};

static unsigned nine_ff_vs_key_hash(void *key)
{
    return ((struct nine_ff_vs_key *)key)->value;
}
static int nine_ff_vs_key_comp(void *key1, void *key2)
{
    struct nine_ff_vs_key *a = (struct nine_ff_vs_key *)key1;
    struct nine_ff_vs_key *b = (struct nine_ff_vs_key *)key2;

    return a->value != b->value;
}
static unsigned nine_ff_ps_key_hash(void *key)
{
    return ((struct nine_ff_ps_key *)key)->value;
}
static int nine_ff_ps_key_comp(void *key1, void *key2)
{
    struct nine_ff_ps_key *a = (struct nine_ff_ps_key *)key1;
    struct nine_ff_ps_key *b = (struct nine_ff_ps_key *)key2;

    return a->value != b->value;
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

#define LIGHT_CONST(i) \
    ureg_src_indirect(ureg_src_register(TGSI_FILE_CONSTANT, 32 + (i)), _X(AL))

#define MATERIAL_CONST(i)                           \
    ureg_src_register(TGSI_FILE_CONSTANT, 89 + (i))

#define MISC_CONST(i)                           \
    ureg_src_register(TGSI_FILE_CONSTANT, (i))

/* VS FF constants layout:
 *
 * CONST[ 0.. 3] D3DTS_WORLD * D3DTS_VIEW * D3DTS_PROJECTION
 * CONST[ 4.. 7] D3DTS_VIEW
 * CONST[ 8..11] D3DTS_PROJECTION
 * CONST[12..15] D3DTS_WORLD
 * CONST[16..19] Normal matrix
 *
 * CONST[32].x___ LIGHT[0].Type
 * CONST[32]._yzw LIGHT[0].Attenuation0,1,2
 * CONST[33]      LIGHT[0].Diffuse
 * CONST[34]      LIGHT[0].Specular
 * CONST[35]      LIGHT[0].Ambient
 * CONST[36].xyz_ LIGHT[0].Position
 * CONST[36].___w LIGHT[0].Range
 * CONST[37].xyz  LIGHT[0].Direction
 * CONST[37].___w LIGHT[0].Falloff
 * CONST[38].x___ LIGHT[0].Theta
 * CONST[38]._y__ LIGHT[0].Phi
 * CONST[39].x___ light count
 * CONST[40]      LIGHT[1]
 * CONST[48]      LIGHT[2]
 * CONST[56]      LIGHT[3]
 * CONST[64]      LIGHT[4]
 * CONST[72]      LIGHT[5]
 * CONST[80]      LIGHT[6]
 * CONST[88]      LIGHT[7]
 *
 * CONST[95]      MATERIAL.Diffuse
 * CONST[96]      MATERIAL.Ambient
 * CONST[97]      MATERIAL.Specular
 * CONST[98]      MATERIAL.Emissive
 * CONST[99].x___ MATERIAL.Power
 *
 * CONST[224] D3DTS_WORLDMATRIX[0]
 * CONST[228] D3DTS_WORLDMATRIX[1]
 * ...
 * CONST[252] D3DTS_WORLDMATRIX[7]
 */
static void *
nine_ff_build_vs(struct NineDevice9 *device, struct nine_ff_vs_key *key)
{
    struct nine_state *state = &device->state;
    struct ureg_program *ureg = ureg_create(TGSI_PROCESSOR_VERTEX);
    struct ureg_src aVtx, aCol[2], aTex[8], aNrm;
    struct ureg_dst oPos, oCol[2], oTex[8];
    struct ureg_src c[18];
    struct ureg_dst r[6];
    struct ureg_src r_src[6];
    struct ureg_dst A0;
    unsigned i;
    unsigned n;
    unsigned label[32], l = 0;

    (void)state;

    for (i = 0; i < 18; ++i)
        c[i] = ureg_DECL_constant(ureg, i); /* transforms */

    if (key->lighting) {
        for (i = 32; i <= 94; ++i)
            ureg_DECL_constant(ureg, i); /* light properties */
        for (i = 95; i <= 99; ++i)
            ureg_DECL_constant(ureg, i); /* material properties */
    }

    n = 0;
    aVtx = ureg_DECL_vs_input(ureg, n++);
    if (key->lighting)
        aNrm = ureg_DECL_vs_input(ureg, n++);
    aCol[0] = ureg_DECL_vs_input(ureg, n++);
    aCol[1] = ureg_DECL_vs_input(ureg, n++);
    aTex[0] = ureg_DECL_vs_input(ureg, n++);

    oPos = ureg_DECL_output(ureg, TGSI_SEMANTIC_POSITION, 0); /* HPOS */
    oCol[0] = ureg_DECL_output(ureg, TGSI_SEMANTIC_COLOR, 0);
    oCol[1] = ureg_DECL_output(ureg, TGSI_SEMANTIC_COLOR, 1);
    oTex[0] = ureg_DECL_output(ureg, TGSI_SEMANTIC_TEXCOORD, 0);

    for (i = 0; i < 6; ++i) {
        r[i] = ureg_DECL_local_temporary(ureg);
        r_src[i] = ureg_src(r[i]);
    }
    if (key->lighting)
        A0 = ureg_DECL_address(ureg);

    /* HPOS */
    if (key->position_t) {
        ureg_MOV(ureg, oPos, aVtx);
    } else {
        ureg_MUL(ureg, r[0], ureg_scalar(aVtx, TGSI_SWIZZLE_X), c[0]);
        ureg_MAD(ureg, r[0], ureg_scalar(aVtx, TGSI_SWIZZLE_Y), c[1], r_src[0]);
        ureg_MAD(ureg, r[0], ureg_scalar(aVtx, TGSI_SWIZZLE_Z), c[2], r_src[0]);
        ureg_MAD(ureg, r[0], ureg_scalar(aVtx, TGSI_SWIZZLE_W), c[3], r_src[0]);
        ureg_MOV(ureg, oPos, r_src[0]);
    }
    /* LIGHT */
    if (key->lighting) {
#if 0
        struct ureg_dst rNrm = ureg_writemask(r[0], TGSI_WRITEMASK_XYZ);
        struct ureg_dst rCtr = ureg_writemask(r[1], TGSI_WRITEMASK_X);
        struct ureg_dst rCmp = ureg_writemask(r[1], TGSI_WRITEMASK_Y);
        struct ureg_dst rDst = ureg_writemask(r[2], TGSI_WRITEMASK_XYZ);
        struct ureg_dst rAtt = ureg_writemask(r[2], TGSI_WRITEMASK_W);
        struct ureg_dst rTmpW = ureg_writemask(r[0], TGSI_WRITEMASK_W);
        struct ureg_dst rTmpZ = ureg_writemask(r[1], TGSI_WRITEMASK_Z);
        struct ureg_dst rColD = r[3];
        struct ureg_dst rColS = r[4];
        struct ureg_dst rColA = r[5];

        struct ureg_dst AL = ureg_writemask(A0, TGSI_WRITEMASK_X);

        struct ureg_src cMColD = _XYZW(MATERIAL_CONST(0));
        struct ureg_src cMColA = _XYZW(MATERIAL_CONST(1));
        struct ureg_src cMColS = _XYZW(MATERIAL_CONST(2));
        struct ureg_src cMEmit = _XYZW(MATERIAL_CONST(3));
        struct ureg_src cMPowr = _XXXX(MATERIAL_CONST(4));

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

        struct ureg_src cLCnt  = _XXXX(MISC_CONST(39));

        ureg_MOV(ureg, rCtr, ureg_imm1f(ureg, 0.0f));
        ureg_NRM(ureg, rNrm, aNrm);

        ureg_BGNLOOP(ureg, &label[l++]);
        ureg_SGE(ureg, rCmp, _X(rCtr), cLCnt);
        ureg_ARL(ureg, AL, _X(rCtr));
        ureg_IF(ureg, _Y(rCmp), &label[l++]);
        ureg_BRK(ureg);
        ureg_ENDIF(ureg);
        ureg_ADD(ureg, rCtr, _X(rCtr), ureg_imm1f(ureg, 1.0f));

        ureg_fixup_label(ureg, label[l-1], ureg_get_instruction_number(ureg));

        ureg_SEQ(ureg, rCmp, cLKind, ureg_imm1f(ureg, D3DLIGHT_POINT));
        ureg_IF(ureg, _Y(rCmp), &label[l++]);
        {
            /* POINT light */
            ureg_SUB(ureg, rDst, ureg_src(rDst), cLPos);
            ureg_DP3(ureg, rTmpW, ureg_src(rDst), ureg_src(rDst));
            ureg_SQRT(ureg, rTmpW, _W(rTmpW));
            /* att0 + d * (att1 + d * att2) */
            ureg_MAD(ureg, rAtt, cLAtt2, _W(rTmpW), cLAtt1);
            ureg_MAD(ureg, rAtt, _W(rAtt), _W(rTmpW), cLAtt0);
        }
        ureg_fixup_label(ureg, label[l-1], ureg_get_instruction_number(ureg));

        ureg_ELSE(ureg, &label[l++]);

        ureg_SEQ(ureg, rCmp, cLKind, ureg_imm1f(ureg, D3DLIGHT_SPOT));
        ureg_IF(ureg, _Y(rCmp), &label[l++]);
        {
            /* SPOT light */
        }
        ureg_fixup_label(ureg, label[l-1], ureg_get_instruction_number(ureg));
        ureg_ELSE(ureg, &label[l++]);
        {
            /* DIRECTIONAL light */
        }
        ureg_fixup_label(ureg, label[l-1], ureg_get_instruction_number(ureg));
        ureg_ENDIF(ureg);
        ureg_fixup_label(ureg, label[l-2], ureg_get_instruction_number(ureg));
        ureg_ENDIF(ureg);

        ureg_fixup_label(ureg, label[l-6], ureg_get_instruction_number(ureg));
        ureg_ENDLOOP(ureg, &label[0]);

        /* apply to material */
        ureg_MUL(ureg, rColD, ureg_src(rColD), cMColD);
        ureg_MAD(ureg, oCol[0], ureg_src(rColS), cMColS, ureg_src(rColD));
        /* clamp_vertex_color */
#endif
    } else {
        /* COLOR */
        ureg_MOV(ureg, oCol[0], aCol[0]);
        ureg_MOV(ureg, oCol[1], aCol[1]);
    }
    (void)l;
    (void)label;
    (void)A0;
    (void)aNrm;

    /* TEXCOORD */
    ureg_MOV(ureg, oTex[0], aTex[0]);

    ureg_END(ureg);
    return ureg_create_shader_and_destroy(ureg, device->pipe);
}

/* PS FF constants layout:
 *
 * CONST[ 0.. 3] D3DTS_TEXTURE0
 * CONST[ 4.. 7] D3DTS_TEXTURE1
 * ...
 * CONST[28..31] D3DTS_TEXTURE7
 */
static void *
nine_ff_build_ps(struct NineDevice9 *device, struct nine_ff_ps_key *key)
{
    struct nine_state *state = &device->state;
    struct ureg_program *ureg = ureg_create(TGSI_PROCESSOR_FRAGMENT);
    struct ureg_src vC[2];
    struct ureg_src vT[8];
    struct ureg_src s[8];
    struct ureg_dst oCol;
    struct ureg_src c[8];
    struct ureg_dst r[4];
    struct ureg_src r_src[4];
    unsigned i;

    (void)state;

    for (i = 0; i < 8; ++i)
        c[i] = ureg_DECL_constant(ureg, i);

    vC[0] = ureg_DECL_fs_input(ureg, TGSI_SEMANTIC_COLOR, 0, TGSI_INTERPOLATE_PERSPECTIVE);
    vC[1] = ureg_DECL_fs_input(ureg, TGSI_SEMANTIC_COLOR, 1, TGSI_INTERPOLATE_PERSPECTIVE);
    vT[0] = ureg_DECL_fs_input(ureg, TGSI_SEMANTIC_TEXCOORD, 0, TGSI_INTERPOLATE_PERSPECTIVE);

    oCol = ureg_DECL_output(ureg, TGSI_SEMANTIC_COLOR, 0);

    for (i = 0; i < 4; ++i) {
        r[i] = ureg_DECL_local_temporary(ureg);
        r_src[i] = ureg_src(r[i]);
    }

    if (0) {
        s[0] = ureg_DECL_sampler(ureg, 0);
        ureg_TEX(ureg, r[0], PIPE_TEXTURE_2D, vT[0], s[0]);
        ureg_MAD(ureg, r[0], r_src[0], vC[0], c[0]);
        ureg_ADD(ureg, oCol, r_src[0], vC[1]);
    } else {
        ureg_ADD(ureg, oCol, vC[0], vC[1]);
    }

    ureg_END(ureg);
    return ureg_create_shader_and_destroy(ureg, device->pipe);
}

static struct NineVertexShader9 *
nine_ff_get_vs(struct NineDevice9 *device)
{
    const struct nine_state *state = &device->state;
    struct NineVertexShader9 *vs;
    enum pipe_error err;
    struct nine_ff_vs_key key;

    key.value = 0;

    /* FIXME: this shouldn't be NULL, but it is on init */
    if (state->vdecl &&
        state->vdecl->usage_map[NINE_DECLUSAGE_POSITIONT] != 0xff)
        key.position_t = 1;

    vs = util_hash_table_get(device->ff.ht_vs, &key);
    if (vs)
        return vs;
    NineVertexShader9_new(device, &vs, NULL, nine_ff_build_vs(device, &key));

    if (vs) {
        unsigned n;

        vs->ff_key = key.value;

        err = util_hash_table_set(device->ff.ht_vs, &vs->ff_key, vs);
        assert(err == PIPE_OK);

        n = 0;
        vs->input_map[n++].ndecl = NINE_DECLUSAGE_POSITION;
        if (key.lighting)
            vs->input_map[n++].ndecl = NINE_DECLUSAGE_NORMAL(0);
        vs->input_map[n++].ndecl = NINE_DECLUSAGE_COLOR(0);
        vs->input_map[n++].ndecl = NINE_DECLUSAGE_COLOR(1);
        vs->input_map[n++].ndecl = NINE_DECLUSAGE_TEXCOORD(0);
        vs->num_inputs = n;

        if (key.position_t)
            vs->input_map[0].ndecl = NINE_DECLUSAGE_POSITIONT;
    }
    return vs;
}

static struct NinePixelShader9 *
nine_ff_get_ps(struct NineDevice9 *device)
{
    struct NinePixelShader9 *ps;
    enum pipe_error err;
    struct nine_ff_ps_key key;

    key.value = 0;

    ps = util_hash_table_get(device->ff.ht_ps, &key);
    if (ps)
        return ps;
    NinePixelShader9_new(device, &ps, NULL, nine_ff_build_ps(device, &key));

    if (ps) {
        ps->ff_key = key.value;

        err = util_hash_table_set(device->ff.ht_ps, &ps->ff_key, ps);
        assert(err == PIPE_OK);
    }
    return ps;
}

#define GET_D3DTS(n) nine_state_access_transform(state, D3DTS_##n, FALSE)
static void
nine_ff_upload_vs_transforms(struct NineDevice9 *device)
{
    struct pipe_context *pipe = device->pipe;
    struct nine_state *state = &device->state;
    D3DMATRIX M, T;
    struct pipe_box box;
    const unsigned usage = PIPE_TRANSFER_WRITE | PIPE_TRANSFER_DISCARD_RANGE;

    nine_d3d_matrix_matrix_mul(&T, GET_D3DTS(WORLD), GET_D3DTS(VIEW));
    nine_d3d_matrix_matrix_mul(&M, &T,               GET_D3DTS(PROJECTION));
    u_box_1d(0, sizeof(M), &box);
    pipe->transfer_inline_write(pipe, device->constbuf_vs, 0, usage,
                                &box,
                                &M.m[0][0], 0, 0);

    nine_d3d_matrix_inverse_3x3(&T, &M);
    nine_d3d_matrix_transpose(&M, &T);
    box.x = 4 * sizeof(M);
    pipe->transfer_inline_write(pipe, device->constbuf_vs, 0, usage,
                                &box,
                                &M.m[0][0], 0, 0);

    box.x = 1 * sizeof(M);
    pipe->transfer_inline_write(pipe, device->constbuf_vs, 0, usage,
                                &box,
                                GET_D3DTS(VIEW), 0, 0);

    box.x = 2 * sizeof(M);
    pipe->transfer_inline_write(pipe, device->constbuf_vs, 0, usage,
                                &box,
                                GET_D3DTS(PROJECTION), 0, 0);

    box.x = 3 * sizeof(M);
    pipe->transfer_inline_write(pipe, device->constbuf_vs, 0, usage,
                                &box,
                                GET_D3DTS(WORLD), 0, 0);
}

void
nine_ff_update(struct NineDevice9 *device)
{
    DBG("Warning: FF is just a dummy.\n");

    if (!device->state.vs)
        nine_reference(&device->ff.vs, nine_ff_get_vs(device));
    if (!device->state.ps)
        nine_reference(&device->ff.ps, nine_ff_get_ps(device));

    if (!device->state.vs &&
        (device->state.ff.changed.transform[0] |
         device->state.ff.changed.transform[256 / 32]))
        nine_ff_upload_vs_transforms(device);

    device->state.changed.group |= NINE_STATE_VS | NINE_STATE_PS;
}


boolean
nine_ff_init(struct NineDevice9 *device)
{
    device->ff.ht_vs = util_hash_table_create(nine_ff_vs_key_hash,
                                              nine_ff_vs_key_comp);
    device->ff.ht_ps = util_hash_table_create(nine_ff_ps_key_hash,
                                              nine_ff_ps_key_comp);

    return device->ff.ht_vs && device->ff.ht_ps;
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
nine_d3d_vector_matrix_mul(D3DVECTOR *d, const D3DVECTOR *v, const D3DMATRIX *M)
{
    d->x = nine_DP4_vec_col(v, M, 0);
    d->z = nine_DP4_vec_col(v, M, 1);
    d->y = nine_DP4_vec_col(v, M, 2);
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
    float v0 = M->m[0][2] * M->m[1][3] - M->m[1][2] * M->m[0][3];
    float v1 = M->m[0][2] * M->m[2][3] - M->m[2][2] * M->m[0][3];
    float v2 = M->m[0][2] * M->m[3][3] - M->m[3][2] * M->m[0][3];
    float v3 = M->m[1][2] * M->m[2][3] - M->m[2][2] * M->m[1][3];
    float v4 = M->m[1][2] * M->m[3][3] - M->m[3][2] * M->m[1][3];
    float v5 = M->m[2][2] * M->m[3][3] - M->m[3][2] * M->m[2][3];

    float t00 = +(v5 * M->m[1][1] - v4 * M->m[2][1] + v3 * M->m[3][1]);
    float t10 = -(v5 * M->m[0][1] - v2 * M->m[2][1] + v1 * M->m[3][1]);
    float t20 = +(v4 * M->m[0][1] - v2 * M->m[1][1] + v0 * M->m[3][1]);
    float t30 = -(v3 * M->m[0][1] - v1 * M->m[1][1] + v0 * M->m[2][1]);

    float det = t00 * M->m[0][0] + t10 * M->m[1][0] + t20 * M->m[2][3] + t30 * M->m[3][0];

    float inv_det = 1 / det;

    float d00 = t00 * inv_det;
    float d01 = t10 * inv_det;
    float d02 = t20 * inv_det;
    float d03 = t30 * inv_det;

    float d10 = -(v5 * M->m[1][0] - v4 * M->m[2][0] + v3 * M->m[3][0]) * inv_det;
    float d11 = +(v5 * M->m[0][0] - v2 * M->m[2][0] + v1 * M->m[3][0]) * inv_det;
    float d12 = -(v4 * M->m[0][0] - v2 * M->m[1][0] + v0 * M->m[3][0]) * inv_det;
    float d13 = +(v3 * M->m[0][0] - v1 * M->m[1][0] + v0 * M->m[2][0]) * inv_det;

    v0 = M->m[0][1] * M->m[1][3] - M->m[1][1] * M->m[0][3];
    v1 = M->m[0][1] * M->m[2][3] - M->m[2][1] * M->m[0][3];
    v2 = M->m[0][1] * M->m[3][3] - M->m[3][1] * M->m[0][3];
    v3 = M->m[1][1] * M->m[2][3] - M->m[2][1] * M->m[1][3];
    v4 = M->m[1][1] * M->m[3][3] - M->m[3][3] * M->m[1][3];
    v5 = M->m[2][1] * M->m[3][3] - M->m[3][1] * M->m[2][3];

    float d20 = +(v5 * M->m[1][0] - v4 * M->m[2][0] + v3 * M->m[3][0]) * inv_det;
    float d21 = -(v5 * M->m[0][0] - v2 * M->m[2][0] + v1 * M->m[3][0]) * inv_det;
    float d22 = +(v4 * M->m[0][0] - v2 * M->m[1][0] + v0 * M->m[3][0]) * inv_det;
    float d23 = -(v3 * M->m[0][0] - v1 * M->m[1][0] + v0 * M->m[2][0]) * inv_det;

    v0 = M->m[1][2] * M->m[0][1] - M->m[0][2] * M->m[1][1];
    v1 = M->m[2][2] * M->m[0][1] - M->m[0][2] * M->m[2][1];
    v2 = M->m[3][2] * M->m[0][1] - M->m[0][2] * M->m[3][1];
    v3 = M->m[2][2] * M->m[1][1] - M->m[1][2] * M->m[2][1];
    v4 = M->m[3][2] * M->m[1][1] - M->m[1][2] * M->m[3][1];
    v5 = M->m[3][2] * M->m[2][1] - M->m[2][2] * M->m[3][1];

    float d30 = -(v5 * M->m[1][0] - v4 * M->m[2][0] + v3 * M->m[3][0]) * inv_det;
    float d31 = +(v5 * M->m[0][0] - v2 * M->m[2][0] + v1 * M->m[3][0]) * inv_det;
    float d32 = -(v4 * M->m[0][0] - v2 * M->m[1][0] + v0 * M->m[3][0]) * inv_det;
    float d33 = +(v3 * M->m[0][0] - v1 * M->m[1][0] + v0 * M->m[2][0]) * inv_det;

    D->m[0][0] = d00; D->m[0][1] = d01; D->m[0][2] = d02; D->m[0][3] = d03;
    D->m[1][0] = d10; D->m[1][1] = d11; D->m[1][2] = d12; D->m[1][3] = d13;
    D->m[2][0] = d20; D->m[2][1] = d21; D->m[2][2] = d22; D->m[2][3] = d23;
    D->m[3][0] = d30; D->m[3][1] = d31; D->m[3][2] = d32; D->m[3][3] = d33;
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
