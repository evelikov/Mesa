
#include "device9.h"
#include "nine_ff.h"

#define DBG_CHANNEL DBG_FF

void
nine_ff_update(struct NineDevice9 *device)
{
    DBG("Error: FF not implemented.\n");
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
 * t.xyzw = MAD(v.yyyy, r[0], t.xyzw);
 * t.xyzw = MAD(v.zzzz, r[0], t.xyzw);
 * v.xyzw = MAD(v.wwww, r[0], t.xyzw);
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
