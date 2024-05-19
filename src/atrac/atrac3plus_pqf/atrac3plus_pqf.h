/*
 * This file is part of AtracDEnc.
 *
 * AtracDEnc is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * AtracDEnc is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with AtracDEnc; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef ATRAC3PLUSPQF_H
#define ATRAC3PLUSPQF_H

#include <stdint.h>

typedef struct at3plus_pqf_a_ctx *at3plus_pqf_a_ctx_t;

#ifdef __cplusplus
extern "C" {
#endif

at3plus_pqf_a_ctx_t at3plus_pqf_create_a_ctx(void);
void at3plus_pqf_free_a_ctx(at3plus_pqf_a_ctx_t ctx);
void at3plus_pqf_do_analyse(at3plus_pqf_a_ctx_t ctx, const float* in, float* out);

#ifdef __cplusplus
}
#endif

#endif
