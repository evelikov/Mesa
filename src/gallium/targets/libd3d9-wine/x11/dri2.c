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

#include "dri2.h"

#include <X11/Xlibint.h>
#include <X11/Xproto.h>
#include <X11/extensions/extutil.h>
#include <X11/extensions/dri2proto.h>

#include <stdio.h>

static XExtensionInfo _dri2_info_data;
static XExtensionInfo *dri2_info = &_dri2_info_data;
static char dri2_name[] = DRI2_NAME;

#define DRI2CheckExtension(dpy, i, val) \
  XextCheckExtension(dpy, i, dri2_name, val)

static int
close_display( Display *dpy,
               XExtCodes *codes );

static Bool
wire_to_event( Display *dpy,
               XEvent *re,
               xEvent *event );

static Status
event_to_wire( Display *dpy,
               XEvent *re,
               xEvent *event );

static int
error( Display *dpy,
       xError *err,
       XExtCodes *codes,
       int *ret_code );

static XExtensionHooks dri2_hooks = {
  NULL,             /* create_gc */
  NULL,             /* copy_gc */
  NULL,             /* flush_gc */
  NULL,             /* free_gc */
  NULL,             /* create_font */
  NULL,             /* free_font */
  close_display,    /* close_display */
  wire_to_event,    /* wire_to_event */
  event_to_wire,    /* event_to_wire */
  error,            /* error */
  NULL,             /* error_string */
};

static XEXT_GENERATE_CLOSE_DISPLAY(close_display, dri2_info);
static XEXT_GENERATE_FIND_DISPLAY(find_display, dri2_info,
                                  dri2_name, &dri2_hooks, 0, NULL);


static Bool
wire_to_event( Display *dpy,
               XEvent *re,
               xEvent *event )
{
    XExtDisplayInfo *info = find_display(dpy);

    DRI2CheckExtension(dpy, info, False);

    printf("dri2 wire_to_event\n");

    return False;
}

static Status
event_to_wire( Display *dpy,
               XEvent *re,
               xEvent *event )
{
    XExtDisplayInfo *info = find_display(dpy);

    DRI2CheckExtension(dpy, info, False);

    printf("dri2 event_to_wire\n");

    return False;
}

static int
error( Display *dpy,
       xError *err,
       XExtCodes *codes,
       int *ret_code )
{
    /*if (err->majorCode == codes->major_opcode &&
        err->minorCode == X_DRI2Connect &&
        err->errorCode == BadRequest) {
	    *ret_code = False;
	    return True;
    }*/

    printf("dri2 error\n");

    return False;
}

/*** Actual API begins here ***/

Bool
DRI2QueryExtension( Display * dpy/* , int *event_basep, int *error_basep */ )
{
    XExtDisplayInfo *info = find_display(dpy);

    if (XextHasExtension(info)) {
        /**event_basep = info->codes->first_event;
        *error_basep = info->codes->first_error;*/
        return True;
    }

    return False;
}

Bool
DRI2QueryVersion( Display *dpy,
                  unsigned *major,
                  unsigned *minor )
{
    static const int nevents[] = { 0, 0, 1, 2 };
    static const int nev = sizeof(nevents)/sizeof(*nevents) - 1;

    XExtDisplayInfo *info = find_display(dpy);
    xDRI2QueryVersionReply rep;
    xDRI2QueryVersionReq *req;
    int i, e;

    DRI2CheckExtension(dpy, info, False);

    LockDisplay(dpy);
    GetReq(DRI2QueryVersion, req);
    req->reqType = info->codes->major_opcode;
    req->dri2ReqType = X_DRI2QueryVersion;
    req->majorVersion = *major;
    req->minorVersion = *minor;
    if (!_XReply(dpy, (xReply *)&rep, 0, xFalse)) {
        UnlockDisplay(dpy);
        SyncHandle();
        return False;
    }
    *major = rep.majorVersion;
    *minor = rep.minorVersion;
    UnlockDisplay(dpy);
    SyncHandle();

    e = (rep.minorVersion < nev) ? nevents[rep.minorVersion] : nevents[nev];
    for (i = 0; i < e; ++i) {
        XESetWireToEvent(dpy, info->codes->first_event + i, wire_to_event);
        XESetEventToWire(dpy, info->codes->first_event + i, event_to_wire);
    }

    return True;
}

