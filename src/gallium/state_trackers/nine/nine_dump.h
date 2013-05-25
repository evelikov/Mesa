
#ifndef _NINE_DUMP_H_
#define _NINE_DUMP_H_

#include "d3d9types.h"

const char *nine_D3DDEVTYPE_to_str(D3DDEVTYPE);

#ifdef DEBUG

void
nine_dump_D3DADAPTER_IDENTIFIER9(unsigned, const D3DADAPTER_IDENTIFIER9 *);

#else /* !DEBUG */

static INLINE void
nine_dump_D3DADAPTER_IDENTIFIER9(unsigned, const D3DADAPTER_IDENTIFIER9 *)
{ }

#endif /* DEBUG */

#endif /* _NINE_DUMP_H_H_ */
