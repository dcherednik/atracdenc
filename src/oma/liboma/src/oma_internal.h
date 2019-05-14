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

#ifndef OMA_INTERNAL_H
#define OMA_INTERNAL_H

#include <stdio.h>
#include "oma.h"

struct omafile_ctx {
    FILE* file;
    oma_info_t info;
};



//static inline uint16_t read_big16(void *x) {
//    return (((const uint8_t*)x)[0] << 8) | ((const uint8_t)x);
//}

#endif /* OMA_INTERNAL_H */
