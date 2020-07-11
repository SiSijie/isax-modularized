//
// Created by Qitong Wang on 2020/7/2.
//

#include "sax.h"


typedef struct SAXCache {
    SAXWord *saxs;
    Value const *summarizations;
    Value const *breakpoints;

    unsigned int size;
    unsigned int sax_length;

    unsigned int *shared_processed_counter;
    unsigned int block_size;
} SAXCache;


unsigned int bSearchFloor(Value value, Value const *breakpoints, unsigned int first, unsigned int last) {
    while (first + 1 < last) {
        unsigned int mid = (first + last) >> 1u;

        if (VALUE_L(value, breakpoints[mid])) {
            last = mid;
        } else {
            first = mid;
        }
    }

    return first;
}


void *summarizations2SAXs8Thread(void *cache) {
    SAXCache *saxCache = (SAXCache *) cache;

    unsigned int start_position, stop_position, i, j;
    while ((start_position = __sync_fetch_and_add(saxCache->shared_processed_counter, saxCache->block_size)) <
           saxCache->size) {
        stop_position = start_position + saxCache->block_size;
        if (stop_position > saxCache->size) {
            stop_position = saxCache->size;
        }

        for (i = start_position * saxCache->sax_length;
             i < stop_position * saxCache->sax_length; i += saxCache->sax_length) {
            for (j = 0; j < saxCache->sax_length; ++j) {
                saxCache->saxs[i + j] = bSearchFloor(saxCache->summarizations[i + j],
                                                     saxCache->breakpoints + OFFSETS_BY_SEGMENTS[j] +
                                                     OFFSETS_BY_CARDINALITY[7], 0, 256);
            }
        }
    }

    return NULL;
}


SAXWord *
summarizations2SAXs(Value const *summarizations, Value const *breakpoints, unsigned int size, unsigned int sax_length,
                    unsigned int sax_cardinality, unsigned int num_threads) {
    SAXWord *saxs = malloc(sizeof(SAXWord) * sax_length * size);

    pthread_t threads[num_threads];
    SAXCache saxCache[num_threads];

    for (unsigned int shared_processed_counter = 0, block_size = 1 + size / (num_threads * 2), i = 0;
         i < num_threads; ++i) {
        saxCache[i].saxs = saxs;
        saxCache[i].summarizations = summarizations;
        saxCache[i].breakpoints = breakpoints;
        saxCache[i].shared_processed_counter = &shared_processed_counter;
        saxCache[i].block_size = block_size;
        saxCache[i].size = size;
        saxCache[i].sax_length = sax_length;

        pthread_create(&threads[i], NULL, summarizations2SAXs8Thread, (void *) &saxCache[i]);
    }

    for (unsigned int i = 0; i < num_threads; ++i) {
        pthread_join(threads[i], NULL);
    }

    return saxs;
}


Value l2SquareValue2SAXByMask(unsigned int sax_length, Value const *summarizations, SAXWord const *sax,
                              SAXMask const *masks, Value const *breakpoints, Value scale_factor) {
    Value sum = 0;
    Value const *current_breakpoints;

    for (unsigned int i = 0; i < sax_length; ++i) {
        current_breakpoints = breakpoints + OFFSETS_BY_SEGMENTS[i] + OFFSETS_BY_MASK[masks[i]] +
                              ((unsigned int) sax[i] >> SHIFTS_BY_MASK[masks[i]]);

        if (VALUE_L(summarizations[i], *current_breakpoints)) {
            sum += (*current_breakpoints - summarizations[i]) * (*current_breakpoints - summarizations[i]);
        } else if (VALUE_G(summarizations[i], *(current_breakpoints + 1))) {
            sum += (summarizations[i] - *(current_breakpoints + 1)) * (summarizations[i] - *(current_breakpoints + 1));
        }
    }

    return sum * scale_factor;
}


Value l2SquareValue2SAX8(unsigned int sax_length, Value const *summarizations, SAXWord const *sax,
                         Value const *breakpoints, Value scale_factor) {
    Value sum = 0;
    Value const *current_breakpoints;

    for (unsigned int i = 0; i < sax_length; ++i) {
        current_breakpoints = breakpoints + OFFSETS_BY_SEGMENTS[i] + OFFSETS_BY_CARDINALITY[7] + (unsigned int) sax[i];

        if (VALUE_L(summarizations[i], *current_breakpoints)) {
            sum += (*current_breakpoints - summarizations[i]) * (*current_breakpoints - summarizations[i]);
        } else if (VALUE_G(summarizations[i], *(current_breakpoints + 1))) {
            sum += (summarizations[i] - *(current_breakpoints + 1)) * (summarizations[i] - *(current_breakpoints + 1));
        }
    }

    return sum * scale_factor;
}


