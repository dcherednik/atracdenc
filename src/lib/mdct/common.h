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

#pragma once
#include "../config.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef CONFIG_DOUBLE
typedef double FLOAT;
#define FCONST(X) (X)
#define AFT_COS cos
#define AFT_SIN sin
#define AFT_TAN tan
#define AFT_LOG10 log10
#define AFT_EXP exp
#define AFT_FABS fabs
#define AFT_SQRT sqrt
#define AFT_EXP exp
#else
typedef float FLOAT;
#define FCONST(X) (X##f)
#define AFT_COS cosf
#define AFT_SIN sinf
#define AFT_TAN tanf
#define AFT_LOG10 log10f
#define AFT_FABS fabsf
#define AFT_SQRT sqrtf
#define AFT_EXP expf
#endif
