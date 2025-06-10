#ifndef SVE_QSORT_VECTOR
#define SVE_QSORT_VECTOR

#include <cstdint>
#include <cfloat>
#include <arm_sve.h>

typedef size_t arrsize_t;

template <typename type>
struct sve_vector;

template <>
struct sve_vector<int32_t> {
    using type_t = int32_t;
    using reg_t = svint32_t;       // SVE2 64-bit integer vector
    using opmask_t = svbool_t;     // predicate register
    static const uint8_t numlanes = 4; // use constant intead of svcntw

    static type_t type_max() { return INT32_MAX; }
    static type_t type_min() { return INT32_MIN; }

    static opmask_t knot_opmask(opmask_t x) { return svnot_b_z(svptrue_b32(), x); }

    static opmask_t ge(reg_t x, reg_t y) {
        return svcmpge_s32(svptrue_b32(),x, y);
    }

    static opmask_t gt(reg_t x, reg_t y) {
        return svcmpgt_s32(svptrue_b32(),x, y);
    }

    static reg_t loadu(void const *mem) { return svld1_s32(svptrue_b32(), (const int32_t*)mem); }

    //To improve: Is it possible not to calculate store mask
    static void mask_compressstoreu(void *mem, opmask_t mask, reg_t x) {
        svint32_t compressed = svcompact_s32(mask, x);
        uint64_t num_active = svcntp_b32(svptrue_b32(), mask); // svcntp_b32(mask, mask);
        svbool_t store_mask = svwhilelt_b32_u64(0, num_active);
        svst1_s32(store_mask, (int32_t*)mem, compressed);
    }

    //keep unmask element unchange
    static reg_t mask_loadu(reg_t x, opmask_t mask, void const *mem) {
        svint32_t loaded_data = svld1_s32(mask, (const int32_t*)mem);
        return svsel_s32(mask, loaded_data, x);
    }

    static reg_t mask_mov(reg_t x, opmask_t mask, reg_t y) {
        return svsel_s32(mask, y, x);
    }
    static void mask_storeu(void *mem, opmask_t mask, reg_t x) {
        svst1_s32(mask, (int32_t*)mem, x);
    }

    /*To Check: overflow of idx * 4 */
    static reg_t permutexvar(svuint32_t idx, reg_t v) {
        return svtbl_s32(v, idx);
    }

    static type_t reducemax(reg_t v) { return svmaxv_s32(svptrue_b32(), v); }
    static type_t reducemin(reg_t v) { return svminv_s32(svptrue_b32(), v); }
    static reg_t set1(type_t v) { return svdup_n_s32(v); }

    static void storeu(void *mem, reg_t x) {
        return svst1_s32(svptrue_b32(), (int32_t*)mem, x);
    }

    static reg_t min(reg_t x, reg_t y) { return svmin_s32_z(svptrue_b32(), x, y); }
    static reg_t max(reg_t x, reg_t y) { return svmax_s32_z(svptrue_b32(), x, y); }

    static reg_t cast_from(svint64_t v) { return svreinterpret_s32_s64(v); }
    static svint64_t cast_to(reg_t v) { return svreinterpret_s64_s32(v); }

    static int double_compressstore(type_t *left_addr, type_t *right_addr,
                                    opmask_t k, reg_t reg) {
        uint64_t amount_ge_pivot = svcntp_b32(svptrue_b32(), k);
        uint64_t amount_nge_pivot = numlanes - amount_ge_pivot;

        svint32_t compressed_1 = svcompact_s32(knot_opmask(k), reg);
        svint32_t compressed_2 = svcompact_s32(k, reg);

        svbool_t store_mask_1 = svwhilelt_b32_u64(0, amount_nge_pivot);
        svbool_t store_mask_2 = svwhilelt_b32_u64(0, amount_ge_pivot);

        svst1_s32(store_mask_1, (int32_t*)left_addr, compressed_1); // TO IMPROVE: STORE_MASK_1 = ALL TRUE
        svst1_s32(store_mask_2, (int32_t*)(right_addr + amount_nge_pivot), compressed_2);

        return amount_ge_pivot;
    }

    static arrsize_t partition_vec(type_t *l_store, type_t *r_store,
                                                 const reg_t curr_vec,
                                                 const reg_t pivot_vec,
                                                 reg_t &smallest_vec,
                                                 reg_t &biggest_vec, bool use_gt) {
        opmask_t mask;
        if (use_gt) mask = gt(curr_vec, pivot_vec);
        else mask = ge(curr_vec, pivot_vec);

        int amount_ge_pivot =
            double_compressstore(l_store, r_store, mask, curr_vec);

        smallest_vec = min(curr_vec, smallest_vec);
        biggest_vec = max(curr_vec, biggest_vec);

        return amount_ge_pivot;
    }

    static void oet_sort(type_t *arr, arrsize_t num) {
        svbool_t p1 = svwhilelt_b32_u64(0, num);
        const svint32x2_t z0_z1 = svld2_s32(p1, arr);
        const svbool_t p2 = svcmplt_s32(p1, svget2_s32(z0_z1, 0), svget2_s32(z0_z1, 1));

        const svint32_t z4 = svsel_s32(p2, svget2_s32(z0_z1, 0), svget2_s32(z0_z1, 1)); // z4 <- smaller values
        const svint32_t z5 = svsel_s32(p2, svget2_s32(z0_z1, 1), svget2_s32(z0_z1, 0)); // z5 <- larger values

        svst2_s32(p1, arr, svcreate2_s32(z4, z5));
    }
};

