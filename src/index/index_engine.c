//
// Created by Qitong Wang on 2020/6/28.
//

#include "index_engine.h"


typedef struct IndexCache {
    Index *index;

    ID *shared_start_id;
    unsigned int block_size;

    unsigned int initial_leaf_size;
    unsigned int leaf_size;

    bool split_by_summarizations;
} IndexCache;


Node *route(Node const *parent, SAXWord const *sax, unsigned int num_segments) {
    for (unsigned int i = 0; i < num_segments; ++i) {
        if (parent->right->masks[i] != parent->masks[i]) {
            if (parent->right->masks[i] & sax[i]) {
                return parent->right;
            } else {
                return parent->left;
            }
        }
    }

    clog_error(CLOG(CLOGGER_ID), "cannot find affiliated child node");
    exit(EXIT_FAILURE);
}


unsigned int decideSplitSegmentByNextBit(Index *index, Node *parent, unsigned int num_segments) {
    unsigned int segment_to_split = -1;
    int bsf_difference = (int) parent->size + 1, local_difference;
    SAXMask next_bit;

    for (unsigned int i = 0; i < num_segments; ++i) {
        if (parent->masks[i] ^ 1u) {
            local_difference = 0;
            next_bit = parent->masks[i] >> 1u;

            for (unsigned int j = 0; j < parent->size; ++j) {
                if (index->saxs[index->sax_length * parent->ids[j] + i] & next_bit) {
                    local_difference += 1;
                } else {
                    local_difference -= 1;
                }
            }

            local_difference = abs(local_difference);
            if (local_difference < bsf_difference) {
                segment_to_split = i;
                bsf_difference = abs(local_difference);
#ifdef DEBUG
                clog_debug(CLOG(CLOGGER_ID), "index - difference of segment %d of mask %d = %d", i, next_bit,
                           local_difference);
#endif
            } else if (local_difference == bsf_difference && parent->masks[i] > parent->masks[segment_to_split]) {
                segment_to_split = i;
#ifdef DEBUG
                clog_debug(CLOG(CLOGGER_ID), "index - difference of segment %d of mask %d = %d", i, next_bit,
                           local_difference);
#endif
            }
        }
    }

#ifdef DEBUG
    if (bsf_difference == parent->size) {
        for (unsigned int i = 0; i < num_segments; i += 8) {
            clog_debug(CLOG(CLOGGER_ID), "index - sax %d-%d (node) = %d/%d %d/%d %d/%d %d/%d %d/%d %d/%d %d/%d %d/%d",
                       i, i + 4, parent->sax[i], parent->masks[i], parent->sax[i + 1], parent->masks[i + 1],
                       parent->sax[i + 2], parent->masks[i + 2], parent->sax[i + 3], parent->masks[i + 3],
                       parent->sax[i + 4], parent->masks[i + 4], parent->sax[i + 5], parent->masks[i + 5],
                       parent->sax[i + 6], parent->masks[i + 6], parent->sax[i + 7], parent->masks[i + 7]);
        }

        for (unsigned int i = 0; i < parent->size; ++i) {
            for (unsigned int j = 0; j < num_segments; j += 8) {
                size_t summarization_offset = index->sax_length * parent->ids[i] + j;
                clog_debug(CLOG(CLOGGER_ID), "index - summarizations %d-%d (series %d) = %f %f %f %f %f %f %f %f", j, j + 8, i,
                           index->summarizations[summarization_offset], index->summarizations[summarization_offset + 1],
                           index->summarizations[summarization_offset + 2], index->summarizations[summarization_offset + 3],
                           index->summarizations[summarization_offset + 4], index->summarizations[summarization_offset + 5],
                           index->summarizations[summarization_offset + 6], index->summarizations[summarization_offset + 7]);

                size_t sax_offset = index->sax_length * parent->ids[i] + j;
                clog_debug(CLOG(CLOGGER_ID), "index - sax %d-%d (series %d) = %d %d %d %d %d %d %d %d", j, j + 8, i,
                           index->saxs[sax_offset], index->saxs[sax_offset + 1],
                           index->saxs[sax_offset + 2], index->saxs[sax_offset + 3],
                           index->saxs[sax_offset + 4], index->saxs[sax_offset + 5],
                           index->saxs[sax_offset + 6], index->saxs[sax_offset + 7]);
            }
        }
    }
#endif

    return segment_to_split;
}