Value l2SquareValue2SAXByMaskSIMD(unsigned int sax_length, Value const *summarizations, SAXWord const *sax,
                                  SAXMask const *masks, Value const *breakpoints, Value scale_factor, Value *cache) {
    __m256i const *m256i_masks = (__m256i const *) masks;
    __m256i m256i_sax_packed = _mm256_cvtepu8_epi16(_mm_lddqu_si128((__m128i const *) sax));

    __m256 m256_summarizations = _mm256_loadu_ps(summarizations);

    __m256i m256i_mask_indices = _mm256_loadu_si256(m256i_masks);
    __m256i m256i_sax = _mm256_cvtepu16_epi32(_mm256_extractf128_si256(m256i_sax_packed, 0));
    __m256i m256i_sax_shifts = _mm256_i32gather_epi32(SHIFTS_BY_MASK, m256i_mask_indices, 4);
    __m256i m256i_cardinality_offsets = _mm256_i32gather_epi32(OFFSETS_BY_MASK, m256i_mask_indices, 4);
    __m256i m256i_breakpoint_indices = _mm256_add_epi32(_mm256_add_epi32(_mm256_loadu_si256(M256I_OFFSETS_BY_SEGMENTS),
                                                                         m256i_cardinality_offsets),
                                                        _mm256_srlv_epi32(m256i_sax, m256i_sax_shifts));

    __m256 m256_floor_breakpoints = _mm256_i32gather_ps(breakpoints, m256i_breakpoint_indices, 4);
    __m256 m256_ceiling_breakpoints = _mm256_i32gather_ps(breakpoints,
                                                          _mm256_add_epi32(m256i_breakpoint_indices, M256I_1), 4);

    __m256 m256_floor_distances = _mm256_and_ps(_mm256_sub_ps(m256_floor_breakpoints, m256_summarizations),
                                                _mm256_cmp_ps(m256_summarizations, m256_floor_breakpoints, _CMP_LT_OS));
    __m256 m256_ceiling_distances = _mm256_and_ps(_mm256_sub_ps(m256_summarizations, m256_ceiling_breakpoints),
                                                  _mm256_cmp_ps(m256_summarizations, m256_ceiling_breakpoints,
                                                                _CMP_GT_OS));

    __m256 m256_distances = _mm256_add_ps(m256_floor_distances, m256_ceiling_distances);
    __m256 m256_l2square = _mm256_mul_ps(m256_distances, m256_distances);

    if (sax_length == 16) {
        m256_summarizations = _mm256_loadu_ps(summarizations + 8);

        m256i_mask_indices = _mm256_loadu_si256(m256i_masks + 1);
        m256i_sax = _mm256_cvtepu16_epi32(_mm256_extractf128_si256(m256i_sax_packed, 1));
        m256i_sax_shifts = _mm256_i32gather_epi32(SHIFTS_BY_MASK, m256i_mask_indices, 4);
        m256i_cardinality_offsets = _mm256_i32gather_epi32(OFFSETS_BY_MASK, m256i_mask_indices, 4);
        m256i_breakpoint_indices = _mm256_add_epi32(_mm256_add_epi32(_mm256_loadu_si256(M256I_OFFSETS_BY_SEGMENTS),
                                                                     m256i_cardinality_offsets),
                                                    _mm256_srlv_epi32(m256i_sax, m256i_sax_shifts));

        m256_floor_breakpoints = _mm256_i32gather_ps(breakpoints, m256i_breakpoint_indices, 4);
        m256_ceiling_breakpoints = _mm256_i32gather_ps(breakpoints,
                                                       _mm256_add_epi32(m256i_breakpoint_indices, M256I_1), 4);

        m256_floor_distances = _mm256_and_ps(_mm256_sub_ps(m256_floor_breakpoints, m256_summarizations),
                                             _mm256_cmp_ps(m256_summarizations, m256_floor_breakpoints, _CMP_LT_OS));
        m256_ceiling_distances = _mm256_and_ps(_mm256_sub_ps(m256_summarizations, m256_ceiling_breakpoints),
                                               _mm256_cmp_ps(m256_summarizations, m256_ceiling_breakpoints,
                                                             _CMP_GT_OS));

        m256_distances = _mm256_add_ps(m256_floor_distances, m256_ceiling_distances);
        m256_l2square = _mm256_fmadd_ps(m256_distances, m256_distances, m256_l2square);
    }

    m256_l2square = _mm256_hadd_ps(m256_l2square, m256_l2square);
    _mm256_storeu_ps(cache, _mm256_hadd_ps(m256_l2square, m256_l2square));

    return (cache[0] + cache[4]) * scale_factor;
}


