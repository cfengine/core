#include <test.h>

#include <array_map_priv.h>
#include <hash_map_priv.h>
#include <map.h>
#include <string_lib.h>

#include <alloc.h>

static unsigned int ConstHash(ARG_UNUSED const void *key,
                              ARG_UNUSED unsigned int seed,
                              ARG_UNUSED unsigned int max)
{
    return 0;
}

static void test_new_destroy(void)
{
    Map *map = MapNew(NULL, NULL, NULL, NULL);
    assert_int_equal(MapSize(map), 0);
    MapDestroy(map);
}

static void test_insert(void)
{
    StringMap *map = StringMapNew();

    assert_false(StringMapHasKey(map, "one"));
    assert_false(StringMapInsert(map, xstrdup("one"), xstrdup("first")));
    assert_true(StringMapHasKey(map, "one"));
    assert_int_equal(StringMapSize(map), 1);
    assert_true(StringMapInsert(map, xstrdup("one"), xstrdup("duplicate")));
    assert_int_equal(StringMapSize(map), 1);

    assert_false(StringMapHasKey(map, "two"));
    assert_false(StringMapInsert(map, xstrdup("two"), xstrdup("second")));
    assert_true(StringMapHasKey(map, "two"));
    assert_int_equal(StringMapSize(map), 2);

    assert_false(StringMapHasKey(map, "third"));
    assert_false(StringMapInsert(map, xstrdup("third"), xstrdup("first")));
    assert_true(StringMapHasKey(map, "third"));

    assert_true(StringMapInsert(map, xstrdup("third"), xstrdup("stuff")));
    assert_true(StringMapHasKey(map, "third"));
    assert_int_equal(StringMapSize(map), 3);

    StringMapDestroy(map);
}

static char *CharTimes(char c, size_t times)
{
    char *res = xmalloc(times + 1);
    memset(res, c, times);
    res[times] = '\0';
    return res;
}

static StringMap *jumbo_map;

static void test_insert_jumbo(void)
{
    jumbo_map = StringMapNew();

    for (int i = 0; i < 10000; i++)
    {
        /* char *s = CharTimes('a', i); */
        char s[i+1];
        memset(s, 'a', i);
        s[i] = '\0';

        assert_false(StringMapHasKey(jumbo_map, s));
        assert_false(StringMapInsert(jumbo_map, xstrdup(s), xstrdup(s)));
        assert_true(StringMapHasKey(jumbo_map, s));
        /* free(s); */
    }

    StringMapPrintStats(jumbo_map, stdout);
}

static void test_remove(void)
{
    HashMap *hashmap = HashMapNew(ConstHash, (MapKeyEqualFn)StringSafeEqual, free, free);

    HashMapInsert(hashmap, xstrdup("a"), xstrdup("b"));

    MapKeyValue *item = HashMapGet(hashmap, "a");
    assert_string_equal(item->key, "a");
    assert_string_equal(item->value, "b");

    assert_true(HashMapRemove(hashmap, "a"));
    assert_int_equal(HashMapGet(hashmap, "a"), NULL);

    HashMapDestroy(hashmap);
}

static void test_get(void)
{
    StringMap *map = StringMapNew();

    assert_false(StringMapInsert(map, xstrdup("one"), xstrdup("first")));
    assert_string_equal(StringMapGet(map, "one"), "first");
    assert_int_equal(StringMapGet(map, "two"), NULL);

    StringMapDestroy(map);
}

static void test_has_key(void)
{
    StringMap *map = StringMapNew();

    assert_false(StringMapInsert(map, xstrdup("one"), xstrdup("first")));
    assert_true(StringMapHasKey(map, "one"));

    assert_false(StringMapHasKey(map, NULL));
    assert_false(StringMapInsert(map, NULL, xstrdup("null")));
    assert_true(StringMapHasKey(map, NULL));

    StringMapDestroy(map);
}

static void test_clear(void)
{
    StringMap *map = StringMapNew();

    assert_false(StringMapInsert(map, xstrdup("one"), xstrdup("first")));
    assert_true(StringMapHasKey(map, "one"));

    StringMapClear(map);
    assert_false(StringMapHasKey(map, "one"));

    StringMapDestroy(map);
}

static void test_soft_destroy(void)
{
    StringMap *map = StringMapNew();

    char *key = xstrdup("one");
    char *value = xstrdup("first");

    assert_false(StringMapInsert(map, key, value));
    assert_true(StringMapHasKey(map, "one"));
    assert_string_equal(StringMapGet(map, "one"),"first");

    StringMapSoftDestroy(map);

    assert_string_equal("first", value);
    free(value);
}

static void test_iterate_jumbo(void)
{
    size_t size = StringMapSize(jumbo_map);

    MapIterator it = MapIteratorInit(jumbo_map->impl);
    MapKeyValue *item = NULL;
    int count = 0;
    int sum_len = 0;
    while ((item = MapIteratorNext(&it)))
    {
        int key_len = strlen(item->key);
        int value_len = strlen(item->value);
        assert_int_equal(key_len, value_len);

        sum_len += key_len;
        count++;
    }
    assert_int_equal(count, 10000);
    assert_int_equal(count, size);
    assert_int_equal(sum_len, 10000*9999/2);

    StringMapDestroy(jumbo_map);
}

