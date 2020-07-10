//
// Created by Qitong Wang on 2020/7/3.
//

#include "query.h"


QuerySet *initializeQuery(Config const *config, Index const *index) {
    QuerySet *queries = malloc(sizeof(QuerySet));

    queries->query_size = config->query_size;

#ifdef FINE_TIMING
    clock_t start_clock = clock();
#endif

    Value *values = malloc(sizeof(Value) * config->series_length * config->query_size);
    FILE *file_values = fopen(config->query_filepath, "rb");
    unsigned int read_values = fread(values, sizeof(Value), config->series_length * config->query_size, file_values);
    fclose(file_values);
    assert(read_values == config->series_length * config->query_size);

    queries->values = (Value const *) values;

#ifdef FINE_TIMING
    clog_info(CLOG(CLOGGER_ID), "query - load series = %lums", (clock() - start_clock) * 1000 / CLOCKS_PER_SEC);

    start_clock = clock();
#endif

    if (config->query_summarization_filepath != NULL) {
        Value *summarizations = malloc(sizeof(Value) * config->sax_length * config->query_size);
        FILE *file_summarizations = fopen(config->query_summarization_filepath, "rb");
        read_values = fread(summarizations, sizeof(Value), config->sax_length * config->query_size, file_summarizations);
        fclose(file_summarizations);
        assert(read_values == config->sax_length * config->query_size);

        queries->summarizations = (Value const *) summarizations;
    } else {
        queries->summarizations = (Value const *) piecewiseAggregate(queries->values, config->query_size,
                                                                     config->series_length, config->sax_length,
                                                                     config->max_threads);
    }

#ifdef FINE_TIMING
    char *method4summarizations = "load";
    if (config->database_summarization_filepath == NULL) {
        method4summarizations = "calculate";
    }
    clog_info(CLOG(CLOGGER_ID), "query - %s summarizations = %lums", method4summarizations,
              (clock() - start_clock) * 1000 / CLOCKS_PER_SEC);

    start_clock = clock();
#endif

    queries->saxs = (SAXWord const *) summarizations2SAXs(queries->summarizations, index->breakpoints,
                                                          queries->query_size, config->sax_length,
                                                          config->sax_cardinality, config->max_threads);

#ifdef FINE_TIMING
    clog_info(CLOG(CLOGGER_ID), "query - calculate SAXs = %lums", (clock() - start_clock) * 1000 / CLOCKS_PER_SEC);
#endif

    return queries;
}


void freeQuery(QuerySet *queries) {
    free((Value *) queries->values);
    free((Value *) queries->saxs);
    free(queries);
}
