#include "cf3.defs.h"
#include "cf3.extern.h"

#include <setjmp.h>
#include <stdarg.h>
#include <cmockery.h>

#include "writer.h"

void test_empty_string_buffer(void **p)
{
Writer *w = StringWriter();

assert_int_equal(StringWriterLength(w), 0);
assert_string_equal(StringWriterData(w), "");

WriterClose(w);
}

void test_write_empty_string_buffer(void **p)
{
Writer *w = StringWriter();

WriterWrite(w, "");

assert_int_equal(StringWriterLength(w), 0);
assert_string_equal(StringWriterData(w), "");

WriterClose(w);
}

void test_write_string_buffer(void **p)
{
Writer *w = StringWriter();

WriterWrite(w, "123");

assert_int_equal(StringWriterLength(w), 3);
assert_string_equal(StringWriterData(w), "123");

WriterClose(w);
}

void test_multiwrite_string_buffer(void **p)
{
Writer *w = StringWriter();

WriterWrite(w, "123");
WriterWrite(w, "456");

assert_int_equal(StringWriterLength(w), 6);
assert_string_equal(StringWriterData(w), "123456");

WriterClose(w);
}

int main()
{
const UnitTest tests[] =
   {
   unit_test(test_empty_string_buffer),
   unit_test(test_write_empty_string_buffer),
   unit_test(test_write_string_buffer),
   unit_test(test_multiwrite_string_buffer),
   };

return run_tests(tests);
}
