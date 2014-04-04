#include <test.h>

#include <string.h>
#include <list.c>
#include <list.h>

// Simple initialization test
static void test_initList(void)
{
    List *list = NULL;
    list = ListNew(NULL, NULL, NULL);
    assert_true(list != NULL);
    assert_int_not_equal(list, NULL);
    assert_int_equal(list->first, NULL);
    assert_int_equal(list->list, NULL);
    assert_int_equal(list->last, NULL);
    assert_int_equal(list->node_count, 0);
    assert_int_equal(list->state, 0);
    // We shouldn't use this function yet, but otherwise we leak memory
    assert_int_equal(ListDestroy(&list), 0);
}

// This function is just an example function for the destroyer
#include <stdio.h>
void testDestroyer(void *element) {
    // We know the elements are just char *
    char *s = (char *)element;
    printf("element: %s \n", s);
    free (s);
}

static void test_destroyer(void)
{
    List *list = NULL;
    list = ListNew(NULL, NULL, testDestroyer);
    assert_true(list != NULL);
    assert_int_not_equal(list, NULL);
    assert_int_equal(list->first, NULL);
    assert_int_equal(list->list, NULL);
    assert_int_equal(list->last, NULL);
    assert_int_equal(list->node_count, 0);
    assert_int_equal(list->state, 0);
    assert_int_equal(list->compare, NULL);
    assert_int_equal(list->copy, NULL);
    assert_int_not_equal(list->destroy, NULL);

    char *element0 = xstrdup("this is a test string");
    char *element1 = xstrdup("another test string");
    char *element2 = xstrdup("yet another test string");
    char *element3 = xstrdup("and one more test string");

    // We add element0 to the list.
    assert_int_equal(ListPrepend(list, element0), 0);
    // We add element1 to the list.
    assert_int_equal(ListPrepend(list, element1), 0);
    // We add element2 to the list.
    assert_int_equal(ListPrepend(list, element2), 0);
    // We add element3 to the list.
    assert_int_equal(ListPrepend(list, element3), 0);

    // Now we try to destroy the list.
    assert_int_equal(ListDestroy(&list), 0);
}

static void test_prependToList(void)
{
    List *list = NULL;
    list = ListNew(NULL, NULL, testDestroyer);
    assert_true(list != NULL);
    assert_int_not_equal(list, NULL);
    assert_int_equal(list->first, NULL);
    assert_int_equal(list->list, NULL);
    assert_int_equal(list->last, NULL);
    assert_int_equal(list->node_count, 0);
    assert_int_equal(list->state, 0);
    assert_int_equal(list->compare, NULL);
    assert_int_equal(list->copy, NULL);
    assert_int_not_equal(list->destroy, NULL);

    char *element0 = xstrdup("this is a test string");
    char *element1 = xstrdup("another test string");
    void *listPointer = NULL;
    void *firstPointer = NULL;
    void *lastPointer = NULL;

    // We add element0 to the list.
    assert_int_equal(ListPrepend(list, element0), 0);
    // Now we check the list
    assert_int_not_equal(list->first, NULL);
    firstPointer = list->first;
    assert_int_not_equal(list->list, NULL);
    listPointer = list->list;
    assert_true(list->list == list->first);
    assert_int_not_equal(list->last, NULL);
    lastPointer = list->last;
    assert_int_equal(list->node_count, 1);
    // Adding elements does not change the state of the list
    assert_int_equal(list->state, 0);

    // We add element1 to the list.
    assert_int_equal(ListPrepend(list, element1), 0);
    // Now we check the list
    assert_int_not_equal(list->first, NULL);
    assert_false(list->first == firstPointer);
    assert_int_not_equal(list->list, NULL);
    assert_false(list->list == listPointer);
    assert_int_not_equal(list->last, NULL);
    assert_true(list->last == lastPointer);
    assert_int_equal(list->node_count, 2);
    assert_int_equal(list->state, 0);

    // Now we try to destroy the list. This should fail because the list is not empty
    assert_int_equal(ListDestroy(&list), 0);
}

