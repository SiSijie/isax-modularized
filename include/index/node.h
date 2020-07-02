//
// Created by Qitong Wang on 2020/6/28.
//

#ifndef ISAX_NODE_H
#define ISAX_NODE_H

#include <assert.h>
#include <stdlib.h>
#include <pthread.h>

#include "globals.h"


typedef struct Node {
    pthread_mutex_t *lock;
    SAXWord *sax;
    SAXMask *masks;

    size_t *positions;
    size_t size;
    size_t capacity;

    struct Node *left;
    struct Node *right;
} Node;

Node *initializeNode(SAXWord *saxWord, SAXMask *saxMask);

#endif //ISAX_NODE_H
