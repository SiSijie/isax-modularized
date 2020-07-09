//
// Created by Qitong Wang on 2020/7/3.
//

#include "query_engine.h"


typedef struct QueryCache {
    Answer *answer;
    Index const *index;

    Node const *const *leaves;
    Value const *leaf_distances;
    unsigned int num_leaves;

    Value scale_factor;

    Value const *query_values;
    Value const *query_summarization;

    unsigned int *shared_leaf_id;

    SAXMask const *masks8;
} QueryCache;


void *queryThread(void *cache) {
    QueryCache *queryCache = (QueryCache *) cache;

    unsigned int leaf_id;
    while ((leaf_id = __sync_fetch_and_add(queryCache->shared_leaf_id, 1)) < queryCache->num_leaves) {
        Node const *const leaf = queryCache->leaves[leaf_id];

        // TODO only using local_bsf suffers from the problem that approximate nearest neighbours < k and never updated
//        pthread_rwlock_rdlock(queryCache->answer->lock);
        Value local_bsf = getBSF(queryCache->answer);
//        pthread_rwlock_unlock(queryCache->answer->lock);

        if (VALUE_G(local_bsf, queryCache->leaf_distances[leaf_id])) {
#ifdef PROFILING
            __sync_fetch_and_add(&visited_leaves_counter_profiling, 1);
#endif

            for (unsigned int i = 0; i < leaf->size; ++i) {
#ifdef PROFILING
                __sync_fetch_and_add(&visited_series_counter_profiling, 1);
#endif

#ifdef DEBUG
                Value result = l2SquareValue2SAXByMask(queryCache->index->sax_length, queryCache->query_summarization,
                                                       queryCache->index->saxs +
                                                       leaf->ids[i] * queryCache->index->sax_length,
                                                       queryCache->masks8, queryCache->index->breakpoints,
                                                       queryCache->scale_factor);
                Value resultSIMD = l2SquareValue2SAXByMaskSIMD(queryCache->index->sax_length,
                                                               queryCache->query_summarization,
                                                               queryCache->index->saxs +
                                                               leaf->ids[i] * queryCache->index->sax_length,
                                                               queryCache->masks8, queryCache->index->breakpoints,
                                                               queryCache->scale_factor);

                if (result - resultSIMD > 1e4 || resultSIMD - result > 1e4) {
                    clog_debug(CLOG(CLOGGER_ID), "query - paa-sax l2square = %f ~ %f (SIMD)", result, resultSIMD);
                }
#endif

                // ignoring equivalence for performance
                if (VALUE_G(local_bsf,
                            l2SquareValue2SAXByMaskSIMD(queryCache->index->sax_length, queryCache->query_summarization,
                                                        queryCache->index->saxs +
                                                        leaf->ids[i] * queryCache->index->sax_length,
                                                        queryCache->masks8, queryCache->index->breakpoints,
                                                        queryCache->scale_factor))) {
#ifdef PROFILING
                    __sync_fetch_and_add(&calculated_series_counter_profiling, 1);
#endif

#ifdef DEBUG
                    result = l2Square(queryCache->index->series_length, queryCache->query_values,
                                      queryCache->index->values +
                                      leaf->ids[i] * queryCache->index->series_length);
                    resultSIMD = l2SquareSIMD(queryCache->index->series_length, queryCache->query_values,
                                              queryCache->index->values +
                                              leaf->ids[i] * queryCache->index->series_length);

                    if (result - resultSIMD > 1e4 || resultSIMD - result > 1e4) {
                        clog_debug(CLOG(CLOGGER_ID), "query - l2square = %f ~ %f (SIMD)", result, resultSIMD);
                    }
#endif

                    Value local_distance = l2SquareEarlySIMD(queryCache->index->series_length, queryCache->query_values,
                                                             queryCache->index->values +
                                                             queryCache->index->series_length * leaf->ids[i],
                                                             local_bsf);
                    // ignoring equivalence for performance
                    if (VALUE_G(local_bsf, local_distance)) {
                        pthread_rwlock_wrlock(queryCache->answer->lock);

                        checkNUpdateBSF(queryCache->answer, local_distance);
                        local_bsf = getBSF(queryCache->answer);

                        pthread_rwlock_unlock(queryCache->answer->lock);
                    }
                }
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
        } else if (node->size == 0 && node->left != NULL) {
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


void conductQueries(QuerySet const *querySet, Index const *index, Config const *config) {
    Answer *answer = initializeAnswer(config);

    QueryCache queryCache[config->max_threads];

    SAXMask *masks8 = malloc(sizeof(SAXMask) * config->sax_length);
    for (unsigned int i = 0; i < config->sax_length; ++i) {
        masks8[i] = 1u;
    }

#ifdef FINE_TIMING
    clock_t start_clock = clock();
#endif

    Node **leaves = malloc(sizeof(Node *) * index->num_leaves);
    unsigned int num_leaves = 0;
    for (unsigned int j = 0; j < index->roots_size; ++j) {
        enqueueLeaf(index->roots[j], leaves, &num_leaves);
    }

#ifdef FINE_TIMING
    clog_info(CLOG(CLOGGER_ID), "query - fetch leaves = %lums", (clock() - start_clock) * 1000 / CLOCKS_PER_SEC);
#endif

    assert(num_leaves == index->num_leaves);

    Value *leaf_distances = malloc(sizeof(Value) * num_leaves);
    Value scale_factor = (Value) config->series_length / (Value) config->sax_length;

    for (unsigned int i = 0; i < querySet->query_size; ++i) {
        Value const *query_values = querySet->values + config->series_length * i;
        Value const *query_summarization = querySet->summarizations + config->sax_length * i;
        SAXWord const *query_sax = querySet->saxs + config->sax_length * i;

#ifdef PROFILING
        query_id_profiling = i;
        visited_leaves_counter_profiling = 0;
        visited_series_counter_profiling = 0;
        calculated_series_counter_profiling = 0;
#endif

#ifdef FINE_TIMING
        start_clock = clock();
#endif

        unsigned int rootID = rootSAX2ID(query_sax, config->sax_length, config->sax_cardinality);
        Node *node = index->roots[rootID];
        if (node != NULL) {
            while (node->left != NULL) {
                node = route(node, query_sax, config->sax_length);
            }

#ifdef PROFILING
            visited_leaves_counter_profiling += 1;
            clog_info(CLOG(CLOGGER_ID), "query %d - affiliated leaf size = %d", i, node->size);
#endif

            for (unsigned int j = 0; j < node->size; ++j) {
#ifdef PROFILING
                visited_series_counter_profiling += 1;
                calculated_series_counter_profiling += 1;
#endif

                checkNUpdateBSF(answer, l2SquareEarlySIMD(config->series_length, query_values,
                                                          index->values + config->series_length * node->ids[j],
                                                          getBSF(answer)));
            }
        }

#ifdef FINE_TIMING
        clog_info(CLOG(CLOGGER_ID), "query %d - approximate search = %lums", i,
                  (clock() - start_clock) * 1000 / CLOCKS_PER_SEC);
#endif

        if (config->exact_search && !(answer->size == answer->k && getBSF(answer) == 0)) {
            logAnswer(querySet->query_size + i, answer);

#ifdef FINE_TIMING
            start_clock = clock();
#endif

            for (unsigned int j = 0; j < num_leaves; ++j) {
                if (leaves[j] == node) {
                    leaf_distances[j] = VALUE_MAX;
                } else {
                    leaf_distances[j] = l2SquareValue2SAXByMaskSIMD(config->sax_length, query_summarization,
                                                                    leaves[j]->sax, leaves[j]->masks,
                                                                    index->breakpoints, scale_factor);
                }
            }

            if (config->sort_leaves) {
                qSortBy(leaves, leaf_distances, 0, (int) (num_leaves - 1));
            }

#ifdef FINE_TIMING
            clog_info(CLOG(CLOGGER_ID), "query %d - calculate leaves distances = %lums", i,
                      (clock() - start_clock) * 1000 / CLOCKS_PER_SEC);
#endif

#ifdef DEBUG
            for (unsigned int j = 0; j < num_leaves - 1; ++j) {
                Value distance = VALUE_MAX, next_distance = VALUE_MAX;
                if (leaves[j] != node) {
                    distance = l2SquareValue2SAXByMaskSIMD(config->sax_length, query_summarization,
                                                           leaves[j]->sax, leaves[j]->masks,
                                                           index->breakpoints, scale_factor);
                } else if (leaves[j + 1] != node) {
                    next_distance = l2SquareValue2SAXByMaskSIMD(config->sax_length, query_summarization,
                                                                leaves[j + 1]->sax, leaves[j + 1]->masks,
                                                                index->breakpoints, scale_factor);
                }

                if (VALUE_G(leaf_distances[j], leaf_distances[j + 1])) {
                    clog_debug(CLOG(CLOGGER_ID), "query - disordered leaves (stored) %d-%d = %f ~ %f", j, j + 1,
                               leaf_distances[j], leaf_distances[j + 1]);
                }

                if (VALUE_G(distance, next_distance)) {
                    clog_debug(CLOG(CLOGGER_ID), "query - disordered leaves (calculated) %d-%d = %f ~ %f", j, j + 1,
                               distance, next_distance);
                }

                if (VALUE_NEQ(distance,leaf_distances[j])) {
                    clog_debug(CLOG(CLOGGER_ID), "query - mismatched leaves %d = %f ~ %f (stored)", j, distance,
                               leaf_distances[j]);
                }

                if (j + 2 == num_leaves && VALUE_NEQ(next_distance, leaf_distances[j + 1])) {
                    clog_debug(CLOG(CLOGGER_ID), "query - mismatched leaves %d = %f ~ %f (stored)", j + 1, distance,
                               leaf_distances[j + 1]);
                }
            }
#endif

#ifdef FINE_TIMING
            start_clock = clock();
#endif

            pthread_t threads[config->max_threads];

            for (unsigned int shared_leaf_id = 0, j = 0; j < config->max_threads; ++j) {
                queryCache[j].answer = answer;
                queryCache[j].index = index;

                queryCache[j].num_leaves = num_leaves;
                queryCache[j].leaves = (Node const *const *) leaves;
                queryCache[j].leaf_distances = (Value const *) leaf_distances;

                queryCache[j].query_values = (Value const *) query_values;
                queryCache[j].query_summarization = (Value const *) query_summarization;

                queryCache[j].scale_factor = scale_factor;

                queryCache[j].shared_leaf_id = &shared_leaf_id;

                queryCache[j].masks8 = (SAXMask const *) masks8;

                pthread_create(&threads[j], NULL, queryThread, (void *) &queryCache[j]);
            }

            for (unsigned int j = 0; j < config->max_threads; ++j) {
                pthread_join(threads[j], NULL);
            }
        }

#ifdef FINE_TIMING
        clog_info(CLOG(CLOGGER_ID), "query %d - exact search = %lums", i,
                  (clock() - start_clock) * 1000 / CLOCKS_PER_SEC);
#endif

#ifdef PROFILING
        clog_info(CLOG(CLOGGER_ID), "query %d - %d visited / %d leaves", i, visited_leaves_counter_profiling,
                  num_leaves);
        clog_info(CLOG(CLOGGER_ID), "query %d - %d calculated / %d visited / %d loaded series", i,
                  calculated_series_counter_profiling, visited_series_counter_profiling, config->database_size);
#endif

        logAnswer(i, answer);
        cleanAnswer(answer);
    }

    freeAnswer(answer);
    free(leaves);
    free(leaf_distances);
}