static void test_appendToList(void)
{
    List *list = NULL;
    list = ListNew(NULL, NULL, testDestroyer);
    assert_true(list != NULL);
    assert_int_not_equal(list, NULL);
    assert_int_equal(list->first, NULL);
    assert_int_equal(list->list, NULL);
    assert_int_equal(list->last, NULL);
    assert_int_equal(list->node_count, 0);
    assert_int_equal(list->state, 0);

    char *element0 = xstrdup("this is a test string");
    char *element1 = xstrdup("another test string");
    void *element0tPointer = NULL;

    // We add element0 to the list.
    assert_int_equal(ListAppend(list, element0), 0);
    // Now we check the list
    assert_int_not_equal(list->first, NULL);
    element0tPointer = list->first;
    assert_int_not_equal(list->list, NULL);
    assert_true(list->list == list->first);
    assert_int_not_equal(list->last, NULL);
    assert_true(list->last == list->first);
    assert_int_equal(list->node_count, 1);
    // Adding elements does not change the list state
    assert_int_equal(list->state, 0);

    // We add element1 to the list.
    assert_int_equal(ListAppend(list, element1), 0);
    // Now we check the list
    assert_int_not_equal(list->first, NULL);
    assert_int_not_equal(list->list, NULL);
    assert_int_not_equal(list->last, NULL);
    assert_true(element0tPointer == list->list);
    assert_true(element0tPointer == list->first);
    assert_false(list->first == list->last);
    assert_int_equal(list->node_count, 2);
    assert_int_equal(list->state, 0);

    // Now we try to destroy the list. This should fail because the list is not empty
    assert_int_equal(ListDestroy(&list), 0);
}

static int compareFunction(const void *a, const void *b)
{
    return strcmp(a, b);
}

static void copyFunction(const void *s, void **d)
{
    if (!s || !d)
        return;
    const char *source = s;
    char **destination = (char **)d;

    *destination = xstrdup(source);
}


