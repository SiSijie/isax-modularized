//
// Created by Qitong Wang on 2020/7/3.
//

#ifndef ISAX_DISTANCE_H
#define ISAX_DISTANCE_H

#include <stdlib.h>
#include <immintrin.h>

#include "globals.h"

Value l2Square(unsigned int length, Value const *left, Value const *right);

Value l2SquareSIMD(unsigned int length, Value const *left, Value const *right, Value *cache);

Value l2SquareEarly(unsigned int length, Value const *left, Value const *right, Value threshold);

Value l2SquareEarlySIMD(unsigned int length, Value const *left, Value const *right, Value threshold, Value *cache);

#endif //ISAX_DISTANCE_H
