//
// Created by Qitong Wang on 2020/6/28.
//

#include "index.h"


SAXWord *rootID2SAX(unsigned int id, unsigned int num_segments, unsigned int cardinality) {
    SAXWord *sax = malloc(sizeof(SAXWord) * num_segments);

    for (unsigned int i = 0; i < num_segments; ++i) {
        sax[num_segments - 1 - i] = (SAXWord) ((id % 2) << (cardinality - 1));
        id >>= 1u;
    }

    return sax;
}


unsigned int rootSAX2ID(SAXWord const *saxs, unsigned int num_segments, unsigned int cardinality) {
    unsigned int id = 0;

    for (unsigned int i = 0; i < num_segments; ++i) {
        id <<= 1u;
        id += (unsigned int) (saxs[i] >> (cardinality - 1));
    }

    return id;
}


Index *initializeIndex(Config const *config) {
    // TODO figure out why M256I_1 cannot use as global constant
    initializeM256IConstants();

    Index *index = malloc(sizeof(Index));
    if (index == NULL) {
        clog_error(CLOG(CLOGGER_ID), "could not allocate memory to initialize index");
        exit(EXIT_FAILURE);
    }

    index->series_length = config->series_length;
    index->sax_length = config->sax_length;
    index->sax_cardinality = config->sax_cardinality;
    index->database_size = config->database_size;
    index->num_leaves = 0;

#ifdef FINE_TIMING
    clock_t start_clock = clock();
#endif

    // initialize first-layer (root) nodes of (1 bit * sax_length) SAX
    index->roots_size = 1u << (unsigned int) config->sax_length;
    index->roots = malloc(sizeof(Node *) * index->roots_size);
    SAXMask *root_masks = malloc(sizeof(SAXMask) * config->sax_length);
    for (unsigned int i = 0; i < config->sax_length; ++i) {
        root_masks[i] = (SAXMask) (1u << (config->sax_cardinality - 1));
    }
    for (unsigned int i = 0; i < index->roots_size; ++i) {
        index->roots[i] = initializeNode(rootID2SAX(i, config->sax_length, config->sax_cardinality),
                                         root_masks);
    }

#ifdef FINE_TIMING
    clog_info(CLOG(CLOGGER_ID), "index - initialize roots = %lums", (clock() - start_clock) * 1000 / CLOCKS_PER_SEC);

    start_clock = clock();
#endif

    Value *values = malloc(sizeof(Value) * config->series_length * config->database_size);
    FILE *file_values = fopen(config->database_filepath, "rb");
    unsigned int read_values = fread(values, sizeof(Value), config->series_length * config->database_size, file_values);
    fclose(file_values);
    assert(read_values == config->series_length * config->database_size);

    index->values = (Value const *) values;

#ifdef FINE_TIMING
    clog_info(CLOG(CLOGGER_ID), "index - load series = %lums", (clock() - start_clock) * 1000 / CLOCKS_PER_SEC);

    start_clock = clock();
#endif

    if (config->database_summarization_filepath != NULL) {
        Value *summarizations = malloc(sizeof(Value) * config->sax_length * config->database_size);
        FILE *file_summarizations = fopen(config->database_summarization_filepath, "rb");
        read_values = fread(summarizations, sizeof(Value), config->sax_length * config->database_size,
                            file_summarizations);
        fclose(file_summarizations);
        assert(read_values == config->sax_length * config->database_size);

        index->summarizations = (Value const *) summarizations;
    } else {
        index->summarizations = (Value const *) piecewiseAggregate(index->values, config->database_size,
                                                                   config->series_length, config->sax_length,
                                                                   config->max_threads);
    }

#ifdef FINE_TIMING
    char *method4summarizations = "load";
    if (config->database_summarization_filepath == NULL) {
        method4summarizations = "calculate";
    }
    clog_info(CLOG(CLOGGER_ID), "index - %s summarizations = %lums", method4summarizations,
              (clock() - start_clock) * 1000 / CLOCKS_PER_SEC);

    start_clock = clock();
#endif

    if (config->use_adhoc_breakpoints) {
        index->breakpoints = getAdhocBreakpoints8(index->summarizations, config->database_size, config->sax_length,
                                                  config->max_threads);
    } else {
        index->breakpoints = getNormalBreakpoints8(config->sax_length);
    }

#ifdef FINE_TIMING
    char *method4breakpoints = "normal";
    if (config->use_adhoc_breakpoints) {
        method4breakpoints = "adhoc";
    }
    clog_info(CLOG(CLOGGER_ID), "index - load %s breakpoints = %lums", method4breakpoints,
              (clock() - start_clock) * 1000 / CLOCKS_PER_SEC);

    start_clock = clock();
#endif

    index->saxs = (SAXWord const *) summarizations2SAXs(index->summarizations, index->breakpoints, index->database_size,
                                                        index->sax_length, index->sax_cardinality, config->max_threads);

#ifdef FINE_TIMING
    clog_info(CLOG(CLOGGER_ID), "index - calculate SAXs = %lums", (clock() - start_clock) * 1000 / CLOCKS_PER_SEC);
#endif

    return index;
}


void truncateNode(Node *node) {
    if (node != NULL) {
        if (node->size != 0) {
            node->ids = realloc(node->ids, sizeof(unsigned int) * node->size);
            node->capacity = node->size;
        }

        if (node->left != NULL) {
            truncateNode(node->left);
            truncateNode(node->right);
        }
    }
}


void finalizeIndex(Index *index) {
#ifdef FINE_TIMING
    clock_t start_clock = clock();
#endif

    free((Value *) index->summarizations);

    for (unsigned int i = 0; i < index->roots_size; ++i) {
        if (index->roots[i]->size == 0 && index->roots[i]->left == NULL) {
            freeNode(index->roots[i], false, true);;
            index->roots[i] = NULL;
        }
    }

    for (unsigned int i = 0; i < index->roots_size; ++i) {
        truncateNode(index->roots[i]);
    }

#ifdef FINE_TIMING
    clog_info(CLOG(CLOGGER_ID), "index - finalize = %lums", (clock() - start_clock) * 1000 / CLOCKS_PER_SEC);
#endif
}


void freeIndex(Config const *config, Index *index) {
    free((Value *) index->values);
    free((SAXWord *) index->saxs);
    free((Value *) index->breakpoints);

    bool first_root = true;
    for (unsigned int i = 0; i < index->roots_size; ++i) {
        if (index->roots[i] != NULL) {
            if (first_root) {
                freeNode(index->roots[i], true, true);
                first_root = false;
            } else {
                freeNode(index->roots[i], false, true);
            }
        }
    }

    free(index->roots);
}


void logIndex(Index *index) {
    unsigned int num_series = 0, num_roots = 0;
    for (unsigned int i = 0; i < index->roots_size; ++i) {
        inspectNode(index->roots[i], &num_series, &index->num_leaves, &num_roots);
    }

    clog_info(CLOG(CLOGGER_ID), "index - %d series in %d leaves from %d / %d roots", num_series, index->num_leaves,
              num_roots, index->roots_size);
}
