/*
 * Copyright © 2014 Olivier Galibert & Intel Corporation
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
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <stdlib.h>
#include <string.h>

#include "sha1.h"

static inline unsigned int
mesa_sha1_shift(unsigned int val, int count)
{
    return (val << count) | (val >> (32 - count));
}

void
_mesa_sha1_init(struct mesa_sha1 *ctx)
{
    ctx->digest[0] = 0x67452301;
    ctx->digest[1] = 0xefcdab89;
    ctx->digest[2] = 0x98badcfe;
    ctx->digest[3] = 0x10325476;
    ctx->digest[4] = 0xc3d2e1f0;
    ctx->msize = 0;
}

static void
mesa_sha1_handle_block(struct mesa_sha1 *ctx, const unsigned char *b)
{
    unsigned int W[80];
    unsigned int i;

    for (i = 0; i != 16; i++)
        W[i] = (b[4 * i] << 24) | (b[4 * i + 1] << 16) | (b[4 * i + 2] << 8) | b[4 * i + 3];

    for (i = 16; i != 80; i++)
        W[i] = mesa_sha1_shift(W[i - 3] ^ W[i - 8] ^ W[i - 14] ^ W[i - 16], 1);

    unsigned int A = ctx->digest[0];
    unsigned int B = ctx->digest[1];
    unsigned int C = ctx->digest[2];
    unsigned int D = ctx->digest[3];
    unsigned int E = ctx->digest[4];

    for (i = 0; i != 20; i++) {
        unsigned int T = mesa_sha1_shift(A, 5) + ((B & C) | ((~B) & D))        + E + W[i] + 0x5A827999;
        E = D;
        D = C;
        C = mesa_sha1_shift(B, 30);
        B = A;
        A = T;
    }

    for (i = 20; i != 40; i++) {
        unsigned int T = mesa_sha1_shift(A, 5) + (B ^ C ^ D)                   + E + W[i] + 0x6ed9eba1;
        E = D;
        D = C;
        C = mesa_sha1_shift(B, 30);
        B = A;
        A = T;
    }

    for (i = 40; i != 60; i++) {
        unsigned int T = mesa_sha1_shift(A, 5) + ((B & C) | (B & D) | (C & D)) + E + W[i] + 0x8f1bbcdc;
        E = D;
        D = C;
        C = mesa_sha1_shift(B, 30);
        B = A;
        A = T;
    }

    for (i = 60; i != 80; i++) {
        unsigned int T = mesa_sha1_shift(A, 5) + (B ^ C ^ D)                   + E + W[i] + 0xca62c1d6;
        E = D;
        D = C;
        C = mesa_sha1_shift(B, 30);
        B = A;
        A = T;
    }

    ctx->digest[0] += A;
    ctx->digest[1] += B;
    ctx->digest[2] += C;
    ctx->digest[3] += D;
    ctx->digest[4] += E;
}

void
_mesa_sha1_final(struct mesa_sha1 *ctx, unsigned char result[20])
{
    unsigned int offset = ctx->msize & 63;

    ctx->block[offset] = 0x80;
    memset(ctx->block + offset + 1, 0, 64 - offset - 1);
    if (offset > 55) {
        mesa_sha1_handle_block(ctx, ctx->block);
        memset(ctx->block, 0, 64);
    }

    ctx->block[59] = ctx->msize >> 29;
    ctx->block[60] = ctx->msize >> 21;
    ctx->block[61] = ctx->msize >> 13;
    ctx->block[62] = ctx->msize >> 5;
    ctx->block[63] = ctx->msize << 3;
    mesa_sha1_handle_block(ctx, ctx->block);

    for (unsigned int i = 0; i < 20; i++)
	result[i] = ctx->digest[i >> 2] >> (28 - 8 * (i & 3));
}

int
_mesa_sha1_update(struct mesa_sha1 *ctx, const void *data, int size)
{
    const unsigned char *pdata = (const unsigned char *)data;
    unsigned int offset = ctx->msize & 63;

    ctx->msize += size;
    if (offset) {
        if (offset + size >= 64) {
            memcpy(ctx->block + offset, pdata, 64 - offset);
            mesa_sha1_handle_block(ctx, ctx->block);
            pdata += offset;
            size -= offset;
        }
    }

    while (size >= 64) {
        mesa_sha1_handle_block(ctx, pdata);
        pdata += 64;
        size -= 64;
    }

    if (size)
        memcpy(ctx->block, pdata, size);

    return 1;
}

void
_mesa_sha1_compute(const void *data, size_t size, unsigned char result[20])
{
    struct mesa_sha1 ctx;

    _mesa_sha1_init(&ctx);
    _mesa_sha1_update(&ctx, data, size);
    _mesa_sha1_final(&ctx, result);
}

char *
_mesa_sha1_format(char *buf, const unsigned char *sha1)
{
    static const char hex_digits[] = "0123456789abcdef";
    int i;

    for (i = 0; i < 40; i += 2) {
        buf[i] = hex_digits[sha1[i >> 1] >> 4];
        buf[i + 1] = hex_digits[sha1[i >> 1] & 0x0f];
    }
    buf[i] = '\0';

    return buf;
}
