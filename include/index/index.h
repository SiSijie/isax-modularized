//
// Created by Qitong Wang on 2020/6/28.
//

#ifndef ISAX_INDEX_H
#define ISAX_INDEX_H

#include <time.h>

#include "globals.h"
#include "node.h"
#include "config.h"
#include "clog.h"
#include "breakpoints.h"
#include "paa.h"
#include "sax.h"


typedef struct Index {
    Node **roots;

    size_t roots_size;
    size_t num_leaves;

    Value const *values;

    size_t database_size;
    size_t series_length;

    Value const *summarizations;
    Value const *const *breakpoints;
    SAXWord const *saxs;

    size_t sax_length;
    unsigned int sax_cardinality;
} Index;

Node *route(Node const *parent, SAXWord const *sax, size_t num_segments);

size_t rootSAX2ID(SAXWord const *saxs, size_t num_segments, unsigned int cardinality);

Index *initializeIndex(Config const *config);

void finalizeIndex(Index *index);

void freeIndex(Config const *config, Index *index);

void logIndex(Index *index);

#endif //ISAX_INDEX_H
