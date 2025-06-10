#include "simdsort-support.hpp"

#include <cmath>
#include <cstddef>
#include <algorithm>
#include <utility>

#include "sve-qsort-vector.hpp"
#include "classfile_constants.h"

/* Compiler specific macros specific */
#if defined(__GNUC__)
#define SVE_SORT_INLINE static inline
#define SVE_SORT_FINLINE static inline __attribute__((always_inline))
#else
#define SVE_SORT_INLINE static
#define SVE_SORT_FINLINE static
#endif

#define OET_SORT_THRESHOLD 8

#define DLL_PUBLIC __attribute__((visibility("default")))

template <typename vtype, typename T = typename vtype::type_t>
bool sve_comparison_func_ge(const T &a, const T &b) {
    return a < b;
}

template <typename vtype, typename T = typename vtype::type_t>
bool sve_comparison_func_gt(const T &a, const T &b) {
    return a <= b;
}

/*
 * Parition an array based on the pivot and returns the index of the
 * first element that is greater than or equal to the pivot.
 */
template <typename vtype, typename type_t>
SVE_SORT_INLINE arrsize_t sve_vect_partition_(type_t *arr, arrsize_t left,
                                                arrsize_t right, type_t pivot,
                                                type_t *smallest,
                                                type_t *biggest,
                                                bool use_gt) {
    auto comparison_func = use_gt ? sve_comparison_func_gt<vtype> : sve_comparison_func_ge<vtype>;

    /* make array length divisible by vtype::numlanes , shortening the array */
    for (int32_t i = (right - left) % vtype::numlanes; i > 0; --i) {

        *smallest = std::min(*smallest, arr[left], comparison_func);
        *biggest = std::max(*biggest, arr[left], comparison_func);

        if (!comparison_func(arr[left], pivot)) {
            std::swap(arr[left], arr[--right]);
        } else {
            ++left;
        }
    }

    if (left == right)
        return left; /* less than vtype::numlanes elements in the array */

    using reg_t = typename vtype::reg_t;
    reg_t pivot_vec = vtype::set1(pivot);
    reg_t min_vec = vtype::set1(*smallest);
    reg_t max_vec = vtype::set1(*biggest);

    if (right - left == vtype::numlanes) {
        reg_t vec = vtype::loadu(arr + left);
        arrsize_t unpartitioned = right - left - vtype::numlanes;
        arrsize_t l_store = left;

        arrsize_t amount_ge_pivot =
            vtype::partition_vec(arr + l_store, arr + l_store + unpartitioned,
                                 vec, pivot_vec, min_vec, max_vec, use_gt);

        l_store += (vtype::numlanes - amount_ge_pivot);
        *smallest = vtype::reducemin(min_vec);
        *biggest = vtype::reducemax(max_vec);

        return l_store;
    }

    // first and last vtype::numlanes values are partitioned at the end
    reg_t vec_left = vtype::loadu(arr + left);
    reg_t vec_right = vtype::loadu(arr + (right - vtype::numlanes));
    // store points of the vectors
    arrsize_t unpartitioned = right - left - vtype::numlanes;
    arrsize_t l_store = left;
    // indices for loading the elements
    left += vtype::numlanes;
    right -= vtype::numlanes;
    while (right - left != 0) {
        reg_t curr_vec;
        /*
         * if fewer elements are stored on the right side of the array,
         * then next elements are loaded from the right side,
         * otherwise from the left side
         */
        if ((l_store + unpartitioned + vtype::numlanes) - right <
            left - l_store) {
            right -= vtype::numlanes;
            curr_vec = vtype::loadu(arr + right);
        } else {
            curr_vec = vtype::loadu(arr + left);
            left += vtype::numlanes;
        }
        // partition the current vector and save it on both sides of the array
        arrsize_t amount_ge_pivot =
            vtype::partition_vec(arr + l_store, arr + l_store + unpartitioned,
                                 curr_vec, pivot_vec, min_vec, max_vec, use_gt);
        l_store += (vtype::numlanes - amount_ge_pivot);
        unpartitioned -= vtype::numlanes;
    }

    /* partition and save vec_left and vec_right */
    arrsize_t amount_ge_pivot =
        vtype::partition_vec(arr + l_store, arr + l_store + unpartitioned,
                             vec_left, pivot_vec, min_vec, max_vec, use_gt);
    l_store += (vtype::numlanes - amount_ge_pivot);
    unpartitioned -= vtype::numlanes;


    amount_ge_pivot =
        vtype::partition_vec(arr + l_store, arr + l_store + unpartitioned,
                             vec_right, pivot_vec, min_vec, max_vec, use_gt);
    l_store += (vtype::numlanes - amount_ge_pivot);
    unpartitioned -= vtype::numlanes;

    *smallest = vtype::reducemin(min_vec);
    *biggest = vtype::reducemax(max_vec);

    return l_store;
}