#define XALIGN(x) (((x) + 3) & (~3))

Bool
DRI2Connect( Display *dpy,
             XID window,
             unsigned driver_type,
             char **driver,
             char **device )
{
    XExtDisplayInfo *info = find_display(dpy);
    xDRI2ConnectReply rep;
    xDRI2ConnectReq *req;
    int dev_len, driv_len;

    DRI2CheckExtension(dpy, info, False);

    LockDisplay(dpy);
    GetReq(DRI2Connect, req);
    req->reqType = info->codes->major_opcode;
    req->dri2ReqType = X_DRI2Connect;
    req->window = window;
    req->driverType = driver_type;
    if (!_XReply(dpy, (xReply *)&rep, 0, xFalse)) {
        UnlockDisplay(dpy);
        SyncHandle();
        return False;
    }

    /* check string lengths */
    dev_len = rep.deviceNameLength;
    driv_len = rep.driverNameLength;
    if (dev_len == 0 || driv_len == 0) {
        _XEatData(dpy, XALIGN(dev_len) + XALIGN(driv_len));
        UnlockDisplay(dpy);
        SyncHandle();
        return False;
    }

    /* read out driver */
    *driver = malloc(driv_len + 1);
    if (!*driver) {
        _XEatData(dpy, XALIGN(dev_len) + XALIGN(driv_len));
        UnlockDisplay(dpy);
        SyncHandle();
        return False;
    }
    _XReadPad(dpy, *driver, driv_len);
    (*driver)[driv_len] = '\0';

    /* read out device */
    *device = malloc(dev_len + 1);
    if (!*device) {
        free(*driver);
        _XEatData(dpy, XALIGN(dev_len));
        UnlockDisplay(dpy);
        SyncHandle();
        return False;
    }
    _XReadPad(dpy, *device, dev_len);
    (*device)[dev_len] = '\0';

    UnlockDisplay(dpy);
    SyncHandle();

    return True;
}

Bool
DRI2Authenticate( Display *dpy,
                  XID window,
                  uint32_t token )
{
    XExtDisplayInfo *info = find_display(dpy);
    xDRI2AuthenticateReply rep;
    xDRI2AuthenticateReq *req;

    DRI2CheckExtension(dpy, info, False);

    LockDisplay(dpy);
    GetReq(DRI2Authenticate, req);
    req->reqType = info->codes->major_opcode;
    req->dri2ReqType = X_DRI2Authenticate;
    req->window = window;
    req->magic = token;
    if (!_XReply(dpy, (xReply *)&rep, 0, xFalse)) {
        UnlockDisplay(dpy);
        SyncHandle();
        return False;
    }
    UnlockDisplay(dpy);
    SyncHandle();

    return rep.authenticated ? True : False;
}

void
DRI2CreateDrawable( Display *dpy,
                    XID drawable )
{
    XExtDisplayInfo *info = find_display(dpy);
    xDRI2CreateDrawableReq *req;

    DRI2CheckExtension(dpy, info, );

    LockDisplay(dpy);
    GetReq(DRI2CreateDrawable, req);
    req->reqType = info->codes->major_opcode;
    req->dri2ReqType = X_DRI2CreateDrawable;
    req->drawable = drawable;
    UnlockDisplay(dpy);
    SyncHandle();
}

void
DRI2DestroyDrawable( Display *dpy,
                     XID drawable )
{
    XExtDisplayInfo *info = find_display(dpy);
    xDRI2DestroyDrawableReq *req;

    DRI2CheckExtension(dpy, info, );

    LockDisplay(dpy);
    GetReq(DRI2DestroyDrawable, req);
    req->reqType = info->codes->major_opcode;
    req->dri2ReqType = X_DRI2DestroyDrawable;
    req->drawable = drawable;
    UnlockDisplay(dpy);
    SyncHandle();
}