static void test_removeFromList(void)
{
    List *list = NULL;
    list = ListNew(compareFunction, NULL, testDestroyer);
    assert_true(list != NULL);
    assert_int_not_equal(list, NULL);
    assert_int_equal(list->first, NULL);
    assert_int_equal(list->list, NULL);
    assert_int_equal(list->last, NULL);
    assert_int_equal(list->node_count, 0);
    assert_int_equal(list->state, 0);
    assert_int_not_equal(list->destroy, NULL);
    assert_int_not_equal(list->compare, NULL);
    assert_int_equal(list->copy, NULL);

    char *element0 = xstrdup("this is a test string");
    char *element1 = xstrdup("another test string");
    char *element2 = xstrdup("yet another test string");
    char *element3 = xstrdup("and one more test string");
    char *element4 = xstrdup("non existing element");
    void *listPointer = NULL;
    void *firstPointer = NULL;
    void *secondPointer = NULL;
    void *thirdPointer = NULL;
    void *lastPointer = NULL;

    // We add element0 to the list.
    assert_int_equal(ListPrepend(list, element0), 0);
    // Now we check the list
    assert_int_not_equal(list->first, NULL);
    firstPointer = list->first;
    assert_int_not_equal(list->list, NULL);
    listPointer = list->list;
    assert_true(list->list == list->first);
    assert_int_not_equal(list->last, NULL);
    lastPointer = list->last;
    assert_int_equal(list->node_count, 1);
    // Adding elements does not change the list state
    assert_int_equal(list->state, 0);

    // We add element1 to the list.
    assert_int_equal(ListPrepend(list, element1), 0);
    // Now we check the list
    assert_int_not_equal(list->first, NULL);
    assert_false(list->first == firstPointer);
    assert_int_not_equal(list->list, NULL);
    assert_false(list->list == listPointer);
    assert_true(list->list == list->first);
    secondPointer = list->list;
    assert_int_not_equal(list->last, NULL);
    assert_true(list->last == lastPointer);
    assert_int_equal(list->node_count, 2);
    assert_int_equal(list->state, 0);

    // We add element2 to the list.
    assert_int_equal(ListPrepend(list, element2), 0);
    // Now we check the list
    assert_int_not_equal(list->first, NULL);
    assert_false(list->first == firstPointer);
    assert_int_not_equal(list->list, NULL);
    assert_false(list->list == listPointer);
    assert_true(list->list == list->first);
    thirdPointer = list->list;
    assert_int_not_equal(list->last, NULL);
    assert_true(list->last == lastPointer);
    assert_int_equal(list->node_count, 3);
    assert_int_equal(list->state, 0);

    // We add element3 to the list.
    assert_int_equal(ListPrepend(list, element3), 0);
    // Now we check the list
    assert_int_not_equal(list->first, NULL);
    assert_false(list->first == firstPointer);
    assert_int_not_equal(list->list, NULL);
    assert_false(list->list == listPointer);
    assert_true(list->list == list->first);
    assert_int_not_equal(list->last, NULL);
    assert_true(list->last == lastPointer);
    assert_int_equal(list->node_count, 4);
    assert_int_equal(list->state, 0);

    // We remove the non existing element
    assert_int_equal(ListRemove(list, element4), -1);
    assert_int_not_equal(list->first, NULL);
    assert_false(list->first == firstPointer);
    assert_int_not_equal(list->list, NULL);
    assert_false(list->list == listPointer);
    assert_true(list->list == list->first);
    assert_int_not_equal(list->last, NULL);
    assert_true(list->last == lastPointer);
    assert_int_equal(list->node_count, 4);
    assert_int_equal(list->state, 0);

    // Remove element1 which is in the middle of the list
    assert_int_equal(ListRemove(list, element1), 0);
    // Now we check the list
    assert_int_not_equal(list->first, NULL);
    assert_false(list->first == firstPointer);
    assert_int_not_equal(list->list, NULL);
    assert_false(list->list == listPointer);
    assert_true(list->list == list->first);
    assert_int_not_equal(list->last, NULL);
    assert_true(list->last == lastPointer);
    assert_int_equal(list->node_count, 3);
    assert_int_equal(list->state, 1);

    // Remove element3 which is at the beginning of the list
    assert_int_equal(ListRemove(list, element3), 0);
    // Now we check the list
    assert_int_not_equal(list->first, NULL);
    assert_false(list->first == secondPointer);
    assert_int_not_equal(list->list, NULL);
    assert_false(list->list == listPointer);
    assert_true(list->list == list->first);
    assert_int_not_equal(list->last, NULL);
    assert_true(list->last == lastPointer);
    assert_int_equal(list->node_count, 2);
    assert_int_equal(list->state, 2);

    // Remove element0 which is at the end of the list
    assert_int_equal(ListRemove(list, element0), 0);
    // Now we check the list
    assert_int_not_equal(list->first, NULL);
    assert_false(list->first == secondPointer);
    assert_int_not_equal(list->list, NULL);
    assert_false(list->list == listPointer);
    assert_true(list->list == list->first);
    assert_int_not_equal(list->last, NULL);
    assert_true(list->last == thirdPointer);
    assert_int_equal(list->node_count, 1);
    assert_int_equal(list->state, 3);

    // Remove element2 which is the only element on the list
    assert_int_equal(ListRemove(list, element2), 0);
    // Now we check the list
    assert_int_equal(list->first, NULL);
    assert_int_equal(list->list, NULL);
    assert_int_equal(list->last, NULL);
    assert_int_equal(list->node_count, 0);
    assert_int_equal(list->state, 4);

    // Now we destroy the list.
    assert_int_equal(ListDestroy(&list), 0);
    free (element4);
}

