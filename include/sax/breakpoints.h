//
// Created by Qitong Wang on 2020/6/28.
//

#ifndef ISAX_BREAKPOINTS_H
#define ISAX_BREAKPOINTS_H

#include <stdlib.h>
#include <pthread.h>

#include "globals.h"
#include "config.h"


static unsigned int const OFFSETS_BY_CARDINALITY[10] = {
        0, // padding
        0, 1, 4, 11, 26, 57, 120, 247,
        502 // indicate breakpoints(8) length
};


static unsigned int const OFFSETS_BY_MASK[129] = {
        0, 247, 120, 0, 57, 0, 0, 0, 26, 0, 0, 0, 0, 0, 0, 0, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0
};


Value *getNormalBreakpoints8();

Value **adhocBreakpoints(Value const *summarizations, size_t size, size_t num_segments, unsigned int cardinality,
                         int num_threads);

#endif //ISAX_BREAKPOINTS_H
