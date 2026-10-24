#include "unity.h"
//include "autotest-validate.h"
//#include "username-from-conf-file.h"
#include <stdbool.h>
#include <stdlib.h>
#include "../../examples/autotest-validate/autotest-validate.h"
#include "../../assignment-autotest/test/assignment1/username-from-conf-file.h"


void test_validate_my_username()
{
    char *actual = malloc_username_from_conf_file();

    TEST_ASSERT_EQUAL_STRING_MESSAGE("patricereneeemery", actual,
        "username.txt does not contain the expected username");

    free(actual);
}
