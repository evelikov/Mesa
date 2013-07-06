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

#ifndef __DRI2_H__
#define __DRI2_H__

#include <X11/Xlib.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/dri2tokens.h>

#include <stdint.h>

typedef struct
{
   unsigned attachment;
   unsigned name;
   unsigned pitch;
   unsigned cpp;
   unsigned flags;
} DRI2Buffer;

Bool
DRI2QueryExtension( Display * dpy/* , int *event_basep, int *error_basep */ );

Bool
DRI2QueryVersion( Display *dpy,
                  unsigned *major,
                  unsigned *minor );

Bool
DRI2Connect( Display *dpy,
             XID window,
             unsigned driver_type,
             char **driver,
             char **device );

Bool
DRI2Authenticate( Display *dpy,
                  XID window,
                  uint32_t token );

void
DRI2CreateDrawable( Display *dpy,
                    XID drawable );

void
DRI2DestroyDrawable( Display *dpy,
                     XID drawable );

unsigned
DRI2GetBuffers( Display *dpy,
                XID drawable,
                const unsigned *attachments,
                unsigned attach_count,
                unsigned *width,
                unsigned *height,
                DRI2Buffer **buffers );

unsigned
DRI2GetBuffersWithFormat( Display *dpy,
                          XID drawable,
                          const unsigned *attachments,
                          unsigned attach_count,
                          unsigned *width,
                          unsigned *height,
                          DRI2Buffer **buffers );

Bool
DRI2CopyRegion( Display *dpy,
                XID drawable,
                XserverRegion region,
                unsigned dest,
                unsigned src );

#endif /* __DRI2_H__ */
