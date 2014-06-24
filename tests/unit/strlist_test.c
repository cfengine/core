#include <test.h>

#include <cmockery.h>
#include <strlist.h>


static StrList *PATH_SL, *HOSTNAME_SL;



/* Strings that will be inserted to strlist, listed here in sorted order. */
char *PATH_STRINGS[] =
{
    " ",
    "  ",
    "/",
    "/path",
    "/path/to/file.name",
    "blah",
    "waza",
};
#define PATH_STRINGS_LEN (sizeof(PATH_STRINGS) / sizeof(PATH_STRINGS[0]))

/* Ideally we need the numbers [0..PATH_STRINGS_LEN) in random order in order to
 * insert non-sorted. To make the test reproducible, here is one random
 * order. Feel free to experiment with changing this. */
size_t PATH_INSERT_ORDER[PATH_STRINGS_LEN] =
{
    5, 3, 1, 0, 4, 6, 2
};



/* Strings that will be inserted to strlist, listed here in special sorted
 * order in the way they should end up after calling
 * StrList_Sort(string_CompareFromEnd). */
char *HOSTNAME_STRINGS[] =
{
    "*",                              /* Globs have no special meaning */
    ".*",                             /* Should not match anything */
    ".",                              /* Should not match as well */
    "com",                            /* No match, nobody has "com" fqdn */
    "cfengine.com",                   /* Match this hostname */
    ".allowed.cfengine.com",          /* Allow everything under this domain */
    "www.cfengine.com",               /* Match this hostname */
    ".no",                            /* Allow all norwegian hostnames */
};
#define HOSTNAME_STRINGS_LEN (sizeof(HOSTNAME_STRINGS) / sizeof(HOSTNAME_STRINGS[0]))

/* Ideally we need the numbers [0..PATH_STRINGS_LEN) in random order in order to
 * insert non-sorted. To make the test reproducible, here is one random
 * order. Feel free to experiment with changing this. */
size_t HOSTNAME_INSERT_ORDER[HOSTNAME_STRINGS_LEN] =
{
    5, 3, 1, 0, 4, 6, 2, 7
};



static StrList *init_strlist(char ** strings, size_t strings_len,
                             size_t *insert_order)
{
    StrList *sl = calloc(1, sizeof(*sl));
    /* Insert in random order. */
    for (size_t i = 0; i < strings_len; i++)
    {
        size_t ret = StrList_Append(&sl, strings[ insert_order[i] ]);
        assert_int_equal(ret, i);
        assert_string_equal(StrList_At(sl, i), strings[ insert_order[i] ]);
    }
    assert_int_equal(StrList_Len(sl), strings_len);
    StrList_Finalise(&sl);

    return sl;
}
static void test_init_SL()
{
    PATH_SL     = init_strlist(PATH_STRINGS, PATH_STRINGS_LEN,
                       PATH_INSERT_ORDER);
    HOSTNAME_SL = init_strlist(HOSTNAME_STRINGS, HOSTNAME_STRINGS_LEN,
                       HOSTNAME_INSERT_ORDER);
}

/* Sort PATH_STRLIST using the common way, and HOSTNAME_STRLIST in the order
 * of reading the strings backwards. */
static void test_StrList_Sort()
{
    StrList_Sort(PATH_SL, string_Compare);
    assert_int_equal(StrList_Len(PATH_SL), PATH_STRINGS_LEN);

    for (size_t i = 0; i < PATH_STRINGS_LEN; i++)
    {
        assert_string_equal(StrList_At(PATH_SL, i),
                            PATH_STRINGS[i]);
    }

    StrList_Sort(HOSTNAME_SL, string_CompareFromEnd);
    assert_int_equal(StrList_Len(HOSTNAME_SL), HOSTNAME_STRINGS_LEN);

    for (size_t i = 0; i < HOSTNAME_STRINGS_LEN; i++)
    {
        assert_string_equal(StrList_At(HOSTNAME_SL, i),
                            HOSTNAME_STRINGS[i]);
    }
}

