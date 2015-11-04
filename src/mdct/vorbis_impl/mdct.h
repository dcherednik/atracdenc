/**
 *
 * This file is derived from libvorbis
 * Copyright (c) 2002, Xiph.org Foundation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * - Neither the name of the Xiph.org Foundation nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file mdct.h
 * MDCT header
 */

#ifndef MDCT_H
#define MDCT_H

#include "../common.h"

#if defined(HAVE_MMX) || defined(HAVE_SSE)
#include "x86/mdct.h"
#endif
#ifdef HAVE_ALTIVEC
#include "ppc/mdct.h"
#endif

#define ONE FCONST(1.0)
#define TWO FCONST(2.0)
#define AFT_PI3_8 FCONST(0.38268343236508977175)
#define AFT_PI2_8 FCONST(0.70710678118654752441)
#define AFT_PI1_8 FCONST(0.92387953251128675613)

struct A52Context;
struct A52ThreadContext;

typedef struct MDCTContext {
    void (*mdct)(struct A52ThreadContext *ctx, FLOAT *out, FLOAT *in);
    void (*mdct_bitreverse)(struct MDCTContext *mdct, FLOAT *x);
    void (*mdct_butterfly_generic)(struct MDCTContext *mdct, FLOAT *x, int points, int trigint);
    void (*mdct_butterfly_first)(FLOAT *trig, FLOAT *x, int points);
    void (*mdct_butterfly_32)(FLOAT *x);
    FLOAT *trig;
#ifndef CONFIG_DOUBLE
#ifdef HAVE_SSE
    FLOAT *trig_bitreverse;
    FLOAT *trig_forward;
    FLOAT *trig_butterfly_first;
    FLOAT *trig_butterfly_generic8;
    FLOAT *trig_butterfly_generic16;
    FLOAT *trig_butterfly_generic32;
    FLOAT *trig_butterfly_generic64;
#endif
#endif /* CONFIG_DOUBLE */
    int *bitrev;
    FLOAT scale;
    int n;
    int log2n;
} MDCTContext;

#ifdef __cplusplus
extern "C" {
#endif
void mdct_ctx_init(MDCTContext *mdct, int n, FLOAT scale);
void mdct_ctx_close(MDCTContext *mdct);
void mdct(MDCTContext *mdct, FLOAT *out, FLOAT *in);
#ifdef __cplusplus
}

#endif
#endif /* MDCT_H */
