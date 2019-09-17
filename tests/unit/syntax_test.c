#include <test.h>

#include <syntax.h>
#include <regex.h>                                       /* StringMatchFull */

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

static void test_copy_from_servers(void)
{
    const BodySyntax *x = BodySyntaxGet("copy_from");
    assert_true(x);

    const ConstraintSyntax *y = BodySyntaxGetConstraintSyntax(x->constraints, "servers");
    assert_true(y);

    assert_true(StringMatchFull(y->range.validation_string, "127.0.0.1"));
    assert_true(StringMatchFull(y->range.validation_string, "www-dashed.stuff.com"));
    assert_true(StringMatchFull(y->range.validation_string, "2604:2000:8441:e300:224:d7ff:fec5:338"));
}

static void test_typecheck_null_rval(void)
{
    SyntaxTypeMatch err = CheckConstraintTypeMatch("whatever", (Rval) { NULL, RVAL_TYPE_NOPROMISEE },
                                                   CF_DATA_TYPE_STRING, "abc", 0);
    assert_int_equal(SYNTAX_TYPE_MATCH_ERROR_GOT_NULL, err);
}

static void test_check_parse_variable_name(void)
{
    // Test was added after function
    // It shows what it actually does (not what was intended)

    // Allowed variable "names":
    assert_true(CheckParseVariableName("a"));
    assert_true(CheckParseVariableName("myvar"));
    assert_true(CheckParseVariableName("SuperCaliFragilisticExpialidocius"));
    assert_true(CheckParseVariableName("a.b"));
    assert_true(CheckParseVariableName("a[b]"));
    assert_true(CheckParseVariableName("a[b.c]"));
    assert_true(CheckParseVariableName("a[b.c.d]"));
    assert_true(CheckParseVariableName("a.b[c.d.e]"));
    assert_true(CheckParseVariableName("a.b[c.d.e]"));
    assert_true(CheckParseVariableName("Namespace.var[$(ns.expand)]"));

    // Not allowed:
    assert_false(CheckParseVariableName("a.b.c"));
    assert_false(CheckParseVariableName("abc.def.ghi"));
    assert_false(CheckParseVariableName(".a"));
    assert_false(CheckParseVariableName("a."));
    assert_false(CheckParseVariableName(".abc"));
    assert_false(CheckParseVariableName("abc."));

    // Reserved:
    assert_false(CheckParseVariableName("promiser"));
    assert_false(CheckParseVariableName("handle"));
    assert_false(CheckParseVariableName("promise_filename"));
    assert_false(CheckParseVariableName("promise_dirname"));
    assert_false(CheckParseVariableName("promise_linenumber"));
    assert_false(CheckParseVariableName("this"));

    // Edge cases, allowed:
    assert_true(CheckParseVariableName(""));
    assert_true(CheckParseVariableName(" "));
    assert_true(CheckParseVariableName("    "));
    assert_true(CheckParseVariableName("\t"));
    assert_true(CheckParseVariableName("a["));
    assert_true(CheckParseVariableName("a.["));

    // Edge cases, not allowed:
    assert_false(CheckParseVariableName("."));
    assert_false(CheckParseVariableName("..."));
    assert_false(CheckParseVariableName(".[]"));
    assert_false(CheckParseVariableName(".[][]"));
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

        unit_test(test_lookup_constraint_edit_xml_set_attribute_attribute_value),

        unit_test(test_copy_from_servers),
        unit_test(test_typecheck_null_rval),
        unit_test(test_check_parse_variable_name),
    };

    return run_tests(tests);
}
