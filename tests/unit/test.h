#ifndef CFENGINE_TEST_H
#define CFENGINE_TEST_H

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmockery.h>
#include <stdio.h>

char *file_read_string(FILE *in);

void assert_file_equal(FILE *a, FILE *b);

#endif
