#include "test.h"

#include "symbol_table.h"
#include "rlist.h"

void test_add_get(void **state)
{
    SymbolTable *t = SymbolTableNew();

    Rval rval = ((Rval) { xstrdup("snookie"), RVAL_TYPE_SCALAR } );
    SymbolTableAdd(t, "ns", "bundle", "name", rval, DATA_TYPE_STRING);

    SymbolTableEntry *entry = SymbolTableGet(t, "ns", "bundle", "name");
    assert_true(entry);
    assert_string_equal("name", entry->lval);
    assert_string_equal("snookie", RvalScalarValue(entry->rval));
    assert_int_equal(DATA_TYPE_STRING, entry->type);

    SymbolTableDestroy(t);
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_add_get)
    };

    return run_tests(tests);
}
