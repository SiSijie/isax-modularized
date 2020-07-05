//
// Created by Qitong Wang on 2020/7/3.
//

#include "query_engine.h"


typedef struct QueryCache {
    Answer *answer;
    Index const *index;

    Node const *const *leaves;
    Value const *leaf_distances;
    size_t num_leaves;

    Value scale_factor;

    Value const *query_values;
    Value const *query_summarization;

    size_t *shared_leaf_id;
} QueryCache;


void *queryThread(void *cache) {
    QueryCache *queryCache = (QueryCache *) cache;

    size_t leaf_id;
    while ((leaf_id = __sync_fetch_and_add(queryCache->shared_leaf_id, 1)) < queryCache->num_leaves) {
        Node const *const leaf = queryCache->leaves[leaf_id];

        // TODO only using local_bsf suffers from the problem that approximate nearest neighbours < k and never updated
        pthread_rwlock_rdlock(queryCache->answer->lock);
        Value local_bsf = getBSF(queryCache->answer);
        pthread_rwlock_unlock(queryCache->answer->lock);

        if (VALUE_G(local_bsf, queryCache->leaf_distances[leaf_id])) {
            for (size_t i = 0; i < leaf->size; ++i) {
                if (VALUE_G(local_bsf,
                            l2SquareSummarization2SAX8(queryCache->index->sax_length, queryCache->query_summarization,
                                                       queryCache->index->saxs +
                                                       leaf->ids[i] * queryCache->index->sax_length,
                                                       queryCache->index->breakpoints, queryCache->scale_factor))) {
                    Value local_distance = l2SquareEarlySIMD(queryCache->index->series_length, queryCache->query_values,
                                                             queryCache->index->values +
                                                             leaf->ids[i] * queryCache->index->series_length,
                                                             local_bsf);
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


void enqueueLeaf(Node *node, Node **leaves, size_t *num_leaves) {
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
#ifdef FINE_TIMING
    clock_t start_clock = clock();
#endif

    size_t num_leaves = 0;
    for (size_t j = 0, num_series = 0, num_roots = 0; j < index->roots_size; ++j) {
        inspectNode(index->roots[j], &num_series, &num_leaves, &num_roots);
    }


    Node **leaves = malloc(sizeof(Node *) * num_leaves);
    num_leaves = 0;
    for (size_t j = 0; j < index->roots_size; ++j) {
        enqueueLeaf(index->roots[j], leaves, &num_leaves);
    }

#ifdef FINE_TIMING
    clog_info(CLOG(CLOGGER_ID), "query - fetch leaves = %lums", (clock() - start_clock) * 1000 / CLOCKS_PER_SEC);
#endif

    Value *leaf_distances = malloc(sizeof(Value) * num_leaves);
    Value scale_factor = (Value) config->series_length / (Value) config->sax_length;

    for (size_t i = 0; i < querySet->query_size; ++i) {
        Answer *answer = initializeAnswer(config);

        Value const *query_values = querySet->values + config->series_length * i;
        Value const *query_summarization = querySet->summarizations + config->sax_length * i;
        SAXWord const *query_sax = querySet->saxs + config->sax_length * i;

#ifdef FINE_TIMING
        start_clock = clock();
#endif

        size_t rootID = rootSAX2ID(query_sax, config->sax_length, config->sax_cardinality);
        Node *node = index->roots[rootID];
        if (node != NULL) {
            while (node->left != NULL) {
                node = route(node, query_sax, config->sax_length);
            }

            for (size_t j = 0; j < node->size; ++j) {
                checkNUpdateBSF(answer, l2SquareSIMD(config->series_length, query_values,
                                                     index->values + config->series_length * node->ids[j]));
            }
        }

#ifdef FINE_TIMING
        clog_info(CLOG(CLOGGER_ID), "query %lu - approximate search = %lums", i,
                  (clock() - start_clock) * 1000 / CLOCKS_PER_SEC);
#endif

        if (config->exact_search && !(answer->size == answer->k && getBSF(answer) == 0)) {
            logAnswer(querySet->query_size + i, answer);

            pthread_t threads[config->max_threads];
            QueryCache queryCache[config->max_threads];


#ifdef FINE_TIMING
            start_clock = clock();
#endif

            for (size_t j = 0; j < num_leaves; ++j) {
                if (leaves[j] == node) {
                    leaf_distances[j] = FLT_MAX;
                } else {
                    leaf_distances[j] = l2SquareSummarization2SAXByMask(config->sax_length, query_summarization,
                                                                        leaves[j]->sax, leaves[j]->masks,
                                                                        index->breakpoints, scale_factor);
                }
            }

            if (config->sort_leaves) {
                qSortBy(leaves, leaf_distances, 0, (int) (num_leaves - 1));
            }

#ifdef FINE_TIMING
            clog_info(CLOG(CLOGGER_ID), "query %lu - calculate leaves distances = %lums", i,
                      (clock() - start_clock) * 1000 / CLOCKS_PER_SEC);

            start_clock = clock();
#endif

            for (size_t shared_leaf_id = 0, j = 0; j < config->max_threads; ++j) {
                queryCache[j].answer = answer;
                queryCache[j].index = index;

                queryCache[j].num_leaves = num_leaves;
                queryCache[j].leaves = (Node const *const *) leaves;
                queryCache[j].leaf_distances = (Value const *) leaf_distances;

                queryCache[j].query_values = (Value const *) query_values;
                queryCache[j].query_summarization = (Value const *) query_summarization;

                queryCache[j].scale_factor = scale_factor;

                queryCache[j].shared_leaf_id = &shared_leaf_id;

                pthread_create(&threads[j], NULL, queryThread, (void *) &queryCache[j]);
            }

            for (size_t j = 0; j < config->max_threads; ++j) {
                pthread_join(threads[j], NULL);
            }
        }

#ifdef FINE_TIMING
        clog_info(CLOG(CLOGGER_ID), "query %lu - exact search = %lums", i,
                  (clock() - start_clock) * 1000 / CLOCKS_PER_SEC);
#endif

        logAnswer(i, answer);
        freeAnswer(answer);
    }

    free(leaves);
    free(leaf_distances);
}
