//
// Created by Qitong Wang on 2020/6/28.
//

#ifndef ISAX_INDEX_ENGINE_H
#define ISAX_INDEX_ENGINE_H

#include <time.h>
#include <math.h>
#include <pthread.h>

#include "globals.h"
#include "index.h"
#include "clog.h"
#include "config.h"
#include "breakpoints.h"
#include "paa.h"

void buildIndex(Config const *config, Index *index);

void finalizeIndex(Index *index);

#endif //ISAX_INDEX_ENGINE_H
