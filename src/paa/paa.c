//
// Created by Qitong Wang on 2020/6/29.
//

#include "paa.h"


typedef struct PAACache {
    Value const *values;
    Value *paas;
    size_t size;

    size_t series_length;
    size_t summarization_length;

    size_t *shared_processed_counter;
    size_t block_size;
} PAACache;


void *piecewiseAggregateThread(void *cache) {
    PAACache *paaCache = (PAACache *) cache;

    size_t segment_length = paaCache->series_length / paaCache->summarization_length;
    float float_segment_length = (float) segment_length;

    size_t start_position;
    while ((start_position = __sync_fetch_and_add(paaCache->shared_processed_counter, paaCache->block_size)) <
           paaCache->size) {
        size_t stop_position = start_position + paaCache->block_size;
        if (stop_position > paaCache->size) {
            stop_position = paaCache->size;
        }

        size_t j = start_position * paaCache->summarization_length, k = 0;
        float segment_sum = 0;

        for (size_t i = paaCache->series_length * start_position; i < paaCache->series_length * stop_position; ++i) {
            segment_sum += paaCache->values[i];

            if ((k += 1) == segment_length) {
                paaCache->paas[j] = segment_sum / float_segment_length;

                j += 1;
                k = 0;
                segment_sum = 0;
            }
        }
    }

    return NULL;
}


Value *piecewiseAggregate(Value const *values, size_t size, size_t series_length, size_t summarization_length,
                          int num_threads) {
    Value *paas = malloc(sizeof(Value) * summarization_length * size);

    size_t shared_processed_counter = 0;
    size_t block_size = size / (num_threads * 2);

    pthread_t threads[num_threads];
    PAACache paaCaches[num_threads];

    for (int i = 0; i < num_threads; ++i) {
        paaCaches[i].values = values;
        paaCaches[i].paas = paas;
        paaCaches[i].size = size;
        paaCaches[i].series_length = series_length;
        paaCaches[i].summarization_length = summarization_length;
        paaCaches[i].shared_processed_counter = &shared_processed_counter;
        paaCaches[i].block_size = block_size;

        pthread_create(&threads[i], NULL, piecewiseAggregateThread, (void *) &paaCaches[i]);
    }

    for (int i = 0; i < num_threads; ++i) {
        pthread_join(threads[i], NULL);
    }

    return paas;
}
