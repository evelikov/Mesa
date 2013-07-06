/*
 * Copyright 2013 Joakim Sindholt <opensource@zhasha.com>
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

#include "adjust.h"

#include <wine/server.h>

#include <d3d9types.h>

#include "../debug.h"

int
get_winex11_hwnd_offset( HWND real,
                         HWND anc,
                         RECT *rect )
{
    int x = 0, y = 0;

    /* get size of the HWND we want to draw to */
    if (!GetClientRect(real, rect)) {
        _ERROR("Unable to get window size.\n");
        return FALSE;
    }

    /* Get the offset caused by non-client area into the real X drawable */
    SERVER_START_REQ(get_window_rectangles)
    {
        req->handle = wine_server_user_handle(anc);
        req->relative = COORDS_CLIENT;
        wine_server_call(req);

        x += reply->visible.left;
        y += reply->visible.top;
    }
    SERVER_END_REQ;

    /* Get the offset caused by client area into the real X drawable */
    if (anc != real) {
        POINT off = {0, 0};
        if (!MapWindowPoints(anc, real, &off, 1)) {
            _ERROR("Unable to get coordinates for fake subwindow.\n");
            return FALSE;
        }
        x += off.x;
        y += off.y;
    }

    _MESSAGE("Adjusting RECT (%u..%u)x(%u..%u) to winex11 system (%+d,%+d)\n",
             rect->left, rect->right, rect->top, rect->bottom, -x, -y);

    rect->left -= x;
    rect->top -= y;
    rect->right -= x;
    rect->bottom -= y;

    return TRUE;
}
