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
