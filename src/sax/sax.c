//
// Created by Qitong Wang on 2020/7/2.
//

#include "sax.h"


typedef struct SAXCache {
    SAXWord *saxs;
    Value const *summarizations;
    Value const *const *breakpoints;

    size_t size;
    size_t sax_length;
    unsigned int sax_cardinality;

    size_t *shared_processed_counter;
    size_t block_size;
} SAXCache;


unsigned int bSearchCeiling(Value value, Value const *breakpoints, unsigned int first, unsigned int last) {
    if (first == last) {
        return last;
    }

    unsigned int mid = (first + last) >> 1u;

    if (VALUE_G(value, breakpoints[mid])) {
        return bSearchCeiling(value, breakpoints, mid + 1, last);
    }

    return bSearchCeiling(value, breakpoints, first, mid);
}


SAXWord value2SAXWord8(Value value, Value const *breakpoints) {
    unsigned int num_breakpoints = (1u << 8u) - 1u;

    if (VALUE_G(value, breakpoints[OFFSETS_BY_CARDINALITY[8] + num_breakpoints - 1])) {
        return (SAXWord) num_breakpoints;
    }

    return (SAXWord) bSearchCeiling(value, breakpoints + OFFSETS_BY_CARDINALITY[8], 0, num_breakpoints - 1);
}


void *summarizations2SAXsThread(void *cache) {
    SAXCache *saxCache = (SAXCache *) cache;

    size_t start_position;
    while ((start_position = __sync_fetch_and_add(saxCache->shared_processed_counter, saxCache->block_size)) <
           saxCache->size) {
        size_t stop_position = start_position + saxCache->block_size;
        if (stop_position > saxCache->size) {
            stop_position = saxCache->size;
        }

        for (size_t i = start_position * saxCache->sax_length;
             i < stop_position * saxCache->sax_length; i += saxCache->sax_length) {
            for (unsigned int j = 0; j < saxCache->sax_length; ++j) {
                saxCache->saxs[i + j] = value2SAXWord8(saxCache->summarizations[i + j], saxCache->breakpoints[j]);
            }
        }
    }

    return NULL;
}


SAXWord *
summarizations2SAXs(Value const *summarizations, Value const *const *breakpoints, size_t size, size_t sax_length,
                    unsigned int sax_cardinality, int num_threads) {
    SAXWord *saxs = malloc(sizeof(SAXWord) * sax_length * size);

    size_t shared_processed_counter = 0, block_size = 1 + size / (num_threads * 2);

    pthread_t threads[num_threads];
    SAXCache saxCache[num_threads];

    for (int i = 0; i < num_threads; ++i) {
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

    for (int i = 0; i < num_threads; ++i) {
        pthread_join(threads[i], NULL);
    }

    return saxs;
}


Value l2SquareValue2SAXWord(Value value, SAXWord sax_word, Value const *breakpoints, unsigned int length) {
    if (sax_word == (SAXWord) length) {
        if (VALUE_L(value, breakpoints[length - 1])) {
            return (breakpoints[length - 1] - value) * (breakpoints[length - 1] - value);
        }
        return 0;
    }

    if (VALUE_G(value, breakpoints[sax_word])) {
        return (value - breakpoints[sax_word]) * (value - breakpoints[sax_word]);
    } else if ((sax_word != (SAXWord) 0u) && (VALUE_L(value, breakpoints[sax_word - 1u]))) {
        return (breakpoints[sax_word - 1u] - value) * (breakpoints[sax_word - 1u] - value);
    }

    return 0;
}


Value l2SquareSummarization2SAXByMask(size_t sax_length, Value const *summarizations, SAXWord const *sax,
                                      SAXMask const *masks, Value const *const *breakpoints, Value scale_factor) {
    Value sum = 0;

    for (size_t i = 0; i < sax_length; ++i) {
        sum += l2SquareValue2SAXWord(summarizations[i], sax[i] >> SHIFTED_BITS_BY_MASK[masks[i]],
                                     breakpoints[i] + OFFSETS_BY_MASK[masks[i]], LENGTHS_BY_MASK[masks[i]]);
    }

    return sum * scale_factor;
}


Value l2SquareSummarization2SAX8(size_t sax_length, Value const *summarizations, SAXWord const *sax,
                                 Value const *const *breakpoints, Value scale_factor) {
    Value sum = 0;

    for (size_t i = 0; i < sax_length; ++i) {
        sum += l2SquareValue2SAXWord(summarizations[i], sax[i], breakpoints[i] + OFFSETS_BY_CARDINALITY[8],
                                     LENGTHS_BY_CARDINALITY[8]);
    }

    return sum * scale_factor;
}
