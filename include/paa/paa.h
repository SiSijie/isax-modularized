//
// Created by Qitong Wang on 2020/6/29.
//

#ifndef ISAX_PAA_H
#define ISAX_PAA_H

#include <stdlib.h>
#include <pthread.h>

#include "globals.h"

Value *piecewiseAggregate(Value const *values, size_t size, size_t series_length, size_t summarization_length,
                          int num_threads);

#endif //ISAX_PAA_H
