#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "unity.h"
#include "autotest-validate.h"
#include "username-from-conf-file.h"

// Required Unity functions
void setUp(void) {}
void tearDown(void) {}

// ------------------------------------------------------------
// Your implementation of my_username()
const char* my_username()
{
    return "patricereneeemery";   // <-- your GitHub username
}

// ------------------------------------------------------------
// Test: compare my_username() with username from conf file
void test_validate_my_username(void)
{
    const char *expected = my_username();
    char *actual = malloc_username_from_conf_file();

    TEST_ASSERT_NOT_NULL_MESSAGE(actual,
        "malloc_username_from_conf_file() returned NULL");

    TEST_ASSERT_EQUAL_STRING_MESSAGE(expected, actual,
        "Username from my_username() does not match username from conf file");

    free(actual);
}

// ------------------------------------------------------------
// Unity test runner
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_validate_my_username);
    return UNITY_END();
}