unsigned int decideSplitSegmentByDistribution(Index *index, Node *parent, unsigned int num_segments) {
    unsigned int segment_to_split = -1;
    double bsf = VALUE_MAX, local_bsf, tmp, mean, stdx3;
    SAXMask next_mask;

    for (unsigned int i = 0; i < num_segments; ++i) {
        if (parent->masks[i] ^ 1u) {
            next_mask = parent->masks[i] >> 1u;
            mean = 0, stdx3 = 0;

            for (unsigned int j = 0; j < parent->size; ++j) {
                tmp = index->summarizations[index->sax_length * parent->ids[j] + i];
                mean += tmp;
                stdx3 += tmp * tmp;
            }

            mean /= parent->size;
            stdx3 = 3 * sqrt(stdx3 / parent->size - mean * mean);

            tmp = index->breakpoints[OFFSETS_BY_SEGMENTS[i] + OFFSETS_BY_MASK[next_mask] +
                                     (((unsigned int) parent->sax[i] >> SHIFTS_BY_MASK[next_mask]) | 1u)];
//#ifdef DEBUG
//            clog_debug(CLOG(CLOGGER_ID), "index - mean/3*std/breakpoint(%d/%d@%d+%d+%d) of s%d(%d/%d) = %f/%f/%f",
//                       next_mask, ((unsigned int) parent->sax[i] >> SHIFTS_BY_MASK[next_mask]),
//                       OFFSETS_BY_SEGMENTS[i], OFFSETS_BY_MASK[next_mask],
//                       (((unsigned int) parent->sax[i] >> SHIFTS_BY_MASK[next_mask]) | 1u), i, parent->sax[i],
//                       parent->masks[i], mean, std, tmp);
//#endif
            if (VALUE_GEQ(tmp, mean - stdx3) && VALUE_LEQ(tmp, mean + stdx3)) {
                local_bsf = fabs(tmp - mean);
                if (VALUE_L(local_bsf, bsf)) {
                    bsf = local_bsf;
                    segment_to_split = i;
//#ifdef DEBUG
//                    clog_debug(CLOG(CLOGGER_ID), "index - mean2breakpoint of s%d (%d / %d-->%d) = %f",
//                               i, parent->sax[i], parent->masks[i], next_mask, bsf);
//#endif
                } else if (VALUE_EQ(local_bsf, bsf) && parent->masks[i] > parent->masks[segment_to_split]) {
                    segment_to_split = i;
//#ifdef DEBUG
//                    clog_debug(CLOG(CLOGGER_ID), "index - mean2breakpoint of s%d (%d / %d-->%d) = %f",
//                               i, parent->sax[i], parent->masks[i], next_mask, bsf);
//#endif
                }
            }
        }
    }

    return segment_to_split;
}


void splitNode(Index *index, Node *parent, unsigned int num_segments, bool split_by_summarizations) {
    unsigned int segment_to_split;

    if (split_by_summarizations) {
        segment_to_split = decideSplitSegmentByDistribution(index, parent, num_segments);
    } else {
        segment_to_split = decideSplitSegmentByNextBit(index, parent, num_segments);
    }

    if (segment_to_split == -1) {
        clog_error(CLOG(CLOGGER_ID), "cannot find segment to split");
        exit(EXIT_FAILURE);
    }

    SAXMask *child_masks = malloc(sizeof(SAXMask) * num_segments);
    memcpy(child_masks, parent->masks, sizeof(SAXMask) * num_segments);
    child_masks[segment_to_split] >>= 1u;

    SAXWord *right_sax = malloc(sizeof(SAXWord) * num_segments);
    memcpy(right_sax, parent->sax, sizeof(SAXWord) * num_segments);
    right_sax[segment_to_split] |= child_masks[segment_to_split];

    parent->left = initializeNode(parent->sax, child_masks);
    parent->right = initializeNode(right_sax, child_masks);

    for (unsigned int i = 0; i < parent->size; ++i) {
        if (index->saxs[index->sax_length * parent->ids[i] + segment_to_split] & child_masks[segment_to_split]) {
            insertNode(parent->right, parent->ids[i], parent->capacity, parent->capacity);
        } else {
            insertNode(parent->left, parent->ids[i], parent->capacity, parent->capacity);
        }
    }

    parent->size = 0;
    parent->capacity = 0;

    free(parent->ids);
    parent->ids = NULL;
}


