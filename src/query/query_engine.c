//
// Created by Qitong Wang on 2020/7/3.
//

#include "query_engine.h"


typedef struct QueryCache {
    Index const *index;
    Node const *const *leaves;
    Value *leaf_distances;
    unsigned int num_leaves;

    Answer *answer;
    Value const *query_values;
    Value const *query_summarization;
    Node *resident_node;

    ID *shared_leaf_id;
    unsigned int block_size;

    Value *m256_fetched_cache;
    Value scale_factor;
} QueryCache;


void *queryThread(void *cache) {
    QueryCache *queryCache = (QueryCache *) cache;

    Value const *values = queryCache->index->values;
    SAXWord const *saxs = queryCache->index->saxs;
    Value const *breakpoints = queryCache->index->breakpoints;

    unsigned int series_length = queryCache->index->series_length;
    unsigned int sax_length = queryCache->index->sax_length;

    Node const *const *leaves = queryCache->leaves;
    Value *leaf_distances = queryCache->leaf_distances;

    Value const *query_summarization = queryCache->query_summarization;
    Value const *query_values = queryCache->query_values;

    Answer *answer = queryCache->answer;
    pthread_rwlock_t *lock = answer->lock;

    Value *m256_fetched_cache = queryCache->m256_fetched_cache;
    Value scale_factor = queryCache->scale_factor;

    unsigned int block_size = queryCache->block_size;
    unsigned int num_leaves = queryCache->num_leaves;
    ID *shared_leaf_id = queryCache->shared_leaf_id;

    Value const *current_series;
    SAXWord const *current_sax;
    Value local_bsf, local_l2Square, local_l2SquarePAA2SAX;
    ID leaf_id, stop_leaf_id, leaf_start_id, leaf_size;

    while ((leaf_id = __sync_fetch_and_add(shared_leaf_id, block_size)) < num_leaves) {
        pthread_rwlock_rdlock(lock);
        local_bsf = getBSF(answer);
        pthread_rwlock_unlock(lock);

        stop_leaf_id = leaf_id + block_size;
        if (stop_leaf_id > num_leaves) {
            stop_leaf_id = num_leaves;
        }

        while (leaf_id < stop_leaf_id) {
            leaf_start_id = leaves[leaf_id]->start_id;
            leaf_size = leaves[leaf_id]->size;

            // TODO correctness-risks traded off for efficiency
            // only using local_bsf suffers from that approximate nearest neighbours < k and never updated
            // could use VALUE_G instead of VALUE_GEQ if not for efficiency
            if (VALUE_G(local_bsf, leaf_distances[leaf_id])) {
#ifdef PROFILING
                __sync_fetch_and_add(&visited_leaves_counter_profiling, 1);
#endif
                for (current_series = values + series_length * leaf_start_id,
                             current_sax = saxs + sax_length * leaf_start_id;
                     current_series < values + series_length * (leaf_start_id + leaf_size);
                     current_series += series_length, current_sax += sax_length) {
#ifdef PROFILING
                    __sync_fetch_and_add(&visited_series_counter_profiling, 1);
#endif
                    local_l2SquarePAA2SAX = l2SquareValue2SAX8SIMD(sax_length, query_summarization, current_sax,
                                                                   breakpoints, scale_factor, m256_fetched_cache);

                    if (VALUE_G(local_bsf, local_l2SquarePAA2SAX)) {
#ifdef PROFILING
                        __sync_fetch_and_add(&calculated_series_counter_profiling, 1);
#endif
                        local_l2Square = l2SquareEarlySIMD(series_length, query_values, current_series, local_bsf,
                                                           m256_fetched_cache);

                        if (VALUE_G(local_bsf, local_l2Square)) {
                            pthread_rwlock_wrlock(lock);

                            checkNUpdateBSF(answer, local_l2Square);
                            local_bsf = getBSF(answer);

                            pthread_rwlock_unlock(lock);
                        }
                    }
                }
            } else {
                return NULL;
            }

            leaf_id += 1;
        }
    }

    return NULL;
}


