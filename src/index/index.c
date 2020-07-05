//
// Created by Qitong Wang on 2020/6/28.
//

#include "index.h"


SAXWord *rootID2SAX(size_t id, size_t num_segments, unsigned int cardinality) {
    SAXWord *sax = malloc(sizeof(SAXWord) * num_segments);

    for (size_t i = 0; i < num_segments; ++i) {
        sax[num_segments - 1 - i] = (SAXWord) ((id % 2) << (cardinality - 1));
        id >>= 1u;
    }

    return sax;
}


size_t rootSAX2ID(SAXWord const *saxs, size_t num_segments, unsigned int cardinality) {
    size_t id = 0;

    for (size_t i = 0; i < num_segments; ++i) {
        id <<= 1u;
        id += (size_t) (saxs[i] >> (cardinality - 1));
    }

    return id;
}


Index *initializeIndex(Config const *config) {
    Index *index = malloc(sizeof(Index));
    if (index == NULL) {
        clog_error(CLOG(CLOGGER_ID), "could not allocate memory to initialize index");
        exit(EXIT_FAILURE);
    }

    index->series_length = config->series_length;
    index->sax_length = config->sax_length;
    index->sax_cardinality = config->sax_cardinality;
    index->database_size = config->database_size;

    // initialize first-layer (root) nodes of (1 bit * sax_length) SAX
    index->roots_size = 1u << (unsigned int) config->sax_length;
    index->roots = malloc(sizeof(Node *) * index->roots_size);
    SAXMask *root_masks = malloc(sizeof(SAXMask) * config->sax_length);
    for (size_t i = 0; i < config->sax_length; ++i) {
        root_masks[i] = (SAXMask) (1u << (config->sax_cardinality - 1));
    }
    for (size_t i = 0; i < index->roots_size; ++i) {
        index->roots[i] = initializeNode(rootID2SAX(i, config->sax_length, config->sax_cardinality),
                                         root_masks);
    }

    Value *values = malloc(sizeof(Value) * config->series_length * config->database_size);
    FILE *file_values = fopen(config->database_filepath, "rb");
    fread(values, sizeof(Value), config->series_length * config->database_size, file_values);
    fclose(file_values);

    index->values = (Value const *) values;

    if (config->database_summarization_filepath != NULL) {
        Value *summarizations = malloc(sizeof(Value) * config->sax_length * config->database_size);
        FILE *file_summarizations = fopen(config->database_summarization_filepath, "rb");
        fread(summarizations, sizeof(Value), config->sax_length * config->database_size, file_summarizations);
        fclose(file_summarizations);

        index->summarizations = (Value const *) summarizations;
    } else {
        index->summarizations = (Value const *) piecewiseAggregate(index->values, config->database_size,
                                                                   config->series_length, config->sax_length,
                                                                   config->max_threads);
    }

    if (config->use_adhoc_breakpoints) {
        index->breakpoints = (Value const *const *) adhocBreakpoints(index->summarizations, config->database_size,
                                                                     config->sax_length, config->sax_cardinality,
                                                                     config->max_threads);
    } else {
        Value **breakpoints = malloc(sizeof(Value *) * config->sax_length);
        Value *normal_breakpoints = getNormalBreakpoints8();

        for (int i = 0; i < config->sax_length; ++i) {
            breakpoints[i] = normal_breakpoints;
        }

        index->breakpoints = (Value const *const *) breakpoints;
    }

    index->saxs = (SAXWord const *) summarizations2SAXs(index->summarizations, index->breakpoints, index->database_size,
                                                        index->sax_length, index->sax_cardinality, config->max_threads);

    return index;
}


void truncateNode(Node *node) {
    if (node != NULL) {
        if (node->size != 0) {
            node->ids = realloc(node->ids, sizeof(size_t) * node->size);
            node->capacity = node->size;
        }

        if (node->left != NULL) {
            truncateNode(node->left);
            truncateNode(node->right);
        }
    }
}


void finalizeIndex(Index *index) {
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
}


void freeIndex(Config const *config, Index *index) {
    free((Value *) index->values);
    free((SAXWord *) index->saxs);

    free((Value *) index->breakpoints[0]);
    if (config->use_adhoc_breakpoints) {
        for (int i = 1; i < config->sax_length; ++i) {
            free((Value *) index->breakpoints[i]);
        }
    }
    free((Value **) index->breakpoints);

    bool first_root = true;
    for (size_t i = 0; i < index->roots_size; ++i) {
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
    clog_debug(CLOG(CLOGGER_ID), "series %d --> SAX %d (cardinality %d)", index->series_length, index->sax_length,
               index->sax_cardinality);

    size_t num_series = 0, num_leaves = 0, num_roots = 0;
    for (unsigned int i = 0; i < index->roots_size; ++i) {
        inspectNode(index->roots[i], &num_series, &num_leaves, &num_roots);
    }

    clog_debug(CLOG(CLOGGER_ID), "%d / %d roots", num_roots, index->roots_size);
    clog_debug(CLOG(CLOGGER_ID), "%d / %d series in %d leaves", num_series, index->database_size, num_leaves);
}
