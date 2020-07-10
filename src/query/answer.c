//
// Created by Qitong Wang on 2020/7/10.
//

#include "answer.h"


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
