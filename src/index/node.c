//
// Created by Qitong Wang on 2020/6/28.
//

#include "node.h"


Node *initializeNode(SAXWord *sax, SAXMask *masks) {
    Node *node = malloc(sizeof(Node));

    node->sax = sax;
    node->masks = masks;
    node->squeezed_masks = NULL;

    node->ids = NULL;
    node->start_id = 0;

    node->size = 0;
    node->capacity = 0;

    node->left = NULL;
    node->right = NULL;

    node->lock = malloc(sizeof(pthread_mutex_t));
    assert(pthread_mutex_init(node->lock, NULL) == 0);

    node->compactness = -1;

    return node;
}


void insertNode(Node *leaf, ID id, unsigned int initial_leaf_size, unsigned int leaf_size) {
    if (leaf->capacity == 0) {
        leaf->ids = malloc(sizeof(ID) * (leaf->capacity = initial_leaf_size));
    } else if (leaf->size == leaf->capacity) {
        if ((leaf->capacity *= 2) > leaf_size) {
            leaf->capacity = leaf_size;
        }
        leaf->ids = realloc(leaf->ids, sizeof(ID) * leaf->capacity);
    }

    leaf->ids[leaf->size++] = id;
}


void inspectNode(Node *node, unsigned int *num_series, unsigned int *num_leaves, unsigned int *num_roots) {
    if (node != NULL) {
        if (node->size != 0) {
            if (num_roots != NULL) {
                *num_roots += 1;
            }

#ifdef PROFILING
            clog_info(CLOG(CLOGGER_ID), "index - node %lu = %lu", *num_leaves, node->size);
#endif

            *num_leaves += 1;
            *num_series += node->size;
        } else if (node->left != NULL) {
            if (num_roots != NULL) {
                *num_roots += 1;
            }

            inspectNode(node->left, num_series, num_leaves, NULL);
            inspectNode(node->right, num_series, num_leaves, NULL);
        }
    }
}


void freeNode(Node *node, bool free_mask, bool free_sax) {
    if (node != NULL) {
        if (node->left != NULL) {
            freeNode(node->left, false, false);
            freeNode(node->right, true, true);
        }

        if (free_mask) {
            free(node->masks);
        }

        if (node->squeezed_masks != NULL) {
            free(node->squeezed_masks);
        }

        if (free_sax) {
            free(node->sax);
        }

        if (node->ids != NULL) {
            free(node->ids);
        }

        pthread_mutex_destroy(node->lock);
        free(node->lock);
        free(node);
    }
}
