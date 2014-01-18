#ifndef _NINE_SHADER_H_
#define _NINE_SHADER_H_

#include "tgsi/tgsi_ureg.h"

#include "d3d9types.h"

union d3d_token;

struct util_hash_table;

struct nine_shader
{
    struct {
        struct ureg_program *program;
        unsigned processor;
    } tgsi;

    struct {
        DWORD *original;
        unsigned noriginal;

        union d3d_token *ir;
        unsigned nir;
    } tokens;

    /* when using relative addressing, everything (might be) a gamble */
    boolean reladdr;
    /* what ints, bools and floats are used */
    uint_least16_t const_int_mask;
    uint_least16_t const_bool_mask;
    uint_least32_t const_float_mask[8];

    struct {
        int val;
        unsigned location;
    } const_ints[16];
    struct {
        boolean val;
        unsigned location;
    } const_bools[16];

    float *const_floats; /* dynamically allocated */
};

HRESULT
nine_translate_shader( DWORD *tokens,
                       unsigned processor,
                       struct ureg_program **program );

#endif /* _NINE_SHADER_H_ */
