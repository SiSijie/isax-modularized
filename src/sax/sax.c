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


SAXWord value2SAXWord(Value value, Value const *breakpoints, unsigned int num_breakpoints) {
    Value *nearest_breakpoint = (Value *) bsearch(&value, breakpoints, num_breakpoints, sizeof(Value), VALUE_COMPARE);

    if (nearest_breakpoint != NULL) {
        return (SAXWord) (nearest_breakpoint - breakpoints);
    } else if (value > 0) {
        return (SAXWord) num_breakpoints;
    }

    return (SAXWord) 0;
}


void *summarizations2SAXsThread(void *cache) {
    SAXCache *saxCache = (SAXCache *) cache;

    unsigned int offset = OFFSETS_BY_CARDINALITY[saxCache->sax_cardinality];
    unsigned int num_breakpoints = (1u << saxCache->sax_cardinality) - 1u;

    size_t start_position;
    while ((start_position = __sync_fetch_and_add(saxCache->shared_processed_counter, saxCache->block_size)) <
           saxCache->size) {
        size_t stop_position = start_position + saxCache->block_size;
        if (stop_position > saxCache->size) {
            stop_position = saxCache->size;
        }

        for (size_t i = start_position; i < stop_position; i += saxCache->sax_length) {
            for (unsigned int j = 0; j < saxCache->sax_length; ++j) {
                saxCache->saxs[i + j] = value2SAXWord(saxCache->summarizations[i + j],
                                                      saxCache->breakpoints[j] + offset, num_breakpoints);
            }
        }
    }

    return NULL;
}


SAXWord *
summarizations2SAXs(Value const *summarizations, Value const *const *breakpoints, size_t size, size_t sax_length,
                    unsigned int sax_cardinality, int num_threads) {
    SAXWord *saxs = malloc(sizeof(SAXWord) * sax_length * size);

    size_t shared_processed_counter = 0;
    size_t block_size = size / (num_threads * 2);

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