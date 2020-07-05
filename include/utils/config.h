//
// Created by Qitong Wang on 2020/6/28.
//

#ifndef ISAX_CONFIG_H
#define ISAX_CONFIG_H

// TODO CPU_ZERO and others are specified for Linux, except Mac
#define _GNU_SOURCE

#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>
#include <assert.h>

#include "globals.h"
#include "clog.h"


typedef struct Config {
    char *database_filepath;
    char *database_summarization_filepath;
    char *query_filepath;
    char *query_summarization_filepath;
    char *log_filepath;

    size_t database_size;
    size_t query_size;

    size_t series_length;
    size_t sax_length;
    unsigned int sax_cardinality;

    size_t initial_leaf_size;
    size_t leaf_size;

    bool use_adhoc_breakpoints;
    bool exact_search;
    bool sort_leaves;

    unsigned int k; // kNN

    int cpu_cores;
    int numa_cores;
    int max_threads;

    size_t index_block_size;
} Config;


Config *initializeConfig(int argc, char **argv);

void logConfig(Config const *config);

#endif //ISAX_CONFIG_H