void *leafThread(void *cache) {
    QueryCache *queryCache = (QueryCache *) cache;

    Value const *breakpoints = queryCache->index->breakpoints;
    unsigned int sax_length = queryCache->index->sax_length;

    Node *resident_node = queryCache->resident_node;
    Value *leaf_distances = queryCache->leaf_distances;

    Value const *query_summarization = queryCache->query_summarization;
    Value scale_factor = queryCache->scale_factor;
    Value *m256_fetched_cache = queryCache->m256_fetched_cache;

    unsigned int block_size = queryCache->block_size;
    unsigned int num_leaves = queryCache->num_leaves;
    ID *shared_leaf_id = queryCache->shared_leaf_id;

    ID leaf_id, stop_leaf_id;
    Node const *leaf;

    while ((leaf_id = __sync_fetch_and_add(shared_leaf_id, block_size)) < num_leaves) {
        stop_leaf_id = leaf_id + block_size;
        if (stop_leaf_id > num_leaves) {
            stop_leaf_id = num_leaves;
        }

        for (unsigned i = leaf_id; i < stop_leaf_id; ++i) {
            leaf = queryCache->leaves[i];

            if (leaf == resident_node) {
                leaf_distances[i] = VALUE_MAX;
            } else {
                leaf_distances[i] = l2SquareValue2SAXByMaskSIMD(sax_length, query_summarization, leaf->sax,
                                                                leaf->squeezed_masks, breakpoints, scale_factor,
                                                                m256_fetched_cache);
            }
        }
    }

    return NULL;
}


void enqueueLeaf(Node *node, Node **leaves, unsigned int *num_leaves) {
    if (node != NULL) {
        if (node->size != 0) {
            leaves[*num_leaves] = node;
            *num_leaves += 1;
        } else if (node->left != NULL) {
            enqueueLeaf(node->left, leaves, num_leaves);
            enqueueLeaf(node->right, leaves, num_leaves);
        }
    }
}


// following C.A.R. Hoare as in https://en.wikipedia.org/wiki/Quicksort#Hoare_partition_scheme
void qSortBy(Node **nodes, Value *distances, int first, int last) {
    if (first < last) {
        Node *tmp_node;
        Value pivot = distances[(unsigned int) (first + last) >> 1u], tmp_distance;

        if (first + 1 < last) {
            Value smaller = distances[first], larger = distances[last];
            if (VALUE_L(larger, smaller)) {
                smaller = distances[last], larger = distances[first];
            }

            if (VALUE_L(pivot, smaller)) {
                pivot = smaller;
            } else if (VALUE_G(pivot, larger)) {
                pivot = larger;
            }
        }

        int first_g = first - 1, last_l = last + 1;
        while (true) {
            do {
                first_g += 1;
            } while (VALUE_L(distances[first_g], pivot));

            do {
                last_l -= 1;
            } while (VALUE_G(distances[last_l], pivot));

            if (first_g >= last_l) {
                break;
            }

            tmp_node = nodes[first_g];
            nodes[first_g] = nodes[last_l];
            nodes[last_l] = tmp_node;

            tmp_distance = distances[first_g];
            distances[first_g] = distances[last_l];
            distances[last_l] = tmp_distance;
        }

        qSortBy(nodes, distances, first, last_l);
        qSortBy(nodes, distances, last_l + 1, last);
    }
}


void qSortFirstHalfBy(Node **nodes, Value *distances, int first, int last, Value pivot) {
    if (first < last) {
        Node *tmp_node;
        Value tmp_distance;

        int first_g = first - 1, last_l = last + 1;
        while (true) {
            do {
                first_g += 1;
            } while (VALUE_L(distances[first_g], pivot));

            do {
                last_l -= 1;
            } while (VALUE_G(distances[last_l], pivot));

            if (first_g >= last_l) {
                break;
            }

            tmp_node = nodes[first_g];
            nodes[first_g] = nodes[last_l];
            nodes[last_l] = tmp_node;

            tmp_distance = distances[first_g];
            distances[first_g] = distances[last_l];
            distances[last_l] = tmp_distance;
        }

        qSortBy(nodes, distances, first, last_l);
    }
}


