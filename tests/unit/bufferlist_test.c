#include "test.h"
#include <string.h>
#include <stdbool.h>
#include "bufferlist.h"

// Simple initialization test
static void test_initList(void)
{
    BufferList *list = NULL;
    list = BufferListNew();
    assert_true(list != NULL);
    // We shouldn't use this function yet, but otherwise we leak memory
    assert_int_equal(BufferListDestroy(&list), 0);
}

static void test_destroyer(void)
{
    BufferList *list = NULL;
    list = BufferListNew();
    assert_true(list != NULL);

    char *element0 = strdup("this is a test string");
    Buffer *buffer0 = BufferNewFrom(element0, strlen(element0));
    char *element1 = strdup("another test string");
    Buffer *buffer1 = BufferNewFrom(element1, strlen(element1));
    char *element2 = strdup("yet another test string");
    Buffer *buffer2 = BufferNewFrom(element2, strlen(element2));
    char *element3 = strdup("and one more test string");
    Buffer *buffer3 = BufferNewFrom(element3, strlen(element3));

    // We add element0 to the list.
    assert_int_equal(BufferListPrepend(list, buffer0), 0);
    // We add element1 to the list.
    assert_int_equal(BufferListPrepend(list, buffer1), 0);
    // We add element2 to the list.
    assert_int_equal(BufferListPrepend(list, buffer2), 0);
    // We add element3 to the list.
    assert_int_equal(BufferListPrepend(list, buffer3), 0);

    // Now we try to destroy the list.
    assert_int_equal(BufferListDestroy(&list), 0);
    free (element0);
    free (element1);
    free (element2);
    free (element3);
}

static void test_prependToList(void)
{
    BufferList *list = NULL;
    list = BufferListNew();
    assert_true(list != NULL);

    char *element0 = strdup("this is a test string");
    Buffer *buffer0 = BufferNewFrom(element0, strlen(element0));
    char *element1 = strdup("another test string");
    Buffer *buffer1 = BufferNewFrom(element1, strlen(element1));

    // We add element0 to the list.
    assert_int_equal(BufferListPrepend(list, buffer0), 0);
    assert_int_equal(BufferListCount(list), 1);

    // We add element1 to the list.
    assert_int_equal(BufferListPrepend(list, buffer1), 0);
    assert_int_equal(BufferListCount(list), 2);

    assert_int_equal(BufferListDestroy(&list), 0);
    free (element0);
    free (element1);
}

static void test_appendToList(void)
{
    BufferList *list = NULL;
    list = BufferListNew();
    assert_true(list != NULL);

    char *element0 = strdup("this is a test string");
    Buffer *buffer0 = BufferNewFrom(element0, strlen(element0));
    char *element1 = strdup("another test string");
    Buffer *buffer1 = BufferNewFrom(element1, strlen(element1));

    // We add element0 to the list.
    assert_int_equal(BufferListAppend(list, buffer0), 0);
    assert_int_equal(BufferListCount(list), 1);
										    
    // We add element1 to the list.
    assert_int_equal(BufferListAppend(list, buffer1), 0);
    assert_int_equal(BufferListCount(list), 2);
													    
    assert_int_equal(BufferListDestroy(&list), 0);
    free (element0);
    free (element1);
}

