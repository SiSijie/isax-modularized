//
// Created by Qitong Wang on 2020/6/28.
//

#include "index.h"


SAXWord *rootPosition2SAX(size_t position, size_t num_segments, unsigned int cardinality) {
    SAXWord *saxs = malloc(sizeof(SAXWord) * num_segments);

    for (size_t i = num_segments - 1; i >= 0; --i) {
        saxs[i] = (SAXWord) ((position % 2) << (cardinality - 1));
        position >>= 1u;
    }

    return saxs;
}


size_t rootSAX2Position(SAXWord const *saxs, size_t num_segments, unsigned int cardinality) {
    size_t position = 0;

    for (size_t i = num_segments - 1; i >= 0; --i) {
        position <<= 1u;
        position += (saxs[i] >> (cardinality - 1));
    }

    return position;
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
        index->roots[i] = initializeNode(rootPosition2SAX(i, config->sax_length, config->sax_cardinality),
                                         root_masks);
    }

    Value *values = malloc(sizeof(Value) * config->series_length * config->database_size);
    FILE *file_values = fopen(config->database_filepath, "rb");
    fread(values, sizeof(Value), sizeof(Value) * config->series_length * config->database_size, file_values);
    fclose(file_values);

    index->values = (Value const *) values;

    if (config->database_summarization_filepath != NULL) {
        Value *summarizations = malloc(sizeof(Value) * config->sax_length * config->database_size);
        FILE *file_summarizations = fopen(config->database_summarization_filepath, "rb");
        fread(summarizations, sizeof(Value), sizeof(Value) * config->sax_length * config->database_size,
              file_summarizations);
        fclose(file_summarizations);

        index->summarizations = (Value const *) index->summarizations;
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

    index->saxs = summarizations2SAXs(index->summarizations, index->breakpoints, index->database_size,
                                      index->sax_length, index->sax_cardinality, config->max_threads);

    return index;
}


void truncateNode(Node *node) {
    if (node->size != 0) {
        node->positions = realloc(node->positions, node->size);
        node->capacity = node->size;
    }

    if (node->left != NULL) {
        truncateNode(node->left);
        truncateNode(node->right);
    }
}


void finalizeIndex(Index *index) {
    free((Value *) index->summarizations);

    for (unsigned int i = 0; i < index->roots_size; ++i) {
        truncateNode(index->roots[i]);
    }
}


void freeNode(Node *node, bool free_mask, bool free_sax) {
    if (node->left != NULL) {
        freeNode(node->left, false, false);
        freeNode(node->right, true, true);
    }

    if (free_mask) {
        free(node->masks);
    }

    if (free_sax) {
        free(node->sax);
    }

    free(node->positions);
    free(node->lock);
    free(node);
}


void freeIndex(Config const *config, Index *index) {
    free((Value *) index->values);
    free(index->saxs);

    free((Value *) index->breakpoints[0]);
    if (config->use_adhoc_breakpoints) {
        for (int i = 1; i < config->sax_length; ++i) {
            free((Value *) index->breakpoints[i]);
        }
    }
    free((Value **) index->breakpoints);

    freeNode(index->roots[0], true, true);
    for (size_t i = 1; i < index->roots_size; ++i) {
        freeNode(index->roots[i], false, true);
    }
    free(index->roots);
}


void inspectNode(Node *node, size_t *num_series, size_t *num_leaves, size_t *num_roots) {
    if (node->size != 0) {
        if (num_roots != NULL) {
            *num_roots += 1;
        }

        *num_leaves += 1;
        *num_series += node->size;
    } else if (node->size == 0 && node->left != NULL) {
        if (num_roots != NULL) {
            *num_roots += 1;
        }

        inspectNode(node->left, num_series, num_leaves, NULL);
        inspectNode(node->right, num_series, num_leaves, NULL);
    }
}


void logIndex(Index *index) {
    clog_debug(CLOG(CLOGGER_ID), "%d reduced to %d (%d)", index->series_length, index->sax_length,
               index->sax_cardinality);

    size_t num_series = 0, num_leaves = 0, num_roots = 0;
    for (unsigned int i = 0; i < index->roots_size; ++i) {
        inspectNode(index->roots[i], &num_series, &num_leaves, &num_roots);
    }

    clog_debug(CLOG(CLOGGER_ID), "%d / %d roots", num_roots, index->roots_size);
    clog_debug(CLOG(CLOGGER_ID), "%d / %d series in %d leaves", num_series, index->database_size, num_leaves);
}
