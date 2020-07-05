//
// Created by Qitong Wang on 2020/7/3.
//

#ifndef ISAX_QUERY_H
#define ISAX_QUERY_H

#include <time.h>
#include <float.h>
#include <stdlib.h>
#include <pthread.h>

#include "globals.h"
#include "config.h"
#include "paa.h"
#include "breakpoints.h"
#include "sax.h"
#include "index.h"
#include "clog.h"


typedef struct QuerySet {
    Value const *values;
    Value const *summarizations;
    SAXWord const *saxs;

    size_t query_size;
} QuerySet;


typedef struct Answer {
    pthread_rwlock_t *lock;

    Value *distances; // max-heap

    size_t size;
    size_t k;
} Answer;


QuerySet *initializeQuery(Config const *config, Index const *index);

void freeQuery(QuerySet *queries);

Answer *initializeAnswer(Config const *config);

void freeAnswer(Answer *answer);

int checkNUpdateBSF(Answer * answer, Value distance);

void logAnswer(size_t query_id, Answer *answer);

Value getBSF(Answer * answer);

#endif //ISAX_QUERY_H