Value l2SquareValue2SAX8SIMD(unsigned int sax_length, Value const *summarizations, SAXWord const *sax,
                             Value const *breakpoints, Value scale_factor, Value *cache) {
    __m256 m256_summarizations = _mm256_loadu_ps(summarizations);
    __m256i m256i_sax_packed = _mm256_cvtepu8_epi16(_mm_lddqu_si128((__m128i const *) sax));

    __m256i m256i_sax = _mm256_cvtepu16_epi32(_mm256_extractf128_si256(m256i_sax_packed, 0));
    __m256i m256i_breakpoint_indices = _mm256_add_epi32(m256i_sax, M256I_BREAKPOINTS8_OFFSETS_0_7);

    __m256 m256_floor_breakpoints = _mm256_i32gather_ps(breakpoints, m256i_breakpoint_indices, 4);
    __m256 m256_ceiling_breakpoints = _mm256_i32gather_ps(breakpoints,
                                                          _mm256_add_epi32(m256i_breakpoint_indices, M256I_1), 4);

    __m256 m256_distances2floor = _mm256_and_ps(_mm256_sub_ps(m256_floor_breakpoints, m256_summarizations),
                                                _mm256_cmp_ps(m256_summarizations, m256_floor_breakpoints, _CMP_LT_OS));
    __m256 m256_distances2ceiling = _mm256_and_ps(_mm256_sub_ps(m256_summarizations, m256_ceiling_breakpoints),
                                                  _mm256_cmp_ps(m256_summarizations, m256_ceiling_breakpoints,
                                                                _CMP_GT_OS));

    __m256 m256_distances = _mm256_add_ps(m256_distances2floor, m256_distances2ceiling);
    __m256 m256_l2square = _mm256_mul_ps(m256_distances, m256_distances);

    // sax_length == 8 or 16, ONLY
    if (sax_length == 16) {
        m256_summarizations = _mm256_loadu_ps(summarizations + 8);

        m256i_sax = _mm256_cvtepu16_epi32(_mm256_extractf128_si256(m256i_sax_packed, 1));
        m256i_breakpoint_indices = _mm256_add_epi32(m256i_sax, M256I_BREAKPOINTS8_OFFSETS_8_15);

        m256_floor_breakpoints = _mm256_i32gather_ps(breakpoints, m256i_breakpoint_indices, 4);
        m256_ceiling_breakpoints = _mm256_i32gather_ps(breakpoints,
                                                       _mm256_add_epi32(m256i_breakpoint_indices, M256I_1), 4);

        m256_distances2floor = _mm256_and_ps(_mm256_sub_ps(m256_floor_breakpoints, m256_summarizations),
                                             _mm256_cmp_ps(m256_summarizations, m256_floor_breakpoints, _CMP_LT_OS));
        m256_distances2ceiling = _mm256_and_ps(_mm256_sub_ps(m256_summarizations, m256_ceiling_breakpoints),
                                               _mm256_cmp_ps(m256_summarizations, m256_ceiling_breakpoints,
                                                             _CMP_GT_OS));

        m256_distances = _mm256_add_ps(m256_distances2floor, m256_distances2ceiling);
        m256_l2square = _mm256_fmadd_ps(m256_distances, m256_distances, m256_l2square);
    }

    m256_l2square = _mm256_hadd_ps(m256_l2square, m256_l2square);
    _mm256_storeu_ps(cache, _mm256_hadd_ps(m256_l2square, m256_l2square));

    return (cache[0] + cache[4]) * scale_factor;
}