void conductQueries(QuerySet const *querySet, Index const *index, Config const *config) {
    Answer *answer = initializeAnswer(config);

    Value const *values = index->values;
    SAXWord const *saxs = index->saxs;
    Value const *breakpoints = index->breakpoints;
    unsigned int series_length = config->series_length;
    unsigned int sax_length = config->sax_length;
    unsigned int sax_cardinality = config->sax_cardinality;

    ID shared_leaf_id;

    unsigned int max_threads = config->max_threads;
    QueryCache queryCache[max_threads];

#ifdef FINE_TIMING
    struct timespec start_timestamp, stop_timestamp;
    TimeDiff time_diff;

    clock_code = clock_gettime(CLK_ID, &start_timestamp);
#endif

    Node **leaves = malloc(sizeof(Node *) * index->num_leaves);
    unsigned int num_leaves = 0;
    for (unsigned int j = 0; j < index->roots_size; ++j) {
        enqueueLeaf(index->roots[j], leaves, &num_leaves);
    }
    assert(num_leaves == index->num_leaves);

#ifdef FINE_TIMING
    clock_code = clock_gettime(CLK_ID, &stop_timestamp);
    getTimeDiff(&time_diff, start_timestamp, stop_timestamp);

    clog_info(CLOG(CLOGGER_ID), "query - fetch leaves = %ld.%lds", time_diff.tv_sec, time_diff.tv_nsec);
#endif

    Value *leaf_distances = malloc(sizeof(Value) * num_leaves);
    Value scale_factor = (Value) series_length / (Value) sax_length;
    unsigned int block_size = num_leaves / (max_threads << 3u);
    if (block_size < 4) {
        block_size = 4;
    }

    for (unsigned int i = 0; i < max_threads; ++i) {
        queryCache[i].answer = answer;
        queryCache[i].index = index;

        queryCache[i].num_leaves = num_leaves;
        queryCache[i].leaves = (Node const *const *) leaves;
        queryCache[i].leaf_distances = leaf_distances;

        queryCache[i].scale_factor = scale_factor;
        queryCache[i].block_size = block_size;

        queryCache[i].m256_fetched_cache = malloc(sizeof(Value) * 8);
    }

    Value *local_m256_fetched_cache = queryCache[0].m256_fetched_cache;

    Value const *query_values, *query_summarization;
    SAXWord const *query_sax;
    for (unsigned int i = 0; i < querySet->query_size; ++i) {
        query_values = querySet->values + series_length * i;
        query_summarization = querySet->summarizations + sax_length * i;
        query_sax = querySet->saxs + sax_length * i;

#ifdef PROFILING
        query_id_profiling = i;
        visited_leaves_counter_profiling = 0;
        visited_series_counter_profiling = 0;
        calculated_series_counter_profiling = 0;
#endif
#ifdef FINE_TIMING
        clock_code = clock_gettime(CLK_ID, &start_timestamp);
#endif

        unsigned int rootID = rootSAX2ID(query_sax, sax_length, sax_cardinality);
        Node *node = index->roots[rootID];
        Value local_bsf = getBSF(answer);

        if (node != NULL) {
            while (node->left != NULL) {
                node = route(node, query_sax, sax_length);
            }
#ifdef PROFILING
            visited_leaves_counter_profiling += 1;
            clog_info(CLOG(CLOGGER_ID), "query %d - affiliated leaf size = %d", i, node->size);
#endif

            Value local_l2SquareSAX8, local_l2Square;
            Value const *current_series = values + series_length * node->start_id;
            SAXWord const *current_sax = saxs + sax_length * node->start_id;

            while (current_series < values + series_length * (node->start_id + node->size)) {
#ifdef PROFILING
                visited_series_counter_profiling += 1;
#endif

                local_l2SquareSAX8 = l2SquareValue2SAX8SIMD(sax_length, query_summarization, current_sax, breakpoints,
                                                            scale_factor, local_m256_fetched_cache);

                if (VALUE_G(local_bsf, local_l2SquareSAX8)) {
#ifdef PROFILING
                    calculated_series_counter_profiling += 1;
#endif

                    local_l2Square = l2SquareEarlySIMD(series_length, query_values, current_series, local_bsf,
                                                       local_m256_fetched_cache);

                    if (VALUE_G(local_bsf, local_l2Square)) {
                        checkNUpdateBSF(answer, local_l2Square);
                        local_bsf = getBSF(answer);
                    }
                }

                current_series += series_length;
                current_sax += sax_length;
            }
        } else {
            clog_info(CLOG(CLOGGER_ID), "query %d - no resident node", i);
        }
#ifdef FINE_TIMING
        clock_code = clock_gettime(CLK_ID, &stop_timestamp);
        getTimeDiff(&time_diff, start_timestamp, stop_timestamp);

        clog_info(CLOG(CLOGGER_ID), "query %d - approximate search = %ld.%lds", i, time_diff.tv_sec,
                  time_diff.tv_nsec);
#endif

        if (config->exact_search && !(getBSF(answer) == 0 && answer->size == answer->k)) {
            logAnswer(querySet->query_size + i, answer);

#ifdef FINE_TIMING
            clock_code = clock_gettime(CLK_ID, &start_timestamp);
#endif

            pthread_t leaves_threads[max_threads];
            shared_leaf_id = 0;

            for (unsigned int j = 0; j < max_threads; ++j) {
                queryCache[j].query_values = query_values;
                queryCache[j].query_summarization = query_summarization;

                queryCache[j].shared_leaf_id = &shared_leaf_id;

                queryCache[j].resident_node = node;

                pthread_create(&leaves_threads[j], NULL, leafThread, (void *) &queryCache[j]);
            }

            for (unsigned int j = 0; j < max_threads; ++j) {
                pthread_join(leaves_threads[j], NULL);
            }

#ifdef FINE_TIMING
            clock_code = clock_gettime(CLK_ID, &stop_timestamp);
            getTimeDiff(&time_diff, start_timestamp, stop_timestamp);

            clog_info(CLOG(CLOGGER_ID), "query %d - calculate leaves distances = %ld.%lds", i, time_diff.tv_sec,
                      time_diff.tv_nsec);
#endif

            if (config->sort_leaves) {
#ifdef FINE_TIMING
                clock_code = clock_gettime(CLK_ID, &start_timestamp);
#endif
                qSortFirstHalfBy(leaves, leaf_distances, 0, (int) (num_leaves - 1), local_bsf);
#ifdef FINE_TIMING
                clog_info(CLOG(CLOGGER_ID), "query %d - sort leaves distances = %ld.%lds", i, time_diff.tv_sec,
                          time_diff.tv_nsec);
#endif
            }
#ifdef FINE_TIMING
            clock_code = clock_gettime(CLK_ID, &start_timestamp);
#endif

            pthread_t query_threads[max_threads];
            shared_leaf_id = 0;

            for (unsigned int j = 0; j < max_threads; ++j) {
                queryCache[j].shared_leaf_id = &shared_leaf_id;

                pthread_create(&query_threads[j], NULL, queryThread, (void *) &queryCache[j]);
            }

            for (unsigned int j = 0; j < max_threads; ++j) {
                pthread_join(query_threads[j], NULL);
            }
#ifdef FINE_TIMING
            clock_code = clock_gettime(CLK_ID, &stop_timestamp);
            getTimeDiff(&time_diff, start_timestamp, stop_timestamp);

            clog_info(CLOG(CLOGGER_ID), "query %d - exact search = %ld.%lds", i, time_diff.tv_sec,
                      time_diff.tv_nsec);
#endif
        }
#ifdef PROFILING
        clog_info(CLOG(CLOGGER_ID), "query %d - %d visited / %d leaves", i, visited_leaves_counter_profiling,
                  num_leaves);
        clog_info(CLOG(CLOGGER_ID), "query %d - %d calculated / %d visited / %d loaded series", i,
                  calculated_series_counter_profiling, visited_series_counter_profiling, config->database_size);
#endif

        logAnswer(i, answer);
        cleanAnswer(answer);
    }

    for (unsigned int i = 0; i < max_threads; ++i) {
        free(queryCache[i].m256_fetched_cache);
    }

    freeAnswer(answer);
    free(leaves);
    free(leaf_distances);
}
