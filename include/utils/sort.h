//
// Created by Qitong Wang on 2020/7/16.
//

#ifndef ISAX_SORT_H
#define ISAX_SORT_H

#include <stdbool.h>

#include "globals.h"
#include "clog.h"


unsigned int bSearchFloor(Value value, Value const *sorted, unsigned int first_inclusive, unsigned int last_inclusive);

unsigned int bSearchByIndicesFloor(Value value, unsigned int const *indices, Value const *values,
                                   unsigned int first_inclusive, unsigned int last_inclusive);

void
qSortFirstHalfIndicesBy(unsigned int *indices, Value *orders, int first_inclusive, int last_inclusive, Value pivot);

void qSortIndicesBy(unsigned int *indices, Value *orders, int first_inclusive, int last_inclusive);

#endif //ISAX_SORT_H
