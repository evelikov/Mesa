#ifndef _INLINE_NINE_REF_HELPER_H_
#define _INLINE_NINE_REF_HELPER_H_

#include "sw/wrapper/wrapper_sw_winsys.h"

#ifdef GALLIUM_SOFTPIPE
#include "softpipe/sp_public.h"
#endif

#ifdef GALLIUM_LLVMPIPE
#include "llvmpipe/lp_public.h"
#endif

/* silly prototype to avoid warning */
struct pipe_screen *
create_screen_ref(struct pipe_screen *hal);

/* create a ref screen based on a hal screen */
struct pipe_screen *
create_screen_ref(struct pipe_screen *hal)
{
    struct sw_winsys *sws;
    struct pipe_screen *ref = NULL;

    sws = wrapper_sw_winsys_wrap_pipe_screen(hal);
    if (!sws) { return NULL; }

#if defined(GALLIUM_LLVMPIPE)
    ref = llvmpipe_create_screen(sws);
#elif defined(GALLIUM_SOFTPIPE)
    ref = softpipe_create_screen(sws);
#endif
    if (!ref) { wrapper_sw_winsys_dewrap_pipe_screen(sws); }

    return ref;
}

#endif /* _INLINE_NINE_REF_HELPER_H_ */