static void test_destroyList(void)
{
    List *list = NULL;
    list = ListNew(NULL, NULL, testDestroyer);
    assert_true(list != NULL);
    assert_int_not_equal(list, NULL);
    assert_int_equal(list->first, NULL);
    assert_int_equal(list->list, NULL);
    assert_int_equal(list->last, NULL);
    assert_int_equal(list->node_count, 0);
    assert_int_equal(list->state, 0);

    // Now we destroy the list
    assert_int_equal(ListDestroy(&list), 0);
    assert_int_equal(list, NULL);
}

static void test_copyList(void)
{
    /*
     * First try the normal path, i.e. with a copy function. Then try it without a copy function.
     */
    List *list1 = NULL;
    List *list2 = NULL;
    List *list3 = NULL;
    List *list4 = NULL;
    char *element0 = xstrdup("this is a test string");
    char *element1 = xstrdup("another test string");
    char *element2 = xstrdup("yet another test string");

    list1 = ListNew(compareFunction, copyFunction, testDestroyer);
    assert_true(list1 != NULL);
    assert_int_not_equal(list1, NULL);
    assert_int_equal(list1->first, NULL);
    assert_int_equal(list1->list, NULL);
    assert_int_equal(list1->last, NULL);
    assert_int_equal(list1->node_count, 0);
    assert_int_equal(list1->state, 0);
    assert_int_equal(0, ListPrepend(list1, (void *)element0));
    assert_int_equal(1, ListCount(list1));
    /*
     * Copy the list1 to list2 and prepend one more element
     */
    assert_int_equal(0, ListCopy(list1, &list2));
    assert_int_equal(1, ListCount(list2));
    assert_true(list1->ref_count == list2->ref_count);
    assert_int_equal(0, ListPrepend(list2, (void *)element1));
    /*
     * The two lists have detached now.
     */
    assert_int_equal(1, ListCount(list1));
    assert_int_equal(2, ListCount(list2));
    assert_false(list1->ref_count == list2->ref_count);
    /*
     * Add one more element to list1 and then attach list3 and list4.
     * Finally detach list4 by removing one element.
     */
    assert_int_equal(0, ListPrepend(list1, (void *)element2));
    assert_int_equal(0, ListCopy(list1, &list3));
    assert_int_equal(0, ListCopy(list1, &list4));
    assert_int_equal(2, ListCount(list1));
    assert_int_equal(2, ListCount(list3));
    assert_int_equal(2, ListCount(list4));
    assert_true(list1->ref_count == list3->ref_count);
    assert_true(list1->ref_count == list4->ref_count);
    assert_true(list4->ref_count == list3->ref_count);
    assert_int_equal(0, ListRemove(list4, (void *)element0));
    assert_int_equal(2, ListCount(list1));
    assert_int_equal(2, ListCount(list3));
    assert_int_equal(1, ListCount(list4));
    assert_true(list1->ref_count == list3->ref_count);
    assert_false(list1->ref_count == list4->ref_count);
    assert_false(list4->ref_count == list3->ref_count);

    assert_int_equal(ListDestroy(&list1), 0);
    assert_int_equal(ListDestroy(&list2), 0);
    assert_int_equal(ListDestroy(&list3), 0);
    assert_int_equal(ListDestroy(&list4), 0);
    /*
     * No copy function now, boys don't cry
     */
    List *list5 = NULL;
    List *list6 = NULL;
    element0 = xstrdup("this is a test string");

    list5 = ListNew(compareFunction, NULL, testDestroyer);
    assert_true(list5 != NULL);
    assert_int_not_equal(list5, NULL);
    assert_int_equal(list5->first, NULL);
    assert_int_equal(list5->list, NULL);
    assert_int_equal(list5->last, NULL);
    assert_int_equal(list5->node_count, 0);
    assert_int_equal(list5->state, 0);
    assert_int_equal(0, ListPrepend(list5, (void *)element0));
    assert_int_equal(1, ListCount(list5));
    /*
     * Copy the list5 to list6 and prepend one more element
     */
    assert_int_equal(-1, ListCopy(list5, &list6));
    assert_true(list6 == NULL);

    assert_int_equal(ListDestroy(&list5), 0);
}

