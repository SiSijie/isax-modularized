//
// Created by Qitong Wang on 2020/6/29.
//

#include "paa.h"


typedef struct PAACache {
    Value const *values;
    Value *paas;

    size_t size;
    unsigned int series_length;
    unsigned int paa_length;

    ID *shared_processed_counter;
    unsigned int block_size;

    unsigned int *segment_lengths;
    Value *segment_length_floats;
} PAACache;


void *piecewiseAggregateThread(void *cache) {
    PAACache *paaCache = (PAACache *) cache;

    Value const *values = paaCache->values;
    ID size = paaCache->size;
    unsigned int series_length = paaCache->series_length;

    Value *paas = paaCache->paas;
    unsigned int paa_length = paaCache->paa_length;
//    unsigned int num_values_in_segment = paaCache->series_length / paaCache->paa_length;
//    float float_num_values_in_segment = (float) num_values_in_segment;

    unsigned int segment_id;
    unsigned int *segment_lengths = paaCache->segment_lengths;
    Value *segment_length_floats = paaCache->segment_length_floats;

    ID *shared_processed_counter = paaCache->shared_processed_counter;
    unsigned int block_size = paaCache->block_size;

    ID start_id, stop_id;
    unsigned int value_counter;
    float segment_sum;
    Value const *pt_value;
    Value *pt_paa;

    while ((start_id = __sync_fetch_and_add(shared_processed_counter, block_size)) < size) {
        stop_id = start_id + block_size;
        if (stop_id > size) {
            stop_id = size;
        }

        for (pt_value = values + series_length * start_id, pt_paa = paas + paa_length * start_id,
             value_counter = 0, segment_sum = 0, segment_id = 0;
             pt_value < values + series_length * stop_id; ++pt_value) {
            segment_sum += *pt_value;
            value_counter += 1;

//            if (value_counter == num_values_in_segment) {
            if (value_counter == segment_lengths[segment_id]) {
//                *pt_paa = segment_sum / float_num_values_in_segment;
                *pt_paa = segment_sum / segment_length_floats[segment_id];

                value_counter = 0;
                segment_sum = 0;

                pt_paa += 1;
                segment_id = (segment_id + 1) % paa_length;
            }
        }
    }

    return NULL;
}


Value *piecewiseAggregate(Value const *values, ID size, unsigned int series_length, unsigned int summarization_length,
                          unsigned int num_threads) {
    unsigned int *segment_lengths = malloc(sizeof(unsigned int) * summarization_length);
    Value *segment_length_floats = malloc(sizeof(Value) * summarization_length);

    for (unsigned int i = 0; i < summarization_length; ++i) {
        segment_lengths[i] = series_length / summarization_length;
    }

    for (unsigned int i = 0; i < series_length % summarization_length; ++i) {
        segment_lengths[i] += 1;
    }

    for (unsigned int i = 0; i < summarization_length; ++i) {
        segment_length_floats[i] = (Value) segment_lengths[i];
    }

#ifdef PROFILING
    unsigned int sum = 0;

    for (unsigned int i = 0; i < summarization_length; ++i) {
        sum += segment_lengths[i];
    }

    if (sum != series_length) {
        clog_info(CLOG(CLOGGER_ID), "cannot assign %d to %d (%d)", series_length, summarization_length, sum);
    }
#endif

    Value *paas = aligned_alloc(256, sizeof(Value) * summarization_length * size);

    ID shared_processed_counter = 0;
    unsigned int block_size = 1 + size / (num_threads << 2u);

    pthread_t threads[num_threads];
    PAACache paaCaches[num_threads];

    for (unsigned int i = 0; i < num_threads; ++i) {
        paaCaches[i].values = values;
        paaCaches[i].paas = paas;

        paaCaches[i].size = size;
        paaCaches[i].series_length = series_length;
        paaCaches[i].paa_length = summarization_length;

        paaCaches[i].segment_lengths = segment_lengths;
        paaCaches[i].segment_length_floats = segment_length_floats;

        paaCaches[i].shared_processed_counter = &shared_processed_counter;
        paaCaches[i].block_size = block_size;

        pthread_create(&threads[i], NULL, piecewiseAggregateThread, (void *) &paaCaches[i]);
    }

    for (unsigned int i = 0; i < num_threads; ++i) {
        pthread_join(threads[i], NULL);
    }

    free(segment_lengths);
    free(segment_length_floats);

    return paas;
}
