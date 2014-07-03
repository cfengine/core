#include <test.h>

#include <csv_writer.h>

void test_empty(void)
{
    Writer *w = StringWriter();
    CsvWriter *c = CsvWriterOpen(w);

    CsvWriterClose(c);
    assert_string_equal(StringWriterClose(w), "");
}

void test_single_field(void)
{
    Writer *w = StringWriter();
    CsvWriter *c = CsvWriterOpen(w);

    CsvWriterField(c, "test");

    CsvWriterClose(c);
    assert_string_equal(StringWriterClose(w), "test\r\n");
}

void test_several_fields(void)
{
    Writer *w = StringWriter();
    CsvWriter *c = CsvWriterOpen(w);

    CsvWriterField(c, "test1");
    CsvWriterField(c, "test2");
    CsvWriterField(c, "test3");

    CsvWriterClose(c);
    assert_string_equal(StringWriterClose(w), "test1,test2,test3\r\n");
}

void test_two_records(void)
{
    Writer *w = StringWriter();
    CsvWriter *c = CsvWriterOpen(w);

    CsvWriterField(c, "test1");
    CsvWriterNewRecord(c);
    CsvWriterField(c, "test2");
    CsvWriterNewRecord(c);

    CsvWriterClose(c);
    assert_string_equal(StringWriterClose(w), "test1\r\ntest2\r\n");
}

void test_empty_record(void)
{
    Writer *w = StringWriter();
    CsvWriter *c = CsvWriterOpen(w);

    CsvWriterNewRecord(c);
    CsvWriterField(c, "test2");
    CsvWriterNewRecord(c);

    CsvWriterClose(c);
    assert_string_equal(StringWriterClose(w), "\r\ntest2\r\n");
}

void test_empty_last_record(void)
{
    Writer *w = StringWriter();
    CsvWriter *c = CsvWriterOpen(w);

    CsvWriterField(c, "test1");
    CsvWriterNewRecord(c);
    CsvWriterNewRecord(c);

    CsvWriterClose(c);
    assert_string_equal(StringWriterClose(w), "test1\r\n\r\n");
}

void test_escape(void)
{
    Writer *w = StringWriter();
    CsvWriter *c = CsvWriterOpen(w);

    CsvWriterField(c, ",\"\r\n");

    CsvWriterClose(c);
    assert_string_equal(StringWriterClose(w), "\",\"\"\r\n\"\r\n");
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_empty),
        unit_test(test_single_field),
        unit_test(test_several_fields),
        unit_test(test_two_records),
        unit_test(test_empty_record),
        unit_test(test_empty_last_record),
        unit_test(test_escape),
    };

    return run_tests(tests);
}

/* STUB OUT */

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
