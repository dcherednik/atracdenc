#pragma once

#define CONFIG_DOUBLE

#ifdef CONFIG_DOUBLE
#    define kiss_fft_scalar double
typedef double TFloat;
#else
#    define kiss_fft_scalar float
typedef float TFloat;
#endif


#ifndef M_PI
#define M_PI (3.14159265358979323846)
#endif