static unsigned
get_buffers( Display *dpy,
             unsigned reqtype,
             XID drawable,
             const unsigned *attachments,
             unsigned attach_count,
             unsigned *width,
             unsigned *height,
             DRI2Buffer **buffers )
{
    XExtDisplayInfo *info = find_display(dpy);
    xDRI2GetBuffersReply rep;
    xDRI2GetBuffersReq *req;
    xDRI2Buffer buf;
    unsigned i, mult = (reqtype == X_DRI2GetBuffersWithFormat) ? 2 : 1;

    DRI2CheckExtension(dpy, info, False);

    LockDisplay(dpy);
    GetReqExtra(DRI2GetBuffers, attach_count*sizeof(CARD32)*mult, req);
    req->reqType = info->codes->major_opcode;
    req->dri2ReqType = reqtype;
    req->drawable = drawable;
    req->count = attach_count;
    /* copy in attachments */
    for (i = 0; i < attach_count*mult; ++i) {
        ((CARD32 *)&req[1])[i] = attachments[i];
    }
    if (!_XReply(dpy, (xReply *)&rep, 0, xFalse)) {
        UnlockDisplay(dpy);
        SyncHandle();
        return False;
    }

    if (rep.count != 0) {
        *buffers = malloc(rep.count * sizeof(DRI2Buffer));
        if (!*buffers) {
            _XEatData(dpy, rep.count * sizeof(buf));
            UnlockDisplay(dpy);
            SyncHandle();
            return False;
        }

        for (i = 0; i < rep.count; ++i) {
            _XReadPad(dpy, (char *)&buf, sizeof(buf));
            (*buffers)[i].attachment = buf.attachment;
            (*buffers)[i].name = buf.name;
            (*buffers)[i].pitch = buf.pitch;
            (*buffers)[i].cpp = buf.cpp;
            (*buffers)[i].flags = buf.flags;
        }
    }

    *width = rep.width;
    *height = rep.height;

    UnlockDisplay(dpy);
    SyncHandle();

    return rep.count;
}

unsigned
DRI2GetBuffers( Display *dpy,
                XID drawable,
                const unsigned *attachments,
                unsigned attach_count,
                unsigned *width,
                unsigned *height,
                DRI2Buffer **buffers )
{
    return get_buffers(dpy, X_DRI2GetBuffers, drawable, attachments,
                       attach_count, width, height, buffers);
}

unsigned
DRI2GetBuffersWithFormat( Display *dpy,
                          XID drawable,
                          const unsigned *attachments,
                          unsigned attach_count,
                          unsigned *width,
                          unsigned *height,
                          DRI2Buffer **buffers )
{
    return get_buffers(dpy, X_DRI2GetBuffersWithFormat, drawable, attachments,
                       attach_count, width, height, buffers);
}

Bool
DRI2CopyRegion( Display *dpy,
                XID drawable,
                XserverRegion region,
                unsigned dest,
                unsigned src )
{
    XExtDisplayInfo *info = find_display(dpy);
    xDRI2CopyRegionReply rep;
    xDRI2CopyRegionReq *req;

    DRI2CheckExtension(dpy, info, False);

    LockDisplay(dpy);
    GetReq(DRI2CopyRegion, req);
    req->reqType = info->codes->major_opcode;
    req->dri2ReqType = X_DRI2CopyRegion;
    req->drawable = drawable;
    req->region = region;
    req->dest = dest;
    req->src = src;
    if (!_XReply(dpy, (xReply *)&rep, 0, xFalse)) {
        UnlockDisplay(dpy);
        SyncHandle();
        return False;
    }
    UnlockDisplay(dpy);
    SyncHandle();

    return True;
}
