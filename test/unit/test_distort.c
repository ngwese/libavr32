#include "unity.h"

// dependencies
#include "fix16.c"

// this
#include "distort.c"

distort_t d;

void setup(void) {
    distort_init(&d);
}

void test_distort_init(void) {
    // set global static to know values
    fix16_half = fix16_max;
    fix16_neg_one = fix16_max;
    setup(); // should initialize statics
    TEST_ASSERT_EQUAL_UINT32(fix16_half, fix16_from_float(0.5));
    TEST_ASSERT_EQUAL_UINT32(fix16_neg_one, fix16_from_int(-1));
    TEST_ASSERT_EQUAL_UINT32(0, fix16_add(fix16_one, fix16_neg_one));
}

void test_distort_phase_default_is_identity(void) {
    setup();
    TEST_ASSERT_EQUAL_UINT32(0, distort_phase(&d, 0));
    TEST_ASSERT_EQUAL_UINT32(0, distort_phase(&d, fix16_one));
    fix16_t mid = fix16_from_float(0.5);
    TEST_ASSERT_EQUAL_UINT32(mid, distort_phase(&d, mid));
    // ensure wraps
    TEST_ASSERT_EQUAL_UINT32(mid, distort_phase(&d, fix16_add(mid, fix16_neg_one)));
}

void test_distort_phase_ratio(void) {
    fix16_t quarter = fix16_from_float(0.25);
    fix16_t mid = fix16_from_float(0.5);
    setup();
    // ratio 0.5 == 1/2 the period or double speed
    distort_set_ratio(&d, fix16_half);
    TEST_ASSERT_EQUAL_UINT32(0, distort_phase(&d, 0));
    TEST_ASSERT_EQUAL_UINT32(0, distort_phase(&d, mid));
    TEST_ASSERT_EQUAL_UINT32(0, distort_phase(&d, fix16_one));
    TEST_ASSERT_EQUAL_UINT32(mid, distort_phase(&d, quarter));
    // ensure wrap
    TEST_ASSERT_EQUAL_UINT32(mid, distort_phase(&d, fix16_add(quarter, fix16_one)));

    // ratio 2.0 == 2x the period or half speed
    distort_set_ratio(&d, fix16_from_int(2));
    TEST_ASSERT_EQUAL_UINT32(0, distort_phase(&d, 0));
    TEST_ASSERT_EQUAL_UINT32(quarter, distort_phase(&d, mid));
    TEST_ASSERT_EQUAL_UINT32(mid, distort_phase(&d, fix16_one));
    TEST_ASSERT_EQUAL_UINT32(0, distort_phase(&d, fix16_two));
    // ensure wrap
    fix16_t three_quarter = fix16_mul(quarter, fix16_from_int(3));
    TEST_ASSERT_EQUAL_UINT32(three_quarter, distort_phase(&d, fix16_add(mid, fix16_one)));
}

void test_distort_phase_shift(void) {
    fix16_t quarter = fix16_from_float(0.25);
    fix16_t mid = fix16_from_float(0.5);
    fix16_t three_quarter = fix16_mul(quarter, fix16_from_int(3));
    setup();
    // positive shift
    distort_set_shift(&d, quarter);
    TEST_ASSERT_EQUAL_UINT32(quarter, distort_phase(&d, 0));
    TEST_ASSERT_EQUAL_UINT32(three_quarter, distort_phase(&d, mid));
    TEST_ASSERT_EQUAL_UINT32(0, distort_phase(&d, three_quarter));
    // negative shift
    distort_set_shift(&d, fix16_mul(quarter, fix16_from_int(-1)));
    TEST_ASSERT_EQUAL_UINT32(three_quarter, distort_phase(&d, 0));
    TEST_ASSERT_EQUAL_UINT32(quarter, distort_phase(&d, mid));
    TEST_ASSERT_EQUAL_UINT32(mid, distort_phase(&d, three_quarter));
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_distort_init);
    RUN_TEST(test_distort_phase_default_is_identity);
    RUN_TEST(test_distort_phase_ratio);
    RUN_TEST(test_distort_phase_shift);

    return UNITY_END();
}
