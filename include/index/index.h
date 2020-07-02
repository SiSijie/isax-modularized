//
// Created by Qitong Wang on 2020/6/28.
//

#ifndef ISAX_INDEX_H
#define ISAX_INDEX_H

#include "node.h"
#include "config.h"
#include "globals.h"
#include "clog.h"
#include "breakpoints.h"
#include "paa.h"
#include "sax.h"


typedef struct Index {
    Node **roots;

    size_t roots_size;

    Value const *values;

    size_t database_size;
    size_t series_length;

    Value const *summarizations;
    Value const *const *breakpoints;
    SAXWord const *saxs;

    size_t sax_length;
    unsigned int sax_cardinality;
} Index;


size_t rootSAX2Position(SAXWord const *saxs, size_t num_segments, unsigned int cardinality);

Index *initializeIndex(Config const *config);

void finalizeIndex(Index *index);

void freeIndex(Config const *config, Index *index);

void logIndex(Index *index);

#endif //ISAX_INDEX_H
