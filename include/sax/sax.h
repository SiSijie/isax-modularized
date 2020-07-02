//
// Created by Qitong Wang on 2020/7/2.
//

#ifndef ISAX_SAX_H
#define ISAX_SAX_H

#include <stdlib.h>
#include <pthread.h>

#include "globals.h"
#include <breakpoints.h>


SAXWord *
summarizations2SAXs(Value const *summarizations, Value const *const *breakpoints, size_t size, size_t sax_length,
                    unsigned int sax_cardinality, int num_threads);

#endif //ISAX_SAX_H
