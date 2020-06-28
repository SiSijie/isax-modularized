//
// Created by Qitong Wang on 2020/6/28.
//

#ifndef ISAX_CONFIG_H
#define ISAX_CONFIG_H

typedef struct {
    char * database_filepath;
    char * database_summarization_filepath;
    char * query_filepath;
    char * query_summarization_filepath;
    char * log_filepath;

    int database_size;
    int query_size;

    int sax_segments;
    int sax_cardinality;
} Config;


Config * initialize(int argc, char **argv);

#endif //ISA_CONFIG_H
