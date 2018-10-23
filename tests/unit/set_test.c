#include <test.h>

#include <set.h>
#include <alloc.h>

void test_stringset_from_string(void)
{
    StringSet *s = StringSetFromString("one,two, three four,,", ',');

    assert_true(StringSetContains(s, "one"));
    assert_true(StringSetContains(s, "two"));
    assert_true(StringSetContains(s, " three four"));
    assert_true(StringSetContains(s, ""));

    assert_int_equal(4, StringSetSize(s));

    StringSetDestroy(s);
}

void test_stringset_clear(void)
{
    StringSet *s = StringSetNew();
    StringSetAdd(s, xstrdup("a"));
    StringSetAdd(s, xstrdup("b"));

    assert_int_equal(2, StringSetSize(s));

    StringSetClear(s);

    assert_int_equal(0, StringSetSize(s));

    StringSetDestroy(s);
}

void test_stringset_serialization(void)
{
    {
        StringSet *set = StringSetNew();
        StringSetAdd(set, xstrdup("tag_1"));
        StringSetAdd(set, xstrdup("tag_2"));
        StringSetAdd(set, xstrdup("tag_3"));

        Buffer *buff = StringSetToBuffer(set, ',');

        assert_true(buff);
        assert_string_equal(BufferData(buff), "tag_1,tag_2,tag_3");

        BufferDestroy(buff);
        StringSetDestroy(set);
    }

    {
        StringSet *set = StringSetNew();

        Buffer *buff = StringSetToBuffer(set, ',');

        assert_true(buff);
        assert_string_equal(BufferData(buff), "");

        BufferDestroy(buff);
        StringSetDestroy(set);
    }
}

void test_stringset_join(void)
{
    {
        StringSet *set1 = StringSetNew();
        StringSet *set2 = StringSetNew();
        StringSetJoin(set1, set2, xstrdup);
        StringSetDestroy(set2);

        assert_int_equal(0, StringSetSize(set1));

        Buffer *buff = StringSetToBuffer(set1, ',');

        assert_true(buff);
        assert_string_equal(BufferData(buff), "");

        BufferDestroy(buff);
        StringSetDestroy(set1);
    }

    {
        StringSet *set = StringSetNew();
        StringSetAdd(set, xstrdup("foo"));
        StringSetJoin(set, set, xstrdup);

        assert_int_equal(1, StringSetSize(set));

        Buffer *buff = StringSetToBuffer(set, ',');

        assert_true(buff);
        assert_string_equal(BufferData(buff), "foo");

        StringSetDestroy(set);
        BufferDestroy(buff);
    }

    {
        StringSet *set1 = StringSetNew();
        StringSet *set2 = StringSetNew();
        StringSetAdd(set1, xstrdup("foo"));
        StringSetAdd(set2, xstrdup("bar"));
        StringSetJoin(set1, set2, xstrdup);
        StringSetDestroy(set2);

        assert_int_equal(2, StringSetSize(set1));

        Buffer *buff = StringSetToBuffer(set1, ',');
        StringSetDestroy(set1);

        assert_true(buff);
        assert_string_equal(BufferData(buff), "foo,bar");

        BufferDestroy(buff);
    }
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_stringset_from_string),
        unit_test(test_stringset_serialization),
        unit_test(test_stringset_clear),
        unit_test(test_stringset_join)
    };

    return run_tests(tests);
}
