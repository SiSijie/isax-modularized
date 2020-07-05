//
// Created by Qitong Wang on 2020/6/28.
//

#ifndef ISAX_GLOBALS_H
#define ISAX_GLOBALS_H

#include <float.h>


#ifdef FINE_TIMING
#ifndef TIMING
#define TIMING
#endif
#endif


#define CLOGGER_ID 0


typedef float Value;
// TODO only supports sax_cardinality <= 8
typedef unsigned char SAXWord;
typedef unsigned char SAXMask;


#define VALUE_L(left, right) (right - left > FLT_EPSILON)
#define VALUE_G(left, right) (left - right > FLT_EPSILON)
#define VALUE_LEQ(left, right) (!VALUE_G(left, right))
#define VALUE_GEQ(left, right) (!(VALUE_L(left, right)))
#define VALUE_EQ(left, right) ((left - right <= FLT_EPSILON) && ((right - left <= FLT_EPSILON)))


static inline int VALUE_COMPARE(void const *left, void const *right) {
    if (VALUE_L(*(Value *) left, *(Value *) right)) {
        return -1;
    }

    if (VALUE_G(*(Value *) left, *(Value *) right)) {
        return 1;
    }

    return 0;
}

#endif //ISAX_GLOBALS_H
