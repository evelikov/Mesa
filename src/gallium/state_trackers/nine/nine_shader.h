#ifndef _NINE_SHADER_H_
#define _NINE_SHADER_H_

#include "d3d9types.h"
#include "d3d9caps.h"
#include "pipe/p_state.h" /* PIPE_MAX_ATTRIBS */

#define NINE_DECLUSAGE_POSITION       0
#define NINE_DECLUSAGE_BLENDWEIGHT    1
#define NINE_DECLUSAGE_BLENDINDICES   2
#define NINE_DECLUSAGE_NORMAL(i)    ( 3 + (i))
#define NINE_DECLUSAGE_PSIZE          5
#define NINE_DECLUSAGE_TEXCOORD(i)  ( 6 + (i))
#define NINE_DECLUSAGE_TANGENT       15
#define NINE_DECLUSAGE_BINORMAL      16
#define NINE_DECLUSAGE_TESSFACTOR    17
#define NINE_DECLUSAGE_POSITIONT     18
#define NINE_DECLUSAGE_COLOR(i)     (19 + (i))
#define NINE_DECLUSAGE_DEPTH         22
#define NINE_DECLUSAGE_SAMPLE        23

struct NineDevice9;

struct nine_shader_info
{
    unsigned type; /* in, PIPE_SHADER_x */

    const DWORD *byte_code; /* in, pointer to shader tokens */
    DWORD        byte_size; /* out, size of data at byte_code */

    void *cso; /* out, pipe cso for bind_vs,fs_state */

    unsigned input_map[32]; /* VS input -> NINE_DECLUSAGE_x */
};

HRESULT
nine_translate_shader(struct NineDevice9 *device, struct nine_shader_info *);

#endif /* _NINE_SHADER_H_ */
