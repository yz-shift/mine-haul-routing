/*
 * Minimal unit test framework.
 *
 * A failed assertion records the failure and returns from the test function,
 * so the remaining tests still run. It uses a plain return rather than
 * setjmp/longjmp, which keeps control flow simple under valgrind. A test that
 * fails may skip its own cleanup, but the run has already failed by then.
 *
 * Usage:
 *     TEST(test_name) { ASSERT_EQ_INT(4, 2 + 2); }
 *     int main(void) { RUN_TEST(test_name); return TEST_SUMMARY(); }
 */
#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>

static int tf_tests_run = 0;
static int tf_tests_failed = 0;
static int tf_current_failed = 0;

/* static inline: a test file that skips a helper gets no unused warning. */
static inline void tf_mark_failed(const char *file, int line)
{
    tf_current_failed = 1;
    printf("  %s:%d: ", file, line);
}

static inline void tf_run(const char *name, void (*test)(void))
{
    tf_current_failed = 0;
    tf_tests_run++;
    test();
    if (tf_current_failed) {
        tf_tests_failed++;
    }
    printf("%s %s\n", tf_current_failed ? "FAIL" : "PASS", name);
}

/* Prints the totals and returns the process exit code. */
static inline int tf_summary(void)
{
    printf("%d run, %d failed\n", tf_tests_run, tf_tests_failed);
    return tf_tests_failed == 0 ? 0 : 1;
}

#define TEST(name) static void name(void)
#define RUN_TEST(name) tf_run(#name, name)
#define TEST_SUMMARY() tf_summary()

#define ASSERT_TRUE(cond)                                \
    do {                                                 \
        if (!(cond)) {                                   \
            tf_mark_failed(__FILE__, __LINE__);          \
            printf("expected true: %s\n", #cond);        \
            return;                                      \
        }                                                \
    } while (0)

/* Copies into locals so each argument is evaluated exactly once. */
#define ASSERT_EQ_INT(expected, actual)                                      \
    do {                                                                     \
        long long tf_expected = (expected);                                  \
        long long tf_actual = (actual);                                      \
        if (tf_expected != tf_actual) {                                      \
            tf_mark_failed(__FILE__, __LINE__);                              \
            printf("expected %lld, got %lld\n", tf_expected, tf_actual);     \
            return;                                                          \
        }                                                                    \
    } while (0)

#endif /* TEST_FRAMEWORK_H */
