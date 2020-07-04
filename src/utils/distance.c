//
// Created by Qitong Wang on 2020/7/3.
//

#include "distance.h"

// TODO SIMD
Value l2Square(size_t length, Value const *left, Value const *right) {
    Value sum = 0;

    for (size_t i = 0; i < length; ++i) {
        sum += (left[i] - right[i]) * (left[i] - right[i]);
    }

    return sum;
}