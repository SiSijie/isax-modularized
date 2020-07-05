//
// Created by Qitong Wang on 2020/7/3.
//

#include "distance.h"


Value fetched_m256[8];


Value l2Square(size_t length, Value const *left, Value const *right) {
    Value sum = 0;

    for (size_t i = 0; i < length; ++i) {
        sum += (left[i] - right[i]) * (left[i] - right[i]);
    }

    return sum;
}


Value l2SquareSIMD(size_t length, Value const *left, Value const *right) {
    __m256 m256_square_cumulated = _mm256_setzero_ps(), m256_diff, m256_sum;

    for (size_t i = 0; i < length; i += 8) {
        // TODO assess whether necessary to use aligned_alloc() for supporting _mm256_load_ps()
        m256_diff = _mm256_sub_ps(_mm256_loadu_ps(left + i), _mm256_loadu_ps(right + i));
        m256_square_cumulated = _mm256_fmadd_ps(m256_diff, m256_diff, m256_square_cumulated);
    }

    m256_sum = _mm256_hadd_ps(m256_square_cumulated, m256_square_cumulated);
    m256_sum = _mm256_hadd_ps(m256_sum, m256_sum);
    _mm256_storeu_ps(fetched_m256, m256_sum);

    return fetched_m256[0] + fetched_m256[4];
}


Value l2SquareEarly(size_t length, Value const *left, Value const *right, Value threshold) {
    Value sum = 0;

    for (size_t i = 0; i < length; ++i) {
        if (VALUE_G(sum += (left[i] - right[i]) * (left[i] - right[i]), threshold)) {
            return sum;
        }
    }

    return sum;
}


Value l2SquareEarlySIMD(size_t length, Value const *left, Value const *right, Value threshold) {
    Value sum = 0;

    __m256 m256_square_cumulated = _mm256_setzero_ps(), m256_diff, m256_sum;
    for (size_t i = 0; i < length; i += 8) {
        m256_diff = _mm256_sub_ps(_mm256_loadu_ps(left + i), _mm256_loadu_ps(right + i));
        m256_square_cumulated = _mm256_fmadd_ps(m256_diff, m256_diff, m256_square_cumulated);

        m256_sum = _mm256_hadd_ps(m256_square_cumulated, m256_square_cumulated);
        m256_sum = _mm256_hadd_ps(m256_sum, m256_sum);
        _mm256_storeu_ps(fetched_m256, m256_sum);

        if (VALUE_G((sum = fetched_m256[0] + fetched_m256[4]), threshold)) {
            return sum;
        }
    }

    return sum;
}