static void test_removeFromList(void)
{
    BufferList *list = NULL;
    list = BufferListNew();
    assert_true(list != NULL);

    char *element0 = strdup("this is a test string");
    Buffer *buffer0 = BufferNewFrom(element0, strlen(element0));
    char *element1 = strdup("another test string");
    Buffer *buffer1 = BufferNewFrom(element1, strlen(element1));
    char *element2 = strdup("yet another test string");
    Buffer *buffer2 = BufferNewFrom(element2, strlen(element2));
    char *element3 = strdup("and one more test string");
    Buffer *buffer3 = BufferNewFrom(element3, strlen(element3));
    char *element4 = strdup("non existing element");
    Buffer *buffer4 = BufferNewFrom(element4, strlen(element4));

    // We add element0 to the list.
    assert_int_equal(BufferListPrepend(list, buffer0), 0);
    assert_int_equal(BufferListCount(list), 1);

    // We add element1 to the list.
    assert_int_equal(BufferListPrepend(list, buffer1), 0);
    assert_int_equal(BufferListCount(list), 2);
		
    // We add element2 to the list.
    assert_int_equal(BufferListPrepend(list, buffer2), 0);
    assert_int_equal(BufferListCount(list), 3);

    // We add element3 to the list.
    assert_int_equal(BufferListPrepend(list, buffer3), 0);
    assert_int_equal(BufferListCount(list), 4);

    // We remove the non existing element
    assert_int_equal(BufferListRemove(list, buffer4), -1);
    assert_int_equal(BufferListCount(list), 4);

    // Remove element1 which is in the middle of the list
    assert_int_equal(BufferListRemove(list, buffer1), 0);
    assert_int_equal(BufferListCount(list), 3);

    // Remove element3 which is at the beginning of the list
    assert_int_equal(BufferListRemove(list, buffer3), 0);
    assert_int_equal(BufferListCount(list), 2);

    // Remove element0 which is at the end of the list
    assert_int_equal(BufferListRemove(list, buffer0), 0);
    assert_int_equal(BufferListCount(list), 1);

    // Remove element2 which is the only element on the list
    assert_int_equal(BufferListRemove(list, buffer2), 0);
    assert_int_equal(BufferListCount(list), 0);

    // Now we destroy the list.
    assert_int_equal(BufferListDestroy(&list), 0);
    BufferDestroy(&buffer4);
    free (element0);
    free (element1);
    free (element2);
    free (element3);
    free (element4);
}

static void test_destroyList(void)
{
    BufferList *list = NULL;
    list = BufferListNew(NULL, NULL, NULL);
    assert_true(list != NULL);
    assert_int_equal(BufferListCount(list), 0);

    // Now we destroy the list
    assert_int_equal(BufferListDestroy(&list), 0);
    assert_int_equal(list, NULL);
    assert_int_equal(BufferListDestroy(NULL), 0);
}

static void test_copyList(void)
{
    BufferList *list1 = NULL;
    BufferList *list2 = NULL;
    BufferList *list3 = NULL;
    BufferList *list4 = NULL;
    char *element0 = strdup("this is a test string");
    Buffer *buffer0 = BufferNewFrom(element0, strlen(element0));
    char *element1 = strdup("another test string");
    Buffer *buffer1 = BufferNewFrom(element1, strlen(element1));
    char *element2 = strdup("yet another test string");
    Buffer *buffer2 = BufferNewFrom(element2, strlen(element2));

    list1 = BufferListNew();
    assert_true(list1 != NULL);
    assert_int_equal(0, BufferListPrepend(list1, buffer0));
    assert_int_equal(1, BufferListCount(list1));
    /*
     * Copy the list1 to list2 and prepend one more element
     */
    assert_int_equal(0, BufferListCopy(list1, &list2));
    assert_int_equal(1, BufferListCount(list2));
    assert_int_equal(0, BufferListPrepend(list2, buffer1));
    /*
     * The two lists have detached now.
     */
    assert_int_equal(1, BufferListCount(list1));
    assert_int_equal(2, BufferListCount(list2));
    /*
     * Add one more element to list1 and then attach list3 and list4.
     * Finally detach list4 by removing one element.
     */
    assert_int_equal(0, BufferListPrepend(list1, buffer2));
    assert_int_equal(0, BufferListCopy(list1, &list3));
    assert_int_equal(0, BufferListCopy(list1, &list4));
    assert_int_equal(2, BufferListCount(list1));
    assert_int_equal(2, BufferListCount(list3));
    assert_int_equal(2, BufferListCount(list4));
    assert_int_equal(0, BufferListRemove(list4, buffer0));
    assert_int_equal(2, BufferListCount(list1));
    assert_int_equal(2, BufferListCount(list3));
    assert_int_equal(1, BufferListCount(list4));

    BufferListDestroy(&list1);
    BufferListDestroy(&list2);
    BufferListDestroy(&list3);
    BufferListDestroy(&list4);
    free (element0);
    free (element1);
    free (element2);
}

