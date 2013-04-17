#include "test.h"

#include "syntax.h"

static void test_lookup_promise_type_agent_vars(void)
{
    const PromiseTypeSyntax *s = PromiseTypeSyntaxLookup("agent", "vars");
    assert_true(s);
    assert_string_equal("vars", s->promise_type);
}

static void test_lookup_promise_type_common_vars(void)
{
    const PromiseTypeSyntax *s = PromiseTypeSyntaxLookup("common", "vars");
    assert_true(s);
    assert_string_equal("vars", s->promise_type);
}

static void test_lookup_promise_type_edit_xml_build_xpath(void)
{
    const PromiseTypeSyntax *s = PromiseTypeSyntaxLookup("edit_xml", "build_xpath");
    assert_true(s);
    assert_string_equal("build_xpath", s->promise_type);
}

static void test_lookup_promise_type_edit_line_delete_lines(void)
{
    const PromiseTypeSyntax *s = PromiseTypeSyntaxLookup("edit_line", "delete_lines");
    assert_true(s);
    assert_string_equal("delete_lines", s->promise_type);
}

static void test_lookup_constraint_edit_xml_set_attribute_attribute_value(void)
{
    const PromiseTypeSyntax *s = PromiseTypeSyntaxLookup("edit_xml", "set_attribute");
    assert_true(s);
    assert_string_equal("set_attribute", s->promise_type);

    const ConstraintSyntax *x = PromiseTypeSyntaxGetConstraintSyntax(s, "attribute_value");
    assert_true(x);
    assert_string_equal("attribute_value", x->lval);
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_lookup_promise_type_agent_vars),
        unit_test(test_lookup_promise_type_common_vars),
        unit_test(test_lookup_promise_type_edit_xml_build_xpath),
        unit_test(test_lookup_promise_type_edit_line_delete_lines),

        unit_test(test_lookup_constraint_edit_xml_set_attribute_attribute_value)
    };

    return run_tests(tests);
}
