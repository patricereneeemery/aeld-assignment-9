#include "unity.h"
#include <stdbool.h>
#include <stdlib.h>
#include "../../examples/autotest-validate/autotest-validate.h"
#include "../../assignment-autotest/test/assignment1/username-from-conf-file.h"

#include <stdio.h>
#include <string.h>
//#include "unity.h"
//#include "autotest-validate.h"
//#include "username-from-conf-file.h"

// ----------------------------------------------------------------------
// Test: validate that my_username() returns the correct GitHub username
// ----------------------------------------------------------------------
void test_validate_my_username(void)
{
    const char *expected = "patricereneeemery";
    const char *actual = my_username();

    TEST_ASSERT_NOT_NULL(actual);
    TEST_ASSERT_EQUAL_STRING(expected, actual);
}

// ----------------------------------------------------------------------
// Test: validate that malloc_username_from_conf_file() reads username.txt
// ----------------------------------------------------------------------
void test_validate_conf_username(void)
{
    const char *expected = "patricereneeemery";

    char *conf_username = malloc_username_from_conf_file();

    TEST_ASSERT_NOT_NULL(conf_username);
    TEST_ASSERT_EQUAL_STRING(expected, conf_username);

    free(conf_username);
}

// ----------------------------------------------------------------------
// Unity test runner
// ----------------------------------------------------------------------
int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_validate_my_username);
    RUN_TEST(test_validate_conf_username);

    return UNITY_END();
}
