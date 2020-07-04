//
// Created by Qitong Wang on 2020/7/3.
//

#include "query.h"


QuerySet *initializeQuery(Config const *config, Index const *index) {
    QuerySet *queries = malloc(sizeof(QuerySet));

    queries->query_size = config->query_size;

    Value *values = malloc(sizeof(Value) * config->series_length * config->query_size);
    FILE *file_values = fopen(config->query_filepath, "rb");
    fread(values, sizeof(Value), config->series_length * config->query_size, file_values);
    fclose(file_values);

    queries->values = (Value const *) values;

    if (config->query_summarization_filepath != NULL) {
        Value *summarizations = malloc(sizeof(Value) * config->sax_length * config->query_size);
        FILE *file_summarizations = fopen(config->query_summarization_filepath, "rb");
        fread(summarizations, sizeof(Value), config->sax_length * config->query_size, file_summarizations);
        fclose(file_summarizations);

        queries->summarizations = (Value const *) summarizations;
    } else {
        queries->summarizations = (Value const *) piecewiseAggregate(queries->values, config->query_size,
                                                                     config->series_length, config->sax_length,
                                                                     config->max_threads);
    }

    queries->saxs = (SAXWord const *) summarizations2SAXs(queries->summarizations, index->breakpoints,
                                                          queries->query_size, config->sax_length,
                                                          config->sax_cardinality, config->max_threads);

//    free((Value *) queries->summarizations);
//    queries->summarizations = NULL;

    return queries;
}


void freeQuery(QuerySet *queries) {
    free((Value *) queries->values);
    free((Value *) queries->saxs);
    free(queries);
}


void heapifyTopDown(Value *heap, size_t parent, size_t size) {
    size_t left = (parent << 1u) + 1, right = left + 1;

    if (right < size) {
        size_t next = left;
        if (VALUE_L(heap[left], heap[right])) {
            next = right;
        }

        if (VALUE_L(heap[parent], heap[next])) {
            Value tmp = heap[next];
            heap[next] = heap[parent];
            heap[parent] = tmp;

            heapifyTopDown(heap, next, size);
        }
    } else if (left < size && VALUE_L(heap[parent], heap[left])) {
        Value tmp = heap[left];
        heap[left] = heap[parent];
        heap[parent] = tmp;
    }
}


void heapifyBottomUp(Value *heap, size_t child) {
    if (child != 0) {
        size_t parent = (child - 1) >> 1u;

        if (VALUE_L(heap[parent], heap[child])) {
            Value tmp = heap[child];
            heap[child] = heap[parent];
            heap[parent] = tmp;
        }

        heapifyBottomUp(heap, parent);
    }
}


int checkNUpdateBSF(Answer *answer, Value distance) {
    if (answer->size < answer->k) {
        answer->distances[answer->size] = distance;
        heapifyBottomUp(answer->distances, answer->size);

        answer->size += 1;
    } else if (VALUE_L(distance, answer->distances[0])) {
        answer->distances[0] = distance;
        heapifyTopDown(answer->distances, 0, answer->size);
    } else {
        return -1;
    }

    return 0;
}


Answer *initializeAnswer(Config const *config) {
    Answer *answer = malloc(sizeof(Answer));

    answer->size = 0;
    answer->k = config->k;
    answer->distances = malloc(sizeof(Value) * config->k);
    answer->distances[0] = FLT_MAX;

    answer->lock = malloc(sizeof(pthread_rwlock_t));
    assert(pthread_rwlock_init(answer->lock, NULL) == 0);

    return answer;
}


void freeAnswer(Answer *answer) {
    free(answer->distances);
    pthread_rwlock_destroy(answer->lock);
    free(answer->lock);
    free(answer);
}


void logAnswer(size_t query_id, Answer *answer) {
    for (size_t i = 0; i < answer->size; ++i) {
        clog_info(CLOG(CLOGGER_ID), "%lu / %lu for %lu = %f", i, answer->k, query_id, answer->distances[i]);
    }
}


Value getBSF(Answer * answer) {
    return answer->distances[0];
}
