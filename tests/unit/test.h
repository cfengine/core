/* ALWAYS INCLUDE FIRST, so that platform.h is included first as well! */


#ifndef CFENGINE_TEST_H
#define CFENGINE_TEST_H


#include <platform.h>

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmockery.h>
#include <stdio.h>


/* Use this define for specific overrides inside all our source tree. */
#define CFENGINE_TEST


#define PRINT_TEST_BANNER()                                             \
    printf("==================================================\n");     \
    printf("Starting test: %s\n", __FILE__);                            \
    printf("==================================================\n")


char *file_read_string(FILE *in);

void assert_file_equal(FILE *a, FILE *b);

#define assert_double_close(a, b) _assert_double_close(a, b, __FILE__, __LINE__)
void _assert_double_close(double left, double right, const char *const file, const int line);

void test_progress(void);
void test_progress_end(void);

// like assert_string_equal, but better with respect to pointers:
// if a and b are NULL, returns true
// if a or b is NULL print error using assert_int
// if a and b are not NULL, use assert_string
#define assert_string_int_equal(a, b)\
{\
    const char* x = a;\
    const char* y = b;\
    if (x!=y)\
    {\
        if (x==NULL||y==NULL)\
        {\
            assert_int_equal(x, y);\
        }\
        else\
        {\
            assert_string_equal(x, y);\
        }\
    }\
}\

#endif
