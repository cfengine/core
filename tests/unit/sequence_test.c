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

static int CompareInts(const void *a, const void *b)
{
   int **x = a;
   int **y = b;
   return **x - **y;
}

static void test_sort(void **state)
{
Sequence *seq = SequenceCreate(5);

int one = 1;
int two = 2;
int three = 3;
int four = 4;
int five = 5;

SequenceAppend(seq, &three);
SequenceAppend(seq, &two);
SequenceAppend(seq, &five);
SequenceAppend(seq, &one);
SequenceAppend(seq, &four);

SequenceSort(seq, CompareInts);

assert_int_equal(seq->data[0], &one);
assert_int_equal(seq->data[1], &two);
assert_int_equal(seq->data[2], &three);
assert_int_equal(seq->data[3], &four);
assert_int_equal(seq->data[4], &five);
}

int main()
{
const UnitTest tests[] =
   {
   unit_test(test_create_destroy),
   unit_test(test_append),
   unit_test(test_sort)
   };

return run_tests(tests);
}
