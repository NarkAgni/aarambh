#ifndef AARAMBH_TESTS_UNIT_TEST_H
#define AARAMBH_TESTS_UNIT_TEST_H

#include <stdio.h>

extern int test_failures;

#define CHECK(condition, description)                         \
    do {                                                      \
        if (condition) {                                      \
            printf("  ok: %s\n", description);                \
        } else {                                              \
            printf("  FAIL: %s (%s:%d)\n", description,       \
                   __FILE__, __LINE__);                       \
            test_failures++;                                  \
        }                                                     \
    } while (0)

int test_result(void);

#endif
