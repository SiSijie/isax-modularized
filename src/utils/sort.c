//
// Created by Qitong Wang on 2020/7/16.
//

#include "sort.h"


unsigned int bSearchFloor(Value value, Value const *sorted, unsigned int first_inclusive, unsigned int last_inclusive) {
    while (first_inclusive + 1 < last_inclusive) {
        unsigned int mid = (first_inclusive + last_inclusive) >> 1u;

        if (VALUE_L(value, sorted[mid])) {
            last_inclusive = mid;
        } else {
            first_inclusive = mid;
        }
    }

    return first_inclusive;
}


unsigned int bSearchByIndicesFloor(Value value, unsigned int const *indices, Value const *values,
                                   unsigned int first_inclusive, unsigned int last_inclusive) {
    while (first_inclusive + 1 < last_inclusive) {
        unsigned int mid = (first_inclusive + last_inclusive) >> 1u;

        if (VALUE_L(value, values[indices[mid]])) {
            last_inclusive = mid;
        } else {
            first_inclusive = mid;
        }
    }

    return first_inclusive;
}


// following C.A.R. Hoare as in https://en.wikipedia.org/wiki/Quicksort#Hoare_partition_scheme
void qSortIndicesBy(unsigned int *indices, Value *orders, int first_inclusive, int last_inclusive) {
    if (first_inclusive < last_inclusive) {
        unsigned int tmp_index;
        Value pivot = orders[indices[(unsigned int) (first_inclusive + last_inclusive) >> 1u]];

        if (first_inclusive + 3 < last_inclusive) {
            Value smaller = orders[indices[first_inclusive]], larger = orders[indices[last_inclusive]];
            if (VALUE_L(larger, smaller)) {
                smaller = orders[indices[last_inclusive]], larger = orders[indices[first_inclusive]];
            }

            if (VALUE_L(pivot, smaller)) {
                pivot = smaller;
            } else if (VALUE_G(pivot, larger)) {
                pivot = larger;
            }
        }

        int first_g = first_inclusive - 1, last_l = last_inclusive + 1;
        while (true) {
            do {
                first_g += 1;
            } while (VALUE_L(orders[indices[first_g]], pivot));

            do {
                last_l -= 1;
            } while (VALUE_G(orders[indices[last_l]], pivot));

            if (first_g >= last_l) {
                break;
            }

            tmp_index = indices[first_g];
            indices[first_g] = indices[last_l];
            indices[last_l] = tmp_index;
        }

        qSortIndicesBy(indices, orders, first_inclusive, last_l);
        qSortIndicesBy(indices, orders, last_l + 1, last_inclusive);
    }
}


void
qSortFirstHalfIndicesBy(unsigned int *indices, Value *orders, int first_inclusive, int last_inclusive, Value pivot) {
    if (first_inclusive < last_inclusive) {
        unsigned int tmp_index;

        int first_g = first_inclusive - 1, last_l = last_inclusive + 1;

        while (true) {
            do {
                first_g += 1;
            } while (first_g <= last_inclusive && VALUE_L(orders[indices[first_g]], pivot));

            do {
                last_l -= 1;
            } while (last_l >= first_inclusive && VALUE_G(orders[indices[last_l]], pivot));

            if (first_g >= last_l) {
                break;
            }

            tmp_index = indices[first_g];
            indices[first_g] = indices[last_l];
            indices[last_l] = tmp_index;
        }

        qSortIndicesBy(indices, orders, first_inclusive, last_l < last_inclusive ? last_l : last_inclusive);
    }
}