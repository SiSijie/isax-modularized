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


void splitNode(Index *index, Node *parent, unsigned int num_segments) {
    unsigned int segment_to_split = -1;
    int smallest_difference = (int) parent->size;

    for (unsigned int i = 0; i < num_segments; ++i) {
        if ((parent->masks[i] & 1u) == 0u) {
            int differences = 0;
            SAXMask next_bit = parent->masks[i] >> 1u;

            for (unsigned int j = 0; j < parent->size; ++j) {
                if ((index->saxs[index->sax_length * parent->ids[j] + i] & next_bit) == 0u) {
                    differences -= 1;
                } else {
                    differences += 1;
                }
            }

            if (abs(differences) < smallest_difference) {
                segment_to_split = i;
                smallest_difference = abs(differences);
            }
        }
    }

    // TODO legalize some leaning situations?
    //          8000
    //         /    \
    //       8000    0
    //      /    \
    //   3000
    // which can also be directly split on the next two bits
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
        if ((index->saxs[index->sax_length * parent->ids[i] + segment_to_split] & child_masks[segment_to_split]) == 0u) {
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
                    splitNode(index, parent, index->sax_length);
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

        pthread_create(&threads[i], NULL, buildIndexThread, (void *) &indexCache[i]);
    }

    for (unsigned int i = 0; i < num_threads; ++i) {
        pthread_join(threads[i], NULL);
    }

#ifdef FINE_TIMING
    clog_info(CLOG(CLOGGER_ID), "index - build = %lums", (clock() - start_clock) * 1000 / CLOCKS_PER_SEC);
#endif
}
