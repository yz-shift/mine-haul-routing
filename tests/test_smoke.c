/* Checks that the build and test toolchain work before any real code exists. */
#include "test_framework.h"

TEST(test_empty)
{
}

TEST(test_passing_assertions)
{
    ASSERT_TRUE(1 + 1 == 2);
    ASSERT_EQ_INT(4, 2 + 2);
}

int main(void)
{
    RUN_TEST(test_empty);
    RUN_TEST(test_passing_assertions);
    return TEST_SUMMARY();
}
