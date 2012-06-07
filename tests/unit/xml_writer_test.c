#include "cf3.defs.h"

#include <setjmp.h>
#include <stdarg.h>
#include <cmockery.h>

#include "xml_writer.h"

/*
 * FIXME: Those unit tests need to be ajusted (or completely changed) if
 * XmlWriter internals are changed, as the tests expect particular layout and
 * escaping of generated XML.
 */

void test_comment(void **state)
{
    Writer *w = StringWriter();

    XmlComment(w, "foobar");
    assert_string_equal(StringWriterClose(w), "<!-- foobar -->\n");
}

void test_no_attr(void **state)
{
    Writer *w = StringWriter();

    XmlTag(w, "foobar", NULL, 0);
    assert_string_equal(StringWriterClose(w), "<foobar></foobar>\n");
}

void test_tag(void **state)
{
    Writer *w = StringWriter();

    XmlTag(w, "foobar", "some value", 1, (XmlAttribute)
           {
           "a", "b"});
    assert_string_equal(StringWriterClose(w), "<foobar a=\"b\" >some value</foobar>\n");
}

void test_complex_tag(void **state)
{
    Writer *w = StringWriter();

    XmlStartTag(w, "complex-tag", 2, (XmlAttribute)
                {
                "attr1", "value1"}, (XmlAttribute)
                {
                "attr2", "value2"});
    XmlContent(w, "Some content");
    XmlEndTag(w, "complex-tag");

    assert_string_equal(StringWriterClose(w),
                        "<complex-tag attr1=\"value1\" attr2=\"value2\" >\nSome content</complex-tag>\n");
}

void test_escape(void **state)
{
    Writer *w = StringWriter();

    XmlContent(w, "&>\"'<");
    assert_string_equal(StringWriterClose(w), "&amp;&gt;&quot;&apos;&lt;");
}

int main()
{
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

void FatalError(char *s, ...)
{
    fail();
    exit(42);
}