void *buildIndexThread(void *cache) {
    IndexCache *indexCache = (IndexCache *) cache;
    Index *index = indexCache->index;

    ID database_size = index->database_size, local_start_id, local_stop_id;
    ID *shared_start_id = indexCache->shared_start_id;
    unsigned int block_size = indexCache->block_size;

    SAXWord const *sax;
    Node *node, *parent;

    while ((local_start_id = __sync_fetch_and_add(shared_start_id, block_size)) < database_size) {
        local_stop_id = local_start_id + block_size;
        if (local_stop_id > database_size) {
            local_stop_id = database_size;
        }

        for (ID i = local_start_id; i < local_stop_id; ++i) {
            sax = index->saxs + index->sax_length * i;
            node = index->roots[rootSAX2ID(sax, index->sax_length, index->sax_cardinality)];

            pthread_mutex_lock(node->lock);

            while (node->left != NULL || (node->capacity != 0 && node->size == indexCache->leaf_size)) {
                parent = node;

                if (node->size == indexCache->leaf_size) {
                    splitNode(index, parent, index->sax_length, indexCache->split_by_summarizations);
                }

                node = route(parent, sax, index->sax_length);

                pthread_mutex_lock(node->lock);
                pthread_mutex_unlock(parent->lock);
            }

            insertNode(node, i, indexCache->initial_leaf_size, indexCache->leaf_size);

            pthread_mutex_unlock(node->lock);
        }
    }

    return NULL;
}


void buildIndex(Config const *config, Index *index) {
    unsigned int num_threads = config->max_threads;
    unsigned int num_blocks = (unsigned int) ceil((double) config->database_size / (double) config->index_block_size);
    if (num_threads > num_blocks) {
        num_threads = num_blocks;
    }

    pthread_t threads[num_threads];
    IndexCache indexCache[num_threads];

#ifdef FINE_TIMING
    struct timespec start_timestamp, stop_timestamp;
    TimeDiff time_diff;

    clock_code = clock_gettime(CLK_ID, &start_timestamp);
#endif

    ID shared_start_id = 0;
    for (unsigned int i = 0; i < num_threads; ++i) {
        indexCache[i].index = index;
        indexCache[i].leaf_size = config->leaf_size;
        indexCache[i].initial_leaf_size = config->initial_leaf_size;
        indexCache[i].block_size = config->index_block_size;
        indexCache[i].shared_start_id = &shared_start_id;
        indexCache[i].split_by_summarizations = config->split_by_summarizations;

        pthread_create(&threads[i], NULL, buildIndexThread, (void *) &indexCache[i]);
    }

    for (unsigned int i = 0; i < num_threads; ++i) {
        pthread_join(threads[i], NULL);
    }

#ifdef FINE_TIMING
    clock_code = clock_gettime(CLK_ID, &stop_timestamp);
    getTimeDiff(&time_diff, start_timestamp, stop_timestamp);

    clog_info(CLOG(CLOGGER_ID), "index - build = %ld.%lds", time_diff.tv_sec, time_diff.tv_nsec);
#endif
}


void fetchPermutation(Node *node, ssize_t *permutation, ID *counter) {
    if (node->left != NULL) {
        fetchPermutation(node->left, permutation, counter);
        fetchPermutation(node->right, permutation, counter);
    } else {
        node->start_id = (ssize_t) *counter;

        for (unsigned int i = 0; i < node->size; ++i) {
            permutation[node->ids[i]] = (ssize_t) *counter;
            *counter += 1;
        }

        free(node->ids);
        node->ids = NULL;
    }
}


void permute(Value *values, SAXWord *saxs, ssize_t *permutation, ssize_t size, unsigned int series_length,
             unsigned int sax_length) {
    unsigned int series_bytes = sizeof(Value) * series_length, sax_bytes = sizeof(SAXWord) * sax_length;

    Value *values_cache = malloc(series_bytes);
    SAXWord *sax_cache = malloc(sax_bytes);

    ssize_t tmp;
    for (ID next, i = 0; i < size; ++i) {
        next = i;

        while (permutation[next] >= 0) {
            memcpy(values_cache, values + series_length * i, series_bytes);
            memcpy(values + series_length * i, values + series_length * permutation[next], series_bytes);
            memcpy(values + series_length * permutation[next], values_cache, series_bytes);

            memcpy(sax_cache, saxs + sax_length * i, sax_bytes);
            memcpy(saxs + sax_length * i, saxs + sax_length * permutation[next], sax_bytes);
            memcpy(saxs + sax_length * permutation[next], sax_cache, sax_bytes);

            tmp = permutation[next];
            permutation[next] -= size;
            next = tmp;
        }
    }

    free(values_cache);
    free(sax_cache);
}