static void test_iterator(void)
{
    BufferList *list = NULL;
    list = BufferListNew();
    assert_true(list != NULL);

    BufferListIterator *emptyListIterator = NULL;
    emptyListIterator = BufferListIteratorGet(list);
    assert_true(emptyListIterator == NULL);
    char *element0 = strdup("this is a test string");
    Buffer *buffer0 = BufferNewFrom(element0, strlen(element0));
    char *element1 = strdup("another test string");
    Buffer *buffer1 = BufferNewFrom(element1, strlen(element1));
    char *element2 = strdup("yet another test string");
    Buffer *buffer2 = BufferNewFrom(element2, strlen(element2));
    char *element3 = strdup("and one more test string");
    Buffer *buffer3 = BufferNewFrom(element3, strlen(element3));

    // We add elements to the list.
    assert_int_equal(BufferListPrepend(list, buffer0), 0);
    assert_int_equal(BufferListPrepend(list, buffer1), 0);
    assert_int_equal(BufferListPrepend(list, buffer2), 0);
    assert_int_equal(BufferListPrepend(list, buffer3), 0);

    BufferListIterator *iterator0 = NULL;
    iterator0 = BufferListIteratorGet(list);
    // Check the iterator
    assert_true(iterator0 != NULL);

    // Remove element1 which is in the middle of the list, this will invalidate the iterator
    assert_int_equal(BufferListRemove(list, buffer1), 0);
    // Check that the iterator is not valid by trying to advance it
    assert_int_equal(BufferListIteratorNext(iterator0), -1);
    // Destroy the iterator
    assert_int_equal(BufferListIteratorDestroy(&iterator0), 0);
    assert_int_equal(iterator0, NULL);

    // Create a new iterator and move it
    BufferListIterator *iterator1 = NULL;
    iterator1 = BufferListIteratorGet(list);
    // Check the iterator
    assert_int_not_equal(iterator1, NULL);
    Buffer *value = NULL;
    value = BufferListIteratorData(iterator1);
    assert_int_equal(BufferCompare(value, buffer3), 0);

    // Advance it
    assert_int_equal(BufferListIteratorNext(iterator1), 0);
    // Check the value, it should be equal to element2
    value = BufferListIteratorData(iterator1);
    assert_int_equal(BufferCompare(value, buffer2), 0);

    // Advance it, now we are at the last element
    assert_int_equal(BufferListIteratorNext(iterator1), 0);
    // Check the value, it should be equal to element0
    value = BufferListIteratorData(iterator1);
    assert_int_equal(BufferCompare(value, buffer0), 0);

    // Advance it, should fail and the iterator should stay where it was
    assert_int_equal(BufferListIteratorNext(iterator1), -1);
    // Check the value, it should be equal to element0
    value = BufferListIteratorData(iterator1);
    assert_int_equal(BufferCompare(value, buffer0), 0);

    // Go back
    assert_int_equal(BufferListIteratorPrevious(iterator1), 0);
    // Check the value, it should be equal to element2
    value = BufferListIteratorData(iterator1);
    assert_int_equal(BufferCompare(value, buffer2), 0);

    // Go back, now we are at the beginning of the list
    assert_int_equal(BufferListIteratorPrevious(iterator1), 0);
    // Check the value, it should be equal to element3
    value = BufferListIteratorData(iterator1);
    assert_int_equal(BufferCompare(value, buffer3), 0);

    // Go back, should fail and the iterator should stay where it was
    assert_int_equal(BufferListIteratorPrevious(iterator1), -1);
    // Check the value, it should be equal to element3
    value = BufferListIteratorData(iterator1);
    assert_int_equal(BufferCompare(value, buffer3), 0);

    // Jump to the last element
    assert_int_equal(BufferListIteratorLast(iterator1), 0);
    // Check the value, it should be equal to element0
    value = BufferListIteratorData(iterator1);
    assert_int_equal(BufferCompare(value, buffer0), 0);

    // Go back
    assert_true(BufferListIteratorHasPrevious(iterator1));
    assert_int_equal(BufferListIteratorPrevious(iterator1), 0);
    // Check the value, it should be equal to element2
    value = BufferListIteratorData(iterator1);
    assert_int_equal(BufferCompare(value, buffer2), 0);

    // Jump to the first element
    assert_int_equal(BufferListIteratorFirst(iterator1), 0);
    // Check the value, it should be equal to element3
    value = BufferListIteratorData(iterator1);
    assert_int_equal(BufferCompare(value, buffer3), 0);

    // Advance it
    assert_true(BufferListIteratorHasNext(iterator1));
    assert_int_equal(BufferListIteratorNext(iterator1), 0);
    // Check the value, it should be equal to element2
    value = BufferListIteratorData(iterator1);
    assert_int_equal(BufferCompare(value, buffer2), 0);

    // Remove the elements
    assert_int_equal(BufferListRemove(list, buffer3), 0);
    assert_int_equal(BufferListRemove(list, buffer0), 0);
    assert_int_equal(BufferListRemove(list, buffer2), 0);

    // Now we destroy the list.
    assert_int_equal(BufferListDestroy(&list), 0);
    assert_int_equal(BufferListIteratorDestroy(&iterator1), 0);
    free (element0);
    free (element1);
    free (element2);
    free (element3);
}

