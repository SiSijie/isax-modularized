//
// Created by Qitong Wang on 2020/7/3.
//

#ifndef ISAX_DISTANCE_H
#define ISAX_DISTANCE_H

#include <stdlib.h>
#include <immintrin.h>

#include "globals.h"

Value l2Square(size_t length, Value const *left, Value const *right);

Value l2SquareSIMD(size_t length, Value const *left, Value const *right);

Value l2SquareEarly(size_t length, Value const *left, Value const *right, Value threshold);

Value l2SquareEarlySIMD(size_t length, Value const *left, Value const *right, Value threshold);

#endif //ISAX_DISTANCE_H
