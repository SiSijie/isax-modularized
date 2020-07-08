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
    unsigned int sax_cardinality;

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


void *summarizations2SAXsThread(void *cache) {
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
        saxCache[i].sax_cardinality = sax_cardinality;

        pthread_create(&threads[i], NULL, summarizations2SAXsThread, (void *) &saxCache[i]);
    }

    for (unsigned int i = 0; i < num_threads; ++i) {
        pthread_join(threads[i], NULL);
    }

    return saxs;
}


Value l2SquareValue2SAXWord(Value value, SAXWord sax_word, Value const *breakpoints, unsigned int length) {
    unsigned int breakpoint_floor = (unsigned int) sax_word;

    if (VALUE_L(value, breakpoints[breakpoint_floor])) {
        return (breakpoints[breakpoint_floor] - value) * (breakpoints[breakpoint_floor] - value);
    } else if (VALUE_GEQ(value, breakpoints[breakpoint_floor + 1])) {
        return (value - breakpoints[breakpoint_floor + 1]) * (value - breakpoints[breakpoint_floor + 1]);
    }

    return 0;
}


Value l2SquareValue2SAXByMask(unsigned int sax_length, Value const *summarizations, SAXWord const *sax,
                              SAXMask const *masks, Value const *breakpoints, Value scale_factor) {
    Value sum = 0;

    for (unsigned int i = 0; i < sax_length; ++i) {
        sum += l2SquareValue2SAXWord(summarizations[i], sax[i] >> SHIFTED_BITS_BY_MASK[masks[i]],
                                     breakpoints + OFFSETS_BY_SEGMENTS[i] + OFFSETS_BY_MASK[masks[i]],
                                     LENGTHS_BY_MASK[masks[i]]);
    }

    return sum * scale_factor;
}


Value l2SquareValue2SAXByMaskSIMD(unsigned int sax_length, Value const *summarizations, SAXWord const *sax,
                                  SAXMask const *masks, Value const *breakpoints, Value scale_factor) {
    __m256i const *m256i_masks = (__m256i const *) masks;
    __m256i m256i_sax_packed = _mm256_cvtepu8_epi16(_mm_lddqu_si128((__m128i const *) sax));

    __m256 m256_summarizations = _mm256_loadu_ps(summarizations);

    __m256i m256i_mask_indices = _mm256_loadu_si256(m256i_masks);
    __m256i m256i_sax = _mm256_cvtepu16_epi32(_mm256_extractf128_si256(m256i_sax_packed, 0));
    __m256i m256i_sax_shifts = _mm256_i32gather_epi32(SHIFTED_BITS_BY_MASK, m256i_mask_indices, 4);
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
        m256i_sax_shifts = _mm256_i32gather_epi32(SHIFTED_BITS_BY_MASK, m256i_mask_indices, 4);
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
    m256_l2square = _mm256_hadd_ps(m256_l2square, m256_l2square);
    _mm256_storeu_ps(M256_FETCHED, m256_l2square);

    return (M256_FETCHED[0] + M256_FETCHED[4]) * scale_factor;
}
