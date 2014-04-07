#include <test.h>
#include <cmockery.h>

#include <strlist.h>


static StrList *SL = NULL;


/* Strings that will be inserted to strlist, listed here in sorted order. */
char *PATH_STRINGS[] =
{
    " ",
    "  ",
    "/",
    "/path",
    "/path/to/file.name",
    "blah",
    "waza"
};
#define PATH_STRINGS_LEN (sizeof(PATH_STRINGS) / sizeof(PATH_STRINGS[0]))

/* Ideally we need the numbers [0..PATH_STRINGS_LEN) in random order in order to
 * insert non-sorted. To make the test reproducible, here is one random
 * order. Feel free to experiment with changing this. */
size_t INSERT_ORDER[PATH_STRINGS_LEN] =
{
    5, 3, 1, 0, 4, 6, 2
};


static void test_init_SL()
{
    SL = calloc(1, sizeof(*SL));

    /* Insert in random order. */
    for (size_t i = 0; i < PATH_STRINGS_LEN; i++)
    {
        size_t ret = StrList_Append(&SL, PATH_STRINGS[ INSERT_ORDER[i] ]);
        assert_int_equal(ret, i);
        assert_string_equal(StrList_At(SL, i), PATH_STRINGS[ INSERT_ORDER[i] ]);
    }

    assert_int_equal(StrList_Len(SL), PATH_STRINGS_LEN);

    StrList_Finalise(&SL);
}

static void test_StrList_Sort()
{
    StrList_Sort(SL, string_Compare);
    assert_int_equal(StrList_Len(SL), PATH_STRINGS_LEN);

    for (size_t i = 0; i < PATH_STRINGS_LEN; i++)
    {
        assert_string_equal(StrList_At(SL, i), PATH_STRINGS[i]);
    }
}

static void test_StrList_BinarySearch()
{
    size_t pos;
    bool found;

    /* Search for existing strings. */
    for (size_t i = 0; i < PATH_STRINGS_LEN; i++)
    {
        found = StrList_BinarySearch(SL, PATH_STRINGS[i], &pos);
        assert_int_equal(found, true);
        assert_int_equal(pos, i);
    }

    /* Search for inexistent entries, check that the returned position is the
     * one they should be inserted into. */

    found = StrList_BinarySearch(SL, "", &pos);
    assert_int_equal(found, false);
    assert_int_equal(pos, 0);      /* empty string should always come first */

    found = StrList_BinarySearch(SL, "   ", &pos);
    assert_int_equal(found, false);
    assert_int_equal(pos, 2);

    found = StrList_BinarySearch(SL, "zzz", &pos);
    assert_int_equal(found, false);
    assert_int_equal(pos, PATH_STRINGS_LEN);

    found = StrList_BinarySearch(SL, "/path/", &pos);
    assert_int_equal(found, false);
    assert_int_equal(pos, 4);

    found = StrList_BinarySearch(SL, "/path/to", &pos);
    assert_int_equal(found, false);
    assert_int_equal(pos, 4);
}

static void test_StrList_SearchLongestPrefix()
{
    /* REMINDER: PATH_STRINGS[] = { " ", "  ", "/", "/path", "/path/to/file.name", "blah", "waza" }; */

    size_t ret, ret2, ret3;

    /* These searches all search for  "/path", since length is the same. */
    ret = StrList_SearchLongestPrefix(SL, "/path", 0, '/', true);
    ret2 = StrList_SearchLongestPrefix(SL, "/path/", 5, '/', true);
    ret3 = StrList_SearchLongestPrefix(SL, "/path/to/file.name", 5, '/', true);
    assert_string_equal(StrList_At(SL, ret), "/path");
    assert_string_equal(StrList_At(SL, ret2), "/path");
    assert_string_equal(StrList_At(SL, ret3), "/path");

    /* Searching for "/path/" does not bring up "/path", but "/", since
     * directories *must* have a trailing slash. */
    ret = StrList_SearchLongestPrefix(SL, "/path/", 0, '/', true);
    assert_string_equal(StrList_At(SL, ret), "/");

    ret = StrList_SearchLongestPrefix(SL, "/path.json", 0, '/', true);
    assert_string_equal(StrList_At(SL, ret), "/");


    /* We insert a couple more directories and sort again. */
    StrList_Append(&SL, "/path/to/file.namewhatever/whatever");
    StrList_Append(&SL, "/path/to/file.name/whatever/");
    StrList_Append(&SL, "/path/to/");
    StrList_Sort(SL, string_Compare);

    ret = StrList_SearchLongestPrefix(SL, "/path/to/file.name",
                                      0, '/', true);
    assert_string_equal(StrList_At(SL, ret), "/path/to/file.name");

    ret = StrList_SearchLongestPrefix(SL, "/path/to/file",
                                      0, '/', true);
    assert_string_equal(StrList_At(SL, ret), "/path/to/");

    ret = StrList_SearchLongestPrefix(SL, "/path/to/file.name/whatever/blah",
                                      0, '/', true);
    assert_string_equal(StrList_At(SL, ret), "/path/to/file.name/whatever/");

    ret = StrList_SearchLongestPrefix(SL, "/path/to/",
                                      0, '/', true);
    assert_string_equal(StrList_At(SL, ret), "/path/to/");
}

static void test_StrList_Free()
{
    StrList_Free(&SL);

    assert_int_equal(SL, NULL);
}


int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_init_SL),
        unit_test(test_StrList_Sort),
        unit_test(test_StrList_BinarySearch),
        unit_test(test_StrList_SearchLongestPrefix),
        unit_test(test_StrList_Free)
    };

    int ret = run_tests(tests);

    return ret;
}
