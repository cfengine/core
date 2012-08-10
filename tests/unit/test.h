#ifndef CFENGINE_TEST_H
#define CFENGINE_TEST_H

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmockery.h>
#include <stdio.h>

char *file_read_string(FILE *in);

void assert_file_equal(FILE *a, FILE *b);

#define assert_double_close(a, b) _assert_double_close(a, b, __FILE__, __LINE__)

void _assert_double_close(double left, double right, const char *const file, const int line);

#define PRINT_TEST_BANNER() \
    printf("==================================================\n");\
    printf("Starting test: %s\n", __FILE__);\
    printf("==================================================\n")

#endif