template <>
struct sve_vector<float> {
    using type_t = float;
    using reg_t = svfloat32_t;       // SVE2 64-bit float vector
    using opmask_t = svbool_t;     // predicate register
    static const uint8_t numlanes = 4;

    static type_t type_max() { return FLT_MAX; }
    static type_t type_min() { return FLT_MIN; }

    static opmask_t knot_opmask(opmask_t x) { return svnot_b_z(svptrue_b32(), x); }

    static opmask_t ge(reg_t x, reg_t y) {
        return svcmpge_f32(svptrue_b32(),x, y);
    }

    static opmask_t gt(reg_t x, reg_t y) {
        return svcmpgt_f32(svptrue_b32(),x, y);
    }

    static reg_t loadu(void const *mem) { return svld1_f32(svptrue_b32(), (const float*)mem); }

    static void mask_compressstoreu(void *mem, opmask_t mask, reg_t x) {
        svfloat32_t compressed = svcompact_f32(mask, x);
        uint64_t num_active = svcntp_b32(svptrue_b32(), mask); // svcntp_b32(mask, mask);
        svbool_t store_mask = svwhilelt_b32_u64(0, num_active);
        svst1_f32(store_mask, (float32_t*)mem, compressed);
    }

    //keep unmask element unchange
    static reg_t mask_loadu(reg_t x, opmask_t mask, void const *mem) {
        svfloat32_t loaded_data = svld1_f32(mask, (const float32_t*)mem);
        return svsel_f32(mask, loaded_data, x);
    }
    static reg_t mask_mov(reg_t x, opmask_t mask, reg_t y) {
        return svsel_f32(mask, y, x);
    }
    static void mask_storeu(void *mem, opmask_t mask, reg_t x) {
        svst1_f32(mask, (float32_t*)mem, x);
    }

    /*To Check: overflow of idx * 4*/
    static reg_t permutexvar(svuint32_t idx, reg_t v) {
        return svtbl_f32(v, idx);
    }

    static type_t reducemax(reg_t v) { return svmaxv_f32(svptrue_b32(), v); }
    static type_t reducemin(reg_t v) { return svminv_f32(svptrue_b32(), v); }
    static reg_t set1(type_t v) { return svdup_n_f32(v); }

    static void storeu(void *mem, reg_t x) {
        return svst1_f32(svptrue_b32(), (float32_t*)mem, x);
    }

    static reg_t min(reg_t x, reg_t y) { return svmin_f32_z(svptrue_b32(), x, y); }
    static reg_t max(reg_t x, reg_t y) { return svmax_f32_z(svptrue_b32(), x, y); }

    static reg_t cast_from(svfloat64_t v) { return svreinterpret_f32_f64(v); }
    static svfloat64_t cast_to(reg_t v) { return svreinterpret_f64_f32(v); }

    static int double_compressstore(type_t *left_addr, type_t *right_addr,
                                    opmask_t k, reg_t reg) {
        uint64_t amount_ge_pivot = svcntp_b32(svptrue_b32(), k);
        uint64_t amount_nge_pivot = numlanes - amount_ge_pivot;

        svfloat32_t compressed_1 = svcompact_f32(knot_opmask(k), reg);
        svfloat32_t compressed_2 = svcompact_f32(k, reg);

        svbool_t store_mask_1 = svwhilelt_b32_u64(0, amount_nge_pivot);
        svbool_t store_mask_2 = svwhilelt_b32_u64(0, amount_ge_pivot);

        svst1_f32(store_mask_1, (float32_t*)left_addr, compressed_1); // TO IMPROVE: STORE_MASK_1 = ALL TRUE
        svst1_f32(store_mask_2, (float32_t*)(right_addr + amount_nge_pivot), compressed_2);

        return amount_ge_pivot;
    }

    static arrsize_t partition_vec(type_t *l_store, type_t *r_store,
                                                 const reg_t curr_vec,
                                                 const reg_t pivot_vec,
                                                 reg_t &smallest_vec,
                                                 reg_t &biggest_vec, bool use_gt) {
        opmask_t mask;
        if (use_gt) mask = gt(curr_vec, pivot_vec);
        else mask = ge(curr_vec, pivot_vec);

        int amount_ge_pivot =
            double_compressstore(l_store, r_store, mask, curr_vec);

        smallest_vec = min(curr_vec, smallest_vec);
        biggest_vec = max(curr_vec, biggest_vec);

        return amount_ge_pivot;
    }

    static void oet_sort(type_t *arr, arrsize_t num) {
        svbool_t p1 = svwhilelt_b32_u64(0, num);
        const svfloat32x2_t z0_z1 = svld2_f32(p1, arr);
        const svbool_t p2 = svcmplt_f32(p1, svget2_f32(z0_z1, 0), svget2_f32(z0_z1, 1));

        const svfloat32_t z4 = svsel_f32(p2, svget2_f32(z0_z1, 0), svget2_f32(z0_z1, 1)); // z4 <- smaller values
        const svfloat32_t z5 = svsel_f32(p2, svget2_f32(z0_z1, 1), svget2_f32(z0_z1, 0)); // z5 <- larger values

        svst2_f32(p1, arr, svcreate2_f32(z4, z5));
    }
};


#endif  // SVE_QSORT_VECTOR
