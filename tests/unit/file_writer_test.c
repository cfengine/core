#include <test.h>

#include <alloc.h>
#include <writer.h>


static Writer *global_w;
static bool global_w_closed;


/* Override libc's fwrite(). */
static size_t CFENGINE_TEST_fwrite(const void *ptr, size_t size, size_t nmemb,
                                   ARG_UNUSED FILE *stream)
{
    for (int i = 0; i < nmemb; ++i)
    {
        char *b = xstrndup(ptr + i * size, size);

        WriterWrite(global_w, b);
        free(b);
    }
    return nmemb * size;
}

/* Override libc's fclose(). */
static int CFENGINE_TEST_fclose(ARG_UNUSED FILE *stream)
{
    global_w_closed = true;
    return 0;
}


#include <writer.c>                /* MUST be included after the overrides. */


void test_empty_file_buffer(void)
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

void test_write_empty_file_buffer(void)
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

void test_write_file_buffer(void)
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

void test_multiwrite_file_buffer(void)
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
    PRINT_TEST_BANNER();
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

void __ProgrammingError(ARG_UNUSED const char *file,
                        ARG_UNUSED int lineno,
                        ARG_UNUSED const char *format, ...)
{
    fail();
    exit(42);
}

void FatalError(ARG_UNUSED char *s, ...)
{
    fail();
    exit(42);
}
