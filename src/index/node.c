//
// Created by Qitong Wang on 2020/6/28.
//

#include "node.h"


Node *initializeNode(SAXWord *sax, SAXMask *masks) {
    Node *node = malloc(sizeof(Node));

    node->sax = sax;
    node->masks = masks;
    node->positions = NULL;

    node->size = 0;
    node->capacity = 0;

    node->left = NULL;
    node->right = NULL;

    assert(pthread_mutex_init(node->lock, NULL) == 0);

    return node;
}
