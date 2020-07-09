//
// Created by Qitong Wang on 2020/6/28.
//

#include "index_engine.h"


typedef struct IndexCache {
    Index *index;

    unsigned int *shared_start_id;
    unsigned int block_size;

    unsigned int initial_leaf_size;
    unsigned int leaf_size;

    bool split_by_summarizations;
} IndexCache;


Node *route(Node const *parent, SAXWord const *sax, unsigned int num_segments) {
    for (unsigned int i = 0; i < num_segments; ++i) {
        if (parent->right->masks[i] != parent->masks[i]) {
            if ((parent->right->masks[i] & sax[i]) == 0u) {
                return parent->left;
            } else {
                return parent->right;
            }
        }
    }

    clog_error(CLOG(CLOGGER_ID), "cannot find affiliated child node");
    exit(EXIT_FAILURE);
}


void insertNode(Node *leaf, unsigned int id, unsigned int initial_leaf_size, unsigned int leaf_size) {
    if (leaf->capacity == 0) {
        leaf->ids = malloc(sizeof(unsigned int) * (leaf->capacity = initial_leaf_size));
    } else if (leaf->size == leaf->capacity) {
        if ((leaf->capacity *= 2) > leaf_size) {
            leaf->capacity = leaf_size;
        }
        leaf->ids = realloc(leaf->ids, sizeof(unsigned int) * leaf->capacity);
    }

    leaf->ids[leaf->size++] = id;
}


unsigned int decideSplitSegmentByNextBit(Index *index, Node *parent, unsigned int num_segments) {
    unsigned int segment_to_split = -1;
    int smallest_difference = (int) parent->size;
    for (unsigned int i = 0; i < num_segments; ++i) {
        if ((parent->masks[i] & 1u) == 0u) {
            int difference = 0;
            SAXMask next_bit = parent->masks[i] >> 1u;

            for (unsigned int j = 0; j < parent->size; ++j) {
                if ((index->saxs[index->sax_length * parent->ids[j] + i] & next_bit) == 0u) {
                    difference -= 1;
                } else {
                    difference += 1;
                }
            }

            if (abs(difference) < smallest_difference) {
                segment_to_split = i;
                smallest_difference = abs(difference);

#ifdef DEBUG
                clog_debug(CLOG(CLOGGER_ID), "index - difference of segment %d of mask %d = %d ~ %d", i, next_bit,
                           difference, smallest_difference);
#endif
            }
        }
    }

    // TODO legalize some leaning situations?
    //          8000
    //         /    \
    //       8000    0
    //      /    \
    //   3000   5000
    // which can also be directly split on the next two bits
    if (segment_to_split == -1) {
        clog_error(CLOG(CLOGGER_ID), "cannot find segment to split");
        exit(EXIT_FAILURE);
    }

    return segment_to_split;
}


unsigned int decideSplitSegmentByDistribution(Index *index, Node *parent, unsigned int num_segments) {
    unsigned int segment_to_split = -1;
    double bsf = VALUE_MAX;

    for (unsigned int i = 0; i < num_segments; ++i) {
        if ((parent->masks[i] & 1u) == 0u) {
            SAXMask next_mask = parent->masks[i] >> 1u;
            double tmp, mean = 0, std = 0;

            for (unsigned int j = 0; j < parent->size; ++j) {
                mean += (tmp = index->summarizations[index->sax_length * parent->ids[j] + i]);
                std += tmp * tmp;
            }

            mean /= parent->size;
            std = 3 * sqrt(std / parent->size - mean * mean);

            tmp = index->breakpoints[OFFSETS_BY_SEGMENTS[i] + OFFSETS_BY_MASK[next_mask] +
                                     (((unsigned int) parent->sax[i] >> SHIFTS_BY_MASK[next_mask]) | 1u)];

//#ifdef DEBUG
//            clog_debug(CLOG(CLOGGER_ID), "index - mean/3*std/breakpoint(%d/%d@%d+%d+%d) of s%d(%d/%d) = %f/%f/%f",
//                       next_mask, ((unsigned int) parent->sax[i] >> SHIFTS_BY_MASK[next_mask]),
//                       OFFSETS_BY_SEGMENTS[i], OFFSETS_BY_MASK[next_mask],
//                       (((unsigned int) parent->sax[i] >> SHIFTS_BY_MASK[next_mask]) | 1u), i, parent->sax[i],
//                       parent->masks[i], mean, std, tmp);
//#endif

            if (VALUE_GEQ(tmp, mean - std) && VALUE_LEQ(tmp, mean + std) && VALUE_L(fabs(tmp - mean), bsf)) {
                bsf = fabs(tmp - mean);
                segment_to_split = i;

#ifdef DEBUG
                clog_debug(CLOG(CLOGGER_ID), "index - mean2breakpoint of s%d (%d / %d-->%d) = %f",
                           i, parent->sax[i], parent->masks[i], next_mask, bsf);
#endif
            }
        }
    }

    if (segment_to_split == -1) {
        clog_error(CLOG(CLOGGER_ID), "cannot find segment to split");
        exit(EXIT_FAILURE);
    }

    return segment_to_split;
}


