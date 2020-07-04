//
// Created by Qitong Wang on 2020/6/28.
//

#ifndef ISAX_NODE_H
#define ISAX_NODE_H

#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>

#include "globals.h"


typedef struct Node {
    pthread_mutex_t *lock;
    SAXWord *sax;
    SAXMask *masks;

    size_t *ids;
    size_t size;
    size_t capacity;

    struct Node *left;
    struct Node *right;
} Node;


Node *initializeNode(SAXWord *saxWord, SAXMask *saxMask);

void inspectNode(Node *node, size_t *num_series, size_t *num_leaves, size_t *num_roots);

void freeNode(Node *node, bool free_mask, bool free_sax);

#endif //ISAX_NODE_H
