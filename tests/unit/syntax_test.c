#include "test.h"

#include "syntax.h"

static void test_lookup_promise_type_agent_vars(void)
{
    const PromiseTypeSyntax *s = PromiseTypeSyntaxGet("agent", "vars");
    assert_true(s);
    assert_string_equal("vars", s->promise_type);
}

static void test_lookup_promise_type_common_vars(void)
{
    const PromiseTypeSyntax *s = PromiseTypeSyntaxGet("common", "vars");
    assert_true(s);
    assert_string_equal("vars", s->promise_type);
}

static void test_lookup_promise_type_edit_xml_build_xpath(void)
{
    const PromiseTypeSyntax *s = PromiseTypeSyntaxGet("edit_xml", "build_xpath");
    assert_true(s);
    assert_string_equal("build_xpath", s->promise_type);
}

static void test_lookup_promise_type_edit_line_delete_lines(void)
{
    const PromiseTypeSyntax *s = PromiseTypeSyntaxGet("edit_line", "delete_lines");
    assert_true(s);
    assert_string_equal("delete_lines", s->promise_type);
}

static void test_lookup_promise_type_edit_xml_commons(void)
{
    const PromiseTypeSyntax *s = PromiseTypeSyntaxGet("edit_xml", "*");
    assert_true(s);
    assert_string_equal("edit_xml", s->bundle_type);
    assert_string_equal("*", s->promise_type);
}

static void test_lookup_promise_type_global_commons(void)
{
    const PromiseTypeSyntax *s = PromiseTypeSyntaxGet("*", "*");
    assert_true(s);
    assert_string_equal("*", s->bundle_type);
    assert_string_equal("*", s->promise_type);
}


static void test_lookup_constraint_edit_xml_set_attribute_attribute_value(void)
{
    const PromiseTypeSyntax *s = PromiseTypeSyntaxGet("edit_xml", "set_attribute");
    assert_true(s);
    assert_string_equal("set_attribute", s->promise_type);

    const ConstraintSyntax *x = PromiseTypeSyntaxGetConstraintSyntax(s, "attribute_value");
    assert_true(x);
    assert_string_equal("attribute_value", x->lval);
}

static void test_lookup_body_classes(void)
{
    const BodySyntax *x = BodySyntaxGet("classes");
    assert_true(x);

    const ConstraintSyntax *y = BodySyntaxGetConstraintSyntax(x->constraints, "promise_repaired");
    assert_true(y);
    assert_string_equal("promise_repaired", y->lval);
}

static void test_lookup_body_process_count(void)
{
    const BodySyntax *x = BodySyntaxGet("process_count");
    assert_true(x);

    const ConstraintSyntax *y = BodySyntaxGetConstraintSyntax(x->constraints, "match_range");
    assert_true(y);
    assert_string_equal("match_range", y->lval);
}

static void test_lookup_body_delete_select(void)
{
    const BodySyntax *x = BodySyntaxGet("delete_select");
    assert_true(x);

    const ConstraintSyntax *y = BodySyntaxGetConstraintSyntax(x->constraints, "delete_if_startwith_from_list");
    assert_true(y);
    assert_string_equal("delete_if_startwith_from_list", y->lval);
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

        unit_test(test_lookup_promise_type_edit_xml_commons),
        unit_test(test_lookup_promise_type_global_commons),

        unit_test(test_lookup_body_classes),
        unit_test(test_lookup_body_process_count),
        unit_test(test_lookup_body_delete_select),

        unit_test(test_lookup_constraint_edit_xml_set_attribute_attribute_value)
    };

    return run_tests(tests);
}