/* Only search in PATH_STRLIST which is sorted in the common way. */
static void test_StrList_BinarySearch()
{
    size_t pos;
    bool found;

    /* Search for existing strings. */
    for (size_t i = 0; i < PATH_STRINGS_LEN; i++)
    {
        found = StrList_BinarySearch(PATH_SL, PATH_STRINGS[i], &pos);
        assert_int_equal(found, true);
        assert_int_equal(pos, i);
    }

    /* Search for inexistent entries, check that the returned position is the
     * one they should be inserted into. */

    found = StrList_BinarySearch(PATH_SL, "", &pos);
    assert_int_equal(found, false);
    assert_int_equal(pos, 0);      /* empty string should always come first */

    found = StrList_BinarySearch(PATH_SL, "   ", &pos);
    assert_int_equal(found, false);
    assert_int_equal(pos, 2);

    found = StrList_BinarySearch(PATH_SL, "zzz", &pos);
    assert_int_equal(found, false);
    assert_int_equal(pos, PATH_STRINGS_LEN);

    found = StrList_BinarySearch(PATH_SL, "/path/", &pos);
    assert_int_equal(found, false);
    assert_int_equal(pos, 4);

    found = StrList_BinarySearch(PATH_SL, "/path/to", &pos);
    assert_int_equal(found, false);
    assert_int_equal(pos, 4);
}

/* Only search in PATH_STRLIST because it makes sense to search longest prefix
 * for paths. */
static void test_StrList_SearchLongestPrefix()
{
    /* REMINDER: PATH_STRINGS[] =
       { " ", "  ", "/", "/path", "/path/to/file.name", "blah", "waza" }; */

    size_t ret, ret2, ret3;

    /* These searches all search for  "/path", since length is the same. */
    ret  = StrList_SearchLongestPrefix(PATH_SL, "/path", 0, '/', true);
    ret2 = StrList_SearchLongestPrefix(PATH_SL, "/path/", 5, '/', true);
    ret3 = StrList_SearchLongestPrefix(PATH_SL, "/path/to/file.name", 5, '/', true);
    assert_string_equal(StrList_At(PATH_SL, ret), "/path");
    assert_string_equal(StrList_At(PATH_SL, ret2), "/path");
    assert_string_equal(StrList_At(PATH_SL, ret3), "/path");

    /* Searching for "/path/" does not bring up "/path", but "/", since
     * directories *must* have a trailing slash. */
    ret = StrList_SearchLongestPrefix(PATH_SL, "/path/", 0, '/', true);
    assert_string_equal(StrList_At(PATH_SL, ret), "/");

    ret = StrList_SearchLongestPrefix(PATH_SL, "/path.json", 0, '/', true);
    assert_string_equal(StrList_At(PATH_SL, ret), "/");


    /* We insert a couple more directories and sort again. */
    StrList_Append(&PATH_SL, "/path/to/file.namewhatever/whatever");
    StrList_Append(&PATH_SL, "/path/to/file.name/whatever/");
    StrList_Append(&PATH_SL, "/path/to/");
    StrList_Sort(PATH_SL, string_Compare);

    ret = StrList_SearchLongestPrefix(PATH_SL, "/path/to/file.name",
                                      0, '/', true);
    assert_string_equal(StrList_At(PATH_SL, ret), "/path/to/file.name");

    ret = StrList_SearchLongestPrefix(PATH_SL, "/path/to/file",
                                      0, '/', true);
    assert_string_equal(StrList_At(PATH_SL, ret), "/path/to/");

    ret = StrList_SearchLongestPrefix(PATH_SL, "/path/to/file.name/whatever/blah",
                                      0, '/', true);
    assert_string_equal(StrList_At(PATH_SL, ret), "/path/to/file.name/whatever/");

    ret = StrList_SearchLongestPrefix(PATH_SL, "/path/to/",
                                      0, '/', true);
    assert_string_equal(StrList_At(PATH_SL, ret), "/path/to/");
}

/* Only search in HOSTNAME_STRLIST because it only makes sense to search for
 * longest suffix with hostnames and subdomains.  */
