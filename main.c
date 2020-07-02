//
// Created by Qitong Wang on 2020/6/28.
//

#include <stdio.h>

#ifndef CLOG_MAIN
#define CLOG_MAIN
#endif

#include "clog.h"
#include "globals.h"
#include "config.h"
#include "index.h"
#include "index_engine.h"


int main(int argc, char **argv) {
    Config *config = initializeConfig(argc, argv);

    int clog_init_code = clog_init_path(CLOGGER_ID, config->log_filepath);
    if (clog_init_code != 0) {
        fprintf(stderr, "Logger initialization failed, log_filepath= %s.\n", config->log_filepath);
        exit(EXIT_FAILURE);
    }

    Index *index = initializeIndex(config);
    buildIndex(config, index);
    finalizeIndex(index);
    logIndex(index);

    freeIndex(config, index);
    clog_free(CLOGGER_ID);

    free(config);
    return 0;
}
