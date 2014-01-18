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

#ifndef _LIBD3D9_WINE_PRESENT_H_
#define _LIBD3D9_WINE_PRESENT_H_

#include "d3d9.h"
#include "d3dpresent.h"

#undef _WIN32
#include <xcb/xcb.h>
#define _WIN32

HRESULT
NineWinePresentFactoryX11_new( xcb_connection_t *c,
                               HWND focus_wnd,
                               D3DPRESENT_PARAMETERS *params,
                               unsigned nparams,
                               unsigned dri2_major,
                               unsigned dri2_minor,
                               ID3DPresentFactory **out );

xcb_drawable_t
X11DRV_ExtEscape_GET_DRAWABLE( HDC hdc );

#endif /* _LIBD3D9_WINE_PRESENT_H_ */
