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
#include "clog.h"


typedef struct Node {
    pthread_mutex_t *lock;
    SAXWord *sax;
    SAXMask *masks;

    unsigned int *ids;
    unsigned int size;
    unsigned int capacity;

    struct Node *left;
    struct Node *right;
} Node;


Node *initializeNode(SAXWord *saxWord, SAXMask *saxMask);

void inspectNode(Node *node, unsigned int *num_series, unsigned int *num_leaves, unsigned int *num_roots);

void freeNode(Node *node, bool free_mask, bool free_sax);

#endif //ISAX_NODE_H
