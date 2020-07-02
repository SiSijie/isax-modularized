//
// Created by Qitong Wang on 2020/6/28.
//

#ifndef ISAX_GLOBALS_H
#define ISAX_GLOBALS_H

#define CLOGGER_ID 0

// TODO only supports sax_cardinality <= 8
typedef unsigned char SAXWord;
typedef unsigned char SAXMask;

typedef float Value;

#define EPSILON 1e-7

#define VALUE_EQUAL(left, right) ((left - right < EPSILON) && ((right - left < EPSILON)))
#define VALUE_LESS(left, right) (left - right < EPSILON)
#define VALUE_MORE(left, right) VALUE_LESS(right, left)


static inline int VALUE_COMPARE(void const *left, void const *right) {
    if (VALUE_LESS(*(Value *) left, *(Value *) right)) {
        return -1;
    } else if (VALUE_LESS(*(Value *) right, *(Value *) left)) {
        return 1;
    } else {
        return 0;
    }
}

#endif //ISAX_GLOBALS_H