static void test_iterator(void)
{
    List *list = NULL;
    list = ListNew(compareFunction, NULL, testDestroyer);
    assert_true(list != NULL);
    assert_int_not_equal(list, NULL);
    assert_int_equal(list->first, NULL);
    assert_int_equal(list->list, NULL);
    assert_int_equal(list->last, NULL);
    assert_int_equal(list->node_count, 0);
    assert_int_equal(list->state, 0);

    ListIterator *emptyListIterator = NULL;
    emptyListIterator = ListIteratorGet(list);
    assert_true(emptyListIterator == NULL);
    char *element0 = xstrdup("this is a test string");
    char *element1 = xstrdup("another test string");
    char *element2 = xstrdup("yet another test string");
    char *element3 = xstrdup("and one more test string");
    void *element0Pointer = NULL;
    void *element1Pointer = NULL;
    void *element2Pointer = NULL;
    void *element3Pointer = NULL;

    // We add element0 to the list.
    assert_int_equal(ListPrepend(list, element0), 0);
    // Now we check the list
    assert_int_not_equal(list->first, NULL);
    element0Pointer = list->first;
    assert_true(list->first == element0Pointer);
    assert_int_not_equal(list->list, NULL);
    assert_true(list->list == list->first);
    assert_int_not_equal(list->last, NULL);
    assert_int_equal(list->node_count, 1);
    assert_int_equal(list->state, 0);

    // We add element1 to the list.
    assert_int_equal(ListPrepend(list, element1), 0);
    // Now we check the list
    assert_int_not_equal(list->first, NULL);
    assert_false(list->first == element0Pointer);
    assert_int_not_equal(list->list, NULL);
    element1Pointer = list->list;
    assert_true(list->first == element1Pointer);
    assert_int_not_equal(list->last, NULL);
    assert_true(list->last == element0Pointer);
    assert_int_equal(list->node_count, 2);
    assert_int_equal(list->state, 0);

    // We add element2 to the list.
    assert_int_equal(ListPrepend(list, element2), 0);
    // Now we check the list
    assert_int_not_equal(list->first, NULL);
    assert_false(list->first == element1Pointer);
    assert_int_not_equal(list->list, NULL);
    element2Pointer = list->list;
    assert_true(list->first == element2Pointer);
    assert_int_not_equal(list->last, NULL);
    assert_true(list->last == element0Pointer);
    assert_int_equal(list->node_count, 3);
    assert_int_equal(list->state, 0);

    // We add element3 to the list.
    assert_int_equal(ListPrepend(list, element3), 0);
    // Now we check the list
    assert_int_not_equal(list->first, NULL);
    assert_false(list->first == element2Pointer);
    assert_int_not_equal(list->list, NULL);
    element3Pointer = list->list;
    assert_true(list->first == element3Pointer);
    assert_int_not_equal(list->last, NULL);
    assert_true(list->last == element0Pointer);
    assert_int_equal(list->node_count, 4);
    assert_int_equal(list->state, 0);

    ListIterator *iterator0 = NULL;
    iterator0 = ListIteratorGet(list);
    // Check the iterator
    assert_true(iterator0 != NULL);
    assert_int_equal(iterator0->state, 0);
    assert_true(iterator0->origin == list);
    assert_true(iterator0->current == list->first);

    // Remove element1 which is in the middle of the list, this will invalidate the iterator
    assert_int_equal(ListRemove(list, element1), 0);
    // Check that the iterator is not valid by trying to advance it
    assert_int_equal(ListIteratorNext(iterator0), -1);
    // Destroy the iterator
    assert_int_equal(ListIteratorDestroy(&iterator0), 0);
    assert_int_equal(iterator0, NULL);

    // Create a new iterator and move it
    ListIterator *iterator1 = NULL;
    iterator1 = ListIteratorGet(list);
    // Check the iterator
    assert_int_not_equal(iterator1, NULL);
    assert_int_equal(iterator1->state, 1);
    assert_true(iterator1->origin == list);
    assert_true(iterator1->current == list->first);
    void *value = NULL;
    value = ListIteratorData(iterator1);
    assert_true(value == element3);

    // Advance it
    assert_int_equal(ListIteratorNext(iterator1), 0);
    // Check the value, it should be equal to element2
    value = ListIteratorData(iterator1);
    assert_true(value == element2);

    // Advance it, now we are at the last element
    assert_int_equal(ListIteratorNext(iterator1), 0);
    // Check the value, it should be equal to element0
    value = ListIteratorData(iterator1);
    assert_true(value == element0);

    // Advance it, should fail and the iterator should stay where it was
    assert_int_equal(ListIteratorNext(iterator1), -1);
    // Check the value, it should be equal to element0
    value = ListIteratorData(iterator1);
    assert_true(value == element0);

    // Go back
    assert_int_equal(ListIteratorPrevious(iterator1), 0);
    // Check the value, it should be equal to element2
    value = ListIteratorData(iterator1);
    assert_true(value == element2);

    // Go back, now we are at the beginning of the list
    assert_int_equal(ListIteratorPrevious(iterator1), 0);
    // Check the value, it should be equal to element3
    value = ListIteratorData(iterator1);
    assert_true(value == element3);

    // Go back, should fail and the iterator should stay where it was
    assert_int_equal(ListIteratorPrevious(iterator1), -1);
    // Check the value, it should be equal to element3
    value = ListIteratorData(iterator1);
    assert_true(value == element3);

    // Jump to the last element
    assert_int_equal(ListIteratorLast(iterator1), 0);
    // Check the value, it should be equal to element0
    value = ListIteratorData(iterator1);
    assert_true(value == element0);

    // Go back
    assert_true(ListIteratorHasPrevious(iterator1));
    assert_int_equal(ListIteratorPrevious(iterator1), 0);
    // Check the value, it should be equal to element2
    value = ListIteratorData(iterator1);
    assert_true(value == element2);

    // Jump to the first element
    assert_int_equal(ListIteratorFirst(iterator1), 0);
    // Check the value, it should be equal to element3
    value = ListIteratorData(iterator1);
    assert_true(value == element3);

    // Advance it
    assert_true(ListIteratorHasNext(iterator1));
    assert_int_equal(ListIteratorNext(iterator1), 0);
    // Check the value, it should be equal to element2
    value = ListIteratorData(iterator1);
    assert_true(value == element2);

    // Remove the elements
    assert_int_equal(ListRemove(list, element3), 0);
    assert_int_equal(ListRemove(list, element0), 0);
    assert_int_equal(ListRemove(list, element2), 0);

    // Destroy the iterator
    assert_int_equal(ListIteratorDestroy(&iterator1), 0);
    // Now we destroy the list.
    assert_int_equal(ListDestroy(&list), 0);
}

