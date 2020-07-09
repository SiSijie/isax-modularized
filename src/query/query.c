//
// Created by Qitong Wang on 2020/7/3.
//

#include "query.h"


QuerySet *initializeQuery(Config const *config, Index const *index) {
    QuerySet *queries = malloc(sizeof(QuerySet));

    queries->query_size = config->query_size;

#ifdef FINE_TIMING
    clock_t start_clock = clock();
#endif

    Value *values = malloc(sizeof(Value) * config->series_length * config->query_size);
    FILE *file_values = fopen(config->query_filepath, "rb");
    unsigned int read_values = fread(values, sizeof(Value), config->series_length * config->query_size, file_values);
    fclose(file_values);
    assert(read_values == config->series_length * config->query_size);

    queries->values = (Value const *) values;

#ifdef FINE_TIMING
    clog_info(CLOG(CLOGGER_ID), "query - load series = %lums", (clock() - start_clock) * 1000 / CLOCKS_PER_SEC);

    start_clock = clock();
#endif

    if (config->query_summarization_filepath != NULL) {
        Value *summarizations = malloc(sizeof(Value) * config->sax_length * config->query_size);
        FILE *file_summarizations = fopen(config->query_summarization_filepath, "rb");
        read_values = fread(summarizations, sizeof(Value), config->sax_length * config->query_size, file_summarizations);
        fclose(file_summarizations);
        assert(read_values == config->sax_length * config->query_size);

        queries->summarizations = (Value const *) summarizations;
    } else {
        queries->summarizations = (Value const *) piecewiseAggregate(queries->values, config->query_size,
                                                                     config->series_length, config->sax_length,
                                                                     config->max_threads);
    }

#ifdef FINE_TIMING
    char *method4summarizations = "load";
    if (config->database_summarization_filepath == NULL) {
        method4summarizations = "calculate";
    }
    clog_info(CLOG(CLOGGER_ID), "query - %s summarizations = %lums", method4summarizations,
              (clock() - start_clock) * 1000 / CLOCKS_PER_SEC);

    start_clock = clock();
#endif

    queries->saxs = (SAXWord const *) summarizations2SAXs(queries->summarizations, index->breakpoints,
                                                          queries->query_size, config->sax_length,
                                                          config->sax_cardinality, config->max_threads);

#ifdef FINE_TIMING
    clog_info(CLOG(CLOGGER_ID), "query - calculate SAXs = %lums", (clock() - start_clock) * 1000 / CLOCKS_PER_SEC);
#endif

    return queries;
}


void freeQuery(QuerySet *queries) {
    free((Value *) queries->values);
    free((Value *) queries->saxs);
    free(queries);
}


void heapifyTopDown(Value *heap, unsigned int parent, unsigned int size) {
    unsigned int left = (parent << 1u) + 1, right = left + 1;

    if (right < size) {
        unsigned int next = left;
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


void heapifyBottomUp(Value *heap, unsigned int child) {
    if (child != 0) {
        unsigned int parent = (child - 1) >> 1u;

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
        return 1;
    }

#ifdef PROFILING
    pthread_mutex_lock(log_lock_profiling);
    clog_info(CLOG(CLOGGER_ID), "query %d - updated BSF = %f at %d calculated / %d visited", query_id_profiling,
              distance, calculated_series_counter_profiling, visited_series_counter_profiling);
    pthread_mutex_unlock(log_lock_profiling);
#endif

    return 0;
}


Answer *initializeAnswer(Config const *config) {
    Answer *answer = malloc(sizeof(Answer));

    answer->size = 0;
    answer->k = config->k;
    answer->distances = malloc(sizeof(Value) * config->k);
    answer->distances[0] = VALUE_MAX;

    answer->lock = malloc(sizeof(pthread_rwlock_t));
    assert(pthread_rwlock_init(answer->lock, NULL) == 0);

    return answer;
}

void cleanAnswer(Answer *answer) {
    answer->size = 0;
    answer->distances[0] = VALUE_MAX;
}

void freeAnswer(Answer *answer) {
    free(answer->distances);
    pthread_rwlock_destroy(answer->lock);
    free(answer->lock);
    free(answer);
}


void logAnswer(unsigned int query_id, Answer *answer) {
    for (unsigned int i = 0; i < answer->size; ++i) {
        clog_info(CLOG(CLOGGER_ID), "query %d - %d / %luNN = %f", query_id, i, answer->k, answer->distances[i]);
    }
}


Value getBSF(Answer *answer) {
    return answer->distances[0];
}
