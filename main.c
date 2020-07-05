//
// Created by Qitong Wang on 2020/6/28.
//

#define FINE_TIMING
//#define TIMING

#include <stdio.h>
#include <time.h>

#ifndef CLOG_MAIN
#define CLOG_MAIN
#endif

#include "clog.h"
#include "globals.h"
#include "config.h"
#include "index.h"
#include "index_engine.h"
#include "query.h"
#include "query_engine.h"


int main(int argc, char **argv) {
    Config const *config = initializeConfig(argc, argv);

    int clog_init_code = clog_init_path(CLOGGER_ID, config->log_filepath);
    if (clog_init_code != 0) {
        fprintf(stderr, "Logger initialization failed, log_filepath = %s\n", config->log_filepath);
        exit(EXIT_FAILURE);
    }

    logConfig(config);

#ifdef TIMING
    clock_t start_clock = clock();
#endif

    Index *index = initializeIndex(config);
    buildIndex(config, index);
    finalizeIndex(index);

#ifdef TIMING
    clog_info(CLOG(CLOGGER_ID), "index - overall = %lums", (clock() - start_clock) * 1000 / CLOCKS_PER_SEC);
#endif

    logIndex(index);

#ifdef TIMING
    start_clock = clock();
#endif

    QuerySet *queries = initializeQuery(config, index);
    conductQueries(queries, index, config);

#ifdef TIMING
    clog_info(CLOG(CLOGGER_ID), "query - overall = %lums", (clock() - start_clock) * 1000 / CLOCKS_PER_SEC);
#endif

    freeIndex(config, index);
    freeQuery(queries);
    free((Config *) config);

    clog_free(CLOGGER_ID);
    return 0;
}
