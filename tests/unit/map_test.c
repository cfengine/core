#include "platform.h"

#include "test.h"
#include "array_map_priv.h"
#include "hash_map_priv.h"
#include "map.h"
#include "string_map.h"
#include "string_lib.h"

#include "alloc.h"

static unsigned int ConstHash(const void *key)
{
    return 0;
}

static void test_new_destroy(void **state)
{
    Map *map = MapNew(NULL, NULL, NULL, NULL);
    MapDestroy(map);
}

static void test_insert(void **state)
{
    StringMap *map = StringMapNew();

    assert_false(StringMapHasKey(map, "one"));
    StringMapInsert(map, xstrdup("one"), xstrdup("first"));
    assert_true(StringMapHasKey(map, "one"));

    assert_false(StringMapHasKey(map, "two"));
    StringMapInsert(map, xstrdup("two"), xstrdup("second"));
    assert_true(StringMapHasKey(map, "two"));

    assert_false(StringMapHasKey(map, "third"));
    StringMapInsert(map, xstrdup("third"), xstrdup("first"));
    assert_true(StringMapHasKey(map, "third"));

    StringMapInsert(map, xstrdup("third"), xstrdup("stuff"));
    assert_true(StringMapHasKey(map, "third"));

    StringMapDestroy(map);
}

static char *StringTimes(char *str, size_t times)
{
    size_t len = strlen(str);
    char *res = xmalloc((sizeof(char) * len * times) + 1);
    for (size_t i = 0; i < (len * times); i += len)
    {
        memcpy(res + i, str, len);
    }
    res[len*times] = '\0';
    return res;
}

static void test_insert_jumbo(void **state)
{
    StringMap *map = StringMapNew();
    for (int i = 0; i < 10000; i++)
    {
        char *s = StringTimes("a", i);
        assert_false(StringMapHasKey(map, s));
        StringMapInsert(map, StringTimes("a", i), NULL);
        assert_true(StringMapHasKey(map, s));
        free(s);
    }
    StringMapDestroy(map);
}

static void test_remove(void **state)
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

static void test_get(void **state)
{
    StringMap *map = StringMapNew();

    StringMapInsert(map, xstrdup("one"), xstrdup("first"));
    assert_string_equal(StringMapGet(map, "one"), "first");
    assert_int_equal(StringMapGet(map, "two"), NULL);

    StringMapDestroy(map);
}

static void test_has_key(void **state)
{
    StringMap *map = StringMapNew();

    StringMapInsert(map, xstrdup("one"), xstrdup("first"));
    assert_true(StringMapHasKey(map, "one"));

    assert_false(StringMapHasKey(map, NULL));
    StringMapInsert(map, NULL, xstrdup("null"));
    assert_true(StringMapHasKey(map, NULL));

    StringMapDestroy(map);
}

static void test_clear(void **state)
{
    StringMap *map = StringMapNew();

    StringMapInsert(map, xstrdup("one"), xstrdup("first"));
    assert_true(StringMapHasKey(map, "one"));

    StringMapClear(map);
    assert_false(StringMapHasKey(map, "one"));

    StringMapDestroy(map);
}

static void test_iterate(void **state)
{
    StringMap *map = StringMapNew();

    for (int i = 0; i < 10000; i++)
    {
        char *s = StringTimes("a", i);
        assert_false(StringMapHasKey(map, s));
        StringMapInsert(map, xstrdup(s), xstrdup(s));
        assert_true(StringMapHasKey(map, s));
        free(s);
    }

    MapIterator it = MapIteratorInit(map->impl);
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
    assert_int_equal(sum_len, 10000*9999/2);

    StringMapDestroy(map);
}

static void test_hashmap_new_destroy(void **state)
{
    HashMap *hashmap = HashMapNew(NULL, NULL, NULL, NULL);
    HashMapDestroy(hashmap);
}

static void test_hashmap_degenerate_hash_fn(void **state)
{
    HashMap *hashmap = HashMapNew(ConstHash, (MapKeyEqualFn)StringSafeEqual, free, free);

    for (int i = 0; i < 100; i++)
    {
        HashMapInsert(hashmap, StringTimes("a", i), StringTimes("a", i));
    }

    MapKeyValue *item = HashMapGet(hashmap, "aaaa");
    assert_string_equal(item->key, "aaaa");
    assert_string_equal(item->value, "aaaa");

    HashMapRemove(hashmap, "aaaa");
    assert_int_equal(HashMapGet(hashmap, "aaaa"), NULL);

    HashMapDestroy(hashmap);
}

int main()
{
    const UnitTest tests[] =
    {
        unit_test(test_new_destroy),
        unit_test(test_insert),
        unit_test(test_insert_jumbo),
        unit_test(test_remove),
        unit_test(test_get),
        unit_test(test_has_key),
        unit_test(test_clear),
        unit_test(test_iterate),
        unit_test(test_hashmap_new_destroy),
        unit_test(test_hashmap_degenerate_hash_fn),
    };

    return run_tests(tests);
}
