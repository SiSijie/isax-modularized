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

    unsigned int start_position;
    while ((start_position = __sync_fetch_and_add(saxCache->shared_processed_counter, saxCache->block_size)) <
           saxCache->size) {
        unsigned int stop_position = start_position + saxCache->block_size;
        if (stop_position > saxCache->size) {
            stop_position = saxCache->size;
        }

        for (unsigned int i = start_position * saxCache->sax_length;
             i < stop_position * saxCache->sax_length; i += saxCache->sax_length) {
            for (unsigned int j = 0; j < saxCache->sax_length; ++j) {
                saxCache->saxs[i + j] = bSearchFloor(saxCache->summarizations[i + j],
                                                     saxCache->breakpoints + OFFSETS_BY_SEGMENTS[i], 0, 256);
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
    Value sum = 0;

    __m256i const *m256i_masks = (__m256i *) masks;
    __m256i m256_indices, m256_offsets, m256_shifts;
    for (unsigned int i = 0; i < (sax_length >> 3u); ++i) {
        m256_indices = _mm256_loadu_si256(m256i_masks + i);
        m256_offsets = _mm256_i32gather_epi32(OFFSETS_BY_MASK, m256_indices, 4);
        m256_shifts = _mm256_i32gather_epi32(SHIFTED_BITS_BY_MASK, m256_indices, 4);
    }

    return sum * scale_factor;
}
