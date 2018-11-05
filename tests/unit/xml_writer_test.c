#include <test.h>

#include <cf3.defs.h>
#include <xml_writer.h>

/*
 * FIXME: Those unit tests need to be ajusted (or completely changed) if
 * XmlWriter internals are changed, as the tests expect particular layout and
 * escaping of generated XML.
 */

void test_comment(void)
{
    Writer *w = StringWriter();

    XmlComment(w, "foobar");
    char *result = StringWriterClose(w);
    assert_string_equal(result, "<!-- foobar -->\n");
    free(result);
}

void test_no_attr(void)
{
    Writer *w = StringWriter();

    XmlTag(w, "foobar", NULL, 0);
    char *result = StringWriterClose(w);
    assert_string_equal(result, "<foobar></foobar>\n");
    free(result);
}

void test_tag(void)
{
    Writer *w = StringWriter();

    XmlTag(w, "foobar", "some value", 1, (XmlAttribute)
           {
           "a", "b"});
    char *result = StringWriterClose(w);
    assert_string_equal(result, "<foobar a=\"b\" >some value</foobar>\n");
    free(result);
}

void test_complex_tag(void)
{
    Writer *w = StringWriter();

    XmlStartTag(w, "complex-tag", 2, (XmlAttribute)
                {
                "attr1", "value1"}, (XmlAttribute)
                {
                "attr2", "value2"});
    XmlContent(w, "Some content");
    XmlEndTag(w, "complex-tag");
    char *result = StringWriterClose(w);
    assert_string_equal(result,
                        "<complex-tag attr1=\"value1\" attr2=\"value2\" >\nSome content</complex-tag>\n");
    free(result);
}

void test_escape(void)
{
    Writer *w = StringWriter();

    XmlContent(w, "&>\"'<");
    char *result = StringWriterClose(w);
    assert_string_equal(result, "&amp;&gt;&quot;&apos;&lt;");
    free(result);
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_comment),
        unit_test(test_no_attr),
        unit_test(test_tag),
        unit_test(test_complex_tag),
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
