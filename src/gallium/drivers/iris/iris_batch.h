/*
 * Copyright © 2017 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef IRIS_BATCH_DOT_H
#define IRIS_BATCH_DOT_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "i915_drm.h"
#include "common/gen_decoder.h"

/* The kernel assumes batchbuffers are smaller than 256kB. */
#define MAX_BATCH_SIZE (256 * 1024)

/* Our target batch size - flush approximately at this point. */
#define BATCH_SZ (20 * 1024)

struct iris_address {
   struct iris_bo *bo;
   uint64_t offset;
   bool write;
};

struct iris_batch {
   struct iris_screen *screen;
   struct iris_vtable *vtbl;
   struct pipe_debug_callback *dbg;

   /** Current batchbuffer being queued up. */
   struct iris_bo *bo;
   void *map;
   void *map_next;
   /** Size of the primary batch if we've moved on to a secondary. */
   unsigned primary_batch_size;

   /** Last BO submitted to the hardware.  Used for glFinish(). */
   struct iris_bo *last_bo;

   /** Last Surface State Base Address set in this hardware context. */
   uint64_t last_surface_base_address;

   uint32_t hw_ctx_id;

   /** Which engine this batch targets - a I915_EXEC_RING_MASK value */
   uint8_t engine;

   /** The validation list */
   struct drm_i915_gem_exec_object2 *validation_list;
   struct iris_bo **exec_bos;
   int exec_count;
   int exec_array_size;

   /** The amount of aperture space (in bytes) used by all exec_bos */
   int aperture_space;

   struct {
      /**
       * Set of struct brw_bo * that have been rendered to within this
       * batchbuffer and would need flushing before being used from another
       * cache domain that isn't coherent with it (i.e. the sampler).
       */
      struct hash_table *render;

      /**
       * Set of struct brw_bo * that have been used as a depth buffer within
       * this batchbuffer and would need flushing before being used from
       * another cache domain that isn't coherent with it (i.e. the sampler).
       */
      struct set *depth;
   } cache;

   /** Map from batch offset to iris_alloc_state data (with DEBUG_BATCH) */
   // XXX: unused
   struct hash_table *state_sizes;
   struct gen_batch_decode_ctx decoder;

   /** Have we emitted any draw calls to this batch? */
   bool contains_draw;
};

void iris_init_batch(struct iris_batch *batch,
                     struct iris_screen *screen,
                     struct iris_vtable *vtbl,
                     struct pipe_debug_callback *dbg,
                     uint8_t ring);
void iris_chain_to_new_batch(struct iris_batch *batch);
void iris_batch_free(struct iris_batch *batch);
void iris_batch_maybe_flush(struct iris_batch *batch, unsigned estimate);

int _iris_batch_flush_fence(struct iris_batch *batch,
                            int in_fence_fd, int *out_fence_fd,
                            const char *file, int line);


#define iris_batch_flush_fence(batch, in_fence_fd, out_fence_fd) \
   _iris_batch_flush_fence((batch), (in_fence_fd), (out_fence_fd), \
                           __FILE__, __LINE__)

#define iris_batch_flush(batch) iris_batch_flush_fence((batch), -1, NULL)

bool iris_batch_references(struct iris_batch *batch, struct iris_bo *bo);

#define RELOC_WRITE EXEC_OBJECT_WRITE

void iris_use_pinned_bo(struct iris_batch *batch, struct iris_bo *bo,
                        bool writable);

static inline unsigned
iris_batch_bytes_used(struct iris_batch *batch)
{
   return batch->map_next - batch->map;
}

/**
 * Ensure the current command buffer has \param size bytes of space
 * remaining.  If not, this creates a secondary batch buffer and emits
 * a jump from the primary batch to the start of the secondary.
 *
 * Most callers want iris_get_command_space() instead.
 */
static inline void
iris_require_command_space(struct iris_batch *batch, unsigned size)
{
   const unsigned required_bytes = iris_batch_bytes_used(batch) + size;

   if (required_bytes >= BATCH_SZ) {
      iris_chain_to_new_batch(batch);
   }
}

/**
 * Allocate space in the current command buffer, and return a pointer
 * to the mapped area so the caller can write commands there.
 *
 * This should be called whenever emitting commands.
 */
static inline void *
iris_get_command_space(struct iris_batch *batch, unsigned bytes)
{
   iris_require_command_space(batch, bytes);
   void *map = batch->map_next;
   batch->map_next += bytes;
   return map;
}

/**
 * Helper to emit GPU commands - allocates space, copies them there.
 */
static inline void
iris_batch_emit(struct iris_batch *batch, const void *data, unsigned size)
{
   void *map = iris_get_command_space(batch, size);
   memcpy(map, data, size);
}

#endif