#include "cf3.defs.h"
#include "cf3.extern.h"

#include <setjmp.h>
#include <cmockery.h>

static void test_prepend_item(void **state)
{
struct Item *ip = NULL, *list = NULL;
ip = PrependItem(&list, "hello", "classes");
assert_int_not_equal(ip, NULL);
assert_int_not_equal(list, NULL);
DeleteItem(&list, ip);
assert_int_equal(list, NULL);
}

static void test_list_len(void **state)
{
struct Item *list = NULL;
PrependItem(&list, "one", "classes");
PrependItem(&list, "two", NULL);
PrependItem(&list, "three", NULL);
assert_int_equal(ListLen(list), 3);
DeleteItemList(list);
assert_int_equal(ListLen(list), 0);
}

int main()
{
const UnitTest tests[] =
   {
   unit_test(test_prepend_item),
   /* unit_test(test_list_len), */
   };

return run_tests(tests);
}

