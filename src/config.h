#pragma once

#define CONFIG_DOUBLE

#ifdef CONFIG_DOUBLE
typedef double TFloat;
#else
typedef float TFloat;
#endif


#ifndef M_PI
#define M_PI (3.14159265358979323846)
#endif
