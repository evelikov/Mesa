
#include "device9.h"
#include "vertexshader9.h"
#include "pixelshader9.h"
#include "nine_ff.h"
#include "nine_defines.h"
#include "nine_helpers.h"

#include "pipe/p_context.h"
#include "tgsi/tgsi_ureg.h"
#include "util/u_box.h"

#define DBG_CHANNEL DBG_FF

/* VS FF constants layout:
 *
 * CONST[ 0.. 3] D3DTS_VIEW * D3DTS_PROJECTION
 * CONST[ 4.. 7] D3DTS_VIEW
 * CONST[ 8..11] D3DTS_PROJECTION
 * CONST[12..15] D3DTS_WORLD
 * CONST[16..19] Normal matrix
 */
static void *
nine_ff_build_vs(struct NineDevice9 *device)
{
    struct nine_state *state = &device->state;
    struct ureg_program *ureg = ureg_create(TGSI_PROCESSOR_VERTEX);
    struct ureg_src a[4];
    struct ureg_dst oPos, oCol[2], oTex[8];
    struct ureg_src c[18];
    struct ureg_dst r[4];
    struct ureg_src r_src[4];
    unsigned i;

    (void)state;

    for (i = 0; i < 18; ++i)
        c[i] = ureg_DECL_constant(ureg, i);
    for (i = 0; i < 4; ++i)
        a[i] = ureg_DECL_vs_input(ureg, i);

    oPos = ureg_DECL_output(ureg, TGSI_SEMANTIC_POSITION, 0); /* HPOS */
    oCol[0] = ureg_DECL_output(ureg, TGSI_SEMANTIC_COLOR, 0);
    oCol[1] = ureg_DECL_output(ureg, TGSI_SEMANTIC_COLOR, 1);
    oTex[0] = ureg_DECL_output(ureg, TGSI_SEMANTIC_TEXCOORD, 0);

    for (i = 0; i < 4; ++i) {
        r[i] = ureg_DECL_local_temporary(ureg);
        r_src[i] = ureg_src(r[i]);
    }

    /* HPOS */
    ureg_MUL(ureg, r[0], ureg_scalar(a[0], TGSI_SWIZZLE_X), c[0]);
    ureg_MAD(ureg, r[0], ureg_scalar(a[0], TGSI_SWIZZLE_Y), c[1], r_src[0]);
    ureg_MAD(ureg, r[0], ureg_scalar(a[0], TGSI_SWIZZLE_Z), c[2], r_src[0]);
    ureg_MAD(ureg, r[0], ureg_scalar(a[0], TGSI_SWIZZLE_W), c[3], r_src[0]);
    ureg_MOV(ureg, oPos, r_src[0]);

    /* COLOR */
    ureg_MOV(ureg, oCol[0], a[1]);
    ureg_MOV(ureg, oCol[1], a[2]);

    /* TEXCOORD */
    ureg_MOV(ureg, oTex[0], a[3]);

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
nine_ff_build_ps(struct NineDevice9 *device)
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
    struct NineVertexShader9 *vs;

    if (!device->ff.vs) {
        NineVertexShader9_new(device, &vs, NULL, nine_ff_build_vs(device));
        vs->input_map[0].ndecl = NINE_DECLUSAGE_POSITION;
        vs->input_map[1].ndecl = NINE_DECLUSAGE_COLOR(0);
        vs->input_map[2].ndecl = NINE_DECLUSAGE_COLOR(1);
        vs->input_map[3].ndecl = NINE_DECLUSAGE_TEXCOORD(0);
        vs->num_inputs = 4;
        device->ff.vs = vs;
    }
    return device->ff.vs;
}

