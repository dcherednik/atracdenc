#pragma once

#define CONFIG_DOUBLE

#ifdef CONFIG_DOUBLE
typedef double TFloat;
#else
typedef float TFloat;
#endif
