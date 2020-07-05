//
// Created by Qitong Wang on 2020/7/2.
//

#ifndef ISAX_SAX_H
#define ISAX_SAX_H

#include <stdlib.h>
#include <pthread.h>

#include "globals.h"
#include "breakpoints.h"


SAXWord *
summarizations2SAXs(Value const *summarizations, Value const *const *breakpoints, size_t size, size_t sax_length,
                    unsigned int sax_cardinality, int num_threads);

// TODO evaluate whether early-stopping is necessary for SAX distance calculation
// TODO evaluate whether SIMD is necessary for SAX distance calculation
Value l2SquareSummarization2SAXByMask(size_t sax_length, Value const *summarizations, SAXWord const *sax,
                                      SAXMask const *masks, Value const *const *breakpoints, Value scale_factor);

Value l2SquareSummarization2SAX8(size_t sax_length, Value const *summarizations, SAXWord const *sax,
                                 Value const *const *breakpoints, Value scale_factor);

#endif //ISAX_SAX_H
