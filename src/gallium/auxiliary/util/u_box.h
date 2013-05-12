#ifndef UTIL_BOX_INLINES_H
#define UTIL_BOX_INLINES_H

#include "pipe/p_state.h"

static INLINE
void u_box_1d( unsigned x,
	       unsigned w,
	       struct pipe_box *box )
{
   box->x = x;
   box->y = 0;
   box->z = 0;
   box->width = w;
   box->height = 1;
   box->depth = 1;
}

static INLINE
void u_box_2d( unsigned x,
	       unsigned y,
	       unsigned w,
	       unsigned h,
	       struct pipe_box *box )
{
   box->x = x;
   box->y = y;
   box->z = 0;
   box->width = w;
   box->height = h;
   box->depth = 1;
}

static INLINE
void u_box_origin_2d( unsigned w,
		      unsigned h,
		      struct pipe_box *box )
{
   box->x = 0;
   box->y = 0;
   box->z = 0;
   box->width = w;
   box->height = h;
   box->depth = 1;
}

static INLINE
void u_box_2d_zslice( unsigned x,
		      unsigned y,
		      unsigned z,
		      unsigned w,
		      unsigned h,
		      struct pipe_box *box )
{
   box->x = x;
   box->y = y;
   box->z = z;
   box->width = w;
   box->height = h;
   box->depth = 1;
}

static INLINE
void u_box_3d( unsigned x,
	       unsigned y,
	       unsigned z,
	       unsigned w,
	       unsigned h,
	       unsigned d,
	       struct pipe_box *box )
{
   box->x = x;
   box->y = y;
   box->z = z;
   box->width = w;
   box->height = h;
   box->depth = d;
}

/* Returns whether @a is contained in or equal to @b. */
static INLINE
boolean u_box_contained(struct pipe_box *a, struct pipe_box *b)
{
   return
      a->x >= b->x && (a->x + a->width  <= b->x + b->width) &&
      a->y >= b->y && (a->y + a->height <= b->y + b->height) &&
      a->z >= b->z && (a->z + a->depth  <= b->z + b->depth);
}

/* Clips @box to width @w and height @h.
 * Returns -1 if the resulting box would be empty (then @box is left unchanged).
 * Otherwise, returns 1/2/0/3 if width/height/neither/both have been reduced.
 */
static INLINE
int u_box_clip_2d(struct pipe_box *box, unsigned w, unsigned h)
{
   unsigned max_w = w - box->x;
   unsigned max_h = h - box->y;
   int res = 0;
   if (box->x >= w || box->y >= h)
      return -1;
   if (box->width  > max_w) { res |= 1; box->width  = max_w; }
   if (box->height > max_h) { res |= 2; box->height = max_h; }
   return res;
}

#endif
