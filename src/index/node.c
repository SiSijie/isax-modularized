//
// Created by Qitong Wang on 2020/6/28.
//

#include "node.h"


Node *initializeNode(SAXWord *sax, SAXMask *masks) {
    Node *node = malloc(sizeof(Node));

    node->sax = sax;
    node->masks = masks;
    node->ids = NULL;

    node->size = 0;
    node->capacity = 0;

    node->left = NULL;
    node->right = NULL;

    node->lock = malloc(sizeof(pthread_mutex_t));
    assert(pthread_mutex_init(node->lock, NULL) == 0);

    return node;
}


void inspectNode(Node *node, size_t *num_series, size_t *num_leaves, size_t *num_roots) {
    if (node != NULL) {
        if (node->size != 0) {
            if (num_roots != NULL) {
                *num_roots += 1;
            }

            *num_leaves += 1;
            *num_series += node->size;
        } else if (node->size == 0 && node->left != NULL) {
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

        if (free_sax) {
            free(node->sax);
        }

        free(node->ids);
        pthread_mutex_destroy(node->lock);
        free(node->lock);
        free(node);
    }
}
