#ifndef UTIL_BOX_INLINES_H
#define UTIL_BOX_INLINES_H

#include "pipe/p_state.h"
#include "util/u_math.h"

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
int u_box_clip_2d(struct pipe_box *box, int w, int h)
{
   int max_w = w - box->x;
   int max_h = h - box->y;
   int res = 0;
   if (box->x >= w || box->y >= h)
      return -1;
   if (box->width  > max_w) { res |= 1; box->width  = max_w; }
   if (box->height > max_h) { res |= 2; box->height = max_h; }
   return res;
}

/* Return true if @a is contained in or equal to @b.
 */
static INLINE
boolean u_box_contained_2d(const struct pipe_box *a, const struct pipe_box *b)
{
   int a_x1 = a->x + a->width;
   int b_x1 = b->x + b->width;
   int a_y1 = a->y + a->height;
   int b_y1 = b->y + b->height;
   return
      a->x >= b->x && a_x1 <= b_x1 &&
      a->y >= b->y && a_y1 <= b_y1;
}

static INLINE
int64_t u_box_volume(const struct pipe_box *box)
{
   return (int64_t)box->width * box->height * box->depth;
}

static INLINE
void u_box_cover(struct pipe_box *dst,
                 const struct pipe_box *a, const struct pipe_box *b)
{
   int x1_a = a->x + a->width;
   int y1_a = a->y + a->height;
   int z1_a = a->z + a->depth;
   int x1_b = b->x + b->width;
   int y1_b = b->y + b->height;
   int z1_b = b->z + b->depth;

   dst->x = MIN2(a->x, b->x);
   dst->y = MIN2(a->y, b->y);
   dst->z = MIN2(a->z, b->z);

   dst->width = MAX2(x1_a, x1_b) - dst->x;
   dst->height = MAX2(y1_a, y1_b) - dst->y;
   dst->depth = MAX2(z1_a, z1_b) - dst->z;
}

#endif