void splitNode(Index *index, Node *parent, unsigned int num_segments, bool split_by_summarizations) {
#ifdef DEBUG
    for (unsigned int i = 0; i < num_segments; i += 8) {
        clog_debug(CLOG(CLOGGER_ID), "index - sax %d-%d (node) = %d/%d %d/%d %d/%d %d/%d %d/%d %d/%d %d/%d %d/%d",
                   i, i + 4, parent->sax[i], parent->masks[i], parent->sax[i + 1], parent->masks[i + 1],
                   parent->sax[i + 2], parent->masks[i + 2], parent->sax[i + 3], parent->masks[i + 3],
                   parent->sax[i + 4], parent->masks[i + 4], parent->sax[i + 5], parent->masks[i + 5],
                   parent->sax[i + 6], parent->masks[i + 6], parent->sax[i + 7], parent->masks[i + 7]);
    }

//    for (unsigned int i = 0; i < parent->size; ++i) {
//        for (unsigned int j = 0; j < num_segments; j += 8) {
//            unsigned int offset = index->sax_length * parent->ids[i] + j;
//            clog_debug(CLOG(CLOGGER_ID), "index - sax %d-%d (series %d) = %d %d %d %d %d %d %d %d", j, j + 8, i,
//                       index->saxs[offset], index->saxs[offset + 1], index->saxs[offset + 2], index->saxs[offset + 3],
//                       index->saxs[offset + 4], index->saxs[offset + 5],
//                       index->saxs[offset + 6], index->saxs[offset + 7]);
//        }
//    }
#endif

    unsigned int segment_to_split;

    if (split_by_summarizations) {
        segment_to_split = decideSplitSegmentByDistribution(index, parent, num_segments);
    } else {
        segment_to_split = decideSplitSegmentByNextBit(index, parent, num_segments);
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
        if ((index->saxs[index->sax_length * parent->ids[i] + segment_to_split] & child_masks[segment_to_split]) ==
            0u) {
            insertNode(parent->left, parent->ids[i], parent->capacity, parent->capacity);
        } else {
            insertNode(parent->right, parent->ids[i], parent->capacity, parent->capacity);
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

    unsigned int start_id;
    while ((start_id = __sync_fetch_and_add(indexCache->shared_start_id, indexCache->block_size)) <
           index->database_size) {

        unsigned int stop_id = start_id + indexCache->block_size;
        if (stop_id > index->database_size) {
            stop_id = index->database_size;
        }

        for (unsigned int i = start_id; i < stop_id; ++i) {
            SAXWord const *sax = index->saxs + index->sax_length * i;
            Node *node = index->roots[rootSAX2ID(sax, index->sax_length, index->sax_cardinality)];

            pthread_mutex_lock(node->lock);

            while (node->left != NULL || (node->capacity != 0 && node->size == indexCache->leaf_size)) {
                Node *parent = node;

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
    unsigned int num_blocks = (int) ceil((double) config->database_size / (double) config->index_block_size);
    if (num_threads > num_blocks) {
        num_threads = num_blocks;
    }

    pthread_t threads[num_threads];
    IndexCache indexCache[num_threads];

#ifdef FINE_TIMING
    clock_t start_clock = clock();
#endif

    for (unsigned int shared_start_id = 0, i = 0; i < num_threads; ++i) {
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
    clog_info(CLOG(CLOGGER_ID), "index - build = %lums", (clock() - start_clock) * 1000 / CLOCKS_PER_SEC);
#endif
}