static void test_StrList_SearchLongestSuffix()
{
    /* REMINDER: HOSTNAME_STRINGS[] =
       {  "*", ".*", ".", "com", "cfengine.com", ".allowed.cfengine.com", "www.cfengine.com", ".no" }; */

    size_t ret, ret2, ret3, ret4, ret5, ret6, ret7, ret8, ret9, ret10, ret11;

    ret  = StrList_SearchLongestPrefix(HOSTNAME_SL, "cfengine.com", 0, '.', false);
    ret2 = StrList_SearchLongestPrefix(HOSTNAME_SL, "google.com", 0, '.', false);
    ret3 = StrList_SearchLongestPrefix(HOSTNAME_SL, "yr.no", 0, '.', false);
    ret4 = StrList_SearchLongestPrefix(HOSTNAME_SL, "ntua.gr", 0, '.', false);
    ret5 = StrList_SearchLongestPrefix(HOSTNAME_SL, "disallowed.cfengine.com", 0, '.', false);
    ret6 = StrList_SearchLongestPrefix(HOSTNAME_SL, "allowed.cfengine.com", 0, '.', false);
    ret7 = StrList_SearchLongestPrefix(HOSTNAME_SL, "blah.allowed.cfengine.com", 0, '.', false);
    ret8 = StrList_SearchLongestPrefix(HOSTNAME_SL, "www.cfengine.com", 0, '.', false);
    ret9 = StrList_SearchLongestPrefix(HOSTNAME_SL, "www1.cfengine.com", 0, '.', false);
    ret10 = StrList_SearchLongestPrefix(HOSTNAME_SL, "1www.cfengine.com", 0, '.', false);
    ret11 = StrList_SearchLongestPrefix(HOSTNAME_SL, "no", 0, '.', false);

    assert_string_equal(StrList_At(HOSTNAME_SL, ret), "cfengine.com");
    assert_int_equal(ret2, (size_t) -1);
    assert_string_equal(StrList_At(HOSTNAME_SL, ret3), ".no");
    assert_int_equal(ret4, (size_t) -1);
    assert_int_equal(ret5, (size_t) -1);
    assert_int_equal(ret6, (size_t) -1);
    assert_string_equal(StrList_At(HOSTNAME_SL, ret7), ".allowed.cfengine.com");
    assert_string_equal(StrList_At(HOSTNAME_SL, ret8), "www.cfengine.com");
    assert_int_equal(ret9, (size_t) -1);
    assert_int_equal(ret10, (size_t) -1);
    assert_int_equal(ret11, (size_t) -1);
}

static void test_StrList_SearchForSuffix()
{
    /* REMINDER: HOSTNAME_STRINGS[] =
       {  "*", ".*", ".", "com", "cfengine.com", ".allowed.cfengine.com", "www.cfengine.com", ".no" }; */

    size_t ret, ret2, ret3, ret4, ret5, ret6, ret7, ret8, ret9, ret10, ret11;

    ret  = StrList_SearchForPrefix(HOSTNAME_SL, "cfengine.com", 0, false);
    ret2 = StrList_SearchForPrefix(HOSTNAME_SL, "google.com", 0, false);
    ret3 = StrList_SearchForPrefix(HOSTNAME_SL, "yr.no", 0, false);
    ret4 = StrList_SearchForPrefix(HOSTNAME_SL, "ntua.gr", 0, false);
    ret5 = StrList_SearchForPrefix(HOSTNAME_SL, "disallowed.cfengine.com", 0, false);
    ret6 = StrList_SearchForPrefix(HOSTNAME_SL, "allowed.cfengine.com", 0, false);
    ret7 = StrList_SearchForPrefix(HOSTNAME_SL, "blah.allowed.cfengine.com", 0, false);
    ret8 = StrList_SearchForPrefix(HOSTNAME_SL, "www.cfengine.com", 0, false);
    ret9 = StrList_SearchForPrefix(HOSTNAME_SL, "www1.cfengine.com", 0, false);
    ret10 = StrList_SearchForPrefix(HOSTNAME_SL, "1www.cfengine.com", 0, false);
    ret11 = StrList_SearchForPrefix(HOSTNAME_SL, "no", 0, false);

    /* SearchForPrefix() does not guarantee which one of all the matches it
     * will return if there are many matches. */

    /* E.g. the first search might return "cfengine.com" or "com" entry. */
    LargestIntegralType set[] = {3, 4};
    assert_in_set(ret, set, 2);
    assert_string_equal(StrList_At(HOSTNAME_SL, ret2),  "com");
    assert_string_equal(StrList_At(HOSTNAME_SL, ret3),  ".no");
    assert_int_equal(ret4,  (size_t) -1);
    assert_in_set(ret5, set, 2);
    assert_in_set(ret6, set, 2);
    LargestIntegralType set2[] = {3, 4, 5};
    assert_in_set(ret7, set2, 3);
    assert_in_set(ret8, set, 2);
    assert_in_set(ret9, set, 2);
    LargestIntegralType set3[] = {3, 4, 6};
    assert_in_set(ret10, set3, 3);
    assert_int_equal(ret11, (size_t) -1);
}

static void test_StrList_Free()
{
    StrList_Free(&PATH_SL);
    assert_int_equal(PATH_SL, NULL);

    StrList_Free(&HOSTNAME_SL);
    assert_int_equal(HOSTNAME_SL, NULL);
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
        unit_test(test_StrList_SearchLongestSuffix),
        unit_test(test_StrList_SearchForSuffix),
        unit_test(test_StrList_Free)
    };

    int ret = run_tests(tests);

    return ret;
}
