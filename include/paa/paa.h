//
// Created by Qitong Wang on 2020/6/29.
//

#ifndef ISAX_PAA_H
#define ISAX_PAA_H

#include <stdlib.h>
#include <pthread.h>

#include "clog.h"
#include "globals.h"


Value *piecewiseAggregate(Value const *values, ID size, unsigned int series_length, unsigned int summarization_length,
                          unsigned int num_threads);

#endif //ISAX_PAA_H