static void test_hashmap_new_destroy(void)
{
    HashMap *hashmap = HashMapNew(NULL, NULL, NULL, NULL);
    HashMapDestroy(hashmap);
}

static void test_hashmap_degenerate_hash_fn(void)
{
    HashMap *hashmap = HashMapNew(ConstHash, (MapKeyEqualFn)StringSafeEqual, free, free);

    for (int i = 0; i < 100; i++)
    {
        assert_false(HashMapInsert(hashmap, CharTimes('a', i), CharTimes('a', i)));
    }

    MapKeyValue *item = HashMapGet(hashmap, "aaaa");
    assert_string_equal(item->key, "aaaa");
    assert_string_equal(item->value, "aaaa");

    HashMapRemove(hashmap, "aaaa");
    assert_int_equal(HashMapGet(hashmap, "aaaa"), NULL);

    HashMapDestroy(hashmap);
}

/* A special struct for *Value* in the Map, that references the Key. */
typedef struct
{
    char *keyref;                                     /* pointer to the key */
    int val;                                          /* arbitrary value */
} TestValue;

/* This tests that in case we insert a pre-existing key, so that the value
 * gets replaced, the key also gets replaced despite being the same so that
 * any references in the new value are not invalid. */
static void test_array_map_key_referenced_in_value(void)
{
    ArrayMap *m = ArrayMapNew((MapKeyEqualFn) StringSafeEqual,
                              free, free);

    char      *key1 = xstrdup("blah");
    TestValue *val1 = xmalloc(sizeof(*val1));
    val1->keyref = key1;
    val1->val    = 1;

    /* Return value of 2 means: new value was inserted. */
    assert_int_equal(ArrayMapInsert(m, key1, val1), 2);

    /* Now we insert the same key, so that it replaces the value. */

    char      *key2 = xstrdup("blah");                   /* same key string */
    TestValue *val2 = xmalloc(sizeof(*val2));
    val2->keyref = key2;
    val2->val    = 2;

    /* Return value of 1 means: key preexisted, old data is replaced. */
    assert_int_equal(ArrayMapInsert(m, key2, val2), 1);

    /* And now the important bit: make sure that both "key" and "val->key" are
     * the same pointer. */
    /* WARNING: key1 and val1 must have been freed, but there is no way to
     *          test that. */
    {
        MapKeyValue *keyval = ArrayMapGet(m, key2);
        assert_true(keyval != NULL);
        char      *key = keyval->key;
        TestValue *val = keyval->value;
        assert_true(val->keyref == key);
        assert_int_equal(val->val, 2);
        /* Valgrind will barf on the next line if the key has freed by
         * mistake. */
        assert_true(strcmp(val->keyref, "blah") == 0);
        /* A bit irrelevant: make sure that using "blah" in the lookup yields
         * the same results, as the string is the same as in key2. */
        MapKeyValue *keyval2 = ArrayMapGet(m, "blah");
        assert_true(keyval2 == keyval);
    }

    ArrayMapDestroy(m);
}

/* Same purpose as the above test. */
static void test_hash_map_key_referenced_in_value(void)
{
    HashMap *m = HashMapNew((MapHashFn) StringHash,
                            (MapKeyEqualFn) StringSafeEqual,
                            free, free);
    char      *key1 = xstrdup("blah");
    TestValue *val1 = xmalloc(sizeof(*val1));
    val1->keyref = key1;
    val1->val    = 1;

    /* Return value false means: new value was inserted. */
    assert_false(HashMapInsert(m, key1, val1));

    /* Now we insert the same key, so that it replaces the value. */

    char      *key2 = xstrdup("blah");                   /* same key string */
    TestValue *val2 = xmalloc(sizeof(*val2));
    val2->keyref = key2;
    val2->val    = 2;

    /* Return value true means: key preexisted, old data is replaced. */
    assert_true(HashMapInsert(m, key2, val2));

    /* And now the important bit: make sure that both "key" and "val->key" are
     * the same pointer. */
    /* WARNING: key1 and val1 must have been freed, but there is no way to
     *          test that. */
    {
        MapKeyValue *keyval = HashMapGet(m, key2);
        assert_true(keyval != NULL);
        char      *key = keyval->key;
        TestValue *val = keyval->value;
        assert_true(val->keyref == key);    /* THIS IS WHAT IT'S ALL ABOUT! */
        assert_int_equal(val->val, 2);
        /* Valgrind will barf on the next line if the key has freed by
         * mistake. */
        assert_true(strcmp(val->keyref, "blah") == 0);
        /* A bit irrelevant: make sure that using "blah" in the lookup yields
         * the same results, as the string is the same as in key2. */
        MapKeyValue *keyval2 = HashMapGet(m, "blah");
        assert_true(keyval2 == keyval);
    }

    HashMapDestroy(m);
}


int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_new_destroy),
        unit_test(test_insert),
        unit_test(test_insert_jumbo),
        unit_test(test_remove),
        unit_test(test_get),
        unit_test(test_has_key),
        unit_test(test_clear),
        unit_test(test_soft_destroy),
        unit_test(test_hashmap_new_destroy),
        unit_test(test_hashmap_degenerate_hash_fn),
        unit_test(test_array_map_key_referenced_in_value),
        unit_test(test_hash_map_key_referenced_in_value),
        unit_test(test_iterate_jumbo),
    };

    return run_tests(tests);
}
