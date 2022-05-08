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

#ifndef LIB_ENDIAN_H
#define LIB_ENDIAN_H

#include <stdint.h>

static inline uint32_t swapbyte32_on_le(uint32_t in) {
#ifdef BIGENDIAN_ORDER
    return in;
#else
    return ((in & 0xff) << 24) | ((in & 0xff00) << 8) | ((in & 0xff0000) >> 8) | ((in & 0xff000000) >> 24);
#endif
}

static inline uint16_t swapbyte16_on_le(uint16_t in) {
#ifdef BIGENDIAN_ORDER
    return in;
#else
    return ((in & 0xff) << 8) | ((in & 0xff00) >> 8);
#endif
}

static inline uint32_t swapbyte32_on_be(uint32_t in) {
#ifdef BIGENDIAN_ORDER
    return ((in & 0xff) << 24) | ((in & 0xff00) << 8) | ((in & 0xff0000) >> 8) | ((in & 0xff000000) >> 24);
#else
    return in;
#endif
}

static inline uint16_t swapbyte16_on_be(uint16_t in) {
#ifdef BIGENDIAN_ORDER
    return ((in & 0xff) << 8) | ((in & 0xff00) >> 8);
#else
    return in;
#endif
}

#ifdef __cplusplus

static inline int16_t conv_ntoh(int16_t in) {
    return (int16_t)swapbyte16_on_le((uint16_t)in);
}

static inline uint16_t conv_ntoh(uint16_t in) {
    return swapbyte16_on_le(in);
}

static inline int32_t conv_ntoh(int32_t in) {
    return (int32_t)swapbyte32_on_le((uint32_t)in);
}

static inline uint32_t conv_ntoh(uint32_t in) {
    return swapbyte32_on_le(in);
}

#endif

#endif