template <typename vtype, typename type_t>
SVE_SORT_INLINE type_t sve_get_pivot_blocks(type_t *arr, const arrsize_t left,
                                             const arrsize_t right) {
    if (right - left < 64)
        return arr[left];

    const uint32_t idx0 = (right - left) / 2;
    const uint32_t idx1 = left;
    const uint32_t idx2 = right - 1;

    if (arr[idx0] > arr[idx1]) {
        if (arr[idx1] > arr[idx2])
            return arr[idx1];
        else if (arr[idx0] > arr[idx2])
            return arr[idx2];
        else
            return arr[idx0];
    } else {
        if (arr[idx0] > arr[idx2])
            return arr[idx0];
        else if (arr[idx1] > arr[idx2])
            return arr[idx2];
        else
            // three equal keys will return idx1, i.e. 0
            // this will prevent the pivot from moving needlessly around
            return arr[idx1];
    }
}

template <typename vtype, typename type_t>
SVE_SORT_INLINE void sve_oet_sort(type_t *arr, arrsize_t from_index, arrsize_t to_index)
{
    arrsize_t arr_num = to_index - from_index;

    for (int32_t i = 0; i <= OET_SORT_THRESHOLD; i++)
    {
        int32_t j = from_index + i % 2; // j always begins as 0 or 1
        int32_t remaining = arr_num - (i%2);

        while (remaining >= 2)
        {
            const int32_t vals_per_iteration = (remaining < (2 * vtype::numlanes)) ? remaining : 2 * vtype::numlanes;
            const int32_t num = vals_per_iteration / 2;
            vtype::oet_sort(&arr[j], num);

            j += vals_per_iteration;
            remaining -= vals_per_iteration;
        }
    }
}

template <typename vtype, typename type_t>
SVE_SORT_INLINE void sve_qsort(type_t *arr, arrsize_t left, arrsize_t right,
                   arrsize_t max_iters) {

    if ((right - left) <= OET_SORT_THRESHOLD)
        return;

    if (max_iters <= 0) {
        std::sort(arr + left, arr + right, sve_comparison_func_ge<vtype>);
        return;
    }


    type_t pivot = sve_get_pivot_blocks<vtype, type_t>(arr, left, right);

    type_t smallest = vtype::type_max();
    type_t biggest = vtype::type_min();

    /*To Improve: Implement unroll partition*/
    arrsize_t pivot_index =
        sve_vect_partition_<vtype>(
            arr, left, right, pivot, &smallest, &biggest, false);

    if (pivot != smallest)
        sve_qsort<vtype>(arr, left, pivot_index, max_iters - 1);
    if (pivot != biggest)
        sve_qsort<vtype>(arr, pivot_index, right, max_iters - 1);
}

template <typename vtype, typename type_t>
SVE_SORT_INLINE int64_t sve_vect_partition(type_t *arr, int64_t from_index, int64_t to_index, type_t pivot, bool use_gt) {
    type_t smallest = vtype::type_max();
    type_t biggest = vtype::type_min();
    /*To Improve: Implement unroll partition*/
    int64_t pivot_index = sve_vect_partition_<vtype>(
            arr, from_index, to_index, pivot, &smallest, &biggest, use_gt);
    return pivot_index;
}

