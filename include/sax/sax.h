//
// Created by Qitong Wang on 2020/7/2.
//

#ifndef ISAX_SAX_H
#define ISAX_SAX_H

#include <stdlib.h>
#include <pthread.h>
#include <immintrin.h>

#include "globals.h"
#include "distance.h"
#include "breakpoints.h"


SAXWord *
summarizations2SAXs(Value const *summarizations, Value const *breakpoints, unsigned int size, unsigned int sax_length,
                    unsigned int sax_cardinality, unsigned int num_threads);

Value l2SquareValue2SAXByMask(unsigned int sax_length, Value const *summarizations, SAXWord const *sax,
                              SAXMask const *masks, Value const *breakpoints, Value scale_factor);

Value l2SquareValue2SAX8(unsigned int sax_length, Value const *summarizations, SAXWord const *sax,
                         Value const *breakpoints, Value scale_factor);

Value l2SquareValue2SAXByMaskSIMD(unsigned int sax_length, Value const *summarizations, SAXWord const *sax,
                                  SAXMask const *masks, Value const *breakpoints, Value scale_factor, Value *cache);

Value l2SquareValue2SAX8SIMD(unsigned int sax_length, Value const *summarizations, SAXWord const *sax,
                             Value const *breakpoints, Value scale_factor, Value *cache);

#endif //ISAX_SAX_H
