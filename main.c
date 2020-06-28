//
// Created by Qitong Wang on 2020/6/28.
//

#include <stdio.h>

#define CLOG_MAIN
#include "clog.h"

#include "globals.h"
#include "config.h"


int main(int argc, char **argv) {
    Config *config = initialize(argc, argv);

    int clog_init_code = clog_init_path(CLOGGER_ID, config->log_filepath);
    if (clog_init_code != 0) {
        fprintf(stderr, "Logger initialization failed, log_filepath= %s.\n", config->log_filepath);
        exit(EXIT_FAILURE);
    }

    clog_info(CLOG(CLOGGER_ID), "Hello, %s!", "world");

    clog_free(CLOGGER_ID);
    return 0;
}
