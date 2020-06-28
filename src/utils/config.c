//
// Created by Qitong Wang on 2020/6/28.
//

#define _GNU_SOURCE
#include <sched.h>

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <pthread.h>

#include "config.h"


const struct option longopts[] = {
        {"database_filepath",    required_argument, NULL, 1},
        {"database_summarization_filepath",    required_argument, NULL, 2},
        {"query_filepath",    required_argument, NULL, 3},
        {"query_summarization_filepath",    required_argument, NULL, 4},
        {"database_size",    required_argument, 0, 5},
        {"query_size",    required_argument, 0, 6},
        {"sax_segments",    required_argument, 0, 7},
        {"sax_cardinality",    required_argument, 0, 8},
        {"cpu_control_code",    required_argument, 0, 9},
        {"log_filepath",    required_argument, 0, 10},
//        {"use-index",           no_argument,       0, 'a'},
//        {"initial-lbl-size",    required_argument, 0, 'b'},
        { NULL, no_argument, NULL, 0 }
};

void initialize_cpu(Config *config, int cpu_control_code) {
    cpu_set_t mask, get;
    CPU_ZERO(&mask);
    CPU_ZERO(&get);

    if (cpu_control_code == 21) {
        CPU_SET(0, &mask);
        CPU_SET(2, &mask);
//        calculate_thread = 2;
//        maxquerythread = 2;
    } else if (cpu_control_code == 22) {
        CPU_SET(0, &mask);
        CPU_SET(1, &mask);
//        calculate_thread = 2;
//        maxquerythread = 2;
    } else if (cpu_control_code == 41) {
        CPU_SET(0, &mask);
        CPU_SET(2, &mask);
        CPU_SET(4, &mask);
        CPU_SET(6, &mask);
//        calculate_thread = 4;
//        maxquerythread = 4;
    } else if (cpu_control_code == 42) {
        CPU_SET(0, &mask);
        CPU_SET(1, &mask);
        CPU_SET(2, &mask);
        CPU_SET(3, &mask);
//        calculate_thread = 4;
//        maxquerythread = 4;
    } else if (cpu_control_code == 81) {
        CPU_SET(0, &mask);
        CPU_SET(2, &mask);
        CPU_SET(4, &mask);
        CPU_SET(6, &mask);
        CPU_SET(8, &mask);
        CPU_SET(10, &mask);
        CPU_SET(12, &mask);
        CPU_SET(14, &mask);
//        calculate_thread = 8;
//        maxquerythread = 8;
    } else if (cpu_control_code == 82) {
        CPU_SET(0, &mask);
        CPU_SET(1, &mask);
        CPU_SET(2, &mask);
        CPU_SET(3, &mask);
        CPU_SET(4, &mask);
        CPU_SET(5, &mask);
        CPU_SET(6, &mask);
        CPU_SET(7, &mask);
//        calculate_thread = 8;
//        maxquerythread = 8;
    } else if (cpu_control_code == 1) {
        // calculate_thread = 1;
        // maxquerythread = 1;
    } else {
        // calculate_thread = cpu_control_code;
        // maxquerythread = cpu_control_code;
    }

    if (pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask) < 0) {
        fprintf(stderr, "set thread affinity failed\n");
    }

    if (pthread_getaffinity_np(pthread_self(), sizeof(get), &get) < 0) {
        fprintf(stderr, "get thread affinity failed\n");
    }
}


Config *initialize(int argc, char **argv) {
    Config *config = malloc(sizeof(Config));
    int cpu_control_code = 81;

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
                config->database_size = atoi(optarg);
                break;
            case 6:
                config->query_size = atoi(optarg);
                break;
            case 7:
                config->sax_segments = atoi(optarg);
                break;
            case 8:
                config->sax_cardinality = atoi(optarg);
                break;
            case 9:
                cpu_control_code = atoi(optarg);
                break;
            case 10:
                config->log_filepath = optarg;
                break;
            default:
                exit(EXIT_FAILURE);
        }
    }

    initialize_cpu(config, cpu_control_code);
    return config;
}