static void test_mutableIterator(void)
{
    BufferList *list = NULL;
    list = BufferListNew();
    assert_true(list != NULL);

    BufferListMutableIterator *emptyListIterator = NULL;
    emptyListIterator = BufferListMutableIteratorGet(list);
    assert_true(emptyListIterator == NULL);
    char *element0 = strdup("this is a test string");
    Buffer *buffer0 = BufferNewFrom(element0, strlen(element0));
    char *element1 = strdup("another test string");
    Buffer *buffer1 = BufferNewFrom(element1, strlen(element1));
    char *element2 = strdup("yet another test string");
    Buffer *buffer2 = BufferNewFrom(element2, strlen(element2));
    char *element3 = strdup("and one more test string");
    Buffer *buffer3 = BufferNewFrom(element3, strlen(element3));
    char *element4 = strdup("prepended by iterator");
    Buffer *buffer4 = BufferNewFrom(element4, strlen(element4));
    char *element5 = strdup("appended by iterator");
    Buffer *buffer5 = BufferNewFrom(element5, strlen(element5));
    char *element6 = strdup("appended by iterator, second");
    Buffer *buffer6 = BufferNewFrom(element6, strlen(element6));
	char *element7 = strdup("prepended by iterator, second");
    Buffer *buffer7 = BufferNewFrom(element7, strlen(element7));

    // We add element0 to the list.
    assert_int_equal(BufferListAppend(list, buffer0), 0);
    // We add element1 to the list.
    assert_int_equal(BufferListAppend(list, buffer1), 0);
    // We add element2 to the list.
    assert_int_equal(BufferListAppend(list, buffer2), 0);
    // We add element3 to the list.
    assert_int_equal(BufferListAppend(list, buffer3), 0);

    // We use a light iterator to check that is valid
    BufferListIterator *lightIterator = NULL;
    lightIterator = BufferListIteratorGet(list);
    BufferListMutableIterator *iterator = NULL;
    BufferListMutableIterator *secondIterator = NULL;
    iterator = BufferListMutableIteratorGet(list);
    assert_true(iterator != NULL);
    secondIterator = BufferListMutableIteratorGet(list);
    assert_true(secondIterator == NULL);
    // Loop through the list until we get to the last element and then back
    while (BufferListMutableIteratorHasNext(iterator))
    {
        assert_int_equal(0, BufferListMutableIteratorNext(iterator));
    }
    assert_int_equal(-1, BufferListMutableIteratorNext(iterator));
    // and back
    while (BufferListMutableIteratorHasPrevious(iterator))
    {
        assert_int_equal(0, BufferListMutableIteratorPrevious(iterator));
    }
    assert_int_equal(-1, BufferListMutableIteratorPrevious(iterator));
    // Jump to the last element
    assert_int_equal(0, BufferListMutableIteratorLast(iterator));
    // and back to the first element
    assert_int_equal(0, BufferListMutableIteratorFirst(iterator));
    // Prepend one element at the beginning of the list
    assert_int_equal(0, BufferListMutableIteratorPrepend(iterator, buffer4));
    assert_int_equal(5, BufferListCount(list));
    // It should be possible to go back one element now.
    assert_int_equal(0, BufferListMutableIteratorPrevious(iterator));
    // Append one element after the first element
    assert_int_equal(0, BufferListMutableIteratorAppend(iterator, buffer5));
    assert_int_equal(6, BufferListCount(list));
    // Loop through the list until we get to the last element and then back
    while (BufferListMutableIteratorHasNext(iterator))
    {
        assert_int_equal(0, BufferListMutableIteratorNext(iterator));
    }
    assert_int_equal(-1, BufferListMutableIteratorNext(iterator));
    // and back
    while (BufferListMutableIteratorHasPrevious(iterator))
    {
        assert_int_equal(0, BufferListMutableIteratorPrevious(iterator));
    }
    assert_int_equal(-1, BufferListMutableIteratorPrevious(iterator));
    // Jump to the last element
    assert_int_equal(0, BufferListMutableIteratorLast(iterator));
    // and back to the first element
    assert_int_equal(0, BufferListMutableIteratorFirst(iterator));
    // And back to the last element
    assert_int_equal(0, BufferListMutableIteratorLast(iterator));
    // Append one element after the last element
    assert_int_equal(0, BufferListMutableIteratorAppend(iterator, buffer6));
    assert_int_equal(7, BufferListCount(list));
    assert_int_equal(0, BufferListMutableIteratorNext(iterator));
    // Prepend one element before the last element
    assert_int_equal(0, BufferListMutableIteratorPrepend(iterator, buffer7));
    assert_int_equal(8, BufferListCount(list));
    // Go back one element and remove the element
    assert_int_equal(0, BufferListMutableIteratorPrevious(iterator));
    // Remove the current element
    assert_int_equal(0, BufferListMutableIteratorRemove(iterator));
    // Check that the list agrees
    assert_int_equal(7, BufferListCount(list));
    // Remove the last element, we should go back to element3
    assert_int_equal(0, BufferListMutableIteratorRemove(iterator));
    // Check that the list agrees
    assert_int_equal(6, BufferListCount(list));
    // Jump to the first element of the list
    assert_int_equal(0, BufferListMutableIteratorFirst(iterator));
    // Remove the first element, we should end up in element5
    assert_int_equal(0, BufferListMutableIteratorRemove(iterator));
    // Check that the list agrees
    assert_int_equal(5, BufferListCount(list));
    // Now remove element3, the last element of the list using the Remove function
    assert_int_equal(0, BufferListRemove(list, buffer3));
    assert_int_equal(4, BufferListCount(list));
    // Jump to the last element of the list
    assert_int_equal(0, BufferListMutableIteratorLast(iterator));
    // Move the iterator to the previous element, element1, and delete it. The iterator should move to element2.
    assert_int_equal(0, BufferListMutableIteratorPrevious(iterator));
    assert_int_equal(0, BufferListRemove(list, buffer1));
    assert_int_equal(3, BufferListCount(list));
    // Remove the last element of the list, the iterator should move to element0
    assert_int_equal(0, BufferListRemove(list, buffer2));
    assert_int_equal(2, BufferListCount(list));
    // Jump to the first element
    assert_int_equal(0, BufferListMutableIteratorFirst(iterator));
    // Remove the first element, that should move the iterator to element0
    assert_int_equal(0, BufferListRemove(list, buffer5));
    assert_int_equal(1, BufferListCount(list));
    // Finally try to remove the only element using the iterator, it should fail.
    assert_int_equal(-1, BufferListMutableIteratorRemove(iterator));
    // Remove the final element using the list and check that the iterator is invalid
    assert_int_equal(0, BufferListRemove(list, buffer0));
    // Destroy the iterators and the list
    assert_int_equal(0, BufferListMutableIteratorRelease(&iterator));
    assert_int_equal(0, BufferListIteratorDestroy(&lightIterator));
    assert_int_equal(0, BufferListDestroy(&list));
    free (element0);
    free (element1);
    free (element2);
    free (element3);
    free (element4);
    free (element5);
    free (element6);
    free (element7);
}

int main()
{
    PRINT_TEST_BANNER();
    const UnitTest tests[] =
    {
        unit_test(test_initList)
        , unit_test(test_destroyList)
        , unit_test(test_destroyer)
        , unit_test(test_prependToList)
        , unit_test(test_appendToList)
        , unit_test(test_removeFromList)
        , unit_test(test_copyList)
        , unit_test(test_iterator)
        , unit_test(test_mutableIterator)
    };

    return run_tests(tests);
}
