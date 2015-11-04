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
