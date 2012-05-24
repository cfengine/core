#include "cf3.defs.h"

#include <setjmp.h>
#include <stdarg.h>
#include <cmockery.h>

#include "writer.h"

static Writer *global_w;
static bool global_w_closed;

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    for (int i = 0; i < nmemb; ++i)
    {
        char *b = xstrndup(ptr + i * size, size);

        WriterWrite(global_w, b);
        free(b);
    }
    return nmemb * size;
}

int fclose(FILE *stream)
{
    global_w_closed = true;
    return 0;
}

void test_empty_file_buffer(void **p)
{
    global_w = StringWriter();
    global_w_closed = false;
    Writer *w = FileWriter(NULL);

    assert_int_equal(StringWriterLength(global_w), 0);
    assert_string_equal(StringWriterData(global_w), "");

    WriterClose(w);
    WriterClose(global_w);
    assert_int_equal(global_w_closed, true);
}

void test_write_empty_file_buffer(void **p)
{
    global_w = StringWriter();
    Writer *w = FileWriter(NULL);

    WriterWrite(w, "");

    assert_int_equal(StringWriterLength(global_w), 0);
    assert_string_equal(StringWriterData(global_w), "");

    WriterClose(w);
    WriterClose(global_w);
    assert_int_equal(global_w_closed, true);
}

void test_write_file_buffer(void **p)
{
    global_w = StringWriter();
    Writer *w = FileWriter(NULL);

    WriterWrite(w, "123");

    assert_int_equal(StringWriterLength(global_w), 3);
    assert_string_equal(StringWriterData(global_w), "123");

    WriterClose(w);
    WriterClose(global_w);
    assert_int_equal(global_w_closed, true);
}

void test_multiwrite_file_buffer(void **p)
{
    global_w = StringWriter();
    Writer *w = FileWriter(NULL);

    WriterWrite(w, "123");
    WriterWrite(w, "456");

    assert_int_equal(StringWriterLength(global_w), 6);
    assert_string_equal(StringWriterData(global_w), "123456");

    WriterClose(w);
    WriterClose(global_w);
    assert_int_equal(global_w_closed, true);
}

int main()
{
    const UnitTest tests[] =
{
        unit_test(test_empty_file_buffer),
        unit_test(test_write_empty_file_buffer),
        unit_test(test_write_file_buffer),
        unit_test(test_multiwrite_file_buffer),
    };

    return run_tests(tests);
}

/* STUB */

void FatalError(char *s, ...)
{
    fail();
    exit(42);
}