static struct NinePixelShader9 *
nine_ff_get_ps(struct NineDevice9 *device)
{
    struct NinePixelShader9 *ps;

    if (!device->ff.ps) {
        NinePixelShader9_new(device, &ps, NULL, nine_ff_build_ps(device));
        device->ff.ps = ps;
    }
    return device->ff.ps;
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

    nine_d3d_matrix_matrix_mul(&M, GET_D3DTS(VIEW), GET_D3DTS(PROJECTION));
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
        nine_reference(&device->state.vs, nine_ff_get_vs(device));
    if (!device->state.ps)
        nine_reference(&device->state.ps, nine_ff_get_ps(device));

    if (device->ff.vs &&
        (device->state.ff.changed.transform[0] |
         device->state.ff.changed.transform[256 / 32]))
        nine_ff_upload_vs_transforms(device);

    device->state.changed.group |= NINE_STATE_VS | NINE_STATE_PS;
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

#define _M_ADD3_PROD3(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r) \
    M->_##a##b * M->_##c##d * M->_##e##f +                 \
    M->_##g##h * M->_##i##j * M->_##k##l +                 \
    M->_##m##n * M->_##o##p * M->_##q##r
void
nine_d3d_matrix_inverse(D3DMATRIX *D, const D3DMATRIX *M)
{
    float s = nine_d3d_matrix_det(M);

    D->_11 = (_M_ADD3_PROD3(2,2, 3,3, 4,4,  2,3, 3,4, 4,2,  2,4, 3,2, 4,3) -
              _M_ADD3_PROD3(2,2, 3,4, 4,3,  2,3, 3,2, 4,4,  2,4, 3,3, 4,2)) / s;

    D->_12 = (_M_ADD3_PROD3(1,2, 3,4, 4,3,  1,3, 3,2, 4,4,  1,4, 3,3, 4,2) -
              _M_ADD3_PROD3(1,2, 3,3, 4,4,  1,3, 3,4, 4,2,  1,4, 3,2, 4,3)) / s;

    D->_13 = (_M_ADD3_PROD3(1,2, 2,3, 4,4,  1,3, 2,4, 4,2,  1,4, 2,2, 4,3) -
              _M_ADD3_PROD3(1,2, 2,4, 4,3,  1,3, 2,2, 4,4,  1,4, 2,3, 4,2)) / s;

    D->_14 = (_M_ADD3_PROD3(1,2, 2,4, 3,3,  1,3, 2,2, 3,4,  1,4, 2,3, 3,2) -
              _M_ADD3_PROD3(1,2, 2,3, 3,4,  1,3, 2,4, 3,2,  1,4, 2,2, 3,3)) / s;

    /* TODO ... */
    D->_21 = (_M_ADD3_PROD3(2,2, 3,3, 4,4,  2,3, 3,4, 4,2,  2,4, 3,2, 4,3) -
              _M_ADD3_PROD3(2,2, 3,4, 4,3,  2,3, 3,2, 4,4,  2,4, 3,3, 4,2)) / s;

    D->_22 = (_M_ADD3_PROD3(2,2, 3,3, 4,4,  2,3, 3,4, 4,2,  2,4, 3,2, 4,3) -
              _M_ADD3_PROD3(2,2, 3,4, 4,3,  2,3, 3,2, 4,4,  2,4, 3,3, 4,2)) / s;

    D->_23 = (_M_ADD3_PROD3(2,2, 3,3, 4,4,  2,3, 3,4, 4,2,  2,4, 3,2, 4,3) -
              _M_ADD3_PROD3(2,2, 3,4, 4,3,  2,3, 3,2, 4,4,  2,4, 3,3, 4,2)) / s;

    D->_24 = (_M_ADD3_PROD3(2,2, 3,3, 4,4,  2,3, 3,4, 4,2,  2,4, 3,2, 4,3) -
              _M_ADD3_PROD3(2,2, 3,4, 4,3,  2,3, 3,2, 4,4,  2,4, 3,3, 4,2)) / s;


    D->_31 = (_M_ADD3_PROD3(2,2, 3,3, 4,4,  2,3, 3,4, 4,2,  2,4, 3,2, 4,3) -
              _M_ADD3_PROD3(2,2, 3,4, 4,3,  2,3, 3,2, 4,4,  2,4, 3,3, 4,2)) / s;

    D->_32 = (_M_ADD3_PROD3(2,2, 3,3, 4,4,  2,3, 3,4, 4,2,  2,4, 3,2, 4,3) -
              _M_ADD3_PROD3(2,2, 3,4, 4,3,  2,3, 3,2, 4,4,  2,4, 3,3, 4,2)) / s;

    D->_33 = (_M_ADD3_PROD3(2,2, 3,3, 4,4,  2,3, 3,4, 4,2,  2,4, 3,2, 4,3) -
              _M_ADD3_PROD3(2,2, 3,4, 4,3,  2,3, 3,2, 4,4,  2,4, 3,3, 4,2)) / s;

    D->_34 = (_M_ADD3_PROD3(2,2, 3,3, 4,4,  2,3, 3,4, 4,2,  2,4, 3,2, 4,3) -
              _M_ADD3_PROD3(2,2, 3,4, 4,3,  2,3, 3,2, 4,4,  2,4, 3,3, 4,2)) / s;


    D->_41 = (_M_ADD3_PROD3(2,2, 3,3, 4,4,  2,3, 3,4, 4,2,  2,4, 3,2, 4,3) -
              _M_ADD3_PROD3(2,2, 3,4, 4,3,  2,3, 3,2, 4,4,  2,4, 3,3, 4,2)) / s;

    D->_42 = (_M_ADD3_PROD3(2,2, 3,3, 4,4,  2,3, 3,4, 4,2,  2,4, 3,2, 4,3) -
              _M_ADD3_PROD3(2,2, 3,4, 4,3,  2,3, 3,2, 4,4,  2,4, 3,3, 4,2)) / s;

    D->_43 = (_M_ADD3_PROD3(2,2, 3,3, 4,4,  2,3, 3,4, 4,2,  2,4, 3,2, 4,3) -
              _M_ADD3_PROD3(2,2, 3,4, 4,3,  2,3, 3,2, 4,4,  2,4, 3,3, 4,2)) / s;

    D->_44 = (_M_ADD3_PROD3(2,2, 3,3, 4,4,  2,3, 3,4, 4,2,  2,4, 3,2, 4,3) -
              _M_ADD3_PROD3(2,2, 3,4, 4,3,  2,3, 3,2, 4,4,  2,4, 3,3, 4,2)) / s;
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
