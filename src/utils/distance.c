//
// Created by Qitong Wang on 2020/7/3.
//

#include "distance.h"


Value l2Square(unsigned int length, Value const *left, Value const *right) {
    Value sum = 0;

    for (unsigned int i = 0; i < length; ++i) {
        sum += (left[i] - right[i]) * (left[i] - right[i]);
    }

    return sum;
}


Value l2SquareSIMD(unsigned int length, Value const *left, Value const *right, Value *cache) {
    __m256 m256_square_cumulated = _mm256_setzero_ps(), m256_diff, m256_sum, m256_left, m256_right;

    for (unsigned int i = 0; i < length; i += 8) {
        m256_left = _mm256_load_ps(left + i);
        m256_right = _mm256_load_ps(right + i);
        m256_diff = _mm256_sub_ps(m256_left, m256_right);
        m256_square_cumulated = _mm256_fmadd_ps(m256_diff, m256_diff, m256_square_cumulated);
    }

    m256_sum = _mm256_hadd_ps(m256_square_cumulated, m256_square_cumulated);
    _mm256_store_ps(cache, _mm256_hadd_ps(m256_sum, m256_sum));

    return cache[0] + cache[4];
}


Value l2SquareEarly(unsigned int length, Value const *left, Value const *right, Value threshold) {
    Value sum = 0;

    for (unsigned int i = 0; i < length; ++i) {
        sum += ((left[i] - right[i]) * (left[i] - right[i]));

        if (VALUE_G(sum, threshold)) {
            return sum;
        }
    }

    return sum;
}


Value l2SquareEarlySIMD(unsigned int length, Value const *left, Value const *right, Value threshold, Value *cache) {
    Value sum = 0;

    __m256 m256_square, m256_diff, m256_sum, m256_left, m256_right;
    for (unsigned int i = 0; i < length; i += 8) {
        m256_left = _mm256_load_ps(left + i);
        m256_right = _mm256_load_ps(right + i);
        m256_diff = _mm256_sub_ps(m256_left, m256_right);
        m256_square = _mm256_mul_ps(m256_diff, m256_diff);
        m256_sum = _mm256_hadd_ps(m256_square, m256_square);
        _mm256_store_ps(cache, _mm256_hadd_ps(m256_sum, m256_sum));

        sum += (cache[0] + cache[4]);

        if (VALUE_G(sum, threshold)) {
            return sum;
        }
    }

    return sum;
}
