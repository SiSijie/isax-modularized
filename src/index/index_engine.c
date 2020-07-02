//
// Created by Qitong Wang on 2020/6/28.
//

#include "index_engine.h"


typedef struct IndexCache {
    Index *index;

    size_t *shared_start_position;

    size_t block_size;

    size_t initial_leaf_size;
    size_t leaf_size;

} IndexCache;


Node *route(Node const *parent, SAXWord const *sax, size_t num_segments) {
    Node const *leftChild = parent->left;

    for (size_t i = 0; i < num_segments; ++i) {
        if (leftChild->masks[i] != parent->masks[i]) {
            if ((leftChild->masks[i] & sax[i]) != (parent->masks[i] & sax[i])) {
                return parent->left;
            } else {
                return parent->right;
            }
        }
    }

    clog_error(CLOG(CLOGGER_ID), "cannot find affiliated child node");
    exit(EXIT_FAILURE);
}


void insertNode(Node *leaf, size_t position, size_t initial_leaf_size, size_t leaf_size) {
    if (leaf->capacity == 0) {
        leaf->positions = malloc(sizeof(size_t) * initial_leaf_size);
    } else if (leaf->size == leaf->capacity) {
        size_t next_leaf_size = leaf->capacity * 2;
        if (next_leaf_size > leaf_size) {
            next_leaf_size = leaf_size;
        }

        leaf->positions = realloc(leaf->positions, next_leaf_size);
    }

    leaf->positions[leaf->size] = position;
    leaf->size += 1;
}


void splitNode(Index *index, Node *parent, SAXWord *sax, size_t num_segments) {
    size_t segment_to_split = -1;
    int smallest_difference = parent->size;

    for (size_t i = 0; i < num_segments; ++i) {
        if ((parent->masks[i] & 1u) == 0) {
            int ones = 0, zeros = 0;
            SAXMask next_bit = parent->masks[i] >> 1u;

            for (size_t j = 0; j < parent->size; ++j) {
                if ((index->saxs[parent->positions[j] + i] & next_bit) == 0) {
                    zeros += 1;
                } else {
                    ones += 1;
                }
            }

            if (abs(ones - zeros) < smallest_difference) {
                segment_to_split = i;
                smallest_difference = abs(ones - zeros);
            }
        }
    }

    if (segment_to_split == -1) {
        clog_error(CLOG(CLOGGER_ID), "cannot find segment to split");
        exit(EXIT_FAILURE);
    }

    SAXMask *child_masks = malloc(sizeof(SAXMask) * num_segments);
    child_masks[segment_to_split] >>= 1u;

    SAXWord *right_sax = malloc(sizeof(SAXWord) * num_segments);
    memcpy(right_sax, parent->sax, sizeof(SAXWord) * num_segments);
    right_sax[segment_to_split] |= child_masks[segment_to_split];

    Node *left_child = initializeNode(parent->sax, child_masks);
    Node *right_child = initializeNode(right_sax, child_masks);

    for (size_t i = 0; i < parent->size; ++i) {
        if ((index->saxs[parent->positions[i] + segment_to_split] & child_masks[segment_to_split]) == 0) {
            insertNode(left_child, parent->positions[i], parent->capacity, parent->capacity);
        } else {
            insertNode(right_child, parent->positions[i], parent->capacity, parent->capacity);
        }
    }

    parent->size = 0;
    parent->capacity = 0;
    free(parent->positions);
    parent->positions = NULL;
}


void *buildIndexThread(void *cache) {
    IndexCache *indexCache = (IndexCache *) cache;
    Index *index = indexCache->index;

    size_t start_position;
    while ((start_position = __sync_fetch_and_add(indexCache->shared_start_position, indexCache->block_size)) <
           index->database_size) {
        size_t stop_position = start_position + indexCache->block_size;
        if (stop_position > index->database_size) {
            stop_position = index->database_size;
        }

        for (size_t i = start_position; i < stop_position; ++i) {
            SAXWord *sax = index->saxs + index->sax_length * i;
            Node *node = index->roots[rootSAX2Position(sax, index->sax_length, index->sax_cardinality)];

            pthread_mutex_lock(node->lock);
            while (node->left != NULL) {
                pthread_mutex_unlock(node->lock);
                node = route(node, sax, index->sax_length);
                pthread_mutex_lock(node->lock);
            }

            if (node->size == indexCache->leaf_size) {
                Node *parent = node;

                splitNode(index, parent, sax, index->sax_length);
                node = route(parent, sax, index->sax_length);

                pthread_mutex_unlock(parent->lock);
                pthread_mutex_lock(node->lock);
            }

            insertNode(node, i, indexCache->initial_leaf_size, indexCache->leaf_size);

            pthread_mutex_unlock(node->lock);
        }
    }

    return NULL;
}


void buildIndex(Config const *config, Index *index) {
    int num_threads = config->max_threads;
    int num_blocks = (int) ceil((double) config->database_size / (double) config->index_block_size);
    if (num_threads > num_blocks) {
        num_threads = num_blocks;
    }

    size_t shared_start_position = 0;

    pthread_t threads[num_threads];
    IndexCache indexCache[num_threads];

    for (int i = 0; i < num_threads; ++i) {
        indexCache[i].index = index;
        indexCache[i].leaf_size = config->leaf_size;
        indexCache[i].initial_leaf_size = config->initial_leaf_size;
        indexCache[i].block_size = config->index_block_size;
        indexCache[i].shared_start_position = &shared_start_position;

        pthread_create(&threads[i], NULL, buildIndexThread, (void *) &indexCache[i]);
    }

    for (int i = 0; i < num_threads; ++i) {
        pthread_join(threads[i], NULL);
    }
}