static void test_mutableIterator(void)
{
    List *list = NULL;
    list = ListNew(compareFunction, NULL, testDestroyer);
    assert_true(list != NULL);
    assert_int_not_equal(list, NULL);
    assert_int_equal(list->first, NULL);
    assert_int_equal(list->list, NULL);
    assert_int_equal(list->last, NULL);
    assert_int_equal(list->node_count, 0);
    assert_int_equal(list->state, 0);

    ListMutableIterator *emptyListIterator = NULL;
    emptyListIterator = ListMutableIteratorGet(list);
    assert_true(emptyListIterator == NULL);
    char *element0 = xstrdup("this is a test string");
    char *element1 = xstrdup("another test string");
    char *element2 = xstrdup("yet another test string");
    char *element3 = xstrdup("and one more test string");
    char *element4 = xstrdup("prepended by iterator");
    char *element5 = xstrdup("appended by iterator");
    char *element6 = xstrdup("appended by iterator, second time");
    char *element7 = xstrdup("prepended by iterator, second time");

    // We add element0 to the list.
    assert_int_equal(ListAppend(list, element0), 0);
    // We add element1 to the list.
    assert_int_equal(ListAppend(list, element1), 0);
    // We add element2 to the list.
    assert_int_equal(ListAppend(list, element2), 0);
    // We add element3 to the list.
    assert_int_equal(ListAppend(list, element3), 0);

    // We use a light iterator to check that is valid
    ListIterator *lightIterator = NULL;
    lightIterator = ListIteratorGet(list);
    ListMutableIterator *iterator = NULL;
    ListMutableIterator *secondIterator = NULL;
    iterator = ListMutableIteratorGet(list);
    assert_true(iterator != NULL);
    // The iterator should be pointing to the first element
    assert_true(iterator->current == list->first);
    // Trying to create a second iterator must fail
    secondIterator = ListMutableIteratorGet(list);
    assert_true(secondIterator == NULL);
    // Loop through the list until we get to the last element and then back
    while (ListMutableIteratorHasNext(iterator))
    {
        assert_int_equal(0, ListMutableIteratorNext(iterator));
    }
    assert_int_equal(-1, ListMutableIteratorNext(iterator));
    // and back
    while (ListMutableIteratorHasPrevious(iterator))
    {
        assert_int_equal(0, ListMutableIteratorPrevious(iterator));
    }
    assert_int_equal(-1, ListMutableIteratorPrevious(iterator));
    // Jump to the last element
    assert_int_equal(0, ListMutableIteratorLast(iterator));
    // and back to the first element
    assert_int_equal(0, ListMutableIteratorFirst(iterator));
    // Prepend one element at the beginning of the list
    assert_int_equal(0, ListMutableIteratorPrepend(iterator, (void *)element4));
    assert_int_equal(5, list->node_count);
    // The light iterator is still valid
    assert_int_equal(list->state, lightIterator->state);
    // It should be possible to go back one element now.
    assert_int_equal(0, ListMutableIteratorPrevious(iterator));
    // Check that the list and the iterator agree who is the first one.
    assert_true(list->first == iterator->current);
    // Append one element after the first element
    assert_int_equal(0, ListMutableIteratorAppend(iterator, (void *)element5));
    assert_int_equal(6, list->node_count);
    // The light iterator is still valid
    assert_int_equal(list->state, lightIterator->state);
    // Loop through the list until we get to the last element and then back
    while (ListMutableIteratorHasNext(iterator))
    {
        assert_int_equal(0, ListMutableIteratorNext(iterator));
    }
    assert_int_equal(-1, ListMutableIteratorNext(iterator));
    // and back
    while (ListMutableIteratorHasPrevious(iterator))
    {
        assert_int_equal(0, ListMutableIteratorPrevious(iterator));
    }
    assert_int_equal(-1, ListMutableIteratorPrevious(iterator));
    // Jump to the last element
    assert_int_equal(0, ListMutableIteratorLast(iterator));
    // and back to the first element
    assert_int_equal(0, ListMutableIteratorFirst(iterator));
    // And back to the last element
    assert_int_equal(0, ListMutableIteratorLast(iterator));
    // Append one element after the last element
    assert_int_equal(0, ListMutableIteratorAppend(iterator, (void *)element6));
    assert_int_equal(7, list->node_count);
    // The light iterator is still valid
    assert_int_equal(list->state, lightIterator->state);
    // It should be possible to advance one position
    assert_int_equal(0, ListMutableIteratorNext(iterator));
    // Check that both the list and the iterator point to the same last element
    assert_true(iterator->current == list->last);
    // Prepend one element before the last element
    assert_int_equal(0, ListMutableIteratorPrepend(iterator, (void *)element7));
    assert_int_equal(8, list->node_count);
    // The light iterator is still valid
    assert_int_equal(list->state, lightIterator->state);
    // Go back one element and remove the element
    assert_int_equal(0, ListMutableIteratorPrevious(iterator));
    // We should be located at element4
    assert_string_equal(element7, (char *)iterator->current->payload);
    // Remove the current element
    assert_int_equal(0, ListMutableIteratorRemove(iterator));
    // Check that the list agrees
    assert_int_equal(7, list->node_count);
    // We should be at element5 now, the last element of the list
    assert_string_equal(element6, (char *)iterator->current->payload);
    assert_true(iterator->current == list->last);
    // The light iterator is not valid anymore
    assert_false(list->state == lightIterator->state);
    // Remove the last element, we should go back to element3
    assert_int_equal(0, ListMutableIteratorRemove(iterator));
    // Check that the list agrees
    assert_int_equal(6, list->node_count);
    // We should be at element3 now, the last element of the list
    assert_string_equal(element3, (char *)iterator->current->payload);
    assert_true(iterator->current == list->last);
    // Jump to the first element of the list
    assert_int_equal(0, ListMutableIteratorFirst(iterator));
    // Remove the first element, we should end up in element5
    assert_int_equal(0, ListMutableIteratorRemove(iterator));
    // Check that the list agrees
    assert_int_equal(5, list->node_count);
    // We should be at element5 now, the first element of the list
    assert_string_equal(element5, (char *)iterator->current->payload);
    assert_true(iterator->current == list->first);
    // Now remove element3, the last element of the list using the Remove function
    assert_int_equal(0, ListRemove(list, (void *)element3));
    assert_int_equal(4, list->node_count);
    // We should be at element5 now, the first element of the list
    assert_string_equal(element5, (char *)iterator->current->payload);
    assert_true(iterator->current == list->first);
    // Jump to the last element of the list
    assert_int_equal(0, ListMutableIteratorLast(iterator));
    // This should be element2
    assert_string_equal(element2, (char *)iterator->current->payload);
    // Move the iterator to the previous element, element1, and delete it. The iterator should move to element2.
    assert_int_equal(0, ListMutableIteratorPrevious(iterator));
    assert_int_equal(0, ListRemove(list, (void *)element1));
    assert_int_equal(3, list->node_count);
    assert_string_equal(element2, (char *)iterator->current->payload);
    assert_true(iterator->current == list->last);
    // Remove the last element of the list, the iterator should move to element0
    assert_int_equal(0, ListRemove(list, (void *)element2));
    assert_int_equal(2, list->node_count);
    assert_string_equal(element0, (char *)iterator->current->payload);
    assert_true(iterator->current == list->last);
    // Jump to the first element
    assert_int_equal(0, ListMutableIteratorFirst(iterator));
    // Remove the first element, that should move the iterator to element0
    assert_int_equal(0, ListRemove(list, (void *)element5));
    assert_int_equal(1, list->node_count);
    assert_string_equal(element0, (char *)iterator->current->payload);
    assert_true(iterator->current == list->last);
    assert_true(iterator->current == list->first);
    // Finally try to remove the only element using the iterator, it should fail.
    assert_int_equal(-1, ListMutableIteratorRemove(iterator));
    // Remove the final element using the list and check that the iterator is invalid
    assert_int_equal(0, ListRemove(list, (void *)element0));
    assert_false(iterator->valid);
    // Destroy the iterators and the list
    assert_int_equal(0, ListMutableIteratorRelease(&iterator));
    assert_int_equal(0, ListIteratorDestroy(&lightIterator));
    assert_int_equal(0, ListDestroy(&list));
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
