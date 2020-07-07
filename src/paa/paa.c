//
// Created by Qitong Wang on 2020/6/29.
//

#include "paa.h"


typedef struct PAACache {
    Value const *values;
    Value *paas;
    unsigned int size;

    unsigned int series_length;
    unsigned int summarization_length;

    unsigned int *shared_processed_counter;
    unsigned int block_size;
} PAACache;


void *piecewiseAggregateThread(void *cache) {
    PAACache *paaCache = (PAACache *) cache;

    unsigned int segment_length = paaCache->series_length / paaCache->summarization_length;
    float float_segment_length = (float) segment_length;

    unsigned int start_position;
    while ((start_position = __sync_fetch_and_add(paaCache->shared_processed_counter, paaCache->block_size)) <
           paaCache->size) {
        unsigned int stop_position = start_position + paaCache->block_size;
        if (stop_position > paaCache->size) {
            stop_position = paaCache->size;
        }

        float segment_sum = 0;
        for (unsigned int i = paaCache->series_length * start_position,
                     j = start_position * paaCache->summarization_length, k = 0;
             i < paaCache->series_length * stop_position; ++i) {
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


Value *piecewiseAggregate(Value const *values, unsigned int size, unsigned int series_length,
                          unsigned int summarization_length,
                          unsigned int num_threads) {
    Value *paas = malloc(sizeof(Value) * summarization_length * size);

    unsigned int shared_processed_counter = 0;
    unsigned int block_size = size / (num_threads * 2);

    pthread_t threads[num_threads];
    PAACache paaCaches[num_threads];

    for (unsigned int i = 0; i < num_threads; ++i) {
        paaCaches[i].values = values;
        paaCaches[i].paas = paas;
        paaCaches[i].size = size;
        paaCaches[i].series_length = series_length;
        paaCaches[i].summarization_length = summarization_length;
        paaCaches[i].shared_processed_counter = &shared_processed_counter;
        paaCaches[i].block_size = block_size;

        pthread_create(&threads[i], NULL, piecewiseAggregateThread, (void *) &paaCaches[i]);
    }

    for (unsigned int i = 0; i < num_threads; ++i) {
        pthread_join(threads[i], NULL);
    }

    return paas;
}