void squeezeNode(Node *node, Index *index, bool *segment_flags) {
    if (node->left != NULL) {
        squeezeNode(node->left, index, segment_flags);
        squeezeNode(node->right, index, segment_flags);
    } else {
        node->squeezed_masks = malloc(sizeof(SAXMask) * index->sax_length);
        for (unsigned int i = 0; i < index->sax_length; ++i) {
            node->squeezed_masks[i] = 1u;
        }

        memcpy(node->sax, index->saxs + index->sax_length * node->start_id, sizeof(SAXWord) * index->sax_length);

        if (node->size > 1) {
            int segments_nonsqueezable = 0;
            for (unsigned int i = 0; i < index->sax_length; ++i) {
                if (node->masks[i] & node->squeezed_masks[i]) {
                    segments_nonsqueezable += 1;
                    segment_flags[i] = false;
                } else {
                    segment_flags[i] = true;
                }
            }

            for (ID i = index->sax_length * (node->start_id + 1);
                 (i < index->sax_length * (node->start_id + node->size)) &&
                 (segments_nonsqueezable < index->sax_length);
                 i += index->sax_length) {
                for (unsigned j = 0; j < index->sax_length; ++j) {
                    if (segment_flags[j]) {
                        for (unsigned int k = BITS_BY_MASK[node->masks[j]] + 1;
                             k < BITS_BY_MASK[node->squeezed_masks[j]]; ++k) {
                            if (((unsigned) index->saxs[i + j] ^ (unsigned) node->sax[j]) & MASKS_BY_BITS[k]) {
                                node->squeezed_masks[j] = MASKS_BY_BITS[k - 1];

                                if (node->squeezed_masks[j] & node->masks[j]) {
                                    segment_flags[j] = false;
                                    segments_nonsqueezable += 1;
                                }
                            }
                        }
                    }
                }
            }
        }

#ifdef DEBUG
        for (unsigned int i = 0; i < index->sax_length; ++i) {
            if (node->squeezed_masks[i] ^ node->masks[i]) {
                clog_info(CLOG(CLOGGER_ID), "index - segment %d (node.size %d) squeezed %d --> %d", i, node->size,
                          node->masks[i], node->squeezed_masks[i]);
            }
        }
#endif
    }
}


void finalizeIndex(Index *index) {
#ifdef FINE_TIMING
    struct timespec start_timestamp, stop_timestamp;
    TimeDiff time_diff;

    clock_code = clock_gettime(CLK_ID, &start_timestamp);
#endif

    ssize_t *permutation = malloc(sizeof(ssize_t) * index->database_size);
    ID counter = 0;

    for (unsigned int i = 0; i < index->roots_size; ++i) {
        if (index->roots[i]->size == 0 && index->roots[i]->left == NULL) {
            freeNode(index->roots[i], false, true);;
            index->roots[i] = NULL;
        } else {
            fetchPermutation(index->roots[i], permutation, &counter);
        }
    }

    assert(counter == index->database_size);

    permute((Value *) index->values, (SAXWord *) index->saxs, permutation, (ssize_t) index->database_size,
            index->series_length, index->sax_length);

#ifdef FINE_TIMING
    clock_code = clock_gettime(CLK_ID, &stop_timestamp);
    getTimeDiff(&time_diff, start_timestamp, stop_timestamp);

    clog_info(CLOG(CLOGGER_ID), "index - permute for memory locality = %ld.%lds", time_diff.tv_sec, time_diff.tv_nsec);
#endif

    free(permutation);

#ifdef FINE_TIMING
    clock_code = clock_gettime(CLK_ID, &start_timestamp);
#endif

    bool *segment_flags = malloc(sizeof(bool) * index->sax_length);
    for (unsigned int i = 0; i < index->roots_size; ++i) {
        if (index->roots[i] != NULL) {
            squeezeNode(index->roots[i], index, segment_flags);
        }
    }
    free(segment_flags);

#ifdef FINE_TIMING
    clock_code = clock_gettime(CLK_ID, &stop_timestamp);
    getTimeDiff(&time_diff, start_timestamp, stop_timestamp);

    clog_info(CLOG(CLOGGER_ID), "index - squeeze nodes = %ld.%lds", time_diff.tv_sec, time_diff.tv_nsec);
#endif

    if (index->summarizations != NULL) {
        free((Value *) index->summarizations);
    }
}

