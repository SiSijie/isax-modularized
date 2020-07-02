//
// Created by Qitong Wang on 2020/6/28.
//

#include "config.h"


const struct option longopts[] = {
        {"database_filepath",               required_argument, NULL, 1},
        {"database_summarization_filepath", required_argument, NULL, 2},
        {"query_filepath",                  required_argument, NULL, 3},
        {"query_summarization_filepath",    required_argument, NULL, 4},
        {"database_size",                   required_argument, 0,    5},
        {"query_size",                      required_argument, 0,    6},
        {"sax_length",                    required_argument, 0,    7},
        {"sax_cardinality",                 required_argument, 0,    8},
        {"cpu_cores",                       required_argument, 0,    9},
        {"log_filepath",                    required_argument, 0,    10},
        {"series_length",                   required_argument, 0,    11},
        {"adhoc_breakpoints",               no_argument,       0,    12},
        {"numa_cores",                      required_argument, 0,    13},
        {"index_block_size",                required_argument, 0,    14},
        {"leaf_size",                       required_argument, 0,    15},
        {"initial_leaf_size",               required_argument, 0,    16},
        {NULL,                              no_argument,       NULL, 0}
};


int initializeThreads(Config *config, int cpu_cores, int numa_cores) {
    config->max_threads = cpu_cores;

    cpu_set_t mask, get;

    CPU_ZERO(&mask);
    CPU_ZERO(&get);

    // system-dependent
    int step = 3 - numa_cores;
    for (int i = 0; i < cpu_cores; ++i) {
        CPU_SET(i * step, &mask);
    }

    if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &mask) != 0) {
        fprintf(stderr, "set thread affinity failed\n");
    }

    if (pthread_getaffinity_np(pthread_self(), sizeof(cpu_set_t), &get) != 0) {
        fprintf(stderr, "get thread affinity failed\n");
    }

    return 0;
}


Config *initializeConfig(int argc, char **argv) {
    Config *config = malloc(sizeof(Config));

    config->database_filepath = NULL;
    config->database_summarization_filepath = NULL;
    config->query_filepath = NULL;
    config->query_summarization_filepath = NULL;
    config->log_filepath = "./isax.log";

    config->series_length = 256;

    config->database_size = 0;
    config->query_size = 0;

    config->initial_leaf_size = 1024;
    config->leaf_size = 8000;

    config->index_block_size = 20000;

    config->sax_cardinality = 8;
    config->sax_length = 16;

    config->use_adhoc_breakpoints = false;

    int cpu_cores = 1, numa_cores = 1;

    char *string_parts;
    int opt, longindex = 0;
    while ((opt = getopt_long(argc, argv, "", longopts, &longindex)) != -1) {
        switch (opt) {
            case 1:
                config->database_filepath = optarg;
                break;
            case 2:
                config->database_summarization_filepath = optarg;
                break;
            case 3:
                config->query_filepath = optarg;
                break;
            case 4:
                config->query_summarization_filepath = optarg;
                break;
            case 5:
                config->database_size = (size_t) strtoull(optarg, &string_parts, 10);
                break;
            case 6:
                config->query_size = (size_t) strtoull(optarg, &string_parts, 10);
                break;
            case 7:
                config->sax_length = (size_t) strtoull(optarg, &string_parts, 10);
                break;
            case 8:
                config->sax_cardinality = (unsigned int) strtol(optarg, &string_parts, 10);
                break;
            case 9:
                cpu_cores = (int) strtol(optarg, &string_parts, 10);
                break;
            case 10:
                config->log_filepath = optarg;
                break;
            case 11:
                config->series_length = (size_t) strtol(optarg, &string_parts, 10);
                break;
            case 12:
                config->use_adhoc_breakpoints = true;
                break;
            case 13:
                numa_cores = (int) strtol(optarg, &string_parts, 10);
                break;
            case 14:
                config->index_block_size = (size_t) strtoull(optarg, &string_parts, 10);
                break;
            case 15:
                config->leaf_size = (size_t) strtoull(optarg, &string_parts, 10);
                break;
            case 16:
                config->initial_leaf_size = (size_t) strtoull(optarg, &string_parts, 10);
                break;
            default:
                exit(EXIT_FAILURE);
        }
    }

    assert(config->series_length % config->sax_length == 0);
    assert(config->sax_length >= 0 && config->sax_length <= 16);
    assert(config->sax_cardinality == 8);
    assert(config->database_size >= 0);
    assert(config->query_size >= 0);
    assert(config->index_block_size >= 0);
    assert(config->series_length >= 0);
    assert(config->leaf_size > 0);
    assert(config->initial_leaf_size > 0 && config->initial_leaf_size <= config->leaf_size);

    assert(cpu_cores > 0 && numa_cores > 0 &&
           (numa_cores == 2 && cpu_cores <= 32) && (numa_cores == 1 && cpu_cores <= 16));

    initializeThreads(config, cpu_cores, numa_cores);
    return config;
}