template <typename vtype, typename T>
SVE_SORT_INLINE void sve_dual_pivot_partition(T *arr, int64_t from_index, int64_t to_index, int32_t *pivot_indices, int64_t index_pivot1, int64_t index_pivot2){
    const T pivot1 = arr[index_pivot1];
    const T pivot2 = arr[index_pivot2];

    const int64_t low = from_index;
    const int64_t high = to_index;
    const int64_t start = low + 1;
    const int64_t end = high - 1;


    std::swap(arr[index_pivot1], arr[low]);
    std::swap(arr[index_pivot2], arr[end]);

    const int64_t pivot_index2 = sve_vect_partition<vtype, T>(arr, start, end, pivot2, true); // use_gt = true
    std::swap(arr[end], arr[pivot_index2]);
    int64_t upper = pivot_index2;

    // if all other elements are greater than pivot2 (and pivot1), no need to do further partitioning
    if (upper == start) {
        pivot_indices[0] = low;
        pivot_indices[1] = upper;
        return;
    }

    const int64_t pivot_index1 = sve_vect_partition<vtype, T>(arr, start, upper, pivot1, false); // use_ge (use_gt = false)
    int64_t lower = pivot_index1 - 1;
    std::swap(arr[low], arr[lower]);

    pivot_indices[0] = lower;
    pivot_indices[1] = upper;
}

template <typename vtype, typename T>
SVE_SORT_INLINE void sve_single_pivot_partition(T *arr, int64_t from_index, int64_t to_index, int32_t *pivot_indices, int64_t index_pivot) {
    const T pivot = arr[index_pivot];

    const int64_t low = from_index;
    const int64_t high = to_index;
    const int64_t end = high - 1;


    const int64_t pivot_index1 = sve_vect_partition<vtype, T>(arr, low, high, pivot, false); // use_gt = false (use_ge)
    int64_t lower = pivot_index1;

    const int64_t pivot_index2 = sve_vect_partition<vtype, T>(arr, pivot_index1, high, pivot, true); // use_gt = true
    int64_t upper = pivot_index2;

    pivot_indices[0] = lower;
    pivot_indices[1] = upper;
}

template <typename T>
SVE_SORT_INLINE void insertion_sort(T *arr, int32_t from_index, int32_t to_index) {
    for (int i, k = from_index; ++k < to_index; ) {
        T ai = arr[i = k];
        if (ai < arr[i - 1]) {
            while (--i >= from_index && ai < arr[i]) {
                arr[i + 1] = arr[i];
            }
            arr[i + 1] = ai;
        }
    }
}

template <typename T>
SVE_SORT_INLINE void sve_fast_sort(T *arr, arrsize_t from_index, arrsize_t to_index, const arrsize_t INS_SORT_THRESHOLD)
{
    arrsize_t arrsize = to_index - from_index;

    if (arrsize <= INS_SORT_THRESHOLD) {
        insertion_sort<T>(arr, from_index, to_index);
    } else{
        sve_qsort<sve_vector<T>, T>(arr, from_index, to_index, 2 * (arrsize_t)log2(arrsize));
        sve_oet_sort<sve_vector<T>, T>(arr, from_index, to_index);
    }
}

template <typename T>
SVE_SORT_INLINE void sve_fast_partition(T *arr, int64_t from_index, int64_t to_index, int32_t *pivot_indices, int64_t index_pivot1, int64_t index_pivot2)
{
    if (index_pivot1 != index_pivot2) {
        sve_dual_pivot_partition<sve_vector<T>, T>(arr, from_index, to_index, pivot_indices, index_pivot1, index_pivot2);
    }
    else {
        sve_single_pivot_partition<sve_vector<T>, T>(arr, from_index, to_index, pivot_indices, index_pivot1);
    }
}

extern "C" {

    DLL_PUBLIC void sve_sort(void *array, int elem_type, int32_t from_index, int32_t to_index) {
        switch(elem_type) {
            case JVM_T_INT:
                sve_fast_sort((int32_t*)array, from_index, to_index, 64);
                break;
            case JVM_T_FLOAT:
                sve_fast_sort((float*)array, from_index, to_index, 64);
                break;
            case JVM_T_LONG:
            case JVM_T_DOUBLE:
            default:
                assert(false, "Unexpected type");
        }
    }

    DLL_PUBLIC void sve_partition(void *array, int elem_type, int32_t from_index, int32_t to_index, int32_t *pivot_indices, int32_t index_pivot1, int32_t index_pivot2) {
        switch(elem_type) {
            case JVM_T_INT:
                sve_fast_partition((int32_t*)array, from_index, to_index, pivot_indices, index_pivot1, index_pivot2);
                break;
            case JVM_T_FLOAT:
                sve_fast_partition((float*)array, from_index, to_index, pivot_indices, index_pivot1, index_pivot2);
                break;
            case JVM_T_LONG:
            case JVM_T_DOUBLE:
            default:
                assert(false, "Unexpected type");
        }
    }
}
