#include "test.h"
#include "sequence.h"


static void test_create_destroy(void **state)
{
Sequence *seq = SequenceCreate(5);
SequenceDestroy(&seq, NULL);
assert_int_equal(seq, NULL);
}

static void test_append(void **state)
{
Sequence *seq = SequenceCreate(2);

for (size_t i = 0; i < 1000; i++)
   {
   SequenceAppend(seq, strdup("snookie"));
   }

assert_int_equal(seq->length, 1000);

for (size_t i = 0; i < 1000; i++)
   {
   assert_string_equal(seq->data[i], "snookie");
   }

SequenceDestroy(&seq, free);
}

int main()
{
const UnitTest tests[] =
   {
   unit_test(test_create_destroy),
   unit_test(test_append),
   };

return run_tests(tests);
}
